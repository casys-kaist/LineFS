#ifndef _FILE_H_
#define _FILE_H_

#include "filesystem/stat.h"
#include "filesystem/shared.h"
#include "filesystem/fs.h"
#include "global/global.h"

typedef enum { FD_NONE, FD_PIPE, FD_INODE, FD_DIR } fd_type_t;
struct file {
	fd_type_t type;
	int fd;
	int ref; // reference count
	uint8_t readable;
	uint8_t writable;
	struct inode *ip;
	offset_t off;

	//useful for issuing remote reads without having to do a path lookup
	char path[MAX_PATH];
	mlfs_hash_t hh;

	pthread_rwlock_t rwlock;
};

// opened file table, indexed by file descriptor
struct open_file_table {
	pthread_spinlock_t lock;
	struct file open_files[g_max_open_files]; //fd to open file mapping
	struct file *open_files_ht; //path to open file mapping
};

extern struct open_file_table g_fd_table;

#define CONSOLE 1

// APIs
void mlfs_file_init(void);
struct file *mlfs_file_alloc(void);
struct file *mlfs_file_dup(struct file *f);

int mlfs_file_close(struct file *f);
struct inode *mlfs_object_create(char *path, unsigned short mode);

int mlfs_file_stat(struct file *f, struct stat *st);
ssize_t mlfs_file_read(struct file *f, struct mlfs_reply *reply, size_t n);
int mlfs_file_read_offset(struct file *f, struct mlfs_reply *reply, 
		size_t n, offset_t off);
int mlfs_file_write(struct file *f, uint8_t *buf, size_t n);

#endif
