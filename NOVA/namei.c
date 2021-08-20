/*
 * BRIEF DESCRIPTION
 *
 * Inode operations for directories.
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
#include <linux/pagemap.h>
#include "nova.h"

static ino_t nova_inode_by_name(struct inode *dir, struct qstr *entry,
				 struct nova_dentry **res_entry)
{
	struct super_block *sb = dir->i_sb;
	struct nova_dentry *direntry;

	direntry = nova_find_dentry(sb, NULL, dir,
					entry->name, entry->len);
	if (direntry == NULL)
		return 0;

	*res_entry = direntry;
	return direntry->ino;
}

static struct dentry *nova_lookup(struct inode *dir, struct dentry *dentry,
				   unsigned int flags)
{
	struct inode *inode = NULL;
	struct nova_dentry *de;
	ino_t ino;
	timing_t lookup_time;

	NOVA_START_TIMING(lookup_t, lookup_time);
	if (dentry->d_name.len > NOVA_NAME_LEN) {
		nova_dbg("%s: namelen %u exceeds limit\n",
			__func__, dentry->d_name.len);
		return ERR_PTR(-ENAMETOOLONG);
	}

	nova_dbg_verbose("%s: %s\n", __func__, dentry->d_name.name);
	ino = nova_inode_by_name(dir, &dentry->d_name, &de);
	nova_dbg_verbose("%s: ino %lu\n", __func__, ino);
	if (ino) {
		inode = nova_iget(dir->i_sb, ino);
		if (inode == ERR_PTR(-ESTALE) || inode == ERR_PTR(-ENOMEM)
				|| inode == ERR_PTR(-EACCES)) {
			nova_err(dir->i_sb,
				  "%s: get inode failed: %lu\n",
				  __func__, (unsigned long)ino);
			return ERR_PTR(-EIO);
		}
	}

	NOVA_END_TIMING(lookup_t, lookup_time);
	return d_splice_alias(inode, dentry);
}

static void nova_lite_transaction_for_new_inode(struct super_block *sb,
	struct nova_inode *pi, struct nova_inode *pidir, u64 pidir_tail)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);
	struct nova_lite_journal_entry entry;
	int cpu;
	u64 journal_tail;
	timing_t trans_time;

	NOVA_START_TIMING(create_trans_t, trans_time);

	/* Commit a lite transaction */
	memset(&entry, 0, sizeof(struct nova_lite_journal_entry));
	entry.addrs[0] = (u64)nova_get_addr_off(sbi, &pidir->log_tail);
	entry.addrs[0] |= (u64)8 << 56;
	entry.values[0] = pidir->log_tail;

	entry.addrs[1] = (u64)nova_get_addr_off(sbi, &pi->valid);
	entry.addrs[1] |= (u64)1 << 56;
	entry.values[1] = pi->valid;

	cpu = smp_processor_id();
	spin_lock(&sbi->journal_locks[cpu]);
	journal_tail = nova_create_lite_transaction(sb, &entry, NULL, 1, cpu);

	pidir->log_tail = pidir_tail;
	nova_flush_buffer(&pidir->log_tail, CACHELINE_SIZE, 0);
	pi->valid = 1;
	nova_flush_buffer(&pi->valid, CACHELINE_SIZE, 0);
	PERSISTENT_BARRIER();

	nova_commit_lite_transaction(sb, journal_tail, cpu);
	spin_unlock(&sbi->journal_locks[cpu]);
	NOVA_END_TIMING(create_trans_t, trans_time);
}

/* Returns new tail after append */
/*
 * By the time this is called, we already have created
 * the directory cache entry for the new file, but it
 * is so far negative - it has no inode.
 *
 * If the create succeeds, we fill in the inode information
 * with d_instantiate().
 */
