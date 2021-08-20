#include "filesystem/shared.h"

#ifdef LIBFS
#include "filesystem/fs.h"
#endif

#ifndef _POSIX_WRAPPER_H_
#define _POSIX_WRAPPER_H_

int mlfs_rpc_read(int fd, struct mlfs_reply *reply, size_t count);
int mlfs_rpc_pread64(int fd, struct mlfs_reply *reply, size_t count, loff_t off);

#endif
