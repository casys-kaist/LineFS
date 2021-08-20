/*
 * BRIEF DESCRIPTION
 *
 * Definitions for the NOVA filesystem.
 *
 * Copyright 2015-2016 Regents of the University of California,
 * UCSD Non-Volatile Systems Lab, Andiry Xu <jix024@cs.ucsd.edu>
 * Copyright 2012-2013 Intel Corporation
 * Copyright 2009-2011 Marco Stornelli <marco.stornelli@gmail.com>
 * Copyright 2003 Sony Corporation
 * Copyright 2003 Matsushita Electric Industrial Co., Ltd.
 * 2003-2004 (c) MontaVista Software, Inc. , Steve Longerbeam
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#ifndef __NOVA_H
#define __NOVA_H

#include <linux/fs.h>
#include <linux/dax.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/crc16.h>
#include <linux/mutex.h>
#include <linux/pagemap.h>
#include <linux/backing-dev.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/rcupdate.h>
#include <linux/types.h>
#include <linux/rbtree.h>
#include <linux/radix-tree.h>
#include <linux/version.h>
#include <linux/kthread.h>
#include <linux/buffer_head.h>
#include <linux/uio.h>
#include <asm/tlbflush.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
#include <linux/pfn_t.h>
#endif

#include "nova_def.h"
#include "journal.h"
#include "stats.h"

#define PAGE_SHIFT_2M 21
#define PAGE_SHIFT_1G 30

#define NOVA_ASSERT(x)                                                 \
	if (!(x)) {                                                     \
		printk(KERN_WARNING "assertion failed %s:%d: %s\n",     \
	               __FILE__, __LINE__, #x);                         \
	}

/*
 * Debug code
 */
#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#endif

/* #define nova_dbg(s, args...)         pr_debug(s, ## args) */
#define nova_dbg(s, args ...)           pr_info(s, ## args)
#define nova_dbg1(s, args ...)
#define nova_err(sb, s, args ...)       nova_error_mng(sb, s, ## args)
#define nova_warn(s, args ...)          pr_warning(s, ## args)
#define nova_info(s, args ...)          pr_info(s, ## args)

extern unsigned int nova_dbgmask;
#define NOVA_DBGMASK_MMAPHUGE          (0x00000001)
#define NOVA_DBGMASK_MMAP4K            (0x00000002)
#define NOVA_DBGMASK_MMAPVERBOSE       (0x00000004)
#define NOVA_DBGMASK_MMAPVVERBOSE      (0x00000008)
#define NOVA_DBGMASK_VERBOSE           (0x00000010)
#define NOVA_DBGMASK_TRANSACTION       (0x00000020)

#define nova_dbg_mmap4k(s, args ...)		 \
	((nova_dbgmask & NOVA_DBGMASK_MMAP4K) ? nova_dbg(s, args) : 0)
#define nova_dbg_mmapv(s, args ...)		 \
	((nova_dbgmask & NOVA_DBGMASK_MMAPVERBOSE) ? nova_dbg(s, args) : 0)
#define nova_dbg_mmapvv(s, args ...)		 \
	((nova_dbgmask & NOVA_DBGMASK_MMAPVVERBOSE) ? nova_dbg(s, args) : 0)