static int nova_create(struct inode *dir, struct dentry *dentry, umode_t mode,
			bool excl)
{
	struct inode *inode = NULL;
	int err = PTR_ERR(inode);
	struct super_block *sb = dir->i_sb;
	struct nova_inode *pidir, *pi;
	u64 pi_addr = 0;
	u64 tail = 0;
	u64 ino;
	timing_t create_time;

	NOVA_START_TIMING(create_t, create_time);

	pidir = nova_get_inode(sb, dir);
	if (!pidir)
		goto out_err;

	ino = nova_new_nova_inode(sb, &pi_addr);
	if (ino == 0)
		goto out_err;

	err = nova_add_dentry(dentry, ino, 0, 0, &tail);
	if (err)
		goto out_err;

	nova_dbgv("%s: %s\n", __func__, dentry->d_name.name);
	nova_dbgv("%s: inode %llu, dir %lu\n", __func__, ino, dir->i_ino);
	inode = nova_new_vfs_inode(TYPE_CREATE, dir, pi_addr, ino, mode,
					0, 0, &dentry->d_name);
	if (IS_ERR(inode))
		goto out_err;

	d_instantiate(dentry, inode);
	unlock_new_inode(inode);

	pi = nova_get_block(sb, pi_addr);
	nova_lite_transaction_for_new_inode(sb, pi, pidir, tail);
	NOVA_END_TIMING(create_t, create_time);
	return err;
out_err:
	nova_err(sb, "%s return %d\n", __func__, err);
	NOVA_END_TIMING(create_t, create_time);
	return err;
}

static int nova_mknod(struct inode *dir, struct dentry *dentry, umode_t mode,
		       dev_t rdev)
{
	struct inode *inode = NULL;
	int err = PTR_ERR(inode);
	struct super_block *sb = dir->i_sb;
	u64 pi_addr = 0;
	struct nova_inode *pidir, *pi;
	u64 tail = 0;
	u64 ino;
	timing_t mknod_time;

	NOVA_START_TIMING(mknod_t, mknod_time);

	pidir = nova_get_inode(sb, dir);
	if (!pidir)
		goto out_err;

	ino = nova_new_nova_inode(sb, &pi_addr);
	if (ino == 0)
		goto out_err;

	nova_dbgv("%s: %s\n", __func__, dentry->d_name.name);
	nova_dbgv("%s: inode %llu, dir %lu\n", __func__, ino, dir->i_ino);
	err = nova_add_dentry(dentry, ino, 0, 0, &tail);
	if (err)
		goto out_err;

	inode = nova_new_vfs_inode(TYPE_MKNOD, dir, pi_addr, ino, mode,
					0, rdev, &dentry->d_name);
	if (IS_ERR(inode))
		goto out_err;

	d_instantiate(dentry, inode);
	unlock_new_inode(inode);

	pi = nova_get_block(sb, pi_addr);
	nova_lite_transaction_for_new_inode(sb, pi, pidir, tail);
	NOVA_END_TIMING(mknod_t, mknod_time);
	return err;
out_err:
	nova_err(sb, "%s return %d\n", __func__, err);
	NOVA_END_TIMING(mknod_t, mknod_time);
	return err;
}

static int nova_symlink(struct inode *dir, struct dentry *dentry,
			 const char *symname)
{
	struct super_block *sb = dir->i_sb;
	int err = -ENAMETOOLONG;
	unsigned len = strlen(symname);
	struct inode *inode;
	u64 pi_addr = 0;
	struct nova_inode *pidir, *pi;
	u64 log_block = 0;
	unsigned long name_blocknr = 0;
	int allocated;
	u64 tail = 0;
	u64 ino;
	timing_t symlink_time;

	NOVA_START_TIMING(symlink_t, symlink_time);
	if (len + 1 > sb->s_blocksize)
		goto out;

	pidir = nova_get_inode(sb, dir);
	if (!pidir)
		goto out_fail1;

	ino = nova_new_nova_inode(sb, &pi_addr);
	if (ino == 0)
		goto out_fail1;

	nova_dbgv("%s: name %s, symname %s\n", __func__,
				dentry->d_name.name, symname);
	nova_dbgv("%s: inode %llu, dir %lu\n", __func__, ino, dir->i_ino);
	err = nova_add_dentry(dentry, ino, 0, 0, &tail);
	if (err)
		goto out_fail1;

	inode = nova_new_vfs_inode(TYPE_SYMLINK, dir, pi_addr, ino,
					S_IFLNK|S_IRWXUGO, len, 0,
					&dentry->d_name);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_fail1;
	}

	pi = nova_get_inode(sb, inode);
	allocated = nova_allocate_inode_log_pages(sb, pi,
						1, &log_block);
	if (allocated != 1 || log_block == 0) {
		err = allocated;
		goto out_fail1;
	}

	allocated = nova_new_data_blocks(sb, pi, &name_blocknr,
					1, 0, 1, 0);
	if (allocated != 1 || name_blocknr == 0) {
		err = allocated;
		goto out_fail2;
	}

	pi->i_blocks = 2;
	nova_block_symlink(sb, pi, inode, log_block, name_blocknr,
				symname, len);
	d_instantiate(dentry, inode);
	unlock_new_inode(inode);

	nova_lite_transaction_for_new_inode(sb, pi, pidir, tail);
