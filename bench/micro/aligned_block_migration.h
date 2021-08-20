#ifndef __block_migration_h__
#define _block_migration_h__

#ifdef __cplusplus
extern "C" {
#endif

#define kb (uint32_t)1024
#define mb (uint32_t)1024*1024
#define gb (uint32_t)1024*1024*1024

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


/*
 *  PROTOTYPES  
 */
int     bm_open(const char *pathname, int flags, mode_t mode);
ssize_t bm_write(int fd, const void *buf, size_t count);
ssize_t bm_read(int fd, void *buf, size_t count);
off_t   bm_lseek(int fd, off_t offset, int whence);
ssize_t bm_pread(int fd, void *buf, size_t count, off_t offset);
ssize_t bm_fsync(int fd);

#define DEBUG 1

#define dbg(fmt, ...) \
            do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

#ifdef __cplusplus
}
#endif



#endif
