#ifdef __x86_64__
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>

#include "mlfs/mlfs_user.h"
#include "global/global.h"
#include "global/util.h"
#include "global/defs.h"
#include "io/balloc.h"
#include "kernfs_interface.h"
#include "fs.h"
#include "io/block_io.h"
#include "storage/storage.h"
#include "extents.h"
#include "extents_bh.h"
#include "filesystem/slru.h"
#include "migrate.h"
#include "nic/loghdr.h"

#ifdef DISTRIBUTED
#include "distributed/rpc_interface.h"
#include "distributed/replication.h"
#endif

#define NOCREATE 0
#define CREATE 1

int log_fd = 0;
int shm_fd = 0;
uint8_t *shm_base;

#ifdef PROFILE_REALTIME_MEMCPY_BW
rt_bw_stat memcpy_bw_stat = {0};
#endif

#if 0
void mlfs_get_time(mlfs_time_t *a) {}
#else
void mlfs_get_time(mlfs_time_t *t)
{
	gettimeofday(t, NULL);
	return;
}
#endif


#if MLFS_LEASE
SharedTable *lease_table;
#endif

#ifdef PROFILE_THPOOL
// one of repreq or repmsg (primary:repreq, replicas:repmsg)
// Per-rep-thread
struct rep_th_stat *rep_th_stats = NULL;

void print_rep_profile_result(void)
{
	int n;
	double sum = 0.0, avg = 0.0, max = 0.0;
	double wait_seqn_sum = 0.0, wait_seqn_avg = 0.0, wait_seqn_max = 0.0;
	double min = 99999.99; //sufficiently large value for min.
	double wait_seqn_min = 99999.99; //sufficiently large value for min.
	unsigned long cnt = 0;

	if (!rep_th_stats)
		return;

	for (n = 0; n < mlfs_conf.thread_num_rep; n++) {
		if (!rep_th_stats[n].schedule_cnt)
			continue;

		cnt += rep_th_stats[n].schedule_cnt;
		sum += rep_th_stats[n].schedule_delay_sum;
		wait_seqn_sum += rep_th_stats[n].wait_seqn_delay_sum;

		if (rep_th_stats[n].schedule_delay_max > max)
			max = rep_th_stats[n].schedule_delay_max;

		if (rep_th_stats[n].schedule_delay_min &&
		    rep_th_stats[n].schedule_delay_min < min)
			min = rep_th_stats[n].schedule_delay_min;

		if (rep_th_stats[n].wait_seqn_delay_max > wait_seqn_max)
			wait_seqn_max = rep_th_stats[n].wait_seqn_delay_max;

		if (rep_th_stats[n].wait_seqn_delay_min &&
		    rep_th_stats[n].wait_seqn_delay_min < wait_seqn_min)
			wait_seqn_min = rep_th_stats[n].wait_seqn_delay_min;
	}

	if (cnt) {
		avg = (double)sum / cnt;
		wait_seqn_avg = (double)wait_seqn_sum / cnt;
		printf("Thread_name replication_worker\n");
		printf("Total_scheduled_count %lu\n", cnt);
		printf("Total_sched_delay_avg(us) %16.4f\n", avg * 1000000.0);
		printf("Total_sched_delay_min(us) %16.4f\n", min * 1000000.0);
		printf("Total_sched_delay_max(us) %16.4f\n", max * 1000000.0);
		printf("Seqn_wait_avg(us) %16.4f\n", wait_seqn_avg * 1000000.0);
		printf("Seqn_wait_min(us) %16.4f\n", wait_seqn_min * 1000000.0);
		printf("Seqn_wait_max(us) %16.4f\n", wait_seqn_max * 1000000.0);
		printf("----------------------------------------------\n");
	} else {
		// printf("Thread name: rep\n");
		// printf("Total scheduled cnt: %lu\n", cnt);
		// printf("----------------------------------------------\n");
	}
}
#endif

// breakdown timers.
static void reset_breakdown_timers(void) {
    if (mlfs_conf.rep_breakdown) {
	RESET_TIMER(evt_rep_local);
	RESET_TIMER(evt_rep_chain);
	RESET_TIMER(evt_wait_log_prefetch);
	RESET_TIMER(evt_rep_read_log);
	RESET_TIMER(evt_rep_create_rsync);
	RESET_TIMER(evt_rep_start_session);
	RESET_TIMER(evt_rep_gen_rsync_msgs);
	RESET_TIMER(evt_rep_rdma_add_sge);
	RESET_TIMER(evt_rep_rdma_finalize);
	RESET_TIMER(evt_rep_grm_nr_blks);
	RESET_TIMER(evt_rep_grm_update_ptrs);
	RESET_TIMER(evt_rep_log_copy);
	RESET_TIMER(evt_rep_build_msg);
	RESET_TIMER(evt_rep_log_copy_write);
	RESET_TIMER(evt_rep_log_copy_read);
	RESET_TIMER(evt_rep_log_copy_compl);
	RESET_TIMER(evt_rep_wait_req_done);
	// RESET_TIMER(evt_loghdr);
	RESET_TIMER(evt_build_loghdrs);
	RESET_TIMER(evt_empty_log_buf);
	RESET_TIMER(evt_loghdr_misc);
	RESET_TIMER(evt_send_loghdr);
	RESET_TIMER(evt_wait_loghdrs_lock_rep);
	RESET_TIMER(evt_set_remote_bit);
	RESET_TIMER(evt_set_remote_bit1);
	RESET_TIMER(evt_rep_critical_host);
	RESET_TIMER(evt_rep_critical_nic);
	RESET_TIMER(evt_handle_rpcmsg_misc);
	RESET_TIMER(evt_free_arg);
	RESET_TIMER(evt_reset_log_buf);
	RESET_TIMER(evt_atomic_add);
	RESET_TIMER(evt_atomic_test);
	RESET_TIMER(evt_clear_replicaing);
	RESET_TIMER(evt_wait_local_host_copy);
    }

    if (mlfs_conf.digest_breakdown) {
	RESET_TIMER(evt_hdr);
	RESET_TIMER(evt_coal);
	RESET_TIMER(evt_rlh);
	RESET_TIMER(evt_dro);
	RESET_TIMER(evt_dlfrl);
	RESET_TIMER(evt_dc);
	RESET_TIMER(evt_icache);
	RESET_TIMER(evt_cre);
	RESET_TIMER(evt_geb);
	RESET_TIMER(evt_ntype_i);
	RESET_TIMER(evt_ntype_f);
	RESET_TIMER(evt_dig_fcon);
	RESET_TIMER(evt_dig_fcon_in);
	RESET_TIMER(evt_dig_wait_peer);
	RESET_TIMER(evt_ntype_u);
	RESET_TIMER(evt_wait_rdma_memcpy);
	RESET_TIMER(evt_wait_loghdrs_lock_dig);
    }

    if (mlfs_conf.breakdown_mp) {
	RESET_TIMER(evt_dig_req_total);
	RESET_TIMER(evt_dig_req_per_task);
	RESET_TIMER(evt_raw_memcpy);
    }

    // RESET_TIMER(evt_k_signal_callback);
}

static void print_breakdown_timers(void) {
    struct event_timer evt_denom;

    if (!mlfs_conf.breakdown)
	return;

    if (mlfs_conf.rep_breakdown) {
	if (evt_rep_chain.count) {
	    evt_denom = evt_rep_chain;
	} else {
	    evt_denom = evt_rep_local;
	}

	if (evt_denom.count == 0)
	    printf("WARNING replication count is 0.\n");

	printf (",=============== Replication breakdown ===============,,,,\n");
	PRINT_HDR();
	PRINT_TIMER(evt_rep_local,	     "REP_LOCAL", evt_denom);
	PRINT_TIMER(evt_rep_chain,	     "REP_CHAIN", evt_denom);
	PRINT_TIMER(evt_rep_critical_host,   "REP_CRITICAL(hostonly)", evt_denom);
	PRINT_TIMER(evt_rep_critical_nic,    "REP_CRITICAL(nic-offload)", evt_denom);
	PRINT_TIMER(evt_handle_rpcmsg_misc,  "REP_HANDLE_RPCMSG_MISC", evt_denom);
	PRINT_TIMER(evt_free_arg,	     "  REP_FREE_ARG", evt_denom);
	PRINT_TIMER(evt_reset_log_buf,	     "  REP_RESET_LOGBUF", evt_denom);
	PRINT_TIMER(evt_atomic_add,	     "  REP_ATOMIC_ADD", evt_denom);
	PRINT_TIMER(evt_atomic_test,	     "  REP_ATOMIC_TEST", evt_denom);
	PRINT_TIMER(evt_clear_replicaing,    "  REP_CLEAR_REPLICATING", evt_denom);
	if(mlfs_conf.log_prefetching)
          PRINT_TIMER(evt_wait_log_prefetch, "  WAIT_LOG_PREFETCH", evt_denom);
        PRINT_TIMER(evt_rep_read_log,	     "  READ_LOG", evt_denom);
	PRINT_TIMER(evt_rep_create_rsync,    "  CREATE_RSYNC", evt_denom);
	PRINT_TIMER(evt_rep_start_session,   "    START_SESSION", evt_denom);
	PRINT_TIMER(evt_rep_gen_rsync_msgs,  "    GEN_RSYNC_MSGS", evt_denom);
	PRINT_TIMER(evt_rep_rdma_add_sge,    "      RDMA_ADD_SGE", evt_denom);
	PRINT_TIMER(evt_rep_rdma_finalize,   "      RDMA_FINALIZE", evt_denom);
	PRINT_TIMER(evt_rep_grm_nr_blks,     "      NR_BLKS_BEFORE_WRAP", evt_denom);
	PRINT_TIMER(evt_rep_grm_update_ptrs, "      UPDATE_PTRS", evt_denom);
	PRINT_TIMER(evt_rep_build_msg,	     "  BUILD_RPC_MSG", evt_denom);
	PRINT_TIMER(evt_rep_log_copy,	     "  LOG_COPY", evt_denom);
	PRINT_TIMER(evt_rep_log_copy_write,  "    LOG_COPY_write", evt_denom);
	PRINT_TIMER(evt_rep_log_copy_read,   "    LOG_COPY_read", evt_denom);
	PRINT_TIMER(evt_rep_log_copy_compl,  "  LOG_COPY_COMPL(kernfs 2)", evt_denom);
	PRINT_TIMER(evt_rep_wait_req_done,   "  WAIT_REQ_DONE", evt_denom);
	PRINT_TIMER(evt_set_remote_bit,	     "  SET_REMOTE_BIT", evt_denom);
	PRINT_TIMER(evt_set_remote_bit1,     "    1", evt_denom);
	PRINT_TIMER(evt_wait_loghdrs_lock_rep,"WAIT_LOGHDR_LOCK_REP", evt_denom);
	// PRINT_TIMER(evt_loghdr,		     "LOGHDR", evt_denom);
	PRINT_TIMER(evt_build_loghdrs,	     "  BUILD_LOGHDR", evt_denom);
	PRINT_TIMER(evt_empty_log_buf,	     "  EMPTY_LOGBUF", evt_denom);
	PRINT_TIMER(evt_loghdr_misc,	     "  LOGHDR_MISC", evt_denom);
	PRINT_TIMER(evt_send_loghdr,	     "  SEND_LOGHDR", evt_denom);
	PRINT_TIMER(evt_wait_local_host_copy,"  LOCAL_HOST_CPY", evt_denom);
    }

    if(mlfs_conf.digest_breakdown) {
	evt_denom = evt_hdr;
	printf (",=============== Digest breakdown ===============,,,,\n");
	PRINT_HDR();
	PRINT_TIMER(evt_hdr,		"HDR", evt_denom);
	PRINT_TIMER(evt_rlh,		"  RLH", evt_denom);
	PRINT_TIMER(evt_coal,		"  COALLESCE", evt_denom);
	PRINT_TIMER(evt_dro,		"    DRO", evt_denom);
	PRINT_TIMER(evt_dlfrl,		"  DLFRL", evt_denom);
	PRINT_TIMER(evt_ntype_i,	"    NTYPE_I", evt_denom);
	PRINT_TIMER(evt_ntype_f,	"    NTYPE_F", evt_denom);
	PRINT_TIMER(evt_dig_fcon,	"    FCON", evt_denom);
	PRINT_TIMER(evt_dig_fcon_in,	"    FCON_IN", evt_denom);
	PRINT_TIMER(evt_icache,		"      icache", evt_denom);
	PRINT_TIMER(evt_cre,		"      create_rdma", evt_denom);
	PRINT_TIMER(evt_geb,		"      get_ext_block", evt_denom);
	if(!mlfs_conf.digest_opt_host_memcpy) {
	    PRINT_TIMER(evt_dc,		"      data_copy", evt_denom);
	    PRINT_TIMER(evt_wait_rdma_memcpy,	"        wait_rdma_memcpy", evt_denom);
	}
	PRINT_TIMER(evt_ntype_u,	"    NTYPE_U", evt_denom);
	if(mlfs_conf.digest_opt_host_memcpy)
	    PRINT_TIMER(evt_dc,		"    data_copy", evt_denom);
	PRINT_TIMER(evt_dig_wait_peer,	"  WAIT_PEER", evt_denom);
	PRINT_TIMER(evt_wait_loghdrs_lock_dig,"WAIT_LOGHDR_LOCK_DIG", evt_denom);
    }

    if (mlfs_conf.breakdown_mp) {
	printf (",===============================================,,,\n");
	printf ("MP_breakdown\n");
	PRINT_MP_HDR();
	PRINT_MP_TIMER(evt_dig_req_total, "dig_req_total");
	PRINT_MP_TIMER(evt_dig_req_per_task, "dig_req_per_task");
	PRINT_MP_TIMER(evt_raw_memcpy, "raw_memcpy");
	printf("data_written %lu\n", evt_raw_memcpy.data_size);
    }

    // printf (",=============== Other breakdown ===============,,,,\n");
    // PRINT_HDR();
    // PRINT_TIMER(evt_k_signal_callback,	"k_signal_callback", evt_denom);
}

pthread_spinlock_t icache_spinlock;
pthread_spinlock_t dcache_spinlock;

pthread_mutex_t digest_mutex;

struct inode *inode_hash;

struct disk_superblock disk_sb[g_n_devices + 1];
struct super_block *sb[g_n_devices + 1];

ncx_slab_pool_t *mlfs_slab_pool;
ncx_slab_pool_t *mlfs_slab_pool_shared;

uint8_t g_log_dev = 0;
uint8_t g_ssd_dev = 0;
uint8_t g_hdd_dev = 0;

kernfs_stats_t g_perf_stats;
uint8_t enable_perf_stats;