out:
	NOVA_END_TIMING(symlink_t, symlink_time);
	return err;

out_fail2:
	nova_free_log_blocks(sb, pi, log_block >> PAGE_SHIFT, 1);
out_fail1:
	nova_err(sb, "%s return %d\n", __func__, err);
	goto out;
}

static void nova_lite_transaction_for_time_and_link(struct super_block *sb,
	struct nova_inode *pi, struct nova_inode *pidir, u64 pi_tail,
	u64 pidir_tail, int invalidate)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);
	struct nova_lite_journal_entry entry;
	u64 journal_tail;
	int cpu;
	timing_t trans_time;

	NOVA_START_TIMING(link_trans_t, trans_time);

	/* Commit a lite transaction */
	memset(&entry, 0, sizeof(struct nova_lite_journal_entry));
	entry.addrs[0] = (u64)nova_get_addr_off(sbi, &pi->log_tail);
	entry.addrs[0] |= (u64)8 << 56;
	entry.values[0] = pi->log_tail;

	entry.addrs[1] = (u64)nova_get_addr_off(sbi, &pidir->log_tail);
	entry.addrs[1] |= (u64)8 << 56;
	entry.values[1] = pidir->log_tail;

	if (invalidate) {
		entry.addrs[2] = (u64)nova_get_addr_off(sbi, &pi->valid);
		entry.addrs[2] |= (u64)1 << 56;
		entry.values[2] = pi->valid;
	}

	cpu = smp_processor_id();
	spin_lock(&sbi->journal_locks[cpu]);
	journal_tail = nova_create_lite_transaction(sb, &entry, NULL, 1, cpu);

	pi->log_tail = pi_tail;
	nova_flush_buffer(&pi->log_tail, CACHELINE_SIZE, 0);
	pidir->log_tail = pidir_tail;
	nova_flush_buffer(&pidir->log_tail, CACHELINE_SIZE, 0);
	if (invalidate) {
		pi->valid = 0;
		nova_flush_buffer(&pi->valid, CACHELINE_SIZE, 0);
	}
	PERSISTENT_BARRIER();

	nova_commit_lite_transaction(sb, journal_tail, cpu);
	spin_unlock(&sbi->journal_locks[cpu]);
	NOVA_END_TIMING(link_trans_t, trans_time);
}

/* Returns new tail after append */
int nova_append_link_change_entry(struct super_block *sb,
	struct nova_inode *pi, struct inode *inode, u64 tail, u64 *new_tail)
{
	struct nova_inode_info *si = NOVA_I(inode);
	struct nova_inode_info_header *sih = &si->header;
	struct nova_link_change_entry *entry;
	u64 curr_p;
	int extended = 0;
	size_t size = sizeof(struct nova_link_change_entry);
	timing_t append_time;

	NOVA_START_TIMING(append_link_change_t, append_time);
	nova_dbg_verbose("%s: inode %lu attr change\n",
				__func__, inode->i_ino);

	curr_p = nova_get_append_head(sb, pi, sih, tail, size, &extended);
	if (curr_p == 0)
		return -ENOMEM;

	entry = (struct nova_link_change_entry *)nova_get_block(sb, curr_p);
	entry->entry_type = LINK_CHANGE;
	entry->links = cpu_to_le16(inode->i_nlink);
	entry->ctime = cpu_to_le32(inode->i_ctime.tv_sec);
	entry->flags = cpu_to_le32(inode->i_flags);
	entry->generation = cpu_to_le32(inode->i_generation);
	nova_flush_buffer(entry, size, 0);
	*new_tail = curr_p + size;
	sih->last_link_change = curr_p;

	NOVA_END_TIMING(append_link_change_t, append_time);
	return 0;
}

void nova_apply_link_change_entry(struct nova_inode *pi,
	struct nova_link_change_entry *entry)
{
	if (entry->entry_type != LINK_CHANGE)
		BUG();

