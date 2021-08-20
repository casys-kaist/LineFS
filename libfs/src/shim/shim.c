#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <syscall.h>
#include <errno.h>
#include <fcntl.h>

#include <mlfs/mlfs_interface.h>
#include <posix/posix_interface.h>
#include <global/types.h>
#include <libsyscall_intercept_hook_point.h>

#define FD_START 1000000 //should be consistent with FD_START in libfs/param.h


#define PATH_BUF_SIZE 4095
#define MLFS_PREFIX (char *)"/mlfs"
#define syscall_trace(...)

#ifdef __cplusplus
// extern "C" {
#endif

static inline int check_mlfs_fd(int fd)
{
  if (fd >= FD_START)
    return 1;
  else
    return 0;
}

static inline int get_mlfs_fd(int fd)
{
  if (fd >= FD_START)
    return fd - FD_START;
  else
    return fd;
}

static int collapse_name(const char *input, char *_output)
{
  char *output = _output;

  while(1) {
    /* Detect a . or .. component */
    if (input[0] == '.') {
      if (input[1] == '.' && input[2] == '/') {
        /* A .. component */
        if (output == _output)
          return -1;
        input += 2;
        while (*(++input) == '/');
        while(--output != _output && *(output - 1) != '/');
        continue;
      } else if (input[1] == '/') {
        /* A . component */
        input += 1;
        while (*(++input) == '/');
        continue;
      }
    }

    /* Copy from here up until the first char of the next component */
    while(1) {
      *output++ = *input++;
      if (*input == '/') {
        *output++ = '/';
        /* Consume any extraneous separators */
        while (*(++input) == '/');
        break;
      } else if (*input == 0) {
        *output = 0;
        return output - _output;
      }
    }
  }
}

int shim_do_open(char *filename, int flags, mode_t mode, int* result)
{
  int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(filename, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    return 1;
  } else {
    ret = mlfs_posix_open(filename, flags, mode);

    if (!check_mlfs_fd(ret)) {
      printf("incorrect fd %d: file %s\n", ret, filename);
    }

    syscall_trace(__func__, ret, 3, filename, flags, mode);

    *result = ret;
    return 0;
  }
}

int shim_do_openat(int dfd, const char *filename, int flags, mode_t mode, int* result)
{
  int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(filename, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    //printf("%s : will not go to libfs\n", path_buf);
    return 1;
  } else {
    //printf("%s -> MLFS \n", path_buf);

    if (dfd != AT_FDCWD) {
      fprintf(stderr, "Only support AT_FDCWD\n");
      exit(-1);
    }

    ret = mlfs_posix_open((char *)filename, flags, mode);

    if (!check_mlfs_fd(ret)) {
      printf("incorrect fd %d: file %s\n", ret, filename);
    }

    syscall_trace(__func__, ret, 4, filename, dfd, flags, mode);

    *result = ret;
    return 0;
  }
}

int shim_do_creat(char *filename, mode_t mode, int* result)
{
  int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(filename, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    return 1;
  } else {
    ret = mlfs_posix_creat(filename, mode);

    if (!check_mlfs_fd(ret)) {
      printf("incorrect fd %d\n", ret);
    }

    syscall_trace(__func__, ret, 2, filename, mode);

    *result = ret;
    return 0;
  }
}

int shim_do_read(int fd, void *buf, size_t count, size_t* result)
{
  size_t ret;

  if (check_mlfs_fd(fd)) {
    ret = mlfs_posix_read(get_mlfs_fd(fd), buf, count);
    syscall_trace(__func__, ret, 3, fd, buf, count);

    *result = ret;
    return 0;
  } else {
    return 1;
  }
}

int shim_do_pread64(int fd, void *buf, size_t count, loff_t off, size_t* result)
{
  size_t ret;

  if (check_mlfs_fd(fd)) {
    ret = mlfs_posix_pread64(get_mlfs_fd(fd), buf, count, off);
    syscall_trace(__func__, ret, 4, fd, buf, count, off);

    *result = ret;
    return 0;
  } else {
    return 1;
  }
}

int shim_do_write(int fd, void *buf, size_t count, size_t* result)
{
  size_t ret;

  if (check_mlfs_fd(fd)) {
    ret = mlfs_posix_write(get_mlfs_fd(fd), buf, count);
    syscall_trace(__func__, ret, 3, fd, buf, count);

    *result = ret;
    return 0;
  } else {
    return 1;
  }

}

int shim_do_pwrite64(int fd, void *buf, size_t count, loff_t off, size_t* result)
{
  size_t ret;

  if (check_mlfs_fd(fd)) {
    /*
    ret = mlfs_posix_pwrite64(get_mlfs_fd(fd), buf, count, off);
    syscall_trace(__func__, ret, 4, fd, buf, count, off);

    *result = ret;
    */
    printf("%s: does not support yet\n", __func__);
    exit(-1);
  } else {
    return 1;
  }

}

int shim_do_close(int fd, int* result)
{
  int ret;

  if (check_mlfs_fd(fd)) {
    ret = mlfs_posix_close(get_mlfs_fd(fd));
    syscall_trace(__func__, ret, 1, fd);

    *result = ret;
    return 0;
  } else {
    return 1;
  }
}

int shim_do_lseek(int fd, off_t offset, int origin, int* result)
{
  int ret;

  if (check_mlfs_fd(fd)) {
    ret = mlfs_posix_lseek(get_mlfs_fd(fd), offset, origin);
    syscall_trace(__func__, ret, 3, fd, offset, origin);

    *result = ret;
    return 0;
  } else {
    return 1;
  }

}

int shim_do_mkdir(void *path, mode_t mode, int* result)
{
  int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name((char *)path, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    return 1;
  } else {
    //printf("%s: go to mlfs\n", path_buf);
    ret = mlfs_posix_mkdir(path_buf, mode);
    syscall_trace(__func__, ret, 2, path, mode);

    *result = ret;
    return 0;
  }

}

int shim_do_rmdir(const char *path, int* result)
{
  int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(path, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    return 1;
  } else {
    ret = mlfs_posix_rmdir((char *)path);
    *result = ret;
    return 0;
  }

}

int shim_do_rename(char *oldname, char *newname, int* result)
{
  int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(oldname, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    return 1;
  } else {
    ret = mlfs_posix_rename(oldname, newname);
    syscall_trace(__func__, ret, 2, oldname, newname);

    *result = ret;
    return 0;
  }

}

int shim_do_fallocate(int fd, int mode, off_t offset, off_t len, int* result)
{
  int ret;

  if (check_mlfs_fd(fd)) {
    ret = mlfs_posix_fallocate(get_mlfs_fd(fd), offset, len);
    syscall_trace(__func__, ret, 4, fd, mode, offset, len);

    *result = ret;
    return 0;
  } else {
    return 1;
  }

}

int shim_do_stat(const char *filename, struct stat *statbuf, int* result)
{
  int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(filename, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    return 1;
  } else {
    ret = mlfs_posix_stat(filename, statbuf);
    syscall_trace(__func__, ret, 2, filename, statbuf);

    *result = ret;
    return 0;
  }

}

int shim_do_lstat(const char *filename, struct stat *statbuf, int* result)
{
  int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(filename, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    return 1;
  } else {
    // Symlink does not implemented yet
    // so stat and lstat is identical now.
    ret = mlfs_posix_stat(filename, statbuf);
    syscall_trace(__func__, ret, 2, filename, statbuf);

    *result = ret;
    return 0;
  }

}

int shim_do_fstat(int fd, struct stat *statbuf, int* result)
{
  int ret;

  if (check_mlfs_fd(fd)) {
    ret = mlfs_posix_fstat(get_mlfs_fd(fd), statbuf);
    syscall_trace(__func__, ret, 2, fd, statbuf);

    *result = ret;
    return 0;
  } else {
    return 1;
  }
}

