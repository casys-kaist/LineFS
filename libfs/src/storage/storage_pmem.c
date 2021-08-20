#ifndef NIC_SIDE
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <libpmem.h>

#include "mlfs/mlfs_user.h"

#include "global/global.h"

static struct stat f_stat;
static uint8_t *pmem_addr[g_n_devices + 1];
static size_t mapped_len[g_n_devices + 1];

uint8_t *pmem_init(uint8_t dev, char *dev_path)
{
	int is_pmem;

	pmem_addr[dev] = (uint8_t *)pmem_map_file(dev_path, 0,
			0,
			PROT_READ | PROT_WRITE,
			&mapped_len[dev], &is_pmem);

	if (pmem_addr[dev] == NULL || !is_pmem) {
                printf("cannot map file system file: %s\n", dev_path);
		perror("error msg");
		exit(-1);
	}

	printf("pmem engine is initialized: dev_path %s\n", dev_path);

	return pmem_addr[dev];
}

int pmem_read(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size)
{
	memcpy(buf, pmem_addr[dev] + (blockno * g_block_size_bytes), io_size);

	//mlfs_debug("read block number %d\n", blockno);

	return io_size;
}

int pmem_write(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size)
{
	//copy and flush data to pmem.
	pmem_memcpy_persist(pmem_addr[dev] + (blockno * g_block_size_bytes), buf, io_size);

	//mlfs_debug("write block number %d\n", blockno);

	return io_size;
}

int pmem_write_unaligned(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t offset, 
		uint32_t io_size)
{
	//copy and flush data to pmem.
	pmem_memcpy_persist(pmem_addr[dev] + (blockno * g_block_size_bytes) + offset, 
			buf, io_size);

	//mlfs_debug("write block number %d\n", blockno);

	return io_size;
}

int pmem_erase(uint8_t dev, addr_t blockno, uint32_t io_size)
{
	memset(pmem_addr[dev] + (blockno * g_block_size_bytes), 0, io_size);
}

void pmem_exit(uint8_t dev)
{
	pmem_unmap(pmem_addr[dev], f_stat.st_size);

	return;
}
#endif /* ! NIC_SIDE */
