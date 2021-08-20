#define _GNU_SOURCE
#include <sched.h> //sched_getcpu

#include "balloc.h"

#ifdef NIC_OFFLOAD
#include "distributed/rpc_interface.h"
#endif

#define SHARED_PARTITION (65536)

uint64_t size_of_bitmap(mlfs_fsblk_t nrblocks)
{
	return (nrblocks >> 3) + ((nrblocks % 8) ? 1 : 0);
}

int blocks_of_bitmap(uint8_t dev, mlfs_fsblk_t nrblocks)
{
	uint64_t bitmapsz = size_of_bitmap(nrblocks);
	return (bitmapsz >> g_bdev[dev]->bd_blocksize_bits) +
		((bitmapsz % g_bdev[dev]->bd_blocksize) ? 1 : 0);
}

struct block_bitmap *alloc_all_bitmap(uint8_t dev, 
		mlfs_fsblk_t nrblocks, mlfs_fsblk_t bitmap_block)
{
	int i, err = 0;
	int bitmap_nrblocks = blocks_of_bitmap(dev, nrblocks);
	uint8_t *bitmap = NULL;
	struct block_bitmap_desc *desc = NULL;
	struct block_bitmap *b_bitmap = 
		(struct block_bitmap *)mlfs_zalloc(sizeof(struct block_bitmap));
	if (!b_bitmap)
		return NULL;

	b_bitmap->b_dev = g_bdev[dev];

	bitmap = (uint8_t *)mlfs_zalloc(
			bitmap_nrblocks << g_bdev[dev]->bd_blocksize_bits);
	if (!bitmap) {
		mlfs_free(b_bitmap);
		return NULL;
	}

	b_bitmap->nr_bits = nrblocks;

	mlfs_muffled("bitmap size %u KB\n", 
			(bitmap_nrblocks << g_bdev[dev]->bd_blocksize_bits) >> 10);

	desc = (struct block_bitmap_desc *)mlfs_zalloc(
			sizeof(struct block_bitmap_desc) * bitmap_nrblocks);

        b_bitmap->bitmap_count = bitmap_nrblocks;
        b_bitmap->bitmap = bitmap;
        b_bitmap->bitmap_desc = desc;
        b_bitmap->bitmap_block = bitmap_block;

	return b_bitmap;
}

/**
 * Read a single bitmap block.
 **/
int read_bitmap_block(uint8_t dev, mlfs_fsblk_t blk_nr, uint8_t *bitmapp)
{
    struct buffer_head *bh;
    int err = 0;
    //bh = mlfs_read(dev, bitmap_block + i, g_block_size_bytes ,&err);
    bh = bh_get_sync_IO(dev, blk_nr, BH_NO_DATA_ALLOC);
    bh->b_size = g_block_size_bytes;
    bh->b_data = bitmapp;
    bh_submit_read_sync_IO(bh);

    mlfs_io_wait(dev, 1);

    bh_release(bh);
    return err;
}

void read_all_bitmap(uint8_t dev, struct block_bitmap *b_bitmap)
{
	int i, err = 0;
	int bitmap_nrblocks = blocks_of_bitmap(dev, b_bitmap->nr_bits);
	uint8_t *bitmap = NULL;
	struct block_bitmap_desc *desc = NULL;

	if (!b_bitmap)
            panic ("b_bitmap is not allocated.\n");

	bitmap = b_bitmap->bitmap;
	desc = b_bitmap->bitmap_desc;
	if (!bitmap || !desc) {
		mlfs_free(b_bitmap);
                panic ("b_bitmap->bitmap is not allocated.\n");
	}

	mlfs_muffled("bitmap size %u KB\n", 
			(bitmap_nrblocks << g_bdev[dev]->bd_blocksize_bits) >> 10);

	for (i = 0; i < bitmap_nrblocks; i++)
                err = read_bitmap_block(dev, b_bitmap->bitmap_block+i, bitmap + (i << g_bdev[dev]->bd_blocksize_bits));
out:
	if (err) {
		if (b_bitmap)
			mlfs_free(b_bitmap);
		if (bitmap)
			mlfs_free(bitmap);
		if (desc)
			mlfs_free(desc);

		b_bitmap = NULL;
	}
}

