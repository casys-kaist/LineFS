/*
 * BRIEF DESCRIPTION
 *
 * Inode methods (allocate/free/read/write).
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

#include <linux/fs.h>
#include <linux/aio.h>
#include <linux/highuid.h>
#include <linux/module.h>
#include <linux/mpage.h>
#include <linux/backing-dev.h>
#include <linux/types.h>
#include <linux/ratelimit.h>
#include "nova.h"

unsigned int blk_type_to_shift[NOVA_BLOCK_TYPE_MAX] = {12, 21, 30};
uint32_t blk_type_to_size[NOVA_BLOCK_TYPE_MAX] = {0x1000, 0x200000, 0x40000000};

int nova_init_inode_inuse_list(struct super_block *sb)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);
	struct nova_range_node *range_node;
	struct inode_map *inode_map;
	unsigned long range_high;
	int i;
	int ret;

	sbi->s_inodes_used_count = NOVA_NORMAL_INODE_START;

	range_high = (NOVA_NORMAL_INODE_START - 1) / sbi->cpus;
	if (NOVA_NORMAL_INODE_START % sbi->cpus)
		range_high++;

	for (i = 0; i < sbi->cpus; i++) {
		inode_map = &sbi->inode_maps[i];
		range_node = nova_alloc_inode_node(sb);
		if (range_node == NULL)
			/* FIXME: free allocated memories */
			return -ENOMEM;

		range_node->range_low = 0;
		range_node->range_high = range_high;
		ret = nova_insert_inodetree(sbi, range_node, i);
		if (ret) {
			nova_err(sb, "%s failed\n", __func__);
			nova_free_inode_node(sb, range_node);
			return ret;
		}
		inode_map->num_range_node_inode = 1;
		inode_map->first_inode_range = range_node;
	}

	return 0;
}

int nova_init_inode_table(struct super_block *sb)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);
	struct inode_table *inode_table;
	struct nova_inode *pi = nova_get_inode_by_ino(sb, NOVA_INODETABLE_INO);
	unsigned long blocknr;
	u64 block;
	int allocated;
	int i;

	pi->i_mode = 0;
	pi->i_uid = 0;
	pi->i_gid = 0;
	pi->i_links_count = cpu_to_le16(1);
	pi->i_flags = 0;
	pi->nova_ino = NOVA_INODETABLE_INO;

	pi->i_blk_type = NOVA_BLOCK_TYPE_2M;

	for (i = 0; i < sbi->cpus; i++) {
		inode_table = nova_get_inode_table(sb, i);
		if (!inode_table)
			return -EINVAL;

		allocated = nova_new_log_blocks(sb, pi, &blocknr, 1, 1);
		nova_dbg_verbose("%s: allocate log @ 0x%lx\n", __func__,
							blocknr);
		if (allocated != 1 || blocknr == 0)
			return -ENOSPC;

		block = nova_get_block_off(sb, blocknr, NOVA_BLOCK_TYPE_2M);
		inode_table->log_head = block;
		nova_flush_buffer(inode_table, CACHELINE_SIZE, 0);
	}

	PERSISTENT_BARRIER();
	return 0;
}

int nova_get_inode_address(struct super_block *sb, u64 ino,
	u64 *pi_addr, int extendable)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);
	struct nova_inode *pi;
	struct inode_table *inode_table;
	unsigned int data_bits;
	unsigned int num_inodes_bits;
	u64 curr;
	unsigned int superpage_count;
	u64 internal_ino;
	int cpuid;
	unsigned int index;
	unsigned int i = 0;
	unsigned long blocknr;
	unsigned long curr_addr;
	int allocated;

	pi = nova_get_inode_by_ino(sb, NOVA_INODETABLE_INO);
	data_bits = blk_type_to_shift[pi->i_blk_type];
	num_inodes_bits = data_bits - NOVA_INODE_BITS;

	cpuid = ino % sbi->cpus;
	internal_ino = ino / sbi->cpus;

	inode_table = nova_get_inode_table(sb, cpuid);
	superpage_count = internal_ino >> num_inodes_bits;
	index = internal_ino & ((1 << num_inodes_bits) - 1);

	curr = inode_table->log_head;
	if (curr == 0)
		return -EINVAL;

	for (i = 0; i < superpage_count; i++) {
		if (curr == 0)
			return -EINVAL;

		curr_addr = (unsigned long)nova_get_block(sb, curr);
		/* Next page pointer in the last 8 bytes of the superpage */
		curr_addr += 2097152 - 8;
		curr = *(u64 *)(curr_addr);

		if (curr == 0) {
			if (extendable == 0)
				return -EINVAL;

			allocated = nova_new_log_blocks(sb, pi, &blocknr,
							1, 1);

			if (allocated != 1)
				return allocated;

			curr = nova_get_block_off(sb, blocknr,
						NOVA_BLOCK_TYPE_2M);
			*(u64 *)(curr_addr) = curr;
			nova_flush_buffer((void *)curr_addr,
						NOVA_INODE_SIZE, 1);
		}
	}

	*pi_addr = curr + index * NOVA_INODE_SIZE;

	return 0;
}

static inline int nova_free_contiguous_data_blocks(struct super_block *sb,
	struct nova_inode_info_header *sih, struct nova_inode *pi,
	struct nova_file_write_entry *entry, unsigned long pgoff,
	unsigned long num_pages, unsigned long *start_blocknr,
	unsigned long *num_free)
{
	int freed = 0;
	unsigned long nvmm;

	if (entry->num_pages < entry->invalid_pages + num_pages) {
		nova_err(sb, "%s: inode %lu, entry pgoff %llu, %llu pages, "
				"invalid %llu, try to free %lu, pgoff %lu\n",
				__func__, sih->ino, entry->pgoff,
				entry->num_pages, entry->invalid_pages,
				num_pages, pgoff);
		return freed;
	}

	entry->invalid_pages += num_pages;
	nvmm = get_nvmm(sb, sih, entry, pgoff);

	if (*start_blocknr == 0) {
		*start_blocknr = nvmm;
		*num_free = num_pages;
	} else {
		if (nvmm == *start_blocknr + *num_free) {
			(*num_free) += num_pages;
		} else {
			/* A new start */
			nova_free_data_blocks(sb, pi, *start_blocknr,
						*num_free);
			freed = *num_free;
			*start_blocknr = nvmm;
			*num_free = num_pages;
		}
	}

	return freed;
}

static int nova_free_contiguous_log_blocks(struct super_block *sb,
	struct nova_inode *pi, u64 head)
{
	struct nova_inode_log_page *curr_page;
	unsigned long blocknr, start_blocknr = 0;
	u64 curr_block = head;
	u32 btype = pi->i_blk_type;
	int num_free = 0;
	int freed = 0;

	while (curr_block) {
		if (curr_block & INVALID_MASK) {
			nova_dbg("%s: ERROR: invalid block %llu\n",
					__func__, curr_block);
			break;
		}
		curr_page = (struct nova_inode_log_page *)nova_get_block(sb,
							curr_block);

		blocknr = nova_get_blocknr(sb, le64_to_cpu(curr_block),
				    btype);
		nova_dbg_verbose("%s: free page %llu\n", __func__, curr_block);
		curr_block = curr_page->page_tail.next_page;

		if (start_blocknr == 0) {
			start_blocknr = blocknr;
			num_free = 1;
		} else {
			if (blocknr == start_blocknr + num_free) {
				num_free++;
			} else {
				/* A new start */
				nova_free_log_blocks(sb, pi, start_blocknr,
							num_free);
				freed += num_free;
				start_blocknr = blocknr;
				num_free = 1;
			}
		}
	}
	if (start_blocknr) {
		nova_free_log_blocks(sb, pi, start_blocknr, num_free);
		freed += num_free;
	}

	return freed;
}

static int nova_delete_cache_tree(struct super_block *sb,
	struct nova_inode *pi, struct nova_inode_info_header *sih,
	unsigned long start_blocknr, unsigned long last_blocknr)
{
	unsigned long addr;
	unsigned long i;
	int deleted = 0;
	void *ret;

	nova_dbgv("%s: inode %lu, mmap pages %lu, start %lu, last %lu\n",
			__func__, sih->ino, sih->mmap_pages,
			start_blocknr, last_blocknr);

	for (i = start_blocknr; i <= last_blocknr; i++) {
		addr = (unsigned long)radix_tree_lookup(&sih->cache_tree, i);
		if (addr) {
			ret = radix_tree_delete(&sih->cache_tree, i);
			nova_free_data_blocks(sb, pi, addr >> PAGE_SHIFT, 1);
			sih->mmap_pages--;
			deleted++;
		}
	}

	nova_dbgv("%s: inode %lu, deleted mmap pages %d\n",
			__func__, sih->ino, deleted);

	if (sih->mmap_pages == 0) {
		sih->low_dirty = ULONG_MAX;
		sih->high_dirty = 0;
	}

	return 0;
}

static int nova_zero_cache_tree(struct super_block *sb,
	struct nova_inode *pi, struct nova_inode_info_header *sih,
	unsigned long start_blocknr)
{
	unsigned long block;
	unsigned long i;
	void *addr;

	nova_dbgv("%s: inode %lu, mmap pages %lu, start %lu, last %lu, "
			"size %lu", __func__, sih->ino, sih->mmap_pages,
			start_blocknr, sih->high_dirty, sih->i_size);

	for (i = start_blocknr; i <= sih->high_dirty; i++) {
		block = (unsigned long)radix_tree_lookup(&sih->cache_tree, i);
		if (block) {
			addr = nova_get_block(sb, block);
			memset(addr, 0, PAGE_SIZE);
		}
	}

	return 0;
}