	pi->i_links_count	= entry->links;
	pi->i_ctime		= entry->ctime;
	pi->i_flags		= entry->flags;
	pi->i_generation	= entry->generation;

	/* Do not flush now */
}

static int nova_link(struct dentry *dest_dentry, struct inode *dir,
		      struct dentry *dentry)
{
	struct super_block *sb = dir->i_sb;
	struct inode *inode = dest_dentry->d_inode;
	struct nova_inode *pi = nova_get_inode(sb, inode);
	struct nova_inode *pidir;
	u64 pidir_tail = 0, pi_tail = 0;
	int err = -ENOMEM;
	timing_t link_time;

	NOVA_START_TIMING(link_t, link_time);
	if (inode->i_nlink >= NOVA_LINK_MAX) {
		err = -EMLINK;
		goto out;
	}

	pidir = nova_get_inode(sb, dir);
	if (!pidir) {
		err = -EINVAL;
		goto out;
	}

	ihold(inode);

	nova_dbgv("%s: name %s, dest %s\n", __func__,
			dentry->d_name.name, dest_dentry->d_name.name);
	nova_dbgv("%s: inode %lu, dir %lu\n", __func__,
			inode->i_ino, dir->i_ino);
	err = nova_add_dentry(dentry, inode->i_ino, 0, 0, &pidir_tail);
	if (err) {
		iput(inode);
		goto out;
	}

	inode->i_ctime = CURRENT_TIME_SEC;
	inc_nlink(inode);

	err = nova_append_link_change_entry(sb, pi, inode, 0, &pi_tail);
	if (err) {
		iput(inode);
		goto out;
	}

	d_instantiate(dentry, inode);
	nova_lite_transaction_for_time_and_link(sb, pi, pidir,
						pi_tail, pidir_tail, 0);

out:
	NOVA_END_TIMING(link_t, link_time);
	return err;
}

static int nova_unlink(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	struct super_block *sb = dir->i_sb;
	int retval = -ENOMEM;
	struct nova_inode *pi = nova_get_inode(sb, inode);
	struct nova_inode *pidir;
	u64 pidir_tail = 0, pi_tail = 0;
	int invalidate = 0;
	timing_t unlink_time;

	NOVA_START_TIMING(unlink_t, unlink_time);

	pidir = nova_get_inode(sb, dir);
	if (!pidir)
		goto out;

	nova_dbgv("%s: %s\n", __func__, dentry->d_name.name);
	nova_dbgv("%s: inode %lu, dir %lu\n", __func__,
				inode->i_ino, dir->i_ino);
	retval = nova_remove_dentry(dentry, 0, 0, &pidir_tail);
	if (retval)
		goto out;

	inode->i_ctime = dir->i_ctime;

	if (inode->i_nlink == 1)
		invalidate = 1;

	if (inode->i_nlink) {
		drop_nlink(inode);
	}

	retval = nova_append_link_change_entry(sb, pi, inode, 0, &pi_tail);
	if (retval)
		goto out;

	nova_lite_transaction_for_time_and_link(sb, pi, pidir,
					pi_tail, pidir_tail, invalidate);

	NOVA_END_TIMING(unlink_t, unlink_time);
	return 0;
out:
	nova_err(sb, "%s return %d\n", __func__, retval);
	NOVA_END_TIMING(unlink_t, unlink_time);
	return retval;
}