int shim_do_truncate(const char *filename, off_t length, int* result)
{
  int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(filename, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    return 1;
  } else {
    ret = mlfs_posix_truncate(filename, length);
    syscall_trace(__func__, ret, 2, filename, length);

    *result = ret;
    return 0;
  }

}

int shim_do_ftruncate(int fd, off_t length, int* result)
{
  int ret;

  if (check_mlfs_fd(fd)) {
    ret = mlfs_posix_ftruncate(get_mlfs_fd(fd), length);
    syscall_trace(__func__, ret, 2, fd, length);

    *result = ret;
    return 0;
  } else {
    return 1;
  }

}

int shim_do_unlink(const char *path, int* result)
{
  int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(path, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    return 1;
  } else {
    ret = mlfs_posix_unlink(path);
    syscall_trace(__func__, ret, 1, path);
    *result = ret;
    return 0;
  }

}

int shim_do_symlink(const char *target, const char *linkpath, int* result)
{
  int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(target, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    return 1;
  } else {
    printf("%s\n", target);
    printf("symlink: do not support yet\n");
    exit(-1);
  }

}

int shim_do_access(const char *pathname, int mode, int* result)
{
  int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(pathname, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    return 1;
  } else {
    ret = mlfs_posix_access((char *)pathname, mode);
    syscall_trace(__func__, ret, 2, pathname, mode);

    *result = ret;
    return 0;
  }

}

int shim_do_fsync(int fd, int* result)
{
  int ret;

  if (check_mlfs_fd(fd)) {
    //FIXME: temporarily override fsyncs to perform rsyncs
    //for assisse
    //printf("Intercepting fsync call and overriding with rsync\n");
    //ret = mlfs_do_rsync();
    ret = mlfs_posix_fsync(fd);
    syscall_trace(__func__, ret, 0);
    *result = ret;
    return 0;
  } else {
    return 1;
  }
}

int shim_do_fdatasync(int fd, int* result)
{
  int ret;

  if (check_mlfs_fd(fd)) {
    // fdatasync is nop.
    syscall_trace(__func__, 0, 1, fd);

    *result = 0;
    return 0;
  } else {
    return 1;
  }

}

int shim_do_sync(int* result)
{
  int ret;

  printf("sync: do not support yet\n");
  exit(-1);
}

int shim_do_fcntl(int fd, int cmd, void *arg, int* result)
{
  int ret;

  if (check_mlfs_fd(fd)) {
    ret = mlfs_posix_fcntl(get_mlfs_fd(fd), cmd, arg);
    syscall_trace(__func__, ret, 3, fd, cmd, arg);

    *result = ret;
    return 0;
  } else {
    return 1;
  }

}

int shim_do_mmap(void *addr, size_t length, int prot,
                   int flags, int fd, off_t offset, void** result)
{
  void* ret;

  if (check_mlfs_fd(fd)) {
    printf("mmap: not implemented\n");
    exit(-1);
  } else {
    return 1;
  }

}

int shim_do_munmap(void *addr, size_t length, int* result)
{
  return 1;

}

int shim_do_getdents(int fd, struct linux_dirent *buf, size_t count, size_t* result)
{
  size_t ret;

  if (check_mlfs_fd(fd)) {
    ret = mlfs_posix_getdents(get_mlfs_fd(fd), buf, count, 0);

    syscall_trace(__func__, ret, 3, fd, buf, count);

    *result = ret;
    return 0;
  } else {
    return 1;
  }

}

int shim_do_getdents64(int fd, struct linux_dirent64 *buf, size_t count, size_t* result)
{
  size_t ret;

  if (check_mlfs_fd(fd)) {
    ret = mlfs_posix_getdents64(get_mlfs_fd(fd), buf, count, 0);

    syscall_trace(__func__, ret, 3, fd, buf, count);

    *result = ret;
    return 0;
  } else {
    return 1;
  }

}