int nova_delete_file_tree(struct super_block *sb,
	struct nova_inode_info_header *sih, unsigned long start_blocknr,
	unsigned long last_blocknr, bool delete_nvmm, bool delete_mmap)
{
	struct nova_file_write_entry *entry;
	struct nova_inode *pi;
	unsigned long free_blocknr = 0, num_free = 0;
	unsigned long pgoff = start_blocknr;
	timing_t delete_time;
	int freed = 0;
	void *ret;

	pi = (struct nova_inode *)nova_get_block(sb, sih->pi_addr);

	NOVA_START_TIMING(delete_file_tree_t, delete_time);

	if (delete_mmap && sih->mmap_pages)
		nova_delete_cache_tree(sb, pi, sih, start_blocknr,
						last_blocknr);

	if (sih->mmap_pages && start_blocknr <= sih->high_dirty)
		nova_zero_cache_tree(sb, pi, sih, start_blocknr);

	pgoff = start_blocknr;
	while (pgoff <= last_blocknr) {
		entry = radix_tree_lookup(&sih->tree, pgoff);
		if (entry) {
			ret = radix_tree_delete(&sih->tree, pgoff);
			BUG_ON(!ret || ret != entry);
			if (delete_nvmm)
				freed += nova_free_contiguous_data_blocks(sb,
						sih, pi, entry, pgoff, 1,
						&free_blocknr, &num_free);
			pgoff++;
		} else {
			/* We are finding a hole. Jump to the next entry. */
			entry = nova_find_next_entry(sb, sih, pgoff);
			if (!entry)
				break;
			pgoff++;
			pgoff = pgoff > entry->pgoff ? pgoff : entry->pgoff;
		}
	}

	if (free_blocknr) {
		nova_free_data_blocks(sb, pi, free_blocknr, num_free);
		freed += num_free;
	}

	NOVA_END_TIMING(delete_file_tree_t, delete_time);
	nova_dbgv("Inode %llu: delete file tree from pgoff %lu to %lu, "
			"%d blocks freed\n",
			pi->nova_ino, start_blocknr, last_blocknr, freed);

	return freed;
}

static int nova_free_dram_resource(struct super_block *sb,
	struct nova_inode_info_header *sih)
{
	unsigned long last_blocknr;
	int freed = 0;

	if (!(S_ISREG(sih->i_mode)) && !(S_ISDIR(sih->i_mode)))
		return 0;

	if (S_ISREG(sih->i_mode)) {
		last_blocknr = nova_get_last_blocknr(sb, sih);
		freed = nova_delete_file_tree(sb, sih, 0,
						last_blocknr, false, true);
	} else {
		nova_delete_dir_tree(sb, sih);
		freed = 1;
	}

	return freed;
}

/*
 * Free data blocks from inode in the range start <=> end
 */
static void nova_truncate_file_blocks(struct inode *inode, loff_t start,
				    loff_t end)
{
	struct super_block *sb = inode->i_sb;
	struct nova_inode *pi = nova_get_inode(sb, inode);
	struct nova_inode_info *si = NOVA_I(inode);
	struct nova_inode_info_header *sih = &si->header;
	unsigned int data_bits = blk_type_to_shift[pi->i_blk_type];
	unsigned long first_blocknr, last_blocknr;
	int freed = 0;

	inode->i_mtime = inode->i_ctime = CURRENT_TIME_SEC;

	nova_dbg_verbose("truncate: pi %p iblocks %llx %llx %llx %llx\n", pi,
			 pi->i_blocks, start, end, pi->i_size);

	first_blocknr = (start + (1UL << data_bits) - 1) >> data_bits;

	if (end == 0)
		return;
	last_blocknr = (end - 1) >> data_bits;

	if (first_blocknr > last_blocknr)
		return;

	freed = nova_delete_file_tree(sb, sih, first_blocknr,
						last_blocknr, 1, 0);

	inode->i_blocks -= (freed * (1 << (data_bits -
				sb->s_blocksize_bits)));

	pi->i_blocks = cpu_to_le64(inode->i_blocks);
	/* Check for the flag EOFBLOCKS is still valid after the set size */
	check_eof_blocks(sb, pi, inode->i_size);

	return;
}

struct nova_file_write_entry *nova_find_next_entry(struct super_block *sb,
	struct nova_inode_info_header *sih, pgoff_t pgoff)
{
	struct nova_file_write_entry *entry = NULL;
	struct nova_file_write_entry *entries[1];
	int nr_entries;

	nr_entries = radix_tree_gang_lookup(&sih->tree,
					(void **)entries, pgoff, 1);
	if (nr_entries == 1)
		entry = entries[0];

	return entry;
}

/* search the radix tree to find hole or data
 * in the specified range
 * Input:
 * first_blocknr: first block in the specified range
 * last_blocknr: last_blocknr in the specified range
 * @data_found: indicates whether data blocks were found
 * @hole_found: indicates whether a hole was found
 * hole: whether we are looking for a hole or data
 */
static int nova_lookup_hole_in_range(struct super_block *sb,
	struct nova_inode_info_header *sih,
	unsigned long first_blocknr, unsigned long last_blocknr,
	int *data_found, int *hole_found, int hole)
{
	struct nova_file_write_entry *entry;
	unsigned long blocks = 0;
	unsigned long pgoff, old_pgoff;

	pgoff = first_blocknr;
	while (pgoff <= last_blocknr) {
		old_pgoff = pgoff;
		entry = radix_tree_lookup(&sih->tree, pgoff);
		if (entry) {
			*data_found = 1;
			if (!hole)
				goto done;
			pgoff++;
		} else {
			*hole_found = 1;
			entry = nova_find_next_entry(sb, sih, pgoff);
			pgoff++;
			if (entry) {
				pgoff = pgoff > entry->pgoff ?
					pgoff : entry->pgoff;
				if (pgoff > last_blocknr)
					pgoff = last_blocknr + 1;
			}
		}

		if (!*hole_found || !hole)
			blocks += pgoff - old_pgoff;
	}
done:
	return blocks;
}

int nova_assign_write_entry(struct super_block *sb,
	struct nova_inode *pi,
	struct nova_inode_info_header *sih,
	struct nova_file_write_entry *entry,
	bool free)
{
	struct nova_file_write_entry *old_entry;
	void **pentry;
	unsigned long old_nvmm;
	unsigned long start_pgoff = entry->pgoff;
	unsigned int num = entry->num_pages;
	unsigned long curr_pgoff;
	int i;
	int ret;
	timing_t assign_time;

	NOVA_START_TIMING(assign_t, assign_time);
	for (i = 0; i < num; i++) {
		curr_pgoff = start_pgoff + i;

		pentry = radix_tree_lookup_slot(&sih->tree, curr_pgoff);
		if (pentry) {
			old_entry = radix_tree_deref_slot(pentry);
			old_nvmm = get_nvmm(sb, sih, old_entry, curr_pgoff);
			if (free) {
				old_entry->invalid_pages++;
				nova_free_data_blocks(sb, pi, old_nvmm, 1);
				pi->i_blocks--;
			}
			radix_tree_replace_slot(pentry, entry);
		} else {
			ret = radix_tree_insert(&sih->tree, curr_pgoff, entry);
			if (ret) {
				nova_dbg("%s: ERROR %d\n", __func__, ret);
				goto out;
			}
		}
	}

out:
	NOVA_END_TIMING(assign_t, assign_time);

	return ret;
}

static int nova_read_inode(struct super_block *sb, struct inode *inode,
	u64 pi_addr)
{
	struct nova_inode_info *si = NOVA_I(inode);
	struct nova_inode *pi;
	struct nova_inode_info_header *sih = &si->header;
	int ret = -EIO;
	unsigned long ino;

	pi = (struct nova_inode *)nova_get_block(sb, pi_addr);
	inode->i_mode = sih->i_mode;
	i_uid_write(inode, le32_to_cpu(pi->i_uid));
	i_gid_write(inode, le32_to_cpu(pi->i_gid));
//	set_nlink(inode, le16_to_cpu(pi->i_links_count));
	inode->i_generation = le32_to_cpu(pi->i_generation);
	nova_set_inode_flags(inode, pi, le32_to_cpu(pi->i_flags));
	ino = inode->i_ino;

	/* check if the inode is active. */
	if (inode->i_mode == 0 || pi->valid == 0) {
		/* this inode is deleted */
		ret = -ESTALE;
		goto bad_inode;
	}

	inode->i_blocks = le64_to_cpu(pi->i_blocks);
	inode->i_mapping->a_ops = &nova_aops_dax;

	switch (inode->i_mode & S_IFMT) {
	case S_IFREG:
		inode->i_op = &nova_file_inode_operations;
		inode->i_fop = &nova_dax_file_operations;
		break;
	case S_IFDIR:
		inode->i_op = &nova_dir_inode_operations;
		inode->i_fop = &nova_dir_operations;
		break;
	case S_IFLNK:
		inode->i_op = &nova_symlink_inode_operations;
		break;
	default:
		inode->i_op = &nova_special_inode_operations;
		init_special_inode(inode, inode->i_mode,
				   le32_to_cpu(pi->dev.rdev));
		break;
	}

	/* Update size and time after rebuild the tree */
	inode->i_size = le64_to_cpu(sih->i_size);
	inode->i_atime.tv_sec = le32_to_cpu(pi->i_atime);
	inode->i_ctime.tv_sec = le32_to_cpu(pi->i_ctime);
	inode->i_mtime.tv_sec = le32_to_cpu(pi->i_mtime);
	inode->i_atime.tv_nsec = inode->i_mtime.tv_nsec =
					 inode->i_ctime.tv_nsec = 0;
	set_nlink(inode, le16_to_cpu(pi->i_links_count));
	return 0;

bad_inode:
	make_bad_inode(inode);
	return ret;
}

