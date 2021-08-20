/*
 * NOVA journal header
 *
 * Copyright 2015-2016 Regents of the University of California,
 * UCSD Non-Volatile Systems Lab, Andiry Xu <jix024@cs.ucsd.edu>
 * Copyright 2012-2013 Intel Corporation
 * Copyright 2009-2011 Marco Stornelli <marco.stornelli@gmail.com>
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
#ifndef __NOVA_JOURNAL_H__
#define __NOVA_JOURNAL_H__
#include <linux/slab.h>

/* Lite journal */
struct nova_lite_journal_entry {
	/* The highest byte of addr is type */
	u64 addrs[4];
	u64 values[4];
};

int nova_lite_journal_soft_init(struct super_block *sb);
int nova_lite_journal_hard_init(struct super_block *sb);
u64 nova_create_lite_transaction(struct super_block *sb,
	struct nova_lite_journal_entry *dram_entry1,
	struct nova_lite_journal_entry *dram_entry2,
	int entries, int cpu);
void nova_commit_lite_transaction(struct super_block *sb, u64 tail, int cpu);
#endif    /* __NOVA_JOURNAL_H__ */
