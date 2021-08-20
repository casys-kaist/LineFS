#include "global/global.h"

/*
uint	g_n_devices 		= 5;
uint	g_root_dev		= 1;
uint	g_max_open_files	= 100;
uint	g_block_size_bytes	= 4096;
uint	g_max_blocks_per_operation	 = 800;
uint	g_log_size_blks		= 4000;
uint	g_fs_size_blks		= (512UL * (1 << 10));
uint	g_max_extent_size_blks		 = (1 * (1 << 10));
uint	g_block_reservation_size_blks = (1 * (1 << 10));
uint	g_fd_start 			= 1000000;
*/

mlfs_config_t mlfs_conf = {0};

static int get_val_from_env(char *conf_name) {
    return getenv(conf_name) ? atoi(getenv(conf_name)) : 0; // default is 0
}

/**
 * @Synopsis  Load global configs from environment variables.
 */
void load_mlfs_configs(void) {
    //// Common
    // print
    // mlfs_conf.print_color	= get_val_from_env("PRINT_COLOR");
    // mlfs_conf.print_replicate	= get_val_from_env("PRINT_REPLICATE");
    // mlfs_conf.print_rep_coalesce= get_val_from_env("PRINT_REP_COALESCE");
    // mlfs_conf.print_posix	= get_val_from_env("PRINT_POSIX");
    // mlfs_conf.print_digest	= get_val_from_env("PRINT_DIGEST");
    // mlfs_conf.print_rpc		= get_val_from_env("PRINT_RPC");
    // mlfs_conf.print_setup	= get_val_from_env("PRINT_SETUP");
    // mlfs_conf.print_rdma	= get_val_from_env("PRINT_RDMA");
    // mlfs_conf.print_meta	= get_val_from_env("PRINT_META");

    mlfs_conf.x86_net_interface_name = getenv("X86_NET_INTERFACE_NAME");
    mlfs_conf.arm_net_interface_name = getenv("ARM_NET_INTERFACE_NAME");
    mlfs_conf.port = getenv("PORT_NUM");
    mlfs_conf.kernfs_num_cores = get_val_from_env("KERNFS_NUM_CORES");

    // TODO change it as runtime config after refactoring.
#ifdef NIC_OFFLOAD
    mlfs_conf.nic_offload = 1;
#endif
    // mlfs_conf.nic_offload = get_val_from_env("NIC_OFFLOAD");
    mlfs_conf.persist_nvm = get_val_from_env("PERSIST_NVM");
    mlfs_conf.persist_nvm_with_clflush = get_val_from_env("PERSIST_NVM_WITH_CLFLUSH");
    mlfs_conf.persist_nvm_with_rdma_read = get_val_from_env("PERSIST_NVM_WITH_RDMA_READ");
    if (mlfs_conf.persist_nvm) {
	// only one of them should be enabled.
	mlfs_assert(mlfs_conf.persist_nvm_with_clflush || mlfs_conf.persist_nvm_with_rdma_read);
	mlfs_assert(!(mlfs_conf.persist_nvm_with_clflush && mlfs_conf.persist_nvm_with_rdma_read));
    }

    mlfs_conf.log_coalesce = get_val_from_env("LOG_COALESCE");
    mlfs_conf.digest_threshold = get_val_from_env("DIGEST_THRESHOLD");
    if (mlfs_conf.nic_offload) {
	mlfs_conf.log_prefetching = get_val_from_env("LOG_PREFETCHING");
    }

    // breakdown
    mlfs_conf.breakdown = get_val_from_env("BREAKDOWN");
    if (mlfs_conf.breakdown) {
	mlfs_conf.rep_breakdown = get_val_from_env("REPLICATION_BREAKDOWN");
	mlfs_conf.digest_breakdown = get_val_from_env("DIGEST_BREAKDOWN");
	mlfs_conf.breakdown_mp = get_val_from_env("BREAKDOWN_MP");
    }

    //// KernFS only
#ifdef NIC_SIDE
    mlfs_conf.ioat_memcpy_offload = 0;
#else
    mlfs_conf.ioat_memcpy_offload = get_val_from_env("IOAT_MEMCPY_OFFLOAD");
    if (mlfs_conf.ioat_memcpy_offload)
        mlfs_conf.numa_node = get_val_from_env("NUMA_NODE");
#endif
    mlfs_conf.digest_noop = get_val_from_env("DIGEST_NOOP");
    mlfs_conf.thread_num_digest = get_val_from_env("THREAD_NUM_DIGEST");
    mlfs_conf.digest_opt_fconcurrent = get_val_from_env("DIGEST_OPT_FCONCURRENT");
    if (mlfs_conf.digest_opt_fconcurrent)
	mlfs_conf.thread_num_digest_fconcurrent = get_val_from_env("THREAD_NUM_DIGEST_FCONCURRENT");
    mlfs_conf.thread_num_rep = get_val_from_env("THREAD_NUM_REP");

    if (mlfs_conf.nic_offload) {
	mlfs_conf.digest_opt_host_memcpy = get_val_from_env("DIGEST_OPT_HOST_MEMCPY");
	mlfs_conf.digest_opt_parallel_rdma_memcpy = get_val_from_env("DIGEST_OPT_PARALLEL_RDMA_MEMCPY");
	mlfs_conf.m_to_n_rep_thread = get_val_from_env("M_TO_N_REP_THREAD");
	mlfs_conf.nic_slab_threshold_high = get_val_from_env("NIC_SLAB_THRESHOLD_HIGH");
	mlfs_conf.nic_slab_threshold_low = get_val_from_env("NIC_SLAB_THRESHOLD_LOW");
	mlfs_conf.host_memcpy_batch_max = get_val_from_env("HOST_MEMCPY_BATCH_MAX");

	if (mlfs_conf.digest_opt_parallel_rdma_memcpy)
	    mlfs_conf.thread_num_digest_rdma_memcpy = get_val_from_env("THREAD_NUM_DIGEST_RDMA_MEMCPY");
	if (mlfs_conf.digest_opt_host_memcpy)
	    mlfs_conf.thread_num_digest_host_memcpy = get_val_from_env("THREAD_NUM_DIGEST_HOST_MEMCPY");
	mlfs_conf.thread_num_persist_log = get_val_from_env("THREAD_NUM_PERSIST_LOG"); // TODO Only used when clflush is enabled?
	if (mlfs_conf.log_prefetching) {
	    mlfs_conf.log_prefetch_threshold = get_val_from_env("LOG_PREFETCH_THRESHOLD");
	    mlfs_conf.thread_num_log_prefetch = get_val_from_env("THREAD_NUM_LOG_PREFETCH");
	    mlfs_conf.thread_num_log_prefetch_req = get_val_from_env("THREAD_NUM_LOG_PREFETCH_REQ");
	}
	mlfs_conf.thread_num_copy_log_to_local_nvm = get_val_from_env("THREAD_NUM_COPY_LOG_TO_LOCAL_NVM");
	mlfs_conf.thread_num_copy_log_to_last_replica = get_val_from_env("THREAD_NUM_COPY_LOG_TO_LAST_REPLICA");
	mlfs_conf.thread_num_coalesce = get_val_from_env("THREAD_NUM_COALESCE");
	mlfs_conf.thread_num_loghdr_build = get_val_from_env("THREAD_NUM_LOGHDR_BUILD");
	mlfs_conf.thread_num_loghdr_fetch = get_val_from_env("THREAD_NUM_LOGHDR_FETCH");
	mlfs_conf.thread_num_compress = get_val_from_env("THREAD_NUM_COMPRESS");
	mlfs_conf.thread_num_log_fetch = get_val_from_env("THREAD_NUM_LOG_FETCH");
	mlfs_conf.thread_num_fsync = get_val_from_env("THREAD_NUM_FSYNC");
	mlfs_conf.thread_num_end_pipeline = get_val_from_env("THREAD_NUM_END_PIPELINE");

	mlfs_conf.request_rate_limit_threshold = get_val_from_env("REQUEST_RATE_LIMIT_THRESHOLD");
	mlfs_conf.prefetch_data_cap = get_val_from_env("PREFETCH_DATA_CAP");

#ifdef NO_PIPELINING
	// Overriding thread num parameters.
	mlfs_conf.thread_num_digest = 1;
	mlfs_conf.thread_num_log_prefetch = 1;
	mlfs_conf.thread_num_copy_log_to_local_nvm = 1; // Comparing background copy and serial copy.
	mlfs_conf.thread_num_copy_log_to_last_replica = 0;
	mlfs_conf.thread_num_coalesce = 0;
	mlfs_conf.thread_num_loghdr_build = 0;
	mlfs_conf.thread_num_loghdr_fetch = 1;
	mlfs_conf.thread_num_compress = 1;
	mlfs_conf.thread_num_log_fetch = 1;
	mlfs_conf.thread_num_fsync = 0;
#endif
    }

    //// LibFS only
    if (mlfs_conf.nic_offload) {
	// add here...
    }
    mlfs_conf.async_replication = get_val_from_env("ASYNC_REPLICATION");

    //// Hyperloop
    mlfs_conf.hyperloop_ops_file_path = getenv("HYPERLOOP_OPS_FILE_PATH");

}

void print_mlfs_configs(void) {
    printf("=============== MLFS CONFIGS ===============\n");
#ifdef NIC_SIDE
    printf("ARM_NET_INTERFACE_NAME = %s\n", mlfs_conf.arm_net_interface_name);
#else
    printf("X86_NET_INTERFACE_NAME = %s\n", mlfs_conf.x86_net_interface_name);
#endif
    printf("PORT_NUM = %s\n", mlfs_conf.port);
    printf("IOAT_MEMCPY_OFFLOAD = %d\n", mlfs_conf.ioat_memcpy_offload);
    if (mlfs_conf.ioat_memcpy_offload)
        printf("NUMA_NODE = %d\n", mlfs_conf.numa_node);
    printf("THREAD_NUM_REP = %d\n", mlfs_conf.thread_num_rep);
    printf("PERSIST_NVM = %d\n", mlfs_conf.persist_nvm);
    printf("PERSIST_NVM_WITH_CLFLUSH = %d\n", mlfs_conf.persist_nvm_with_clflush);
    printf("PERSIST_NVM_WITH_RDMA_READ = %d\n", mlfs_conf.persist_nvm_with_rdma_read);
    printf("LOG_COALESCE = %d\n", mlfs_conf.log_coalesce);
    printf("DIGEST_THRESHOLD = %d\n", mlfs_conf.digest_threshold);
    printf("THREAD_NUM_DIGEST = %d\n", mlfs_conf.thread_num_digest);
    printf("DIGEST_OPT_FCONCURRENT = %d\n", mlfs_conf.digest_opt_fconcurrent);
    printf("THREAD_NUM_DIGEST_FCONCURRENT = %d\n", mlfs_conf.thread_num_digest_fconcurrent);
    printf("DIGEST_NOOP = %d\n", mlfs_conf.digest_noop);
    printf("ASYNC_REPLICATION = %d\n", mlfs_conf.async_replication);
    printf("----------- breakdown ----------------------\n");
    printf("BREAKDOWN = %d\n", mlfs_conf.breakdown);
    printf("REPLICATION_BREAKDOWN = %d\n", mlfs_conf.rep_breakdown);
    printf("DIGEST_BREAKDOWN = %d\n", mlfs_conf.digest_breakdown);
    printf("BREAKDOWN_MP = %d\n", mlfs_conf.breakdown_mp);
    printf("----------- nic-offload --------------------\n");
    printf("NIC_OFFLOAD = %d\n", mlfs_conf.nic_offload);
    printf("DIGEST_OPT_HOST_MEMCPY = %d\n", mlfs_conf.digest_opt_host_memcpy);
    printf("THREAD_NUM_DIGEST_HOST_MEMCPY = %d\n", mlfs_conf.thread_num_digest_host_memcpy);
    printf("THREAD_NUM_PERSIST_LOG = %d\n", mlfs_conf.thread_num_persist_log);
    printf("DIGEST_OPT_PARALLEL_RDMA_MEMCPY = %d\n", mlfs_conf.digest_opt_parallel_rdma_memcpy);
    printf("THREAD_NUM_DIGEST_RDMA_MEMCPY = %d\n", mlfs_conf.thread_num_digest_rdma_memcpy);
    printf("LOG_PREFETCHING = %d\n", mlfs_conf.log_prefetching);
    printf("LOG_PREFETCH_THRESHOLD = %d\n", mlfs_conf.log_prefetch_threshold);
    printf("THREAD_NUM_LOG_PREFETCH(Kernfs) = %d\n", mlfs_conf.thread_num_log_prefetch);
    printf("THREAD_NUM_LOG_PREFETCH_REQ(Libfs) = %d\n", mlfs_conf.thread_num_log_prefetch_req);
    printf("THREAD_NUM_END_PIPELINE = %d\n", mlfs_conf.thread_num_end_pipeline);
    printf("REQUEST_RATE_LIMIT_THRESHOLD = %d\n", mlfs_conf.request_rate_limit_threshold);
    printf("PREFETCH_DATA_CAP = %d\n", mlfs_conf.prefetch_data_cap);
    printf("============================================\n");
}