static void nova_get_inode_flags(struct inode *inode, struct nova_inode *pi)
{
	unsigned int flags = inode->i_flags;
	unsigned int nova_flags = le32_to_cpu(pi->i_flags);

	nova_flags &= ~(FS_SYNC_FL | FS_APPEND_FL | FS_IMMUTABLE_FL |
			 FS_NOATIME_FL | FS_DIRSYNC_FL);
	if (flags & S_SYNC)
		nova_flags |= FS_SYNC_FL;
	if (flags & S_APPEND)
		nova_flags |= FS_APPEND_FL;
	if (flags & S_IMMUTABLE)
		nova_flags |= FS_IMMUTABLE_FL;
	if (flags & S_NOATIME)
		nova_flags |= FS_NOATIME_FL;
	if (flags & S_DIRSYNC)
		nova_flags |= FS_DIRSYNC_FL;

	pi->i_flags = cpu_to_le32(nova_flags);
}

static void nova_update_inode(struct inode *inode, struct nova_inode *pi)
{
	nova_memunlock_inode(inode->i_sb, pi);
	pi->i_mode = cpu_to_le16(inode->i_mode);
	pi->i_uid = cpu_to_le32(i_uid_read(inode));
	pi->i_gid = cpu_to_le32(i_gid_read(inode));
	pi->i_links_count = cpu_to_le16(inode->i_nlink);
	pi->i_size = cpu_to_le64(inode->i_size);
	pi->i_blocks = cpu_to_le64(inode->i_blocks);
	pi->i_atime = cpu_to_le32(inode->i_atime.tv_sec);
	pi->i_ctime = cpu_to_le32(inode->i_ctime.tv_sec);
	pi->i_mtime = cpu_to_le32(inode->i_mtime.tv_sec);
	pi->i_generation = cpu_to_le32(inode->i_generation);
	nova_get_inode_flags(inode, pi);

	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
		pi->dev.rdev = cpu_to_le32(inode->i_rdev);

	nova_memlock_inode(inode->i_sb, pi);
}

static int nova_alloc_unused_inode(struct super_block *sb, int cpuid,
	unsigned long *ino)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);
	struct inode_map *inode_map;
	struct nova_range_node *i, *next_i;
	struct rb_node *temp, *next;
	unsigned long next_range_low;
	unsigned long new_ino;
	unsigned long MAX_INODE = 1UL << 31;

	inode_map = &sbi->inode_maps[cpuid];
	i = inode_map->first_inode_range;
	NOVA_ASSERT(i);
	temp = &i->node;
	next = rb_next(temp);

	if (!next) {
		next_i = NULL;
		next_range_low = MAX_INODE;
	} else {
		next_i = container_of(next, struct nova_range_node, node);
		next_range_low = next_i->range_low;
	}

	new_ino = i->range_high + 1;

	if (next_i && new_ino == (next_range_low - 1)) {
		/* Fill the gap completely */
		i->range_high = next_i->range_high;
		rb_erase(&next_i->node, &inode_map->inode_inuse_tree);
		nova_free_inode_node(sb, next_i);
		inode_map->num_range_node_inode--;
	} else if (new_ino < (next_range_low - 1)) {
		/* Aligns to left */
		i->range_high = new_ino;
	} else {
		nova_dbg("%s: ERROR: new ino %lu, next low %lu\n", __func__,
			new_ino, next_range_low);
		return -ENOSPC;
	}

	*ino = new_ino * sbi->cpus + cpuid;
	sbi->s_inodes_used_count++;
	inode_map->allocated++;

	nova_dbg_verbose("Alloc ino %lu\n", *ino);
	return 0;
}

static int nova_free_inuse_inode(struct super_block *sb, unsigned long ino)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);
	struct inode_map *inode_map;
	struct nova_range_node *i = NULL;
	struct nova_range_node *curr_node;
	int found = 0;
	int cpuid = ino % sbi->cpus;
	unsigned long internal_ino = ino / sbi->cpus;
	int ret = 0;

	nova_dbg_verbose("Free inuse ino: %lu\n", ino);
	inode_map = &sbi->inode_maps[cpuid];

	mutex_lock(&inode_map->inode_table_mutex);
	found = nova_search_inodetree(sbi, ino, &i);
	if (!found) {
		nova_dbg("%s ERROR: ino %lu not found\n", __func__, ino);
		mutex_unlock(&inode_map->inode_table_mutex);
		return -EINVAL;
	}

	if ((internal_ino == i->range_low) && (internal_ino == i->range_high)) {
		/* fits entire node */
		rb_erase(&i->node, &inode_map->inode_inuse_tree);
		nova_free_inode_node(sb, i);
		inode_map->num_range_node_inode--;
		goto block_found;
	}
	if ((internal_ino == i->range_low) && (internal_ino < i->range_high)) {
		/* Aligns left */
		i->range_low = internal_ino + 1;
		goto block_found;
	}
	if ((internal_ino > i->range_low) && (internal_ino == i->range_high)) {
		/* Aligns right */
		i->range_high = internal_ino - 1;
		goto block_found;
	}
	if ((internal_ino > i->range_low) && (internal_ino < i->range_high)) {
		/* Aligns somewhere in the middle */
		curr_node = nova_alloc_inode_node(sb);
		NOVA_ASSERT(curr_node);
		if (curr_node == NULL) {
			/* returning without freeing the block */
			goto block_found;
		}
		curr_node->range_low = internal_ino + 1;
		curr_node->range_high = i->range_high;
		i->range_high = internal_ino - 1;
		ret = nova_insert_inodetree(sbi, curr_node, cpuid);
		if (ret) {
			nova_free_inode_node(sb, curr_node);
			goto err;
		}
		inode_map->num_range_node_inode++;
		goto block_found;
	}

err:
	nova_error_mng(sb, "Unable to free inode %lu\n", ino);
	nova_error_mng(sb, "Found inuse block %lu - %lu\n",
				 i->range_low, i->range_high);
	mutex_unlock(&inode_map->inode_table_mutex);
	return ret;

block_found:
	sbi->s_inodes_used_count--;
	inode_map->freed++;
	mutex_unlock(&inode_map->inode_table_mutex);
	return ret;
}

/*
 * NOTE! When we get the inode, we're the only people
 * that have access to it, and as such there are no
 * race conditions we have to worry about. The inode
 * is not on the hash-lists, and it cannot be reached
 * through the filesystem because the directory entry
 * has been deleted earlier.
 */
static int nova_free_inode(struct inode *inode,
	struct nova_inode_info_header *sih)
{
	struct super_block *sb = inode->i_sb;
	struct nova_inode *pi;
	int err = 0;
	timing_t free_time;

	NOVA_START_TIMING(free_inode_t, free_time);

	pi = nova_get_inode(sb, inode);

	if (pi->valid) {
		nova_dbg("%s: inode %lu still valid\n",
				__func__, inode->i_ino);
		pi->valid = 0;
	}

	if (pi->nova_ino != inode->i_ino) {
		nova_err(sb, "%s: inode %lu ino does not match: %llu\n",
				__func__, inode->i_ino, pi->nova_ino);
		nova_dbg("inode size %llu, pi addr 0x%lx, pi head 0x%llx, "
				"tail 0x%llx, mode %u\n",
				inode->i_size, sih->pi_addr, pi->log_head,
				pi->log_tail, pi->i_mode);
		nova_dbg("sih: ino %lu, inode size %lu, mode %u, "
				"inode mode %u\n", sih->ino, sih->i_size,
				sih->i_mode, inode->i_mode);
		nova_print_inode_log(sb, inode);
	}

	nova_free_inode_log(sb, pi);
	pi->i_blocks = 0;

	sih->log_pages = 0;
	sih->i_mode = 0;
	sih->pi_addr = 0;
	sih->i_size = 0;

	err = nova_free_inuse_inode(sb, pi->nova_ino);

	NOVA_END_TIMING(free_inode_t, free_time);
	return err;
}

struct inode *nova_iget(struct super_block *sb, unsigned long ino)
{
	struct nova_inode_info *si;
	struct inode *inode;
	u64 pi_addr;
	int err;

	inode = iget_locked(sb, ino);
	if (unlikely(!inode))
		return ERR_PTR(-ENOMEM);
	if (!(inode->i_state & I_NEW))
		return inode;

	si = NOVA_I(inode);

	nova_dbgv("%s: inode %lu\n", __func__, ino);

	if (ino == NOVA_ROOT_INO) {
		pi_addr = NOVA_ROOT_INO_START;
	} else {
		err = nova_get_inode_address(sb, ino, &pi_addr, 0);
		if (err) {
			nova_dbg("%s: get inode %lu address failed %d\n",
					__func__, ino, err);
			goto fail;
		}
	}

	if (pi_addr == 0) {
		err = -EACCES;
		goto fail;
	}

	err = nova_rebuild_inode(sb, si, pi_addr);
	if (err)
		goto fail;

	err = nova_read_inode(sb, inode, pi_addr);
	if (unlikely(err))
		goto fail;
	inode->i_ino = ino;

	unlock_new_inode(inode);
	return inode;
fail:
	iget_failed(inode);
	return ERR_PTR(err);
}

unsigned long nova_get_last_blocknr(struct super_block *sb,
	struct nova_inode_info_header *sih)
{
	struct nova_inode *pi;
	unsigned long last_blocknr;
	unsigned int btype;
	unsigned int data_bits;

	pi = nova_get_block(sb, sih->pi_addr);
	btype = pi->i_blk_type;
	data_bits = blk_type_to_shift[btype];

	if (sih->i_size == 0)
		last_blocknr = 0;
	else
		last_blocknr = (sih->i_size - 1) >> data_bits;

	return last_blocknr;
}