static int nova_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	struct super_block *sb = dir->i_sb;
	struct inode *inode;
	struct nova_inode *pidir, *pi;
	struct nova_inode_info *si;
	struct nova_inode_info_header *sih = NULL;
	u64 pi_addr = 0;
	u64 tail = 0;
	u64 ino;
	int err = -EMLINK;
	timing_t mkdir_time;

	NOVA_START_TIMING(mkdir_t, mkdir_time);
	if (dir->i_nlink >= NOVA_LINK_MAX)
		goto out;

	ino = nova_new_nova_inode(sb, &pi_addr);
	if (ino == 0)
		goto out_err;

	nova_dbgv("%s: name %s\n", __func__, dentry->d_name.name);
	nova_dbgv("%s: inode %llu, dir %lu, link %d\n", __func__,
				ino, dir->i_ino, dir->i_nlink);
	err = nova_add_dentry(dentry, ino, 1, 0, &tail);
	if (err) {
		nova_dbg("failed to add dir entry\n");
		goto out_err;
	}

	inode = nova_new_vfs_inode(TYPE_MKDIR, dir, pi_addr, ino,
					S_IFDIR | mode, sb->s_blocksize,
					0, &dentry->d_name);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_err;
	}

	pi = nova_get_inode(sb, inode);
	nova_append_dir_init_entries(sb, pi, inode->i_ino, dir->i_ino);

	/* Build the dir tree */
	si = NOVA_I(inode);
	sih = &si->header;
	nova_rebuild_dir_inode_tree(sb, pi, pi_addr, sih);

	pidir = nova_get_inode(sb, dir);
	dir->i_blocks = pidir->i_blocks;
	inc_nlink(dir);
	d_instantiate(dentry, inode);
	unlock_new_inode(inode);

	nova_lite_transaction_for_new_inode(sb, pi, pidir, tail);
out:
	NOVA_END_TIMING(mkdir_t, mkdir_time);
	return err;

out_err:
//	clear_nlink(inode);
	nova_err(sb, "%s return %d\n", __func__, err);
	goto out;
}

/*
 * routine to check that the specified directory is empty (for rmdir)
 */
static int nova_empty_dir(struct inode *inode)
{
	struct super_block *sb;
	struct nova_inode_info *si = NOVA_I(inode);
	struct nova_inode_info_header *sih = &si->header;
	struct nova_dentry *entry;
	unsigned long pos = 0;
	struct nova_dentry *entries[4];
	int nr_entries;
	int i;

	sb = inode->i_sb;
	nr_entries = radix_tree_gang_lookup(&sih->tree,
					(void **)entries, pos, 4);
	if (nr_entries > 2)
		return 0;

	for (i = 0; i < nr_entries; i++) {
		entry = entries[i];
		if (!is_dir_init_entry(sb, entry))
			return 0;
	}

	return 1;
}

static int nova_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	struct nova_dentry *de;
	struct super_block *sb = inode->i_sb;
	struct nova_inode *pi = nova_get_inode(sb, inode), *pidir;
	u64 pidir_tail = 0, pi_tail = 0;
	struct nova_inode_info *si = NOVA_I(inode);
	struct nova_inode_info_header *sih = &si->header;
	int err = -ENOTEMPTY;
	timing_t rmdir_time;

	NOVA_START_TIMING(rmdir_t, rmdir_time);
	if (!inode)
		return -ENOENT;

	nova_dbgv("%s: name %s\n", __func__, dentry->d_name.name);
	pidir = nova_get_inode(sb, dir);
	if (!pidir)
		return -EINVAL;

	if (nova_inode_by_name(dir, &dentry->d_name, &de) == 0)
		return -ENOENT;

	if (!nova_empty_dir(inode))
		return err;

	nova_dbgv("%s: inode %lu, dir %lu, link %d\n", __func__,
				inode->i_ino, dir->i_ino, dir->i_nlink);

	if (inode->i_nlink != 2)
		nova_dbg("empty directory %lu has nlink!=2 (%d), dir %lu",
				inode->i_ino, inode->i_nlink, dir->i_ino);

	err = nova_remove_dentry(dentry, -1, 0, &pidir_tail);
	if (err)
		goto end_rmdir;

	/*inode->i_version++; */
	clear_nlink(inode);
	inode->i_ctime = dir->i_ctime;

	if (dir->i_nlink)
		drop_nlink(dir);

	nova_delete_dir_tree(sb, sih);

	err = nova_append_link_change_entry(sb, pi, inode, 0, &pi_tail);
	if (err)
		goto end_rmdir;

	nova_lite_transaction_for_time_and_link(sb, pi, pidir,
						pi_tail, pidir_tail, 1);

	NOVA_END_TIMING(rmdir_t, rmdir_time);
	return err;

end_rmdir:
	nova_err(sb, "%s return %d\n", __func__, err);
	NOVA_END_TIMING(rmdir_t, rmdir_time);
	return err;
}