#ifdef NIC_OFFLOAD
void read_all_bitmap_from_host (int cli_sock_fd, uint8_t dev, struct block_bitmap *b_bitmap)
{
	int i, err = 0;
	int bitmap_nrblocks = blocks_of_bitmap(dev, b_bitmap->nr_bits);
	uint8_t *bitmap = NULL;
	struct block_bitmap_desc *desc = NULL;
        char buf[TCP_BUF_SIZE+1] = {0};
	uint8_t bm;

	if (!b_bitmap)
            panic ("b_bitmap is not allocated.\n");

	bitmap = b_bitmap->bitmap;
	desc = b_bitmap->bitmap_desc;
	if (!bitmap || !desc) {
		mlfs_free(b_bitmap);
                panic ("b_bitmap->bitmap is not allocated.\n");
	}

	mlfs_muffled("bitmap size %u KB\n", 
			(bitmap_nrblocks << g_bdev[dev]->bd_blocksize_bits) >> 10);

	for (i = 0; i < bitmap_nrblocks; i++) {
            tcp_client_req_bitmap_block(cli_sock_fd, buf, dev, b_bitmap->bitmap_block + i);
            memcpy(bitmap + (i << g_bdev[dev]->bd_blocksize_bits), buf, g_block_size_bytes);
	}
out:
	if (err) {
		if (b_bitmap)
			mlfs_free(b_bitmap);
		if (bitmap)
			mlfs_free(bitmap);
		if (desc)
			mlfs_free(desc);

		b_bitmap = NULL;
	}
}
#endif /* NIC_OFFLOAD */

void free_all_bitmap(struct block_bitmap *b_bitmap)
{
	mlfs_free(b_bitmap->bitmap);
	mlfs_free(b_bitmap->bitmap_desc);
	mlfs_free(b_bitmap);
}

void store_all_bitmap(uint8_t dev, struct block_bitmap *b_bitmap)
{
	int i, err = 0;
	int bitmap_nrblocks = b_bitmap->bitmap_count;
	uint8_t *bitmap = b_bitmap->bitmap;
	mlfs_fsblk_t bitmap_block = b_bitmap->bitmap_block;

	for (i = 0; i < bitmap_nrblocks; i++) {
		struct buffer_head *bh;

		if (b_bitmap->bitmap_desc[i].dirty) {
			bh = bh_get_sync_IO(dev, bitmap_block + i, BH_NO_DATA_ALLOC);
			bh->b_data = (bitmap + (i << g_bdev[dev]->bd_blocksize_bits));
			bh->b_size = g_block_size_bytes;

			mlfs_write(bh);

			b_bitmap->bitmap_desc[i].dirty = 0;

			mlfs_debug("bitmap block %d persisted\n", i);

			bh_release(bh);
			}
		/*
		if (b_bitmap->bitmap_desc[i].dirty) {
			bh = get_bh_from_cache(dev, bitmap_block + i,
					g_block_size_bytes, BUF_CACHE_ALLOC);

			memcpy(bh->b_data, bitmap + (i << g_bdev[dev]->bd_blocksize_bits),
					g_bdev[dev]->bd_blocksize);

			mlfs_write(bh);
			fs_mark_buffer_dirty(bh);
			set_buffer_pin(bh);

			b_bitmap->bitmap_desc[i].dirty = 0;
		}
		*/
	}
out:
	return;
}

void bitmap_bits_set(struct block_bitmap *b_bitmap, mlfs_fsblk_t bit)
{
	uint8_t *bitmap = b_bitmap->bitmap;
	int bitmap_block = bit / (b_bitmap->b_dev->bd_blocksize << 3);
	*(bitmap + (bit >> 3)) |= (1 << (bit & 7));
	b_bitmap->bitmap_desc[bitmap_block].dirty = 1;

	//mlfs_info("bitmap block %u is dirty, bit %lu\n", bitmap_block, bit);
}

void bitmap_bits_set_range(struct block_bitmap *b_bitmap, mlfs_fsblk_t bit, 
		uint32_t length) 
{
	uint64_t *bitmap = (uint64_t *)b_bitmap->bitmap;
	int start_bitmap_block, end_bitmap_block, i;
	
	bitmap_set(bitmap, bit, length);

	start_bitmap_block = bit / (g_block_size_bytes << 3);
	end_bitmap_block = (bit + length) / (g_block_size_bytes << 3);

	for (i = start_bitmap_block; i <= end_bitmap_block; i++)  {
		b_bitmap->bitmap_desc[i].dirty = 1;
		mlfs_debug("bitmap block %u is dirty, bit %lu\n", i, bit);
	}
}