void nova_evict_inode(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct nova_inode *pi = nova_get_inode(sb, inode);
	struct nova_inode_info *si = NOVA_I(inode);
	struct nova_inode_info_header *sih = &si->header;
	unsigned long last_blocknr;
	timing_t evict_time;
	int err = 0;
	int freed = 0;
	int destroy = 0;

	if (!sih) {
		nova_err(sb, "%s: ino %lu sih is NULL!\n",
				__func__, inode->i_ino);
		NOVA_ASSERT(0);
		goto out;
	}

	NOVA_START_TIMING(evict_inode_t, evict_time);
	nova_dbg_verbose("%s: %lu\n", __func__, inode->i_ino);
	if (!inode->i_nlink && !is_bad_inode(inode)) {
		if (IS_APPEND(inode) || IS_IMMUTABLE(inode))
			goto out;

		destroy = 1;
		/* We need the log to free the blocks from the b-tree */
		switch (inode->i_mode & S_IFMT) {
		case S_IFREG:
			last_blocknr = nova_get_last_blocknr(sb, sih);
			nova_dbgv("%s: file ino %lu\n", __func__, inode->i_ino);
			freed = nova_delete_file_tree(sb, sih, 0,
						last_blocknr, true, true);
			break;
		case S_IFDIR:
			nova_dbgv("%s: dir ino %lu\n", __func__, inode->i_ino);
			nova_delete_dir_tree(sb, sih);
			break;
		case S_IFLNK:
			/* Log will be freed later */
			nova_dbgv("%s: symlink ino %lu\n",
					__func__, inode->i_ino);
			freed = nova_delete_file_tree(sb, sih, 0, 0,
							true, true);
			break;
		default:
			nova_dbgv("%s: special ino %lu\n",
					__func__, inode->i_ino);
			break;
		}

		nova_dbg_verbose("%s: Freed %d\n", __func__, freed);
		/* Then we can free the inode */
		err = nova_free_inode(inode, sih);
		if (err) {
			nova_err(sb, "%s: free inode %lu failed\n",
					__func__, inode->i_ino);
			goto out;
		}
		pi = NULL; /* we no longer own the nova_inode */

		inode->i_mtime = inode->i_ctime = CURRENT_TIME_SEC;
		inode->i_size = 0;
	}
out:
	if (destroy == 0)
		nova_free_dram_resource(sb, sih);

	/* TODO: Since we don't use page-cache, do we really need the following
	 * call? */
	truncate_inode_pages(&inode->i_data, 0);

	clear_inode(inode);
	NOVA_END_TIMING(evict_inode_t, evict_time);
}

/* Returns 0 on failure */
u64 nova_new_nova_inode(struct super_block *sb, u64 *pi_addr)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);
	struct inode_map *inode_map;
	unsigned long free_ino = 0;
	int map_id;
	u64 ino = 0;
	int ret;
	timing_t new_inode_time;

	NOVA_START_TIMING(new_nova_inode_t, new_inode_time);
	map_id = sbi->map_id;
	sbi->map_id = (sbi->map_id + 1) % sbi->cpus;

	inode_map = &sbi->inode_maps[map_id];

	mutex_lock(&inode_map->inode_table_mutex);
	ret = nova_alloc_unused_inode(sb, map_id, &free_ino);
	if (ret) {
		nova_dbg("%s: alloc inode number failed %d\n", __func__, ret);
		mutex_unlock(&inode_map->inode_table_mutex);
		return 0;
	}

	ret = nova_get_inode_address(sb, free_ino, pi_addr, 1);
	if (ret) {
		nova_dbg("%s: get inode address failed %d\n", __func__, ret);
		mutex_unlock(&inode_map->inode_table_mutex);
		return 0;
	}

	mutex_unlock(&inode_map->inode_table_mutex);

	ino = free_ino;

	NOVA_END_TIMING(new_nova_inode_t, new_inode_time);
	return ino;
}

struct inode *nova_new_vfs_inode(enum nova_new_inode_type type,
	struct inode *dir, u64 pi_addr, u64 ino, umode_t mode,
	size_t size, dev_t rdev, const struct qstr *qstr)
{
	struct super_block *sb;
	struct nova_sb_info *sbi;
	struct inode *inode;
	struct nova_inode *diri = NULL;
	struct nova_inode_info *si;
	struct nova_inode_info_header *sih = NULL;
	struct nova_inode *pi;
	int errval;
	timing_t new_inode_time;

	NOVA_START_TIMING(new_vfs_inode_t, new_inode_time);
	sb = dir->i_sb;
	sbi = (struct nova_sb_info *)sb->s_fs_info;
	inode = new_inode(sb);
	if (!inode) {
		errval = -ENOMEM;
		goto fail2;
	}

	inode_init_owner(inode, dir, mode);
	inode->i_blocks = inode->i_size = 0;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;

	inode->i_generation = atomic_add_return(1, &sbi->next_generation);
	inode->i_size = size;

	diri = nova_get_inode(sb, dir);
	if (!diri) {
		errval = -EACCES;
		goto fail1;
	}

	pi = (struct nova_inode *)nova_get_block(sb, pi_addr);
	nova_dbg_verbose("%s: allocating inode %llu @ 0x%llx\n",
					__func__, ino, pi_addr);

	/* chosen inode is in ino */
	inode->i_ino = ino;

	switch (type) {
		case TYPE_CREATE:
			inode->i_op = &nova_file_inode_operations;
			inode->i_mapping->a_ops = &nova_aops_dax;
			inode->i_fop = &nova_dax_file_operations;
			break;
		case TYPE_MKNOD:
			init_special_inode(inode, mode, rdev);
			inode->i_op = &nova_special_inode_operations;
			break;
		case TYPE_SYMLINK:
			inode->i_op = &nova_symlink_inode_operations;
			inode->i_mapping->a_ops = &nova_aops_dax;
			break;
		case TYPE_MKDIR:
			inode->i_op = &nova_dir_inode_operations;
			inode->i_fop = &nova_dir_operations;
			inode->i_mapping->a_ops = &nova_aops_dax;
			set_nlink(inode, 2);
			break;
		default:
			nova_dbg("Unknown new inode type %d\n", type);
			break;
	}

	/*
	 * Pi is part of the dir log so no transaction is needed,
	 * but we need to flush to NVMM.
	 */
	nova_memunlock_inode(sb, pi);
	pi->i_blk_type = NOVA_DEFAULT_BLOCK_TYPE;
	pi->i_flags = nova_mask_flags(mode, diri->i_flags);
	pi->log_head = 0;
	pi->log_tail = 0;
	pi->nova_ino = ino;
	nova_memlock_inode(sb, pi);

	si = NOVA_I(inode);
	sih = &si->header;
	nova_init_header(sb, sih, inode->i_mode);
	sih->pi_addr = pi_addr;
	sih->ino = ino;

	nova_update_inode(inode, pi);

	nova_set_inode_flags(inode, pi, le32_to_cpu(pi->i_flags));

	if (insert_inode_locked(inode) < 0) {
		nova_err(sb, "nova_new_inode failed ino %lx\n", inode->i_ino);
		errval = -EINVAL;
		goto fail1;
	}

	nova_flush_buffer(&pi, NOVA_INODE_SIZE, 0);
	NOVA_END_TIMING(new_vfs_inode_t, new_inode_time);
	return inode;
fail1:
	make_bad_inode(inode);
	iput(inode);
fail2:
	NOVA_END_TIMING(new_vfs_inode_t, new_inode_time);
	return ERR_PTR(errval);
}

int nova_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	/* write_inode should never be called because we always keep our inodes
	 * clean. So let us know if write_inode ever gets called. */
//	BUG();
	return 0;
}

/*
 * dirty_inode() is called from mark_inode_dirty_sync()
 * usually dirty_inode should not be called because NOVA always keeps its inodes
 * clean. Only exception is touch_atime which calls dirty_inode to update the
 * i_atime field.
 */
void nova_dirty_inode(struct inode *inode, int flags)
{
	struct super_block *sb = inode->i_sb;
	struct nova_inode *pi = nova_get_inode(sb, inode);

	/* only i_atime should have changed if at all.
	 * we can do in-place atomic update */
	nova_memunlock_inode(sb, pi);
	pi->i_atime = cpu_to_le32(inode->i_atime.tv_sec);
	nova_memlock_inode(sb, pi);
	/* Relax atime persistency */
	nova_flush_buffer(&pi->i_atime, sizeof(pi->i_atime), 0);
}

/*
 * Zero the tail page. Used in resize request
 * to avoid to keep data in case the file grows again.
 */
static void nova_clear_last_page_tail(struct super_block *sb,
	struct inode *inode, loff_t newsize)
{
	struct nova_inode_info *si = NOVA_I(inode);
	struct nova_inode_info_header *sih = &si->header;
	unsigned long offset = newsize & (sb->s_blocksize - 1);
	unsigned long pgoff, length;
	u64 nvmm;
	char *nvmm_addr;

	if (offset == 0 || newsize > inode->i_size)
		return;

	length = sb->s_blocksize - offset;
	pgoff = newsize >> sb->s_blocksize_bits;

	nvmm = nova_find_nvmm_block(sb, si, NULL, pgoff);
	if (nvmm == 0)
		return;

	nvmm_addr = (char *)nova_get_block(sb, nvmm);
	memset(nvmm_addr + offset, 0, length);
	nova_flush_buffer(nvmm_addr + offset, length, 0);

	/* Clear mmap page */
	if (sih->mmap_pages && pgoff <= sih->high_dirty &&
			pgoff >= sih->low_dirty) {
		nvmm = (unsigned long)radix_tree_lookup(&sih->cache_tree,
							pgoff);
		if (nvmm) {
			nvmm_addr = nova_get_block(sb, nvmm);
			memset(nvmm_addr + offset, 0, length);
		}
	}
}