#define nova_dbg_verbose(s, args ...)		 \
	((nova_dbgmask & NOVA_DBGMASK_VERBOSE) ? nova_dbg(s, ##args) : 0)
#define nova_dbgv(s, args ...)	nova_dbg_verbose(s, ##args)
#define nova_dbg_trans(s, args ...)		 \
	((nova_dbgmask & NOVA_DBGMASK_TRANSACTION) ? nova_dbg(s, ##args) : 0)

#define nova_set_bit                   __test_and_set_bit_le
#define nova_clear_bit                 __test_and_clear_bit_le
#define nova_find_next_zero_bit                find_next_zero_bit_le

#define clear_opt(o, opt)       (o &= ~NOVA_MOUNT_ ## opt)
#define set_opt(o, opt)         (o |= NOVA_MOUNT_ ## opt)
#define test_opt(sb, opt)       (NOVA_SB(sb)->s_mount_opt & NOVA_MOUNT_ ## opt)

#define NOVA_LARGE_INODE_TABLE_SIZE    (0x200000)
/* NOVA size threshold for using 2M blocks for inode table */
#define NOVA_LARGE_INODE_TABLE_THREASHOLD    (0x20000000)
/*
 * nova inode flags
 *
 * NOVA_EOFBLOCKS_FL	There are blocks allocated beyond eof
 */
#define NOVA_EOFBLOCKS_FL      0x20000000
/* Flags that should be inherited by new inodes from their parent. */
#define NOVA_FL_INHERITED (FS_SECRM_FL | FS_UNRM_FL | FS_COMPR_FL | \
			    FS_SYNC_FL | FS_NODUMP_FL | FS_NOATIME_FL |	\
			    FS_COMPRBLK_FL | FS_NOCOMP_FL | FS_JOURNAL_DATA_FL | \
			    FS_NOTAIL_FL | FS_DIRSYNC_FL)
/* Flags that are appropriate for regular files (all but dir-specific ones). */
#define NOVA_REG_FLMASK (~(FS_DIRSYNC_FL | FS_TOPDIR_FL))
/* Flags that are appropriate for non-directories/regular files. */
#define NOVA_OTHER_FLMASK (FS_NODUMP_FL | FS_NOATIME_FL)
#define NOVA_FL_USER_VISIBLE (FS_FL_USER_VISIBLE | NOVA_EOFBLOCKS_FL)

/* IOCTLs */
#define	NOVA_PRINT_TIMING		0xBCD00010
#define	NOVA_CLEAR_STATS		0xBCD00011
#define	NOVA_PRINT_LOG			0xBCD00013
#define	NOVA_PRINT_LOG_BLOCKNODE	0xBCD00014
#define	NOVA_PRINT_LOG_PAGES		0xBCD00015
#define	NOVA_PRINT_FREE_LISTS		0xBCD00018


#define	READDIR_END			(ULONG_MAX)
#define	INVALID_CPU			(-1)
#define	SHARED_CPU			(65536)
#define FREE_BATCH			(16)

extern int measure_timing;

extern unsigned int blk_type_to_shift[NOVA_BLOCK_TYPE_MAX];
extern unsigned int blk_type_to_size[NOVA_BLOCK_TYPE_MAX];

/* ======================= Log entry ========================= */
/* Inode entry in the log */

#define	INVALID_MASK	4095
#define	BLOCK_OFF(p)	((p) & ~INVALID_MASK)

#define	ENTRY_LOC(p)	((p) & INVALID_MASK)

enum nova_entry_type {
	FILE_WRITE = 1,
	DIR_LOG,
	SET_ATTR,
	LINK_CHANGE,
	NEXT_PAGE,
};

static inline u8 nova_get_entry_type(void *p)
{
	return *(u8 *)p;
}

static inline void nova_set_entry_type(void *p, enum nova_entry_type type)
{
	*(u8 *)p = type;
}

struct nova_file_write_entry {
	/* ret of find_nvmm_block, the lowest byte is entry type */
	__le64	block;
	__le64	pgoff;
	__le32	num_pages;
	__le32	invalid_pages;
	/* For both ctime and mtime */
	__le32	mtime;
	__le32	padding;
	__le64	size;
} __attribute((__packed__));

struct nova_inode_page_tail {
	__le64	padding1;
	__le64	padding2;
	__le64	padding3;
	__le64	next_page;
} __attribute((__packed__));

#define	LAST_ENTRY	4064
#define	PAGE_TAIL(p)	(((p) & ~INVALID_MASK) + LAST_ENTRY)

/* Fit in PAGE_SIZE */
struct	nova_inode_log_page {
	char padding[LAST_ENTRY];
	struct nova_inode_page_tail page_tail;
} __attribute((__packed__));

#define	EXTEND_THRESHOLD	256

/*
 * Structure of a directory log entry in NOVA.
 * Update DIR_LOG_REC_LEN if modify this struct!
 */
struct nova_dentry {
	u8	entry_type;
	u8	name_len;               /* length of the dentry name */
	u8	file_type;              /* file type */
	u8	invalid;		/* Invalid now? */
	__le16	de_len;                 /* length of this dentry */
	__le16	links_count;
	__le32	mtime;			/* For both mtime and ctime */
	__le64	ino;                    /* inode no pointed to by this entry */
	__le64	size;
	char	name[NOVA_NAME_LEN + 1];	/* File name */
} __attribute((__packed__));

#define NOVA_DIR_PAD			8	/* Align to 8 bytes boundary */
#define NOVA_DIR_ROUND			(NOVA_DIR_PAD - 1)
#define NOVA_DIR_LOG_REC_LEN(name_len)	(((name_len) + 29 + NOVA_DIR_ROUND) & \
				      ~NOVA_DIR_ROUND)

/* Struct of inode attributes change log (setattr) */
struct nova_setattr_logentry {
	u8	entry_type;
	u8	attr;
	__le16	mode;
	__le32	uid;
	__le32	gid;
	__le32	atime;
	__le32	mtime;
	__le32	ctime;
	__le64	size;
} __attribute((__packed__));

/* Do we need this to be 32 bytes? */
struct nova_link_change_entry {
	u8	entry_type;
	u8	padding;
	__le16	links;
	__le32	ctime;
	__le32	flags;
	__le32	generation;
	__le64	paddings[2];
} __attribute((__packed__));

enum alloc_type {
	LOG = 1,
	DATA,
};

#define	MMAP_WRITE_BIT	0x20UL	// mmaped for write
#define	IS_MAP_WRITE(p)	((p) & (MMAP_WRITE_BIT))
#define	MMAP_ADDR(p)	((p) & (PAGE_MASK))

static inline void nova_update_tail(struct nova_inode *pi, u64 new_tail)
{
	timing_t update_time;

	NOVA_START_TIMING(update_tail_t, update_time);

	PERSISTENT_BARRIER();
	pi->log_tail = new_tail;
	nova_flush_buffer(&pi->log_tail, CACHELINE_SIZE, 1);

	NOVA_END_TIMING(update_tail_t, update_time);
}

/* symlink.c */
int nova_block_symlink(struct super_block *sb, struct nova_inode *pi,
	struct inode *inode, u64 log_block,
	unsigned long name_blocknr, const char *symname, int len);

/* Inline functions start here */

/* Mask out flags that are inappropriate for the given type of inode. */
static inline __le32 nova_mask_flags(umode_t mode, __le32 flags)
{
	flags &= cpu_to_le32(NOVA_FL_INHERITED);
	if (S_ISDIR(mode))
		return flags;
	else if (S_ISREG(mode))
		return flags & cpu_to_le32(NOVA_REG_FLMASK);
	else
		return flags & cpu_to_le32(NOVA_OTHER_FLMASK);
}

static inline int nova_calc_checksum(u8 *data, int n)
{
	u16 crc = 0;

	crc = crc16(~0, (__u8 *)data + sizeof(__le16), n - sizeof(__le16));
	if (*((__le16 *)data) == cpu_to_le16(crc))
		return 0;
	else
		return 1;
}

struct nova_range_node_lowhigh {
	__le64 range_low;
	__le64 range_high;
};

#define	RANGENODE_PER_PAGE	254

struct nova_range_node {
	struct rb_node node;
	unsigned long range_low;
	unsigned long range_high;
};

struct nova_inode_info_header {
	struct radix_tree_root tree;	/* Dir name entry tree root */
	struct radix_tree_root cache_tree;	/* Mmap cache tree root */
	unsigned short i_mode;		/* Dir or file? */
	unsigned long log_pages;	/* Num of log pages */
	unsigned long i_size;
	unsigned long ino;
	unsigned long pi_addr;
	unsigned long mmap_pages;	/* Num of mmap pages */
	unsigned long low_dirty;	/* Mmap dirty low range */
	unsigned long high_dirty;	/* Mmap dirty high range */
	unsigned long valid_bytes;	/* For thorough GC */
	u64 last_setattr;		/* Last setattr entry */
	u64 last_link_change;		/* Last link change entry */
};

struct nova_inode_info {
	struct nova_inode_info_header header;
	struct inode vfs_inode;
};

enum bm_type {
	BM_4K = 0,
	BM_2M,
	BM_1G,
};

struct single_scan_bm {
	unsigned long bitmap_size;
	unsigned long *bitmap;
};

struct scan_bitmap {
	struct single_scan_bm scan_bm_4K;
	struct single_scan_bm scan_bm_2M;
	struct single_scan_bm scan_bm_1G;
};

struct free_list {
	spinlock_t s_lock;
	struct rb_root	block_free_tree;
	struct nova_range_node *first_node;
	unsigned long	block_start;
	unsigned long	block_end;
	unsigned long	num_free_blocks;
	unsigned long	num_blocknode;

	/* Statistics */
	unsigned long	alloc_log_count;
	unsigned long	alloc_data_count;
	unsigned long	free_log_count;
	unsigned long	free_data_count;
	unsigned long	alloc_log_pages;
	unsigned long	alloc_data_pages;
	unsigned long	freed_log_pages;
	unsigned long	freed_data_pages;

	u64		padding[8];	/* Cache line break */
};

/*
 * The first block contains super blocks and reserved inodes;
 * The second block contains pointers to journal pages.
 * The third block contains pointers to inode tables.
 */
#define	RESERVED_BLOCKS	3

struct inode_map {
	struct mutex inode_table_mutex;
	struct rb_root	inode_inuse_tree;
	unsigned long	num_range_node_inode;
	struct nova_range_node *first_inode_range;
	int allocated;
	int freed;
};

/*
 * NOVA super-block data in memory
 */
struct nova_sb_info {
	struct super_block *sb;
	struct block_device *s_bdev;

	/*
	 * base physical and virtual address of NOVA (which is also
	 * the pointer to the super block)
	 */
	phys_addr_t	phys_addr;
	void		*virt_addr;

	unsigned long	num_blocks;

	/*
	 * Backing store option:
	 * 1 = no load, 2 = no store,
	 * else do both
	 */
	unsigned int	nova_backing_option;

	/* Mount options */
	unsigned long	bpi;
	unsigned long	num_inodes;
	unsigned long	blocksize;
	unsigned long	initsize;
	unsigned long	s_mount_opt;
	kuid_t		uid;    /* Mount uid for root directory */
	kgid_t		gid;    /* Mount gid for root directory */
	umode_t		mode;   /* Mount mode for root directory */
	atomic_t	next_generation;
	/* inode tracking */
	unsigned long	s_inodes_used_count;
	unsigned long	reserved_blocks;

	struct mutex 	s_lock;	/* protects the SB's buffer-head */

	int cpus;
	struct proc_dir_entry *s_proc;

	/* ZEROED page for cache page initialized */
	void *zeroed_page;

	/* Per-CPU journal lock */
	spinlock_t *journal_locks;

	/* Per-CPU inode map */
	struct inode_map	*inode_maps;

	/* Decide new inode map id */
	unsigned long map_id;

	/* Per-CPU free block list */
	struct free_list *free_lists;

	/* Shared free block list */
	unsigned long per_list_blocks;
	struct free_list shared_free_list;
};

static inline struct nova_sb_info *NOVA_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline struct nova_inode_info *NOVA_I(struct inode *inode)
{
	return container_of(inode, struct nova_inode_info, vfs_inode);
}

/* If this is part of a read-modify-write of the super block,
 * nova_memunlock_super() before calling! */
static inline struct nova_super_block *nova_get_super(struct super_block *sb)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);

	return (struct nova_super_block *)sbi->virt_addr;
}