static inline void __bitmap_bits_clr(struct block_bitmap *b_bitmap,
		uint8_t *bitmap, mlfs_fsblk_t bit)
{
	/*
	int bitmap_block =
		((bitmap - b_bitmap->bitmap) >> b_bitmap->b_dev->bd_blocksize_bits) +
		(bit >> (b_bitmap->b_dev->bd_blocksize_bits << 3));
	*/
	int bitmap_block =
		((bitmap - b_bitmap->bitmap) / b_bitmap->b_dev->bd_blocksize) +
		(bit / (b_bitmap->b_dev->bd_blocksize << 3));
	*(bitmap + (bit >> 3)) &= ~(1 << (bit & 7));
	b_bitmap->bitmap_desc[bitmap_block].dirty = 1;
}

void bitmap_bits_clr(struct block_bitmap *b_bitmap, mlfs_fsblk_t bit)
{
	uint8_t *bitmap = b_bitmap->bitmap;
	int bitmap_block = bit / (b_bitmap->b_dev->bd_blocksize << 3);
	*(bitmap + (bit >> 3)) &= ~(1 << (bit & 7));
	b_bitmap->bitmap_desc[bitmap_block].dirty = 1;
}

static inline bool bitmap_is_bit_set(uint8_t *bitmap, mlfs_fsblk_t bit)
{
	return (*(bitmap + (bit >> 3)) & (1 << (bit & 7)));
}

static inline bool bitmap_is_bit_clr(uint8_t *bitmap, mlfs_fsblk_t bit)
{
	return !bitmap_is_bit_set(bitmap, bit);
}

void bitmap_bits_free(struct block_bitmap *b_bitmap, 
		mlfs_fsblk_t bit, uint32_t bcnt)
{
	mlfs_fsblk_t i = bit;
	uint8_t *bitmap = b_bitmap->bitmap;

	while (i & 7) {

		if (!bcnt)
			return;

		__bitmap_bits_clr(b_bitmap, bitmap, i);

		bcnt--;
		i++;
	}
	bit = i;
	bitmap += (bit >> 3);

	while (bcnt >= 32) {
		*(uint32_t *)bitmap = 0;
		bitmap += 4;
		bcnt -= 32;
		bit += 32;
	}

	while (bcnt >= 16) {
		*(uint16_t *)bitmap = 0;
		bitmap += 2;
		bcnt -= 16;
		bit += 16;
	}

	while (bcnt >= 8) {
		*bitmap = 0;
		bitmap += 1;
		bcnt -= 8;
		bit += 8;
	}

	for (i = 0; i < bcnt; ++i) {
		__bitmap_bits_clr(b_bitmap, bitmap, i);
	}
}

int bitmap_find_next_clr(struct block_bitmap *b_bitmap, mlfs_fsblk_t start_bit, 
		mlfs_fsblk_t ebit, mlfs_fsblk_t *bit_id)
{
	uint64_t *bitmap = (uint64_t *)b_bitmap->bitmap;

	*bit_id = find_next_zero_bit(bitmap, b_bitmap->nr_bits, start_bit);

	if (*bit_id >= ebit) {
		*bit_id = 0;
		return -ENOSPC;
	}

	return 0;
}

int bitmap_find_next_contiguous_clr(struct block_bitmap *b_bitmap, 
		mlfs_fsblk_t start_bit, mlfs_fsblk_t end_bit, 
		uint32_t length, mlfs_fsblk_t *bit_id)
{
	uint64_t *bitmap = (uint64_t *)b_bitmap->bitmap;

	*bit_id = bitmap_find_next_zero_area(bitmap,
			b_bitmap->nr_bits, start_bit, length, 0);

	if (*bit_id >= end_bit) {
		*bit_id = 0;
		return -ENOSPC;
	}

	return 0;
}

int bitmap_find_bits_clr(struct block_bitmap *b_bitmap, mlfs_fsblk_t bit, 
		mlfs_fsblk_t ebit, mlfs_fsblk_t *bit_id)
{
	mlfs_fsblk_t i;
	uint64_t bcnt = ebit - bit + 1;
	uint8_t *bitmap = b_bitmap->bitmap;

	i = bit;

	while (i & 7) {
		if (!bcnt)
			return -ENOSPC;

		if (bitmap_is_bit_clr(bitmap, i)) {
			*bit_id = bit;
			return 0;
		}

		i++;
		bcnt--;
	}

	bit = i;
	bitmap += (bit >> 3);

	while (bcnt >= 32) {
		if (*(uint32_t *)bitmap != 0xFFFFFFFF)
			goto finish_it;

		bitmap += 4;
		bcnt -= 32;
		bit += 32;
	}

	while (bcnt >= 16) {
		if (*(uint16_t *)bitmap != 0xFFFF)
			goto finish_it;

		bitmap += 2;
		bcnt -= 16;
		bit += 16;
	}

finish_it:
	while (bcnt >= 8) {
		if (*bitmap != 0xFF) {
			for (i = 0; i < 8; ++i) {
				if (bitmap_is_bit_clr(bitmap, i)) {
					*bit_id = bit + i;
					return 0;
				}
			}
		}

		bitmap += 1;
		bcnt -= 8;
		bit += 8;
	}

	for (i = 0; i < bcnt; ++i) {
		if (bitmap_is_bit_clr(bitmap, i)) {
			*bit_id = bit + i;
			return 0;
		}
	}

	return -ENOSPC;
}

