#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "batch_migration.h"
#include "migration_manager.h"


static migration_manager* mm = NULL;

/*
 * POSIX overloading, actual code
 */

int bb_open(const char *pathname, int flags, mode_t mode)
{
    if (mm == NULL) {
        mm = new migration_manager(pathname, flags);
    }

    return mm->get_fd();
}

ssize_t bb_write(int fd, const void *buf, size_t count) 
{
    return mm->_write(buf, count);
}

ssize_t bb_read(int fd, void *buf, size_t count)
{
    return mm->_read(buf, count);
}

ssize_t bb_pread(int fd, void *buf, size_t count, off_t offset)
{
    mm->_lseek(offset, SEEK_SET);
    return mm->_read(buf, count);
}

off_t bb_lseek(int fd, off_t offset, int whence)
{
    return mm->_lseek(offset, whence);
}

off_t bb_fsync(int fd)
{
    return mm->_fsync();
}

int bb_close(int fd)
{
    return mm->_close();
}
