#include <malloc.h>
#include <memory.h>
#include <string.h>
#include "extents.h"
#include "extents_bh.h"
#include "global/util.h"
#include "io/balloc.h"
#include "storage/storage.h"
#ifdef KERNFS
#include "migrate.h"
#endif

/*
 * used by extent splitting.
 */
#define MLFS_EXT_MAY_ZEROOUT  \
    0x1				  /* safe to zeroout if split fails \
				     due to ENOSPC */
#define MLFS_EXT_MARK_UNWRIT1 0x2 /* mark first half unwritten */
#define MLFS_EXT_MARK_UNWRIT2 0x4 /* mark second half unwritten */

#define MLFS_EXT_DATA_VALID1 0x8  /* first half contains valid data */
#define MLFS_EXT_DATA_VALID2 0x10 /* second half contains valid data */

#define CONFIG_EXTENT_TEST

#define mlfs_lsm_debug mlfs_debug
#define mlfs_lsm_info mlfs_info

#define MLFS_ERROR_INODE(inode, str, ...)                        \
	do {                                                         \
		mlfs_info("inode[%p]: " str "\n", inode, __VA_ARGS__); \
	} while (0)

// For optimization
//#define REUSE_PREVIOUS_PATH

#define BUG_ON(x) mlfs_assert((x) == 0)

pthread_mutex_t block_bitmap_mutex;
pthread_spinlock_t inode_dirty_mutex;

pthread_mutex_t tree_init_lock; // TODO is it required? (making inode dirty)

void mlfs_ext_init_locks(void)
{
    //pthread_mutex_init(&inode_dirty_mutex, &attr);
    pthread_spin_init(&inode_dirty_mutex, PTHREAD_PROCESS_PRIVATE);
    pthread_mutex_init(&tree_init_lock, NULL);
}

static struct inode *__buffer_search(struct rb_root *root,
					   uint32_t inum)
{
	struct rb_node *_new = root->rb_node;

	/* Figure out where to put new node */
	while (_new) {
		struct inode *ip =
		    container_of(_new, struct inode, i_rb_node);
		int64_t result = inum - ip->inum;

		if (result < 0)
			_new = _new->rb_left;
		else if (result > 0)
			_new = _new->rb_right;
		else
			return ip;
	}

	return NULL;
}

static int inode_cmp(struct rb_node *a, struct rb_node *b)
{
	struct inode *a_inode, *b_inode;
	a_inode = container_of(a, struct inode, i_rb_node);
	b_inode = container_of(b, struct inode, i_rb_node);

	if (a_inode->inum < b_inode->inum)
		return -1;
	else if (a_inode->inum > b_inode->inum)
		return 1;

	return 0;
}

int mlfs_mark_inode_dirty(int id, struct inode *inode) 
{
	int ret;
	struct super_block *sb = get_inode_sb(g_root_dev, inode);
	inode->i_data_dirty = 1;
	mlfs_assert(sb != NULL);

	mlfs_debug("mark inode as dirty inum %d log_id %d\n", inode->inum, id);
	// lock
	//pthread_mutex_lock(&inode_dirty_mutex);
	pthread_spin_lock(&inode_dirty_mutex);  // JYKIM ADDED not sure..FIXME
	ret = rb_insert(&sb->s_dirty_root[id],
			&inode->i_rb_node, inode_cmp);
	// unlock
	//pthread_mutex_unlock(&inode_dirty_mutex);
	pthread_spin_unlock(&inode_dirty_mutex); // JYKIM ADDED not sure..FIXME
#ifdef REUSE_PREVIOUS_PATH 
	inode->invalidate_path = 1;
#endif

	return 0;
}

static inline uint32_t get_block_size(struct inode *inode) 
{
	//return g_bdev[inode->dev]->bd_blocksize;
	return g_block_size_bytes;
}

static inline int mlfs_handle_dirty_metadata(handle_t *handle, 
		struct inode *inode, struct buffer_head *bh)
{
	set_buffer_dirty(bh);
	return 0;
}

int mlfs_ext_alloc_blocks(handle_t *handle, struct inode *inode,
		int goal, unsigned int flags, mlfs_fsblk_t *blockp, mlfs_lblk_t *count)
{
	struct super_block *sb = get_inode_sb(handle->dev, inode);
	int ret;
	int retry_count = 0;
	enum alloc_type a_type;
	
	if (flags & MLFS_GET_BLOCKS_CREATE_DATA_LOG)
		a_type = DATA_LOG;
	else if (flags & MLFS_GET_BLOCKS_CREATE_META)
		a_type = TREE;
	else 
		a_type = DATA;

retry:
	ret = mlfs_new_blocks(get_inode_sb(handle->dev, inode), blockp, 
			*count, 0, 0, a_type, goal);

	if (ret > 0) {
		//mlfs_assert(*blockp >= disk_sb[handle->dev].datablock_start);
		*count = ret;
		pthread_mutex_lock(&block_bitmap_mutex);    // Concurrent support.
		bitmap_bits_set_range(get_inode_sb(handle->dev, inode)->s_blk_bitmap, 
				*blockp, *count);
		// pthread_mutex_unlock(&block_bitmap_mutex);  // Concurrent support.
		// printf("%lu [USED_BLOCKS] ADD inode=%u used_blk_prev=%lu "
		//        "count=%u "
		//        "used_blk=%lu\n",
		//        get_tid(), inode->inum,
		//        get_inode_sb(handle->dev, inode)->used_blocks, *count,
		//        get_inode_sb(handle->dev, inode)->used_blocks + *count);
		get_inode_sb(handle->dev, inode)->used_blocks += *count;
		pthread_mutex_unlock(&block_bitmap_mutex);  // Concurrent support.
	} else if (ret == -ENOSPC) {
		retry_count++;

		if (retry_count > 2)
			panic("Fail to allocate block\n");

#ifdef KERNFS
		try_migrate_blocks(g_root_dev, g_ssd_dev, 0, 0, 1);
#endif

		goto retry;
	}

#if 0 // old code of using NAIVE BLOCK ALLOCATOR
	pthread_mutex_lock(&block_bitmap_mutex);

retry:
	ret = bitmap_find_next_contiguous_clr(sb->s_blk_bitmap, 
			sb->last_block_allocated,
			sb->ondisk->ndatablocks - 1,
			*count, blockp);

	if (*blockp == 0) {
		sb->last_block_allocated = 0;

#ifdef KERNFS
		if (retry_count == 1) {
			try_migrate_blocks(g_root_dev, g_ssd_dev, 0, 1);
			goto retry;
		}
#endif

		if (retry_count == 2) 
			panic("File system is full!\n");

		retry_count++;
		goto retry;
	}

	if (!ret) 
		bitmap_bits_set_range(sb->s_blk_bitmap, *blockp, *count);

	sb->last_block_allocated = *blockp + *count - 1;

	if (ret) {
		// TODO: set the last block allocated to 0 and research.
		panic("fail to allocate multiple blocks\n");
	}

	sb->used_blocks += *count;

	if (sb->used_blocks >= sb->ondisk->size)
		panic("File system is full!\n");

	pthread_mutex_unlock(&block_bitmap_mutex);
#endif
	
	return ret;
}

static inline mlfs_fsblk_t mlfs_inode_to_goal_block(struct inode *inode) 
{
	return 0;
}

/* used for file data blocks in extent tree */
static mlfs_fsblk_t mlfs_new_data_blocks(handle_t *handle,
		struct inode *inode, int goal, unsigned int flags,
		mlfs_lblk_t *count, int *errp) 
{
	struct super_block *sb = get_inode_sb(handle->dev, inode);
	mlfs_fsblk_t block = 0;
	mlfs_lblk_t nrblocks = (count) ? (*count) : 1;

	*errp = mlfs_ext_alloc_blocks(handle, inode, goal, flags, &block, count);
	
	mlfs_debug("[dev %u] used blocks %d\n", g_root_dev,
			bitmap_weight((uint64_t *)inode->i_sb[handle->dev]->s_blk_bitmap->bitmap,
				inode->i_sb[handle->dev]->ondisk->ndatablocks));

	return block;
}

/* used for internal node blocks in extent tree */
static mlfs_fsblk_t mlfs_new_meta_blocks(handle_t *handle,
		struct inode *inode, mlfs_fsblk_t goal, unsigned int flags,
		mlfs_lblk_t *count, int *errp) 
{
	mlfs_fsblk_t block = 0;
	mlfs_lblk_t nrblocks = (count) ? (*count) : 1;

	flags |= MLFS_GET_BLOCKS_CREATE_META;

	*errp = mlfs_ext_alloc_blocks(handle, inode, goal, flags, &block, count);

	mlfs_debug("[dev %u] used blocks %d\n", g_root_dev,
			bitmap_weight((uint64_t *)inode->i_sb[handle->dev]->s_blk_bitmap->bitmap,
				inode->i_sb[handle->dev]->ondisk->ndatablocks));

	return block;
}

static void mlfs_free_blocks(handle_t *handle, struct inode *inode,
		void *fake, mlfs_fsblk_t block, int count, int flags) 
{
	int ret;
	struct buffer_head *bh, *tmp;
	struct super_block *sb = get_inode_sb(handle->dev, inode);
	UNUSED(flags);

	ret = mlfs_free_blocks_node(get_inode_sb(handle->dev, inode), 
			block, count, 0, 0);
	mlfs_assert(ret == 0);

	pthread_mutex_lock(&block_bitmap_mutex);    // Concurrent support.
	bitmap_bits_free(sb->s_blk_bitmap, block, count);
	// pthread_mutex_unlock(&block_bitmap_mutex);  // Concurrent support.

	mlfs_debug("inode %u free blocks %lu - %lu \n", 
			inode->inum, block, block + count - 1);

	mlfs_debug("[dev %u] used blocks %d\n", g_root_dev,
			bitmap_weight((uint64_t *)sb->s_blk_bitmap->bitmap,
				sb->ondisk->ndatablocks));

	mlfs_assert(sb->used_blocks > count);

	// printf("%lu [USED_BLOCKS] SUB inode=%u used_blk_prev=%lu count=%u "
	//        "used_blk=%lu\n",
	//        get_tid(), inode->inum, sb->used_blocks, count,
	//        sb->used_blocks - count);

	sb->used_blocks -= count;

	pthread_mutex_unlock(&block_bitmap_mutex);  // Concurrent support.

	// FIXME: This code has a problem that makes unlink digest very slow
	// especially in Varmail workload. 
#if 0
	/* remove previously issued block io from writeback list.
	 * Tree internal nodes (meta blocks) are registered to a dirty writeback list,
	 * and persisted at the end of digest, sync_all_buffers().
	 * Problem is data blocks are written synchronously. So, block write ordering
	 * is not guaranteed; Meta blocks previously allocated and freed might overwrite
	 * reused data blocks. e.g, happens in mlfs_ext_try_to_merge_up().
	 * To avoid this side effect, the block free function eliminates
	 * previous buffer head from the dirty writeback list.
	 */
	//pthread_mutex_lock(&sb->s_bdev->bd_bh_dirty_lock);
	list_for_each_entry_safe(bh, tmp, &sb->s_bdev->bd_bh_dirty, b_dirty_list)
	{
		if (bh->b_blocknr == block) {
			remove_buffer_from_writeback(bh);
			break;
		}
	}
	//pthread_mutex_unlock(&sb->s_bdev->bd_bh_dirty_lock);
#endif

	return;
}

/*
 * Return the right sibling of a tree node(either leaf or indexes node)
 */

#define EXT_MAX_BLOCKS 0xffffffff

static inline int mlfs_ext_space_block(struct inode *inode, int check) 
{
	int size;

	size = (get_block_size(inode) - sizeof(struct mlfs_extent_header)) /
		sizeof(struct mlfs_extent);
#ifdef AGGRESSIVE_TEST
	if (!check && size > 6) size = 6;
#endif
	return size;
}

static inline int mlfs_ext_space_block_idx(struct inode *inode, int check) 
{
	int size;

	size = (get_block_size(inode) - sizeof(struct mlfs_extent_header)) /
		sizeof(struct mlfs_extent_idx);
#ifdef AGGRESSIVE_TEST
	if (!check && size > 5) size = 5;
#endif
	return size;
}

static inline int mlfs_ext_space_root(struct inode *inode, int check) 
{
	int size;

	size = sizeof(inode->l1.i_block);
	size -= sizeof(struct mlfs_extent_header);
	size /= sizeof(struct mlfs_extent);
#ifdef AGGRESSIVE_TEST
	if (!check && size > 3) size = 3;
#endif
	return size;
}

static inline int mlfs_ext_space_root_idx(struct inode *inode, int check)
{
	int size;

	size = sizeof(inode->l1.i_block);
	size -= sizeof(struct mlfs_extent_header);
	size /= sizeof(struct mlfs_extent_idx);
#ifdef AGGRESSIVE_TEST
	if (!check && size > 4) size = 4;
#endif
	return size;
}

static int mlfs_ext_max_entries(handle_t *handle, struct inode *inode, int depth)
{
	int max;

	if (depth == ext_depth(handle, inode)) {
		if (depth == 0)
			max = mlfs_ext_space_root(inode, 1);
		else
			max = mlfs_ext_space_root_idx(inode, 1);
	} else {
		if (depth == 0)
			max = mlfs_ext_space_block(inode, 1);
		else
			max = mlfs_ext_space_block_idx(inode, 1);
	}

	return max;
}