#if defined (NIC_OFFLOAD) && defined(NIC_SIDE)
void balloc_init(uint8_t dev, struct super_block *_sb, int initialize, int cli_sock_fd)
#else
void balloc_init(uint8_t dev, struct super_block *_sb, int initialize)
#endif
{
  _sb->s_blk_bitmap = alloc_all_bitmap(dev,
      _sb->ondisk->ndatablocks, _sb->ondisk->bmap_start);
#if defined(NIC_OFFLOAD) && defined(NIC_SIDE)
  read_all_bitmap_from_host(cli_sock_fd, dev, _sb->s_blk_bitmap);
#else
  read_all_bitmap(dev, _sb->s_blk_bitmap);
#endif

  // compute used_blocks from bitmap.
  _sb->used_blocks = bitmap_weight((uint64_t *)_sb->s_blk_bitmap->bitmap,
      _sb->ondisk->ndatablocks);

  mlfs_info("[dev %u] used blocks %lu\n", dev, _sb->used_blocks);
#if 0
  {
    mlfs_fsblk_t a;
    bitmap_find_next_clr(_sb->s_blk_bitmap,
        0, _sb->ondisk->ndatablocks - 1, &a);
    mlfs_debug("first unused block %lx\n", a);
  }
#endif

  mlfs_alloc_block_free_lists(_sb);
  mlfs_init_blockmap(_sb, initialize);

	if (!initialize) 
		mlfs_build_blocknode_map(_sb, (uint64_t *)_sb->s_blk_bitmap->bitmap,
				_sb->s_blk_bitmap->nr_bits, 0);
}

static struct mlfs_range_node *mlfs_alloc_range_node(struct super_block *sb)
{
	struct mlfs_range_node *p;
	p = (struct mlfs_range_node *)mlfs_alloc(sizeof(struct mlfs_range_node));
	return p;
}

struct mlfs_range_node *mlfs_alloc_blocknode(struct super_block *sb)
{
	return mlfs_alloc_range_node(sb);
}

struct mlfs_range_node *mlfs_alloc_inode_node(struct super_block *sb)
{
	return mlfs_alloc_range_node(sb);
}

static inline
struct free_list *mlfs_get_free_list(struct super_block *sb, int cpu)
{
	if (cpu < sb->n_partition)
		return &sb->free_lists[cpu];
	else
		return &sb->shared_free_list;
}

static inline int mlfs_rbtree_compare_rangenode(struct mlfs_range_node *curr,
		unsigned long range_low)
{
	if (range_low < curr->range_low)
		return -1;
	if (range_low > curr->range_high)
		return 1;

	return 0;
}

static void mlfs_free_range_node(struct mlfs_range_node *node)
{
	mlfs_free(node);
}

static void mlfs_free_blocknode(struct super_block *sb,
		    struct mlfs_range_node *node)
{
	mlfs_free_range_node(node);
}

static int mlfs_insert_range_node(struct super_block *sb,
	struct rb_root *tree, struct mlfs_range_node *new_node)
{
	struct mlfs_range_node *curr;
	struct rb_node **temp, *parent;
	int compVal;

	temp = &(tree->rb_node);
	parent = NULL;

	while (*temp) {
		curr = container_of(*temp, struct mlfs_range_node, node);
		compVal = mlfs_rbtree_compare_rangenode(curr,
					new_node->range_low);
		parent = *temp;

		if (compVal == -1) {
			temp = &((*temp)->rb_left);
		} else if (compVal == 1) {
			temp = &((*temp)->rb_right);
		} else {
			mlfs_info("%s: entry %lu - %lu already exists: "
				"%lu - %lu\n", __func__,
				new_node->range_low,
				new_node->range_high,
				curr->range_low,
				curr->range_high);
			return -EINVAL;
		}
	}

	rb_link_node(&new_node->node, parent, temp);
	rb_insert_color(&new_node->node, tree);

