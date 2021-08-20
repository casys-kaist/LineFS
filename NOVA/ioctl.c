/*
 * BRIEF DESCRIPTION
 *
 * Ioctl operations.
 *
 * Copyright 2015-2016 Regents of the University of California,
 * UCSD Non-Volatile Systems Lab, Andiry Xu <jix024@cs.ucsd.edu>
 * Copyright 2012-2013 Intel Corporation
 * Copyright 2010-2011 Marco Stornelli <marco.stornelli@gmail.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/capability.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/compat.h>
#include <linux/mount.h>
#include "nova.h"

long nova_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct address_space *mapping = filp->f_mapping;
	struct inode    *inode = mapping->host;
	struct nova_inode *pi;
	struct super_block *sb = inode->i_sb;
	unsigned int flags;
	u64 new_tail = 0;
	int ret;

	pi = nova_get_inode(sb, inode);
	if (!pi)
		return -EACCES;

	switch (cmd) {
	case FS_IOC_GETFLAGS:
		flags = le32_to_cpu(pi->i_flags) & NOVA_FL_USER_VISIBLE;
		return put_user(flags, (int __user *)arg);
	case FS_IOC_SETFLAGS: {
		unsigned int oldflags;

		ret = mnt_want_write_file(filp);
		if (ret)
			return ret;

		if (!inode_owner_or_capable(inode)) {
			ret = -EPERM;
			goto flags_out;
		}

		if (get_user(flags, (int __user *)arg)) {
			ret = -EFAULT;
			goto flags_out;
		}

		inode_lock(inode);
		oldflags = le32_to_cpu(pi->i_flags);

		if ((flags ^ oldflags) &
		    (FS_APPEND_FL | FS_IMMUTABLE_FL)) {
			if (!capable(CAP_LINUX_IMMUTABLE)) {
				inode_unlock(inode);
				ret = -EPERM;
				goto flags_out;
			}
		}

		if (!S_ISDIR(inode->i_mode))
			flags &= ~FS_DIRSYNC_FL;

		flags = flags & FS_FL_USER_MODIFIABLE;
		flags |= oldflags & ~FS_FL_USER_MODIFIABLE;
		inode->i_ctime = CURRENT_TIME_SEC;
		nova_set_inode_flags(inode, pi, flags);

		nova_memunlock_inode(sb, pi);
		ret = nova_append_link_change_entry(sb, pi, inode, 0,
							&new_tail);
		if (!ret)
			nova_update_tail(pi, new_tail);
		nova_memlock_inode(sb, pi);
		inode_unlock(inode);
flags_out:
		mnt_drop_write_file(filp);
		return ret;
	}
	case FS_IOC_GETVERSION:
		return put_user(inode->i_generation, (int __user *)arg);
	case FS_IOC_SETVERSION: {
		__u32 generation;
		if (!inode_owner_or_capable(inode))
			return -EPERM;
		ret = mnt_want_write_file(filp);
		if (ret)
			return ret;
		if (get_user(generation, (int __user *)arg)) {
			ret = -EFAULT;
			goto setversion_out;
		}
		inode_lock(inode);
		inode->i_ctime = CURRENT_TIME_SEC;
		inode->i_generation = generation;

		nova_memunlock_inode(sb, pi);
		ret = nova_append_link_change_entry(sb, pi, inode, 0,
							&new_tail);
		if (!ret)
			nova_update_tail(pi, new_tail);
		nova_memlock_inode(sb, pi);
		inode_unlock(inode);
setversion_out:
		mnt_drop_write_file(filp);
		return ret;
	}
	case NOVA_PRINT_TIMING: {
		nova_print_timing_stats(sb);
		return 0;
	}
	case NOVA_CLEAR_STATS: {
		nova_clear_stats();
		return 0;
	}
	case NOVA_PRINT_LOG: {
		nova_print_inode_log(sb, inode);
		return 0;
	}
	case NOVA_PRINT_LOG_PAGES: {
		nova_print_inode_log_pages(sb, inode);
		return 0;
	}
	case NOVA_PRINT_FREE_LISTS: {
		nova_print_free_lists(sb);
		return 0;
	}
	default:
		return -ENOTTY;
	}
}

#ifdef CONFIG_COMPAT
long nova_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case FS_IOC32_GETFLAGS:
		cmd = FS_IOC_GETFLAGS;
		break;
	case FS_IOC32_SETFLAGS:
		cmd = FS_IOC_SETFLAGS;
		break;
	case FS_IOC32_GETVERSION:
		cmd = FS_IOC_GETVERSION;
		break;
	case FS_IOC32_SETVERSION:
		cmd = FS_IOC_SETVERSION;
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return nova_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif
