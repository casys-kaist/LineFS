/*
 * NOVA File System statistics
 *
 * Copyright 2015-2016 Regents of the University of California,
 * UCSD Non-Volatile Systems Lab, Andiry Xu <jix024@cs.ucsd.edu>
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

#include "nova.h"

const char *Timingstring[TIMING_NUM] = 
{
	"init",
	"mount",
	"ioremap",
	"new_init",
	"recovery",

	"create",
	"lookup",
	"link",
	"unlink",
	"symlink",
	"mkdir",
	"rmdir",
	"mknod",
	"rename",
	"readdir",
	"add_dentry",
	"remove_dentry",
	"setattr",

	"dax_read",
	"cow_write",
	"copy_to_nvmm",
	"dax_get_block",

	"memcpy_read_nvmm",
	"memcpy_write_nvmm",
	"memcpy_write_back_to_nvmm",
	"handle_partial_block",

	"new_data_blocks",
	"new_log_blocks",
	"free_data_blocks",
	"free_log_blocks",

	"transaction_new_inode",
	"transaction_link_change",
	"update_tail",

	"append_dir_entry",
	"append_file_entry",
	"append_link_change",
	"append_setattr",
	"log_fast_gc",
	"log_thorough_gc",
	"check_invalid_log",

	"find_cache_page",
	"assign_blocks",
	"fsync",
	"direct_IO",
	"delete_file_tree",
	"delete_dir_tree",
	"new_vfs_inode",
	"new_nova_inode",
	"free_inode",
	"free_inode_log",
	"evict_inode",
	"mmap_page_fault",

	"rebuild_dir",
	"rebuild_file",
};

u64 Timingstats[TIMING_NUM];
DEFINE_PER_CPU(u64[TIMING_NUM], Timingstats_percpu);
u64 Countstats[TIMING_NUM];
DEFINE_PER_CPU(u64[TIMING_NUM], Countstats_percpu);
u64 IOstats[STATS_NUM];
DEFINE_PER_CPU(u64[STATS_NUM], IOstats_percpu);

static void nova_print_alloc_stats(struct super_block *sb)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);
	struct free_list *free_list;
	unsigned long alloc_log_count = 0;
	unsigned long alloc_log_pages = 0;
	unsigned long alloc_data_count = 0;
	unsigned long alloc_data_pages = 0;
	unsigned long free_log_count = 0;
	unsigned long freed_log_pages = 0;
	unsigned long free_data_count = 0;
	unsigned long freed_data_pages = 0;
	int i;

	printk("=========== NOVA allocation stats ===========\n");
	printk("Alloc %llu, alloc steps %llu, average %llu\n",
		Countstats[new_data_blocks_t], IOstats[alloc_steps],
		Countstats[new_data_blocks_t] ?
			IOstats[alloc_steps] / Countstats[new_data_blocks_t] : 0);
	printk("Free %llu\n", Countstats[free_data_t]);
	printk("Fast GC %llu, check pages %llu, free pages %llu, average %llu\n",
		Countstats[fast_gc_t], IOstats[fast_checked_pages],
		IOstats[fast_gc_pages], Countstats[fast_gc_t] ?
			IOstats[fast_gc_pages] / Countstats[fast_gc_t] : 0);
	printk("Thorough GC %llu, checked pages %llu, free pages %llu, "
		"average %llu\n", Countstats[thorough_gc_t],
		IOstats[thorough_checked_pages], IOstats[thorough_gc_pages],
		Countstats[thorough_gc_t] ?
			IOstats[thorough_gc_pages] / Countstats[thorough_gc_t] : 0);

	for (i = 0; i < sbi->cpus; i++) {
		free_list = nova_get_free_list(sb, i);

		alloc_log_count += free_list->alloc_log_count;
		alloc_log_pages += free_list->alloc_log_pages;
		alloc_data_count += free_list->alloc_data_count;
		alloc_data_pages += free_list->alloc_data_pages;
		free_log_count += free_list->free_log_count;
		freed_log_pages += free_list->freed_log_pages;
		free_data_count += free_list->free_data_count;
		freed_data_pages += free_list->freed_data_pages;
	}

	printk("alloc log count %lu, allocated log pages %lu, "
		"alloc data count %lu, allocated data pages %lu, "
		"free log count %lu, freed log pages %lu, "
		"free data count %lu, freed data pages %lu\n",
		alloc_log_count, alloc_log_pages,
		alloc_data_count, alloc_data_pages,
		free_log_count, freed_log_pages,
		free_data_count, freed_data_pages);
}

static void nova_print_IO_stats(struct super_block *sb)
{
	printk("=========== NOVA I/O stats ===========\n");
	printk("Read %llu, bytes %llu, average %llu\n",
		Countstats[dax_read_t], IOstats[read_bytes],
		Countstats[dax_read_t] ?
			IOstats[read_bytes] / Countstats[dax_read_t] : 0);
	printk("COW write %llu, bytes %llu, average %llu, "
		"write breaks %llu, average %llu\n",
		Countstats[cow_write_t], IOstats[cow_write_bytes],
		Countstats[cow_write_t] ?
			IOstats[cow_write_bytes] / Countstats[cow_write_t] : 0,
		IOstats[write_breaks], Countstats[cow_write_t] ?
			IOstats[write_breaks] / Countstats[cow_write_t] : 0);
}

void nova_get_timing_stats(void)
{
	int i;
	int cpu;

	for (i = 0; i < TIMING_NUM; i++) {
		Timingstats[i] = 0;
		Countstats[i] = 0;
		for_each_possible_cpu(cpu) {
			Timingstats[i] += per_cpu(Timingstats_percpu[i], cpu);
			Countstats[i] += per_cpu(Countstats_percpu[i], cpu);
		}
	}
}

void nova_get_IO_stats(void)
{
	int i;
	int cpu;

	for (i = 0; i < STATS_NUM; i++) {
		IOstats[i] = 0;
		for_each_possible_cpu(cpu)
			IOstats[i] += per_cpu(IOstats_percpu[i], cpu);
	}
}

void nova_print_timing_stats(struct super_block *sb)
{
	int i;

	nova_get_timing_stats();
	nova_get_IO_stats();

	printk("======== NOVA kernel timing stats ========\n");
	for (i = 0; i < TIMING_NUM; i++) {
		if (measure_timing || Timingstats[i]) {
			printk("%s: count %llu, timing %llu, average %llu\n",
				Timingstring[i],
				Countstats[i],
				Timingstats[i],
				Countstats[i] ?
				Timingstats[i] / Countstats[i] : 0);
		} else {
			printk("%s: count %llu\n",
				Timingstring[i],
				Countstats[i]);
		}
	}

	nova_print_alloc_stats(sb);
	nova_print_IO_stats(sb);
}

static void nova_clear_timing_stats(void)
{
	int i;
	int cpu;

	for (i = 0; i < TIMING_NUM; i++) {
		Countstats[i] = 0;
		Timingstats[i] = 0;
		for_each_possible_cpu(cpu) {
			per_cpu(Timingstats_percpu[i], cpu) = 0;
			per_cpu(Countstats_percpu[i], cpu) = 0;
		}
	}
}

static void nova_clear_IO_stats(void)
{
	int i;
	int cpu;

	for (i = 0; i < STATS_NUM; i++) {
		IOstats[i] = 0;
		for_each_possible_cpu(cpu)
			per_cpu(IOstats_percpu[i], cpu) = 0;
	}
}

void nova_clear_stats(void)
{
	nova_clear_timing_stats();
	nova_clear_IO_stats();
}

static inline void nova_print_file_write_entry(struct super_block *sb,
	u64 curr, struct nova_file_write_entry *entry)
{
	nova_dbg("file write entry @ 0x%llx: paoff %llu, pages %u, "
			"blocknr %llu, invalid count %u, size %llu\n",
			curr, entry->pgoff, entry->num_pages,
			entry->block >> PAGE_SHIFT,
			entry->invalid_pages, entry->size);
}

static inline void nova_print_set_attr_entry(struct super_block *sb,
	u64 curr, struct nova_setattr_logentry *entry)
{
	nova_dbg("set attr entry @ 0x%llx: mode %u, size %llu\n",
			curr, entry->mode, entry->size);
}

static inline void nova_print_link_change_entry(struct super_block *sb,
	u64 curr, struct nova_link_change_entry *entry)
{
	nova_dbg("link change entry @ 0x%llx: links %u, flags %u\n",
			curr, entry->links, entry->flags);
}

static inline size_t nova_print_dentry(struct super_block *sb,
	u64 curr, struct nova_dentry *entry)
{
	nova_dbg("dir logentry @ 0x%llx: inode %llu, "
			"namelen %u, rec len %u\n", curr,
			le64_to_cpu(entry->ino),
			entry->name_len, le16_to_cpu(entry->de_len));

	return le16_to_cpu(entry->de_len);
}

static u64 nova_print_log_entry(struct super_block *sb, u64 curr)
{
	void *addr;
	size_t size;
	u8 type;

	addr = (void *)nova_get_block(sb, curr);
	type = nova_get_entry_type(addr);
	switch (type) {
		case SET_ATTR:
			nova_print_set_attr_entry(sb, curr, addr);
			curr += sizeof(struct nova_setattr_logentry);
			break;
		case LINK_CHANGE:
			nova_print_link_change_entry(sb, curr, addr);
			curr += sizeof(struct nova_link_change_entry);
			break;
		case FILE_WRITE:
			nova_print_file_write_entry(sb, curr, addr);
			curr += sizeof(struct nova_file_write_entry);
			break;
		case DIR_LOG:
			size = nova_print_dentry(sb, curr, addr);
			curr += size;
			if (size == 0) {
				nova_dbg("%s: dentry with size 0 @ 0x%llx\n",
						__func__, curr);
				curr += sizeof(struct nova_file_write_entry);
				NOVA_ASSERT(0);
			}
			break;
		case NEXT_PAGE:
			nova_dbg("%s: next page sign @ 0x%llx\n",
						__func__, curr);
			curr = PAGE_TAIL(curr);
			break;
		default:
			nova_dbg("%s: unknown type %d, 0x%llx\n",
						__func__, type, curr);
			curr += sizeof(struct nova_file_write_entry);
			NOVA_ASSERT(0);
			break;
	}

	return curr;
}

void nova_print_curr_log_page(struct super_block *sb, u64 curr)
{
	struct nova_inode_page_tail *tail;
	u64 start, end;

	start = curr & (~INVALID_MASK);
	end = PAGE_TAIL(curr);

	while (start < end) {
		start = nova_print_log_entry(sb, start);
	}

	tail = nova_get_block(sb, end);
	nova_dbg("Page tail. curr 0x%llx, next page 0x%llx\n",
			start, tail->next_page);
}

void nova_print_nova_log(struct super_block *sb,
	struct nova_inode_info_header *sih, struct nova_inode *pi)
{
	u64 curr;

	if (pi->log_tail == 0)
		return;

	curr = pi->log_head;
	nova_dbg("Pi %lu: log head 0x%llx, tail 0x%llx\n",
			sih->ino, curr, pi->log_tail);
	while (curr != pi->log_tail) {
		if ((curr & (PAGE_SIZE - 1)) == LAST_ENTRY) {
			struct nova_inode_page_tail *tail =
					nova_get_block(sb, curr);
			nova_dbg("Log tail, curr 0x%llx, next page 0x%llx\n",
					curr, tail->next_page);
			curr = tail->next_page;
		} else {
			curr = nova_print_log_entry(sb, curr);
		}
	}
}

void nova_print_inode_log(struct super_block *sb, struct inode *inode)
{
	struct nova_inode_info *si = NOVA_I(inode);
	struct nova_inode_info_header *sih = &si->header;
	struct nova_inode *pi;

	pi = nova_get_inode(sb, inode);
	nova_print_nova_log(sb, sih, pi);
}

int nova_get_nova_log_pages(struct super_block *sb,
	struct nova_inode_info_header *sih, struct nova_inode *pi)
{
	struct nova_inode_log_page *curr_page;
	u64 curr, next;
	int count = 1;

	if (pi->log_head == 0 || pi->log_tail == 0) {
		nova_dbg("Pi %lu has no log\n", sih->ino);
		return 0;
	}

	curr = pi->log_head;
	curr_page = (struct nova_inode_log_page *)nova_get_block(sb, curr);
	while ((next = curr_page->page_tail.next_page) != 0) {
		curr = next;
		curr_page = (struct nova_inode_log_page *)
			nova_get_block(sb, curr);
		count++;
	}

	return count;
}

void nova_print_nova_log_pages(struct super_block *sb,
	struct nova_inode_info_header *sih, struct nova_inode *pi)
{
	struct nova_inode_log_page *curr_page;
	u64 curr, next;
	int count = 1;
	int used = count;

	if (pi->log_head == 0 || pi->log_tail == 0) {
		nova_dbg("Pi %lu has no log\n", sih->ino);
		return;
	}

	curr = pi->log_head;
	nova_dbg("Pi %lu: log head @ 0x%llx, tail @ 0x%llx\n",
			sih->ino, curr, pi->log_tail);
	curr_page = (struct nova_inode_log_page *)nova_get_block(sb, curr);
	while ((next = curr_page->page_tail.next_page) != 0) {
		nova_dbg("Current page 0x%llx, next page 0x%llx\n",
			curr >> PAGE_SHIFT, next >> PAGE_SHIFT);
		if (pi->log_tail >> PAGE_SHIFT == curr >> PAGE_SHIFT)
			used = count;
		curr = next;
		curr_page = (struct nova_inode_log_page *)
			nova_get_block(sb, curr);
		count++;
	}
	if (pi->log_tail >> PAGE_SHIFT == curr >> PAGE_SHIFT)
		used = count;
	nova_dbg("Pi %lu: log used %d pages, has %d pages, "
		"si reports %lu pages\n", sih->ino, used, count,
		sih->log_pages);
}

void nova_print_inode_log_pages(struct super_block *sb, struct inode *inode)
{
	struct nova_inode *pi;
	struct nova_inode_info *si = NOVA_I(inode);
	struct nova_inode_info_header *sih = &si->header;

	pi = nova_get_inode(sb, inode);
	nova_print_nova_log_pages(sb, sih, pi);
}

void nova_print_free_lists(struct super_block *sb)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);
	struct free_list *free_list;
	int i;

	nova_dbg("======== NOVA per-CPU free list allocation stats ========\n");
	for (i = 0; i < sbi->cpus; i++) {
		free_list = nova_get_free_list(sb, i);
		nova_dbg("Free list %d: block start %lu, block end %lu, "
			"num_blocks %lu, num_free_blocks %lu, blocknode %lu\n",
			i, free_list->block_start, free_list->block_end,
			free_list->block_end - free_list->block_start + 1,
			free_list->num_free_blocks, free_list->num_blocknode);

		nova_dbg("Free list %d: alloc log count %lu, "
			"allocated log pages %lu, alloc data count %lu, "
			"allocated data pages %lu, free log count %lu, "
			"freed log pages %lu, free data count %lu, "
			"freed data pages %lu\n", i,
			free_list->alloc_log_count,
			free_list->alloc_log_pages,
			free_list->alloc_data_count,
			free_list->alloc_data_pages,
			free_list->free_log_count,
			free_list->freed_log_pages,
			free_list->free_data_count,
			free_list->freed_data_pages);
	}

	i = SHARED_CPU;
	free_list = nova_get_free_list(sb, i);
	nova_dbg("Free list %d: block start %lu, block end %lu, "
		"num_blocks %lu, num_free_blocks %lu, blocknode %lu\n",
		i, free_list->block_start, free_list->block_end,
		free_list->block_end - free_list->block_start + 1,
		free_list->num_free_blocks, free_list->num_blocknode);

	nova_dbg("Free list %d: alloc log count %lu, "
		"allocated log pages %lu, alloc data count %lu, "
		"allocated data pages %lu, free log count %lu, "
		"freed log pages %lu, free data count %lu, "
		"freed data pages %lu\n", i,
		free_list->alloc_log_count, free_list->alloc_log_pages,
		free_list->alloc_data_count, free_list->alloc_data_pages,
		free_list->free_log_count, free_list->freed_log_pages,
		free_list->free_data_count, free_list->freed_data_pages);
}