static int mlfs_ext_check(struct inode *inode, struct mlfs_extent_header *eh,
		int depth, mlfs_fsblk_t pblk);

int mlfs_ext_tree_init(handle_t *handle, struct inode *inode) 
{
	struct mlfs_extent_header *eh;

#ifdef KERNFS
	pthread_mutex_lock(&tree_init_lock);
#endif

	eh = ext_inode_hdr(handle, inode);
	eh->eh_depth = 0;
	eh->eh_entries = 0;
	eh->eh_magic = cpu_to_le16(MLFS_EXT_MAGIC);
	eh->eh_max = cpu_to_le16(mlfs_ext_space_root(inode, 0));
	mlfs_mark_inode_dirty(handle->libfs, inode);

#ifdef KERNFS
	pthread_mutex_unlock(&tree_init_lock);
#endif

	return 0;
}

void mlfs_ext_init(struct super_block *sb) {
	/*
	 * possible initialization would be here
	 */
}

/*
 * read_extent_tree_block:
 * Get a buffer_head by fs_bread, and read fresh data from the storage.
 */
static struct buffer_head *read_extent_tree_block(handle_t *handle,
		struct inode *inode, mlfs_fsblk_t pblk, int depth, int flags) 
{
	struct buffer_head *bh;
	int err;

	bh = fs_bread(handle->dev, pblk, &err);
	if (!bh) 
		return (struct buffer_head *)ERR_PTR(-ENOMEM);

	if (!buffer_uptodate(bh)) {
		err = -EIO;
		goto errout;
	}

	if (buffer_verified(bh)) 
		return bh;

	err = mlfs_ext_check(inode, ext_block_hdr(bh), depth, pblk);
	if (err) 
		goto errout;

	set_buffer_verified(bh);
	return bh;

errout:
	fs_brelse(bh);
	return (struct buffer_head *)ERR_PTR(err);
}

int mlfs_ext_check_inode(handle_t *handle, struct inode *inode) 
{
	return mlfs_ext_check(inode, ext_inode_hdr(handle, inode), 
			ext_depth(handle, inode), 0);
}

static uint32_t mlfs_ext_block_csum(struct inode *inode,
		struct mlfs_extent_header *eh) 
{
	/*return mlfs_crc32c(inode->i_csum, eh, MLFS_EXTENT_TAIL_OFFSET(eh));*/
	return ET_CHECKSUM_MAGIC;
}

static void mlfs_extent_block_csum_set(struct inode *inode,
		struct mlfs_extent_header *eh) 
{
	struct mlfs_extent_tail *tail;

	tail = find_mlfs_extent_tail(eh);
	tail->et_checksum = mlfs_ext_block_csum(inode, eh);
}

static int mlfs_split_extent_at(handle_t *handle,
		struct inode *inode,
		struct mlfs_ext_path **ppath, mlfs_lblk_t split,
		int split_flag, int flags);

static inline int mlfs_force_split_extent_at(handle_t *handle,
		struct inode *inode,
		struct mlfs_ext_path **ppath,
		mlfs_lblk_t lblk, int nofail) 
{
	struct mlfs_ext_path *path = *ppath;
	int unwritten = mlfs_ext_is_unwritten(path[path->p_depth].p_ext);

	return mlfs_split_extent_at(handle, inode, ppath, lblk,
			unwritten ? MLFS_EXT_MARK_UNWRIT1 | MLFS_EXT_MARK_UNWRIT2 : 0,
			MLFS_EX_NOCACHE | MLFS_GET_BLOCKS_PRE_IO |
			(nofail ? MLFS_GET_BLOCKS_METADATA_NOFAIL : 0));
}

static mlfs_fsblk_t mlfs_ext_find_goal(struct inode *inode,
		struct mlfs_ext_path *path, mlfs_lblk_t block) 
{
	if (path) {
		int depth = path->p_depth;
		struct mlfs_extent *ex;

		/*
		 * Try to predict block placement assuming that we are
		 * filling in a file which will eventually be
		 * non-sparse --- i.e., in the case of libbfd writing
		 * an ELF object sections out-of-order but in a way
		 * the eventually results in a contiguous object or
		 * executable file, or some database extending a table
		 * space file.  However, this is actually somewhat
		 * non-ideal if we are writing a sparse file such as
		 * qemu or KVM writing a raw image file that is going
		 * to stay fairly sparse, since it will end up
		 * fragmenting the file system's free space.  Maybe we
		 * should have some hueristics or some way to allow
		 * userspace to pass a hint to file system,
		 * especially if the latter case turns out to be
		 * common.
		 */
		ex = path[depth].p_ext;
		if (ex) {
			mlfs_fsblk_t ext_pblk = mlfs_ext_pblock(ex);
			mlfs_lblk_t ext_block = le32_to_cpu(ex->ee_block);

			if (block > ext_block)
				return ext_pblk + (block - ext_block);
			else
				return ext_pblk - (ext_block - block);
		}

		/* it looks like index is empty;
		 * try to find starting block from index itself */
		if (path[depth].p_bh) 
			return path[depth].p_bh->b_blocknr;
	}

	/* OK. use inode's group */
	return mlfs_inode_to_goal_block(inode);
}

/*
 * Allocation for a meta data block
 */
static mlfs_fsblk_t mlfs_ext_new_meta_block(handle_t *handle,
		struct inode *inode,
		struct mlfs_ext_path *path,
		struct mlfs_extent *ex, int *err,
		unsigned int flags) 
{
	mlfs_fsblk_t goal, newblock;
	mlfs_lblk_t count = 1;	

	//goal = mlfs_ext_find_goal(inode, path, le32_to_cpu(ex->ee_block));
	mlfs_debug("meta: start offset %u, len %u\n", ex->ee_block, ex->ee_len);
	flags |= MLFS_GET_BLOCKS_CREATE_META;
	newblock = mlfs_new_meta_blocks(handle, inode, goal, flags, &count, err);

	return newblock;
}

int __mlfs_ext_dirty(const char *where, unsigned int line,
		handle_t *handle, struct inode *inode,
		struct mlfs_ext_path *path) 
{
	int err = 0;

	if (path->p_bh) {
		/* path points to block */
		mlfs_extent_block_csum_set(inode, ext_block_hdr(path->p_bh));
		fs_mark_buffer_dirty(path->p_bh);
	} else {
		/* path points to leaf/index in inode body */
		err = mlfs_mark_inode_dirty(handle->libfs, inode);
	}

#ifdef REUSE_PREVIOUS_PATH 
	inode->invalidate_path = 1;
#endif
	return err;
}

void mlfs_ext_drop_refs(struct mlfs_ext_path *path) 
{
	int depth, i;

	if (!path) 
		return;

	depth = path->p_depth;
	for (i = 0; i <= depth; i++, path++)
		if (path->p_bh) {
			fs_brelse(path->p_bh);
			path->p_bh = NULL;
		}
}

/*
 * Check that whether the basic information inside the extent header
 * is correct or not.
 */
static int mlfs_ext_check(struct inode *inode, struct mlfs_extent_header *eh,
		int depth, mlfs_fsblk_t pblk) 
{
	struct mlfs_extent_tail *tail;
	const char *error_msg;
	int max = 0;

	if (eh->eh_magic != MLFS_EXT_MAGIC) {
		error_msg = "invalid magic";
		goto corrupted;
	}
	if (le16_to_cpu(eh->eh_depth) != depth) {
		error_msg = "unexpected eh_depth";
		goto corrupted;
	}
	if (eh->eh_max == 0) {
		error_msg = "invalid eh_max";
		goto corrupted;
	}
	if (eh->eh_entries > eh->eh_max) {
		error_msg = "invalid eh_entries";
		goto corrupted;
	}

	tail = find_mlfs_extent_tail(eh);
	if (tail->et_checksum != mlfs_ext_block_csum(inode, eh)) {
		mlfs_lsm_info(
				"Warning: extent checksum damaged? tail->et_checksum = "
				"%u, mlfs_ext_block_csum = %u\n",
				tail->et_checksum, mlfs_ext_block_csum(inode, eh));
	}

	return 0;

corrupted:
	mlfs_info("corrupted block %lu\n", pblk);
	panic(error_msg);
	return -EIO;
}

/*
 * mlfs_ext_binsearch_idx:
 * binary search for the closest index of the given block
 * the header must be checked before calling this
 */
static void mlfs_ext_binsearch_idx(struct inode *inode,
		struct mlfs_ext_path *path, mlfs_lblk_t block) 
{
	struct mlfs_extent_header *eh = path->p_hdr;
	struct mlfs_extent_idx *r, *l, *m;

	//mlfs_lsm_debug("binsearch for %x(idx):\n", block);

	l = EXT_FIRST_INDEX(eh) + 1;
	r = EXT_LAST_INDEX(eh);

	while (l <= r) {
		m = l + (r - l) / 2;
		if (block < mlfs_idx_lblock(m))
			r = m - 1;
		else
			l = m + 1;
		/*
		mlfs_lsm_debug("%p(%x):%p(%x):%p(%x) ", l, (l->ei_block), m,
				(m->ei_block), r, (r->ei_block));
		*/
	}

	path->p_idx = l - 1;
	
	/*
	mlfs_lsm_debug("  -> %u->%lx\n", mlfs_idx_lblock(path->p_idx),
			mlfs_idx_pblock(path->p_idx));
	*/

#ifdef CHECK_BINSEARCH
	{
		struct mlfs_extent_idx *chix, *ix;
		int k;

		chix = ix = EXT_FIRST_INDEX(eh);
		for (k = 0; k < (eh->eh_entries); k++, ix++) {
			if (k != 0 && (ix->ei_block) <= (ix[-1].ei_block)) {
				printk(KERN_DEBUG
						"k=%d, ix=0x%p, "
						"first=0x%p\n",
						k, ix, EXT_FIRST_INDEX(eh));
				printk(KERN_DEBUG "%u <= %u\n", (ix->ei_block),
						(ix[-1].ei_block));
			}
			BUG_ON(k && (ix->ei_block) <= (ix[-1].ei_block));
			if (block < (ix->ei_block)) 
				break;
			chix = ix;
		}
		BUG_ON(chix != path->p_idx);
	}
#endif
}

/*
 * mlfs_ext_binsearch:
 * binary search for closest extent of the given block
 * the header must be checked before calling this
 * When returning, it sets proper extents to path->p_ext.
 */
static void mlfs_ext_binsearch(struct inode *inode, struct mlfs_ext_path *path,
		mlfs_lblk_t block) 
{
	struct mlfs_extent_header *eh = path->p_hdr;
	struct mlfs_extent *r, *l, *m;

	if (eh->eh_entries == 0) {
		/*
		 * this leaf is empty:
		 * we get such a leaf in split/add case
		 */
		return;
	}

	l = EXT_FIRST_EXTENT(eh) + 1;
	r = EXT_LAST_EXTENT(eh);

	while (l <= r) {
		m = l + (r - l) / 2;
		if (block < mlfs_ext_lblock(m))
			r = m - 1;
		else
			l = m + 1;

		mlfs_lsm_debug("%p(%u): %p(%u): %p(%u) ", l, l->ee_block, m,
				(m->ee_block), r, (r->ee_block));
	}

	path->p_ext = l - 1;
	mlfs_lsm_debug("  -> %d:%lx:[%d]%d\n", (path->p_ext->ee_block),
			mlfs_ext_pblock(path->p_ext),
			mlfs_ext_is_unwritten(path->p_ext),
			mlfs_ext_get_actual_len(path->p_ext));

#ifdef CHECK_BINSEARCH
	{
		struct mlfs_extent *chex, *ex;
		int k;

		chex = ex = EXT_FIRST_EXTENT(eh);
		for (k = 0; k < le16_to_cpu(eh->eh_entries); k++, ex++) {
			BUG_ON(k && (ex->ee_block) <= (ex[-1].ee_block));
			if (block < (ex->ee_block)) break;
			chex = ex;
		}
		BUG_ON(chex != path->p_ext);
	}
#endif
}

#if 1 
static void mlfs_ext_show_path(struct inode *inode, struct mlfs_ext_path *path) 
{
	int k, l = path->p_depth;

	mlfs_info("--------- path dump inum = %u\n", inode->inum);
	for (k = 0; k <= l; k++, path++) {
		if (path->p_idx) {
			mlfs_info("idx : lb %d @ %lu\n", le32_to_cpu(path->p_idx->ei_block),
					mlfs_idx_pblock(path->p_idx));
		} else if (path->p_ext) {
			mlfs_info("leaf: lb %u:[%d] ~ %u @ pb %lu ~ %lu\n",
					le32_to_cpu(path->p_ext->ee_block),
					mlfs_ext_is_unwritten(path->p_ext),
					(mlfs_ext_get_actual_len(path->p_ext) - 1) << 12,
					mlfs_ext_pblock(path->p_ext),
					mlfs_ext_pblock(path->p_ext) + mlfs_ext_get_actual_len(path->p_ext) - 1
					);
		} else
			mlfs_info("%s\n", "  []");
	}
}

static void mlfs_ext_show_leaf(handle_t *handle, struct inode *inode,
		struct mlfs_ext_path *path) 
{
	int depth = ext_depth(handle, inode);
	struct mlfs_extent_header *eh;
	struct mlfs_extent *ex;
	int i;

	if (!path) 
		return;

	eh = path[depth].p_hdr;
	ex = EXT_FIRST_EXTENT(eh);