threadpool thread_pool_digest;
threadpool thread_pool_ssd;

#ifdef PER_LIBFS_REPLICATION_THREAD
threadpool *replicate_thpools;
#else
threadpool thread_pool_replicate;
#endif

// NIC offloading.
threadpool thread_pool_host_memcpy;
threadpool thread_pool_persist_log;
#ifdef PER_LIBFS_PREFETCH_THREAD
threadpool *log_prefetch_thpools;
#else
threadpool thread_pool_log_prefetch;
#endif

// thread pool for nvm write on host (digest).
threadpool thread_pool_nvm_write;

// thread pool to handle trivial jobs.
threadpool thread_pool_misc;

threadpool thread_pool_send_heartbeat;

// NIC_OFFLOAD
// It stores the number of recently digested log block.
addr_t digest_end_blknrs[g_n_nodes + MAX_LIBFS_PROCESSES];
// Per-libfs buffers to store host_memcpy_list.
memcpy_meta_t *host_memcpy_buf_p[g_n_nodes + MAX_LIBFS_PROCESSES];

#if MLFS_LEASE
struct sockaddr_un all_cli_addr[g_n_hot_rep];
int addr_idx = 0;
#endif

DECLARE_BITMAP(g_log_bitmap, MAX_LIBFS_PROCESSES);

int digest_unlink(uint8_t from_dev, uint8_t to_dev, int libfs_id, uint32_t inum);

#define NTYPE_I 1
#define NTYPE_D 2
#define NTYPE_F 3
#define NTYPE_U 4

struct digest_arg {
	int sock_fd;
#ifdef DISTRIBUTED
	uint64_t seqn;
#endif
	struct sockaddr_un cli_addr;
	char msg[MAX_SOCK_BUF];
};

void show_kernfs_stats(void)
{
	float clock_speed_mhz = get_cpu_clock_speed();
	uint64_t n_digest = g_perf_stats.n_digest == 0 ? 1.0 : g_perf_stats.n_digest;

	printf("\n");
	//printf("CPU clock : %.3f MHz\n", clock_speed_mhz);
	printf("----------------------- kernfs statistics\n");
	printf("digest       : %.3f ms\n",
			g_perf_stats.digest_time_tsc / (clock_speed_mhz * 1000.0));
	printf("- replay     : %.3f ms\n",
			g_perf_stats.replay_time_tsc / (clock_speed_mhz * 1000.0));
	printf("- apply      : %.3f ms\n",
			g_perf_stats.apply_time_tsc / (clock_speed_mhz * 1000.0));
	printf("-- inode digest : %.3f ms\n",
			g_perf_stats.digest_inode_tsc / (clock_speed_mhz * 1000.0));
	printf("-- dir digest   : %.3f ms\n",
			g_perf_stats.digest_dir_tsc / (clock_speed_mhz * 1000.0));
	printf("-- file digest  : %.3f ms\n",
			g_perf_stats.digest_file_tsc / (clock_speed_mhz * 1000.0));
	printf("- persist    : %.3f ms\n",
			g_perf_stats.persist_time_tsc / (clock_speed_mhz * 1000.0));
	printf("n_digest        : %lu\n", g_perf_stats.n_digest);
	printf("n_digest_skipped: %lu (%.1f %%)\n",
			g_perf_stats.n_digest_skipped,
			((float)g_perf_stats.n_digest_skipped * 100.0) / (float)n_digest);
	printf("path search    : %.3f ms\n",
			g_perf_stats.path_search_tsc / (clock_speed_mhz * 1000.0));
	printf("total migrated : %lu MB\n", g_perf_stats.total_migrated_mb);
#ifdef MLFS_LEASE
	printf("nr lease rpc (local)	: %lu\n", g_perf_stats.lease_rpc_local_nr);
	printf("nr lease rpc (remote)	: %lu\n", g_perf_stats.lease_rpc_remote_nr);
	printf("nr lease migration	: %lu\n", g_perf_stats.lease_migration_nr);
	printf("nr lease contention	: %lu\n", g_perf_stats.lease_contention_nr);
#endif
	printf("--------------------------------------\n");
}

void show_storage_stats(void)
{
	printf("------------------------------ storage stats\n");
	printf("NVM - %.2f %% used (%.2f / %.2f GB) - # of used blocks %lu\n",
			(100.0 * (float)sb[g_root_dev]->used_blocks) /
			(float)disk_sb[g_root_dev].ndatablocks,
			(float)(sb[g_root_dev]->used_blocks << g_block_size_shift) / (1 << 30),
			(float)(disk_sb[g_root_dev].ndatablocks << g_block_size_shift) / (1 << 30),
			sb[g_root_dev]->used_blocks);

#ifdef USE_SSD
	printf("SSD - %.2f %% used (%.2f / %.2f GB) - # of used blocks %lu\n",
			(100.0 * (float)sb[g_ssd_dev]->used_blocks) /
			(float)disk_sb[g_ssd_dev].ndatablocks,
			(float)(sb[g_ssd_dev]->used_blocks << g_block_size_shift) / (1 << 30),
			(float)(disk_sb[g_ssd_dev].ndatablocks << g_block_size_shift) / (1 << 30),
			sb[g_ssd_dev]->used_blocks);
#endif

#ifdef USE_HDD
	printf("HDD - %.2f %% used (%.2f / %.2f GB) - # of used blocks %lu\n",
			(100.0 * (float)sb[g_hdd_dev]->used_blocks) /
			(float)disk_sb[g_hdd_dev].ndatablocks,
			(float)(sb[g_hdd_dev]->used_blocks << g_block_size_shift) / (1 << 30),
			(float)(disk_sb[g_hdd_dev].ndatablocks << g_block_size_shift) / (1 << 30),
			sb[g_hdd_dev]->used_blocks);
#endif

		mlfs_info("--- lru_list count %lu, %lu, %lu\n",
			  g_lru[g_root_dev].n, g_lru[g_ssd_dev].n, g_lru[g_hdd_dev].n);
}

/*void read_log_header_from_nic_dram(loghdr_meta_t *loghdr_meta, addr_t hdr_blkno) {
    uint64_t nr_cur_log_blks;

    nr_cur_log_blks = loghdr_meta->loghdr->nr_log_blocks;

    // point to the next log header.
    loghdr_meta->loghdr =
        loghdr_meta->loghdr + (nr_cur_log_blks << g_block_size_shift);

    loghdr_meta->hdr_blkno = hdr_blkno;
    loghdr_meta->is_hdr_allocated = 1;
}
*/

loghdr_meta_t *read_log_header(uint8_t from_dev, addr_t hdr_addr)
{
    	START_TIMER(evt_rlh);
	int ret, i;
	loghdr_t *_loghdr;
	loghdr_meta_t *loghdr_meta;

	loghdr_meta = (loghdr_meta_t *)mlfs_zalloc(sizeof(loghdr_meta_t));
	if (!loghdr_meta)
		panic("cannot allocate logheader\n");

	INIT_LIST_HEAD(&loghdr_meta->link);

	/* optimization: instead of reading log header block to kernel's memory,
	 * buffer head points to memory address for log header block.
	 */
	_loghdr = (loghdr_t *)(g_bdev[from_dev]->map_base_addr +
		(hdr_addr << g_block_size_shift));

	loghdr_meta->loghdr = _loghdr;
	loghdr_meta->hdr_blkno = hdr_addr;
	loghdr_meta->is_hdr_allocated = 1;

	mlfs_log("%s", "--------------------------------\n");
	mlfs_log("%d\n", _loghdr->n);
	mlfs_log("ts %ld.%06ld\n", _loghdr->mtime.tv_sec, _loghdr->mtime.tv_usec);
	mlfs_log("blknr %lu\n", hdr_addr);
	mlfs_log("size %u\n", _loghdr->nr_log_blocks);
	mlfs_log("inuse %x\n", _loghdr->inuse);
	for(int i=0; i< _loghdr->n; i++) {
		mlfs_log("type[%d]: %u\n", i, _loghdr->type[i]);
		mlfs_log("inum[%d]: %u\n", i, _loghdr->inode_no[i]);
	}

	/*
	for (i = 0; i < _loghdr->n; i++) {
		mlfs_debug("types %d blocks %lx\n",
				_loghdr->type[i], _loghdr->blocks[i] + loghdr_meta->hdr_blkno);
	}
	*/

	END_TIMER(evt_rlh);
	return loghdr_meta;
}

static int persist_dirty_objects_ssd(void)
{
	struct rb_node *node;

	sync_all_buffers(g_bdev[g_ssd_dev]);

	store_all_bitmap(g_ssd_dev, sb[g_ssd_dev]->s_blk_bitmap);

	return 0;
}

static int persist_dirty_objects_hdd(void)
{
	struct rb_node *node;

	sync_all_buffers(g_bdev[g_hdd_dev]);

	store_all_bitmap(g_hdd_dev, sb[g_hdd_dev]->s_blk_bitmap);

	return 0;
}

// digest_blk_count used in NIC-offloading.
static int digest_logs(uint8_t from_dev, int libfs_id, int n_hdrs,
                       addr_t start_blkno, addr_t end_blkno,
                       addr_t *loghdr_to_digest, int *rotated,
                       uint32_t digest_blk_count)
{
	loghdr_meta_t *loghdr_meta;
	int i, n_digest;
	uint64_t tsc_begin;
	uint16_t size;
	struct memcpy_meta_array memcpy_meta_list;
	addr_t current_loghdr_blk, next_loghdr_blk;
	struct replay_list replay_list = {
		.i_digest_hash = NULL,
		.f_digest_hash = NULL,
		.u_digest_hash = NULL,
	};

	INIT_LIST_HEAD(&replay_list.head);

	//memset(inode_version_table, 0, sizeof(uint16_t) * NINODES);

	current_loghdr_blk = next_loghdr_blk = *loghdr_to_digest;

	START_TIMER(evt_coal);

        // DEBUG
        // mlfs_printf("DEBUG DIGEST START peer->rsync_start=%lu peer->digest_start=%lu digest_start_blkno=%lu\n",
                // g_sync_ctx[libfs_id]->peer->local_start,
                // g_sync_ctx[libfs_id]->peer->start_digest,
                // start_blkno);


	// digest log entries
	for (i = 0 ; i < n_hdrs; i++) {
		loghdr_meta = read_log_header(from_dev, *loghdr_to_digest);
                /*
                // DEBUG
                mlfs_log("%s", "--------------------------------\n");
                mlfs_log("%d\n", loghdr_meta->loghdr->n);
                mlfs_log("ts %ld.%06ld\n", loghdr_meta->loghdr->mtime.tv_sec, loghdr_meta->loghdr->mtime.tv_usec);
                mlfs_log("blknr %lu\n", hdr_addr);
                mlfs_log("size %u\n", loghdr_meta->loghdr->nr_log_blocks);
                mlfs_log("inuse %x\n", loghdr_meta->loghdr->inuse);
                for(int i=0; i< loghdr_meta->loghdr->n; i++) {
                        mlfs_log("type[%d]: %u\n", i, loghdr_meta->loghdr->type[i]);
                        mlfs_log("inum[%d]: %u\n", i, loghdr_meta->loghdr->inode_no[i]);
                }
                */
		size = loghdr_meta->loghdr->nr_log_blocks;

		if (loghdr_meta->loghdr->inuse != LH_COMMIT_MAGIC) {
			mlfs_assert(loghdr_meta->loghdr->inuse == 0);
			mlfs_free(loghdr_meta);
                        panic("loghdr_meta->loghdr->inuse != LH_COMMIT_MAGIC");
			break;
		}

#ifdef DIGEST_OPT
		if (enable_perf_stats)
			tsc_begin = asm_rdtscp();

		digest_replay_and_optimize(from_dev, loghdr_meta, &replay_list);

		if (enable_perf_stats)
			g_perf_stats.replay_time_tsc += asm_rdtscp() - tsc_begin;
#else
		digest_each_log_entries(from_dev, libfs_id, loghdr_meta);
#endif

		// rotated when next log header jumps to beginning of the log.
		// FIXME: instead of this condition, it would be better if
		// *loghdr_to_digest > the lost block of application log.

		next_loghdr_blk = next_loghdr_blknr(current_loghdr_blk, size, start_blkno, end_blkno);
		if (current_loghdr_blk > next_loghdr_blk) {
                        mlfs_debug("current header %lu, next header %lu\n",
                                        current_loghdr_blk, next_loghdr_blk);
			*rotated = 1;
		}

		*loghdr_to_digest = next_loghdr_blk;

		current_loghdr_blk = *loghdr_to_digest;

		mlfs_free(loghdr_meta);
	}

	END_TIMER(evt_coal);

#ifdef DIGEST_OPT
	if (enable_perf_stats)
		tsc_begin = asm_rdtscp();

	if (mlfs_conf.digest_opt_host_memcpy) {
#ifdef NIC_OFFLOAD
          /* Allocate digest_blk_count entries. It is the worst case from the
           * perspective of memory usage because it includes other type of
           * blocks like inode or unlink. Besides, the number of memcpy entries
           * might decrease after coalescing. The exact number of memcpy is
           * decided after mlfs_ext_get_blocks() call in digest_file().
           * So, we cannot know the exact number of memcopies in advance.
	   */
          memcpy_meta_list.id = 0;
          memcpy_meta_list.buf = (memcpy_meta_t *)nic_slab_alloc_in_byte(
              sizeof(memcpy_meta_t) * digest_blk_count);

          digest_log_from_replay_list(from_dev, libfs_id, &replay_list,
                                      &memcpy_meta_list);

          nic_slab_free((void *)memcpy_meta_list.buf);
#endif
        } else {
	    digest_log_from_replay_list(from_dev, libfs_id, &replay_list, 0);
	}

	if (enable_perf_stats)
		g_perf_stats.apply_time_tsc += asm_rdtscp() - tsc_begin;
#endif

	n_digest = i;

	if (0) {
		ncx_slab_stat_t slab_stat;
		ncx_slab_stat(mlfs_slab_pool, &slab_stat);
	}

	return n_digest;
}

