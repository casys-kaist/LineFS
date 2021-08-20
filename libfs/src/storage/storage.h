#ifndef _STORAGE_H_
#define _STORAGE_H_

#include "global/global.h"

struct block_device *g_bdev[g_n_devices + 1];

// Interface for different storage engines.
struct storage_operations
{
	uint8_t *(*init)(uint8_t dev, char *dev_path);
	int (*read)(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size);
	int (*read_unaligned)(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t offset,
			uint32_t io_size);
	int (*write)(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size);
	int (*write_unaligned)(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t offset,
			uint32_t io_size);
	int (*write_opt)(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size);
	int (*write_opt_unaligned)(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t offset,
			uint32_t io_size);
	int (*erase)(uint8_t dev, addr_t blockno, uint32_t io_size);
	//int (*commit)(uint8_t dev, addr_t blockno, uint32_t offset, uint32_t io_size,
	//		int flags);
	int (*commit)(uint8_t dev);
	int (*persist)(uint8_t dev, addr_t blockno, uint32_t offset, uint32_t io_size);
	int (*wait_io)(uint8_t dev, int isread);
	int (*readahead)(uint8_t dev, addr_t blockno, uint32_t io_size);
	void (*exit)(uint8_t dev);
};

#ifdef __cplusplus
extern "C" {
#endif

/*
 To get the dev-dax size,
 cat /sys/devices/platform/e820_pmem/ndbus0/region0/size
 1: g_root_dev
 2: g_ssd_dev
 3: g_hdd_dev
 4~ per-application log device
*/
// setting 1 - debugging. 5 GB, 20 GB, 30 GB
//static uint64_t dev_size[g_n_devices + 1] = {0, 5282725888UL, 26422018048UL, 51539607552UL, 4294967296UL};
//static uint64_t dev_size[g_n_devices + 1] = {0, 5282725888UL, 26422018048UL, 51539607552UL, 4294967296UL};
//static uint64_t dev_size[g_n_devices + 1] =   {0, 3221225472UL, 5282725888UL * 16UL, 5282725888UL, 3221225472UL, 3221225472UL, 3221225472UL, 3221225472UL};

// setting 2 - production. 20GB, 120GB, 180GB, 2GB
//static uint64_t dev_size[g_n_devices + 1] = {0, 21474836480UL, 128849018880UL, 193273528320UL, 2147483648UL};

// setting 2 - rsync testing. 20GB, 120GB, 180GB, 650MB
//static uint64_t dev_size[g_n_devices + 1] = {0, 21474836480UL, 128849018880UL, 193273528320UL, 681574400UL};

// setting 3 - one application
//static uint64_t dev_size[g_n_devices + 1] = {0, 21474836480UL, 128849018880UL, 193273528320UL, 891289600UL};

// setting 4 - two applications
//static uint64_t dev_size[g_n_devices + 1] = {0, 21474836480UL, 128849018880UL, 193273528320UL, 1761607680UL, 1761607680UL};
//static uint64_t dev_size[g_n_devices + 1] = {0, 21474836480UL, 128849018880UL, 193273528320UL, 21474836480UL, 21474836480UL};

// setting 5 - four applications
//static uint64_t dev_size[g_n_devices + 1] = {0, 21474836480UL, 128849018880UL, 193273528320UL, 7046430720UL, 1761607680UL, 1761607680UL, 1761607680UL};
//static uint64_t dev_size[g_n_devices + 1] = {0, 214748364800UL, 128849018880UL, 193273528320UL, 21474836480UL, 21474836480UL};

//static uint64_t dev_size[g_n_devices + 1] = {0, 42949672960UL, 128849018880UL, 193273528320UL, 5368709120UL, 5368709120UL};
//
// 1GB
// static uint64_t dev_size[g_n_devices + 1] = {0, 1024*1024*1024UL, 5368709120UL, 5368709120UL, 1024*1024*1024UL};

// 5GB
// static uint64_t dev_size[g_n_devices + 1] = {0, 5368709120UL, 5368709120UL, 5368709120UL, 5368709120UL};
//
// 14GB
// static uint64_t dev_size[g_n_devices + 1] = {0, 14*1024*1024*1024UL, 5368709120UL, 5368709120UL, 14*1024*1024*1024UL};

// 28GB
// static uint64_t dev_size[g_n_devices + 1] = {0, 28*1024*1024*1024UL, 5368709120UL, 5368709120UL, 14*1024*1024*1024UL};

// 20GB
// static uint64_t dev_size[g_n_devices + 1] = {0, 20*1024*1024*1024UL, 5368709120UL, 5368709120UL, 20*1024*1024*1024UL};

// 19GB
static uint64_t dev_size[g_n_devices + 1] = {0, 19*1024*1024*1024UL, 5368709120UL, 5368709120UL, 19*1024*1024*1024UL};
//
//
// 32GB
// static uint64_t dev_size[g_n_devices + 1] = {0, 32*1024*1024*1024UL, 5368709120UL, 5368709120UL, 32*1024*1024*1024UL};
// //
// 40GB
// static uint64_t dev_size[g_n_devices + 1] = {0, 40*1024*1024*1024UL, 5368709120UL, 5368709120UL, 40*1024*1024*1024UL};

// 48GB
// static uint64_t dev_size[g_n_devices + 1] = {0, 48*1024*1024*1024UL, 5368709120UL, 5368709120UL, 48*1024*1024*1024UL};
// 64GB
// static uint64_t dev_size[g_n_devices + 1] = {0, 64*1024*1024*1024UL, 5368709120UL, 5368709120UL, 64*1024*1024*1024UL};
//
// 56GB
// static uint64_t dev_size[g_n_devices + 1] = {0, 56*1024*1024*1024UL, 5368709120UL, 5368709120UL, 56*1024*1024*1024UL};

// 15GB
// static uint64_t dev_size[g_n_devices + 1] = {0, 15*1024*1024*1024UL, 5368709120UL, 5368709120UL, 15*1024*1024*1024UL};
// 1GB
// static uint64_t dev_size[g_n_devices + 1] = {0, 1024*1024*1024UL, 1024*1024*1024UL, 1024*1024*1024UL, 1024*1024*1024UL, 1024*1024*1024UL};

//static uint64_t dev_size[g_n_devices + 1] = {0, 21474836480UL, 5368709120UL, 5368709120UL, 5368709120UL, 5368709120UL};

//static uint64_t dev_size[g_n_devices + 1] = {0, 5368709120UL, 5368709120UL, 5368709120UL, 26843545600UL, 26843545600UL};

// setting 6. 40 GB, 120 GB, 300 GB
// static uint64_t dev_size[g_n_devices + 1] = {0, 36507222016UL, 128849018880UL, 193273528320UL, 4294967296UL};
// static uint64_t dev_size[g_n_devices + 1] = {0, 36507222016UL, 128849018880UL, 193273528320UL, 2147483648UL};
//static uint64_t dev_size[g_n_devices + 1] = {0, 36507222016UL, 128849018880UL, 193273528320UL, 1073741824UL};
// static uint64_t dev_size[g_n_devices + 1] = {0, 36507222016UL, 128849018880UL, 193273528320UL, 524288000UL};
// static uint64_t dev_size[g_n_devices + 1] = {0, 36507222016UL, 128849018880UL, 193273528320UL, 104857600UL};

// setting 7: for two applications.
//static uint64_t dev_size[g_n_devices + 1] =
//{0, 34877734912UL, 150849018880UL, 193273528320UL, 5282725888UL, 5282725888UL}; //Debugging for levelDB
//static uint64_t dev_size[g_n_devices + 1] =
//	{0, 34877734912UL, 150849018880UL, 193273528320UL, 4294967296UL, 4294967296UL};


enum pmem_persist_options {
  PMEM_PERSIST_FLUSH = 1,   //flush cache line
  PMEM_PERSIST_DRAIN = 2,   //drain hw queues
  PMEM_PERSIST_ORDERED = 4, //enforce ordering
};

//extern struct storage_operations storage_ramdisk; // not tested.
extern struct storage_operations storage_dax;
extern struct storage_operations storage_hdd;
extern struct storage_operations storage_aio;
extern struct storage_operations storage_nic_rpc;

// ramdisk
uint8_t *ramdisk_init(uint8_t dev, char *dev_path);
int ramdisk_read(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size);
int ramdisk_write(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size);
int ramdisk_erase(uint8_t dev, uint32_t blockno, addr_t io_size);
void ramdisk_exit(uint8_t dev);

// pmem
uint8_t *pmem_init(uint8_t dev, char *dev_path);
int pmem_read(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size);
int pmem_write(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size);
int pmem_write_unaligned(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t offset,
    uint32_t io_size);
int pmem_erase(uint8_t dev, addr_t blockno, uint32_t io_size);
void pmem_exit(uint8_t dev);

// pmem-dax
uint8_t *dax_init(uint8_t dev, char *dev_path);
int dax_read(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size);
int dax_read_unaligned(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t offset,
    uint32_t io_size);
int dax_write(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size);
int dax_write_unaligned(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t offset,
		uint32_t io_size);
int dax_write_opt(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size);
int dax_write_opt_unaligned(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t offset,
		uint32_t io_size);
int dax_erase(uint8_t dev, addr_t blockno, uint32_t io_size);
//int dax_commit(uint8_t dev, addr_t blockno, uint32_t offset, uint32_t io_size,
//    int flags);
int dax_commit(uint8_t dev);
int dax_persist(uint8_t dev, addr_t blockno, uint32_t offset, uint32_t io_size);
void dax_exit(uint8_t dev);

// HDD
uint8_t *hdd_init(uint8_t dev, char *dev_path);
int hdd_read(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size);
int hdd_write(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size);
//int hdd_commit(uint8_t fd, addr_t na1, uint32_t na2, uint32_t na3, int na4);
int hdd_commit(uint8_t dev);
int hdd_erase(uint8_t dev, addr_t blockno, uint32_t io_size);
int hdd_wait_io(uint8_t dev);
int hdd_readahead(uint8_t dev, addr_t blockno, uint32_t io_size);
void hdd_exit(uint8_t dev);

// AIO
uint8_t *mlfs_aio_init(uint8_t dev, char *dev_path);
int mlfs_aio_read(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size);
int mlfs_aio_write(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size);
//int mlfs_aio_commit(uint8_t dev, addr_t blockno, uint32_t offset, uint32_t io_size, int flags);
int mlfs_aio_commit(uint8_t dev);
int mlfs_aio_erase(uint8_t dev, addr_t blockno, uint32_t io_size);
int mlfs_aio_wait_io(uint8_t dev, int read);
int mlfs_aio_erase(uint8_t dev, addr_t blockno, uint32_t io_size);
int mlfs_aio_readahead(uint8_t dev, addr_t blockno, uint32_t io_size);
void mlfs_aio_exit(uint8_t dev);

// NIC RPC
extern ncx_slab_pool_t *nic_slab_pool;
extern struct mr_context *remote_mrs;
extern int remote_n_regions;
void set_remote_mr(addr_t start_addr, uint64_t len, int region_id);
uint8_t* nic_rpc_init(uint8_t dev, char *dev_path);
int nic_rpc_read_and_get(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size);
int nic_rpc_read_and_get_unaligned(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t offset,
    uint32_t io_size);
int nic_rpc_send_and_write(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size);
int nic_rpc_send_and_write_unaligned(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t offset,
		uint32_t io_size);
int nic_rpc_write_opt_local(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size);
int nic_rpc_write_opt_local_unaligned(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t offset,
		uint32_t io_size);
int nic_rpc_erase(uint8_t dev, addr_t blockno, uint32_t io_size);
int nic_rpc_commit(uint8_t dev);
void nic_rpc_exit(uint8_t dev);
void nic_slab_init(uint64_t pool_size);
void *nic_slab_alloc_in_blk(uint64_t block_cnt);
void *nic_slab_alloc_in_byte(uint64_t bytes);
void nic_slab_free(void *p);
void nic_slab_print_stat(void);
size_t get_nic_slab_used_pct(void);

extern uint64_t *bandwidth_consumption;

#ifdef __cplusplus
}
#endif

#endif
