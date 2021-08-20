#include "storage/storage.h"

// It has been moved to storage.h to be used in storage_nic_rpc.c
// struct block_device *g_bdev[g_n_devices + 1];

#if 1

#if defined(NIC_OFFLOAD) && defined(NIC_SIDE)

/* Ramdisk for NIC.
 * Example command to create three 1G ramdisks:
 *      modprobe brd rd_nr=3 rd_size=1048576
 * *** In the current implementation, it is not used.
 * *** Rather, NIC uses allocated DRAM space for RDMA MRs.
 */
char *g_dev_path[] = {
	(char *)"unused",
        (char *)"/dev/ram0",
	(char *)"/backup/mlfs_ssd",
	(char *)"/backup/mlfs_hdd",
        (char *)"/dev/ram1",
        (char *)"/dev/ram2",
};

#else
char *g_dev_path[] = {
	(char *)"unused",
	(char *)"/dev/dax0.0",
	(char *)"/backup/mlfs_ssd",
	(char *)"/backup/mlfs_hdd",
	(char *)"/dev/dax0.1",
};
#endif

#else
char *g_dev_path[] = {
	(char *)"unused",
	(char *)"/dev/dax0.0",
	(char *)"/backup/mlfs_ssd",
	(char *)"/backup/mlfs_hdd",
	(char *)"/dev/dax0.1",
	(char *)"/dev/dax1.1",
	(char *)"/dev/dax0.3",
	(char *)"/dev/dax0.4",
};
#endif

#ifdef __cplusplus
// Not tested.
/*
struct storage_operations storage_ramdisk = {
	ramdisk_init,
	ramdisk_read,
	NULL,
	ramdisk_write,
	NULL,
        NULL,
        NULL,
	ramdisk_erase,
	NULL,
	NULL,
	NULL,
	ramdisk_exit,
};
*/

#ifndef NIC_SIDE
struct storage_operations storage_dax = {
	dax_init,
	dax_read,
	dax_read_unaligned,
	dax_write,
	dax_write_unaligned,
	dax_write_opt,
	dax_write_opt_unaligned,
	dax_erase,
	dax_commit,
        dax_persist,
	NULL,
	NULL,
	dax_exit,
};
#endif

struct storage_operations storage_nic_rpc = {
	nic_rpc_init,
	nic_rpc_read_and_get,
	nic_rpc_read_and_get_unaligned,
	nic_rpc_send_and_write,  //  send and write??? TODO
	nic_rpc_send_and_write_unaligned,
	nic_rpc_write_opt_local,
	nic_rpc_write_opt_local_unaligned,
	nic_rpc_erase,
	nic_rpc_commit,
        NULL,
	NULL,
	NULL,
	nic_rpc_exit,
};

struct storage_operations storage_hdd = {
	hdd_init,
	hdd_read,
	NULL,
	hdd_write,
	NULL,
	NULL,
	NULL,
	NULL,
	hdd_commit,
        NULL,
	NULL,
	NULL,
	hdd_exit,
};

struct storage_operations storage_aio = {
	mlfs_aio_init,
	mlfs_aio_read,
	NULL,
	mlfs_aio_write,
	NULL,
	NULL,
	NULL,
	mlfs_aio_commit,
        NULL,
	mlfs_aio_wait_io,
	mlfs_aio_erase,
	mlfs_aio_readahead,
	mlfs_aio_exit,
};

#else
// Not tested.
/*
struct storage_operations storage_ramdisk = {
	.init                   = ramdisk_init,
	.read                   = ramdisk_read,
	.read_unaligned         = NULL,
	.write                  = ramdisk_write,
	.write_unaligned        = NULL,
	.write_opt              = NULL,
	.write_opt_unaligned    = NULL,
	.commit                 = NULL,
	.persist                = NULL,
	.wait_io                = NULL,
	.erase                  = ramdisk_erase,
	.readahead              = NULL,
	.exit                   = ramdisk_exit,
};
*/

#ifndef NIC_SIDE
struct storage_operations storage_dax = {
	.init = dax_init,
	.read = dax_read,
	.read_unaligned = dax_read_unaligned,
	.write = dax_write,
	.write_unaligned = dax_write_unaligned,
	.write_opt = dax_write_opt,
	.write_opt_unaligned = dax_write_opt_unaligned,
	.commit = dax_commit,
	.persist = dax_persist,
	.wait_io = NULL,
	.erase = dax_erase,
	.readahead = NULL,
	.exit = dax_exit,
};
#endif

struct storage_operations storage_hdd = {
	.init = hdd_init,
	.read = hdd_read,
	.read_unaligned = NULL,
	.write = hdd_write,
	.write_unaligned = NULL,
	.write_opt = NULL,
	.write_opt_unaligned = NULL,
	.commit = hdd_commit,
        .persist = NULL,
	.wait_io = NULL,
	.erase = NULL,
	.readahead = NULL,
	.exit = hdd_exit,
};

struct storage_operations storage_aio = {
	.init = mlfs_aio_init,
	.read = mlfs_aio_read,
	.read_unaligned = NULL,
	.write = mlfs_aio_write,
	.write_unaligned = NULL,
	.write_opt = NULL,
	.write_opt_unaligned = NULL,
	.commit = mlfs_aio_commit,
        .persist = NULL,
	.wait_io = mlfs_aio_wait_io,
	.erase = mlfs_aio_erase,
	.readahead = mlfs_aio_readahead,
	.exit = mlfs_aio_exit,
};

struct storage_operations storage_nic_rpc = {
	.init = nic_rpc_init,
	.read = nic_rpc_read_and_get,
	.read_unaligned = nic_rpc_read_and_get_unaligned,
	.write = nic_rpc_send_and_write,
	.write_unaligned = nic_rpc_send_and_write_unaligned,
	.write_opt = nic_rpc_write_opt_local,
	.write_opt_unaligned = nic_rpc_write_opt_local_unaligned,
	.commit = nic_rpc_commit,
	.persist = NULL,
	.wait_io = NULL,
	.erase = nic_rpc_erase,
	.readahead = NULL,
	.exit = nic_rpc_exit,
};
#endif