	mlfs_lsm_debug("--------- leaf (inum %u)\n", inode->inum);

	for (i = 0; i < le16_to_cpu(eh->eh_entries); i++, ex++) {
		mlfs_lsm_debug("lb %u:[%d] ~ %u @ %lx\n", 
				le32_to_cpu(ex->ee_block),
				mlfs_ext_is_unwritten(ex), 
				(mlfs_ext_get_actual_len(ex) - 1) << 12,
				mlfs_ext_pblock(ex));
	}
}

static void mlfs_ext_show_move(handle_t *handle, struct inode *inode, 
		struct mlfs_ext_path *path, mlfs_fsblk_t newblock, int level) 
{
	int depth = ext_depth(handle, inode);
	struct mlfs_extent *ex;

	if (depth != level) {
		struct mlfs_extent_idx *idx;
		idx = path[level].p_idx;
		while (idx <= EXT_MAX_INDEX(path[level].p_hdr)) {
			mlfs_lsm_debug("%d: move %d:%lx in new index %lx\n", level,
					le32_to_cpu(idx->ei_block), mlfs_idx_pblock(idx),
					newblock);
			idx++;
		}

		return;
	}

	ex = path[depth].p_ext;
	while (ex <= EXT_MAX_EXTENT(path[depth].p_hdr)) {
		mlfs_lsm_debug("move %d:%lx:[%d]%d in new leaf %lx\n",
				le32_to_cpu(ex->ee_block), mlfs_ext_pblock(ex),
				mlfs_ext_is_unwritten(ex), mlfs_ext_get_actual_len(ex),
				newblock);
		ex++;
	}
}

#else
#define mlfs_ext_show_path(inode, path) 
#define mlfs_ext_show_leaf(inode, path)
#define mlfs_ext_show_move(inode, path, newblock, level)
#endif

// debug functions
void mlfs_ext_dump(uint8_t dev, uint32_t inum)
{
	struct inode *inode;
	struct mlfs_ext_path *path = NULL;
	handle_t handle = {.dev = dev};

	inode = iget(inum);

	if (!(inode->flags & I_VALID)) {
		struct dinode dip;
		int ret;

		read_ondisk_inode(inum, &dip);

		if(dip.itype == 0) {
			mlfs_info("inum %d does not exist\n", inum);
			iput(inode);

			return;
		}

		inode->_dinode = (struct dinode *)inode;
		sync_inode_from_dinode(inode, &dip);
		inode->flags |= I_VALID;
	}

	// TODO: Traverse all nodes in the tree to dump entire tree.
	
	path = mlfs_find_extent(&handle, inode, 0, NULL, 0);

	mlfs_ext_show_path(inode, path);

	mlfs_free(path);

	iput(inode);
}


/* path works like cursor of extent tree.
 * path[0] is root of the tree (stored in inode->i_data)
 */
struct mlfs_ext_path *mlfs_find_extent(handle_t *handle,
		struct inode *inode, mlfs_lblk_t block,
		struct mlfs_ext_path **orig_path, int flags) 
{
	struct mlfs_extent_header *eh;
	struct buffer_head *bh;
	struct mlfs_ext_path *path = orig_path ? *orig_path : NULL;
	short int depth, i, ppos = 0;
	int ret;

	eh = ext_inode_hdr(handle, inode);
	depth = ext_depth(handle, inode);

	if (path) {
		mlfs_ext_drop_refs(path);
		if (depth > path[0].p_maxdepth) {
			mlfs_free(path);
			*orig_path = path = NULL;
		}
	}

	if (!path) {
		/* account possible depth increase */
		path = (struct mlfs_ext_path *)mlfs_zalloc(
				sizeof(struct mlfs_ext_path) * (depth + 2));
		if (unlikely(!path)) 
			return (struct mlfs_ext_path *)ERR_PTR(-ENOMEM);
		path[0].p_maxdepth = depth + 1;
	}

	path[0].p_hdr = eh;
	// buffer_head of root is always NULL.
	path[0].p_bh = NULL;

	i = depth;
	/* walk through internal nodes (index nodes) of the tree from a root */
	while (i) {
		mlfs_lsm_debug("depth %d: num %d, max %d\n", ppos,
				le16_to_cpu(eh->eh_entries), le16_to_cpu(eh->eh_max));

		/* set the nearest index node */
		mlfs_ext_binsearch_idx(inode, path + ppos, block);

		path[ppos].p_block = mlfs_idx_pblock(path[ppos].p_idx);
		path[ppos].p_depth = i;
		path[ppos].p_ext = NULL;

		i--;

		bh = read_extent_tree_block(handle, inode, path[ppos].p_block, i, flags);
		if (unlikely(IS_ERR(bh))) {
			ret = PTR_ERR(bh);
			goto err;
		}

		eh = ext_block_hdr(bh);

		ppos++;
		if (unlikely(ppos > depth)) {
			fs_brelse(bh);
			MLFS_ERROR_INODE(inode, "ppos %d > depth %d", ppos, depth);
			ret = -EIO;
			goto err;
		}

		path[ppos].p_bh = bh;
		path[ppos].p_hdr = eh;
	}

	path[ppos].p_depth = i;
	path[ppos].p_ext = NULL;
	path[ppos].p_idx = NULL;

	/* Search leaf node (extent) of the tree */
	mlfs_ext_binsearch(inode, path + ppos, block);

	/* if not an empty leaf */
	if (path[ppos].p_ext)
		path[ppos].p_block = mlfs_ext_pblock(path[ppos].p_ext);

	return path;

err:
	mlfs_ext_drop_refs(path);
	if (path) {
		mlfs_free(path);
		if (orig_path) 
			*orig_path = NULL;
	}
	return (struct mlfs_ext_path *)ERR_PTR(ret);
}

/*
 * mlfs_ext_insert_index:
 * insert new index [@logical;@ptr] into the block at @curp;
 * check where to insert: before @curp or after @curp
 */
static int mlfs_ext_insert_index(handle_t *handle, struct inode *inode, 
		struct mlfs_ext_path *curp, int logical, mlfs_fsblk_t ptr) 
{
	struct mlfs_extent_idx *ix;
	int len, err;

	if (unlikely(logical == le32_to_cpu(curp->p_idx->ei_block))) {
		MLFS_ERROR_INODE(inode, "logical %d == ei_block %d!", logical,
				le32_to_cpu(curp->p_idx->ei_block));
		return -EIO;
	}

	if (unlikely(le16_to_cpu(curp->p_hdr->eh_entries) >=
				le16_to_cpu(curp->p_hdr->eh_max))) {
		MLFS_ERROR_INODE(inode, "eh_entries %d >= eh_max %d!",
				le16_to_cpu(curp->p_hdr->eh_entries),
				le16_to_cpu(curp->p_hdr->eh_max));
		return -EIO;
	}

	if (logical > le32_to_cpu(curp->p_idx->ei_block)) {
		/* insert after */
		mlfs_lsm_debug("insert new index %d after: %lx\n", logical, ptr);
		ix = curp->p_idx + 1;
	} else {
		/* insert before */
		mlfs_lsm_debug("insert new index %d before: %lx\n", logical, ptr);
		ix = curp->p_idx;
	}

	len = EXT_LAST_INDEX(curp->p_hdr) - ix + 1;
	BUG_ON(len < 0);
	if (len > 0) {
		mlfs_lsm_debug(
				"insert new index %d: "
				"move %d indices from 0x%p to 0x%p\n",
				logical, len, ix, ix + 1);
		memmove(ix + 1, ix, len * sizeof(struct mlfs_extent_idx));
	}

	if (unlikely(ix > EXT_MAX_INDEX(curp->p_hdr))) {
		MLFS_ERROR_INODE(inode, "%s\n", "ix > EXT_MAX_INDEX!");
		return -EIO;
	}

	ix->ei_block = cpu_to_le32(logical);
	mlfs_idx_store_pblock(ix, ptr);
	le16_add_cpu(&curp->p_hdr->eh_entries, 1);

	if (unlikely(ix > EXT_LAST_INDEX(curp->p_hdr))) {
		MLFS_ERROR_INODE(inode, "%s\n", "ix > EXT_LAST_INDEX!");
		return -EIO;
	}

	err = mlfs_ext_dirty(handle, inode, curp);
	// mlfs_std_error(inode->i_sb, err);

	return err;
}

/*
 * mlfs_ext_split:
 * inserts new subtree into the path, using free index entry
 * at depth @at:
 * - allocates all needed blocks (new leaf and all intermediate index blocks)
 * - makes decision where to split
 * - moves remaining extents and index entries (right to the split point)
 *   into the newly allocated blocks
 * - initializes subtree
 */
static int mlfs_ext_split(handle_t *handle, struct inode *inode,
		unsigned int flags, struct mlfs_ext_path *path,
		struct mlfs_extent *newext, int at) 
{
	struct buffer_head *bh = NULL;
	int depth = ext_depth(handle, inode);
	struct mlfs_extent_header *neh;
	struct mlfs_extent_idx *fidx;
	int i = at, k, m, a, ret;
	mlfs_fsblk_t newblock, oldblock;
	__le32 border;
	mlfs_fsblk_t *ablocks = NULL; /* array of allocated blocks */
	int err = 0;

	/* make decision: where to split? */
	/* FIXME: now decision is simplest: at current extent */

	/* if current leaf will be split, then we should use
	 * border from split point */
	if (unlikely(path[depth].p_ext > EXT_MAX_EXTENT(path[depth].p_hdr))) {
		MLFS_ERROR_INODE(inode, "%s\n", "p_ext > EXT_MAX_EXTENT!");
		return -EIO;
	}
	if (path[depth].p_ext != EXT_MAX_EXTENT(path[depth].p_hdr)) {
		border = path[depth].p_ext[1].ee_block;
		mlfs_lsm_debug(
				"leaf will be split."
				" next leaf starts at %d\n",
				le32_to_cpu(border));
	} else {
		border = newext->ee_block;
		mlfs_lsm_debug(
				"leaf will be added."
				" next leaf starts at %d\n",
				le32_to_cpu(border));
	}

	/*
	 * If error occurs, then we break processing
	 * and mark filesystem read-only. index won't
	 * be inserted and tree will be in consistent
	 * state. Next mount will repair buffers too.
	 */

	/*
	 * Get array to track all allocated blocks.
	 * We need this to handle errors and free blocks
	 * upon them.
	 */
	ablocks = (mlfs_fsblk_t *)mlfs_zalloc(sizeof(mlfs_fsblk_t) * depth);
	if (!ablocks) 
		return -ENOMEM;

	/* allocate all needed blocks */
	mlfs_lsm_debug("allocate %d blocks for indexes/leaf\n", depth - at);
	for (a = 0; a < depth - at; a++) {
		newblock = mlfs_ext_new_meta_block(handle, inode, path, newext,
				&err, flags);
		if (newblock == 0) 
			goto cleanup;
		ablocks[a] = newblock;
	}

	/* initialize new leaf */
	newblock = ablocks[--a];
	if (unlikely(newblock == 0)) {
		MLFS_ERROR_INODE(inode, "%s\n", "newblock == 0!");
		err = -EIO;
		goto cleanup;
	}

	bh = fs_get_bh(handle->dev, newblock, &ret);

	if (unlikely(!bh)) {
		err = -ENOMEM;
		goto cleanup;
	}

	//TODO: call sync dirty buffer
	//bh = mlfs_write(inode->i_sb, newblock);
	
	/*
	err = mlfs_journal_get_create_access(handle, bh);
	if (err) 
		goto cleanup;
	*/

	neh = ext_block_hdr(bh);
	neh->eh_entries = 0;
	neh->eh_max = cpu_to_le16(mlfs_ext_space_block(inode, 0));
	neh->eh_magic = cpu_to_le16(MLFS_EXT_MAGIC);
	neh->eh_depth = 0;

	/* move remainder of path[depth] to the new leaf */
	if (unlikely(path[depth].p_hdr->eh_entries != path[depth].p_hdr->eh_max)) {
		MLFS_ERROR_INODE(inode, "eh_entries %d != eh_max %d!",
				path[depth].p_hdr->eh_entries,
				path[depth].p_hdr->eh_max);
		err = -EIO;
		goto cleanup;
	}

	/* start copy from next extent */
	m = EXT_MAX_EXTENT(path[depth].p_hdr) - path[depth].p_ext++;
	//mlfs_ext_show_move(handle, inode, path, newblock, depth);
	if (m) {
		struct mlfs_extent *ex;
		ex = EXT_FIRST_EXTENT(neh);
		memmove(ex, path[depth].p_ext, sizeof(struct mlfs_extent) * m);
		le16_add_cpu(&neh->eh_entries, m);
	}

	mlfs_extent_block_csum_set(inode, neh);
	set_buffer_uptodate(bh);

	err = mlfs_handle_dirty_metadata(handle, inode, bh);
	if (err) 
		goto cleanup;

	fs_brelse(bh);
	bh = NULL;

	/* correct old leaf */
	if (m) {
		le16_add_cpu(&path[depth].p_hdr->eh_entries, -m);
		err = mlfs_ext_dirty(handle, inode, path + depth);
		if (err) 
			goto cleanup;
	}

	/* create intermediate indexes */
	k = depth - at - 1;
	if (unlikely(k < 0)) {
		MLFS_ERROR_INODE(inode, "k %d < 0!", k);
		err = -EIO;
		goto cleanup;
	}