static int nova_rename(struct inode *old_dir,
			struct dentry *old_dentry,
			struct inode *new_dir, struct dentry *new_dentry)
{
	struct inode *old_inode = old_dentry->d_inode;
	struct inode *new_inode = new_dentry->d_inode;
	struct super_block *sb = old_inode->i_sb;
	struct nova_sb_info *sbi = NOVA_SB(sb);
	struct nova_inode *old_pi = NULL, *new_pi = NULL;
	struct nova_inode *new_pidir = NULL, *old_pidir = NULL;
	struct nova_lite_journal_entry entry, entry1;
	struct nova_dentry *father_entry = NULL;
	char *head_addr = NULL;
	u64 old_tail = 0, new_tail = 0, new_pi_tail = 0, old_pi_tail = 0;
	int err = -ENOENT;
	int inc_link = 0, dec_link = 0;
	int entries = 0;
	int cpu;
	int change_parent = 0;
	u64 journal_tail;
	timing_t rename_time;

	nova_dbgv("%s: rename %s to %s,\n", __func__,
			old_dentry->d_name.name, new_dentry->d_name.name);
	nova_dbgv("%s: %s inode %lu, old dir %lu, new dir %lu, new inode %lu\n",
			__func__, S_ISDIR(old_inode->i_mode) ? "dir" : "normal",
			old_inode->i_ino, old_dir->i_ino, new_dir->i_ino,
			new_inode ? new_inode->i_ino : 0);
	NOVA_START_TIMING(rename_t, rename_time);

	if (new_inode) {
		err = -ENOTEMPTY;
		if (S_ISDIR(old_inode->i_mode) && !nova_empty_dir(new_inode))
			goto out;
	} else {
		if (S_ISDIR(old_inode->i_mode)) {
			err = -EMLINK;
			if (new_dir->i_nlink >= NOVA_LINK_MAX)
				goto out;
		}
	}

	if (S_ISDIR(old_inode->i_mode)) {
		dec_link = -1;
		if (!new_inode)
			inc_link = 1;
	}

	new_pidir = nova_get_inode(sb, new_dir);
	old_pidir = nova_get_inode(sb, old_dir);

	old_pi = nova_get_inode(sb, old_inode);
	old_inode->i_ctime = CURRENT_TIME;
	err = nova_append_link_change_entry(sb, old_pi,
						old_inode, 0, &old_pi_tail);
	if (err)
		goto out;

	if (S_ISDIR(old_inode->i_mode) && old_dir != new_dir) {
		/* My father is changed. Update .. entry */
		/* For simplicity, we use in-place update and journal it */
		change_parent = 1;
		head_addr = (char *)nova_get_block(sb, old_pi->log_head);
		father_entry = (struct nova_dentry *)(head_addr +
					NOVA_DIR_LOG_REC_LEN(1));
		if (le64_to_cpu(father_entry->ino) != old_dir->i_ino)
			nova_err(sb, "%s: dir %lu parent should be %lu, "
				"but actually %lu\n", __func__,
				old_inode->i_ino, old_dir->i_ino,
				le64_to_cpu(father_entry->ino));
	}

	if (new_inode) {
		/* First remove the old entry in the new directory */
		err = nova_remove_dentry(new_dentry, 0,  0, &new_tail);
		if (err)
			goto out;
	}

	/* link into the new directory. */
	err = nova_add_dentry(new_dentry, old_inode->i_ino,
				inc_link, new_tail, &new_tail);
	if (err)
		goto out;

	if (inc_link)
		inc_nlink(new_dir);

	if (old_dir == new_dir)
		old_tail = new_tail;

	err = nova_remove_dentry(old_dentry, dec_link, old_tail, &old_tail);
	if (err)
		goto out;

	if (dec_link < 0)
		drop_nlink(old_dir);

	if (new_inode) {
		new_pi = nova_get_inode(sb, new_inode);
		new_inode->i_ctime = CURRENT_TIME;

		if (S_ISDIR(old_inode->i_mode)) {
			if (new_inode->i_nlink)
				drop_nlink(new_inode);
		}
		if (new_inode->i_nlink)
			drop_nlink(new_inode);

		err = nova_append_link_change_entry(sb, new_pi,
						new_inode, 0, &new_pi_tail);
		if (err)
			goto out;
	}

	entries = 1;
	memset(&entry, 0, sizeof(struct nova_lite_journal_entry));

