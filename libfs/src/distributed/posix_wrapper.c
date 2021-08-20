#ifdef LIBFS
#include "distributed/posix_wrapper.h"
#include "global/global.h"
#include "posix/posix_interface.h"
#include "filesystem/file.h"

//modified posix for performing reads in response to rpcs (as opposed to system calls)
int mlfs_rpc_read(int fd, struct mlfs_reply *reply, size_t count)
{
	int ret = 0;
	struct file *f;

	fd = GET_MLFS_FD(fd);

	f = &g_fd_table.open_files[fd];

	mlfs_debug("opening fd: %d | ref: %d\n", fd, (&g_fd_table.open_files[fd])->ref);

	//pthread_rwlock_rdlock(&f->rwlock);

	mlfs_assert(f);

	if (f->ref == 0) {
		panic("file descriptor is wrong\n");
		return -EBADF;
	}

	ret = mlfs_file_read(f, reply, count);

	//pthread_rwlock_unlock(&f->rwlock);

	return ret;
}

int mlfs_rpc_pread64(int fd, struct mlfs_reply *reply, size_t count, loff_t off)
{
	int ret = 0;
	struct file *f;

	fd = GET_MLFS_FD(fd);

	f = &g_fd_table.open_files[fd];

	mlfs_debug("opening fd: %d | ref: %d\n", fd, (&g_fd_table.open_files[fd])->ref);

	//pthread_rwlock_rdlock(&f->rwlock);

	mlfs_assert(f);

	if (f->ref == 0) {
		panic("file descriptor is wrong\n");
		return -EBADF;
	}

	ret = mlfs_file_read_offset(f, reply, count, off);

	//pthread_rwlock_unlock(&f->rwlock);

	return ret;
}

#endif