	if (k) 
		mlfs_lsm_debug("create %d intermediate indices\n", k);

	/* insert new index into current index block */
	/* current depth stored in i var */
	i = depth - 1;

	while (k--) {
		oldblock = newblock;
		newblock = ablocks[--a];
		//bh = extents_bwrite(inode->i_sb, newblock);

		bh = fs_get_bh(handle->dev, newblock, &ret);

		if (unlikely(!bh)) {
			err = -ENOMEM;
			goto cleanup;
		}

		neh = ext_block_hdr(bh);
		neh->eh_entries = cpu_to_le16(1);
		neh->eh_magic = cpu_to_le16(MLFS_EXT_MAGIC);
		neh->eh_max = cpu_to_le16(mlfs_ext_space_block_idx(inode, 0));
		neh->eh_depth = cpu_to_le16(depth - i);
		fidx = EXT_FIRST_INDEX(neh);
		fidx->ei_block = border;
		mlfs_idx_store_pblock(fidx, oldblock);

		mlfs_lsm_debug("int.index at %d (block %lx): %u -> %lx\n", i,
				newblock, le32_to_cpu(border), oldblock);

		/* move remainder of path[i] to the new index block */
		if (unlikely(EXT_MAX_INDEX(path[i].p_hdr) !=
					EXT_LAST_INDEX(path[i].p_hdr))) {
			MLFS_ERROR_INODE(inode,
					"EXT_MAX_INDEX != EXT_LAST_INDEX ee_block %d!",
					le32_to_cpu(path[i].p_ext->ee_block));
			err = -EIO;
			goto cleanup;
		}
		/* start copy indexes */
		m = EXT_MAX_INDEX(path[i].p_hdr) - path[i].p_idx++;
		mlfs_lsm_debug("cur 0x%p, last 0x%p\n", path[i].p_idx,
				EXT_MAX_INDEX(path[i].p_hdr));

		//mlfs_ext_show_move(handle, inode, path, newblock, i);
		if (m) {
			memmove(++fidx, path[i].p_idx, sizeof(struct mlfs_extent_idx) * m);
			le16_add_cpu(&neh->eh_entries, m);
		}

		mlfs_extent_block_csum_set(inode, neh);
		set_buffer_uptodate(bh);

		err = mlfs_handle_dirty_metadata(handle, inode, bh);
		if (err) 
			goto cleanup;
		fs_brelse(bh);
		bh = NULL;

		/* correct old index */
		if (m) {
			le16_add_cpu(&path[i].p_hdr->eh_entries, -m);
			err = mlfs_ext_dirty(handle, inode, path + i);
			if (err) 
				goto cleanup;
		}

		i--;
	}

	/* insert new index */
	err = mlfs_ext_insert_index(handle, inode, path + at,
			le32_to_cpu(border), newblock);

cleanup:
	if (bh) 
		fs_brelse(bh);

	if (err) {
		/* free all allocated blocks in error case */
		for (i = 0; i < depth; i++) {
			if (!ablocks[i]) continue;
			mlfs_free_blocks(handle, inode, NULL, ablocks[i], 1,
					MLFS_FREE_BLOCKS_METADATA);
		}
	}

	mlfs_free(ablocks);

	return err;
}

/*
 * mlfs_ext_grow_indepth:
 * implements tree growing procedure:
 * - allocates new block
 * - moves top-level data (index block or leaf) into the new block
 * - initializes new top-level, creating index that points to the
 *   just created block
 */
static int mlfs_ext_grow_indepth(handle_t *handle,
		struct inode *inode, unsigned int flags) 
{
	struct mlfs_extent_header *neh;
	struct buffer_head *bh;
	mlfs_fsblk_t newblock, goal = 0;
	int err = 0, ret;
	mlfs_lblk_t count = 1;

	/* Try to prepend new index to old one */
	if (ext_depth(handle, inode))
		goal = mlfs_idx_pblock(EXT_FIRST_INDEX(ext_inode_hdr(handle, inode)));
	goal = mlfs_inode_to_goal_block(inode);

	newblock = mlfs_new_meta_blocks(handle, inode, goal, flags, &count, &err);
	if (newblock == 0) 
		return err;

	bh = fs_get_bh(handle->dev, newblock, &ret);
	//bh = extents_bwrite(inode->i_sb, newblock);
	if (!bh) 
		return -ENOMEM;
	lock_buffer(bh);

	/*
	err = mlfs_journal_get_create_access(handle, bh);
	if (err) 
		goto out;
	*/

	/* move top-level index/leaf into new block */
	//memmove(bh->b_data, inode->i_data, sizeof(inode->l1.i_data));
	memmove(bh->b_data, ext_inode_hdr(handle, inode), sizeof(inode->l1.i_data));

	/* set size of new block */
	neh = ext_block_hdr(bh);
	/* old root could have indexes or leaves
	 * so calculate e_max right way */
	if (ext_depth(handle, inode))
		neh->eh_max = cpu_to_le16(mlfs_ext_space_block_idx(inode, 0));
	else
		neh->eh_max = cpu_to_le16(mlfs_ext_space_block(inode, 0));
	neh->eh_magic = cpu_to_le16(MLFS_EXT_MAGIC);
	mlfs_extent_block_csum_set(inode, neh);
	set_buffer_uptodate(bh);
	unlock_buffer(bh);

	err = mlfs_handle_dirty_metadata(handle, inode, bh);
	if (err) 
		goto out;

	/* Update top-level index: num,max,pointer */
	neh = ext_inode_hdr(handle, inode);
	neh->eh_entries = cpu_to_le16(1);
	mlfs_idx_store_pblock(EXT_FIRST_INDEX(neh), newblock);
	if (neh->eh_depth == 0) {
		/* Root extent block becomes index block */
		neh->eh_max = cpu_to_le16(mlfs_ext_space_root_idx(inode, 0));
		EXT_FIRST_INDEX(neh)->ei_block = 
			EXT_FIRST_EXTENT(neh)->ee_block;
	}
	mlfs_lsm_debug("new root: num %d(%d), lblock %d, ptr %lx\n",
			(neh->eh_entries), (neh->eh_max),
			(EXT_FIRST_INDEX(neh)->ei_block),
			mlfs_idx_pblock(EXT_FIRST_INDEX(neh)));

	le16_add_cpu(&neh->eh_depth, 1);
	mlfs_mark_inode_dirty(handle->libfs, inode);
out:
	fs_brelse(bh);

	return err;
}

/*
 * mlfs_ext_create_new_leaf:
 * finds empty index and adds new leaf.
 * if no free index is found, then it requests in-depth growing.
 */
static int mlfs_ext_create_new_leaf(handle_t *handle,
		struct inode *inode, unsigned int mb_flags, unsigned int gb_flags,
		struct mlfs_ext_path **ppath, struct mlfs_extent *newext) 
{ 
	struct mlfs_ext_path *path = *ppath;
	struct mlfs_ext_path *curp;
	int depth, i, err = 0;

repeat:
	i = depth = ext_depth(handle, inode);

	/* walk up to the tree and look for free index entry */
	curp = path + depth;
	while (i > 0 && !EXT_HAS_FREE_INDEX(curp)) {
		i--;
		curp--;
	}

	/* we use already allocated block for index block,
	 * so subsequent data blocks should be contiguous */
	if (EXT_HAS_FREE_INDEX(curp)) {
		/* if we found index with free entry, then use that
		 * entry: create all needed subtree and add new leaf */
		err = mlfs_ext_split(handle, inode, mb_flags, path, newext, i);
		if (err) 
			goto out;

		/* refill path */
		path = mlfs_find_extent(handle, inode, mlfs_ext_lblock(newext), 
				ppath, gb_flags);

		if (IS_ERR(path)) 
			err = PTR_ERR(path);
	} else {
		/* tree is full, time to grow in depth */
		err = mlfs_ext_grow_indepth(handle, inode, mb_flags);
		if (err) 
			goto out;

		/* refill path */
		path = mlfs_find_extent(handle, inode, mlfs_ext_lblock(newext), 
				ppath, gb_flags);
		if (IS_ERR(path)) {
			err = PTR_ERR(path);
			goto out;
		}

		/*
		 * only first (depth 0 -> 1) produces free space;
		 * in all other cases we have to split the grown tree
		 */
		depth = ext_depth(handle, inode);
		if (path[depth].p_hdr->eh_entries == path[depth].p_hdr->eh_max) {
			/* now we need to split */
			goto repeat;
		}
	}

out:
	return err;
}

/*
 * search the closest allocated block to the left for *logical
 * and returns it at @logical + it's physical address at @phys
 * if *logical is the smallest allocated block, the function
 * returns 0 at @phys
 * return value contains 0 (success) or error code
 */
static int mlfs_ext_search_left(struct inode *inode, struct mlfs_ext_path *path,
		mlfs_lblk_t *logical, mlfs_fsblk_t *phys) 
{
	struct mlfs_extent_idx *ix;
	struct mlfs_extent *ex;
	int depth, ee_len;

	if (unlikely(path == NULL)) {
		MLFS_ERROR_INODE(inode, "path == NULL *logical %d!", *logical);
		return -EIO;
	}
	depth = path->p_depth;
	*phys = 0;

	if (depth == 0 && path->p_ext == NULL) 
		return 0;

	/* usually extent in the path covers blocks smaller
	 * then *logical, but it can be that extent is the
	 * first one in the file */

	ex = path[depth].p_ext;
	ee_len = mlfs_ext_get_actual_len(ex);
	if (*logical < le32_to_cpu(ex->ee_block)) {
		if (unlikely(EXT_FIRST_EXTENT(path[depth].p_hdr) != ex)) {
			MLFS_ERROR_INODE(inode,
					"EXT_FIRST_EXTENT != ex *logical %d ee_block %d!",
					*logical, le32_to_cpu(ex->ee_block));
			return -EIO;
		}
		while (--depth >= 0) {
			ix = path[depth].p_idx;
			if (unlikely(ix != EXT_FIRST_INDEX(path[depth].p_hdr))) {
				MLFS_ERROR_INODE(
						inode, "ix (%d) != EXT_FIRST_INDEX (%d) (depth %d)!",
						ix != NULL ? le32_to_cpu(ix->ei_block) : 0,
						EXT_FIRST_INDEX(path[depth].p_hdr) != NULL
						? le32_to_cpu(
							EXT_FIRST_INDEX(path[depth].p_hdr)->ei_block)
						: 0,
						depth);
				return -EIO;
			}
		}
		return 0;
	}

	if (unlikely(*logical < (le32_to_cpu(ex->ee_block) + ee_len))) {
		MLFS_ERROR_INODE(inode, "logical %d < ee_block %d + ee_len %d!",
				*logical, le32_to_cpu(ex->ee_block), ee_len);
		return -EIO;
	}

	*logical = le32_to_cpu(ex->ee_block) + ee_len - 1;
	*phys = mlfs_ext_pblock(ex) + ee_len - 1;
	return 0;
}

/*
 * search the closest allocated block to the right for *logical
 * and returns it at @logical + it's physical address at @phys
 * if *logical is the largest allocated block, the function
 * returns 0 at @phys
 * return value contains 0 (success) or error code
 */
static int mlfs_ext_search_right(handle_t *handle, struct inode *inode, 
		struct mlfs_ext_path *path, mlfs_lblk_t *logical, mlfs_fsblk_t *phys, 
		struct mlfs_extent **ret_ex) 
{
	struct buffer_head *bh = NULL;
	struct mlfs_extent_header *eh;
	struct mlfs_extent_idx *ix;
	struct mlfs_extent *ex;
	mlfs_fsblk_t block;
	int depth; /* Note, NOT eh_depth; depth from top of tree */
	int ee_len;

	if (path == NULL) {
		MLFS_ERROR_INODE(inode, "path == NULL *logical %d!", *logical);
		return -EIO;
	}
	depth = path->p_depth;
	*phys = 0;

	if (depth == 0 && path->p_ext == NULL) 
		return 0;

	/* usually extent in the path covers blocks smaller
	 * then *logical, but it can be that extent is the
	 * first one in the file */

	ex = path[depth].p_ext;
	ee_len = mlfs_ext_get_actual_len(ex);
	/*if (*logical < le32_to_cpu(ex->ee_block)) {*/
	if (*logical < (ex->ee_block)) {
		if (unlikely(EXT_FIRST_EXTENT(path[depth].p_hdr) != ex)) {
			MLFS_ERROR_INODE(inode, "first_extent(path[%d].p_hdr) != ex",
					depth);
			return -EIO;
		}
		while (--depth >= 0) {
			ix = path[depth].p_idx;
			if (unlikely(ix != EXT_FIRST_INDEX(path[depth].p_hdr))) {
				MLFS_ERROR_INODE(inode, "ix != EXT_FIRST_INDEX *logical %d!",
						*logical);
				return -EIO;
			}
		}
		goto found_extent;
	}

	/*if (unlikely(*logical < (le32_to_cpu(ex->ee_block) + ee_len))) {*/
	if (unlikely(*logical < ((ex->ee_block) + ee_len))) {
		MLFS_ERROR_INODE(inode, "logical %d < ee_block %d + ee_len %d!",
				/**logical, le32_to_cpu(ex->ee_block), ee_len);*/
			*logical, (ex->ee_block), ee_len);
		return -EIO;
	}

	if (ex != EXT_LAST_EXTENT(path[depth].p_hdr)) {
		/* next allocated block in this leaf */
		ex++;
		goto found_extent;
	}