	return 0;
}

static inline int mlfs_insert_blocktree(struct super_block *sb,
	struct rb_root *tree, struct mlfs_range_node *new_node)
{
	int ret;

	ret = mlfs_insert_range_node(sb, tree, new_node);
	if (ret)
		mlfs_info("ERROR: %s failed %d\n", __func__, ret);

	return ret;
}

int mlfs_alloc_block_free_lists(struct super_block *sb)
{
	int i;
	struct free_list *free_list;
	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);

	sb->free_lists = mlfs_alloc(sb->n_partition * sizeof(struct free_list));

	if (!sb->free_lists)
		return -ENOMEM;

	for (i = 0; i < sb->n_partition; i++) {
		free_list = mlfs_get_free_list(sb, i);
		free_list->block_free_tree = RB_ROOT;
		free_list->num_free_blocks = 0;
		pthread_mutex_init(&free_list->mutex, &attr);
	}

	return 0;
}

void mlfs_delete_free_lists(struct super_block *sb)
{
	/* Each tree is freed in save_blocknode_mappings */
	mlfs_free(sb->free_lists);
	sb->free_lists = NULL;
}

void mlfs_init_blockmap(struct super_block *sb, int initialize)
{
	struct rb_root *tree;
	unsigned long num_used_block;
	struct mlfs_range_node *blknode;
	struct free_list *free_list;
	unsigned long per_list_blocks;
	int i;
	int ret;

	num_used_block = sb->reserved_blocks;

	/* Divide the block range among per-partition free lists */
	per_list_blocks = sb->num_blocks / sb->n_partition;

	mlfs_debug("blocks in each segment %lu (%lu MB)\n", 
			per_list_blocks, per_list_blocks >> 8);

	sb->per_list_blocks = per_list_blocks;
	for (i = 0; i < sb->n_partition; i++) {
		free_list = mlfs_get_free_list(sb, i);
		free_list->id = i;

		tree = &(free_list->block_free_tree);

		free_list->block_start = per_list_blocks * i;
		free_list->block_end = free_list->block_start +
			per_list_blocks - 1;

		free_list->first_node = NULL;
		free_list->num_blocknode = 0;

		/* For recovery, update these fields later */
		if (initialize) {
			free_list->num_free_blocks = per_list_blocks;
			if (i == 0) {
				free_list->block_start += num_used_block;
				free_list->num_free_blocks -= num_used_block;
			}

			blknode = mlfs_alloc_blocknode(sb);
			if (blknode == NULL)
				panic("cannot allocate blocknode\n");
			blknode->range_low = free_list->block_start;
			blknode->range_high = free_list->block_end;
			ret = mlfs_insert_blocktree(sb, tree, blknode);
			if (ret) {
				mlfs_info("%s\n", "fail to insert block tree");
				mlfs_free_blocknode(sb, blknode);
				return;
			}
			free_list->first_node = blknode;
			free_list->num_blocknode = 1;
		}

	}

	free_list = mlfs_get_free_list(sb, (sb->n_partition - 1));
	if (free_list->block_end + 1 < sb->num_blocks) {
		/* Shared free list gets any remaining blocks */
		sb->shared_free_list.block_start = free_list->block_end + 1;
		sb->shared_free_list.block_end = sb->num_blocks - 1;
		mlfs_debug("initialize shared free list : %lu ~ %lu\n",
				sb->shared_free_list.block_start, 
				sb->shared_free_list.block_end);
	}
}

static int mlfs_find_range_node(struct super_block *sb,
	struct rb_root *tree, unsigned long range_low,
	struct mlfs_range_node **ret_node)
{
	struct mlfs_range_node *curr = NULL;
	struct rb_node *temp;
	int compVal;
	int ret = 0;

	temp = tree->rb_node;

	while (temp) {
		curr = container_of(temp, struct mlfs_range_node, node);
		compVal = mlfs_rbtree_compare_rangenode(curr, range_low);

		if (compVal == -1) {
			temp = temp->rb_left;
		} else if (compVal == 1) {
			temp = temp->rb_right;
		} else {
			ret = 1;
			break;
		}
	}

	*ret_node = curr;
	return ret;
}

int mlfs_find_free_slot(struct super_block *sb,
	struct rb_root *tree, unsigned long range_low,
	unsigned long range_high, struct mlfs_range_node **prev,
	struct mlfs_range_node **next)
{
	struct mlfs_range_node *ret_node = NULL;
	struct rb_node *temp;
	int ret;

