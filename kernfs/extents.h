#ifndef _NEW_BTREE_H
#define _NEW_BTREE_H

#include "mlfs/mlfs_user.h"

#ifdef __x86_64__
#include "fs.h"
#elif __aarch64__
#include "nic/nic_fs.h"
#else
#error "Unsupported architecture."
#endif

#include "io/buffer_head.h"
#include "io/block_io.h"
#include "io/device.h"
#include "mlfs/kerncompat.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t mlfs_crc32c(uint32_t crc, const void *buf, size_t size);

/*
 * With AGGRESSIVE_TEST defined, the capacity of index/leaf blocks
 * becomes very small, so index split, in-depth growing and
 * other hard changes happen much more often.
 * This is for debug purposes only.
 */
#define AGGRESSIVE_TEST_

/*
 * If CHECK_BINSEARCH is defined, then the results of the binary search
 * will also be checked by linear search.
 */
#define CHECK_BINSEARCH__

struct mlfs_extent_handle {
	uint8_t libfs;
	uint8_t dev;
};

typedef struct mlfs_extent_handle handle_t;

/* ERRORs 
 */
#define EFSCORRUPTED	117
#define EFSBADCRC		74

/*
 * mlfs_inode has i_data array (60 bytes total).
 * The first 12 bytes store mlfs_extent_header;
 * the remainder stores an array of mlfs_extent.
 * For non-inode extent blocks, mlfs_extent_tail
 * follows the array.
 */

/*
 * This is the extent tail on-disk structure.
 * All other extent structures are 12 bytes long.  It turns out that
 * block_size % 12 >= 4 for at least all powers of 2 greater than 512, which
 * covers all valid ext4 block sizes.  Therefore, this tail structure can be
 * crammed into the end of the block without having to rebalance the tree.
 */
struct mlfs_extent_tail {
	uint32_t et_checksum; /* crc32c(uuid+inum+extent_block) */
};

#define ET_CHECKSUM_MAGIC 0xF1ABCD1F

/*
 * This is the extent on-disk structure.
 * It's used at the bottom of the tree.
 */
struct mlfs_extent {
	uint32_t ee_block;    /* first logical block extent covers */
	uint16_t ee_len;      /* number of blocks covered by extent */
	uint16_t ee_start_hi; /* high 16 bits of physical block */
	uint32_t ee_start_lo; /* low 32 bits of physical block */
};

/*
 * This is the index on-disk structure.
 * It's used at all the levels except for the bottom.
 */
struct mlfs_extent_idx {
	uint32_t ei_block;   /* index covers logical blocks from 'block' */
	uint32_t ei_leaf_lo; /* pointer to the physical block of the next level. 
							leaf or next index could be there */
	uint16_t ei_leaf_hi; /* high 16 bits of physical block */
	uint16_t ei_unused;
};

/*
 * Each block (leaves and indexes), even inode-stored has header.
 */
struct mlfs_extent_header {
	uint16_t eh_magic;      /* probably will support different formats */
	uint16_t eh_entries;    /* number of valid entries */
	uint16_t eh_max;	/* capacity of store in entries */
	uint16_t eh_depth;      /* has tree real underlying blocks? */
	uint32_t eh_generation; /* generation of the tree */
};

#define MLFS_EXT_MAGIC (0xf30a)

#define MLFS_EXTENT_TAIL_OFFSET(hdr)                 \
	(sizeof(struct mlfs_extent_header) +             \
	 (sizeof(struct mlfs_extent) * (hdr)->eh_max))

static inline struct mlfs_extent_tail *
find_mlfs_extent_tail(struct mlfs_extent_header *eh)
{
#ifdef __cplusplus
	return (struct mlfs_extent_tail *)((eh) +
			MLFS_EXTENT_TAIL_OFFSET(eh));
#else
	return (struct mlfs_extent_tail *)(((void *)eh) +
			MLFS_EXTENT_TAIL_OFFSET(eh));
#endif
}

/*
 * Array of mlfs_ext_path contains path to some extent.
 * It works as a cursor for a given key (logical block).
 * Creation/lookup routines use it for traversal/splitting/etc.
 * Truncate uses it to simulate recursive walking.
 */