	/* go up and search for index to the right */
	while (--depth >= 0) {
		ix = path[depth].p_idx;
		if (ix != EXT_LAST_INDEX(path[depth].p_hdr)) 
			goto got_index;
	}

	/* we've gone up to the root and found no index to the right */
	return 0;

got_index:
	/* we've found index to the right, let's
	 * follow it and find the closest allocated
	 * block to the right */
	ix++;
	block = mlfs_idx_pblock(ix);
	while (++depth < path->p_depth) {
		/* subtract from p_depth to get proper eh_depth */
		bh = read_extent_tree_block(handle, inode, block, path->p_depth - depth, 0);
		if (IS_ERR(bh)) 
			return PTR_ERR(bh);

		eh = ext_block_hdr(bh);
		ix = EXT_FIRST_INDEX(eh);
		block = mlfs_idx_pblock(ix);
		fs_brelse(bh);
	}

	bh = read_extent_tree_block(handle, inode, block, path->p_depth - depth, 0);
	if (IS_ERR(bh)) 
		return PTR_ERR(bh);

	eh = ext_block_hdr(bh);
	ex = EXT_FIRST_EXTENT(eh);
found_extent:
	/**logical = le32_to_cpu(ex->ee_block);*/
	*logical = (ex->ee_block);
	*phys = mlfs_ext_pblock(ex);
	*ret_ex = ex;

	if (bh) 
		fs_brelse(bh);

	return 0;
}

/*
 * mlfs_ext_next_allocated_block:
 * returns allocated block in subsequent extent or EXT_MAX_BLOCKS.
 * NOTE: it considers block number from index entry as
 * allocated block. Thus, index entries have to be consistent
 * with leaves.
 */
mlfs_lblk_t mlfs_ext_next_allocated_block(struct mlfs_ext_path *path) 
{
	int depth;

	depth = path->p_depth;

	if (depth == 0 && path->p_ext == NULL) 
		return EXT_MAX_BLOCKS;

	while (depth >= 0) {
		if (depth == path->p_depth) {
			/* extent (leaf) */
			if (path[depth].p_ext &&
					path[depth].p_ext != EXT_LAST_EXTENT(path[depth].p_hdr))

				return mlfs_ext_lblock(&path[depth].p_ext[1]);
		} else {
			/* index */
			if (path[depth].p_idx != EXT_LAST_INDEX(path[depth].p_hdr))

				return mlfs_idx_lblock(&path[depth].p_idx[1]);
		}
		depth--;
	}

	return EXT_MAX_BLOCKS;
}

/*
 * mlfs_ext_next_leaf_block:
 * returns first allocated block from next leaf or EXT_MAX_BLOCKS
 */
static mlfs_lblk_t mlfs_ext_next_leaf_block(struct mlfs_ext_path *path) 
{
	int depth;

	BUG_ON(path == NULL);
	depth = path->p_depth;

	/* zero-tree has no leaf blocks at all */
	if (depth == 0) 
		return EXT_MAX_BLOCKS;

	/* go to upper internal node (index block) */
	depth--;

	while (depth >= 0) {
		if (path[depth].p_idx != EXT_LAST_INDEX(path[depth].p_hdr))
			/* return lblock of a next index node in a index block */
			return mlfs_idx_lblock(&path[depth].p_idx[1]);

		depth--;
	}

	return EXT_MAX_BLOCKS;
}

/*
 * mlfs_ext_correct_indexes:
 * if leaf gets modified and modified extent is first in the leaf,
 * then we have to correct all indexes above.
 * TODO: do we need to correct tree in all cases?
 */
static int mlfs_ext_correct_indexes(handle_t *handle,
		struct inode *inode, struct mlfs_ext_path *path) 
{
	struct mlfs_extent_header *eh;
	int depth = ext_depth(handle, inode);
	struct mlfs_extent *ex;
	__le32 border;
	int k, err = 0;

	eh = path[depth].p_hdr;
	ex = path[depth].p_ext;

	if (unlikely(ex == NULL || eh == NULL)) {
		MLFS_ERROR_INODE(inode, "ex %p == NULL or eh %p == NULL", ex, eh);
		return -EIO;
	}

	if (depth == 0) {
		/* there is no tree at all */
		return 0;
	}

	if (ex != EXT_FIRST_EXTENT(eh)) {
		/* we correct tree if first leaf got modified only */
		return 0;
	}

	/*
	 * TODO: we need correction if border is smaller than current one
	 */
	k = depth - 1;
	border = path[depth].p_ext->ee_block;

	path[k].p_idx->ei_block = border;
	err = mlfs_ext_dirty(handle, inode, path + k);

	if (err) 
		return err;

	while (k--) {
		/* change all left-side indexes */
		if (path[k + 1].p_idx != EXT_FIRST_INDEX(path[k + 1].p_hdr)) 
			break;

		path[k].p_idx->ei_block = border;
		err = mlfs_ext_dirty(handle, inode, path + k);
		if (err) break;
	}

	return err;
}

int mlfs_can_extents_be_merged(struct inode *inode, struct mlfs_extent *ex1,
		struct mlfs_extent *ex2) 
{
	unsigned short ext1_ee_len, ext2_ee_len;

	/*
	 * Make sure that both extents are initialized. We don't merge
	 * unwritten extents so that we can be sure that end_io code has
	 * the extent that was written properly split out and conversion to
	 * initialized is trivial.
	 */
	if (mlfs_ext_is_unwritten(ex1) != mlfs_ext_is_unwritten(ex2)) 
		return 0;

	ext1_ee_len = mlfs_ext_get_actual_len(ex1);
	ext2_ee_len = mlfs_ext_get_actual_len(ex2);

	/* Logical block is contiguous */
	if (mlfs_ext_lblock(ex1) + ext1_ee_len != mlfs_ext_lblock(ex2))
		return 0;

	/*
	 * To allow future support for preallocated extents to be added
	 * as an RO_COMPAT feature, refuse to merge to extents if
	 * this can result in the top bit of ee_len being set.
	 */
	if (ext1_ee_len + ext2_ee_len > EXT_INIT_MAX_LEN) 
		return 0;

	if (mlfs_ext_is_unwritten(ex1) &&
			(ext1_ee_len + ext2_ee_len > EXT_UNWRITTEN_MAX_LEN))
		return 0;

#ifdef AGGRESSIVE_TEST
	if (ext1_ee_len >= 4) return 0;
#endif

	/* Physical block is contiguous */
	if (mlfs_ext_pblock(ex1) + ext1_ee_len == mlfs_ext_pblock(ex2)) 
		return 1;

	return 0;
}

/*
 * This function tries to merge the "ex" extent to the next extent in the tree.
 * It always tries to merge towards right. If you want to merge towards
 * left, pass "ex - 1" as argument instead of "ex".
 * Returns 0 if the extents (ex and ex+1) were _not_ merged and returns
 * 1 if they got merged.
 */
static int mlfs_ext_try_to_merge_right(handle_t *handle, struct inode *inode,
		struct mlfs_ext_path *path, struct mlfs_extent *ex) 
{
	struct mlfs_extent_header *eh;
	unsigned int depth, len;
	int merge_done = 0, unwritten;

	depth = ext_depth(handle, inode);
	assert(path[depth].p_hdr != NULL);
	eh = path[depth].p_hdr;

	while (ex < EXT_LAST_EXTENT(eh)) {
		if (!mlfs_can_extents_be_merged(inode, ex, ex + 1)) 
			break;

		/* merge with next extent! */
		unwritten = mlfs_ext_is_unwritten(ex);
		ex->ee_len = cpu_to_le16(mlfs_ext_get_actual_len(ex) +
				mlfs_ext_get_actual_len(ex + 1));

		if (unwritten) 
			mlfs_ext_mark_unwritten(ex);

		if (ex + 1 < EXT_LAST_EXTENT(eh)) {
			len = (EXT_LAST_EXTENT(eh) - ex - 1) * sizeof(struct mlfs_extent);
			memmove(ex + 1, ex + 2, len);
		}

		le16_add_cpu(&eh->eh_entries, -1);
		merge_done = 1;

		if (!eh->eh_entries) 
			MLFS_ERROR_INODE(inode, "%s\n", "eh->eh_entries = 0!");
	}

	return merge_done;
}

/*
 * This function does a very simple check to see if we can collapse
 * an extent tree with a single extent tree leaf block into the inode.
 */
static void mlfs_ext_try_to_merge_up(handle_t *handle,
		struct inode *inode, struct mlfs_ext_path *path) 
{
	size_t s;
	unsigned max_root = mlfs_ext_space_root(inode, 0);
	mlfs_fsblk_t blk;

	if ((path[0].p_depth != 1) ||
			(le16_to_cpu(path[0].p_hdr->eh_entries) != 1) ||
			(le16_to_cpu(path[1].p_hdr->eh_entries) > max_root))
		return;

	/*
	 * We need to modify the block allocation bitmap and the block
	 * group descriptor to release the extent tree block.  If we
	 * can't get the journal credits, give up.
	 */
	/*
	if (mlfs_journal_extend(handle, 2)) 
		return;
	*/
	/*
	 * Copy the extent data up to the inode
	 */
	blk = mlfs_idx_pblock(path[0].p_idx);
	s = le16_to_cpu(path[1].p_hdr->eh_entries) * sizeof(struct mlfs_extent_idx);
	s += sizeof(struct mlfs_extent_header);

	path[1].p_maxdepth = path[0].p_maxdepth;
	memcpy(path[0].p_hdr, path[1].p_hdr, s);
	path[0].p_depth = 0;
	path[0].p_ext = EXT_FIRST_EXTENT(path[0].p_hdr) +
		(path[1].p_ext - EXT_FIRST_EXTENT(path[1].p_hdr));
	path[0].p_hdr->eh_max = cpu_to_le16(max_root);

	fs_brelse(path[1].p_bh);
	mlfs_free_blocks(handle, inode, NULL, blk, 1,
			MLFS_FREE_BLOCKS_METADATA | MLFS_FREE_BLOCKS_FORGET);
}

/*
 * This function tries to merge the @ex extent to neighbours in the tree.
 * return 1 if merge left else 0.
 */
static void mlfs_ext_try_to_merge(handle_t *handle,
		struct inode *inode, struct mlfs_ext_path *path, struct mlfs_extent *ex) 
{
	struct mlfs_extent_header *eh;
	unsigned int depth;
	int merge_done = 0;

	depth = ext_depth(handle, inode);
	BUG_ON(path[depth].p_hdr == NULL);
	eh = path[depth].p_hdr;

	if (ex > EXT_FIRST_EXTENT(eh))
		merge_done = mlfs_ext_try_to_merge_right(handle, inode, path, ex - 1);

	if (!merge_done) 
		(void)mlfs_ext_try_to_merge_right(handle, inode, path, ex);

	mlfs_ext_try_to_merge_up(handle, inode, path);
}

/*
 * mlfs_ext_insert_extent:
 * tries to merge requsted extent into the existing extent or
 * inserts requested extent as new one into the tree,
 * creating new leaf in the no-space case.
 */
int mlfs_ext_insert_extent(handle_t *handle, struct inode *inode,
		struct mlfs_ext_path **ppath, struct mlfs_extent *newext, int gb_flags) 
{
	struct mlfs_ext_path *path = *ppath;
	struct mlfs_extent_header *eh;
	struct mlfs_extent *ex, *last_ex;
	struct mlfs_extent *nearex; /* nearest extent */
	struct mlfs_ext_path *npath = NULL;
	int depth, len, err;
	mlfs_lblk_t next_lb;
	int mb_flags = 0, unwritten;

	if (unlikely(mlfs_ext_get_actual_len(newext) == 0)) {
		MLFS_ERROR_INODE(inode, "%s\n", 
				"mlfs_ext_get_actual_len(newext) == 0");
		return -EIO;
	}

	depth = ext_depth(handle, inode);
	ex = path[depth].p_ext;
	eh = path[depth].p_hdr;

	if (unlikely(path[depth].p_hdr == NULL)) {
		MLFS_ERROR_INODE(inode, "path[%d].p_hdr == NULL", depth);
		return -EIO;
	}