static inline struct nova_super_block *nova_get_redund_super(struct super_block *sb)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);

	return (struct nova_super_block *)(sbi->virt_addr + NOVA_SB_SIZE);
}

/* If this is part of a read-modify-write of the block,
 * nova_memunlock_block() before calling! */
static inline void *nova_get_block(struct super_block *sb, u64 block)
{
	struct nova_super_block *ps = nova_get_super(sb);

	return block ? ((void *)ps + block) : NULL;
}

static inline u64
nova_get_addr_off(struct nova_sb_info *sbi, void *addr)
{
	NOVA_ASSERT((addr >= sbi->virt_addr) &&
			(addr < (sbi->virt_addr + sbi->initsize)));
	return (u64)(addr - sbi->virt_addr);
}

static inline u64
nova_get_block_off(struct super_block *sb, unsigned long blocknr,
		    unsigned short btype)
{
	return (u64)blocknr << PAGE_SHIFT;
}

static inline
struct free_list *nova_get_free_list(struct super_block *sb, int cpu)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);

	if (cpu < sbi->cpus)
		return &sbi->free_lists[cpu];
	else
		return &sbi->shared_free_list;
}

struct ptr_pair {
	__le64 journal_head;
	__le64 journal_tail;
};

static inline
struct ptr_pair *nova_get_journal_pointers(struct super_block *sb, int cpu)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);

	if (cpu >= sbi->cpus)
		return NULL;

	return (struct ptr_pair *)((char *)nova_get_block(sb,
		NOVA_DEF_BLOCK_SIZE_4K)	+ cpu * CACHELINE_SIZE);
}