	entry.addrs[0] = (u64)nova_get_addr_off(sbi, &old_pi->log_tail);
	entry.addrs[0] |= (u64)8 << 56;
	entry.values[0] = old_pi->log_tail;

	entry.addrs[1] = (u64)nova_get_addr_off(sbi, &old_pidir->log_tail);
	entry.addrs[1] |= (u64)8 << 56;
	entry.values[1] = old_pidir->log_tail;

	if (old_dir != new_dir) {
		entry.addrs[2] = (u64)nova_get_addr_off(sbi,
						&new_pidir->log_tail);
		entry.addrs[2] |= (u64)8 << 56;
		entry.values[2] = new_pidir->log_tail;

		if (change_parent && father_entry) {
			entry.addrs[3] = (u64)nova_get_addr_off(sbi,
						&father_entry->ino);
			entry.addrs[3] |= (u64)8 << 56;
			entry.values[3] = father_entry->ino;
		}
	}

	if (new_inode) {
		entries++;
		memset(&entry1, 0, sizeof(struct nova_lite_journal_entry));

		entry1.addrs[0] = (u64)nova_get_addr_off(sbi,
						&new_pi->log_tail);
		entry1.addrs[0] |= (u64)8 << 56;
		entry1.values[0] = new_pi->log_tail;

		if (!new_inode->i_nlink) {
			entry1.addrs[1] = (u64)nova_get_addr_off(sbi,
							&new_pi->valid);
			entry1.addrs[1] |= (u64)1 << 56;
			entry1.values[1] = new_pi->valid;
		}

	}

	cpu = smp_processor_id();
	spin_lock(&sbi->journal_locks[cpu]);
	journal_tail = nova_create_lite_transaction(sb, &entry, &entry1,
							entries, cpu);

	old_pi->log_tail = old_pi_tail;
	nova_flush_buffer(&old_pi->log_tail, CACHELINE_SIZE, 0);
	old_pidir->log_tail = old_tail;
	nova_flush_buffer(&old_pidir->log_tail, CACHELINE_SIZE, 0);

	if (old_pidir != new_pidir) {
		new_pidir->log_tail = new_tail;
		nova_flush_buffer(&new_pidir->log_tail, CACHELINE_SIZE, 0);
	}

	if (change_parent && father_entry) {
		father_entry->ino = cpu_to_le64(new_dir->i_ino);
		nova_flush_buffer(father_entry, NOVA_DIR_LOG_REC_LEN(2), 0);
	}

	if (new_inode) {
		new_pi->log_tail = new_pi_tail;
		nova_flush_buffer(&new_pi->log_tail, CACHELINE_SIZE, 0);
		if (!new_inode->i_nlink) {
			new_pi->valid = 0;
			nova_flush_buffer(&new_pi->valid, CACHELINE_SIZE, 0);
		}
	}

	PERSISTENT_BARRIER();

	nova_commit_lite_transaction(sb, journal_tail, cpu);
	spin_unlock(&sbi->journal_locks[cpu]);

	NOVA_END_TIMING(rename_t, rename_time);
	return 0;
out:
	nova_err(sb, "%s return %d\n", __func__, err);
	NOVA_END_TIMING(rename_t, rename_time);
	return err;
}

struct dentry *nova_get_parent(struct dentry *child)
{
	struct inode *inode;
	struct qstr dotdot = QSTR_INIT("..", 2);
	struct nova_dentry *de = NULL;
	ino_t ino;

	nova_inode_by_name(child->d_inode, &dotdot, &de);
	if (!de)
		return ERR_PTR(-ENOENT);
	ino = le64_to_cpu(de->ino);

	if (ino)
		inode = nova_iget(child->d_inode->i_sb, ino);
	else
		return ERR_PTR(-ENOENT);

	return d_obtain_alias(inode);
}

const struct inode_operations nova_dir_inode_operations = {
	.create		= nova_create,
	.lookup		= nova_lookup,
	.link		= nova_link,
	.unlink		= nova_unlink,
	.symlink	= nova_symlink,
	.mkdir		= nova_mkdir,
	.rmdir		= nova_rmdir,
	.mknod		= nova_mknod,
	.rename		= nova_rename,
	.setattr	= nova_notify_change,
	.get_acl	= NULL,
};

const struct inode_operations nova_special_inode_operations = {
	.setattr	= nova_notify_change,
	.get_acl	= NULL,
};
