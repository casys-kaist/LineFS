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


/* ======================= Timing ========================= */
enum timing_category {
	init_t,
	mount_t,
	ioremap_t,
	new_init_t,
	recovery_t,

	/* Namei operations */
	create_t,
	lookup_t,
	link_t,
	unlink_t,
	symlink_t,
	mkdir_t,
	rmdir_t,
	mknod_t,
	rename_t,
	readdir_t,
	add_dentry_t,
	remove_dentry_t,
	setattr_t,

	/* I/O operations */
	dax_read_t,
	cow_write_t,
	copy_to_nvmm_t,
	dax_get_block_t,

	/* Memory operations */
	memcpy_r_nvmm_t,
	memcpy_w_nvmm_t,
	memcpy_w_wb_t,
	partial_block_t,

	/* Memory management */
	new_data_blocks_t,
	new_log_blocks_t,
	free_data_t,
	free_log_t,

	/* Transaction */
	create_trans_t,
	link_trans_t,
	update_tail_t,

	/* Logging */
	append_dir_entry_t,
	append_file_entry_t,
	append_link_change_t,
	append_setattr_t,
	fast_gc_t,
	thorough_gc_t,
	check_invalid_t,

	/* Others */
	find_cache_t,
	assign_t,
	fsync_t,
	direct_IO_t,
	delete_file_tree_t,
	delete_dir_tree_t,
	new_vfs_inode_t,
	new_nova_inode_t,
	free_inode_t,
	free_inode_log_t,
	evict_inode_t,
	mmap_fault_t,

	rebuild_dir_t,
	rebuild_file_t,

	/* Sentinel */
	TIMING_NUM,
};

enum stats_category {
	alloc_steps,
	write_breaks,
	read_bytes,
	cow_write_bytes,
	fast_checked_pages,
	thorough_checked_pages,
	fast_gc_pages,
	thorough_gc_pages,

	/* Sentinel */
	STATS_NUM,
};

extern const char *Timingstring[TIMING_NUM];
extern u64 Timingstats[TIMING_NUM];
DECLARE_PER_CPU(u64[TIMING_NUM], Timingstats_percpu);
extern u64 Countstats[TIMING_NUM];
DECLARE_PER_CPU(u64[TIMING_NUM], Countstats_percpu);
extern u64 IOstats[STATS_NUM];
DECLARE_PER_CPU(u64[STATS_NUM], IOstats_percpu);

typedef struct timespec timing_t;

#define NOVA_START_TIMING(name, start) \
	{if (measure_timing) getrawmonotonic(&start);}

#define NOVA_END_TIMING(name, start) \
	{if (measure_timing) { \
		timing_t end; \
		getrawmonotonic(&end); \
		__this_cpu_add(Timingstats_percpu[name], \
			(end.tv_sec - start.tv_sec) * 1000000000 + \
			(end.tv_nsec - start.tv_nsec)); \
	} \
	__this_cpu_add(Countstats_percpu[name], 1); \
	}

#define NOVA_STATS_ADD(name, value) \
	{__this_cpu_add(IOstats_percpu[name], value);}


