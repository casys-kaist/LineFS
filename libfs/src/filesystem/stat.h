#ifndef _STAT_H_
#define _STAT_H_

#include "global/global.h"

struct fs_stat {
	short type;		// Type of file
	int dev;		// File system's disk device id
	uint32_t ino;	// Inode number
	short nlink;	// Number of links to file
	uint32_t size;	// Size of file in bytes

	mlfs_time_t mtime;
	mlfs_time_t ctime;
	mlfs_time_t atime;
};

static inline void mlfs_get_time(mlfs_time_t *t)
{
	//gettimeofday(t, NULL);
	return;
}

#endif
