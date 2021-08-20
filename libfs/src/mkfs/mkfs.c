#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#include "filesystem/fs.h"
#include "io/block_io.h"
#include "global/global.h"
#include "storage/storage.h"
#include "mlfs/kerncompat.h"
#include "io/balloc.h"

#if MLFS_LEASE
#include "experimental/leases.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define g_root_dev 1
#define g_ssd_dev 2
#define g_hdd_dev 3
#define g_log_dev 4

#ifndef static_assert
#define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)
#endif

#define dbg_printf 
//#define dbg_printf printf

// In-kernel fs Disk layout:
// [ sb block | inode blocks | free bitmap | data blocks ]
// [ inode block | free bitmap | data blocks ] is a block group.
// If data blocks is full, then file system will allocate a new block group.

int dev_id, fsfd;
struct disk_superblock ondisk_sb;
struct super_block inmem_sb;
char zeroes[g_block_size_bytes];
addr_t freeinode = 1;
addr_t freeblock;

void write_bitmap(int);
void wsect(addr_t, uint8_t *buf);
void rsect(addr_t sec, uint8_t *buf);
void write_inode(uint32_t inum, struct dinode *dip);
void read_inode(uint8_t dev, uint32_t inum, struct dinode *dip);
uint32_t mkfs_ialloc(uint8_t dev, uint16_t type);
void iappend(uint8_t dev, uint32_t inum, void *p, int n);
void mkfs_read_superblock(uint8_t dev, struct disk_superblock *disk_sb);

// Inodes per block.
#define IPB           (g_block_size_bytes / sizeof(struct dinode))
// Block containing inode i
static inline addr_t mkfs_get_inode_block(uint32_t inum, addr_t inode_start)
{
	return (inum /IPB) + inode_start;
}

typedef enum {NON, NVM, HDD, FS} storage_mode_t;

storage_mode_t storage_mode;

#if 0
#ifdef __cplusplus

static struct storage_operations nvm_storage_ops = {
	dax_init,
	dax_read,
	NULL,
	dax_write,
	NULL,
	dax_erase,
	dax_commit,
	NULL,
	dax_exit
};

struct storage_operations storage_hdd = {
	hdd_init,
	hdd_read,
	NULL,
	hdd_write,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	hdd_exit,
};
#else
static struct storage_operations nvm_storage_ops = {
	.init = dax_init,
	.read = dax_read,
	.erase = dax_erase,
	.write = dax_write,
	.commit = dax_commit,
	.wait_io = NULL,
	.exit = dax_exit,
};

struct storage_operations storage_hdd = {
	.init = hdd_init,
	.read = hdd_read,
	.read_unaligned = NULL,
	.write = hdd_write,
	.write_unaligned = NULL,
	.commit = NULL,
	.wait_io = NULL,
	.erase = NULL,
	.readahead = NULL,
	.exit = hdd_exit,
};
#endif
#endif

#define xshort(x) x
#define xint(x) x