static void
wait_rsync_done(int libfs_id, addr_t digest_start, uint32_t digest_cnt,
        addr_t start_blkno, addr_t end_blkno)
{
    // There are two cases.
    // 1)             digest_end(ver=0)        rsync_start(ver=0)
    // [log start]---------|-------------------------|-------------[log end]
    //
    // 2)            rsync_start(ver=1)        digest_end(ver=0)
    // [log start]---------|-------------------------|-------------[log end]
    //
    addr_t rsync_start;
    addr_t digest_end;   // end after wrap around
    addr_t digest_end_overflow; // digest_start + digest_cnt
    uint32_t start_version; // version of digest_start
    uint32_t avail_version; // version of log available
    uint32_t end_version;   // version of digest_end

    // It doesn't need for local digestion.
    if(host_kid_to_nic_kid(local_kernfs_id(libfs_id)) == g_self_id)
        return;

    // Don't acquire lock because we are only reading it.
    rsync_start = g_sync_ctx[libfs_id]->peer->local_start;
    digest_end = digest_start + digest_cnt - 1;
    digest_end_overflow = digest_end;

    avail_version = g_sync_ctx[libfs_id]->peer->avail_version;
    start_version = g_sync_ctx[libfs_id]->peer->start_version;

    // Non zero value of end_blkno does not guarantee that wrap around occurs.
    // So, we need to check whether wrap around actually occurs with index.
    if (end_blkno && digest_end_overflow > end_blkno) {
        end_version = start_version + 1;

        // Update digest_end on wrap around.
        digest_end = wrap_arounded_blknr(digest_start, digest_cnt,
                start_blkno, end_blkno);
    } else {
        end_version = start_version;
    }

    /*
    pr_digest("Start waiting for rsync done. libfs_id=%d |DIGEST: start=%lu "
              "cnt=%u end_over=%lu end=%lu |RSYNC: start=%lu |VERSION: "
              "start=%u end=%u avail=%u",
              libfs_id, digest_start, digest_cnt, digest_end_overflow,
              digest_end, rsync_start, start_version, end_version,
              avail_version);
	      */

    // Wait until rsync_start goes further than digest_end.
    while(1) {
        rsync_start = g_sync_ctx[libfs_id]->peer->local_start;
        avail_version = g_sync_ctx[libfs_id]->peer->avail_version;

        if (end_version == avail_version) {
            if (rsync_start > digest_end)
                break;
        } else if (end_version < avail_version) {
            break;
        }
        cpu_relax();
    }

    /*
    pr_digest("End waiting for rsync done. |DIGEST: start=%lu cnt=%u end_over=%lu "
            "end=%lu |RSYNC: start=%lu |VERSION: start=%u end=%u avail=%u",
            digest_start, digest_cnt, digest_end_overflow, digest_end, rsync_start,
            start_version, end_version, avail_version);
            */
}

/**
 * Fake handling. To check replication bottleneck.
 * It only calculates meta data and send response.
 */
static void handle_digest_request_noop(void *arg)
{
	int dev_id, log_id;
	int sock_fd;
	struct digest_arg *digest_arg;
	char *buf, response[MAX_SOCK_BUF];
	char cmd_header[12];
	int rotated = 0;
	int lru_updated = 0;
	addr_t digest_blkno, start_blkno, end_blkno;
	uint32_t digest_count, digest_blk_count;
	addr_t next_hdr_of_digested_hdr;
	int requester_kernfs;
	bool is_last;
	addr_t digest_blkno_orig;

	digest_arg = (struct digest_arg *)arg;

	sock_fd = digest_arg->sock_fd;
	buf = digest_arg->msg;

	sscanf(buf, "|%s |%d|%d|%u|%lu|%lu|%lu|%u|",
			cmd_header, &log_id, &dev_id, &digest_count, &digest_blkno,
                        &start_blkno, &end_blkno, &digest_blk_count);

	digest_blkno_orig = digest_blkno;

	uint32_t overflow = 0;
	if (end_blkno != 0 && digest_blkno_orig + digest_blk_count > end_blkno) {
	    rotated = 1;
	    overflow = digest_blkno_orig + digest_blk_count - end_blkno - 1;
	    digest_blkno_orig = start_blkno + overflow;
	} else {
	    digest_blkno_orig += digest_blk_count;
	}
	digest_blkno = digest_blkno_orig;

	requester_kernfs = host_kid_to_nic_kid(local_kernfs_id(log_id));
	is_last = cur_kernfs_is_last(requester_kernfs, g_self_id);

	if(log_id && host_kid_to_nic_kid(local_kernfs_id(log_id)) == g_self_id) {
#ifdef NIC_OFFLOAD
	    // NOT implemented.
#else
	    // Send ack to libfs.
	    memset(response, 0, MAX_SOCK_BUF);
	    update_peer_digest_state(g_sync_ctx[log_id]->peer, digest_blkno, digest_count, rotated);
	    // TODO do we need to send digest_blk_count?? JYKIM
	    sprintf(response, "|complete |%d|%d|%d|%lu|%d|%d|%u|",
		    log_id, dev_id, digest_count, digest_blkno, rotated, lru_updated, digest_blk_count);
	    pr_digest("%s", "Send digest response to Libfs");
	    rpc_forward_msg(sock_fd, response);
#endif
	} else {
	    //if I'm not last in chain; wait for next kernfs to respond first
	    if(!is_last) {
		pr_digest("wait for KernFS %d to digest", get_next_kernfs(g_self_id));
		START_TIMER(evt_dig_wait_peer);
		wait_on_peer_digesting(g_sync_ctx[log_id]->peer);
		END_TIMER(evt_dig_wait_peer);
	    } else {
		update_peer_digest_state(g_sync_ctx[log_id]->peer, digest_blkno, digest_count, rotated);
	    }
	    pr_digest("digest response to Peer with ID = %d", log_id);
	    // TODO do we need to send digest_blk_count?? JYKIM
	    rpc_remote_digest_response(sock_fd, log_id, dev_id,
		    digest_blkno, digest_count, rotated, digest_arg->seqn, digest_blk_count);
	}

	mlfs_free(arg);
}

static void handle_digest_request(void *arg)
{
	int dev_id, log_id;
	int sock_fd;
	struct digest_arg *digest_arg;
	char *buf, response[MAX_SOCK_BUF];
	char cmd_header[12];
	int rotated = 0;
	int lru_updated = 0;
	addr_t digest_blkno, start_blkno, end_blkno;
	uint32_t digest_count, digest_blk_count;
	addr_t next_hdr_of_digested_hdr;

#ifndef CONCURRENT
	pthread_mutex_lock(&digest_mutex);
#endif

	START_MP_TOTAL_TIMER(evt_dig_req_total);
	START_MP_PER_TASK_TIMER(evt_dig_req_per_task);
        START_TIMER(evt_hdr);

	memset(cmd_header, 0, 12);

	digest_arg = (struct digest_arg *)arg;

	sock_fd = digest_arg->sock_fd;
	buf = digest_arg->msg;

        /* digest_blkno: start_digest
         * start_blkno: first blkno of log area. (superblock number +1)
         * end_blkno: end of the log area.
         * digest_count: # of LOG HEADERS to be digested.
         */
	sscanf(buf, "|%s |%d|%d|%u|%lu|%lu|%lu|%u|",
			cmd_header, &log_id, &dev_id, &digest_count, &digest_blkno,
                        &start_blkno, &end_blkno, &digest_blk_count);
        pr_digest("Digest requested: log_id=%d dev_id=%d n_loghdr_digest=%u "
                "digest_start=%lu log_start=%lu log_end=%lu n_blk_digest=%u",
                log_id, dev_id, digest_count, digest_blkno, start_blkno,
                end_blkno, digest_blk_count);
	PR_METADATA(g_sync_ctx[log_id]);

        /* Wait for previous replication completed.
         * TODO print rsync meta of third node.
         * digest metadata:
         *      digest_blkno
         *      digest_count
         * rsync metadata:
         *      g_sync_ctx[libfs_id]->peer->n_unsync_blk
         *      g_sync_ctx[libfs_id]->peer->local_start
         *          It is updated on signal_callback (if it is the last node)
         *          or in create_rsync() (if it is not the last node).
         */
        wait_rsync_done(log_id, digest_blkno, digest_blk_count, start_blkno, end_blkno);

	mlfs_debug("%s\n", cmd_header);
	if (strcmp(cmd_header, "digest") == 0) {
		pr_digest("Digest command: dev_id=%u digest_blkno=%lu "
                        "digest_count=%u start_blkno=%lu end_blkno=%lu",
                        dev_id, digest_blkno, digest_count, start_blkno,
                        end_blkno);

		if (enable_perf_stats) {
			g_perf_stats.digest_time_tsc = asm_rdtscp();
			g_perf_stats.persist_time_tsc = asm_rdtscp();
			g_perf_stats.path_search_tsc = 0;
			g_perf_stats.replay_time_tsc = 0;
			g_perf_stats.apply_time_tsc= 0;
			g_perf_stats.digest_dir_tsc = 0;
			g_perf_stats.digest_file_tsc = 0;
			g_perf_stats.digest_inode_tsc = 0;
			g_perf_stats.n_digest_skipped = 0;
			g_perf_stats.n_digest = 0;
		}

#ifdef DISTRIBUTED
                int requester_kernfs;
                bool is_last;

		requester_kernfs = host_kid_to_nic_kid(local_kernfs_id(log_id));
                is_last = cur_kernfs_is_last(requester_kernfs, g_self_id);

		// update peer digesting state
		set_peer_digesting(g_sync_ctx[log_id]->peer);

		///////////// Relay digest request to the next node.
#ifdef NIC_OFFLOAD
		if(log_id && !is_last)
#else
		if(log_id && requester_kernfs != g_self_id && !is_last) // no relay on local kernfs
#endif
		{
		    pr_digest("Relay digest to next KernFS with ID=%u",
			    get_next_kernfs(g_self_id));

                    rpc_forward_msg_with_per_libfs_seqn(
                        g_sync_ctx[log_id]->next_digest_sockfd, (char *)buf,
                        log_id);
                }

#endif /* DISTRIBUTED */

		////////////// DIGEST //////////////////
                int digest_cnt_orig;
                digest_cnt_orig = digest_count;
                digest_count = digest_logs(
                    dev_id, log_id, digest_count, start_blkno, end_blkno,
                    &digest_blkno, &rotated, digest_blk_count);

                if (digest_cnt_orig != digest_count){
                    printf("Digest incomplete. requested=%d done=%d\n",
                            digest_cnt_orig, digest_count);
                    panic("Digest incomplete");
                }

		mlfs_debug("-- Total used block %d\n",
				bitmap_weight((uint64_t *)sb[g_root_dev]->s_blk_bitmap->bitmap,
					sb[g_root_dev]->ondisk->ndatablocks));

#ifdef MIGRATION
		try_migrate_blocks(g_root_dev, g_ssd_dev, 0, 0, 1);
#endif

		if (enable_perf_stats)
			g_perf_stats.persist_time_tsc = asm_rdtscp();

		// flush writes to NVM (only relevant if writes are issued asynchronously using a DMA engine)
		mlfs_commit(g_root_dev);

		persist_dirty_objects_nvm(log_id);  // TODO How to sync cached DRAM data to NVM? JYKIM
#ifdef USE_SSD
		persist_dirty_objects_ssd();
#endif
//#ifdef USE_HDD
//		persist_dirty_objects_hdd();
//#endif

		if (enable_perf_stats)
			g_perf_stats.persist_time_tsc =
				(asm_rdtscp() - g_perf_stats.persist_time_tsc);

#ifdef DISTRIBUTED
		///////////////// SEND RESPONSE ///////////////////////////////
		// this is a local peer; respond to libfs.
		if(log_id && host_kid_to_nic_kid(local_kernfs_id(log_id)) == g_self_id) {
#ifdef NIC_OFFLOAD
			if(!is_last) {
			    // Do not send ack to libfs. Local NIC kernfs send ack
			    // to libfs as it receives ack from the next node.
			    pr_digest("wait for KernFS %d to digest", get_next_kernfs(g_self_id));
			    START_TIMER(evt_dig_wait_peer);
			    wait_on_peer_digesting(g_sync_ctx[log_id]->peer);
			    END_TIMER(evt_dig_wait_peer);
			    pr_digest("digest response to Peer with ID = %d", log_id);
			    // TODO do we need to send digest_blk_count?? JYKIM
			    rpc_remote_digest_response(sock_fd, log_id, dev_id,
					    digest_blkno, digest_count, rotated, digest_arg->seqn, digest_blk_count);

			} else {
			    // There is no peer. Send ack to libfs.
			    memset(response, 0, MAX_SOCK_BUF);
			    update_peer_digest_state(g_sync_ctx[log_id]->peer, digest_blkno, digest_count, rotated);
			    // TODO do we need to send digest_blk_count?? JYKIM
			    sprintf(response, "|complete |%d|%d|%d|%lu|%d|%d|%u|",
					    log_id, dev_id, digest_count, digest_blkno, rotated, lru_updated, digest_blk_count);
			    pr_digest("%s", "Send digest response to Libfs");
			    rpc_forward_msg_no_seqn(sock_fd, response);
			}
#else
			// Send ack to libfs.
			memset(response, 0, MAX_SOCK_BUF);
			update_peer_digest_state(g_sync_ctx[log_id]->peer, digest_blkno, digest_count, rotated);
			// TODO do we need to send digest_blk_count?? JYKIM
			sprintf(response, "|complete |%d|%d|%d|%lu|%d|%d|%u|",
					log_id, dev_id, digest_count, digest_blkno, rotated, lru_updated, digest_blk_count);
			pr_digest("%s", "Send digest response to Libfs");
			rpc_forward_msg_no_seqn(sock_fd, response);
#endif
		} else {
			//if I'm not last in chain; wait for next kernfs to respond first
			if(!is_last) {
				pr_digest("wait for KernFS %d to digest", get_next_kernfs(g_self_id));
				START_TIMER(evt_dig_wait_peer);
				wait_on_peer_digesting(g_sync_ctx[log_id]->peer);
				END_TIMER(evt_dig_wait_peer);
			} else {
				update_peer_digest_state(g_sync_ctx[log_id]->peer, digest_blkno, digest_count, rotated);
			}
			pr_digest("digest response to Peer with ID = %d", log_id);
			// TODO do we need to send digest_blk_count?? JYKIM
			rpc_remote_digest_response(sock_fd, log_id, dev_id,
					digest_blkno, digest_count, rotated, digest_arg->seqn, digest_blk_count);
		}

#if MLFS_LEASE  // NIC kernfs does not support LEASE yet. TODO JYKIM
		clear_lease_checkpoints(log_id, g_sync_ctx[log_id]->peer->start_version, g_sync_ctx[log_id]->peer->start_digest);
#endif
#else
		memset(response, 0, MAX_SOCK_BUF);
		sprintf(response, "|ACK |%d|%d|%d|%lu|%d|%d|",
				log_id, dev_id, digest_count, digest_blkno, rotated, lru_updated);
		mlfs_info("Write %s to libfs\n", response);
		sendto(sock_fd, response, MAX_SOCK_BUF, 0,
				(struct sockaddr *)&digest_arg->cli_addr, sizeof(struct sockaddr_un));
#endif /* DISTRIBUTED */

#if MLFS_LEASE
		/*
		sprintf(response, "|NOTIFY |%d|", 1);

		// notify any other local libfs instances
		for(int i=0; i<g_n_hot_rep; i++) {

			if(sockaddr_cmp((struct sockaddr *)&digest_arg->cli_addr,
					       (struct sockaddr *)&all_cli_addr[i])) {
				sendto(sock_fd, response, MAX_SOCK_BUF, 0,
					(struct sockaddr *)&all_cli_addr[i], sizeof(struct sockaddr_un));
			}
		}
		*/
#endif

		if (enable_perf_stats)
			g_perf_stats.digest_time_tsc =
				(asm_rdtscp() - g_perf_stats.digest_time_tsc);

#ifdef MLFS_PRINTF
		// show_storage_stats();
#endif

		if (enable_perf_stats)
			show_kernfs_stats();

	} else if (strcmp(cmd_header, "lru") == 0) {
		// only used for debugging.
		if (0) {
			lru_node_t *l;
			list_for_each_entry(l, &lru_heads[g_log_dev], list)
				mlfs_info("%u - %lu\n",
						l->val.inum, l->val.lblock);
		}
	} else {
		panic("invalid command\n");
	}

	END_MP_PER_TASK_TIMER(evt_dig_req_per_task);
	END_MP_TOTAL_TIMER(evt_dig_req_total);

#ifndef CONCURRENT
	pthread_mutex_unlock(&digest_mutex);
#endif
	mlfs_free(arg);

	END_TIMER(evt_hdr);
}

