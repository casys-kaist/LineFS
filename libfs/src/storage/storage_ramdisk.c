#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "mlfs/mlfs_user.h"
#include "global/global.h"

static int fd;
static uint8_t *base_addr[g_n_devices + 1];
static struct stat f_stat[g_n_devices + 1];

uint8_t *ramdisk_init(uint8_t dev, char *dev_path)
{
	int ret;

	fd = open(dev_path, O_RDWR);

	if (fd < 0) {
		perror("cannot open file system file\n");
		exit(-1);
	}

	mlfs_debug("%s\n", "open file system file");

	ret = fstat(fd, &f_stat[dev]);
	if (ret < 0) {
		perror("cannot stat file system file\n");
		exit(-1);
	}

	base_addr[dev] = (uint8_t *)mmap(NULL, f_stat[dev].st_size, PROT_READ | PROT_WRITE,
			MAP_SHARED| MAP_POPULATE, fd, 0);

	if (base_addr[dev] == MAP_FAILED) {
		perror("cannot map file system file\n");
		exit(-1);
	}

	printf("ramdisk engine is intialized\n");

	return (uint8_t *)base_addr[dev];
}

int ramdisk_read(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size)
{
	memcpy(buf, base_addr[dev] + (blockno * g_block_size_bytes), io_size);

	//mlfs_debug("read block number %d\n", blockno);

	return 0;
}

int ramdisk_write(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size)
{
	memcpy(base_addr[dev] + (blockno * g_block_size_bytes), buf, io_size);

	//mlfs_debug("write block number %d\n", b->blockno);
	return 0;
}

int ramdisk_erase(uint8_t dev, addr_t blockno, uint32_t io_size)
{
	return 0;
}

void ramdisk_exit(uint8_t dev)
{
	munmap(base_addr[dev], f_stat[dev].st_size);

	return;
}
