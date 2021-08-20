// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /var/log/kern.log for hierarchical namespace.

#include "mlfs/mlfs_user.h"
#include "global/global.h"
#include "concurrency/synchronization.h"
#include "io/block_io.h"

#ifdef __x86_64__
#include "fs.h"
#elif __aarch64__
#include "nic/nic_fs.h"
#else
#error "Unsupported architecture."
#endif

#include "kernfs_interface.h"
#include "extents_bh.h"

#if NIC_OFFLOAD
void set_dram_root_inode(struct dinode *dip) {
	struct inode *_inode;
	mlfs_assert(dip->itype == T_DIR);

	_inode = icache_alloc_add(ROOTINO);
	_inode->_dinode = (struct dinode *)_inode;
	sync_inode_from_dinode(_inode, dip);
	_inode->flags |= I_VALID;
	_inode->i_sb = sb;
}
#endif /* NIC_OFFLOAD */

void read_root_inode()
{
	struct dinode _dinode;
	struct inode *_inode;

	printf("Reading root inode with inum: %u\n", ROOTINO);
	read_ondisk_inode(ROOTINO, &_dinode);

	mlfs_assert(_dinode.itype == T_DIR);

	_inode = icache_alloc_add(ROOTINO);
	_inode->_dinode = (struct dinode *)_inode;
	sync_inode_from_dinode(_inode, &_dinode);
	_inode->flags |= I_VALID;
	_inode->i_sb = sb;
}

int read_ondisk_inode(uint32_t inum, struct dinode *dip)
{
	int ret;
	struct buffer_head *bh;
	addr_t inode_block;

	inode_block = get_inode_block(g_root_dev, inum);
	bh = bh_get_sync_IO(g_root_dev, inode_block, BH_NO_DATA_ALLOC);

	bh->b_size = sizeof(struct dinode);
	bh->b_data = (uint8_t *)dip;
	bh->b_offset = sizeof(struct dinode) * (inum % IPB);
	bh_submit_read_sync_IO(bh);
	mlfs_io_wait(g_root_dev, 1);

	return 0;
}

int write_ondisk_inode(struct inode *ip)
{
	int ret;
	struct dinode *dip = ip->_dinode;
	struct buffer_head *bh;
	addr_t inode_block;

	inode_block = get_inode_block(g_root_dev, ip->inum);
	bh = bh_get_sync_IO(g_root_dev, inode_block, BH_NO_DATA_ALLOC);

	bh->b_size = sizeof(struct dinode);
	bh->b_data = (uint8_t *)dip;
	bh->b_offset = sizeof(struct dinode) * (ip->inum % IPB);
	ret = mlfs_write(bh);
	mlfs_io_wait(g_root_dev, 0);

	return ret;
}

#ifdef DISTRIBUTED
void update_remote_ondisk_inode(uint8_t node_id, struct inode *ip)
{
	mlfs_debug("writing remote ondisk inode inum %u to peer %u\n", ip->inum, node_id);
	addr_t offset = (get_inode_block(g_root_dev, ip->inum) << g_block_size_shift) + sizeof(struct dinode) * (ip->inum %IPB);
	addr_t src = ((uintptr_t)g_bdev[g_root_dev]->map_base_addr) + offset;
	addr_t dst = mr_remote_addr(g_peers[node_id]->sockfd[0], MR_NVM_SHARED) + offset;
	rdma_meta_t *meta = create_rdma_meta(src, dst, sizeof(struct dinode));

	IBV_WRAPPER_WRITE_ASYNC(g_peers[node_id]->sockfd[0], meta, MR_NVM_SHARED, MR_NVM_SHARED);
}

#if MLFS_LEASE
void update_remote_ondisk_lease(uint8_t node_id, mlfs_lease_t *ls)
{
	mlfs_debug("writing remote ondisk lease inum %u to peer %u\n", ls->inum, node_id);
	addr_t offset = (get_lease_block(g_root_dev, ls->inum) << g_block_size_shift) + sizeof(mlfs_lease_t) * (ls->inum %LPB);
	addr_t src = ((uintptr_t)g_bdev[g_root_dev]->map_base_addr) + offset;
	addr_t dst = mr_remote_addr(g_peers[node_id]->sockfd[0], MR_NVM_SHARED) + offset;
	rdma_meta_t *meta = create_rdma_meta(src, dst, sizeof(mlfs_lease_t));

	IBV_WRAPPER_WRITE_ASYNC(g_peers[node_id]->sockfd[0], meta, MR_NVM_SHARED, MR_NVM_SHARED);
}
#endif