	ret = mlfs_find_range_node(sb, tree, range_low, &ret_node);
	if (ret) {
		mlfs_info("ERROR: %lu - %lu already in free list\n",
			range_low, range_high);
		fflush(stdout);
		return -EINVAL;
	}

	if (!ret_node) {
		*prev = *next = NULL;
	} else if (ret_node->range_high < range_low) {
		*prev = ret_node;
		temp = rb_next(&ret_node->node);
		if (temp)
			*next = container_of(temp, struct mlfs_range_node, node);
		else
			*next = NULL;
	} else if (ret_node->range_low > range_high) {
		*next = ret_node;
		temp = rb_prev(&ret_node->node);
		if (temp)
			*prev = container_of(temp, struct mlfs_range_node, node);
		else
			*prev = NULL;
	} else {
		mlfs_info("%s ERROR: %lu - %lu overlaps with existing node "
			"%lu - %lu\n", __func__, range_low,
			range_high, ret_node->range_low,
			ret_node->range_high);
		return -EINVAL;
	}

	return 0;
}

int mlfs_free_blocks_node(struct super_block *sb, unsigned long blocknr,
	int num, unsigned short btype, int log_page)
{
	struct rb_root *tree;
	unsigned long block_low;
	unsigned long block_high;
	unsigned long num_blocks = 0;
	struct mlfs_range_node *prev = NULL;
	struct mlfs_range_node *next = NULL;
	struct mlfs_range_node *curr_node;
	struct free_list *free_list;
	int id;
	int new_node_used = 0;
	int ret;

	if (num <= 0) {
		mlfs_info("ERROR: free %d\n", num);
		return -EINVAL;
	}

	id = blocknr / sb->per_list_blocks;
	if (id >= sb->n_partition)
		id = SHARED_PARTITION;

	/* Pre-allocate blocknode */
	curr_node = mlfs_alloc_blocknode(sb);
	if (curr_node == NULL) {
		/* returning without freeing the block*/
		return -ENOMEM;
	}

	free_list = mlfs_get_free_list(sb, id);
	pthread_mutex_lock(&free_list->mutex);

	tree = &(free_list->block_free_tree);

	// Current block size of MLFS is 4 KB.
	num_blocks = 1 * num;
	block_low = blocknr;
	block_high = blocknr + num_blocks - 1;

	mlfs_debug("Free: %lu - %lu\n", block_low, block_high);

	ret = mlfs_find_free_slot(sb, tree, block_low,
					block_high, &prev, &next);

	if (ret) {
		mlfs_debug("%s: find free slot fail: %d\n", __func__, ret);
		pthread_mutex_unlock(&free_list->mutex);
		mlfs_free_blocknode(sb, curr_node);
		return ret;
	}

	if (prev && next && (block_low == prev->range_high + 1) &&
			(block_high + 1 == next->range_low)) {
		/* fits the hole */
		rb_erase(&next->node, tree);
		free_list->num_blocknode--;
		prev->range_high = next->range_high;
		mlfs_free_blocknode(sb, next);
		goto block_found;
	}
	if (prev && (block_low == prev->range_high + 1)) {
		/* Aligns left */
		prev->range_high += num_blocks;
		goto block_found;
	}
	if (next && (block_high + 1 == next->range_low)) {
		/* Aligns right */
		next->range_low -= num_blocks;
		goto block_found;
	}

	/* Aligns somewhere in the middle */
	curr_node->range_low = block_low;
	curr_node->range_high = block_high;
	new_node_used = 1;
	ret = mlfs_insert_blocktree(sb, tree, curr_node);
	if (ret) {
		new_node_used = 0;
		goto out;
	}
	if (!prev)
		free_list->first_node = curr_node;
	free_list->num_blocknode++;

block_found:
	free_list->num_free_blocks += num_blocks;

	if (log_page) {
		free_list->free_log_count++;
		free_list->freed_log_pages += num_blocks;
	} else {
		free_list->free_data_count++;
		free_list->freed_data_pages += num_blocks;
	}

out:
	pthread_mutex_unlock(&free_list->mutex);
	if (new_node_used == 0)
		mlfs_free_blocknode(sb, curr_node);

	return ret;
}

