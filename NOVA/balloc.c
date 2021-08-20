/*
 * NOVA persistent memory management
 *
 * Copyright 2015-2016 Regents of the University of California,
 * UCSD Non-Volatile Systems Lab, Andiry Xu <jix024@cs.ucsd.edu>
 * Copyright 2012-2013 Intel Corporation
 * Copyright 2009-2011 Marco Stornelli <marco.stornelli@gmail.com>
 * Copyright 2003 Sony Corporation
 * Copyright 2003 Matsushita Electric Industrial Co., Ltd.
 * 2003-2004 (c) MontaVista Software, Inc. , Steve Longerbeam
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/fs.h>
#include <linux/bitops.h>
#include "nova.h"

int nova_alloc_block_free_lists(struct super_block *sb)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);
	struct free_list *free_list;
	int i;

	sbi->free_lists = kzalloc(sbi->cpus * sizeof(struct free_list),
							GFP_KERNEL);

	if (!sbi->free_lists)
		return -ENOMEM;

	for (i = 0; i < sbi->cpus; i++) {
		free_list = nova_get_free_list(sb, i);
		free_list->block_free_tree = RB_ROOT;
		spin_lock_init(&free_list->s_lock);
	}

	return 0;
}

void nova_delete_free_lists(struct super_block *sb)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);

	/* Each tree is freed in save_blocknode_mappings */
	kfree(sbi->free_lists);
	sbi->free_lists = NULL;
}

void nova_init_blockmap(struct super_block *sb, int recovery)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);
	struct rb_root *tree;
	unsigned long num_used_block;
	struct nova_range_node *blknode;
	struct free_list *free_list;
	unsigned long per_list_blocks;
	int i;
	int ret;

	num_used_block = sbi->reserved_blocks;

	/* Divide the block range among per-CPU free lists */
	per_list_blocks = sbi->num_blocks / sbi->cpus;
	sbi->per_list_blocks = per_list_blocks;
	for (i = 0; i < sbi->cpus; i++) {
		free_list = nova_get_free_list(sb, i);
		tree = &(free_list->block_free_tree);
		free_list->block_start = per_list_blocks * i;
		free_list->block_end = free_list->block_start +
						per_list_blocks - 1;

		/* For recovery, update these fields later */
		if (recovery == 0) {
			free_list->num_free_blocks = per_list_blocks;
			if (i == 0) {
				free_list->block_start += num_used_block;
				free_list->num_free_blocks -= num_used_block;
			}

			blknode = nova_alloc_blocknode(sb);
			if (blknode == NULL)
				NOVA_ASSERT(0);
			blknode->range_low = free_list->block_start;
			blknode->range_high = free_list->block_end;
			ret = nova_insert_blocktree(sbi, tree, blknode);
			if (ret) {
				nova_err(sb, "%s failed\n", __func__);
				nova_free_blocknode(sb, blknode);
				return;
			}
			free_list->first_node = blknode;
			free_list->num_blocknode = 1;
		}
	}

	free_list = nova_get_free_list(sb, (sbi->cpus - 1));
	if (free_list->block_end + 1 < sbi->num_blocks) {
		/* Shared free list gets any remaining blocks */
		sbi->shared_free_list.block_start = free_list->block_end + 1;
		sbi->shared_free_list.block_end = sbi->num_blocks - 1;
	}
}

static inline int nova_rbtree_compare_rangenode(struct nova_range_node *curr,
	unsigned long range_low)
{
	if (range_low < curr->range_low)
		return -1;
	if (range_low > curr->range_high)
		return 1;

	return 0;
}

static int nova_find_range_node(struct nova_sb_info *sbi,
	struct rb_root *tree, unsigned long range_low,
	struct nova_range_node **ret_node)
{
	struct nova_range_node *curr = NULL;
	struct rb_node *temp;
	int compVal;
	int ret = 0;

	temp = tree->rb_node;

