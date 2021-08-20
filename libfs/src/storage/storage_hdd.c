#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "mlfs/mlfs_user.h"
#include "global/global.h"

static int fd;
static struct stat f_stat[g_n_devices + 1];

uint8_t *hdd_init(uint8_t dev, char *dev_path)
{
	int ret;

	if (lstat(dev_path, &f_stat[dev]) == ENOENT) {
		ret = creat(dev_path, 0666);
		if (ret < 0) {
			perror("cannot create mlfs hdd file\n");
			exit(-1);
		}
	}

	fd = open(dev_path, O_RDWR, 0600);
	if (fd < 0) {
		perror("cannot open the storage file\n");
		exit(-1);
	}

	mlfs_debug("%s\n", "open the storage file");
	ret = fstat(fd, &f_stat[dev]);
	if (ret < 0) {
		perror("cannot stat the storage file\n");
		exit(-1);
	}

	printf("hdd engine is initialized %s\n", dev_path);

	return 0;
}

int hdd_read(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size)
{
	int ret;
	ret = pread(fd, buf, io_size, blockno << g_block_size_shift);

	return ret;
}

int hdd_write(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size)
{
	int ret;
	ret = pwrite(fd, buf, io_size, blockno << g_block_size_shift);

	return ret;
}

int hdd_erase(uint8_t dev, addr_t blockno, uint32_t io_size)
{
	return 0;
}

//int hdd_commit(uint8_t fd, addr_t na1, uint32_t na2, uint32_t na3, int na4)
int hdd_commit(uint8_t fd)
{
	fdatasync(fd);
	
	return 0;
}

void hdd_exit(uint8_t dev)
{
	close(fd);

	return;
}