static void nova_setsize(struct inode *inode, loff_t oldsize, loff_t newsize)
{
	struct super_block *sb = inode->i_sb;
	struct nova_inode_info *si = NOVA_I(inode);
	struct nova_inode_info_header *sih = &si->header;

	/* We only support truncate regular file */
	if (!(S_ISREG(inode->i_mode))) {
		nova_err(inode->i_sb, "%s:wrong file mode %x\n", inode->i_mode);
		return;
	}

	inode_dio_wait(inode);

	nova_dbgv("%s: inode %lu, old size %llu, new size %llu\n",
		__func__, inode->i_ino, oldsize, newsize);

	if (newsize != oldsize) {
		nova_clear_last_page_tail(sb, inode, newsize);
		i_size_write(inode, newsize);
		sih->i_size = newsize;
	}

	/* FIXME: we should make sure that there is nobody reading the inode
	 * before truncating it. Also we need to munmap the truncated range
	 * from application address space, if mmapped. */
	/* synchronize_rcu(); */

	/* FIXME: Do we need to clear truncated DAX pages? */
//	dax_truncate_page(inode, newsize, nova_dax_get_block);

	truncate_pagecache(inode, newsize);
	nova_truncate_file_blocks(inode, newsize, oldsize);
}

int nova_getattr(struct vfsmount *mnt, struct dentry *dentry,
		         struct kstat *stat)
{
	struct inode *inode;

	inode = dentry->d_inode;
	generic_fillattr(inode, stat);
	/* stat->blocks should be the number of 512B blocks */
	stat->blocks = (inode->i_blocks << inode->i_sb->s_blocksize_bits) >> 9;
	return 0;
}

static void nova_update_setattr_entry(struct inode *inode,
	struct nova_setattr_logentry *entry, struct iattr *attr)
{
	unsigned int ia_valid = attr->ia_valid, attr_mask;

	/* These files are in the lowest byte */
	attr_mask = ATTR_MODE | ATTR_UID | ATTR_GID | ATTR_SIZE |
			ATTR_ATIME | ATTR_MTIME | ATTR_CTIME;

	entry->entry_type	= SET_ATTR;
	entry->attr	= ia_valid & attr_mask;
	entry->mode	= cpu_to_le16(inode->i_mode);
	entry->uid	= cpu_to_le32(i_uid_read(inode));
	entry->gid	= cpu_to_le32(i_gid_read(inode));
	entry->atime	= cpu_to_le32(inode->i_atime.tv_sec);
	entry->ctime	= cpu_to_le32(inode->i_ctime.tv_sec);
	entry->mtime	= cpu_to_le32(inode->i_mtime.tv_sec);

	if (ia_valid & ATTR_SIZE)
		entry->size = cpu_to_le64(attr->ia_size);
	else
		entry->size = cpu_to_le64(inode->i_size);

	nova_flush_buffer(entry, sizeof(struct nova_setattr_logentry), 0);
}

void nova_apply_setattr_entry(struct super_block *sb, struct nova_inode *pi,
	struct nova_inode_info_header *sih,
	struct nova_setattr_logentry *entry)
{
	unsigned int data_bits = blk_type_to_shift[pi->i_blk_type];
	unsigned long first_blocknr, last_blocknr;
	loff_t start, end;
	int freed = 0;

	if (entry->entry_type != SET_ATTR)
		BUG();

	pi->i_mode	= entry->mode;
	pi->i_uid	= entry->uid;
	pi->i_gid	= entry->gid;
	pi->i_atime	= entry->atime;
	pi->i_ctime	= entry->ctime;
	pi->i_mtime	= entry->mtime;

	if (pi->i_size > entry->size && S_ISREG(pi->i_mode)) {
		start = entry->size;
		end = pi->i_size;

		first_blocknr = (start + (1UL << data_bits) - 1) >> data_bits;

		if (end > 0)
			last_blocknr = (end - 1) >> data_bits;
		else
			last_blocknr = 0;

		if (first_blocknr > last_blocknr)
			goto out;

		freed = nova_delete_file_tree(sb, sih, first_blocknr,
						last_blocknr, 0, 0);
	}
out:
	pi->i_size	= entry->size;
	sih->i_size = le64_to_cpu(pi->i_size);
	/* Do not flush now */
}

/* Returns new tail after append */
static u64 nova_append_setattr_entry(struct super_block *sb,
	struct nova_inode *pi, struct inode *inode, struct iattr *attr,
	u64 tail)
{
	struct nova_inode_info *si = NOVA_I(inode);
	struct nova_inode_info_header *sih = &si->header;
	struct nova_setattr_logentry *entry;
	u64 curr_p, new_tail = 0;
	int extended = 0;
	size_t size = sizeof(struct nova_setattr_logentry);
	timing_t append_time;

	NOVA_START_TIMING(append_setattr_t, append_time);
	nova_dbg_verbose("%s: inode %lu attr change\n",
				__func__, inode->i_ino);

	curr_p = nova_get_append_head(sb, pi, sih, tail, size, &extended);
	if (curr_p == 0)
		BUG();

	entry = (struct nova_setattr_logentry *)nova_get_block(sb, curr_p);
	/* inode is already updated with attr */
	nova_update_setattr_entry(inode, entry, attr);
	new_tail = curr_p + size;
	sih->last_setattr = curr_p;

	NOVA_END_TIMING(append_setattr_t, append_time);
	return new_tail;
}

int nova_notify_change(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	struct nova_inode *pi = nova_get_inode(sb, inode);
	struct nova_inode_info *si = NOVA_I(inode);
	struct nova_inode_info_header *sih = &si->header;
	int ret;
	unsigned int ia_valid = attr->ia_valid, attr_mask;
	loff_t oldsize = inode->i_size;
	u64 new_tail;
	timing_t setattr_time;

	NOVA_START_TIMING(setattr_t, setattr_time);
	if (!pi)
		return -EACCES;

	ret = inode_change_ok(inode, attr);
	if (ret)
		return ret;

	/* Update inode with attr except for size */
	setattr_copy(inode, attr);

	if (ia_valid & ATTR_MODE)
		sih->i_mode = inode->i_mode;

	attr_mask = ATTR_MODE | ATTR_UID | ATTR_GID | ATTR_SIZE | ATTR_ATIME
			| ATTR_MTIME | ATTR_CTIME;

	ia_valid = ia_valid & attr_mask;

	if (ia_valid == 0)
		return ret;

	/* We are holding i_mutex so OK to append the log */
	new_tail = nova_append_setattr_entry(sb, pi, inode, attr, 0);

	nova_update_tail(pi, new_tail);

	/* Only after log entry is committed, we can truncate size */
	if ((ia_valid & ATTR_SIZE) && (attr->ia_size != oldsize ||
			pi->i_flags & cpu_to_le32(NOVA_EOFBLOCKS_FL))) {
//		nova_set_blocksize_hint(sb, inode, pi, attr->ia_size);

		/* now we can freely truncate the inode */
		nova_setsize(inode, oldsize, attr->ia_size);
	}

	NOVA_END_TIMING(setattr_t, setattr_time);
	return ret;
}

void nova_set_inode_flags(struct inode *inode, struct nova_inode *pi,
	unsigned int flags)
{
	inode->i_flags &=
		~(S_SYNC | S_APPEND | S_IMMUTABLE | S_NOATIME | S_DIRSYNC);
	if (flags & FS_SYNC_FL)
		inode->i_flags |= S_SYNC;
	if (flags & FS_APPEND_FL)
		inode->i_flags |= S_APPEND;
	if (flags & FS_IMMUTABLE_FL)
		inode->i_flags |= S_IMMUTABLE;
	if (flags & FS_NOATIME_FL)
		inode->i_flags |= S_NOATIME;
	if (flags & FS_DIRSYNC_FL)
		inode->i_flags |= S_DIRSYNC;
	if (!pi->i_xattr)
		inode_has_no_xattr(inode);
	inode->i_flags |= S_DAX;
}

#if 0
static ssize_t nova_direct_IO(struct kiocb *iocb,
	struct iov_iter *iter, loff_t offset)
{
	struct file *filp = iocb->ki_filp;
	loff_t end = offset;
	size_t count = iov_iter_count(iter);
	ssize_t ret = -EINVAL;
	ssize_t written = 0;
	unsigned long seg;
	unsigned long nr_segs = iter->nr_segs;
	const struct iovec *iv = iter->iov;
	timing_t dio_time;

	NOVA_START_TIMING(direct_IO_t, dio_time);
	end = offset + count;

	nova_dbgv("%s: %lu segs\n", __func__, nr_segs);
	iv = iter->iov;
	for (seg = 0; seg < nr_segs; seg++) {
		if (iov_iter_rw(iter) == READ) {
			ret = nova_dax_file_read(filp, iv->iov_base,
					iv->iov_len, &offset);
		} else if (iov_iter_rw(iter) == WRITE) {
			ret = nova_cow_file_write(filp, iv->iov_base,
					iv->iov_len, &offset, false);
		}
		if (ret < 0)
			goto err;

		if (iter->count > iv->iov_len)
			iter->count -= iv->iov_len;
		else
			iter->count = 0;

		written += ret;
		iter->nr_segs--;
		iv++;
	}
	if (offset != end)
		printk(KERN_ERR "nova: direct_IO: end = %lld"
			"but offset = %lld\n", end, offset);
	ret = written;
err:
	NOVA_END_TIMING(direct_IO_t, dio_time);
	return ret;
}
#endif

static ssize_t nova_direct_IO(struct kiocb *iocb, struct iov_iter *iter)
{
	struct file *filp = iocb->ki_filp;
	struct address_space *mapping = filp->f_mapping;
	struct inode *inode = mapping->host;
	ssize_t ret;
	timing_t dio_time;

	NOVA_START_TIMING(direct_IO_t, dio_time);

	ret = dax_do_io(iocb, inode, iter, nova_dax_get_block,
				NULL, DIO_LOCKING);
	NOVA_END_TIMING(direct_IO_t, dio_time);
	return ret;
}