struct inode_table {
	__le64 log_head;
};

static inline
struct inode_table *nova_get_inode_table(struct super_block *sb, int cpu)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);

	if (cpu >= sbi->cpus)
		return NULL;

	return (struct inode_table *)((char *)nova_get_block(sb,
		NOVA_DEF_BLOCK_SIZE_4K * 2) + cpu * CACHELINE_SIZE);
}

// BKDR String Hash Function
static inline unsigned long BKDRHash(const char *str, int length)
{
	unsigned int seed = 131; // 31 131 1313 13131 131313 etc..
	unsigned long hash = 0;
	int i;

	for (i = 0; i < length; i++) {
		hash = hash * seed + (*str++);
	}

	return hash;
}

/* uses CPU instructions to atomically write up to 8 bytes */
static inline void nova_memcpy_atomic (void *dst, const void *src, u8 size)
{
	switch (size) {
		case 1: {
			volatile u8 *daddr = dst;
			const u8 *saddr = src;
			*daddr = *saddr;
			break;
		}
		case 2: {
			volatile __le16 *daddr = dst;
			const u16 *saddr = src;
			*daddr = cpu_to_le16(*saddr);
			break;
		}
		case 4: {
			volatile __le32 *daddr = dst;
			const u32 *saddr = src;
			*daddr = cpu_to_le32(*saddr);
			break;
		}
		case 8: {
			volatile __le64 *daddr = dst;
			const u64 *saddr = src;
			*daddr = cpu_to_le64(*saddr);
			break;
		}
		default:
			nova_dbg("error: memcpy_atomic called with %d bytes\n",
					size);
			//BUG();
	}
}