	/* try to insert block into found extent and return */
	if (ex && !(gb_flags & MLFS_GET_BLOCKS_PRE_IO)) {
		/*
		 * Try to see whether we should rather test the extent on
		 * right from ex, or from the left of ex. This is because
		 * mlfs_find_extent() can return either extent on the
		 * left, or on the right from the searched position. This
		 * will make merging more effective.
		 */
		if (ex < EXT_LAST_EXTENT(eh) &&
				(mlfs_ext_lblock(ex) + mlfs_ext_get_actual_len(ex) <
				 mlfs_ext_lblock(newext))) {
			ex += 1;
			goto prepend;
		} else if ((ex > EXT_FIRST_EXTENT(eh)) &&
				(mlfs_ext_lblock(newext) + mlfs_ext_get_actual_len(newext) <
				 mlfs_ext_lblock(ex)))
			ex -= 1;

		/* Try to append newex to the ex */
		if (mlfs_can_extents_be_merged(inode, ex, newext)) {
			mlfs_lsm_debug(
					"append [%d]%d block to %u:[%d] ~ %u (from %lx)\n",
					mlfs_ext_is_unwritten(newext), 
					mlfs_ext_get_actual_len(newext),
					le32_to_cpu(ex->ee_block), 
					mlfs_ext_is_unwritten(ex),
					mlfs_ext_get_actual_len(ex), 
					mlfs_ext_pblock(ex));

			unwritten = mlfs_ext_is_unwritten(ex);
			ex->ee_len = cpu_to_le16(mlfs_ext_get_actual_len(ex) +
					mlfs_ext_get_actual_len(newext));

			if (unwritten) 
				mlfs_ext_mark_unwritten(ex);
			
			eh = path[depth].p_hdr;
			nearex = ex;

			goto merge;
		}

prepend:
		/* Try to prepend newex to the ex */
		if (mlfs_can_extents_be_merged(inode, newext, ex)) {
			mlfs_lsm_debug(
					"prepend %u[%d]%d block to %u:[%d] ~ %u from %lx)\n",
					le32_to_cpu(newext->ee_block), 
					mlfs_ext_is_unwritten(newext),
					mlfs_ext_get_actual_len(newext), 
					le32_to_cpu(ex->ee_block),
					mlfs_ext_is_unwritten(ex), 
					mlfs_ext_get_actual_len(ex),
					mlfs_ext_pblock(ex));

			unwritten = mlfs_ext_is_unwritten(ex);
			ex->ee_block = newext->ee_block;

			mlfs_ext_store_pblock(ex, mlfs_ext_pblock(newext));
			ex->ee_len = cpu_to_le16(mlfs_ext_get_actual_len(ex) +
					mlfs_ext_get_actual_len(newext));

			if (unwritten) 
				mlfs_ext_mark_unwritten(ex);

			eh = path[depth].p_hdr;
			nearex = ex;

			goto merge;
		}
	}

	depth = ext_depth(handle, inode);
	eh = path[depth].p_hdr;

	if (le16_to_cpu(eh->eh_entries) < le16_to_cpu(eh->eh_max)) 
		goto has_space;

	/* probably next leaf has space for us? */
	last_ex = EXT_LAST_EXTENT(eh);
	next_lb = EXT_MAX_BLOCKS;

	if (mlfs_ext_lblock(newext) > mlfs_ext_lblock(last_ex))
		next_lb = mlfs_ext_next_leaf_block(path);

	/* There is a possibility to add new extent in a sibling 
	 * extent block */
	if (next_lb != EXT_MAX_BLOCKS) {
		mlfs_lsm_debug("next leaf block - %u\n", next_lb);
		BUG_ON(npath != NULL);

		npath = mlfs_find_extent(handle, inode, next_lb, NULL, 0);
		if (IS_ERR(npath)) 
			return PTR_ERR(npath);

		BUG_ON(npath->p_depth != path->p_depth);

		eh = npath[depth].p_hdr;

		/* checks whether next a extent block is full or not */
		if (le16_to_cpu(eh->eh_entries) < le16_to_cpu(eh->eh_max)) {
			mlfs_lsm_debug("next leaf isn't full(%d)\n",
					le16_to_cpu(eh->eh_entries));

			/* update path to new one since the newext will use 
			 * the next extent block */
			path = npath;
			goto has_space;
		}

		mlfs_lsm_debug("next leaf has no free space(%d,%d)\n",
				le16_to_cpu(eh->eh_entries), le16_to_cpu(eh->eh_max));
	}

	/*
	 * There is no free space in the found leaf.
	 * We're gonna add a new leaf in the tree.
	 */
	if (gb_flags & MLFS_GET_BLOCKS_METADATA_NOFAIL)
		mb_flags |= MLFS_MB_USE_RESERVED;

	/* create new leaf extent block and update path */
	err = mlfs_ext_create_new_leaf(handle, inode, mb_flags, gb_flags,
			ppath, newext);

	if (err) 
		goto cleanup;

	depth = ext_depth(handle, inode);
	eh = path[depth].p_hdr;

has_space:
	/* At this point, it is guaranteed that there exists a room
	 * for adding the new extent */

	/* nearex is a pointer in a extent block and will be updated 
	 * to the proper location */
	nearex = path[depth].p_ext;

	if (!nearex) {
		/* there is no extent in this leaf (e.g., extent block is newly created),
		 * create first one */
		mlfs_lsm_debug("first extent in the leaf: %u:%lx:[%d]%d\n",
				le32_to_cpu(newext->ee_block), mlfs_ext_pblock(newext),
				mlfs_ext_is_unwritten(newext),
				mlfs_ext_get_actual_len(newext));
		nearex = EXT_FIRST_EXTENT(eh);
	} else {
		if (mlfs_ext_lblock(newext) > mlfs_ext_lblock(nearex)) {
			mlfs_lsm_debug(
					"insert %u:%lx:[%d]%d before: "
					"nearest %p\n",
					mlfs_ext_lblock(newext), mlfs_ext_pblock(newext),
					mlfs_ext_is_unwritten(newext), mlfs_ext_get_actual_len(newext),
					nearex);
			nearex++;
		} else {
			BUG_ON(mlfs_ext_lblock(newext) == mlfs_ext_lblock(nearex));
			mlfs_lsm_debug(
					"insert %u:%lx:[%d]%d after: "
					"nearest %p\n",
					mlfs_ext_lblock(newext), mlfs_ext_pblock(newext),
					mlfs_ext_is_unwritten(newext), mlfs_ext_get_actual_len(newext),
					nearex);
		}
		len = EXT_LAST_EXTENT(eh) - nearex + 1;

		if (len > 0) {
			mlfs_lsm_debug(
					"insert %u:%lx:[%d]%d: "
					"move %d extents from 0x%p to 0x%p\n",
					mlfs_ext_lblock(newext), mlfs_ext_pblock(newext),
					mlfs_ext_is_unwritten(newext), mlfs_ext_get_actual_len(newext),
					len, nearex, nearex + 1);
			memmove(nearex + 1, nearex, len * sizeof(struct mlfs_extent));
		}
	}

	le16_add_cpu(&eh->eh_entries, 1);
	path[depth].p_ext = nearex;

	/* nearex points to the location in a extent block.
	 * update nearex with newext information */
	//nearex->ee_block = mlfs_ext_lblock(newext);
	mlfs_ext_set_lblock(nearex, mlfs_ext_lblock(newext));
	mlfs_ext_store_pblock(nearex, mlfs_ext_pblock(newext));
	nearex->ee_len = newext->ee_len;

merge:
	/* try to merge extents */
	if (!(gb_flags & MLFS_GET_BLOCKS_PRE_IO))
		mlfs_ext_try_to_merge(handle, inode, path, nearex);

	/* time to correct all indexes above */
	err = mlfs_ext_correct_indexes(handle, inode, path);
	if (err) 
		goto cleanup;

	err = mlfs_ext_dirty(handle, inode, path + path->p_depth);

cleanup:
	if (npath) {
		mlfs_ext_drop_refs(npath);
		mlfs_free(npath);
	}

	return err;
}

static inline int get_default_free_blocks_flags(struct inode *inode) 
{
	return 0;
}

/* FIXME!! we need to try to merge to left or right after zero-out  */
static int mlfs_ext_zeroout(struct inode *inode, struct mlfs_extent *ex) 
{
	mlfs_fsblk_t ee_pblock;
	unsigned int ee_len;
	int ret;

	ee_len = mlfs_ext_get_actual_len(ex);
	ee_pblock = mlfs_ext_pblock(ex);

	ret = 0;

	return ret;
}

static int mlfs_remove_blocks(handle_t *handle, struct inode *inode,
		struct mlfs_extent *ex, unsigned long from, unsigned long to) 
{
	struct buffer_head *bh;
	int i;

	if (from >= le32_to_cpu(ex->ee_block) &&
			to == le32_to_cpu(ex->ee_block) + mlfs_ext_get_actual_len(ex) - 1) {
		/* tail removal */
		unsigned long num, start;
		num = le32_to_cpu(ex->ee_block) + mlfs_ext_get_actual_len(ex) - from;
		start = mlfs_ext_pblock(ex) + mlfs_ext_get_actual_len(ex) - num;
		mlfs_free_blocks(handle, inode, NULL, start, num, 0);
	} else if (from == le32_to_cpu(ex->ee_block) &&
			to <= le32_to_cpu(ex->ee_block) + mlfs_ext_get_actual_len(ex) - 1) {
	} else {
	}
	return 0;
}

/*
 * routine removes index from the index block
 * it's used in truncate case only. thus all requests are for
 * last index in the block only
 */
int mlfs_ext_rm_idx(handle_t *handle, struct inode *inode,
		struct mlfs_ext_path *path, int depth) 
{
	int err;
	mlfs_fsblk_t leaf;

	path--;
	path = path + depth;
	leaf = mlfs_idx_pblock(path->p_idx);

	// index block is corrupted.
	BUG_ON(path->p_hdr->eh_entries == 0);

	if (path->p_idx != EXT_LAST_INDEX(path->p_hdr)) {
		int len = EXT_LAST_INDEX(path->p_hdr) - path->p_idx;
		len *= sizeof(struct mlfs_extent_idx);
		memmove(path->p_idx, path->p_idx + 1, len);
	}

	le16_add_cpu(&path->p_hdr->eh_entries, -1);

	if ((err = mlfs_ext_dirty(handle, inode, path))) 
		return err;

	mlfs_free_blocks(handle, inode, NULL, leaf, 1, 0);

	while (--depth >= 0) {
		if (path->p_idx != EXT_FIRST_INDEX(path->p_hdr))
			break;

		// if current path pointer is root, then escape this loop.
		if (path->p_hdr == ext_inode_hdr(handle, inode))
			break;

		path--;

		mlfs_assert(path->p_idx);
		mlfs_assert((path+1)->p_idx);

		path->p_idx->ei_block = (path+1)->p_idx->ei_block;
		err = mlfs_ext_dirty(handle, inode, path);
		if (err)
			break;
	}

	return err;
}

static int mlfs_ext_rm_leaf(handle_t *handle, struct inode *inode,
		struct mlfs_ext_path *path, mlfs_lblk_t start, mlfs_lblk_t end) 
{
	int err = 0, correct_index = 0;
	int depth = ext_depth(handle, inode);
	struct mlfs_extent_header *eh;
	mlfs_lblk_t a, b, block;
	unsigned int num, unwritten = 0;
	mlfs_lblk_t ex_ee_block;
	unsigned short ex_ee_len;
	struct mlfs_extent *ex;

	mlfs_lsm_debug("truncate from %u(0x%x) to %u(0x%x)\n", 
			start << g_block_size_shift,
			start << g_block_size_shift,
			end << g_block_size_shift,
			end << g_block_size_shift);

	/* the header must be checked already in mlfs_ext_remove_space() */
	if (!path[depth].p_hdr) 
		path[depth].p_hdr = ext_block_hdr(path[depth].p_bh);

	eh = path[depth].p_hdr;
	BUG_ON(eh == NULL);

	/* Start removing extent from the end. ex is pointer for
	 * extents in the given extent block */
	ex = EXT_LAST_EXTENT(eh);

	ex_ee_block = le32_to_cpu(ex->ee_block);
	ex_ee_len = mlfs_ext_get_actual_len(ex);

	while (ex >= EXT_FIRST_EXTENT(eh) && 
			ex_ee_block + ex_ee_len > start) {

		if (mlfs_ext_is_unwritten(ex))
			unwritten = 1;
		else
			unwritten = 0;

		path[depth].p_ext = ex;

		/* a is starting logical address of current extent and
		 * b is ending logical address to remove */
		a = ex_ee_block > start ? ex_ee_block : start;
		b = ex_ee_block+ex_ee_len - 1 < end ?
			ex_ee_block+ex_ee_len - 1 : end;

		mlfs_lsm_debug("remove ext %u:[%d]%d - border %u:%u\n", 
				ex_ee_block, unwritten, ex_ee_len, a, b);

		/* If this extent is beyond the end of the hole, skip it */
		if (end < ex_ee_block) {
			ex--;
			ex_ee_block = le32_to_cpu(ex->ee_block);
			ex_ee_len = mlfs_ext_get_actual_len(ex);
			continue;
		} else if (b != ex_ee_block + ex_ee_len - 1) {
			MLFS_ERROR_INODE(inode,
					 "can not handle truncate %u:%u "
					 "on extent %u:%u",
					 start, end, ex_ee_block,
					 ex_ee_block + ex_ee_len - 1);
			err = -EFSCORRUPTED;
			goto out;
		} else if (a != ex_ee_block) {
			/* remove tail of the extent */
			num = a - ex_ee_block;
		} else {
			/* remove whole extent: excellent! */
			num = 0;
		}

		if (ex == EXT_FIRST_EXTENT(eh)) 
			correct_index = 1;

		err = mlfs_remove_blocks(handle, inode, ex, a, b);
		if (err) 
			goto out;

		/* this extent is removed entirely mark slot unused */
		if (num == 0) 
			mlfs_ext_store_pblock(ex, 0);

		ex->ee_len = cpu_to_le16(num);

		/*
		 * Do not mark unwritten if all the blocks in the
		 * extent have been removed.
		 */
		if (unwritten && num)
			mlfs_ext_mark_unwritten(ex);
		/*
		 * If the extent was completely released,
		 * we need to remove it from the leaf
		 */
		if (num == 0) {
			if (end != EXT_MAX_BLOCKS - 1) {
				/*
				 * For hole punching, we need to scoot all the
				 * extents up when an extent is removed so that
				 * we dont have blank extents in the middle
				 */
				memmove(ex, ex+1, (EXT_LAST_EXTENT(eh) - ex) *
					sizeof(struct mlfs_extent));

				/* Now get rid of the one at the end */
				memset(EXT_LAST_EXTENT(eh), 0,
					sizeof(struct mlfs_extent));
			}
			le16_add_cpu(&eh->eh_entries, -1);
		}

		err = mlfs_ext_dirty(handle, inode, path + depth);
		if (err) 
			goto out;

		mlfs_lsm_debug("new extent: %u:%u @ %lu\n", ex_ee_block, num,
				mlfs_ext_pblock(ex));

		ex--;
		ex_ee_block = le32_to_cpu(ex->ee_block);
		ex_ee_len = mlfs_ext_get_actual_len(ex);
	}

