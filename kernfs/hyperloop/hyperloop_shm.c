#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "global/defs.h"

#define SHM_PATH "/hyperloop_server_shm"

void* create_hyperloop_server_shm(size_t size)
{
    void* addr;
    int fd, ret;

    fd = shm_open(SHM_PATH, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0)
	panic("shm_open of hyperloop server shm failed.");

    ret = ftruncate(fd, size);
    if (ret < 0)
	panic("ftruncate of hyperloop server shm failed.");

    addr = mmap(NULL, size, PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
	panic("mmap of hyperloop server shm failed.");

    // zeroing all shm.
    memset(addr, 0, size);

    return addr;
}

void destroy_hyperloop_server_shm(void *addr, size_t size) {
    int ret, fd;
    ret = munmap(addr, size);
    if (ret < 0)
	panic("munmap of hyperloop server shm failed.");

    fd = shm_unlink(SHM_PATH);
    if (fd < 0)
	panic("unlink of hyperloop server shm failed.");
}