int main(int argc, char *argv[])
{
	int i, cc, fd;
	uint32_t rootino, mlfs_dir_ino;
	addr_t off;
	struct mlfs_dirent de;
	uint8_t buf[g_block_size_bytes];
	struct dinode din;
	uint32_t nbitmap;
	int ninodeblocks = NINODES / IPB + 1;
	int leaseblocks = NINODES / LPB + 1;
	uint64_t file_size_bytes;
	uint64_t file_size_blks, log_size_blks;
	uint64_t nlog, ndatablocks;
	unsigned int nmeta;    // Number of meta blocks (boot, sb, inode, bitmap)

	static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

	if (argc < 2) {
		fprintf(stderr, "Usage: mkfs dev_id\n");
		exit(1);
	}

	dev_id = atoi(argv[1]);
	printf("dev_id %d\n", dev_id);

	if (dev_id == 1) {
		printf("Formating fs with DEVDAX\n");
		storage_mode = NVM;
	} else if (dev_id == 2 || dev_id == 3) {
		printf("Formating fs with HDD\n");
		storage_mode = HDD;
	} else {
		printf("Formating fs with DEVDAX\n");
		storage_mode = NVM;
	}

	// Bypass dev-dax mmap problem: use actual device size - 550 MB.
	file_size_bytes = dev_size[dev_id] - (550 << 20);
	file_size_blks = file_size_bytes >> g_block_size_shift;
	log_size_blks = file_size_blks - (1UL * (1 << 10));
	nbitmap = file_size_blks / (g_block_size_bytes * 8) + 1;

	printf("Ondisk inode size = %lu\n", sizeof(struct dinode));

	/* all invariants check */
	if (g_block_size_bytes % sizeof(struct dinode) != 0) {
		printf("dinode size (%lu) is not power of 2\n",
				sizeof(struct dinode));
		exit(-1);
	}

	//mlfs_assert((g_block_size_bytes % sizeof(struct mlfs_dirent)) == 0);
	if (g_block_size_bytes % sizeof(struct mlfs_dirent) != 0) {
		printf("dirent (size %lu) should be power of 2\n",
				sizeof(struct mlfs_dirent));
		exit(-1);
	}

	/* nmeta = Empty block + Superblock + inode block + block allocation bitmap block */
	if (dev_id == g_root_dev) {
		nmeta = 2 + ninodeblocks + nbitmap + leaseblocks;
		ndatablocks = file_size_blks - nmeta; 
		nlog = 0;
	} 
	// SSD and HDD case.	
	else if (dev_id <= g_hdd_dev) { 
		nmeta = 2 + ninodeblocks + nbitmap + leaseblocks;
		ndatablocks = file_size_blks - nmeta; 
		nlog = 0;
	} 
	// Per-application log.
	else {
		nmeta = 2 + ninodeblocks + nbitmap + leaseblocks;
		ndatablocks = 0;
		nlog = log_size_blks;
	}

	// Fill superblock data
	ondisk_sb.size = file_size_blks;
	ondisk_sb.ndatablocks = ndatablocks;
	ondisk_sb.ninodes = NINODES;
	ondisk_sb.nlog = nlog;
	ondisk_sb.inode_start = 2;
	ondisk_sb.lease_start = 2 + ninodeblocks;
	ondisk_sb.bmap_start = 2 + ninodeblocks + leaseblocks;
	ondisk_sb.datablock_start = nmeta; 
	ondisk_sb.log_start = ondisk_sb.datablock_start + ndatablocks;

	// Setup in-memory superblock that is required for extent tree and block allocator.
	inmem_sb.ondisk = &ondisk_sb;
	/*
		 inmem_sb.s_inode_bitmap = (unsigned long *)
		 mlfs_zalloc(BITS_TO_LONGS(ondisk_sb.ninodes));
	 */

	assert(sizeof(ondisk_sb) <= g_block_size_bytes);

	printf("Creating file system\n");
	printf("size of superblock %ld\n", sizeof(ondisk_sb));
	printf("----------------------------------------------------------------\n");
	printf("nmeta %d (boot 1, super 1, inode blocks %u, bitmap blocks %u) \n"
			"[ inode start %lu, bmap start %lu, datablock start %lu, log start %lu ] \n"
			": data blocks %lu log blocks %lu -- total %lu (%lu MB)\n",
			nmeta, 
			ninodeblocks, 
			nbitmap,
			ondisk_sb.inode_start, 
			ondisk_sb.bmap_start, 
			ondisk_sb.datablock_start,
			ondisk_sb.log_start,
			ndatablocks, 
			nlog,
			file_size_blks, 
			(file_size_blks * g_block_size_bytes) >> 20);
	printf("----------------------------------------------------------------\n");

	g_bdev[dev_id] = bdev_alloc(dev_id, 12);

	if (storage_mode == NVM) {
		g_bdev[dev_id]->storage_engine = &storage_dax;
		g_bdev[dev_id]->map_base_addr = 
			storage_dax.init(dev_id, g_dev_path[dev_id]);
	} else if (storage_mode == HDD) {
		g_bdev[dev_id]->storage_engine = &storage_hdd;
		g_bdev[dev_id]->map_base_addr = NULL;
		storage_hdd.init(dev_id, g_dev_path[dev_id]); 
	} else {
		fsfd = open(argv[1], O_RDWR|O_CREAT|O_TRUNC, 0666);
		if(fsfd < 0){
			perror(argv[1]);
			exit(1);
		}
	}

	memset(zeroes, 0, g_block_size_bytes);
#if 1
	if (storage_mode == HDD) {
		for(i = 0; i < ondisk_sb.datablock_start; i++)
			wsect(i, (uint8_t *)zeroes);
	} else {
		for(i = 0; i < file_size_blks - 1; i++) 
			wsect(i, (uint8_t *)zeroes);
	}
#else
	for(i = 0; i < ondisk_sb.datablock_start; i++)
		wsect(i, (uint8_t *)zeroes);
#endif

	disk_sb = (struct disk_superblock *)mlfs_zalloc(
			sizeof(struct disk_superblock) * (g_n_devices + 1));

	for (i = 0; i < g_n_devices + 1; i++) 
		sb[i] = (struct super_block *)mlfs_zalloc(sizeof(struct super_block));

	disk_sb[dev_id] = ondisk_sb;

	memset(buf, 0, sizeof(buf));
	printf("== Write superblock\n");
	memmove(buf, &ondisk_sb, sizeof(ondisk_sb));
	wsect(1, buf);

	// Nothing to be done if not the NVM shared area device.
	if (dev_id != g_root_dev) {
		write_bitmap(nmeta);
		goto exit;
	}

	read_superblock(g_root_dev);

	// followings should be identical to read_superblock in the kernfs.
	for(int i=0; i < MAX_LIBFS_PROCESSES; i++)
		sb[g_root_dev]->s_dirty_root[i] = RB_ROOT;

	sb[g_root_dev]->last_block_allocated = 0;

	sb[g_root_dev]->n_partition = 1;

	sb[g_root_dev]->num_blocks = disk_sb[g_root_dev].ndatablocks;
	sb[g_root_dev]->reserved_blocks = disk_sb[g_root_dev].datablock_start;;

	assert(g_bdev[g_root_dev] != NULL);
	sb[g_root_dev]->s_bdev = g_bdev[g_root_dev];



#if defined (NIC_OFFLOAD) && defined(NIC_SIDE)
	balloc_init(g_root_dev, sb[g_root_dev], 1, 0);
#else
	balloc_init(g_root_dev, sb[g_root_dev], 1);
#endif

	// for debugging.
	if (0) {
		unsigned long blknr;
		mlfs_new_blocks(sb[g_root_dev], &blknr, 1, 0, 0, DATA, 0);
		printf("%lu\n", blknr);
	}

	freeblock = nmeta;  // the first free block that we can allocate

	// update block allocation bitmap used by metadata blocks
	bitmap_bits_set_range(sb[g_root_dev]->s_blk_bitmap, 0, nmeta);
	write_bitmap(nmeta);

	mlfs_ext_init_locks();

	// Create / directory
	rootino = mkfs_ialloc(dev_id, T_DIR);
	printf("== create / directory\n");
	printf("root inode(inum = %u) at block address %lx\n", 
			rootino, mkfs_get_inode_block(rootino, ondisk_sb.inode_start));
	assert(rootino == ROOTINO);

	bzero(&de, sizeof(de));
	de.inum = xshort(rootino);
	strcpy(de.name, ".");
	iappend(dev_id, rootino, &de, sizeof(de));

	bzero(&de, sizeof(de));
	de.inum = xshort(rootino);
	strcpy(de.name, "..");
	iappend(dev_id, rootino, &de, sizeof(de));

	// Create /mlfs directory
#if 1
	mlfs_dir_ino = mkfs_ialloc(dev_id, T_DIR);
	printf("== create /mlfs directory\n");
	printf("/mlfs inode(inum = %u) at block address %lx\n", 
			mlfs_dir_ino, mkfs_get_inode_block(mlfs_dir_ino, ondisk_sb.inode_start));

	bzero(&de, sizeof(de));
	de.inum = xshort(mlfs_dir_ino);
	strcpy(de.name, ".");
	iappend(dev_id, mlfs_dir_ino, &de, sizeof(de));

	bzero(&de, sizeof(de));
	de.inum = xshort(rootino);
	strcpy(de.name, "..");
	iappend(dev_id, mlfs_dir_ino, &de, sizeof(de));

	// append /mlfs to / directory.
	bzero(&de, sizeof(de));
	de.inum = xshort(mlfs_dir_ino);
	strcpy(de.name, "mlfs");
	iappend(dev_id, rootino, &de, sizeof(de));

#if 0
	// if we use leases, shard mlfs_dir_ino such that only
        // 1 node has a Write lease on it
	struct dinode mlfs_dir_din;
	read_inode(dev_id, mlfs_dir_ino, &mlfs_dir_din);
	mlfs_dir_din.lstate = LEASE_FREE;
	mlfs_dir_din.lmid = 0; //node 0 is always the lease manager for /mlfs
	write_inode(mlfs_dir_ino, &mlfs_dir_din);
#endif

#endif

	// clean /mlfs directory
	//read_inode(dev_id, mlfs_dir_ino, &din);
	//wsect(din.l1_addrs[0], (uint8_t *)zeroes);

#if 0
	for(i = 3; i < argc; i++){
		assert(index(argv[i], '/') == 0);

		if((fd = open(argv[i], 0)) < 0){
			perror(argv[i]);
			exit(1);
		}

		// Skip leading _ in name when writing to file system.
		// The binaries are named _rm, _cat, etc. to keep the
		// build operating system from trying to execute them
		// in place of system binaries like rm and cat.
		if(argv[i][0] == '_')
			++argv[i];

		inum = mkfs_ialloc(T_FILE);

		bzero(&de, sizeof(de));
		de.inum = xshort(inum);
		strncpy(de.name, argv[i], DIRSIZ);
		iappend(rootino, &de, sizeof(de));

		while((cc = read(fd, buf, sizeof(buf))) > 0)
			iappend(inum, buf, cc);

		close(fd);
	}
#endif

	// fix size of root inode dir
	read_inode(dev_id, rootino, &din);
	din.size = xint(din.size);
	write_inode(rootino, &din);

exit:
	if (storage_mode == NVM) {
		//FIXME: make sure that data persists before exiting
		//storage_dax.commit(dev_id);
	}
	else if (storage_mode == HDD)
		//storage_hdd.commit(dev_id, 0, 0, 0, 0);
		storage_hdd.commit(dev_id);

	exit(0);
}