struct mlfs_ext_path {
	mlfs_fsblk_t p_block;
	uint16_t p_depth;
	uint16_t p_maxdepth;
	struct mlfs_extent *p_ext;
	struct mlfs_extent_idx *p_idx;
	struct mlfs_extent_header *p_hdr;
	struct buffer_head *p_bh;
};

/*
 * Flags used by ext4_map_blocks()
 */
	/* Allocate any needed blocks and/or convert an unwritten
	   extent to be an initialized ext4 */
#define MLFS_GET_BLOCKS_CREATE 0x0001 //alias of CREATE_DATA
#define MLFS_GET_BLOCKS_CREATE_DATA			0x0001
#define MLFS_GET_BLOCKS_CREATE_DATA_LOG 0x0002
#define MLFS_GET_BLOCKS_CREATE_META			0x0004
	/* Request the creation of an unwritten extent */
#define MLFS_GET_BLOCKS_UNWRIT_EXT		0x0008
#define MLFS_GET_BLOCKS_CREATE_UNWRIT_EXT	(MLFS_GET_BLOCKS_UNWRIT_EXT|\
						 MLFS_GET_BLOCKS_CREATE)
	/* caller is from the direct IO path, request to creation of an
	unwritten extents if not allocated, split the unwritten
	extent if blocks has been preallocated already*/
#define MLFS_GET_BLOCKS_PRE_IO			0x0010
	/* Eventual metadata allocation (due to growing extent tree)
	 * should not fail, so try to use reserved blocks for that.*/
#define MLFS_GET_BLOCKS_METADATA_NOFAIL		0x0020
	/* Convert written extents to unwritten */
#define MLFS_GET_BLOCKS_CONVERT_UNWRITTEN	0x0040
	/* Write zeros to newly created written extents */
#define MLFS_GET_BLOCKS_ZERO			0x0080
#define MLFS_GET_BLOCKS_CREATE_ZERO		(MLFS_GET_BLOCKS_CREATE |\
					MLFS_GET_BLOCKS_ZERO)

/*
 * Flags used in mballoc's allocation_context flags field.
 */
#define MLFS_MB_USE_RESERVED        0x2000

/*
 * The bit position of these flags must not overlap with any of the
 * MLFS_GET_BLOCKS_*.  They are used by mlfs_find_extent(),
 * read_extent_tree_block(), mlfs_split_extent_at(),
 * mlfs_ext_insert_extent(), and mlfs_ext_create_new_leaf().
 * MLFS_EX_NOCACHE is used to indicate that the we shouldn't be
 * caching the extents when reading from the extent tree while a
 *  truncate or punch hole operation is in progress.
 */
#define MLFS_EX_NOCACHE             0x40000000
#define MLFS_EX_FORCE_CACHE         0x20000000

/*
 * Flags used by mlfs_free_blocks
 */
#define MLFS_FREE_BLOCKS_METADATA   0x0001
#define MLFS_FREE_BLOCKS_FORGET     0x0002
#define MLFS_FREE_BLOCKS_VALIDATED  0x0004
#define MLFS_FREE_BLOCKS_NO_QUOT_UPDATE 0x0008
#define MLFS_FREE_BLOCKS_NOFREE_FIRST_CLUSTER   0x0010
#define MLFS_FREE_BLOCKS_NOFREE_LAST_CLUSTER    0x0020

#define MLFS_MAP_NEW        (1 << 0)
#define MLFS_MAP_LOG_ALLOC  (1 << 1)
#define MLFS_MAP_GC_ALLOC   (1 << 2)
#define MLFS_MAP_ALLOCATED  (1 << 3)
#define MLFS_MAP_FOUND		  (1 << 4)

struct mlfs_map_blocks {
	mlfs_fsblk_t m_pblk;
	mlfs_lblk_t m_lblk;
	uint32_t m_len;
	uint32_t m_flags;
};

/*
 * structure for external API
 */

/*
 * EXT_INIT_MAX_LEN is the maximum number of blocks we can have in an
 * initialized extent. This is 2^15 and not (2^16 - 1), since we use the
 * MSB of ee_len field in the extent datastructure to signify if this
 * particular extent is an initialized extent or an uninitialized (i.e.
 * preallocated).
 * EXT_UNINIT_MAX_LEN is the maximum number of blocks we can have in an
 * uninitialized extent.
 * If ee_len is <= 0x8000, it is an initialized extent. Otherwise, it is an
 * uninitialized one. In other words, if MSB of ee_len is set, it is an
 * uninitialized extent with only one special scenario when ee_len = 0x8000.
 * In this case we can not have an uninitialized extent of zero length and
 * thus we make it as a special case of initialized extent with 0x8000 length.
 * This way we get better extent-to-group alignment for initialized extents.
 * Hence, the maximum number of blocks we can have in an *initialized*
 * extent is 2^15 (32768) and in an *uninitialized* extent is 2^15-1 (32767).
 */
