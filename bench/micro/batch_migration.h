#ifndef __batch_migration_h__
#define __batch_migration_h__

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
 *  PROTOTYPES  
 */
int     bb_open(const char *pathname, int flags, mode_t mode);
ssize_t bb_write(int fd, const void *buf, size_t count);
ssize_t bb_read(int fd, void *buf, size_t count);
off_t   bb_lseek(int fd, off_t offset, int whence);
ssize_t bb_pread(int fd, void *buf, size_t count, off_t offset);
ssize_t bb_fsync(int fd);
int     bb_close(int fd);


#ifdef __cplusplus
}
#endif



#endif