static unsigned long mlfs_alloc_blocks_in_free_list(struct super_block *sb,
	struct free_list *free_list, enum alloc_type atype,
	unsigned long num_blocks, unsigned long *new_blocknr)
{
	struct rb_root *tree;
	struct mlfs_range_node *curr, *next = NULL;
	struct rb_node *temp, *next_node;
	unsigned long curr_blocks;
	bool found = 0;
	unsigned long step = 0;

	tree = &(free_list->block_free_tree);

	if (atype == TREE) {
		temp = rb_last(tree);
	} else {
		temp = &(free_list->first_node->node);
	}

	while (temp) {
		step++;
		curr = container_of(temp, struct mlfs_range_node, node);

		curr_blocks = curr->range_high - curr->range_low + 1;

		if (num_blocks >= curr_blocks) {

			/* Otherwise, allocate the whole blocknode */
			if (curr == free_list->first_node) {
				next_node = rb_next(temp);
				if (next_node)
					next = container_of(next_node,
						struct mlfs_range_node, node);
				free_list->first_node = next;
			}

			rb_erase(&curr->node, tree);
			free_list->num_blocknode--;
			num_blocks = curr_blocks;
			*new_blocknr = curr->range_low;
			mlfs_free_blocknode(sb, curr);
			found = 1;
			break;
		}

		if (atype == TREE) {
			*new_blocknr = curr->range_high;
			curr->range_high -= num_blocks;
		} else {
			*new_blocknr = curr->range_low;
			curr->range_low += num_blocks;
		}
		found = 1;
		break;
	}

	free_list->num_free_blocks -= num_blocks;

	if (found == 0)
		return -ENOSPC;

	return num_blocks;
}

static int mlfs_find_free_list(struct super_block *sb, 
		unsigned int num, enum alloc_type a_type)
{
	struct free_list *free_list;
	int i;

	if (a_type == DATA_LOG) {
		// FIXME: find out GCed segment.
		for (i = 0; i < sb->n_partition; i++) {
			free_list = mlfs_get_free_list(sb, i);
			if (free_list->num_free_blocks > num) {
				return i;
			}
		}
	} else {
		/* Find out the first free list to allocate requested blocks */
		for (i = 0; i < sb->n_partition; i++) {
			free_list = mlfs_get_free_list(sb, i);
			if (free_list->num_free_blocks > num) {
				return i;
			}
		}
	}

	return -1;
}

/* core part of mlfs block allocator.
 * block allocation policy is different from device type.
 * NVM, HDD: atype == DATA - in-place update
 * SSD: DATA_LOG - log-structured update
 */
int mlfs_new_blocks(struct super_block *sb, unsigned long *blocknr,
	unsigned int num, unsigned short btype, int zero,
	enum alloc_type atype, int goal)
{
	struct free_list *free_list;
	void *bp;
	unsigned long num_blocks = 0;
	unsigned long ret_blocks = 0;
	unsigned long new_blocknr = 0;
	struct rb_node *temp;
	struct mlfs_range_node *first;
	int id;
	int retried = 0;
	UNUSED(btype);
	UNUSED(goal);

	num_blocks = num * 1;
	if (num_blocks == 0)
		return -EINVAL;

	/* per-CPU allocations
	if (atype == TREE)
		id = SHARED_PARTITION;
	else
		id = sched_getcpu();
	*/

	/*if (atype == TREE) {
		id = sb->n_partition - 1;

		if (id == -1)
			id = mlfs_find_free_list(sb, num, atype);
	} else */ 
	
	id = mlfs_find_free_list(sb, num, atype);

	if (id == -1)
		return -ENOSPC;

retry:
	free_list = mlfs_get_free_list(sb, id);
	pthread_mutex_lock(&free_list->mutex);

	if (free_list->num_free_blocks < num_blocks || !free_list->first_node) {
		mlfs_debug("id %d, free_blocks %lu, required %lu, blocknode %lu\n", 
				id, free_list->num_free_blocks, num_blocks,
				free_list->num_blocknode);
		if (free_list->num_free_blocks >= num_blocks) {
			mlfs_debug("%s\n", "first node is NULL "
					"but still has free blocks");
			temp = rb_first(&free_list->block_free_tree);
			first = container_of(temp, struct mlfs_range_node, node);
			free_list->first_node = first;
		} else {
			pthread_mutex_unlock(&free_list->mutex);
			if (retried >= 3) {
				int i = 0;
				mlfs_info("fail to allocate block %u, current segid %d\n",
						num, id);
				for (i = 0; i < sb->n_partition; i++) {
					free_list = mlfs_get_free_list(sb, i);
					mlfs_info("id %d, freeblocks %lu\n", i, free_list->num_free_blocks);
				}
				return -ENOSPC;
			}
			id = mlfs_find_free_list(sb, num, atype);
			mlfs_assert(id >= 0);
			retried++;
			goto retry;
		}
	}