#define MAX_EVENTS 4
static void wait_for_event(void)
{
	int sock_fd, epfd, flags, n, ret;
	struct sockaddr_un addr;
	struct epoll_event epev = {0};
	struct epoll_event *events;
	int i;

	if ((sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
		panic ("socket error");

	memset(&addr, 0, sizeof(addr));

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, SRV_SOCK_PATH, sizeof(addr.sun_path));

	unlink(SRV_SOCK_PATH);

	if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
		panic("bind error");

	// make it non-blocking
	flags = fcntl(sock_fd, F_GETFL, 0);
	flags |= O_NONBLOCK;
	ret = fcntl(sock_fd, F_SETFL, flags);
	if (ret < 0)
		panic("fail to set non-blocking mode\n");

	epfd = epoll_create(1);
	epev.data.fd = sock_fd;
	epev.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP;
	ret = epoll_ctl(epfd, EPOLL_CTL_ADD, sock_fd, &epev);
	if (ret < 0)
		panic("fail to connect epoll fd\n");

	events = mlfs_zalloc(sizeof(struct epoll_event) * MAX_EVENTS);

	while(1) {
		n = epoll_wait(epfd, events, MAX_EVENTS, -1);

		if (n < 0 && errno != EINTR)
			panic("epoll has error\n");

		for (i = 0; i < n; i++) {
			/*
			if ((events[i].events & EPOLLERR) ||
					(events[i].events & EPOLLHUP) ||
					(!(events[i].events & EPOLLIN)))
			{
				fprintf (stderr, "epoll error\n");
				continue;
			}
			*/

			if ((events[i].events & EPOLLIN) &&
					events[i].data.fd == sock_fd) {
				int ret;
				char buf[MAX_SOCK_BUF];
				char cmd_header[12];
				uint32_t dev_id;
				uint32_t digest_count;
				addr_t digest_blkno, start_blkno, end_blkno;
				struct sockaddr_un cli_addr;
				socklen_t len = sizeof(struct sockaddr_un);
				struct digest_arg *digest_arg;

				ret = recvfrom(sock_fd, buf, MAX_SOCK_BUF, 0,
						(struct sockaddr *)&cli_addr, &len);

				// When clients hang up, the recvfrom returns 0 (EOF).
				if (ret == 0)
					continue;

				mlfs_info("GET: %s\n", buf);

				memset(cmd_header, 0, 12);
				sscanf(buf, "|%s |%d|%u|%lu|%lu|%lu|",
						cmd_header, &dev_id, &digest_count, &digest_blkno, &start_blkno, &end_blkno);

				if(cmd_header[0] == 'd') {
					digest_arg = (struct digest_arg *)mlfs_alloc(sizeof(struct digest_arg));
					digest_arg->sock_fd = sock_fd;
					digest_arg->cli_addr = cli_addr;
					memmove(digest_arg->msg, buf, MAX_SOCK_BUF);

#ifdef CONCURRENT
                                        thpool_add_work(thread_pool_digest, handle_digest_request, (void *)digest_arg);
#else
					handle_digest_request((void *)digest_arg);
#endif

#ifdef MIGRATION
					/*
					thpool_wait(thread_pool);
					thpool_wait(thread_pool_ssd);
					*/

					//try_writeback_blocks();
					//try_migrate_blocks(g_root_dev, g_ssd_dev, 0, 0, 1);
					//try_migrate_blocks(g_root_dev, g_hdd_dev, 0);
#endif
				}
#if MLFS_LEASE
				else if(cmd_header[0] == 'a') {
					//store client address
					all_cli_addr[addr_idx] = cli_addr;
					addr_idx++;
				}
#endif
				else {
					printf("received cmd: %s\n", cmd_header);
					panic("unidentified code path!");
				}
			} else {
				mlfs_info("%s\n", "Huh?");
			}
		}
	}

	close(epfd);
}

void shutdown_fs(void)
{
#ifdef PROFILE_THPOOL
	print_all_thpool_profile_results();
	print_rep_profile_result();
#endif

	printf("Finalize FS\n");
	device_shutdown();
	mlfs_commit(g_root_dev);
	exit(0);
}

#ifdef USE_SLAB
void mlfs_slab_init(uint64_t pool_size)
{
	uint8_t *pool_space;

	// Transparent huge page allocation.
	pool_space = (uint8_t *)mmap(0, pool_size, PROT_READ|PROT_WRITE,
			MAP_PRIVATE|MAP_ANONYMOUS|MAP_POPULATE, -1, 0);

	mlfs_assert(pool_space);

	if(madvise(pool_space, pool_size, MADV_HUGEPAGE) < 0)
		panic("cannot do madvise for huge page\n");

	mlfs_slab_pool = (ncx_slab_pool_t *)pool_space;
	mlfs_slab_pool->addr = pool_space;
	mlfs_slab_pool->min_shift = 3;
	mlfs_slab_pool->end = pool_space + pool_size;

	ncx_slab_init(mlfs_slab_pool);
}
#endif

void debug_init(void)
{
#ifdef MLFS_LOG
	log_fd = open(LOG_PATH, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
#endif
}

#if 0
static void shared_memory_init(void)
{
	int ret;

	shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
	if (shm_fd == -1)
		panic("cannot create shared memory\n");

	ret = ftruncate(shm_fd, SHM_SIZE);
	if (ret == -1)
		panic("cannot ftruncate shared memory\n");

	shm_base = (uint8_t *)mmap(SHM_START_ADDR,
			SHM_SIZE + 4096,
			PROT_READ| PROT_WRITE,
			MAP_SHARED | MAP_POPULATE | MAP_FIXED,
			shm_fd, 0);

	printf("shm mmap base %p\n", shm_base);
	if (shm_base == MAP_FAILED) {
		printf("error: %s\n", strerror(errno));
		panic("cannot map shared memory\n");
	}

	// the first 4 KB is reserved.
	mlfs_slab_pool_shared = (ncx_slab_pool_t *)(shm_base + 4096);
	mlfs_slab_pool_shared->addr = shm_base + 4096;
	mlfs_slab_pool_shared->min_shift = 3;
	mlfs_slab_pool_shared->end = shm_base + SHM_SIZE - 4096;

	ncx_slab_init(mlfs_slab_pool_shared);

	bandwidth_consumption = (uint64_t *)shm_base;
	lru_heads = (struct list_head *)shm_base + 128;

	return;
}
#endif

void init_device_lru_list(void)
{
	int i;

	for (i = 1; i < g_n_devices + 1; i++) {
		memset(&g_lru[i], 0, sizeof(struct lru));
		INIT_LIST_HEAD(&g_lru[i].lru_head);
		g_lru_hash[i] = NULL;

#ifdef MLFS_REPLICA
		memset(&g_stage_lru[i], 0, sizeof(struct lru));
		INIT_LIST_HEAD(&g_stage_lru[i].lru_head);

		memset(&g_swap_lru[i], 0, sizeof(struct lru));
		INIT_LIST_HEAD(&g_swap_lru[i].lru_head);
#endif
	}

	pthread_spin_init(&lru_spinlock, PTHREAD_PROCESS_SHARED);

	return;
}

void locks_init(void)
{
	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);

	pthread_mutex_init(&block_bitmap_mutex, &attr);
	mlfs_ext_init_locks();

#ifndef CONCURRENT
	pthread_mutex_init(&digest_mutex, &attr); //prevent concurrent digests
#endif
}

#ifdef NIC_OFFLOAD
void get_superblock(uint8_t dev, struct disk_superblock* sbp)
{
	int ret;
	struct buffer_head *bh;

	bh = bh_get_sync_IO(dev, 1, BH_NO_DATA_ALLOC);
	bh->b_size = g_block_size_bytes;
	bh->b_data = mlfs_zalloc(g_block_size_bytes);

	bh_submit_read_sync_IO(bh);
	mlfs_io_wait(dev, 1);

	if (!bh)
		panic("cannot read superblock\n");

	mlfs_debug("size of superblock %ld\n", sizeof(struct disk_superblock));

	memmove(sbp, bh->b_data, sizeof(struct disk_superblock));

	mlfs_info("superblock(dev:%d): size %lu nblocks %lu ninodes %u\n"
			"[inode start %lu bmap start %lu datablock start %lu log start %lu]\n",
                        dev,
			sbp->size,
			sbp->ndatablocks,
			sbp->ninodes,
			sbp->inode_start,
			sbp->bmap_start,
			sbp->datablock_start,
			sbp->log_start);
	mlfs_free(bh->b_data);
	bh_release(bh);
}

void handle_tcp_client_req(int cli_sock_fd)
{
    char buf[MAX_SOCK_BUF+1] = {0};
    char cmd_hdr[TCP_HDR_SIZE] = {0};
    char resp_buf[TCP_BUF_SIZE+1] = {0};
    int n, tcp_connected;
    int dev;

    tcp_connected = 1;
    while (tcp_connected) {
        if ((n = read(cli_sock_fd, buf, MAX_SOCK_BUF)) > 0) {  // read cmd.
            sscanf(buf, "|%s |", cmd_hdr);
            buf[n] = '\0';
            mlfs_info("received tcp with body: %s\n", buf);

            switch(cmd_hdr[0]) {
                case 's':   // sb : Request superblock.
                    printf ("Server: cmd_hdr is s\n");

                    sscanf(buf, "|%s |%d|", cmd_hdr, &dev);

                    // Read ondisk superblock.
                    memset(resp_buf, 0, sizeof(resp_buf));
                    get_superblock(dev, (struct disk_superblock*)resp_buf);
                    mlfs_info("Server: superblock data: %s\n", resp_buf);
                    mlfs_assert(sizeof(struct disk_superblock) <= TCP_BUF_SIZE);

                    struct disk_superblock *sb;
                    sb = (struct disk_superblock*)resp_buf;
                    mlfs_info("Server send resp: superblock: size %lu nblocks %lu ninodes %u\n"
                            "\t[inode start %lu bmap start %lu datablock start %lu log start %lu]\n",
                            sb->size,
                            sb->ndatablocks,
                            sb->ninodes,
                            sb->inode_start,
                            sb->bmap_start,
                            sb->datablock_start,
                            sb->log_start);

                    // Send response to client.
                    tcp_send_client_resp(cli_sock_fd, resp_buf, sizeof(struct disk_superblock));
                    break;

                case 'r':   // rinode :  Request root inode.
                    mlfs_info ("%s", "Server: cmd_hdr is r\n");
                    // Read ondisk root_inode.
                    memset(resp_buf, 0, sizeof(resp_buf));
                    read_ondisk_inode(ROOTINO, (struct dinode*)resp_buf);

                    mlfs_info("Server: ROOTINO:%d\n", ROOTINO);
                    mlfs_info("Server: root_inode data size: %ld\n", sizeof(struct dinode));
                    mlfs_assert(sizeof(struct dinode) <= TCP_BUF_SIZE);

                    struct dinode *di;
                    di = (struct dinode*)resp_buf;
                    /*
                    mlfs_info("Server send resp: dinode: "
                            "\t[ itype %d nlink %d size %lu atime %ld.%06ld ctime %ld.%06ld mtime %ld.%06ld]\n",
                            di->itype,
                            di->nlink,
                            di->size,
                            di->atime.tv_sec,
                            di->atime.tv_usec,
                            di->ctime.tv_sec,
                            di->ctime.tv_usec,
                            di->mtime.tv_sec,
                            di->mtime.tv_usec);
                    */

                    mlfs_assert(di->itype == T_DIR);

                    // Send response to client.
                    tcp_send_client_resp(cli_sock_fd, resp_buf, sizeof(struct dinode));
                    break;
                case 'b':   // bitmapb : Request bitmap block.
                    mlfs_info("%s", "Server: cmd_hdr is b\n");
                    int err;
                    mlfs_fsblk_t blk_nr;

                    sscanf(buf, "|%s |%d|%lu|", cmd_hdr, &dev, &blk_nr);

                    // Read ondisk block.
                    memset(resp_buf, 0, sizeof(resp_buf));
                    err = read_bitmap_block(dev, blk_nr, (uint8_t*)resp_buf);
                    if (err) {
                        panic("Server: Error: cannot read bitmap block.\n");
                    }

                    tcp_send_client_resp(cli_sock_fd, resp_buf, g_block_size_bytes);
                    break;

                case 'c':   // close : Close TCP connection.
                    // tcp connection should be closed in the caller of this function.
                    tcp_connected = 0;
                    break;

                case 'd':   // devba : Request device base address.
                    mlfs_info("%s", "Server: cmd_hdr is b\n");
                    sscanf(buf, "|%s |%d", cmd_hdr, &dev);

                    memset(resp_buf, 0, sizeof(resp_buf));
                    memmove(resp_buf, &g_bdev[dev]->map_base_addr, sizeof(uint8_t*));
                    mlfs_info("Server: dev base addr: %p\n", g_bdev[dev]->map_base_addr);
                    tcp_send_client_resp(cli_sock_fd, resp_buf, sizeof(uint8_t*));
                    break;

                default:
                    printf ("Error: Unknown cmd_hdr for tcp connection.\n");
                    break;
            }
        }
    }
}

void set_dram_superblock(uint8_t dev)
{
	sb[dev]->ondisk = &disk_sb[dev];

	// set all rb tree roots to NULL
	for(int i=0; i<(MAX_LIBFS_PROCESSES+g_n_nodes); i++)
		sb[dev]->s_dirty_root[i] = RB_ROOT;

	sb[dev]->last_block_allocated = 0;

	// The partition is GC unit (1 GB) in SSD.
	// disk_sb[dev].size : total # of blocks
#if 0
	sb[dev]->n_partition = disk_sb[dev].size >> 18;
	if (disk_sb[dev].size % (1 << 18))
		sb[dev]->n_partition++;
#endif

	sb[dev]->n_partition = 1;

	//single partitioned allocation, used for debugging.
	mlfs_info("dev %u: # of segment %u\n", dev, sb[dev]->n_partition);
	sb[dev]->num_blocks = disk_sb[dev].ndatablocks;
	sb[dev]->reserved_blocks = disk_sb[dev].datablock_start;
	sb[dev]->s_bdev = g_bdev[dev];
}

#endif /* NIC_OFFLOAD */

#ifdef SIGNAL_HANDLER
void signal_shutdown_fs(int signum)
{
	printf("received shutdown signal [signum:%d]\n", signum);
	shutdown_fs();
}
#endif

/**
 * @Synopsis  Periodically send heartbeat signal to its local NIC kernFS.
 */
void send_heartbeat (void* arg) {

	int sockfd;
	uint64_t seqn = 1;

	// Get sockfd after connection is established.
	while(1) {
		sockfd = g_kernfs_peers[host_kid_to_nic_kid(g_kernfs_id)]
			->sockfd[SOCK_BG];
		if (sockfd != -1)
			break;
	}

	// heartbeat loop
	while(1) {
		usleep(500*1000); // every 0.5 second
		rpc_send_heartbeat(sockfd, seqn++);
	}
}

void init_fs(void)
{
	int i;
	const char *perf_profile;

#ifdef SIGNAL_HANDLER
	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_handler = signal_shutdown_fs;
	sigaction(SIGINT, &action, NULL);
#endif

	load_mlfs_configs();
	print_mlfs_configs();

	g_ssd_dev = 2;
	g_hdd_dev = 3;
	g_log_dev = 4;

#ifdef PROFILE_THPOOL
	if (mlfs_conf.thread_num_rep)
		rep_th_stats = (struct rep_th_stat *)mlfs_zalloc(
			sizeof(struct rep_th_stat) * mlfs_conf.thread_num_rep);
#endif

#ifdef USE_SLAB
	mlfs_slab_init(3UL << 30);
#endif

	device_init();

	debug_init();

	init_device_lru_list();

	//shared_memory_init();

	cache_init(g_root_dev);

	locks_init();

	for (i = 0; i < g_n_devices + 1; i++)
		sb[i] = mlfs_zalloc(sizeof(struct super_block));

        // Setup node ids and g_self_id.
        set_self_ip();

        // Set g_node_id and g_host_node_id.
        // set_self_ip() should be called before calling it.
        set_self_node_id();

#ifdef NIC_OFFLOAD // TODO Currently, metadata is managed by host kernfs. It will be managed by NIC kernfs. JYKIM.
        // Setup TCP socket for getting metadata from host.
        char buf[TCP_BUF_SIZE+1] = {0};
        int cli_sock_fd;

        int serv_sock_fd;

	read_superblock(g_root_dev);    // FIXME need to be called for read_root_inode().
	read_superblock(g_log_dev);

        // Setup server tcp socket.
        tcp_setup_server(&serv_sock_fd, &cli_sock_fd);

        // Handle client request.
        handle_tcp_client_req(cli_sock_fd);

        // Close client and server sockets.
        tcp_close(cli_sock_fd);
        tcp_close(serv_sock_fd);

        // read_superblock(g_root_dev);
	read_root_inode(g_root_dev);
	balloc_init(g_root_dev, sb[g_root_dev], 0);

        /* TODO SSD and HDD are not supported with SmartNIC
#ifdef USE_SSD
	read_superblock(g_ssd_dev);
	balloc_init(g_ssd_dev, sb[g_ssd_dev], 0);
#endif

#ifdef USE_HDD
	read_superblock(g_hdd_dev);
	balloc_init(g_hdd_dev, sb[g_hdd_dev], 0);
#endif
         */

#else /* Not NIC_OFFLOAD */
	read_superblock(g_root_dev);
	read_root_inode(g_root_dev);
	balloc_init(g_root_dev, sb[g_root_dev], 0);

#ifdef USE_SSD
	read_superblock(g_ssd_dev);
	balloc_init(g_ssd_dev, sb[g_ssd_dev], 0);
#endif

#ifdef USE_HDD
	read_superblock(g_hdd_dev);
	balloc_init(g_hdd_dev, sb[g_hdd_dev], 0);
#endif

        read_superblock(g_log_dev);

#endif /* NIC_OFFLOAD */

	mlfs_assert(g_log_size * MAX_LIBFS_PROCESSES <= disk_sb[g_log_dev].nlog);

	memset(&g_perf_stats, 0, sizeof(kernfs_stats_t));

	digest_init();

	perf_profile = getenv("MLFS_PROFILE");

	if (perf_profile)
		enable_perf_stats = 1;
	else
		enable_perf_stats = 0;

#ifdef KERNFS
	mlfs_debug("%s\n", "KERNFS is initialized");
#else
	mlfs_debug("%s\n", "LIBFS is initialized");
#endif

	if (mlfs_conf.thread_num_digest) {
	    thread_pool_digest =
		thpool_init(mlfs_conf.thread_num_digest, "digest");
	}
	// A fixed thread for using SPDK.
	thread_pool_ssd = thpool_init(1, "ssd");

#ifdef NVM_WRITE_CONCURRENT
        thread_pool_nvm_write = thpool_init(8);
#endif

        thread_pool_misc = thpool_init(1, "misc");

#ifdef BACKUP_RDMA_MEMCPY
	thread_pool_send_heartbeat = thpool_init(1, "heartbeat");
#endif

	if (mlfs_conf.thread_num_rep) {
#ifdef PER_LIBFS_REPLICATION_THREAD
	    replicate_thpools = mlfs_zalloc(sizeof(threadpool) * mlfs_conf.thread_num_rep);
	    // One pool for one libfs.
	    char tp_name[64] = {0};
	    for (int i = 0; i < mlfs_conf.thread_num_rep; i++) {
		sprintf(tp_name, "rep%d", i);
		replicate_thpools[i] = thpool_init_no_sleep(1, tp_name);
	    }
#ifdef PROFILE_THPOOL
	    printf("(Warn) PROFILE_THPOOL is not supported with PER_LIBFS_REPLICATION_THREAD.\n");
#endif
#else
	    thread_pool_replicate = thpool_init(mlfs_conf.thread_num_rep, "rep");

#ifdef PROFILE_THPOOL
	    for (int i = 0; i < mlfs_conf.thread_num_rep; i++) {
		    rep_th_stats[i].schedule_cnt = 0;
		    rep_th_stats[i].schedule_delay_sum = 0.0;
		    rep_th_stats[i].schedule_delay_max = 0.0;
		    rep_th_stats[i].schedule_delay_min = 99999.99; // large value.
		    rep_th_stats[i].wait_seqn_delay_sum = 0.0;
		    rep_th_stats[i].wait_seqn_delay_max = 0.0;
		    rep_th_stats[i].wait_seqn_delay_min = 99999.99; // large value.
	    }
#endif

#endif /* PER_LIBFS_REPLICATION_THREAD */
	}


#ifdef NIC_OFFLOAD

	if (mlfs_conf.thread_num_digest_host_memcpy){
	    thread_pool_host_memcpy = thpool_init(
		    mlfs_conf.thread_num_digest_host_memcpy, "host_memcpy");
        }
	thread_pool_persist_log = thpool_init(
			mlfs_conf.thread_num_persist_log, "flush_log");
#endif

#ifdef DISTRIBUTED
	bitmap_set(g_log_bitmap, 0, MAX_LIBFS_PROCESSES);

	mlfs_assert(disk_sb[g_log_dev].nlog >= g_log_size * g_n_hot_rep);
	//for(int i=0; i<g_n_hot_rep; i++)
	//	init_log(i);

	//set memory regions to be registered by rdma device
	int n_regions = 3;
	struct mr_context *mrs = (struct mr_context *) mlfs_zalloc(sizeof(struct mr_context) * n_regions);

        int ret;
	for(int i=0; i<n_regions; i++) {
		switch(i) {
			case MR_NVM_LOG: {

				//FIXME: share all log entries; current impl shares only a single log mr
				mrs[i].type = MR_NVM_LOG;
				mrs[i].addr = (uint64_t)g_bdev[g_log_dev]->map_base_addr;
				mrs[i].length = ((disk_sb[g_log_dev].nlog) << g_block_size_shift);
				break;
			}
			case MR_NVM_SHARED: {
				mrs[i].type = MR_NVM_SHARED;
				mrs[i].addr = (uint64_t)g_bdev[g_root_dev]->map_base_addr;
				//mrs[i].length = dev_size[g_root_dev];
				//mrs[i].length = (1UL << 30);
                                mrs[i].length = (sb[g_root_dev]->ondisk->size << g_block_size_shift);   // All possible address of device. Ref) mkfs.c
				//mrs[i].length = (disk_sb[g_root_dev].datablock_start - disk_sb[g_root_dev].inode_start << g_block_size_shift);  // data blocks only.
				break;
			}
			/*
			case MR_DRAM_CACHE: {
				mrs[i].type = MR_DRAM_CACHE;
				mrs[i].addr = (uint64_t) g_fcache_base;
				mrs[i].length = (g_max_read_cache_blocks << g_block_size_shift);
				break;
			}
			*/

			case MR_DRAM_BUFFER: {
				// Check nic_slab_pool does not exceed 12GB. Note that the size of
				// DRAM in SmartNIC is 16GB.
				mlfs_assert((g_max_nicrpc_buf_blocks << g_block_size_shift)
					<= (12ULL*1024*1024*1024));
				// TODO need to adjust buf size.
				nic_slab_init((g_max_nicrpc_buf_blocks << g_block_size_shift)/4);
				mrs[i].length = (g_max_nicrpc_buf_blocks << g_block_size_shift)/4;
				mrs[i].type = MR_DRAM_BUFFER;
				mrs[i].addr = (uint64_t) nic_slab_pool->addr;

				pr_dram_alloc("[DRAM_ALLOC] MR_DRAM_BUFFER size=%lu(MB)", mrs[i].length/1024/1024);
                                break;
			}
			default:
				break;
		}
                printf("mrs[%d] type %d, addr 0x%lx - 0x%lx, length %lu(%lu MB)\n",
                       i, mrs[i].type, mrs[i].addr, mrs[i].addr + mrs[i].length,
                       mrs[i].length, mrs[i].length / 1024 / 1024);
        }

#if defined(NIC_OFFLOAD) & !defined(NIC_SIDE) // host kernfs.
	// Allocate contiguous region for all the per-libfs host memcpy buffers.
	host_memcpy_buf_p[0] = nic_slab_alloc_in_byte(
		sizeof(memcpy_meta_t) * g_memcpy_meta_cnt *
		(g_n_nodes + MAX_LIBFS_PROCESSES));
	// printf("host_memcpy_buf_p[0]=%p\n", host_memcpy_buf_p[0]);

	// Set each buf pointer.
	for (int i = 1; i < g_n_nodes + MAX_LIBFS_PROCESSES; i++) {
		host_memcpy_buf_p[i] = host_memcpy_buf_p[i - 1] +
				       g_memcpy_meta_cnt; // pointer addition
		// printf("host_memcpy_buf_p[%d]=%p\n", i, host_memcpy_buf_p[i]);
	}
#endif

	// initialize rpc module
	init_rpc(mrs, n_regions, mlfs_conf.port, signal_callback, NULL);

#ifdef BACKUP_RDMA_MEMCPY
	thpool_add_work(thread_pool_send_heartbeat, send_heartbeat, NULL);
#endif

        // Print global variables.
        print_rpc_setup();
        print_sync_ctx();
        print_g_peers();
        print_g_kernfs_peers();

#endif

#ifdef PROFILE_REALTIME_MEMCPY_BW
	init_rt_bw_stat(&memcpy_bw_stat, "host_memcpy");
#endif

	wait_for_event();
}

#ifdef NVM_WRITE_CONCURRENT     // Used in host side. Deprecated.
struct nvm_write_arg
{
    int msg_sockfd;
    uint32_t msg_id;
    int dev;
    // uint8_t *buf_addr;
    addr_t blockno;
    uint32_t io_size;
    uint32_t offset;
};

static void nvm_write_worker(void *arg)
{
    struct nvm_write_arg *nw_arg = (struct nvm_write_arg *)arg;

    /* To make all writes synchronous. Currently, response is sent as nvm_write_worker runs.
    // setup response for ACK.
    int buffer_id = rpc_nicrpc_setup_response(nw_arg->msg_sockfd, "|nrresp |", nw_arg->msg_id);
    */

    int ret;
    // write data.
    if (nw_arg->offset) {
        ret = g_bdev[nw_arg->dev]->storage_engine->write_opt_unaligned(nw_arg->dev,
                nw_arg->buf_addr, nw_arg->blockno, nw_arg->offset, nw_arg->io_size);
    } else {
        ret = g_bdev[nw_arg->dev]->storage_engine->write_opt(nw_arg->dev,
                nw_arg->buf_addr, nw_arg->blockno, nw_arg->io_size);
    }
    if (ret != nw_arg->io_size)
        panic ("Failed to write storage: Write size and io_size mismatch.\n");

    /* To make all writes synchronous.
    // send response.
    rpc_nicrpc_send_response(nw_arg->msg_sockfd, buffer_id);
    */

    mlfs_free(arg);
}

#endif /* NVM_WRITE_CONCURRENT */

static void print_memcpy_meta (memcpy_meta_t *meta, uint64_t i)
{
	printf("%lu HOST_MEMCPY i=%4lu is_single_blk=%d to_dev=%u blk_nr=%lu "
	       "data=%p size=%d offset=%d\n",
	       get_tid(), i, meta->is_single_blk, meta->to_dev, meta->blk_nr,
	       meta->data, meta->size, meta->offset_in_blk);
}

/**
 * @Synopsis
 *
 * @Param libfs_id
 * @Param seqn Fetch sequence number.
 * @Param digest_start_blknr.
 * @Param n_digested The number of digested log headers before coalesced.
 * @Param n_digested_blks The number of digested blocks before coalesced.
 * @Param reset_meta Whether it is the first log group after rotation. It is
 * passed from libfs.
 */
static void send_publish_ack_to_libfs(int libfs_id, uint64_t fetch_seqn,
				      addr_t digest_start_blknr,
				      uint32_t n_digested,
				      uint64_t n_digested_blks, int reset_meta)
{
	char msg[MAX_SIGNAL_BUF];
	sprintf(msg, "|" TO_STR(RPC_PUBLISH_ACK) " |%d|%lu|%lu|%u|%lu|%d",
		libfs_id, fetch_seqn, digest_start_blknr, n_digested,
		n_digested_blks, reset_meta);
	rpc_forward_msg_with_per_libfs_seqn(g_peers[libfs_id]->sockfd[SOCK_BG],
					    msg, libfs_id);
}

/**
 * @Synopsis
 *
 * @Param libfs_id
 * @Param seqn Fetch sequence number.
 * @Param digest_start_blknr.
 * @Param n_digested The number of digested log headers before coalesced.
 * @Param n_digested_blks The number of digested blocks before coalesced.
 * @Param reset_meta Whether it is the first log group after rotation. It is
 * passed from libfs.
 * @Param sge_cnt The number of batched requests.
 */
static void send_batched_publish_ack_to_libfs(int libfs_id, uint64_t fetch_seqn,
				      addr_t digest_start_blknr,
				      uint32_t n_digested,
				      uint64_t n_digested_blks, int reset_meta, int sge_cnt)
{
	char msg[MAX_SIGNAL_BUF];
	sprintf(msg, "|" TO_STR(RPC_PUBLISH_ACK) " |%d|%lu|%lu|%u|%lu|%d|%d",
		libfs_id, fetch_seqn, digest_start_blknr, n_digested,
		n_digested_blks, reset_meta, sge_cnt);
	rpc_forward_msg_with_per_libfs_seqn(g_peers[libfs_id]->sockfd[SOCK_BG],
					    msg, libfs_id);
}

static void memcpy_worker (void *arg)
{
    struct mc_arg *m_arg;
    int ret, i;
    struct buffer_head *bh_data;
    memcpy_meta_t *meta;
    memcpy_meta_t *meta_buf;

    m_arg = (struct mc_arg*)arg;

    pr_pipe("HOST MEMCPY libfs_id=%d fetch_seqn=%lu n_memcpy=%lu",
                m_arg->libfs_id, m_arg->fetch_seqn, m_arg->n_memcpy_entries);

    // Do memcpy.
    // TODO OPTIMIZE: parallelize it. Consider bad NVM random write performance.
    // printf("%lu HOST_MEMCPY %-6s %-6s %-6s %-20s %-20s %-6s %-6s\n", get_tid(), "i",
    //        "is_sb", "to_dev", "blknr", "*data", "size", "offset");

    meta_buf = host_memcpy_buf_p[m_arg->libfs_id]; // get buf start.

#ifdef PROFILE_REALTIME_MEMCPY_BW
    uint64_t total_sent_size = 0;
#endif

    for (i = 0; i < m_arg->n_memcpy_entries; i++) {
	meta = &meta_buf[i];

	// print_memcpy_meta(meta, i);

	bh_data = bh_get_sync_IO(g_root_dev, meta->blk_nr, BH_NO_DATA_ALLOC);
	mlfs_assert(bh_data);

	bh_data->b_data = meta->data;
	bh_data->b_size = meta->size;
	if (meta->is_single_blk)
	    bh_data->b_offset = meta->offset_in_blk;

#ifndef DIGEST_MEMCPY_NO_COPY
	ret = mlfs_write_opt(bh_data);
#endif
	mlfs_assert(!ret);
	if (!meta->is_single_blk)
	    clear_buffer_uptodate(bh_data); // XXX What does this function do?
	bh_release(bh_data);

#ifdef PROFILE_REALTIME_MEMCPY_BW
	total_sent_size += meta->size;
#endif
    }

    if (mlfs_conf.persist_nvm_with_clflush) {
	    // Update digest_end_blknr. It is used in flushing log.
	    digest_end_blknrs[m_arg->libfs_id] =
		    ((uintptr_t)meta->data + meta->size - 1 -
		     (uintptr_t)g_bdev[g_log_dev]->map_base_addr) >>
		    g_block_size_shift;

	    // mlfs_printf("digest_end_blknrs[%d]=%lu meta->data=%p\n",
	    // m_arg->libfs_id,
	    //             digest_end_blknrs[m_arg->libfs_id], meta->data);
    }

#ifdef PROFILE_REALTIME_MEMCPY_BW
    check_rt_bw(&memcpy_bw_stat, total_sent_size);
#endif

    // NIC kernfs doesn't need to send commit request because NIC
    // kernfs issues a memcpy request to memcpy worker and awaits
    // its completion.
    //
    // flush writes to NVM (only relevant if writes are issued asynchronously
    // using a DMA engine)
    // mlfs_commit(g_root_dev);

    if (is_local_kernfs(m_arg->libfs_id, g_kernfs_id)) {
	    // Send Ack to Libfs.
	    // NOTE n_digested and n_memcpy_entries can have different value due
	    // to coalescing. (n_digested: before coalescing, n_memcpy_entries:
	    // after coalescing.)
#ifdef BATCH_MEMCPY_LIST
	    send_batched_publish_ack_to_libfs(
		    m_arg->libfs_id, m_arg->fetch_seqn,
		    m_arg->digest_start_blknr, m_arg->n_digested,
		    m_arg->n_digested_blks, m_arg->reset_meta, m_arg->sge_cnt);
#else
	    send_publish_ack_to_libfs(m_arg->libfs_id, m_arg->fetch_seqn,
				      m_arg->digest_start_blknr,
				      m_arg->n_digested, m_arg->n_digested_blks,
				      m_arg->reset_meta);
#endif
    }

    // Send response to NIC kernfs.
    // rpc_send_memcpy_response(m_arg->sender_sockfd, m_arg->seqn, m_arg->libfs_id);

    // Set ack bit of NIC kernfs.
    rpc_set_remote_bit(m_arg->sender_sockfd, m_arg->ack_bit_addr);

    // Free.
    mlfs_free(arg);
}

/**
 * Replication msg (repmsg) handler. A replication message is sent from the
 * previous kernfs in a replication chain.
 */
static void handle_replicate_msg(void *arg)
{
    struct rpcmsg_replicate *rep;
    struct replication_context *rctx;
    int peer_kernfs;
    int last_in_chain;
    addr_t local_start;
    int libfs_id;
    uint64_t n_log_blks;

    START_TIMER(evt_rep_chain);

    rep = (struct rpcmsg_replicate *)arg;

    // mlfs_printf(ANSI_COLOR_GREEN "[REPMSG] libfs=%d "
    //                              "seqn=%lu\n" ANSI_COLOR_RESET,
    //             rep->libfs_id, rep->common.seqn);

#ifdef PROFILE_THPOOL
    struct rep_th_stat *stat = &rep_th_stats[rep->rep_thread_id];
    double duration;
    // Record scheduled time.
    clock_gettime(CLOCK_MONOTONIC, &rep->time_scheduled);
    duration = get_duration(&rep->time_added, &rep->time_scheduled);
    stat->schedule_delay_sum += duration;
    stat->schedule_cnt++;
    if (duration > stat->schedule_delay_max)
	    stat->schedule_delay_max = duration;
    if (duration < stat->schedule_delay_min)
	    stat->schedule_delay_min = duration;

    // Record seqn wait time.
    duration = get_duration(&rep->time_dequeued, &rep->time_scheduled);
    stat->wait_seqn_delay_sum += duration;
    if (duration > stat->wait_seqn_delay_max)
	    stat->wait_seqn_delay_max = duration;
    if (duration < stat->wait_seqn_delay_min)
	    stat->wait_seqn_delay_min = duration;
#endif

#ifdef NIC_OFFLOAD
    uint64_t len, cur;
    uint32_t ret;
    len = rep->n_log_blks << g_block_size_shift;
#endif

    libfs_id = rep->libfs_id;
    n_log_blks = rep->n_log_blks;
    rctx = g_sync_ctx[libfs_id];

    //FIXME: this currently updates nr of loghdrs incorrectly (assumes it's just 1)
    // nr_unsync is always 1 currently.
    pthread_mutex_lock(rctx->peer->n_unsync_blk_handle_lock);
    update_peer_sync_state(rctx->peer, rep->n_log_blks, 1);

#ifdef NIC_OFFLOAD
    // n_log_hdrs should be set in NIC-offloading.
    mlfs_assert(rep->n_log_hdrs);
    atomic_store(&rctx->peer->n_unsync, rep->n_log_hdrs);

    // Update end blknr. peer->remote_end will be updated to this value at
    // make_replication_request_sync().
    atomic_store(rctx->end, rep->end_blknr);
#endif

    // replicate to the next node in the chain
    // As a latency-hiding optimization, we perform this before persisting
    peer_kernfs = local_kernfs_id(libfs_id);
    last_in_chain = cur_kernfs_is_last(peer_kernfs, g_self_id);
    local_start = rctx->peer->local_start;

    // Store end_blknr before starting replication.
    // TODO do it here? or in mlfs_do_rsync_forward()?
    // do on all replicas vs only relaying replica.
    atomic_store(rctx->end, rep->end_blknr);

    if(!last_in_chain) {
        // forward to next kernfs in chain
        mlfs_do_rsync_forward(rep);
    } else {
	pthread_mutex_unlock(g_sync_ctx[libfs_id]->peer->n_unsync_blk_handle_lock);
    }


#ifndef NIC_OFFLOAD
    // Persist replicated data to NVM.
    if (mlfs_conf.persist_nvm_with_clflush && rep->persist)
        persist_log(local_start, n_log_blks);
#endif

    // Update replication metadata.
    addr_t start = rctx->peer->local_start;
    addr_t end = rctx->size;
    addr_t begin = rctx->begin;

    if (last_in_chain) {
        // Update local_start and avail_version.
#ifdef NIC_OFFLOAD
	// Some metadata need to be updated.
	// TODO It is required because we separately send REPREQ in the first NIC kernfs
	//      when there is a wrap around.
	if (rep->inc_avail_ver) {
	    rctx->peer->local_start = begin + n_log_blks;
	    rctx->peer->avail_version++;
	}
#else
        if (start + n_log_blks > end) {      // wrap around
            // LibFS sends RPC requests as 2 separated messages
            // if wrap around occurs:
            //
            // MSG 1: from local_start to the last POSSIBLE blk in log.
            //   N.B. The last POSSIBLE blk doesn't always match the last blk of
            //   log. (If # of blocks of a logheader exceed log size, wrap around
            //   occurs and the first block of log is used as the logheader.
            //   (Refer to g_log_sb[0]->end at log.c:log_alloc())
            //
            // MSG 2: from log start to remaining blocks.
            //
            // We don't need to think of the number of the last POSSIBLE blk
            // by just starting from the first block of log.
            rctx->peer->local_start = begin + n_log_blks;
            rctx->peer->avail_version++;

        }
#endif
	else {
            rctx->peer->local_start += n_log_blks;
        }

        pr_rep("Meta updated: libfs_id=%d local_start=%lu avail_version=%d",
                libfs_id, rctx->peer->local_start,
                rctx->peer->avail_version);
	PR_METADATA(rctx);

	// Respond to peer
	if (rep->ack) {	    // Or ack_bit_p != 0
	    pr_rep("last_in_chain -> send replication response to peer %d on "
		    "sockfd %u\n",
		    libfs_id, g_peers[libfs_id]->sockfd[SOCK_IO]);

	    END_TIMER(evt_rep_critical_host); // critical path in replica 3.

	    START_TIMER(evt_set_remote_bit);

#ifdef NO_BUSY_WAIT
	    // send rpc to libfs.
	    rpc_send_replicate_ack(g_peers[libfs_id]->sockfd[SOCK_IO], rep->ack_bit_p);
#else
	    rpc_set_remote_bit(g_peers[libfs_id]->sockfd[SOCK_IO], rep->ack_bit_p);

	    END_TIMER(evt_set_remote_bit);
#endif
	}
    }

    START_TIMER(evt_free_arg);
    if (!mlfs_conf.m_to_n_rep_thread) {
	    mlfs_free(arg);
    }
    END_TIMER(evt_free_arg);
    END_TIMER(evt_rep_chain);
}

static void handle_bootstrap(void *arg)
{
    int sockfd, i = 0, n = 0;
    uint64_t seqn;
    uint64_t *ack_bit = 0;
    struct peer_id *libfs_peer;
    struct bootstrap_arg *bs_arg = (struct bootstrap_arg*)arg;

    sockfd = bs_arg->sockfd;
    seqn = bs_arg->seqn;

    // libfs bootstrap calls are forwarded to all other KernFS instances
    libfs_peer = g_rpc_socks[sockfd]->peer;
    register_peer_log(libfs_peer, 1);
    for(i = g_kernfs_id; i < g_n_nodes+g_kernfs_id; i++) {
	n = i % g_n_nodes;

	if (n == g_kernfs_id)
	    continue;

#ifdef NIC_OFFLOAD
	// Do not send to Host kernfs.
	if (!is_kernfs_on_nic(n))
	    continue;
#endif
	ack_bit = rpc_alloc_ack_bit();
        rpc_register_log(g_kernfs_peers[n]->sockfd[SOCK_BG],
                         libfs_peer, ack_bit);
        rpc_wait_ack(ack_bit);
    }

    rpc_bootstrap_response(sockfd, seqn);
    mlfs_free(arg);
}

static void handle_peer_register(void *arg)
{
    struct peer_register_arg *pr_arg = (struct peer_register_arg*)arg;
    struct peer_id *peer;

    peer = _find_peer(pr_arg->ip, pr_arg->pid);
    mlfs_assert(peer);
    peer->id = pr_arg->id;

    register_peer_log(peer, 0);

    // Send ack.
    rpc_set_remote_bit(pr_arg->sockfd, pr_arg->ack_bit_p);
    mlfs_free(arg);
}

// Not used.
//static void handle_remote_libfs_peer_register(void *arg)
//{
//    struct peer_register_arg *pr_arg = (struct peer_register_arg*)arg;
//    struct peer_id *peer;
//
//    peer = _find_peer(pr_arg->ip, pr_arg->pid);
//    mlfs_assert(peer);
//    peer->id = pr_arg->id;
//
//    register_peer(peer, 0);
//
//    // Send ack.
//    rpc_set_remote_bit(pr_arg->sockfd, pr_arg->ack_bit_p);
//    mlfs_free(arg);
//
//}

void print_all_thpool_profile_results(void) {
#ifdef PROFILE_THPOOL
	// Prints all thpool profile results.
	printf("======= Thread scheduling delay profile ======\n");
	print_profile_result(thread_pool_digest);
	print_profile_result(thread_pool_ssd);
#ifdef PER_LIBFS_REPLICATION_THREAD
	for (int i = 0; i < mlfs_conf.thread_num_rep; i++) {
		print_profile_result(replicate_thpools[i]);
	}
#else
	print_profile_result(thread_pool_replicate);
#endif
	print_profile_result(thread_pool_host_memcpy);
#ifdef PER_LIBFS_PREFETCH_THREAD
	for (int i = 0; i < mlfs_conf.thread_num_log_prefetch; i++) {
		print_profile_result(log_prefetch_thpools[i]);
	}
#else
	print_profile_result(thread_pool_log_prefetch);
#endif
	print_profile_result(thread_pool_nvm_write);
	print_profile_result(thread_pool_misc);
#endif
}

//////////////////////////////////////////////////////////////////////////
// KernFS signal callback (used to handle signaling between replicas)

#ifdef DISTRIBUTED
void signal_callback(struct app_context *msg)
{
	// START_TIMER(evt_k_signal_callback);
	char cmd_hdr[12];
	// handles 4 message types (bootstrap, log, digest, lease)
	if(msg->data) {
		sscanf(msg->data, "|%s |", cmd_hdr);
		pr_rpc("RECV: %s sockfd=%d seqn=%lu", msg->data, msg->sockfd, msg->id);
		mlfs_info("received rpc with body: %s on sockfd %d\n", msg->data, msg->sockfd);
	}
	else {
                cmd_hdr[0] = 'i';
                mlfs_info("received imm with id %lu on sockfd %d\n", msg->id, msg->sockfd);
	}

	if (cmd_hdr[0] == 'm') {
	    if (cmd_hdr[1] == 'b')	// mr : RPC_REQ_MEMCPY_BUF
		goto req_memcpy_buf;
	    else if (cmd_hdr[1] == 'r')	// mr : RPC_MEMCPY_REQ
		goto memcpy_req;
//	    else if (cmd_hdr[1] == 'c')	    // mc : RPC_MEMCPY_COMPLETE
//		goto memcpy_complete;

	} else if (cmd_hdr[0] == 'p') {
	    if (cmd_hdr[1] == 'l')	// pl : RPC_PERSIST_LOG
		goto persist_log;

	} else if (cmd_hdr[0] == 't') {
	    if (cmd_hdr[1] == 'p')	// tp
		goto timer_print;
	    else if(cmd_hdr[1] == 'r')	// tr : RPC_RESET_BREAKDOWN_TIMER
		goto timer_reset;

	}

	// digest request
	if (cmd_hdr[0] == 'd') {
		struct digest_arg *digest_arg = (struct digest_arg *)mlfs_zalloc(sizeof(struct digest_arg));
		digest_arg->sock_fd = msg->sockfd;
		digest_arg->seqn = msg->id;
		memmove(digest_arg->msg, msg->data, MAX_SOCK_BUF);

		if (mlfs_conf.digest_noop)
		    thpool_add_work(thread_pool_digest, handle_digest_request_noop, (void*) digest_arg);
		else
		    thpool_add_work(thread_pool_digest, handle_digest_request, (void*) digest_arg);

#ifdef MIGRATION
		//try_migrate_blocks(g_root_dev, g_ssd_dev, 0, 0, 1);
#endif
	}
	//digestion completion notification
	else if (cmd_hdr[0] == 'c') {
		//FIXME: this callback currently does not support coalescing; it assumes remote
		// and local kernfs digests the same amount of data
		int log_id, dev, rotated, lru_updated;
		uint32_t n_digested;
		addr_t start_digest;

		sscanf(msg->data, "|%s |%d|%d|%d|%lu|%d|%d|", cmd_hdr, &log_id, &dev,
				&n_digested, &start_digest, &rotated, &lru_updated);
		update_peer_digest_state(g_sync_ctx[log_id]->peer, start_digest, n_digested, rotated);
		//handle_digest_response(msg->data);
	}
#if 0
	//read command
	else if(cmd_hdr[0] == 'r') {
		//trigger mlfs read and send back response
		//TODO: check if block is up-to-date (one way to do this is by propogating metadata updates
		//to slave as soon as possible and checking if blocks are up-to-date)
		char path[MAX_REMOTE_PATH];
		uintptr_t dst;
		loff_t offset;
		uint32_t io_size;
		//uint8_t * buf;
		int fd, ret;
		int flags = 0;

		sscanf(msg->data, "|%s |%s |%ld|%u|%lu", cmd_hdr, path, &offset, &io_size, &dst);
		mlfs_debug("received remote read RPC with path: %s | offset: %ld | io_size: %u | dst: %lu\n",
				path, offset, io_size, dst);
 		//buf = (uint8_t*) mlfs_zalloc(io_size << g_block_size_shift);

		struct mlfs_reply *reply = rpc_get_reply_object(msg->sockfd, (uint8_t*)dst, msg->id);

		fd = mlfs_posix_open(path, flags, 0); //FIXME: mode is currently unused - setting to 0
		if(fd < 0)
			panic("remote_read: failed to open file\n");

		ret = mlfs_rpc_pread64(fd, reply, io_size, offset);
		if(ret < 0)
			panic("remote_read: failed to read file\n");
	}
#endif
	else if(cmd_hdr[0] == 'i') { //immediate completions are replication notifications
	    mlfs_assert(0); // Do not use imm.
//                struct rpcmsg_replicate rep;
//                rep.is_imm = 1;
//		decode_rsync_metadata(msg->id, &rep.common.seqn, &rep.n_log_blks,
//                        &rep.steps, &rep.libfs_id, &rep.ack, &rep.persist);
//                pr_rpc("Received replicate imm from %d:", rep.libfs_id);
//                print_rpcmsg_replicate(&rep);
//                handle_replicate_msg(&rep);

        } else if (cmd_hdr[0] == 'r') {
	    if (cmd_hdr[3] == 'm') {	// repmsg: replicate message.
		START_TIMER(evt_rep_critical_host);

                struct rpcmsg_replicate *rep;

                rep = (struct rpcmsg_replicate *)mlfs_zalloc(
                    sizeof(struct rpcmsg_replicate));
                rpcmsg_parse_replicate(msg->data, rep);
                print_rpcmsg_replicate(rep, __func__);

                thpool_add_work(thread_pool_replicate, handle_replicate_msg,
                        (void *)rep);
            }
        }
#if 0
	//digestion completion notification
	else if (cmd_hdr[0] == 'c') {
		uint32_t n_digested, rotated;
		addr_t start_digest;

		printf("peer recv: %s\n", msg->data);
		sscanf(msg->data, "|%s |%lu|%d|%d", cmd_hdr, &start_digest, &n_digested, &rotated);
		update_peer_digest_state(get_next_peer(), start_digest, n_digested, rotated);
	}
#endif
	else if(cmd_hdr[0] == 'l') {
#if MLFS_LEASE
		//char path[MAX_PATH];
		int type;
		uint32_t req_id;
		uint32_t inum;
		uint32_t version;
		addr_t blknr;
		sscanf(msg->data, "|%s |%u|%u|%d|%u|%lu", cmd_hdr, &req_id, &inum, &type, &version, &blknr);
		//mlfs_debug("received remote lease acquire with inum %u | type[%d]\n", inum, type);

		int ret = modify_lease_state(req_id, inum, type, version, blknr);

		// If ret < 0 due to incorrect lease manager
		// (a) For read/write lease RPCs, return 'invalid lease request' to LibFS
		// [Only for LEASE_MIGRATION] (b) For lease revocations, simply forward to correct lease manager
		if(ret < 0) {
			rpc_lease_invalid(msg->sockfd, g_rpc_socks[msg->sockfd]->peer->id, inum, msg->id);
		}
		else {
			rpc_send_lease_ack(msg->sockfd, msg->id);
		}
#ifdef LESAE_MIGRATION
		if(ret < 0) {
			rpc_lease_change(abs(ret), req_id, inum, type, version, blknr, 0);
		}
#endif
#else
		panic("invalid code path\n");
#endif
	}
	else if(cmd_hdr[0] == 'm') { // migrate lease (enforce)
#if MLFS_LEASE
		int digest;
		uint32_t inum;
		uint32_t new_kernfs_id;
		sscanf(msg->data, "|%s |%u|%d", cmd_hdr, &inum, &new_kernfs_id);
		update_lease_manager(inum, new_kernfs_id);
		//rpc_send_ack(msg->sockfd, msg->id);
#else
		panic("invalid code path\n");
#endif
	}
	else if (cmd_hdr[0] == 'b') {
		uint32_t pid;
		struct bootstrap_arg *bs_arg;

		sscanf(msg->data, "|%s |%u", cmd_hdr, &pid);

                bs_arg = (struct bootstrap_arg *)mlfs_alloc(
                    sizeof(struct bootstrap_arg));
                bs_arg->sockfd = msg->sockfd;
		bs_arg->seqn = msg->id;
		thpool_add_work(thread_pool_misc, handle_bootstrap, (void *)bs_arg);
	}
	else if(cmd_hdr[0] == 'p') { //log registration
		struct peer_register_arg *pr_arg;
                pr_arg = (struct peer_register_arg *)mlfs_alloc(
                    sizeof(struct peer_register_arg));

                sscanf(msg->data, "|%s |%d|%u|%lu|%s", cmd_hdr, &pr_arg->id,
                       &pr_arg->pid, &pr_arg->ack_bit_p, pr_arg->ip);
		pr_arg->sockfd = msg->sockfd;

		thpool_add_work(thread_pool_misc, handle_peer_register, (void *)pr_arg);
	}
	else if(cmd_hdr[0] == 'a') {//ack (ignore)
		goto ret;
	}

#ifdef NIC_OFFLOAD
        else if (cmd_hdr[0] == 'n' && cmd_hdr[1] == 'r') {  // nic_rpc request/response
            /* Request list:
             *      nrrg
             *      nrrgu
             *      nrwol
             *      nrwolu
             *      nrsw
             *      nrswu
             *      nrcommit
             *      nrerase
             */
            unsigned int dev;
            addr_t blockno;
            uint32_t offset, io_size;
            unsigned long buf_addr_ul = 0;
            uint8_t *data_buf=0, *buf_addr=0;
            int buffer_id, ret, len;
            // struct app_context *resp_msg;
            struct buffer_head *bh_data;
            char prefix[1000];  // enough to include prefix of data.
            struct rdma_meta_entry *resp;
            uintptr_t src_addr, dst_addr;   // Used for RDMA write.

            if (strcmp(cmd_hdr, "nrrg") == 0) {  // Read and get
		mlfs_assert(0); // Not used any more.
//                sscanf(msg->data, "|%s |%u|%lu|%u|%lu", cmd_hdr, &dev, &blockno, &io_size, &dst_addr);
//
//                mlfs_assert(io_size <= g_block_size_bytes);
//
//                // Setup RDMA response. nrrg is implemented with 1 write with imm RDMA.
//                src_addr = (uintptr_t)g_bdev[dev]->map_base_addr + (blockno << g_block_size_shift);
//                resp = create_rdma_entry(src_addr, dst_addr, io_size, MR_DRAM_BUFFER, -1);
//                // Set imm to encoded nic rpc request. Prepending
//                rpc_remote_read_response(msg->sockfd, resp->meta, resp->local_mr, msg->id); // msg->id is already encoded.
//                mlfs_debug("[NIC_RPC] nrrg response sent with RDMA. src_addr "
//                           "0x%p *src_addr 0x%016x dst_addr 0x%p iosize %d\n",
//                           src_addr, *(uint64_t *)src_addr, dst_addr, io_size);
//                list_del(&resp->head);
//                mlfs_free(resp->meta);
//                mlfs_free(resp);


            } else if (strcmp(cmd_hdr, "nrrgu") == 0) {     // Read and get unaligned
		mlfs_assert(0); // Not used any more.
//                sscanf(msg->data, "|%s |%u|%lu|%u|%u|%lu", cmd_hdr, &dev, &blockno, &offset, &io_size, &dst_addr);
//
//                mlfs_assert(io_size <= g_block_size_bytes);
//
//                // Setup RDMA response. nrrgu is implemented with 1 write with imm RDMA.
//                src_addr = (uintptr_t)g_bdev[dev]->map_base_addr + (blockno << g_block_size_shift) + offset;
//                resp = create_rdma_entry(src_addr, dst_addr, io_size, MR_DRAM_BUFFER, -1);
//                // Set imm to encoded nic rpc request. Prepending
//                rpc_remote_read_response(msg->sockfd, resp->meta, resp->local_mr, msg->id); // msg->id is already encoded.
//                mlfs_debug("[NIC_RPC] nrrgu response sent with RDMA. src_addr "
//                           "0x%p *src_addr 0x%016x dst_addr 0x%p iosize %d\n",
//                           src_addr, *(uint64_t *)src_addr, dst_addr, io_size);
//                list_del(&resp->head);
//                mlfs_free(resp->meta);
//                mlfs_free(resp);

            } else if (strcmp(cmd_hdr, "nrwol") == 0) {     // Write opt local
                sscanf(msg->data, "|%s |%u|%lu|%lu|%u", cmd_hdr, &dev, &buf_addr_ul, &blockno, &io_size);
                buf_addr = (uint8_t *)buf_addr_ul;

#ifdef NVM_WRITE_CONCURRENT
                struct nvm_write_arg *nw_arg;
                // Worker thread will free the arg.
                nw_arg = (struct nvm_write_arg *)mlfs_alloc(sizeof(struct nvm_write_arg));
                nw_arg->msg_sockfd = msg->sockfd;
                nw_arg->msg_id = msg->id;
                nw_arg->dev = dev;
                nw_arg->buf_addr = buf_addr;
                nw_arg->blockno = blockno;
                nw_arg->io_size = io_size;
                nw_arg->offset = 0;

                thpool_add_work (thread_pool_nvm_write, nvm_write_worker, (void *)nw_arg);

                // setup response for ACK.
                buffer_id = rpc_nicrpc_setup_response(msg->sockfd, "|nrresp |", msg->id);
                // send response. Asynchronously. NVM memcpy is done by nvm_write_worker.
                rpc_nicrpc_send_response(msg->sockfd, buffer_id);

#else /* NVM_WRITE_CONCURRENT */
                // setup response for ACK.
                buffer_id = rpc_nicrpc_setup_response(msg->sockfd, "|nrresp |", msg->id);

                // write data.
                ret = g_bdev[dev]->storage_engine->write_opt(dev, buf_addr, blockno, io_size);
                if (ret != io_size)
                    panic ("Failed to write storage: Write size and io_size mismatch.\n");

                // send response.
                rpc_nicrpc_send_response(msg->sockfd, buffer_id);
#endif /* NVM_WRITE_CONCURRENT */

            } else if (strcmp(cmd_hdr, "nrwolu") == 0) {    // Write opt local unaligned
                sscanf(msg->data, "|%s |%u|%lu|%lu|%u|%u", cmd_hdr, &dev, &buf_addr_ul, &blockno, &offset, &io_size);
                buf_addr = (uint8_t *)buf_addr_ul;
#ifdef NVM_WRITE_CONCURRENT
                struct nvm_write_arg *nw_arg;
                // Worker thread will free the arg.
                nw_arg = (struct nvm_write_arg *)mlfs_alloc(
                        sizeof(struct nvm_write_arg));
                nw_arg->msg_sockfd = msg->sockfd;
                nw_arg->msg_id = msg->id;
                nw_arg->dev = dev;
                nw_arg->buf_addr = buf_addr;
                nw_arg->blockno = blockno;
                nw_arg->io_size = io_size;
                nw_arg->offset = offset;

                thpool_add_work (thread_pool_nvm_write, nvm_write_worker, (void *)nw_arg);

                // setup response for ACK.
                buffer_id = rpc_nicrpc_setup_response(msg->sockfd, "|nrresp |", msg->id);
                // send response.
                rpc_nicrpc_send_response(msg->sockfd, buffer_id);

#else /* NVM_WRITE_CONCURRENT */
                // setup response for ACK.
                buffer_id = rpc_nicrpc_setup_response(msg->sockfd, "|nrresp |", msg->id);

                // write data.
                ret = g_bdev[dev]->storage_engine->write_opt_unaligned(dev, buf_addr, blockno, offset, io_size);
                if (ret != io_size)
                    panic ("Failed to write storage: Write size and io_size mismatch.\n");

                // send response.
                rpc_nicrpc_send_response(msg->sockfd, buffer_id);
#endif /* NVM_WRITE_CONCURRENT */

            } else if (strcmp(cmd_hdr, "nrsw") == 0) {      // Send and write
		mlfs_assert(0); // Not used any more.

            } else if (strcmp(cmd_hdr, "nrswu") == 0) {     // Send and write unaligned
		mlfs_assert(0); // Not used any more.

            } else if (strcmp(cmd_hdr, "nrcommit") == 0) {
                sscanf(msg->data, "|%s |%u", cmd_hdr, &dev);

                // commit.
                mlfs_commit(dev);

                // send response.
                buffer_id = rpc_nicrpc_setup_response(msg->sockfd, "|nrresp |", msg->id);
                rpc_nicrpc_send_response(msg->sockfd, buffer_id);

            } else if (strcmp(cmd_hdr, "nrerase") == 0) {
            } else if (strcmp(cmd_hdr, "nrresp") == 0) {
                mlfs_info("%s", "[NIC RPC] nrresp Do nothing.\n");
            } else {
		panic("Unidentified nic rpc request\n");
            }
        }
#endif
	else {
		mlfs_printf("peer recv: %s\n", msg->data);
		panic("unidentified remote signal\n");
        }

	goto ret;

timer_reset:
	{
	    int libfs_id;
	    char cmd[50];
            sscanf(msg->data, "|%s |%d", cmd_hdr, &libfs_id);
	    sprintf(cmd, "|" TO_STR(RPC_RESET_BREAKDOWN_TIMER) " |%d|", libfs_id);
	    if (!is_last_kernfs(libfs_id, g_kernfs_id))
		rpc_forward_msg(g_sync_ctx[libfs_id]->next_digest_sockfd, cmd);

	    reset_breakdown_timers();
	    goto ret;
	}

timer_print:
	{
	    // relay msg to the next peer.
	    int libfs_id;
	    char cmd[50];
            sscanf(msg->data, "|%s |%d", cmd_hdr, &libfs_id);
	    sprintf(cmd, "|" TO_STR(RPC_PRINT_BREAKDOWN_TIMER) " |%d|", libfs_id);
	    if (!is_last_kernfs(libfs_id, g_kernfs_id))
		rpc_forward_msg(g_sync_ctx[libfs_id]->next_digest_sockfd, cmd);

	    print_breakdown_timers();
	    reset_breakdown_timers(); // Reset after printing times.

	    goto ret;
	}

req_memcpy_buf:
	{
		// Send address of memcpy list buffer.
		rpc_send_host_memcpy_buffer_addr(msg->sockfd,
						 (uintptr_t)host_memcpy_buf_p[0]);
		goto ret;
	}

memcpy_req : // sender:nic_kernfs, receiver:host_kernfs
	{
		struct mc_arg *m_arg = mlfs_zalloc(sizeof(struct mc_arg));
#ifdef BATCH_MEMCPY_LIST
		sscanf(msg->data, "|%s |%d|%lu|%lu|%lu|%u|%lu|%d|%lu|%d",
		       cmd_hdr, &m_arg->libfs_id, &m_arg->fetch_seqn,
		       &m_arg->n_memcpy_entries, &m_arg->digest_start_blknr,
		       &m_arg->n_digested, &m_arg->n_digested_blks,
		       &m_arg->reset_meta, &m_arg->ack_bit_addr,
		       &m_arg->sge_cnt);
#else
		sscanf(msg->data, "|%s |%d|%lu|%lu|%lu|%u|%lu|%d|%lu",
		       cmd_hdr, &m_arg->libfs_id, &m_arg->fetch_seqn,
		       &m_arg->n_memcpy_entries, &m_arg->digest_start_blknr,
		       &m_arg->n_digested, &m_arg->n_digested_blks,
		       &m_arg->reset_meta, &m_arg->ack_bit_addr);
#endif
		m_arg->seqn = msg->id;
		m_arg->sender_sockfd = msg->sockfd;
		thpool_add_work(thread_pool_host_memcpy, memcpy_worker,
				(void *)m_arg);
		goto ret;
	}

//memcpy_complete: // sender:host_kernfs, receiver:nic_kernfs
//	{
//	    // Do nothing.
//	    goto ret;
//	}

persist_log : // sender: replica 1, receiver: host_kernfs of replica 2
	{
		struct persist_log_arg *pl_arg =
			mlfs_zalloc(sizeof(struct persist_log_arg));
		sscanf(msg->data, "|%s |%d|%lu|%lu|%lu|%lu|%lu|%lu", cmd_hdr,
		       &pl_arg->libfs_id, &pl_arg->seqn,
		       &pl_arg->log_area_begin_blknr, &pl_arg->log_area_end_blknr,
		       &pl_arg->start_blknr, &pl_arg->n_log_blks,
		       &pl_arg->fsync_ack_addr);
		thpool_add_work(thread_pool_persist_log, persist_log_worker,
				(void *)pl_arg);
		goto ret;
	}

ret:
	{
	    // END_TIMER(evt_k_signal_callback);
	    return;
	}
}
#endif

/**
 * Persist replicated log into NVM.
 * TODO It persists all the replicated log as one single large region. If log
 * data has some holes, persisting each sub-regions separately might get better
 * performance. (By not flushing blocks that doesn't need to be flushed.)
 */
void persist_log(addr_t start_blk, addr_t n_log_blk)
{
    mlfs_persist(g_log_dev, start_blk, 0, n_log_blk << g_block_size_shift);
}

void persist_replicated_logs(int id, addr_t n_log_blk)
{
#ifdef DISTRIBUTED
	//addr_t size = nr_blks_between_ptrs(g_log_sb->start_persist, end_blk+1);
	int flags = 0;

	flags |= (PMEM_PERSIST_FLUSH | PMEM_PERSIST_DRAIN);

	uint32_t size = 0;

	//wrap around
	//FIXME: pass log start_blk
 	if(g_peers[id]->log_sb->start_persist + n_log_blk > g_log_size)
		g_peers[id]->log_sb->start_persist = g_sync_ctx[id]->begin;

	//move_log_ptr(&g_log_sb->start_persist, 0, n_log_blk);

	//mlfs_commit(id, g_peers[id]->log_sb->start_persist, 0, (n_log_blk << g_block_size_shift), flags);

	g_peers[id]->log_sb->start_persist += n_log_blk;



	//TODO: apply to individual logs to avoid flushing any unused memory
	/*
	addr_t last_blk =0;
	addr_t loghdr_blk = 0;
	struct buffer_head *io_bh;
	loghdr_t *loghdr = read_log_header(g_fs_log->dev, start_blk);

	while(end != last_blk) {
		io_bh = bh_get_sync_IO(g_log_dev, loghdr_blk, BH_NO_DATA_ALLOC);

		io_bh->b_data = (uint8_t *)loghdr;
		io_bh->b_size = sizeof(struct logheader);

		mlfs_commit(io_bh);

		loghdr_blk = next_loghdr_blknr(loghdr->hdr_blkno,loghdr->nr_log_blocks,
				g_fs_log->log_sb_blk+1, atomic_load(&g_log_sb->end));
		loghdr = read_log_header(g_fs_log->dev, loghdr_blk);
	}
	*/
#endif
}

static void persist_log_worker(void *arg)
{
	struct persist_log_arg *pl_arg = (struct persist_log_arg*) arg;
	addr_t log_area_begin_blknr, log_area_end_blknr;
	addr_t log_end_blknr, digest_end_blknr, flush_size;

	mlfs_assert(mlfs_conf.persist_nvm_with_clflush);

	// calculate range to be flushed.
	// 1) DIGEST END blknr < REPLICATE LOG START blknr
	//	flush from DIGEST END + 1 to REPLICATE LOG END(start + size) blknr.
	//
	// 2) REPLICATE LOG START < DIGEST END blknr blknr
	//	flush from DIGEST END blknr + 1 to LOG AREA END and
	//	flush from LOG AREA BEGIN to REPLICATE LOG END (start + size) blknr.
	//
	// Note that we don't need to flush digested blocks.

	digest_end_blknr = digest_end_blknrs[pl_arg->libfs_id];
	log_end_blknr = pl_arg->start_blknr + pl_arg->n_log_blks - 1;

	if (digest_end_blknr < pl_arg->start_blknr) {
		flush_size = log_end_blknr - digest_end_blknr;
		persist_log(digest_end_blknr + 1, flush_size);

	} else if (digest_end_blknr > pl_arg->start_blknr) {
		log_area_begin_blknr = pl_arg->log_area_begin_blknr;
		log_area_end_blknr = pl_arg->log_area_end_blknr;

		flush_size =
			log_end_blknr - log_area_begin_blknr + 1;
		persist_log(log_area_begin_blknr, flush_size);

		flush_size = log_area_end_blknr - digest_end_blknr;
		persist_log(digest_end_blknr + 1, flush_size);
	} else {
		// Should not be taken.
		mlfs_printf("libfs_id=%d seqn=%lu log_area_begin_blknr=%lu "
			    "log_area_end_blknr=%lu start_blknr=%lu "
			    "n_log_blks=%lu fsync_ack_addr=0x%lu\n",
			    pl_arg->libfs_id, pl_arg->seqn,
			    pl_arg->log_area_begin_blknr,
			    pl_arg->log_area_end_blknr, pl_arg->start_blknr,
			    pl_arg->n_log_blks, pl_arg->fsync_ack_addr);
		mlfs_printf("digest_end_blknr=%lu start_blknr=%lu\n",
		       digest_end_blknr, pl_arg->start_blknr);
		mlfs_assert(0);
	}

	// Send ack to libfs from replica 2.
	uint8_t val = 1;
	// Refer to struct fsync_acks.
	uintptr_t dst_addr = pl_arg->fsync_ack_addr + sizeof(uint8_t);
	// mlfs_printf("fsync ack val:%hhu RDMA write to libfs_id=%d addr=0x%lx seqn=%lu\n",
	//             val, pl_arg->libfs_id,
	//             pl_arg->fsync_ack_addr + sizeof(uint8_t), pl_arg->seqn);
	rpc_write_remote_val8(g_peers[pl_arg->libfs_id]->sockfd[SOCK_IO],
			      dst_addr, val);
}

void cache_init(uint8_t dev)
{
	int i;
	inode_hash = NULL;

	pthread_spin_init(&icache_spinlock, PTHREAD_PROCESS_SHARED);
	pthread_spin_init(&dcache_spinlock, PTHREAD_PROCESS_SHARED);

#if MLFS_LEASE
#ifndef LEASE_OPT
	lease_table = SharedTable_mock();
#else
    	lease_table = SharedTable_create("/lease", 1048576000);
#endif

#endif
}

void read_superblock(uint8_t dev)
{
	int ret;
	struct buffer_head *bh;

	bh = bh_get_sync_IO(dev, 1, BH_NO_DATA_ALLOC);
	bh->b_size = g_block_size_bytes;
	bh->b_data = mlfs_zalloc(g_block_size_bytes);

	bh_submit_read_sync_IO(bh);
	mlfs_io_wait(dev, 1);

	if (!bh)
		panic("cannot read superblock\n");

	mlfs_debug("size of superblock %ld\n", sizeof(struct disk_superblock));

	memmove(&disk_sb[dev], bh->b_data, sizeof(struct disk_superblock));

	mlfs_info("superblock: size %lu nblocks %lu ninodes %u\n"
			"[inode start %lu bmap start %lu datablock start %lu log start %lu]\n",
			disk_sb[dev].size,
			disk_sb[dev].ndatablocks,
			disk_sb[dev].ninodes,
			disk_sb[dev].inode_start,
			disk_sb[dev].bmap_start,
			disk_sb[dev].datablock_start,
			disk_sb[dev].log_start);

	sb[dev]->ondisk = &disk_sb[dev];

	// set all rb tree roots to NULL
	for(int i=0; i<(MAX_LIBFS_PROCESSES+g_n_nodes); i++)
		sb[dev]->s_dirty_root[i] = RB_ROOT;

	sb[dev]->last_block_allocated = 0;

	// The partition is GC unit (1 GB) in SSD.
	// disk_sb[dev].size : total # of blocks
#if 0
	sb[dev]->n_partition = disk_sb[dev].size >> 18;
	if (disk_sb[dev].size % (1 << 18))
		sb[dev]->n_partition++;
#endif

	sb[dev]->n_partition = 1;

	//single partitioned allocation, used for debugging.
	mlfs_info("dev %u: # of segment %u\n", dev, sb[dev]->n_partition);
	sb[dev]->num_blocks = disk_sb[dev].ndatablocks;
	sb[dev]->reserved_blocks = disk_sb[dev].datablock_start;
	sb[dev]->s_bdev = g_bdev[dev];

	mlfs_free(bh->b_data);
	bh_release(bh);
}
#endif /* __x86_64__ */