static int nova_coalesce_log_pages(struct super_block *sb,
	unsigned long prev_blocknr, unsigned long first_blocknr,
	unsigned long num_pages)
{
	unsigned long next_blocknr;
	u64 curr_block, next_page;
	struct nova_inode_log_page *curr_page;
	int i;

	if (prev_blocknr) {
		/* Link prev block and newly allocated head block */
		curr_block = nova_get_block_off(sb, prev_blocknr,
						NOVA_BLOCK_TYPE_4K);
		curr_page = (struct nova_inode_log_page *)
				nova_get_block(sb, curr_block);
		next_page = nova_get_block_off(sb, first_blocknr,
				NOVA_BLOCK_TYPE_4K);
		nova_set_next_page_address(sb, curr_page, next_page, 0);
	}

	next_blocknr = first_blocknr + 1;
	curr_block = nova_get_block_off(sb, first_blocknr,
						NOVA_BLOCK_TYPE_4K);
	curr_page = (struct nova_inode_log_page *)
				nova_get_block(sb, curr_block);
	for (i = 0; i < num_pages - 1; i++) {
		next_page = nova_get_block_off(sb, next_blocknr,
				NOVA_BLOCK_TYPE_4K);
		nova_set_next_page_address(sb, curr_page, next_page, 0);
		curr_page++;
		next_blocknr++;
	}

	/* Last page */
	nova_set_next_page_address(sb, curr_page, 0, 1);
	return 0;
}

/* Log block resides in NVMM */
int nova_allocate_inode_log_pages(struct super_block *sb,
	struct nova_inode *pi, unsigned long num_pages,
	u64 *new_block)
{
	unsigned long new_inode_blocknr;
	unsigned long first_blocknr;
	unsigned long prev_blocknr;
	int allocated;
	int ret_pages = 0;

	allocated = nova_new_log_blocks(sb, pi, &new_inode_blocknr,
					num_pages, 0);

	if (allocated <= 0) {
		nova_err(sb, "ERROR: no inode log page available: %d %d\n",
			num_pages, allocated);
		return allocated;
	}
	ret_pages += allocated;
	num_pages -= allocated;
	nova_dbg_verbose("Pi %llu: Alloc %d log blocks @ 0x%lx\n",
			pi->nova_ino, allocated, new_inode_blocknr);

	/* Coalesce the pages */
	nova_coalesce_log_pages(sb, 0, new_inode_blocknr, allocated);
	first_blocknr = new_inode_blocknr;
	prev_blocknr = new_inode_blocknr + allocated - 1;

	/* Allocate remaining pages */
	while (num_pages) {
		allocated = nova_new_log_blocks(sb, pi,
					&new_inode_blocknr, num_pages, 0);

		nova_dbg_verbose("Alloc %d log blocks @ 0x%lx\n",
					allocated, new_inode_blocknr);
		if (allocated <= 0) {
			nova_dbg("%s: no inode log page available: "
				"%lu %d\n", __func__, num_pages, allocated);
			/* Return whatever we have */
			break;
		}
		ret_pages += allocated;
		num_pages -= allocated;
		nova_coalesce_log_pages(sb, prev_blocknr, new_inode_blocknr,
						allocated);
		prev_blocknr = new_inode_blocknr + allocated - 1;
	}

	*new_block = nova_get_block_off(sb, first_blocknr,
						NOVA_BLOCK_TYPE_4K);

	return ret_pages;
}

static bool curr_log_entry_invalid(struct super_block *sb,
	struct nova_inode *pi, struct nova_inode_info_header *sih,
	u64 curr_p, size_t *length)
{
	struct nova_setattr_logentry *setattr_entry;
	struct nova_file_write_entry *entry;
	struct nova_dentry *dentry;
	void *addr;
	u8 type;
	bool ret = true;

	addr = (void *)nova_get_block(sb, curr_p);
	type = nova_get_entry_type(addr);
	switch (type) {
		case SET_ATTR:
			if (sih->last_setattr == curr_p)
				ret = false;
			/* Do not invalidate setsize entries */
			setattr_entry = (struct nova_setattr_logentry *)addr;
			if (setattr_entry->attr & ATTR_SIZE)
				ret = false;
			*length = sizeof(struct nova_setattr_logentry);
			break;
		case LINK_CHANGE:
			if (sih->last_link_change == curr_p)
				ret = false;
			*length = sizeof(struct nova_link_change_entry);
			break;
		case FILE_WRITE:
			entry = (struct nova_file_write_entry *)addr;
			if (entry->num_pages != entry->invalid_pages)
				ret = false;
			*length = sizeof(struct nova_file_write_entry);
			break;
		case DIR_LOG:
			dentry = (struct nova_dentry *)addr;
			if (dentry->ino && dentry->invalid == 0)
				ret = false;
			*length = le16_to_cpu(dentry->de_len);
			break;
		case NEXT_PAGE:
			/* No more entries in this page */
			*length = PAGE_SIZE - ENTRY_LOC(curr_p);;
			break;
		default:
			nova_dbg("%s: unknown type %d, 0x%llx\n",
						__func__, type, curr_p);
			NOVA_ASSERT(0);
			*length = PAGE_SIZE - ENTRY_LOC(curr_p);;
			break;
	}

	return ret;
}

static bool curr_page_invalid(struct super_block *sb,
	struct nova_inode *pi, struct nova_inode_info_header *sih,
	u64 page_head)
{
	u64 curr_p = page_head;
	bool ret = true;
	size_t length;
	timing_t check_time;

	NOVA_START_TIMING(check_invalid_t, check_time);
	while (curr_p < page_head + LAST_ENTRY) {
		if (curr_p == 0) {
			nova_err(sb, "File inode %lu log is NULL!\n",
					sih->ino);
			BUG();
		}

		length = 0;
		if (!curr_log_entry_invalid(sb, pi, sih, curr_p, &length)) {
			sih->valid_bytes += length;
			ret = false;
		}

		curr_p += length;
	}

	NOVA_END_TIMING(check_invalid_t, check_time);
	return ret;
}

static void nova_set_next_page_flag(struct super_block *sb, u64 curr_p)
{
	void *p;

	if (ENTRY_LOC(curr_p) >= LAST_ENTRY)
		return;

	p = nova_get_block(sb, curr_p);
	nova_set_entry_type(p, NEXT_PAGE);
	nova_flush_buffer(p, CACHELINE_SIZE, 1);
}

static void free_curr_page(struct super_block *sb, struct nova_inode *pi,
	struct nova_inode_log_page *curr_page,
	struct nova_inode_log_page *last_page, u64 curr_head)
{
	unsigned short btype = pi->i_blk_type;

	nova_set_next_page_address(sb, last_page,
			curr_page->page_tail.next_page, 1);
	nova_free_log_blocks(sb, pi,
			nova_get_blocknr(sb, curr_head, btype), 1);
}

int nova_gc_assign_file_entry(struct super_block *sb,
	struct nova_inode_info_header *sih,
	struct nova_file_write_entry *old_entry,
	struct nova_file_write_entry *new_entry)
{
	struct nova_file_write_entry *temp;
	void **pentry;
	unsigned long start_pgoff = old_entry->pgoff;
	unsigned int num = old_entry->num_pages;
	unsigned long curr_pgoff;
	int i;
	int ret = 0;

	for (i = 0; i < num; i++) {
		curr_pgoff = start_pgoff + i;

		pentry = radix_tree_lookup_slot(&sih->tree, curr_pgoff);
		if (pentry) {
			temp = radix_tree_deref_slot(pentry);
			if (temp == old_entry)
				radix_tree_replace_slot(pentry, new_entry);
		}
	}

	return ret;
}

static int nova_gc_assign_dentry(struct super_block *sb,
	struct nova_inode_info_header *sih, struct nova_dentry *old_dentry,
	struct nova_dentry *new_dentry)
{
	struct nova_dentry *temp;
	void **pentry;
	unsigned long hash;
	int ret = 0;

	hash = BKDRHash(old_dentry->name, old_dentry->name_len);
	nova_dbgv("%s: assign %s hash %lu\n", __func__,
			old_dentry->name, hash);

	/* FIXME: hash collision ignored here */
	pentry = radix_tree_lookup_slot(&sih->tree, hash);
	if (pentry) {
		temp = radix_tree_deref_slot(pentry);
		if (temp == old_dentry)
			radix_tree_replace_slot(pentry, new_dentry);
	}

	return ret;
}

static int nova_gc_assign_new_entry(struct super_block *sb,
	struct nova_inode *pi, struct nova_inode_info_header *sih,
	u64 curr_p, u64 new_curr)
{
	struct nova_file_write_entry *old_entry, *new_entry;
	struct nova_dentry *old_dentry, *new_dentry;
	void *addr, *new_addr;
	u8 type;
	int ret = 0;

	addr = (void *)nova_get_block(sb, curr_p);
	type = nova_get_entry_type(addr);
	switch (type) {
		case SET_ATTR:
			sih->last_setattr = new_curr;
			break;
		case LINK_CHANGE:
			sih->last_link_change = new_curr;
			break;
		case FILE_WRITE:
			new_addr = (void *)nova_get_block(sb, new_curr);
			old_entry = (struct nova_file_write_entry *)addr;
			new_entry = (struct nova_file_write_entry *)new_addr;
			ret = nova_gc_assign_file_entry(sb, sih, old_entry,
							new_entry);
			break;
		case DIR_LOG:
			new_addr = (void *)nova_get_block(sb, new_curr);
			old_dentry = (struct nova_dentry *)addr;
			new_dentry = (struct nova_dentry *)new_addr;
			ret = nova_gc_assign_dentry(sb, sih, old_dentry,
							new_dentry);
			break;
		default:
			nova_dbg("%s: unknown type %d, 0x%llx\n",
						__func__, type, curr_p);
			NOVA_ASSERT(0);
			break;
	}

	return ret;
}

