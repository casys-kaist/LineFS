/*
 * BRIEF DESCRIPTION
 *
 * Proc fs operations
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
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include "nova.h"

const char *proc_dirname = "fs/NOVA";
struct proc_dir_entry *nova_proc_root;

static int nova_seq_timing_show(struct seq_file *seq, void *v)
{
	int i;

	nova_get_timing_stats();

	seq_printf(seq, "======== NOVA kernel timing stats ========\n");
	for (i = 0; i < TIMING_NUM; i++) {
		if (measure_timing || Timingstats[i]) {
			seq_printf(seq, "%s: count %llu, timing %llu, "
				"average %llu\n",
				Timingstring[i],
				Countstats[i],
				Timingstats[i],
				Countstats[i] ?
				Timingstats[i] / Countstats[i] : 0);
		} else {
			seq_printf(seq, "%s: count %llu\n",
				Timingstring[i],
				Countstats[i]);
		}
	}

	return 0;
}

static int nova_seq_timing_open(struct inode *inode, struct file *file)
{
	return single_open(file, nova_seq_timing_show, PDE_DATA(inode));
}

ssize_t nova_seq_clear_stats(struct file *filp, const char __user *buf,
	size_t len, loff_t *ppos)
{
	nova_clear_stats();
	return len;
}

static const struct file_operations nova_seq_timing_fops = {
	.owner		= THIS_MODULE,
	.open		= nova_seq_timing_open,
	.read		= seq_read,
	.write		= nova_seq_clear_stats,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void nova_sysfs_init(struct super_block *sb)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);

	if (nova_proc_root)
		sbi->s_proc = proc_mkdir(sbi->s_bdev->bd_disk->disk_name,
					 nova_proc_root);

	if (sbi->s_proc) {
		proc_create_data("timing_stats", S_IRUGO, sbi->s_proc,
				 &nova_seq_timing_fops, sb);
	}
}

void nova_sysfs_exit(struct super_block *sb)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);

	remove_proc_entry("timing_stats", sbi->s_proc);
	remove_proc_entry(sbi->s_bdev->bd_disk->disk_name, nova_proc_root);
}
