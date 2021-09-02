#ifndef __global_h__
#define __global_h__

#include "types.h"
#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t g_ssd_dev;
extern uint8_t g_log_dev;
extern uint8_t g_hdd_dev;

void load_mlfs_configs(void);
void print_mlfs_configs(void);

// Global parameters.
#define g_n_devices			4
#define g_root_dev			1
#define g_block_size_bytes	4096UL
#define g_block_size_mask   (g_block_size_bytes - 1)
#define g_block_size_shift	12UL
// 2 works for microbenchmark
// 6 is generally too big. but Redis AOF mode requires at least 6.
#define g_max_blocks_per_operation 4
#define g_hdd_block_size_bytes 4194304UL
#define g_hdd_block_size_shift 22UL

//#define g_log_size 430080UL // # of blocks per log (1680 MB)
// #define g_log_size 32768UL // (128 MB)
// #define g_log_size 65536UL // (256 MB)
#define g_log_size 131072UL // (512 MB)
//#define g_log_size 262144UL // (1 GB)
//#define g_log_size 524288UL // (2 GB)
//#define g_log_size 1310720UL // (5 GB)
//#define g_log_size 131072UL

#define g_directory_shift  16UL
#define g_directory_mask ((1 << ((sizeof(inum_t) * 8) - g_directory_shift)) - 1)
#define g_segsize_bytes    (1ULL << 30)  // 1 GB
#define g_max_read_cache_blocks  (1 << 18) // 1 GB

// #define g_max_nicrpc_buf_blocks  (1ULL << 21) // 8 GB, used for DRAM buffer.
#define g_max_nicrpc_buf_blocks  (11 * (1ULL << 18)) // 11 GB, used for DRAM buffer.

/* The number of memcpy meta entries in the host memcpy buffer. Let's allocate
 * half of the number of blocks in log area for now. The worst case where each
 * loghdr has only 1 block.
 */
// #define g_memcpy_meta_cnt (g_log_size >> 1)  // LevelDB failed with assertion with this value.
#define g_memcpy_meta_cnt (g_log_size)

#define g_namespace_bits 6
#define g_namespace_mask (((1 << g_namespace_bits) - 1) << (32 - g_namespace_bits))

#define g_fd_start  1000000

/* note for this posix handlers' (syscall handler) return value and errno.
 * glibc checks syscall return value in INLINE_SYSCALL macro.
 * if the return value is negative, it sets the value as errno
 * (after turnning to positive one) and returns -1 to application.
 * Therefore, the return value must be correct -errno in posix semantic
 */

#define SET_MLFS_FD(fd) fd + g_fd_start
#define GET_MLFS_FD(fd) fd - g_fd_start

#define ROOTINO 1  // root i-number

#define NDIRECT 7
#define NINDIRECT (g_block_size_bytes / sizeof(addr_t))

#define N_FILE_PER_DIR (g_block_size_bytes / sizeof(struct mlfs_dirent))

#define NINODES 300000
#define g_max_open_files 1000000

// maximum number of libfs processes
// FIXME Setting MAX_LIBFS_PROCESSES to 6, 12 incurs a deadlock at 'ilock()' when running iobench.
//#define MAX_LIBFS_PROCESSES 24
#define MAX_LIBFS_PROCESSES 8

// maximum size of full path
//#define MAX_PATH 4096
#define MAX_PATH 1024

//FIXME: Use inums instead of paths for remote reads
// Temporarily using a small path for read RPC
#define MAX_REMOTE_PATH 40

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 28
//#define DIRSIZ 60

#define MLFS_SEED defined(DISTRIBUTED) && defined(SEED)
#define MLFS_PASSIVE defined(DISTRIBUTED) && !defined(ACTIVE)
#define MLFS_MASTER defined(DISTRIBUTED) && defined(MASTER)
#define MLFS_LEASE defined(DISTRIBUTED) && (defined(MASTER) || defined(KERNFS)) && defined(USE_LEASE)
#define MLFS_REPLICA defined(DISTRIBUTED) && !defined(MASTER)
#define MLFS_HOT defined(DISTRIBUTED) && defined(HOT) && !defined(MASTER)
#define MLFS_COLD defined(DISTRIBUTED) && !defined(HOT) && !defined(MASTER)
#define MLFS_NAMESPACES defined(NAMESPACES) && defined(DISTRIBUTED)

#define SHM_START_ADDR (void *)0x7ff000000000UL

#if MLFS_LEASE
#define SHM_SIZE (NINODES * sizeof(mlfs_lease_t))
#else
#define SHM_SIZE (200 << 20) //200 MB
#endif

#define SHM_NAME "/mlfs_shm"

#ifdef NIC_OFFLOAD
#define g_n_kernfs_nic 3    // The number of kernfs on SmartNIC. Change it correctly.
#define g_n_hot_rep (2 * g_n_kernfs_nic)  // It includes kernfs on SmartNIC.
#else
#define g_n_kernfs_nic 0    // The number of kernfs on SmartNIC.
#define g_n_hot_rep 3	    // Change it correctly.
#endif

#define g_n_hot_bkp 0
#define g_n_cold_bkp 0
#define g_n_ext_rep 0
#define g_n_nodes (g_n_hot_rep + g_n_hot_bkp + g_n_cold_bkp + g_n_ext_rep)