	while (temp) {
		curr = container_of(temp, struct nova_range_node, node);
		compVal = nova_rbtree_compare_rangenode(curr, range_low);

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

inline int nova_search_inodetree(struct nova_sb_info *sbi,
	unsigned long ino, struct nova_range_node **ret_node)
{
	struct rb_root *tree;
	unsigned long internal_ino;
	int cpu;

	cpu = ino % sbi->cpus;
	tree = &sbi->inode_maps[cpu].inode_inuse_tree;
	internal_ino = ino / sbi->cpus;
	return nova_find_range_node(sbi, tree, internal_ino, ret_node);
}

static int nova_insert_range_node(struct nova_sb_info *sbi,
	struct rb_root *tree, struct nova_range_node *new_node)
{
	struct nova_range_node *curr;
	struct rb_node **temp, *parent;
	int compVal;

	temp = &(tree->rb_node);
	parent = NULL;

	while (*temp) {
		curr = container_of(*temp, struct nova_range_node, node);
		compVal = nova_rbtree_compare_rangenode(curr,
					new_node->range_low);
		parent = *temp;

		if (compVal == -1) {
			temp = &((*temp)->rb_left);
		} else if (compVal == 1) {
			temp = &((*temp)->rb_right);
		} else {
			nova_dbg("%s: entry %lu - %lu already exists: "
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

inline int nova_insert_blocktree(struct nova_sb_info *sbi,
	struct rb_root *tree, struct nova_range_node *new_node)
{
	int ret;

	ret = nova_insert_range_node(sbi, tree, new_node);
	if (ret)
		nova_dbg("ERROR: %s failed %d\n", __func__, ret);

	return ret;
}

inline int nova_insert_inodetree(struct nova_sb_info *sbi,
	struct nova_range_node *new_node, int cpu)
{
	struct rb_root *tree;
	int ret;

	tree = &sbi->inode_maps[cpu].inode_inuse_tree;
	ret = nova_insert_range_node(sbi, tree, new_node);
	if (ret)
		nova_dbg("ERROR: %s failed %d\n", __func__, ret);

	return ret;
}

/* Used for both block free tree and inode inuse tree */
int nova_find_free_slot(struct nova_sb_info *sbi,
	struct rb_root *tree, unsigned long range_low,
	unsigned long range_high, struct nova_range_node **prev,
	struct nova_range_node **next)
{
	struct nova_range_node *ret_node = NULL;
	struct rb_node *temp;
	int ret;

	ret = nova_find_range_node(sbi, tree, range_low, &ret_node);
	if (ret) {
		nova_dbg("%s ERROR: %lu - %lu already in free list\n",
			__func__, range_low, range_high);
		return -EINVAL;
	}

	if (!ret_node) {
		*prev = *next = NULL;
	} else if (ret_node->range_high < range_low) {
		*prev = ret_node;
		temp = rb_next(&ret_node->node);
		if (temp)
			*next = container_of(temp, struct nova_range_node, node);
		else
			*next = NULL;
	} else if (ret_node->range_low > range_high) {
		*next = ret_node;
		temp = rb_prev(&ret_node->node);
		if (temp)
			*prev = container_of(temp, struct nova_range_node, node);
		else
			*prev = NULL;
	} else {
		nova_dbg("%s ERROR: %lu - %lu overlaps with existing node "
			"%lu - %lu\n", __func__, range_low,
			range_high, ret_node->range_low,
			ret_node->range_high);
		return -EINVAL;
	}

	return 0;
}

static int nova_free_blocks(struct super_block *sb, unsigned long blocknr,
	int num, unsigned short btype, int log_page)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);
	struct rb_root *tree;
	unsigned long block_low;
	unsigned long block_high;
	unsigned long num_blocks = 0;
	struct nova_range_node *prev = NULL;
	struct nova_range_node *next = NULL;
	struct nova_range_node *curr_node;
	struct free_list *free_list;
	int cpuid;
	int new_node_used = 0;
	int ret;

	if (num <= 0) {
		nova_dbg("%s ERROR: free %d\n", __func__, num);
		return -EINVAL;
	}

	cpuid = blocknr / sbi->per_list_blocks;
	if (cpuid >= sbi->cpus)
		cpuid = SHARED_CPU;

	/* Pre-allocate blocknode */
	curr_node = nova_alloc_blocknode(sb);
	if (curr_node == NULL) {
		/* returning without freeing the block*/
		return -ENOMEM;
	}

	free_list = nova_get_free_list(sb, cpuid);
	spin_lock(&free_list->s_lock);

	tree = &(free_list->block_free_tree);

	num_blocks = nova_get_numblocks(btype) * num;
	block_low = blocknr;
	block_high = blocknr + num_blocks - 1;

	nova_dbgv("Free: %lu - %lu\n", block_low, block_high);

	ret = nova_find_free_slot(sbi, tree, block_low,
					block_high, &prev, &next);

	if (ret) {
		nova_dbg("%s: find free slot fail: %d\n", __func__, ret);
		spin_unlock(&free_list->s_lock);
		nova_free_blocknode(sb, curr_node);
		return ret;
	}

	if (prev && next && (block_low == prev->range_high + 1) &&
			(block_high + 1 == next->range_low)) {
		/* fits the hole */
		rb_erase(&next->node, tree);
		free_list->num_blocknode--;
		prev->range_high = next->range_high;
		nova_free_blocknode(sb, next);
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
	ret = nova_insert_blocktree(sbi, tree, curr_node);
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
	spin_unlock(&free_list->s_lock);
	if (new_node_used == 0)
		nova_free_blocknode(sb, curr_node);

	return ret;
}

int nova_free_data_blocks(struct super_block *sb, struct nova_inode *pi,
	unsigned long blocknr, int num)
{
	int ret;
	timing_t free_time;

	nova_dbgv("Inode %llu: free %d data block from %lu to %lu\n",
			pi->nova_ino, num, blocknr, blocknr + num - 1);
	if (blocknr == 0) {
		nova_dbg("%s: ERROR: %lu, %d\n", __func__, blocknr, num);
		return -EINVAL;
	}
	NOVA_START_TIMING(free_data_t, free_time);
	ret = nova_free_blocks(sb, blocknr, num, pi->i_blk_type, 0);
	if (ret)
		nova_err(sb, "Inode %llu: free %d data block from %lu to %lu "
				"failed!\n", pi->nova_ino, num, blocknr,
				blocknr + num - 1);
	NOVA_END_TIMING(free_data_t, free_time);

	return ret;
}

int nova_free_log_blocks(struct super_block *sb, struct nova_inode *pi,
	unsigned long blocknr, int num)
{
	int ret;
	timing_t free_time;

	nova_dbgv("Inode %llu: free %d log block from %lu to %lu\n",
			pi->nova_ino, num, blocknr, blocknr + num - 1);
	if (blocknr == 0) {
		nova_dbg("%s: ERROR: %lu, %d\n", __func__, blocknr, num);
		return -EINVAL;
	}
	NOVA_START_TIMING(free_log_t, free_time);
	ret = nova_free_blocks(sb, blocknr, num, pi->i_blk_type, 1);
	if (ret)
		nova_err(sb, "Inode %llu: free %d log block from %lu to %lu "
				"failed!\n", pi->nova_ino, num, blocknr,
				blocknr + num - 1);
	NOVA_END_TIMING(free_log_t, free_time);

	return ret;
}

static unsigned long nova_alloc_blocks_in_free_list(struct super_block *sb,
	struct free_list *free_list, unsigned short btype,
	unsigned long num_blocks, unsigned long *new_blocknr)
{
	struct rb_root *tree;
	struct nova_range_node *curr, *next = NULL;
	struct rb_node *temp, *next_node;
	unsigned long curr_blocks;
	bool found = 0;
	unsigned long step = 0;

	tree = &(free_list->block_free_tree);
	temp = &(free_list->first_node->node);

	while (temp) {
		step++;
		curr = container_of(temp, struct nova_range_node, node);

		curr_blocks = curr->range_high - curr->range_low + 1;

		if (num_blocks >= curr_blocks) {
			/* Superpage allocation must succeed */
			if (btype > 0 && num_blocks > curr_blocks) {
				temp = rb_next(temp);
				continue;
			}

			/* Otherwise, allocate the whole blocknode */
			if (curr == free_list->first_node) {
				next_node = rb_next(temp);
				if (next_node)
					next = container_of(next_node,
						struct nova_range_node, node);
				free_list->first_node = next;
			}

			rb_erase(&curr->node, tree);
			free_list->num_blocknode--;
			num_blocks = curr_blocks;
			*new_blocknr = curr->range_low;
			nova_free_blocknode(sb, curr);
			found = 1;
			break;
		}

		/* Allocate partial blocknode */
		*new_blocknr = curr->range_low;
		curr->range_low += num_blocks;
		found = 1;
		break;
	}

	free_list->num_free_blocks -= num_blocks;

	NOVA_STATS_ADD(alloc_steps, step);

	if (found == 0)
		return -ENOSPC;

	return num_blocks;
}

/* Find out the free list with most free blocks */
static int nova_get_candidate_free_list(struct super_block *sb)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);
	struct free_list *free_list;
	int cpuid = 0;
	int num_free_blocks = 0;
	int i;

	for (i = 0; i < sbi->cpus; i++) {
		free_list = nova_get_free_list(sb, i);
		if (free_list->num_free_blocks > num_free_blocks) {
			cpuid = i;
			num_free_blocks = free_list->num_free_blocks;
		}
	}

	return cpuid;
}

/* Return how many blocks allocated */
static int nova_new_blocks(struct super_block *sb, unsigned long *blocknr,
	unsigned int num, unsigned short btype, int zero,
	enum alloc_type atype)
{
	struct free_list *free_list;
	void *bp;
	unsigned long num_blocks = 0;
	unsigned long ret_blocks = 0;
	unsigned long new_blocknr = 0;
	struct rb_node *temp;
	struct nova_range_node *first;
	int cpuid;
	int retried = 0;

	num_blocks = num * nova_get_numblocks(btype);
	if (num_blocks == 0)
		return -EINVAL;

	cpuid = smp_processor_id();

retry:
	free_list = nova_get_free_list(sb, cpuid);
	spin_lock(&free_list->s_lock);

	if (free_list->num_free_blocks < num_blocks || !free_list->first_node) {
		nova_dbgv("%s: cpu %d, free_blocks %lu, required %lu, "
			"blocknode %lu\n", __func__, cpuid,
			free_list->num_free_blocks, num_blocks,
			free_list->num_blocknode);
		if (free_list->num_free_blocks >= num_blocks) {
			nova_dbg("first node is NULL "
				"but still has free blocks\n");
			temp = rb_first(&free_list->block_free_tree);
			first = container_of(temp, struct nova_range_node, node);
			free_list->first_node = first;
		} else {
			spin_unlock(&free_list->s_lock);
			if (retried >= 3)
				return -ENOSPC;
			cpuid = nova_get_candidate_free_list(sb);
			retried++;
			goto retry;
		}
	}

	ret_blocks = nova_alloc_blocks_in_free_list(sb, free_list, btype,
						num_blocks, &new_blocknr);

	if (atype == LOG) {
		free_list->alloc_log_count++;
		free_list->alloc_log_pages += ret_blocks;
	} else if (atype == DATA) {
		free_list->alloc_data_count++;
		free_list->alloc_data_pages += ret_blocks;
	}

	spin_unlock(&free_list->s_lock);

	if (ret_blocks <= 0 || new_blocknr == 0)
		return -ENOSPC;

	if (zero) {
		bp = nova_get_block(sb, nova_get_block_off(sb,
						new_blocknr, btype));
		memset_nt(bp, 0, PAGE_SIZE * ret_blocks);
	}
	*blocknr = new_blocknr;

	nova_dbg_verbose("Alloc %lu NVMM blocks 0x%lx\n", ret_blocks, *blocknr);
	return ret_blocks / nova_get_numblocks(btype);
}

inline int nova_new_data_blocks(struct super_block *sb, struct nova_inode *pi,
	unsigned long *blocknr,	unsigned int num, unsigned long start_blk,
	int zero, int cow)
{
	int allocated;
	timing_t alloc_time;
	NOVA_START_TIMING(new_data_blocks_t, alloc_time);
	allocated = nova_new_blocks(sb, blocknr, num,
					pi->i_blk_type, zero, DATA);
	NOVA_END_TIMING(new_data_blocks_t, alloc_time);
	nova_dbgv("Inode %llu, start blk %lu, cow %d, "
			"alloc %d data blocks from %lu to %lu\n",
			pi->nova_ino, start_blk, cow, allocated, *blocknr,
			*blocknr + allocated - 1);
	return allocated;
}

inline int nova_new_log_blocks(struct super_block *sb, struct nova_inode *pi,
	unsigned long *blocknr, unsigned int num, int zero)
{
	int allocated;
	timing_t alloc_time;
	NOVA_START_TIMING(new_log_blocks_t, alloc_time);
	allocated = nova_new_blocks(sb, blocknr, num,
					pi->i_blk_type, zero, LOG);
	NOVA_END_TIMING(new_log_blocks_t, alloc_time);
	nova_dbgv("Inode %llu, alloc %d log blocks from %lu to %lu\n",
			pi->nova_ino, allocated, *blocknr,
			*blocknr + allocated - 1);
	return allocated;
}

unsigned long nova_count_free_blocks(struct super_block *sb)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);
	struct free_list *free_list;
	unsigned long num_free_blocks = 0;
	int i;

	for (i = 0; i < sbi->cpus; i++) {
		free_list = nova_get_free_list(sb, i);
		num_free_blocks += free_list->num_free_blocks;
	}

	free_list = nova_get_free_list(sb, SHARED_CPU);
	num_free_blocks += free_list->num_free_blocks;
	return num_free_blocks;
}