	if (correct_index && eh->eh_entries)
		err = mlfs_ext_correct_indexes(handle, inode, path);

	/* if this leaf is free, then we should
	 * remove it from index block above */
	if (err == 0 && eh->eh_entries == 0 && path[depth].p_bh != NULL)
		err = mlfs_ext_rm_idx(handle, inode, path, depth);

out:
	return err;
}

/*
 * mlfs_split_extent_at() splits an extent at given block.
 *
 * @handle: the journal handle
 * @inode: the file inode
 * @path: the path to the extent
 * @split: the logical block where the extent is splitted.
 * @split_flags: indicates if the extent could be zeroout if split fails, and
 *		 the states(init or unwritten) of new extents.
 * @flags: flags used to insert new extent to extent tree.
 *
 *
 * Splits extent [a, b] into two extents [a, @split) and [@split, b], states
 * of which are deterimined by split_flag.
 *
 * There are two cases:
 *  a> the extent are splitted into two extent.
 *  b> split is not needed, and just mark the extent.
 *
 * return 0 on success.
 */
static int mlfs_split_extent_at(handle_t *handle,
		struct inode *inode, struct mlfs_ext_path **ppath, mlfs_lblk_t split,
		int split_flag, int flags) 
{
	struct mlfs_ext_path *path = *ppath;
	mlfs_fsblk_t newblock;
	mlfs_lblk_t ee_block;
	struct mlfs_extent *ex, newex, orig_ex, zero_ex;
	struct mlfs_extent *ex2 = NULL;
	unsigned int ee_len, depth;
	int err = 0;

	//mlfs_ext_show_leaf(inode, path);
	
	mlfs_lsm_debug("split at logical block %llu\n", (unsigned long long)split);

	depth = ext_depth(handle, inode);
	ex = path[depth].p_ext;
	ee_block = le32_to_cpu(ex->ee_block);
	ee_len = mlfs_ext_get_actual_len(ex);
	newblock = split - ee_block + mlfs_ext_pblock(ex);

	BUG_ON(split < ee_block || split >= (ee_block + ee_len));

	if (split == ee_block) {
		/*
		 * case b: block @split is the block that the extent begins with
		 * then we just change the state of the extent, and splitting
		 * is not needed.
		 */
		if (split_flag & MLFS_EXT_MARK_UNWRIT2)
			mlfs_ext_mark_unwritten(ex);
		else
			mlfs_ext_mark_initialized(ex);

		if (!(flags & MLFS_GET_BLOCKS_PRE_IO))
			mlfs_ext_try_to_merge(handle, inode, path, ex);

		err = mlfs_ext_dirty(handle, inode, path + path->p_depth);
		goto out;
	}

	/* case a */
	memcpy(&orig_ex, ex, sizeof(orig_ex));
	ex->ee_len = cpu_to_le16(split - ee_block);

	if (split_flag & MLFS_EXT_MARK_UNWRIT1) 
		mlfs_ext_mark_unwritten(ex);

	/*
	 * path may lead to new leaf, not to original leaf any more
	 * after mlfs_ext_insert_extent() returns,
	 */
	err = mlfs_ext_dirty(handle, inode, path + depth);
	if (err) 
		goto fix_extent_len;

	ex2 = &newex;
	ex2->ee_block = cpu_to_le32(split);
	ex2->ee_len = cpu_to_le16(ee_len - (split - ee_block));
	mlfs_ext_store_pblock(ex2, newblock);
	if (split_flag & MLFS_EXT_MARK_UNWRIT2) 
		mlfs_ext_mark_unwritten(ex2);

	err = mlfs_ext_insert_extent(handle, inode, ppath, &newex, flags);

	if (err == -ENOSPC && (MLFS_EXT_MAY_ZEROOUT & split_flag)) {
		if (split_flag & (MLFS_EXT_DATA_VALID1 | MLFS_EXT_DATA_VALID2)) {
			if (split_flag & MLFS_EXT_DATA_VALID1) {
				err = mlfs_ext_zeroout(inode, ex2);
				zero_ex.ee_block = ex2->ee_block;
				zero_ex.ee_len = cpu_to_le16(mlfs_ext_get_actual_len(ex2));
				mlfs_ext_store_pblock(&zero_ex, mlfs_ext_pblock(ex2));
			} else {
				err = mlfs_ext_zeroout(inode, ex);
				zero_ex.ee_block = ex->ee_block;
				zero_ex.ee_len = cpu_to_le16(mlfs_ext_get_actual_len(ex));
				mlfs_ext_store_pblock(&zero_ex, mlfs_ext_pblock(ex));
			}
		} else {
			err = mlfs_ext_zeroout(inode, &orig_ex);
			zero_ex.ee_block = orig_ex.ee_block;
			zero_ex.ee_len = cpu_to_le16(mlfs_ext_get_actual_len(&orig_ex));
			mlfs_ext_store_pblock(&zero_ex, mlfs_ext_pblock(&orig_ex));
		}

		if (err) 
			goto fix_extent_len;
		/* update the extent length and mark as initialized */
		ex->ee_len = cpu_to_le16(ee_len);
		mlfs_ext_try_to_merge(handle, inode, path, ex);
		err = mlfs_ext_dirty(handle, inode, path + path->p_depth);
		if (err) 
			goto fix_extent_len;

		goto out;
	} else if (err)
		goto fix_extent_len;

out:
	//mlfs_ext_show_leaf(inode, path);
	return err;

fix_extent_len:
	ex->ee_len = orig_ex.ee_len;
	mlfs_ext_dirty(handle, inode, path + path->p_depth);
	return err;
}

/*
 * returns 1 if current index have to be freed (even partial)
 */
static int inline mlfs_ext_more_to_rm(struct mlfs_ext_path *path) 
{
	BUG_ON(path->p_idx == NULL);

	if (path->p_idx < EXT_FIRST_INDEX(path->p_hdr)) 
		return 0;

	/*
	 * if truncate on deeper level happened it it wasn't partial
	 * so we have to consider current index for truncation
	 */
	if (le16_to_cpu(path->p_hdr->eh_entries) == path->p_block) 
		return 0;

	return 1;
}

int mlfs_ext_remove_space(handle_t *handle, struct inode *inode, 
		mlfs_lblk_t start, mlfs_lblk_t end)
{
	struct super_block *sb = get_inode_sb(handle->dev, inode);
	int depth = ext_depth(handle, inode);
	struct mlfs_ext_path *path;
	int i = 0, err = 0;

	mlfs_lsm_debug("truncate from %u(0x%x) to %u(0x%x)\n", 
			start << g_block_size_shift,
			start << g_block_size_shift,
			end << g_block_size_shift,
			end << g_block_size_shift);

	/*
	 * Check if we are removing extents inside the extent tree. If that
	 * is the case, we are going to punch a hole inside the extent tree.
	 * We have to check whether we need to split the extent covering
	 * the last block to remove. We can easily remove the part of it
	 * in mlfs_ext_rm_leaf().
	 */
	if (end < EXT_MAX_BLOCKS - 1) {
		struct mlfs_extent *ex;
		mlfs_lblk_t ee_block, ex_end, lblk;
		mlfs_fsblk_t pblk;

		/* find extent for or closest extent to this block */
		path = mlfs_find_extent(handle, inode, end, NULL, MLFS_EX_NOCACHE);
		if (IS_ERR(path)) 
			return PTR_ERR(path);

		//mlfs_ext_show_path(inode, path);

		depth = ext_depth(handle, inode);
		/* Leaf node may not exist only if inode has no blocks at all */
		ex = path[depth].p_ext;
		if (!ex) {
			if (depth) {
				MLFS_ERROR_INODE(inode, "path[%d].p_hdr == NULL", depth);
				err = -EFSCORRUPTED;
			}
			goto out;
		}

		ee_block = le32_to_cpu(ex->ee_block);

		// inode has zero-length.
		if (ee_block + mlfs_ext_get_actual_len(ex) == 0) {
			goto out;
		}

		ex_end = ee_block + mlfs_ext_get_actual_len(ex) - 1;

		/*
		 * See if the last block is inside the extent, if so split
		 * the extent at 'end' block so we can easily remove the
		 * tail of the first part of the split extent in
		 * mlfs_ext_rm_leaf().
		 */
		if (end >= ee_block && end < ex_end) {
			/*
			 * Split the extent in two so that 'end' is the last
			 * block in the first new extent. Also we should not
			 * fail removing space due to ENOSPC so try to use
			 * reserved block if that happens.
			 */
			err = mlfs_force_split_extent_at(handle, inode, &path,
							 end + 1, 1);
			if (err < 0)
				goto out;

		} else if (end >= ex_end) {
			/*
			 * If there's an extent to the right its first cluster
			 * contains the immediate right boundary of the
			 * truncated/punched region. The end < ee_block case
			 * is handled in mlfs_ext_rm_leaf().
			 */
			lblk = ex_end + 1;
			err = mlfs_ext_search_right(handle, inode, path, &lblk, &pblk, &ex);
			if (err)
				goto out;
		}
	}

	/*
	 * we start scanning from right side freeing all the blocks
	 * after i_size and walking into the deep
	 */
	depth = ext_depth(handle, inode);
	if (path) {
		int k = i = depth;
		// ?
		while (--k > 0)
			path[k].p_block =
				le16_to_cpu(path[k].p_hdr->eh_entries)+1;
	} else {
		path = (struct mlfs_ext_path *)mlfs_zalloc(
				sizeof(struct mlfs_ext_path) * (depth + 1));

		if (path == NULL) 
			return -ENOMEM;

		path[0].p_maxdepth = path[0].p_depth = depth;
		path[0].p_hdr = ext_inode_hdr(handle, inode);
		i = 0;

		if (mlfs_ext_check_inode(handle, inode)) {
			err = -EFSCORRUPTED;
			goto out;
		}
	}

	err = 0;

	while (i >= 0 && err == 0) {
		if (i == depth) {
			/* this is leaf block */
			err = mlfs_ext_rm_leaf(handle, inode, path, start, end);
			/* root level have p_bh == NULL, fs_brelse() can handle this. */
			fs_brelse(path[i].p_bh);
			path[i].p_bh = NULL;
			i--;
			continue;
		}

		/* this is index block */
		if (!path[i].p_hdr) {
			path[i].p_hdr = ext_block_hdr(path[i].p_bh);
		}

		if (!path[i].p_idx) {
			/* this level hasn't touched yet */
			path[i].p_idx = EXT_LAST_INDEX(path[i].p_hdr);
			path[i].p_block = le16_to_cpu(path[i].p_hdr->eh_entries) + 1;
			mlfs_lsm_debug("init index ptr: hdr 0x%p, num %d\n",
				  path[i].p_hdr, le16_to_cpu(path[i].p_hdr->eh_entries));
		} else {
			/* we've already was here, see at next index */
			path[i].p_idx--;
		}

		mlfs_lsm_debug("level %d - index, first 0x%p, cur 0x%p\n",
				i, EXT_FIRST_INDEX(path[i].p_hdr),
				path[i].p_idx);

		if (mlfs_ext_more_to_rm(path + i)) {
			struct buffer_head *bh;
			/* go to the next level */
			mlfs_lsm_debug("move to level %d (block %lx)\n",
				  i + 1, mlfs_idx_pblock(path[i].p_idx));
			memset(path + i + 1, 0, sizeof(*path));

			bh = read_extent_tree_block(handle, inode, 
					mlfs_idx_pblock(path[i].p_idx),
					path[0].p_depth - (i + 1), 0);
			if (IS_ERR(bh)) {
				/* should we reset i_size? */
				err = -EIO;
				break;
			}
			path[i + 1].p_bh = bh;

			/* put actual number of indexes to know is this
			 * number got changed at the next iteration */
			path[i].p_block = le16_to_cpu(path[i].p_hdr->eh_entries);
			i++;
		} else {
			/* we finish processing this index, go up */
			if (path[i].p_hdr->eh_entries == 0 && i > 0) {
				/* index is empty, remove it
				 * handle must be already prepared by the
				 * truncatei_leaf() */
				err = mlfs_ext_rm_idx(handle, inode, path, i);
			}
			/* root level have p_bh == NULL, fs_brelse() eats this */
			fs_brelse(path[i].p_bh);
			path[i].p_bh = NULL;
			i--;
			mlfs_lsm_debug("return to level %d\n", i);
		}
	}

	/* TODO: flexible tree reduction should be here */
	if (path->p_hdr->eh_entries == 0) {
		/*
		 * truncate to zero freed all the tree
		 * so, we need to correct eh_depth
		 */
		ext_inode_hdr(handle, inode)->eh_depth = 0;
		ext_inode_hdr(handle, inode)->eh_max =
			cpu_to_le16(mlfs_ext_space_root(inode, 0));
		err = mlfs_ext_dirty(handle, inode, path);
	}
out:
	if (path) {
		mlfs_ext_drop_refs(path);
		mlfs_free(path);
	}

	return err;
}