#define g_n_replica (g_n_nodes - g_n_kernfs_nic)    /* The number of nodes in a replica chain.
                                                     * (Host + NIC) is regarded as one.
                                                     * Example: host_kernfs1,
                                                     *          nic_kernfs1,
                                                     *          host_kernfs2,
                                                     *          nic_kernfs2
                                                     *          => g_n_replica = 2
                                                     */

#define COPY_NEXT_BITMAP_SIZE_IN_BIT (1024*1024) // 1M bits

/**
 * Configurations
 */
typedef struct mlfs_config {
    //// Common
    // print TODO use macro.
    // int print_replicate;
    // int print_rep_coalesce;
    // int print_posix;
    // int print_digest;
    // int print_rpc;
    // int print_setup;
    // int print_rdma;
    // int print_meta;

    int persist_nvm;
    int persist_nvm_with_rdma_read; // host only
    int persist_nvm_with_clflush;    // host only
    int log_coalesce;	// Not supported in NIC-offloading setup.
    int ioat_memcpy_offload; /* memcpy offloading to I/OAT DMA engine. */
    char *x86_net_interface_name; /* the name of RDMA network interface of X86 host. */
    char *arm_net_interface_name; /* the name of RDMA network interface of ARM SmartNIC. */
    char *port; /* port number */
    char *low_latency_port; /* port number for low-latency channel */

    int nic_offload;

    int breakdown;
    int rep_breakdown;
    int digest_breakdown;
    int breakdown_mp;
    int digest_threshold; /* The 30% is ad-hoc parameter:
			   * In genernal, 30% ~ 40% shows good performance
			   * in all workloads
			   */
    int log_prefetching;    // Log prefetching to NIC DRAM.
    int log_prefetch_threshold; /* prefetch threshold in the number of log blocks.
				 * Ex) 2 means a prefetch request is sent to
				 * NIC kernfs on every 2-block-write.
				 */
    // int kernfs_num_cores; // Set core affinity to CPU 0-<kernfs_num_cores>.

    //// Kernfs only
    int digest_noop;
    int digest_opt_host_memcpy;
    int digest_opt_parallel_rdma_memcpy;
    int digest_opt_fconcurrent;
    int m_to_n_rep_thread;	// M to N replicate threads.
    int nic_slab_threshold_high; // upper threshold.
    int nic_slab_threshold_low; // lower threshold.
    int host_memcpy_batch_max;	// max number of batched host memcpy entries. It must less than or equal to MAX_SEND_QUEUE_SIZE
    int numa_node; // Index of numa node to use. It works only if I/OAT memcopy
		   // is enabled. Set this value to -1 will use all numa nodes.
		   // E.g., 0 to use numa node 0, 1 to use numa node 1.

    // # of threads.
    int thread_num_digest;
    int thread_num_digest_fconcurrent;
    int thread_num_digest_rdma_memcpy;
    int thread_num_digest_host_memcpy; // host memcpy handler.
    int thread_num_persist_log; // host persist log handler.
    int thread_num_rep;
    int thread_num_log_prefetch;    // # of threads of log prefetcher in Kernfs.
    int thread_num_log_prefetch_req; // # of threads of prefetch requester in Libfs.
    int thread_num_copy_log_to_local_nvm; // # of threads for copying log to
					  // local NVM on Replica 1.
    int thread_num_copy_log_to_last_replica; // # of threads for copying log to
					     // the last replica's NVM (by
					     // Replica 1).

    int thread_num_coalesce;	// coalescing in primary NIC kernfs.
    int thread_num_loghdr_build;// build loghdr or fetch loghdr.
    int thread_num_loghdr_fetch;// build loghdr or fetch loghdr.
    int thread_num_compress;	// compress or dedup.
    int thread_num_log_fetch;// fetch log from primary nic dram.
    int thread_num_fsync;// a thread for fsync.
    int thread_num_end_pipeline; // Freeing buffers.

    // Parameters.
    int request_rate_limit_threshold; // Limit the number of posted request from primary to replica 1.
    int prefetch_data_cap; // For limiting prefetch rate. In MB.

    //// Libfs only
    int async_replication;

    // Hyperloop
    char *hyperloop_ops_file_path;

} mlfs_config_t;

extern mlfs_config_t mlfs_conf;

/**
 *
 * All global variables here are default set by global.c
 * but might be changed the .ini reader.
 * Please keep the "g_" prefix so we know what is global and
 * what isn't.
 *
 */
// extern uint	g_n_devices; /* old NDEV */
// extern uint	g_root_dev; /* old ROOTDEV */
// extern uint	g_max_open_files; /*old NFILE*/
// extern uint	g_block_size_bytes; /* block size in bytes, old BSIZE*/
// extern uint	g_max_blocks_per_operation; /* old MAXOPBLOCKS*/
// extern uint	g_log_size_blks; /* log size in blocks, old LOGSIZE*/
// extern uint	g_fs_size_blks; /* filesystem size in blocks, old FSSIZE*/
// extern uint	g_max_extent_size_blks; /* filesystem size in blocks, old MAX_EXTENT_SIZE*/
// extern uint	g_block_reservation_size_blks; /* reservation size in blocks, old BRESRV_SIZE*/
// extern uint	g_fd_start; /* offset start of fds used by mlfs, old FD_START*/

#ifdef __cplusplus
}
#endif

#endif