static int
hook(long syscall_number,
     long arg0, long arg1,
     long arg2, long arg3,
     long arg4, long arg5,
     long *result) {
  switch (syscall_number) {
    case SYS_open: return shim_do_open((char*)arg0, (int)arg1, (mode_t)arg2, (int*)result);
    case SYS_openat: return shim_do_openat((int)arg0, (const char*)arg1, (int)arg2, (mode_t)arg3, (int*)result);
    case SYS_creat: return shim_do_creat((char*)arg0, (mode_t)arg1, (int*)result);
    case SYS_read: return shim_do_read((int)arg0, (void*)arg1, (size_t)arg2, (size_t*)result);
    case SYS_pread64: return shim_do_pread64((int)arg0, (void*)arg1, (size_t)arg2, (loff_t)arg3, (size_t*)result);
    case SYS_write: return shim_do_write((int)arg0, (void*)arg1, (size_t)arg2, (size_t*)result);
    case SYS_pwrite64: return shim_do_pwrite64((int)arg0, (void*)arg1, (size_t)arg2, (loff_t)arg3, (size_t*)result);
    case SYS_close: return shim_do_close((int)arg0, (int*)result);
    case SYS_lseek: return shim_do_lseek((int)arg0, (off_t)arg1, (int)arg2, (int*)result);
    case SYS_mkdir: return shim_do_mkdir((void*)arg0, (mode_t)arg1, (int*)result);
    case SYS_rmdir: return shim_do_rmdir((const char*)arg0, (int*)result);
    case SYS_rename: return shim_do_rename((char*)arg0, (char*)arg1, (int*)result);
    case SYS_fallocate: return shim_do_fallocate((int)arg0, (int)arg1, (off_t)arg2, (off_t)arg3, (int*)result);
    case SYS_stat: return shim_do_stat((const char*)arg0, (struct stat*)arg1, (int*)result);
    case SYS_lstat: return shim_do_lstat((const char*)arg0, (struct stat*)arg1, (int*)result);
    case SYS_fstat: return shim_do_fstat((int)arg0, (struct stat*)arg1, (int*)result);
    case SYS_truncate: return shim_do_truncate((const char*)arg0, (off_t)arg1, (int*)result);
    case SYS_ftruncate: return shim_do_ftruncate((int)arg0, (off_t)arg1, (int*)result);
    case SYS_unlink: return shim_do_unlink((const char*)arg0, (int*)result);
    case SYS_symlink: return shim_do_symlink((const char*)arg0, (const char*)arg1, (int*)result);
    case SYS_access: return shim_do_access((const char*)arg0, (int)arg1, (int*)result);
    case SYS_fsync: return shim_do_fsync((int)arg0, (int*)result);
    case SYS_fdatasync: return shim_do_fdatasync((int)arg0, (int*)result);
    case SYS_sync: return shim_do_sync((int*)result);
    case SYS_fcntl: return shim_do_fcntl((int)arg0, (int)arg1, (void*)arg2, (int*)result);
    case SYS_mmap: return shim_do_mmap((void*)arg0, (size_t)arg1, (int)arg2, (int)arg3, (int)arg4, (off_t)arg5, (void**)result);
    case SYS_munmap: return shim_do_munmap((void*)arg0, (size_t)arg1, (int*)result);
    case SYS_getdents: return shim_do_getdents((int)arg0, (struct linux_dirent*)arg1, (size_t)arg2, (size_t*)result);
    case SYS_getdents64: return shim_do_getdents64((int)arg0, (struct linux_dirent64*)arg1, (size_t)arg2, (size_t*)result);
  }
  return 1;
}

static __attribute__((constructor)) void init(void)
{
  // Set up the callback function
  intercept_hook_point = hook;
}


#ifdef __cplusplus
// }
#endif