#endif

void iupdate(struct inode *ip)
{
}

// Allocate a new inode with the given type on device dev.
// A free inode has a type of zero.
struct inode* ialloc(uint8_t type, uint32_t inode_nr)
{
    uint32_t inum;
	int ret;
    struct buffer_head *bp;
    struct dinode dip;
    struct inode *ip;

	// allocate with empty inode number
	if (inode_nr == 0) {
		for (inum = 1; inum < disk_sb[g_root_dev].ninodes; inum++) {
			read_ondisk_inode(inum, &dip);

			if (dip.itype == 0) {  // a free inode
				memset(&dip, 0, sizeof(struct dinode));
				dip.itype = type;

				/* add in-memory inode to icache */
				ip = iget(inum);
				ip->_dinode = (struct dinode *)ip;
				sync_inode_from_dinode(ip, &dip);
				ip->flags |= I_VALID;

				ip->i_sb = sb;
				ip->i_generation = 0;
				ip->i_data_dirty = 0;
				ip->nlink = 1;

				return ip;
			}

		}
	} else {
		ip = icache_find(inode_nr);

		if (!ip)
			ip = iget(inode_nr);
		else 
			ip->i_ref++;

		ip->_dinode = (struct dinode *)ip;

		if (!(ip->flags & I_VALID)) {
			read_ondisk_inode(inode_nr, &dip);

			if (dip.itype == 0) {
				memset(&dip, 0, sizeof(struct dinode));
				dip.itype = type;
			}

			sync_inode_from_dinode(ip, &dip);
			ip->flags |= I_VALID;
		}

		ip->i_sb = sb;
		ip->i_generation = 0;
		ip->i_data_dirty = 0;
		ip->nlink = 1;

		return ip;
	}

    panic("ialloc: no inodes");

    return NULL;
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
struct inode* iget(uint32_t inum)
{
	struct inode *ip;

	ip = icache_find(inum);

	if (ip) {
		ip->i_ref++;
		return ip;
	}

	ip = icache_alloc_add(inum);

	return ip;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode* idup(struct inode *ip)
{
	ip->i_ref++;
	return ip;
}

// Lock the given inode; set iflag of I_BUSY and I_VALID
// Reads the inode from disk if necessary.
void ilock(struct inode *ip)
{
}

// Unlock the given inode.
void iunlock(struct inode *ip)
{
}

/* iput does not deallocate inode. it just drops reference count. 
 * An inode is explicitly deallocated by ideallc() 
 */
void iput(struct inode *ip)
{
	ip->i_ref--;
}

int idealloc(struct inode *inode)
{
	struct dinode dip;

	mlfs_assert(inode->i_ref < 2);
	mlfs_assert(inode->flags & I_DELETING);

	if (inode->i_ref == 1 && 
			(inode->flags & I_VALID) && 
			inode->nlink == 0) {
		if (inode->flags & I_BUSY)
			panic("Inode must not be busy!");

		inode->flags &= ~I_BUSY;
	}

	read_ondisk_inode(inode->inum, &dip);

	mlfs_assert(inode->itype == dip.itype);

	ilock(inode);
	inode->size = 0;
	/* After persisting the inode, libfs moves it to
	 * deleted inode hash table in persist_log_inode() */
	inode->itype = 0;

	iunlock(inode);

	mlfs_debug("delete %u\n", inode->inum);

	return 0;
}

// Common idiom: unlock, then put.
void iunlockput(struct inode *ip)
{
	iunlock(ip);
	iput(ip);
}

// Truncate inode (discard contents).
// Only called when the inode has no links
// to it (no directory entries referring to it)
// and has no in-memory reference to it (is
// not an open file or current directory).
void itrunc(struct inode *ip)
{
	panic("truncate is not supported\n");
}