static inline int memcpy_to_pmem_nocache(void *dst, const void *src,
	unsigned int size)
{
	int ret;

	ret = __copy_from_user_inatomic_nocache(dst, src, size);

	return ret;
}

/* assumes the length to be 4-byte aligned */
static inline void memset_nt(void *dest, uint32_t dword, size_t length)
{
	uint64_t dummy1, dummy2;
	uint64_t qword = ((uint64_t)dword << 32) | dword;

	asm volatile ("movl %%edx,%%ecx\n"
		"andl $63,%%edx\n"
		"shrl $6,%%ecx\n"
		"jz 9f\n"
		"1:      movnti %%rax,(%%rdi)\n"
		"2:      movnti %%rax,1*8(%%rdi)\n"
		"3:      movnti %%rax,2*8(%%rdi)\n"
		"4:      movnti %%rax,3*8(%%rdi)\n"
		"5:      movnti %%rax,4*8(%%rdi)\n"
		"8:      movnti %%rax,5*8(%%rdi)\n"
		"7:      movnti %%rax,6*8(%%rdi)\n"
		"8:      movnti %%rax,7*8(%%rdi)\n"
		"leaq 64(%%rdi),%%rdi\n"
		"decl %%ecx\n"
		"jnz 1b\n"
		"9:     movl %%edx,%%ecx\n"
		"andl $7,%%edx\n"
		"shrl $3,%%ecx\n"
		"jz 11f\n"
		"10:     movnti %%rax,(%%rdi)\n"
		"leaq 8(%%rdi),%%rdi\n"
		"decl %%ecx\n"
		"jnz 10b\n"
		"11:     movl %%edx,%%ecx\n"
		"shrl $2,%%ecx\n"
		"jz 12f\n"
		"movnti %%eax,(%%rdi)\n"
		"12:\n"
		: "=D"(dummy1), "=d" (dummy2) : "D" (dest), "a" (qword), "d" (length) : "memory", "rcx");
}

static inline struct nova_file_write_entry *
nova_get_write_entry(struct super_block *sb,
	struct nova_inode_info *si, unsigned long blocknr)
{
	struct nova_inode_info_header *sih = &si->header;
	struct nova_file_write_entry *entry;

	entry = radix_tree_lookup(&sih->tree, blocknr);

	return entry;
}

void nova_print_curr_log_page(struct super_block *sb, u64 curr);
void nova_print_nova_log(struct super_block *sb,
	struct nova_inode_info_header *sih, struct nova_inode *pi);
int nova_get_nova_log_pages(struct super_block *sb,
	struct nova_inode_info_header *sih, struct nova_inode *pi);
void nova_print_nova_log_pages(struct super_block *sb,
	struct nova_inode_info_header *sih, struct nova_inode *pi);

static inline unsigned long get_nvmm(struct super_block *sb,
	struct nova_inode_info_header *sih,
	struct nova_file_write_entry *data, unsigned long pgoff)
{
	if (data->pgoff > pgoff || (unsigned long)data->pgoff +
			(unsigned long)data->num_pages <= pgoff) {
		struct nova_sb_info *sbi = NOVA_SB(sb);
		struct nova_inode *pi;
		u64 curr;

		curr = nova_get_addr_off(sbi, data);
		nova_dbg("Entry ERROR: inode %lu, curr 0x%llx, pgoff %lu, "
			"entry pgoff %llu, num %u\n", sih->ino,
			curr, pgoff, data->pgoff, data->num_pages);
		pi = nova_get_block(sb, sih->pi_addr);
		nova_print_nova_log_pages(sb, sih, pi);
		nova_print_nova_log(sb, sih, pi);
		NOVA_ASSERT(0);
	}

	return (unsigned long)(data->block >> PAGE_SHIFT) + pgoff
		- data->pgoff;
}

static inline u64 nova_find_nvmm_block(struct super_block *sb,
	struct nova_inode_info *si, struct nova_file_write_entry *entry,
	unsigned long blocknr)
{
	unsigned long nvmm;

	if (!entry) {
		entry = nova_get_write_entry(sb, si, blocknr);
		if (!entry)
			return 0;
	}

	nvmm = get_nvmm(sb, &si->header, entry, blocknr);
	return nvmm << PAGE_SHIFT;
}

static inline unsigned long nova_get_cache_addr(struct super_block *sb,
	struct nova_inode_info *si, unsigned long blocknr)
{
	struct nova_inode_info_header *sih = &si->header;
	unsigned long addr;

