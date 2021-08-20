#ifndef _BALLOC_H_
#define _BALLOC_H_

//#include "ds/bitops.h"
#include "ds/bitmap.h"
#include "io/block_io.h"
#include "io/device.h"

#ifdef __cplusplus
extern "C" {
#endif

struct block_bitmap_desc {
	int dirty:1;
};

struct block_bitmap {
	struct block_device *b_dev;
	uint8_t *bitmap;
	struct block_bitmap_desc *bitmap_desc;
	int bitmap_count;
	uint64_t nr_bits;
	mlfs_fsblk_t bitmap_block;
};

//bitmap operations
struct block_bitmap *alloc_all_bitmap(uint8_t dev, 
		mlfs_fsblk_t nrblocks, mlfs_fsblk_t bitmap_block);
int read_bitmap_block(uint8_t dev, mlfs_fsblk_t blk_nr, uint8_t *bitmapp);
void read_all_bitmap(uint8_t dev, struct block_bitmap *b_bitmap);

void free_all_bitmap(struct block_bitmap *b_bitmap);

void store_all_bitmap(uint8_t dev, struct block_bitmap *b_bitmap);

uint64_t size_of_bitmap(mlfs_fsblk_t nrblocks);

int blocks_of_bitmap(uint8_t dev, mlfs_fsblk_t nrblocks);

void bitmap_bits_set(struct block_bitmap *b_bitmap, mlfs_fsblk_t bit);

void bitmap_bits_set_range(struct block_bitmap *b_bitmap, mlfs_fsblk_t bit, 
		uint32_t length);

void bitmap_bits_clr(struct block_bitmap *b_bitmap, mlfs_fsblk_t bit);

void bitmap_bits_free(struct block_bitmap *b_bitmap, mlfs_fsblk_t bit, 
		uint32_t bcnt);

int bitmap_find_bits_clr(struct block_bitmap *b_bitmap, mlfs_fsblk_t bit, 
		mlfs_fsblk_t ebit, mlfs_fsblk_t *bit_id);

int bitmap_find_next_clr(struct block_bitmap *b_bitmap, mlfs_fsblk_t start_bit, 
		mlfs_fsblk_t ebit, mlfs_fsblk_t *bit_id);

int bitmap_find_next_contiguous_clr(struct block_bitmap *b_bitmap, 
		mlfs_fsblk_t start_bit, mlfs_fsblk_t end_bit, 
		uint32_t length, mlfs_fsblk_t *bit_id);

enum alloc_type {
	TREE = 1,
	DATA,	// allow in-place updates
	DATA_LOG, // allow blocks from GCed segment
};

#if defined (NIC_OFFLOAD) && defined(NIC_SIDE)
void balloc_init(uint8_t dev, struct super_block *_sb, int initialize, int cli_sock_fd);
#else
void balloc_init(uint8_t dev, struct super_block *_sb, int initialize);
#endif
int mlfs_alloc_block_free_lists(struct super_block *sb);
void mlfs_init_blockmap(struct super_block *sb, int initialize);
int mlfs_new_blocks(struct super_block *sb, unsigned long *blocknr,
	unsigned int num, unsigned short btype, int zero,
	enum alloc_type atype, int goal);
int mlfs_build_blocknode_map(struct super_block *sb, unsigned long *bitmap, 
		unsigned long bsize, unsigned long scale);
int mlfs_free_blocks_node(struct super_block *sb, unsigned long blocknr,
	int num, unsigned short btype, int log_page);
unsigned long mlfs_count_free_blocks(struct super_block *sb);
#endif

#ifdef __cplusplus
}
#endif