void wsect(addr_t sec, uint8_t *buf)
{
	if (storage_mode == NVM) {
		storage_dax.write(dev_id, buf, sec, g_block_size_bytes);
	} else if (storage_mode == HDD) {
		storage_hdd.write(dev_id, buf, sec, g_block_size_bytes);
	} else if (storage_mode == FS) {
		if(lseek(fsfd, sec * g_block_size_bytes, 0) != sec * g_block_size_bytes) {
			perror("lseek");
			exit(1);
		}

		if(write(fsfd, buf, g_block_size_bytes) != g_block_size_bytes) {
			perror("write");
			exit(1);
		}
	}
}

void write_inode(uint32_t inum, struct dinode *ip)
{
	uint8_t buf[g_block_size_bytes];
	addr_t inode_block;
	struct dinode *dip;

	inode_block = mkfs_get_inode_block(inum, ondisk_sb.inode_start);
	rsect(inode_block, buf);

	dip = ((struct dinode*)buf) + (inum % IPB);
	*dip = *ip;

	dbg_printf("%s: inode %u (addr %u) type %u\n", 
			__func__, inum, inode_block, ip->itype);

	wsect(inode_block, buf);
}

void read_inode(uint8_t dev_id, uint32_t inum, struct dinode *ip)
{
	uint8_t buf[g_block_size_bytes];
	addr_t bn;
	struct dinode *dip;

	bn = mkfs_get_inode_block(inum, ondisk_sb.inode_start);
	rsect(bn, buf);

	dip = ((struct dinode*)buf) + (inum % IPB);
	*ip = *dip;
}