#define EXT_INIT_MAX_LEN (1 << 15)
#define EXT_UNWRITTEN_MAX_LEN (EXT_INIT_MAX_LEN - 1)

#define EXT_EXTENT_SIZE sizeof(struct mlfs_extent)
#define EXT_INDEX_SIZE sizeof(struct mlfs_extent_idx)

#define EXT_FIRST_EXTENT(__hdr__)                                 \
	((struct mlfs_extent *)(((char *)(__hdr__)) +                 \
		sizeof(struct mlfs_extent_header)))
#define EXT_FIRST_INDEX(__hdr__)                                  \
	((struct mlfs_extent_idx *)(((char *)(__hdr__)) +             \
		sizeof(struct mlfs_extent_header)))
#define EXT_HAS_FREE_INDEX(__path__)                              \
	((__path__)->p_hdr->eh_entries < (__path__)->p_hdr->eh_max)
#define EXT_LAST_EXTENT(__hdr__)                                  \
	(EXT_FIRST_EXTENT((__hdr__)) + (__hdr__)->eh_entries - 1)
#define EXT_LAST_INDEX(__hdr__)                                   \
	(EXT_FIRST_INDEX((__hdr__)) + (__hdr__)->eh_entries - 1)
#define EXT_MAX_EXTENT(__hdr__)                                   \
	(EXT_FIRST_EXTENT((__hdr__)) + (__hdr__)->eh_max - 1)
#define EXT_MAX_INDEX(__hdr__)                                    \
	(EXT_FIRST_INDEX((__hdr__)) + (__hdr__)->eh_max - 1)

static inline struct mlfs_extent_header *ext_inode_hdr(handle_t *handle, 
		struct inode *inode)
{
	if (handle->dev == g_root_dev)
		return (struct mlfs_extent_header *)inode->l1.i_data;
	else if (handle->dev == g_ssd_dev)
		return (struct mlfs_extent_header *)inode->l2.i_data;
	else if (handle->dev == g_hdd_dev)
		return (struct mlfs_extent_header *)inode->l3.i_data;
	else
		panic("invalid handle: dev is wrong\n");
}

static inline struct mlfs_extent_header *ext_block_hdr(struct buffer_head *bh)
{
	return (struct mlfs_extent_header *)bh->b_data;
}

static inline uint16_t ext_depth(handle_t *handle, struct inode *inode)
{
	return le16_to_cpu(ext_inode_hdr(handle, inode)->eh_depth);
}

static inline uint16_t mlfs_ext_get_actual_len(struct mlfs_extent *ext)
{
	return (le16_to_cpu(ext->ee_len) <= EXT_INIT_MAX_LEN
			? le16_to_cpu(ext->ee_len)
			: (le16_to_cpu(ext->ee_len) - EXT_INIT_MAX_LEN));
}

static inline void mlfs_ext_mark_initialized(struct mlfs_extent *ext)
{
	ext->ee_len = cpu_to_le16(mlfs_ext_get_actual_len(ext));
}

static inline void mlfs_ext_mark_unwritten(struct mlfs_extent *ext)
{
	ext->ee_len |= cpu_to_le16(EXT_INIT_MAX_LEN);
}

static inline int mlfs_ext_is_unwritten(struct mlfs_extent *ext)
{
	/* Extent with ee_len of 0x8000 is treated as an initialized extent */
	return (le16_to_cpu(ext->ee_len) > EXT_INIT_MAX_LEN);
}

static inline mlfs_lblk_t mlfs_ext_lblock(struct mlfs_extent *ex)
{
	return le32_to_cpu(ex->ee_block);
}

static inline mlfs_lblk_t mlfs_idx_lblock(struct mlfs_extent_idx *ix)
{
	return le32_to_cpu(ix->ei_block);
}

/*
 * mlfs_ext_pblock:
 * combine low and high parts of physical block number into mlfs_fsblk_t
 */