	ret_blocks = mlfs_alloc_blocks_in_free_list(sb, free_list, atype,
			num_blocks, &new_blocknr);

	mlfs_debug("Alloc %lu blocks from freelist %d: %lu ~ %lu\n", 
			ret_blocks, free_list->id, new_blocknr, new_blocknr + ret_blocks - 1);
	if (atype == TREE) {
		free_list->alloc_log_count++;
		free_list->alloc_log_pages += ret_blocks;
	} else if (atype == DATA) {
		free_list->alloc_data_count++;
		free_list->alloc_data_pages += ret_blocks;
	}

	pthread_mutex_unlock(&free_list->mutex);

	if (ret_blocks <= 0 || new_blocknr == 0)
		return -ENOSPC;

	if (zero) {
		panic("Unsupported code path\n");
	}

	*blocknr = new_blocknr;

	//return ret_blocks / mlfs_get_numblocks(btype);
	return ret_blocks;
}

unsigned long mlfs_count_free_blocks(struct super_block *sb)
{
	struct free_list *free_list;
	unsigned long num_free_blocks = 0;
	int i;

	for (i = 0; i < sb->n_partition; i++) {
		free_list = mlfs_get_free_list(sb, i);
		num_free_blocks += free_list->num_free_blocks;
		mlfs_debug("id %d, freeblocks %lu\n", i, free_list->num_free_blocks);
	}

	free_list = mlfs_get_free_list(sb, SHARED_PARTITION);
	num_free_blocks += free_list->num_free_blocks;

	return num_free_blocks;
}

static int mlfs_insert_blocknode_map(struct super_block *sb,
		int id, unsigned long low, unsigned long high)
{
	struct free_list *free_list;
	struct rb_root *tree;
	struct mlfs_range_node *blknode = NULL;
	unsigned long num_blocks = 0;
	int ret;

	num_blocks = high - low + 1;
	mlfs_debug("id %d, low %lu, high %lu, num %lu\n",
			id, low, high, num_blocks);
	free_list = mlfs_get_free_list(sb, id);
	tree = &(free_list->block_free_tree);

	blknode = mlfs_alloc_blocknode(sb);
	if (blknode == NULL)
		return -ENOMEM;

	blknode->range_low = low;
	blknode->range_high = high;
	ret = mlfs_insert_blocktree(sb, tree, blknode);
	if (ret) {
		mlfs_info("%s\n", "failed to insert blocktree");
		mlfs_free_blocknode(sb, blknode);
		goto out;
	}
	if (!free_list->first_node)
		free_list->first_node = blknode;
	free_list->num_blocknode++;
	free_list->num_free_blocks += num_blocks;
	
	mlfs_debug("insert block tree - id %u # of free blocks %lu\n",
			free_list->id, free_list->num_free_blocks);
out:
	return ret;
}

int mlfs_build_blocknode_map(struct super_block *sb,
		unsigned long *bitmap, unsigned long bsize, unsigned long scale)
{
	struct free_list *free_list;
	unsigned long next = 0;
	unsigned long low = 0;
	unsigned long start, end;
	int id = 0;

	free_list = mlfs_get_free_list(sb, id);
	start = free_list->block_start;
	end = free_list->block_end + 1;
	while (1) {
		next = find_next_zero_bit(bitmap, end, start);
		if (next == bsize)
			break;
		if (next == end) {
			mlfs_debug("id %u # of free blocks %lu\n",
					free_list->id, free_list->num_free_blocks);

			if (id == sb->n_partition - 1)
				id = SHARED_PARTITION;
			else
				id++;
			free_list = mlfs_get_free_list(sb, id);
			start = free_list->block_start;
			end = free_list->block_end + 1;

			continue;
		}

		low = next;
		next = find_next_bit(bitmap, end, next);
		if (mlfs_insert_blocknode_map(sb, id,
					low << scale, (next << scale) - 1)) {
			mlfs_info("Error: could not insert %lu - %lu\n",
					low << scale, ((next << scale) - 1));
		}
		start = next;
		if (next == bsize)
			break;
		if (next == end) {
			mlfs_debug("id %u # of free blocks %lu\n",
					free_list->id, free_list->num_free_blocks);

			if (id == sb->n_partition - 1)
				id = SHARED_PARTITION;
			else
				id++;
			free_list = mlfs_get_free_list(sb, id);
			start = free_list->block_start;
			end = free_list->block_end + 1;
		}
	}

	mlfs_debug("total free blocks %lu\n", mlfs_count_free_blocks(sb));
	return 0;
}