void rsect(addr_t sec, uint8_t *buf)
{
	if (storage_mode == NVM) {
		storage_dax.read(dev_id, buf, sec, g_block_size_bytes);
	} else if (storage_mode == HDD) {
		storage_hdd.read(dev_id, buf, sec, g_block_size_bytes);
	} else if (storage_mode == FS) {
		if(lseek(fsfd, sec * g_block_size_bytes, 0) != sec * g_block_size_bytes){
			perror("lseek");
			exit(1);
		}

		if(read(fsfd, buf, g_block_size_bytes) != g_block_size_bytes) {
			perror("read");
			exit(1);
		}
	}
}

uint32_t mkfs_ialloc(uint8_t dev, uint16_t type)
{
	uint32_t inum = freeinode++;
	struct dinode din;

	bzero(&din, sizeof(din));
	din.itype = xshort(type);
	din.nlink = xshort(1);
	din.size = xint(0);

	memset(din.l1_addrs, 0, sizeof(addr_t) * (NDIRECT + 1));
	memset(din.l2_addrs, 0, sizeof(addr_t) * (NDIRECT + 1));
	memset(din.l3_addrs, 0, sizeof(addr_t) * (NDIRECT + 1));

	write_inode(inum, &din);

	return inum;
}

void write_bitmap(int used)
{
	uint8_t buf[g_block_size_bytes];
	int i;

	dbg_printf("balloc: first %d blocks have been allocated\n", used);
	assert(used < g_block_size_bytes*8);
	bzero(buf, g_block_size_bytes);
	for(i = 0; i < used; i++){
		buf[i/8] = buf[i/8] | (0x1 << (i%8));
	}
	dbg_printf("balloc: write bitmap block at sector %lx\n", ondisk_sb.bmap_start);
	wsect(ondisk_sb.bmap_start, buf);
}