/* Copy alive log entries to the new log and atomically replace the old log */
static int nova_inode_log_thorough_gc(struct super_block *sb,
	struct nova_inode *pi, struct nova_inode_info_header *sih,
	unsigned long blocks, unsigned long checked_pages)
{
	struct nova_inode_log_page *curr_page = NULL;
	size_t length;
	u64 ino = pi->nova_ino;
	u64 curr_p, new_curr;
	u64 old_curr_p;
	u64 tail_block;
	u64 old_head;
	u64 new_head = 0;
	u64 next;
	int allocated;
	int extended = 0;
	int ret;
	timing_t gc_time;

	NOVA_START_TIMING(thorough_gc_t, gc_time);

	curr_p = pi->log_head;
	old_curr_p = curr_p;
	old_head = pi->log_head;
	nova_dbg_verbose("Log head 0x%llx, tail 0x%llx\n",
				curr_p, pi->log_tail);
	if (curr_p == 0 && pi->log_tail == 0)
		goto out;

	if (curr_p >> PAGE_SHIFT == pi->log_tail >> PAGE_SHIFT)
		goto out;

	allocated = nova_allocate_inode_log_pages(sb, pi, blocks,
					&new_head);
	if (allocated != blocks) {
		nova_err(sb, "%s: ERROR: no inode log page "
					"available\n", __func__);
		goto out;
	}

	new_curr = new_head;
	while (curr_p != pi->log_tail) {
		old_curr_p = curr_p;
		if (goto_next_page(sb, curr_p))
			curr_p = next_log_page(sb, curr_p);

		if (curr_p >> PAGE_SHIFT == pi->log_tail >> PAGE_SHIFT) {
			/* Don't recycle tail page */
			break;
		}

		if (curr_p == 0) {
			nova_err(sb, "File inode %llu log is NULL!\n", ino);
			BUG();
		}

		length = 0;
		ret = curr_log_entry_invalid(sb, pi, sih, curr_p, &length);
		if (!ret) {
			extended = 0;
			new_curr = nova_get_append_head(sb, pi, NULL,
						new_curr, length, &extended);
			if (extended)
				blocks++;
			/* Copy entry to the new log */
			memcpy_to_pmem_nocache(nova_get_block(sb, new_curr),
				nova_get_block(sb, curr_p), length);
			nova_gc_assign_new_entry(sb, pi, sih, curr_p, new_curr);
			new_curr += length;
		}

		curr_p += length;
	}

	/* Step 1: Link new log to the tail block */
	tail_block = BLOCK_OFF(pi->log_tail);
	curr_page = (struct nova_inode_log_page *)nova_get_block(sb,
							BLOCK_OFF(new_curr));
	next = curr_page->page_tail.next_page;
	if (next)
		nova_free_contiguous_log_blocks(sb, pi, next);
	nova_set_next_page_flag(sb, new_curr);
	nova_set_next_page_address(sb, curr_page, tail_block, 0);
	nova_flush_buffer(curr_page, PAGE_SIZE, 0);

	/* Step 2: Atomically switch to the new log */
	pi->log_head = new_head;
	nova_flush_buffer(pi, sizeof(struct nova_inode), 1);

	/* Step 3: Unlink the old log */
	curr_page = (struct nova_inode_log_page *)nova_get_block(sb,
							BLOCK_OFF(old_curr_p));
	next = curr_page->page_tail.next_page;
	if (next != tail_block) {
		nova_err(sb, "Old log error: old curr_p 0x%lx, next 0x%lx ",
			"curr_p 0x%lx, tail block 0x%lx\n", old_curr_p,
			next, curr_p, tail_block);
		BUG();
	}
	nova_set_next_page_address(sb, curr_page, 0, 1);

	/* Step 4: Free the old log */
	nova_free_contiguous_log_blocks(sb, pi, old_head);

	sih->log_pages = sih->log_pages + blocks - checked_pages;
	NOVA_STATS_ADD(thorough_gc_pages, checked_pages - blocks);
	NOVA_STATS_ADD(thorough_checked_pages, checked_pages);
out:
	NOVA_END_TIMING(thorough_gc_t, gc_time);
	return 0;
}

static int need_thorough_gc(struct super_block *sb,
	struct nova_inode_info_header *sih, unsigned long blocks,
	unsigned long checked_pages)
{
	if (blocks && blocks * 2 < checked_pages)
		return 1;

	return 0;
}

static int nova_inode_log_fast_gc(struct super_block *sb,
	struct nova_inode *pi, struct nova_inode_info_header *sih,
	u64 curr_tail, u64 new_block, int num_pages)
{
	u64 curr, next, possible_head = 0;
	int found_head = 0;
	struct nova_inode_log_page *last_page = NULL;
	struct nova_inode_log_page *curr_page = NULL;
	int first_need_free = 0;
	unsigned short btype = pi->i_blk_type;
	unsigned long blocks;
	unsigned long checked_pages = 0;
	int freed_pages = 0;
	timing_t gc_time;

	NOVA_START_TIMING(fast_gc_t, gc_time);
	curr = pi->log_head;
	sih->valid_bytes = 0;

	nova_dbg_verbose("%s: log head 0x%llx, tail 0x%llx\n",
				__func__, curr, curr_tail);
	while (1) {
		if (curr >> PAGE_SHIFT == pi->log_tail >> PAGE_SHIFT) {
			/* Don't recycle tail page */
			if (found_head == 0)
				possible_head = cpu_to_le64(curr);
			break;
		}

		curr_page = (struct nova_inode_log_page *)
					nova_get_block(sb, curr);
		next = curr_page->page_tail.next_page;
		nova_dbg_verbose("curr 0x%llx, next 0x%llx\n", curr, next);
		if (curr_page_invalid(sb, pi, sih, curr)) {
			nova_dbg_verbose("curr page %p invalid\n", curr_page);
			if (curr == pi->log_head) {
				/* Free first page later */
				first_need_free = 1;
				last_page = curr_page;
			} else {
				nova_dbg_verbose("Free log block 0x%llx\n",
						curr >> PAGE_SHIFT);
				free_curr_page(sb, pi, curr_page, last_page,
						curr);
			}
			NOVA_STATS_ADD(fast_gc_pages, 1);
			freed_pages++;
		} else {
			if (found_head == 0) {
				possible_head = cpu_to_le64(curr);
				found_head = 1;
			}
			last_page = curr_page;
		}

		curr = next;
		checked_pages++;
		if (curr == 0)
			break;
	}

	NOVA_STATS_ADD(fast_checked_pages, checked_pages);
	checked_pages -= freed_pages;

	curr = BLOCK_OFF(curr_tail);
	curr_page = (struct nova_inode_log_page *)nova_get_block(sb, curr);
	nova_set_next_page_address(sb, curr_page, new_block, 1);

	curr = pi->log_head;

	pi->log_head = possible_head;
	nova_dbg_verbose("%s: %d new head 0x%llx\n", __func__,
					found_head, possible_head);
	nova_dbg_verbose("Num pages %d, freed %d\n", num_pages, freed_pages);
	sih->log_pages += num_pages - freed_pages;
	pi->i_blocks += num_pages - freed_pages;
	/* Don't update log tail pointer here */
	nova_flush_buffer(&pi->log_head, CACHELINE_SIZE, 1);

	if (first_need_free) {
		nova_dbg_verbose("Free log head block 0x%llx\n",
					curr >> PAGE_SHIFT);
		nova_free_log_blocks(sb, pi,
				nova_get_blocknr(sb, curr, btype), 1);
	}

	blocks = sih->valid_bytes / LAST_ENTRY;
	if (sih->valid_bytes % LAST_ENTRY)
		blocks++;

	NOVA_END_TIMING(fast_gc_t, gc_time);

	if (need_thorough_gc(sb, sih, blocks, checked_pages)) {
		nova_dbgv("Thorough GC for inode %lu: checked pages %lu, "
				"valid pages %lu\n", sih->ino,
				checked_pages, blocks);
		nova_inode_log_thorough_gc(sb, pi, sih, blocks, checked_pages);
	}

	return 0;
}

static u64 nova_extend_inode_log(struct super_block *sb, struct nova_inode *pi,
	struct nova_inode_info_header *sih, u64 curr_p)
{
	u64 new_block;
	int allocated;
	unsigned long num_pages;

	if (curr_p == 0) {
		allocated = nova_allocate_inode_log_pages(sb, pi,
					1, &new_block);
		if (allocated != 1) {
			nova_err(sb, "%s ERROR: no inode log page "
					"available\n", __func__);
			return 0;
		}
		pi->log_tail = new_block;
		nova_flush_buffer(&pi->log_tail, CACHELINE_SIZE, 0);
		pi->log_head = new_block;
		sih->log_pages = 1;
		pi->i_blocks++;
		nova_flush_buffer(&pi->log_head, CACHELINE_SIZE, 1);
	} else {
		num_pages = sih->log_pages >= EXTEND_THRESHOLD ?
				EXTEND_THRESHOLD : sih->log_pages;
//		nova_dbg("Before append log pages:\n");
//		nova_print_inode_log_page(sb, inode);
		allocated = nova_allocate_inode_log_pages(sb, pi,
					num_pages, &new_block);
		nova_dbg_verbose("Link block %llu to block %llu\n",
					curr_p >> PAGE_SHIFT,
					new_block >> PAGE_SHIFT);
		if (allocated <= 0) {
			nova_err(sb, "%s ERROR: no inode log page "
					"available\n", __func__);
			nova_dbg("curr_p 0x%llx, %lu pages\n", curr_p,
					sih->log_pages);
			return 0;
		}

		nova_inode_log_fast_gc(sb, pi, sih, curr_p,
						new_block, allocated);

//		nova_dbg("After append log pages:\n");
//		nova_print_inode_log_page(sb, inode);
		/* Atomic switch to new log */
//		nova_switch_to_new_log(sb, pi, new_block, num_pages);
	}
	return new_block;
}

/* For thorough GC, simply append one more page */
static u64 nova_append_one_log_page(struct super_block *sb,
	struct nova_inode *pi, u64 curr_p)
{
	struct nova_inode_log_page *curr_page;
	u64 new_block;
	u64 curr_block;
	int allocated;

	allocated = nova_allocate_inode_log_pages(sb, pi, 1, &new_block);
	if (allocated != 1) {
		nova_err(sb, "%s: ERROR: no inode log page available\n",
				__func__);
		return 0;
	}