	addr = (unsigned long)radix_tree_lookup(&sih->cache_tree, blocknr);
	nova_dbgv("%s: inode %lu, blocknr %lu, addr 0x%lx\n",
		__func__, sih->ino, blocknr, addr);
	return addr;
}

static inline unsigned int nova_inode_blk_shift (struct nova_inode *pi)
{
	return blk_type_to_shift[pi->i_blk_type];
}

static inline uint32_t nova_inode_blk_size (struct nova_inode *pi)
{
	return blk_type_to_size[pi->i_blk_type];
}

/*
 * ROOT_INO: Start from NOVA_SB_SIZE * 2
 */
static inline struct nova_inode *nova_get_basic_inode(struct super_block *sb,
	u64 inode_number)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);

	return (struct nova_inode *)(sbi->virt_addr + NOVA_SB_SIZE * 2 +
			 (inode_number - NOVA_ROOT_INO) * NOVA_INODE_SIZE);
}

/* If this is part of a read-modify-write of the inode metadata,
 * nova_memunlock_inode() before calling! */
static inline struct nova_inode *nova_get_inode_by_ino(struct super_block *sb,
						  u64 ino)
{
	if (ino == 0 || ino >= NOVA_NORMAL_INODE_START)
		return NULL;

	return nova_get_basic_inode(sb, ino);
}

static inline struct nova_inode *nova_get_inode(struct super_block *sb,
	struct inode *inode)
{
	struct nova_inode_info *si = NOVA_I(inode);
	struct nova_inode_info_header *sih = &si->header;

	return (struct nova_inode *)nova_get_block(sb, sih->pi_addr);
}

static inline unsigned long
nova_get_numblocks(unsigned short btype)
{
	unsigned long num_blocks;

	if (btype == NOVA_BLOCK_TYPE_4K) {
		num_blocks = 1;
	} else if (btype == NOVA_BLOCK_TYPE_2M) {
		num_blocks = 512;
	} else {
		//btype == NOVA_BLOCK_TYPE_1G
		num_blocks = 0x40000;
	}
	return num_blocks;
}

static inline unsigned long
nova_get_blocknr(struct super_block *sb, u64 block, unsigned short btype)
{
	return block >> PAGE_SHIFT;
}

static inline unsigned long nova_get_pfn(struct super_block *sb, u64 block)
{
	return (NOVA_SB(sb)->phys_addr + block) >> PAGE_SHIFT;
}

static inline int nova_is_mounting(struct super_block *sb)
{
	struct nova_sb_info *sbi = (struct nova_sb_info *)sb->s_fs_info;
	return sbi->s_mount_opt & NOVA_MOUNT_MOUNTING;
}

static inline void check_eof_blocks(struct super_block *sb,
		struct nova_inode *pi, loff_t size)
{
	if ((pi->i_flags & cpu_to_le32(NOVA_EOFBLOCKS_FL)) &&
		(size + sb->s_blocksize) > (le64_to_cpu(pi->i_blocks)
			<< sb->s_blocksize_bits))
		pi->i_flags &= cpu_to_le32(~NOVA_EOFBLOCKS_FL);
}

enum nova_new_inode_type {
	TYPE_CREATE = 0,
	TYPE_MKNOD,
	TYPE_SYMLINK,
	TYPE_MKDIR
};

static inline u64 next_log_page(struct super_block *sb, u64 curr_p)
{
	void *curr_addr = nova_get_block(sb, curr_p);
	unsigned long page_tail = ((unsigned long)curr_addr & ~INVALID_MASK)
					+ LAST_ENTRY;
	return ((struct nova_inode_page_tail *)page_tail)->next_page;
}

static inline void nova_set_next_page_address(struct super_block *sb,
	struct nova_inode_log_page *curr_page, u64 next_page, int fence)
{
	curr_page->page_tail.next_page = next_page;
	nova_flush_buffer(&curr_page->page_tail,
				sizeof(struct nova_inode_page_tail), 0);
	if (fence)
		PERSISTENT_BARRIER();
}

#define	CACHE_ALIGN(p)	((p) & ~(CACHELINE_SIZE - 1))

static inline bool is_last_entry(u64 curr_p, size_t size)
{
	unsigned int entry_end;

	entry_end = ENTRY_LOC(curr_p) + size;

	return entry_end > LAST_ENTRY;
}

static inline bool goto_next_page(struct super_block *sb, u64 curr_p)
{
	void *addr;
	u8 type;

	/* Each kind of entry takes at least 32 bytes */
	if (ENTRY_LOC(curr_p) + 32 > LAST_ENTRY)
		return true;

	addr = nova_get_block(sb, curr_p);
	type = nova_get_entry_type(addr);
	if (type == NEXT_PAGE)
		return true;

	return false;
}