static inline mlfs_fsblk_t mlfs_ext_pblock(struct mlfs_extent *ex)
{
	mlfs_fsblk_t block;

	block = ex->ee_start_lo;
	block |= ((mlfs_fsblk_t)ex->ee_start_hi << 31) << 1;
	return block;
}

/*
 * mlfs_idx_pblock:
 * combine low and high parts of a leaf physical block number into mlfs_fsblk_t
 */
static inline mlfs_fsblk_t mlfs_idx_pblock(struct mlfs_extent_idx *ix)
{
	mlfs_fsblk_t block;

	block = ix->ei_leaf_lo;
	block |= ((mlfs_fsblk_t)ix->ei_leaf_hi << 31) << 1;
	return block;
}

static inline void mlfs_ext_set_lblock(struct mlfs_extent *ex,
		mlfs_lblk_t lblk)
{
	ex->ee_block = cpu_to_le32(lblk);
}

static inline void mlfs_idx_set_lblock(struct mlfs_extent_idx *ix,
		mlfs_lblk_t lblk)
{
	ix->ei_block = cpu_to_le32(lblk);
}

/*
 * mlfs_ext_set_pblock:
 * stores a large physical block number into an extent struct,
 * breaking it into parts
 */
static inline void mlfs_ext_set_pblock(struct mlfs_extent *ex,
		mlfs_fsblk_t pb)
{
	ex->ee_start_lo = (unsigned long)(pb & 0xffffffff);
	ex->ee_start_hi = (unsigned long)((pb >> 31) >> 1) & 0xffff;
}

static inline void mlfs_ext_store_pblock(struct mlfs_extent *ex,
		mlfs_fsblk_t pb)
{
	ex->ee_start_lo = (unsigned long)(pb & 0xffffffff);
	ex->ee_start_hi = (unsigned long)((pb >> 31) >> 1) & 0xffff;
}

/*
 * mlfs_idx_set_pblock:
 * stores a large physical block number into an index struct,
 * breaking it into parts
 */
static inline void mlfs_idx_set_pblock(struct mlfs_extent_idx *ix,
		mlfs_fsblk_t pb)
{
	ix->ei_leaf_lo = (unsigned long)(pb & 0xffffffff);
	ix->ei_leaf_hi = (unsigned long)((pb >> 31) >> 1) & 0xffff;
}

static inline void mlfs_idx_store_pblock(struct mlfs_extent_idx *ix,
		mlfs_fsblk_t pb)
{
	ix->ei_leaf_lo = (unsigned long)(pb & 0xffffffff);
	ix->ei_leaf_hi = (unsigned long)((pb >> 31) >> 1) & 0xffff;
}

static inline int in_range(mlfs_lblk_t b, uint32_t first, uint16_t len) {
	if((first + len) == 0)
		return 0;
	else
		return ((b) >= (first) && (b) <= (first) + (len) - 1);
}

void mlfs_ext_init_locks(void);

#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

#define mlfs_ext_dirty(handle, inode, path)  \
	__mlfs_ext_dirty(__func__, __LINE__, (handle), (inode), (path))

int mlfs_ext_alloc_blocks(handle_t *handle, struct inode *inode,
		int goal, unsigned int flags, mlfs_fsblk_t *blockp, mlfs_lblk_t *count);

int mlfs_ext_get_blocks(handle_t *handle, struct inode *inode, 
			struct mlfs_map_blocks *map, int flags);

struct mlfs_ext_path *mlfs_find_extent(handle_t *handle, struct inode *inode, 
		mlfs_lblk_t block, struct mlfs_ext_path **orig_path, int flags);

void mlfs_ext_init(struct super_block *sb);

int mlfs_ext_tree_init(handle_t *handle, struct inode *inode);

static int __mlfs_ext_truncate(handle_t *handle, struct inode *inode, 
		mlfs_lblk_t from, mlfs_lblk_t to);
int mlfs_ext_truncate(handle_t *handle, struct inode *inode, 
		mlfs_lblk_t from, mlfs_lblk_t to);

extern pthread_mutex_t block_bitmap_mutex;
extern pthread_spinlock_t inode_dirty_mutex;

int mlfs_mark_inode_dirty(int id, struct inode *inode);

#ifdef __cplusplus
}
#endif

#endif /* _NEW_BTREE_H */