	if (curr_p == 0) {
		curr_p = new_block;
	} else {
		/* Link prev block and newly allocated head block */
		curr_block = BLOCK_OFF(curr_p);
		curr_page = (struct nova_inode_log_page *)
				nova_get_block(sb, curr_block);
		nova_set_next_page_address(sb, curr_page, new_block, 1);
	}

	return curr_p;
}

u64 nova_get_append_head(struct super_block *sb, struct nova_inode *pi,
	struct nova_inode_info_header *sih, u64 tail, size_t size, int *extended)
{
	u64 curr_p;

	if (tail)
		curr_p = tail;
	else
		curr_p = pi->log_tail;

	if (curr_p == 0 || (is_last_entry(curr_p, size) &&
				next_log_page(sb, curr_p) == 0)) {
		if (is_last_entry(curr_p, size))
			nova_set_next_page_flag(sb, curr_p);

		if (sih) {
			curr_p = nova_extend_inode_log(sb, pi, sih, curr_p);
		} else {
			curr_p = nova_append_one_log_page(sb, pi, curr_p);
			/* For thorough GC */
			*extended = 1;
		}

		if (curr_p == 0)
			return 0;
	}

	if (is_last_entry(curr_p, size)) {
		nova_set_next_page_flag(sb, curr_p);
		curr_p = next_log_page(sb, curr_p);
	}

	return  curr_p;
}

/*
 * Append a nova_file_write_entry to the current nova_inode_log_page.
 * blocknr and start_blk are pgoff.
 * We cannot update pi->log_tail here because a transaction may contain
 * multiple entries.
 */
u64 nova_append_file_write_entry(struct super_block *sb, struct nova_inode *pi,
	struct inode *inode, struct nova_file_write_entry *data, u64 tail)
{
	struct nova_inode_info *si = NOVA_I(inode);
	struct nova_inode_info_header *sih = &si->header;
	struct nova_file_write_entry *entry;
	u64 curr_p;
	int extended = 0;
	size_t size = sizeof(struct nova_file_write_entry);
	timing_t append_time;

	NOVA_START_TIMING(append_file_entry_t, append_time);

	curr_p = nova_get_append_head(sb, pi, sih, tail, size, &extended);
	if (curr_p == 0)
		return curr_p;

	entry = (struct nova_file_write_entry *)nova_get_block(sb, curr_p);
	memcpy_to_pmem_nocache(entry, data,
			sizeof(struct nova_file_write_entry));
	nova_dbg_verbose("file %lu entry @ 0x%llx: pgoff %llu, num %u, "
			"block %llu, size %llu\n", inode->i_ino,
			curr_p, entry->pgoff, entry->num_pages,
			entry->block >> PAGE_SHIFT, entry->size);
	/* entry->invalid is set to 0 */

	NOVA_END_TIMING(append_file_entry_t, append_time);
	return curr_p;
}

void nova_free_inode_log(struct super_block *sb, struct nova_inode *pi)
{
	u64 curr_block;
	int freed = 0;
	timing_t free_time;

	if (pi->log_head == 0 || pi->log_tail == 0)
		return;

	NOVA_START_TIMING(free_inode_log_t, free_time);

	curr_block = pi->log_head;

	/* The inode is invalid now, no need to call PCOMMIT */
	pi->log_head = pi->log_tail = 0;
	nova_flush_buffer(&pi->log_head, CACHELINE_SIZE, 0);

	freed = nova_free_contiguous_log_blocks(sb, pi, curr_block);

	NOVA_END_TIMING(free_inode_log_t, free_time);
}

static inline void nova_rebuild_file_time_and_size(struct super_block *sb,
	struct nova_inode *pi, struct nova_file_write_entry *entry)
{
	if (!entry || !pi)
		return;

	pi->i_ctime = cpu_to_le32(entry->mtime);
	pi->i_mtime = cpu_to_le32(entry->mtime);
	pi->i_size = cpu_to_le64(entry->size);
}

int nova_rebuild_file_inode_tree(struct super_block *sb,
	struct nova_inode *pi, u64 pi_addr,
	struct nova_inode_info_header *sih)
{
	struct nova_file_write_entry *entry = NULL;
	struct nova_setattr_logentry *attr_entry = NULL;
	struct nova_link_change_entry *link_change_entry = NULL;
	struct nova_inode_log_page *curr_page;
	unsigned int data_bits = blk_type_to_shift[pi->i_blk_type];
	u64 ino = pi->nova_ino;
	timing_t rebuild_time;
	void *addr;
	u64 curr_p;
	u64 next;
	u8 type;

	NOVA_START_TIMING(rebuild_file_t, rebuild_time);
	nova_dbg_verbose("Rebuild file inode %llu tree\n", ino);

	sih->pi_addr = pi_addr;

	curr_p = pi->log_head;
	nova_dbg_verbose("Log head 0x%llx, tail 0x%llx\n",
				curr_p, pi->log_tail);
	if (curr_p == 0 && pi->log_tail == 0)
		return 0;

	sih->log_pages = 1;

	while (curr_p != pi->log_tail) {
		if (goto_next_page(sb, curr_p)) {
			sih->log_pages++;
			curr_p = next_log_page(sb, curr_p);
		}

		if (curr_p == 0) {
			nova_err(sb, "File inode %llu log is NULL!\n", ino);
			BUG();
		}

		addr = (void *)nova_get_block(sb, curr_p);
		type = nova_get_entry_type(addr);
		switch (type) {
			case SET_ATTR:
				attr_entry =
					(struct nova_setattr_logentry *)addr;
				nova_apply_setattr_entry(sb, pi, sih,
								attr_entry);
				sih->last_setattr = curr_p;
				curr_p += sizeof(struct nova_setattr_logentry);
				continue;
			case LINK_CHANGE:
				link_change_entry =
					(struct nova_link_change_entry *)addr;
				nova_apply_link_change_entry(pi,
							link_change_entry);
				sih->last_link_change = curr_p;
				curr_p += sizeof(struct nova_link_change_entry);
				continue;
			case FILE_WRITE:
				break;
			default:
				nova_err(sb, "unknown type %d, 0x%llx\n",
							type, curr_p);
				NOVA_ASSERT(0);
				curr_p += sizeof(struct nova_file_write_entry);
				continue;
		}

		entry = (struct nova_file_write_entry *)addr;
		if (entry->num_pages != entry->invalid_pages) {
			/*
			 * The overlaped blocks are already freed.
			 * Don't double free them, just re-assign the pointers.
			 */
			nova_assign_write_entry(sb, pi, sih, entry, false);
		}

		nova_rebuild_file_time_and_size(sb, pi, entry);
		/* Update sih->i_size for setattr apply operations */
		sih->i_size = le64_to_cpu(pi->i_size);
		curr_p += sizeof(struct nova_file_write_entry);
	}

	sih->i_size = le64_to_cpu(pi->i_size);
	sih->i_mode = le16_to_cpu(pi->i_mode);
	nova_flush_buffer(pi, sizeof(struct nova_inode), 0);

	/* Keep traversing until log ends */
	curr_p &= PAGE_MASK;
	curr_page = (struct nova_inode_log_page *)nova_get_block(sb, curr_p);
	while ((next = curr_page->page_tail.next_page) != 0) {
		sih->log_pages++;
		curr_p = next;
		curr_page = (struct nova_inode_log_page *)
			nova_get_block(sb, curr_p);
	}

	pi->i_blocks = sih->log_pages + (sih->i_size >> data_bits);

//	nova_print_inode_log_page(sb, inode);
	NOVA_END_TIMING(rebuild_file_t, rebuild_time);
	return 0;
}

/*
 * find the file offset for SEEK_DATA/SEEK_HOLE
 */
unsigned long nova_find_region(struct inode *inode, loff_t *offset, int hole)
{
	struct super_block *sb = inode->i_sb;
	struct nova_inode *pi = nova_get_inode(sb, inode);
	struct nova_inode_info *si = NOVA_I(inode);
	struct nova_inode_info_header *sih = &si->header;
	unsigned int data_bits = blk_type_to_shift[pi->i_blk_type];
	unsigned long first_blocknr, last_blocknr;
	unsigned long blocks = 0, offset_in_block;
	int data_found = 0, hole_found = 0;

	if (*offset >= inode->i_size)
		return -ENXIO;

	if (!inode->i_blocks || !sih->i_size) {
		if (hole)
			return inode->i_size;
		else
			return -ENXIO;
	}

	offset_in_block = *offset & ((1UL << data_bits) - 1);

	first_blocknr = *offset >> data_bits;
	last_blocknr = inode->i_size >> data_bits;

	nova_dbg_verbose("find_region offset %llx, first_blocknr %lx,"
		" last_blocknr %lx hole %d\n",
		  *offset, first_blocknr, last_blocknr, hole);

	blocks = nova_lookup_hole_in_range(inode->i_sb, sih,
		first_blocknr, last_blocknr, &data_found, &hole_found, hole);

	/* Searching data but only hole found till the end */
	if (!hole && !data_found && hole_found)
		return -ENXIO;

	if (data_found && !hole_found) {
		/* Searching data but we are already into them */
		if (hole)
			/* Searching hole but only data found, go to the end */
			*offset = inode->i_size;
		return 0;
	}

	/* Searching for hole, hole found and starting inside an hole */
	if (hole && hole_found && !blocks) {
		/* we found data after it */
		if (!data_found)
			/* last hole */
			*offset = inode->i_size;
		return 0;
	}

	if (offset_in_block) {
		blocks--;
		*offset += (blocks << data_bits) +
			   ((1 << data_bits) - offset_in_block);
	} else {
		*offset += blocks << data_bits;
	}

	return 0;
}

const struct address_space_operations nova_aops_dax = {
	.direct_IO		= nova_direct_IO,
	/*.dax_mem_protect	= nova_dax_mem_protect,*/
};