static inline int is_dir_init_entry(struct super_block *sb,
	struct nova_dentry *entry)
{
	if (entry->name_len == 1 && strncmp(entry->name, ".", 1) == 0)
		return 1;
	if (entry->name_len == 2 && strncmp(entry->name, "..", 2) == 0)
		return 1;

	return 0;
}

#include "wprotect.h"

/* Function Prototypes */
extern void nova_error_mng(struct super_block *sb, const char *fmt, ...);

/* balloc.c */
int nova_alloc_block_free_lists(struct super_block *sb);
void nova_delete_free_lists(struct super_block *sb);
inline struct nova_range_node *nova_alloc_blocknode(struct super_block *sb);
inline struct nova_range_node *nova_alloc_inode_node(struct super_block *sb);
inline void nova_free_range_node(struct nova_range_node *node);
inline void nova_free_blocknode(struct super_block *sb,
	struct nova_range_node *bnode);
inline void nova_free_inode_node(struct super_block *sb,
	struct nova_range_node *bnode);
extern void nova_init_blockmap(struct super_block *sb, int recovery);
extern int nova_free_data_blocks(struct super_block *sb, struct nova_inode *pi,
	unsigned long blocknr, int num);
extern int nova_free_log_blocks(struct super_block *sb, struct nova_inode *pi,
	unsigned long blocknr, int num);
extern int nova_new_data_blocks(struct super_block *sb, struct nova_inode *pi,
	unsigned long *blocknr, unsigned int num, unsigned long start_blk,
	int zero, int cow);
extern int nova_new_log_blocks(struct super_block *sb, struct nova_inode *pi,
	unsigned long *blocknr, unsigned int num, int zero);
extern unsigned long nova_count_free_blocks(struct super_block *sb);
inline int nova_search_inodetree(struct nova_sb_info *sbi,
	unsigned long ino, struct nova_range_node **ret_node);
inline int nova_insert_blocktree(struct nova_sb_info *sbi,
	struct rb_root *tree, struct nova_range_node *new_node);
inline int nova_insert_inodetree(struct nova_sb_info *sbi,
	struct nova_range_node *new_node, int cpu);
int nova_find_free_slot(struct nova_sb_info *sbi,
	struct rb_root *tree, unsigned long range_low,
	unsigned long range_high, struct nova_range_node **prev,
	struct nova_range_node **next);

/* bbuild.c */
inline void set_bm(unsigned long bit, struct scan_bitmap *bm,
	enum bm_type type);
int nova_rebuild_inode(struct super_block *sb, struct nova_inode_info *si,
	u64 pi_addr);
void nova_save_blocknode_mappings_to_log(struct super_block *sb);
void nova_save_inode_list_to_log(struct super_block *sb);
void nova_init_header(struct super_block *sb,
	struct nova_inode_info_header *sih, u16 i_mode);
int nova_recovery(struct super_block *sb);

/*
 * Inodes and files operations
 */

/* dax.c */
int nova_reassign_file_tree(struct super_block *sb,
	struct nova_inode *pi, struct nova_inode_info_header *sih,
	u64 begin_tail);
ssize_t nova_dax_file_read(struct file *filp, char __user *buf, size_t len,
			    loff_t *ppos);
ssize_t nova_dax_file_write(struct file *filp, const char __user *buf,
		size_t len, loff_t *ppos);
int nova_dax_get_block(struct inode *inode, sector_t iblock,
	struct buffer_head *bh, int create);
int nova_dax_file_mmap(struct file *file, struct vm_area_struct *vma);

/* dir.c */
extern const struct file_operations nova_dir_operations;
int nova_append_dir_init_entries(struct super_block *sb,
	struct nova_inode *pi, u64 self_ino, u64 parent_ino);
extern int nova_add_dentry(struct dentry *dentry, u64 ino,
	int inc_link, u64 tail, u64 *new_tail);
extern int nova_remove_dentry(struct dentry *dentry, int dec_link, u64 tail,
	u64 *new_tail);
void nova_print_dir_tree(struct super_block *sb,
	struct nova_inode_info_header *sih, unsigned long ino);
void nova_delete_dir_tree(struct super_block *sb,
	struct nova_inode_info_header *sih);
struct nova_dentry *nova_find_dentry(struct super_block *sb,
	struct nova_inode *pi, struct inode *inode, const char *name,
	unsigned long name_len);
int nova_rebuild_dir_inode_tree(struct super_block *sb,
	struct nova_inode *pi, u64 pi_addr,
	struct nova_inode_info_header *sih);

/* file.c */
extern const struct inode_operations nova_file_inode_operations;
extern const struct file_operations nova_dax_file_operations;
int nova_fsync(struct file *file, loff_t start, loff_t end, int datasync);