static int mlfs_ext_convert_to_initialized(handle_t *handle,
		struct inode *inode,
		struct mlfs_ext_path **ppath,
		mlfs_lblk_t split,
		unsigned long blocks, int flags) 
{
	int depth = ext_depth(handle, inode), err;
	struct mlfs_extent *ex = (*ppath)[depth].p_ext;

	assert(le32_to_cpu(ex->ee_block) <= split);

	if (split + blocks ==
			le32_to_cpu(ex->ee_block) + mlfs_ext_get_actual_len(ex)) {
		/* split and initialize right part */
		err = mlfs_split_extent_at(handle, inode, ppath, split,
				MLFS_EXT_MARK_UNWRIT1, flags);

	} else if (le32_to_cpu(ex->ee_block) == split) {
		/* split and initialize left part */
		err = mlfs_split_extent_at(handle, inode, ppath, split + blocks,
				MLFS_EXT_MARK_UNWRIT2, flags);

	} else {
		/* split 1 extent to 3 and initialize the 2nd */
		err = mlfs_split_extent_at(handle, inode, ppath, split + blocks,
				MLFS_EXT_MARK_UNWRIT1 | MLFS_EXT_MARK_UNWRIT2, flags);
		if (0 == err) {
			err = mlfs_split_extent_at(handle, inode, ppath, split,
					MLFS_EXT_MARK_UNWRIT1, flags);
		}
	}

	return err;
}

/*
 * mlfs_ext_determine_hole - determine hole around given block
 * @inode:	inode we lookup in
 * @path:	path in extent tree to @lblk
 * @lblk:	pointer to logical block around which we want to determine hole
 *
 * Determine hole length (and start if easily possible) around given logical
 * block. We don't try too hard to find the beginning of the hole but @path
 * actually points to extent before @lblk, we provide it.
 *
 * The function returns the length of a hole starting at @lblk. We update @lblk
 * to the beginning of the hole if we managed to find it.
 */
static mlfs_lblk_t mlfs_ext_determine_hole(handle_t *handle, struct inode *inode,
		struct mlfs_ext_path *path, mlfs_lblk_t *lblk)
{
	int depth = ext_depth(handle, inode);
	struct mlfs_extent *ex;
	mlfs_lblk_t len;

	ex = path[depth].p_ext;
	if (ex == NULL) {
		/* there is no extent yet, so gap is [0;-] */
		*lblk = 0;
		len = EXT_MAX_BLOCKS;
	} else if (*lblk < le32_to_cpu(ex->ee_block)) {
		len = le32_to_cpu(ex->ee_block) - *lblk;
	} else if (*lblk >= le32_to_cpu(ex->ee_block)
			+ mlfs_ext_get_actual_len(ex)) {
		mlfs_lblk_t next;

		*lblk = le32_to_cpu(ex->ee_block) + mlfs_ext_get_actual_len(ex);
		next = mlfs_ext_next_allocated_block(path);
		BUG_ON(next == *lblk);
		len = next - *lblk;
	} else {
		BUG_ON(1);
	}
	return len;
}

/* Core interface API to get/allocate blocks of an inode 
 *
 * return > 0, number of of blocks already mapped/allocated
 *          if create == 0 and these are pre-allocated blocks
 *          	buffer head is unmapped
 *          otherwise blocks are mapped
 *
 * return = 0, if plain look up failed (blocks have not been allocated)
 *          buffer head is unmapped
 *
 * return < 0, error case.
 *
 */
int mlfs_ext_get_blocks(handle_t *handle, struct inode *inode, 
			struct mlfs_map_blocks *map, int flags)
{
	struct mlfs_ext_path *path = NULL;
	struct mlfs_extent newex, *ex;
	int goal, err = 0, depth;
	mlfs_lblk_t allocated = 0;
	mlfs_fsblk_t next, newblock;
	int create;
	uint64_t tsc_start = 0;

	mlfs_assert(handle !=  NULL);

	create = flags & MLFS_GET_BLOCKS_CREATE_DATA;

	/*mutex_lock(&inode->truncate_mutex);*/
#ifdef KERNFS
	pthread_spin_lock(&inode->truncate_lock);
#endif

#ifdef REUSE_PREVIOUS_PATH 
	if (create) {
		mlfs_ext_drop_refs(inode->previous_path);
		mlfs_free(inode->previous_path); //TODO: continue debugging
		inode->previous_path = NULL;
		goto find_ext_path;
	}

	if (!inode->previous_path || (map->m_flags & MLFS_MAP_LOG_ALLOC))
		goto find_ext_path;

	struct mlfs_ext_path * _path = inode->previous_path;
	depth = ext_depth(handle, inode);
	ex = _path[depth].p_ext;
	if (ex) {
		mlfs_lblk_t ee_block = le32_to_cpu(ex->ee_block);
		mlfs_fsblk_t ee_start = mlfs_ext_pblock(ex);
		unsigned short ee_len;
		
		/*
		 * unwritten extents are treated as holes, except that
		 * we split out initialized portions during a write.
		 */
		ee_len = mlfs_ext_get_actual_len(ex);

		/* find extent covers block. simply return the extent */
		if (in_range(map->m_lblk, ee_block, ee_len)) {
			allocated = ee_len + ee_block - map->m_lblk;

			if (!mlfs_ext_is_unwritten(ex)) {
				newblock = map->m_lblk - ee_block + ee_start;
				inode->invalidate_path = 0;
				map->m_flags |= MLFS_MAP_FOUND;
				goto out;
			}
		}
	} 

	mlfs_ext_drop_refs(_path);
	mlfs_free(_path);
	inode->previous_path = NULL;

#endif
find_ext_path:

#ifdef KERNFS
	if (enable_perf_stats) 
		tsc_start = asm_rdtscp();
#endif

	/* find extent for this block */
	path = mlfs_find_extent(handle, inode, map->m_lblk, NULL, 0);
	if (IS_ERR(path)) {
		err = PTR_ERR(path);
		path = NULL;
		goto out2;
	}

#ifdef KERNFS
	if (enable_perf_stats) 
		g_perf_stats.path_search_tsc += (asm_rdtscp() - tsc_start);
#endif

	depth = ext_depth(handle, inode);

	/*
	 * consistent leaf must not be empty
	 * this situations is possible, though, _during_ tree modification
	 * this is why assert can't be put in mlfs_ext_find_extent()
	 */
	BUG_ON(path[depth].p_ext == NULL && depth != 0);

	ex = path[depth].p_ext;
	if (ex) {
		mlfs_lblk_t ee_block = le32_to_cpu(ex->ee_block);
		mlfs_fsblk_t ee_start = mlfs_ext_pblock(ex);
		unsigned short ee_len;
		
		/*
		 * unwritten extents are treated as holes, except that
		 * we split out initialized portions during a write.
		 */
		ee_len = mlfs_ext_get_actual_len(ex);

		/* find extent covers block. simply return the extent */
		if (in_range(map->m_lblk, ee_block, ee_len)) {
			/* number of remain blocks in the extent */
			allocated = ee_len + ee_block - map->m_lblk;

			// Delete original block and update extent tree for log-structured updates
			// and garbage collection of SSD.
			// FIXME: this is slightly inefficient since it searches mlfs_ext_path repeatedly
			// Deletion of original block and allocating new block could be merged
			// by a new API.
			if (map->m_flags & MLFS_MAP_LOG_ALLOC) {
				int ret;
				ret = __mlfs_ext_truncate(handle, inode, map->m_lblk, 
						map->m_lblk + map->m_len - 1);
				// Set flags to block allocator to do log-structured allocation.
				//flags &= ~MLFS_GET_BLOCKS_CREATE_DATA;
				//flags |= MLFS_GET_BLOCKS_CREATE_DATA_LOG;
				mlfs_assert(ret == 0);

				// TODO: optimize this! Figure out a way to reuse the path
				mlfs_ext_drop_refs(path);
				mlfs_free(path);
				path = mlfs_find_extent(handle, inode, map->m_lblk, NULL, 0);
				mlfs_assert(!IS_ERR(path));
			} else if (mlfs_ext_is_unwritten(ex)) {
				if (create) {
					newblock = map->m_lblk - ee_block + ee_start;
					err = mlfs_ext_convert_to_initialized(handle, inode, 
							&path, map->m_lblk , allocated, flags);
					if (err) {
						goto out2;
					}
				} else {
					newblock = 0;
				}
				map->m_flags |= MLFS_MAP_FOUND;
				goto out;
			} else {
				newblock = map->m_lblk - ee_block + ee_start;
				map->m_flags |= MLFS_MAP_FOUND;
				goto out;
			}
		}
	}

	/*
	 * requested block isn't allocated yet
	 * we couldn't try to create block if create flag is zero
	 */
	if (!create) {
		mlfs_lblk_t hole_start, hole_len;

		hole_start = map->m_lblk;
		hole_len = mlfs_ext_determine_hole(handle, inode, path, &hole_start);

		/* Update hole_len to reflect hole size after map->m_lblk */
		if (hole_start != map->m_lblk)
			hole_len -= map->m_lblk - hole_start;

		map->m_pblk = 0;
		map->m_len = min_t(unsigned int, map->m_len, hole_len);
		err = 0;
		goto out2;
	}

	/* find next allocated block so that we know how many
	 * blocks we can allocate without ovelapping next extent */
	next = mlfs_ext_next_allocated_block(path);
	BUG_ON(next <= map->m_lblk);

	allocated = next - map->m_lblk;

	if ((flags & MLFS_GET_BLOCKS_PRE_IO) && 
			map->m_len > EXT_UNWRITTEN_MAX_LEN)
		map->m_len = EXT_UNWRITTEN_MAX_LEN;

	if (allocated > map->m_len) 
		allocated = map->m_len;

	//goal = mlfs_ext_find_goal(inode, path, map->m_lblk);
	
	newblock = mlfs_new_data_blocks(handle, inode, 
			goal, flags, &allocated, &err);

	if (!newblock) 
		goto out2;

	/* try to insert new extent into found leaf and return */
	newex.ee_block = cpu_to_le32(map->m_lblk);
	mlfs_ext_store_pblock(&newex, newblock);
	newex.ee_len = cpu_to_le16(allocated);

	/* if it's fallocate, mark ex as unwritten */
	if (flags & MLFS_GET_BLOCKS_PRE_IO) {
		mlfs_ext_mark_unwritten(&newex);
	}

	err = mlfs_ext_insert_extent(handle, inode, &path, &newex,
			flags & MLFS_GET_BLOCKS_PRE_IO);

	if (err) {
		/* free data blocks we just allocated */
		mlfs_free_blocks(handle, inode, NULL, mlfs_ext_pblock(&newex),
				le16_to_cpu(newex.ee_len),
				get_default_free_blocks_flags(inode));
		goto out2;
	}

	mlfs_mark_inode_dirty(handle->libfs, inode);

	/* previous routine could use block we allocated */
	if (mlfs_ext_is_unwritten(&newex))
		newblock = 0;
	else
		newblock = mlfs_ext_pblock(&newex);

	map->m_flags |= MLFS_MAP_ALLOCATED;

out:
	if (allocated > map->m_len) 
		allocated = map->m_len;

	//mlfs_ext_show_leaf(inode, path);

	map->m_pblk = newblock;
	map->m_len = allocated;

#ifdef REUSE_PREVIOUS_PATH
	if (inode->invalidate_path) {
		inode->invalidate_path = 0;
		mlfs_ext_drop_refs(inode->previous_path);
		mlfs_free(inode->previous_path); //TODO: continue debugging
		inode->previous_path = NULL;
	}
	else {
		inode->previous_path = path;
		path = NULL;
	}

#endif
out2:
	if (path) {
		/* write back tree changes (internal/leaf nodes) */
		mlfs_ext_drop_refs(path);
		mlfs_free(path);
	}

	// mutex_unlock(&inode->truncate_mutex);
#ifdef KERNFS
	pthread_spin_unlock(&inode->truncate_lock);
#endif

	return err ? err : allocated;
}

/** mlfs_ext_truncate without acquiring lock(mutex). **/
static int __mlfs_ext_truncate(handle_t *handle, struct inode *inode, 
		mlfs_lblk_t start, mlfs_lblk_t end) 
{
	int ret;

	mlfs_assert(handle != NULL);

	ret = mlfs_ext_remove_space(handle, inode, start, end);

	/* Save modifications on i_blocks field of the inode. */
	if (!ret)
		ret = mlfs_mark_inode_dirty(handle->libfs, inode);

	return ret;
}

int mlfs_ext_truncate(handle_t *handle, struct inode *inode, 
		mlfs_lblk_t start, mlfs_lblk_t end) 
{
	int ret;
	
	mlfs_assert(handle != NULL);

#ifdef KERNFS
	pthread_spin_lock(&inode->truncate_lock);
#endif

	ret = mlfs_ext_remove_space(handle, inode, start, end);

	/* Save modifications on i_blocks field of the inode. */
	if (!ret) 
		ret = mlfs_mark_inode_dirty(handle->libfs, inode);

#ifdef KERNFS
	pthread_spin_unlock(&inode->truncate_lock);
#endif

	return ret;
}