#define _min(a, b) ((a) < (b) ? (a) : (b))

// append new data (xp) to an inode (inum)
// 1. allocate new block
// 2. update inode pointers for data block (dinode.addrs) - in-memory
// 3. write data blocks - on-disk
// 4. update inode blocks - on-disk
void iappend(uint8_t dev, uint32_t inum, void *xp, int n)
{
	char *data = (char*)xp;
	struct dinode din;
	struct inode inode;
	uint8_t buf[g_block_size_bytes];
	addr_t  off, off_in_block;
	int ret;
	handle_t handle = {.dev = g_root_dev};
	struct mlfs_map_blocks map;
	addr_t block_address;

	read_inode(dev, inum, &din);

	// Extent tree APIs requires struct inode. So, I used a temporary 
	// inode to set up the target on-disk inode.
	inode._dinode = (struct dinode *)&inode;
	memmove(inode._dinode, &din, sizeof(struct dinode));
	inode.flags |= I_VALID;
	inode.i_sb = malloc(sizeof(struct super_block) * g_n_devices);
	inode.i_sb[dev] = &inmem_sb;
	inode.i_sb[g_root_dev] = sb[g_root_dev];
	inode.inum = inum;

	off = xint(din.size);

	if (inode.itype == T_FILE || inode.itype == T_DIR) {
		struct mlfs_extent_header *ihdr;

		ihdr = ext_inode_hdr(&handle, &inode);

		// First creation of dinode of file
		if (ihdr->eh_magic != MLFS_EXT_MAGIC) {
			mlfs_ext_tree_init(&handle, &inode);

			/* For testing purpose, those data are hard-coded. */
			inode.i_writeback = NULL;
			memset(inode.i_uuid, 0xCC, sizeof(inode.i_uuid));
			inode.i_csum = mlfs_crc32c(~0, inode.i_uuid, sizeof(inode.i_uuid));
			inode.i_csum =
				mlfs_crc32c(inode.i_csum, &inode.inum, sizeof(inode.inum));
			inode.i_csum = mlfs_crc32c(inode.i_csum, &inode.i_generation,
					sizeof(inode.i_generation));
		}
	}

	off_in_block = off % g_block_size_bytes;
	if (off_in_block + n > g_block_size_bytes) {
		printf("cannot support cross block IO yet");
		exit(-1);
	}

	map.m_lblk = (off >> g_block_size_shift);
	map.m_len = (n >> g_block_size_shift);
	map.m_len += n % g_block_size_bytes ? 1 : 0;
	map.m_flags = 0;

	ret = mlfs_ext_get_blocks(&handle, &inode, &map, MLFS_GET_BLOCKS_CREATE_DATA);

	assert(ret == map.m_len);

	rsect(map.m_pblk, buf);	

	memcpy(buf + off_in_block, (uint8_t *)data, n);
	wsect(map.m_pblk, buf);
	off += n;
	printf("write inode inum %d data: blocknr = %lu\n", inode.inum, map.m_pblk);

	sync_all_buffers(g_bdev[g_root_dev]);
	store_all_bitmap(g_root_dev, sb[g_root_dev]->s_blk_bitmap);

	memmove(&din, inode._dinode, sizeof(struct dinode));
	din.size = xint(off);

	printf("inode %u size %lu\n", inum, din.size);

	write_inode(inum, &din);
	free(inode.i_sb);
}

#ifdef __cplusplus
}
#endif