/* inode.c */
extern const struct address_space_operations nova_aops_dax;
int nova_init_inode_inuse_list(struct super_block *sb);
extern int nova_init_inode_table(struct super_block *sb);
unsigned long nova_get_last_blocknr(struct super_block *sb,
	struct nova_inode_info_header *sih);
int nova_get_inode_address(struct super_block *sb, u64 ino,
	u64 *pi_addr, int extendable);
int nova_set_blocksize_hint(struct super_block *sb, struct inode *inode,
	struct nova_inode *pi, loff_t new_size);
struct nova_file_write_entry *nova_find_next_entry(struct super_block *sb,
	struct nova_inode_info_header *sih, pgoff_t pgoff);
extern struct inode *nova_iget(struct super_block *sb, unsigned long ino);
extern void nova_evict_inode(struct inode *inode);
extern int nova_write_inode(struct inode *inode, struct writeback_control *wbc);
extern void nova_dirty_inode(struct inode *inode, int flags);
extern int nova_notify_change(struct dentry *dentry, struct iattr *attr);
int nova_getattr(struct vfsmount *mnt, struct dentry *dentry,
		struct kstat *stat);
extern void nova_set_inode_flags(struct inode *inode, struct nova_inode *pi,
	unsigned int flags);
extern unsigned long nova_find_region(struct inode *inode, loff_t *offset,
		int hole);
void nova_apply_setattr_entry(struct super_block *sb, struct nova_inode *pi,
	struct nova_inode_info_header *sih,
	struct nova_setattr_logentry *entry);
void nova_free_inode_log(struct super_block *sb, struct nova_inode *pi);
int nova_allocate_inode_log_pages(struct super_block *sb,
	struct nova_inode *pi, unsigned long num_pages,
	u64 *new_block);
int nova_delete_file_tree(struct super_block *sb,
	struct nova_inode_info_header *sih, unsigned long start_blocknr,
	unsigned long last_blocknr, bool delete_nvmm, bool delete_mmap);
u64 nova_get_append_head(struct super_block *sb, struct nova_inode *pi,
	struct nova_inode_info_header *sih, u64 tail, size_t size,
	int *extended);
u64 nova_append_file_write_entry(struct super_block *sb, struct nova_inode *pi,
	struct inode *inode, struct nova_file_write_entry *data, u64 tail);
int nova_rebuild_file_inode_tree(struct super_block *sb,
	struct nova_inode *pi, u64 pi_addr,
	struct nova_inode_info_header *sih);
u64 nova_new_nova_inode(struct super_block *sb, u64 *pi_addr);
extern struct inode *nova_new_vfs_inode(enum nova_new_inode_type,
	struct inode *dir, u64 pi_addr, u64 ino, umode_t mode,
	size_t size, dev_t rdev, const struct qstr *qstr);
int nova_assign_write_entry(struct super_block *sb,
	struct nova_inode *pi,
	struct nova_inode_info_header *sih,
	struct nova_file_write_entry *entry,
	bool free);

/* ioctl.c */
extern long nova_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#ifdef CONFIG_COMPAT
extern long nova_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg);
#endif

/* namei.c */
extern const struct inode_operations nova_dir_inode_operations;
extern const struct inode_operations nova_special_inode_operations;
extern struct dentry *nova_get_parent(struct dentry *child);
int nova_append_link_change_entry(struct super_block *sb,
	struct nova_inode *pi, struct inode *inode, u64 tail, u64 *new_tail);
void nova_apply_link_change_entry(struct nova_inode *pi,
	struct nova_link_change_entry *entry);

/* super.c */
extern struct super_block *nova_read_super(struct super_block *sb, void *data,
	int silent);
extern int nova_statfs(struct dentry *d, struct kstatfs *buf);
extern int nova_remount(struct super_block *sb, int *flags, char *data);
int nova_check_integrity(struct super_block *sb,
	struct nova_super_block *super);
void *nova_ioremap(struct super_block *sb, phys_addr_t phys_addr,
	ssize_t size);

/* symlink.c */
extern const struct inode_operations nova_symlink_inode_operations;

/* sysfs.c */
extern const char *proc_dirname;
extern struct proc_dir_entry *nova_proc_root;
void nova_sysfs_init(struct super_block *sb);
void nova_sysfs_exit(struct super_block *sb);

/* nova_stats.c */
void nova_get_timing_stats(void);
void nova_print_timing_stats(struct super_block *sb);
void nova_clear_stats(void);
void nova_print_inode_log(struct super_block *sb, struct inode *inode);
void nova_print_inode_log_pages(struct super_block *sb, struct inode *inode);
void nova_print_free_lists(struct super_block *sb);

extern spinlock_t nvm_spinlock;

#endif /* __NOVA_H */
