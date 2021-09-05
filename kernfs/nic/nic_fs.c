// #ifdef __aarch64__ // Comment out for development.
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#include "mlfs/mlfs_user.h"
#include "global/global.h"
#include "global/util.h"
#include "global/defs.h"
#include "io/balloc.h"
#include "kernfs_interface.h"
#include "nic_fs.h"
#include "io/block_io.h"
#include "storage/storage.h"
#include "extents.h"
#include "extents_bh.h"
#include "filesystem/slru.h"
#include "migrate.h"
#include "nic/loghdr.h"
#include "nic/copy_log.h"
#include "nic/host_memcpy.h"	// print pipeline profile
#include "nic/coalesce.h"	// print pipeline profile
#include "nic/build_memcpy_list.h" // print pipeline profile
#include "nic/compress.h" // print pipeline profile
#include "nic/limit_rate.h"

#ifdef DISTRIBUTED
#include "distributed/rpc_interface.h"
#include "distributed/replication.h"
#endif

#define NOCREATE 0
#define CREATE 1

int log_fd = 0;
int shm_fd = 0;
uint8_t *shm_base;

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
threadpool thread_pool_ssd;

// NIC offloading.
threadpool thread_pool_log_prefetch; // primary
threadpool thread_pool_log_fetch; // replicas
threadpool thread_pool_fsync;	// replicas
threadpool thread_pool_loghdr_fetch; // replicas
threadpool thread_pool_copy_done_handle; // last replica
threadpool thread_pool_rate_limiter;
threadpool thread_pool_handle_heartbeat;
threadpool thread_pool_heartbeat_checker;

// thread pool for nvm write on host (digest).
threadpool thread_pool_nvm_write;

// thread pool to handle trivial jobs.
threadpool thread_pool_misc;

#if MLFS_LEASE
struct sockaddr_un all_cli_addr[g_n_hot_rep];
int addr_idx = 0;
#endif

DECLARE_BITMAP(g_log_bitmap, MAX_LIBFS_PROCESSES);

// breakdown timers.
static void reset_breakdown_timers(void)
{
	// Noop
}

static void print_breakdown_timers(void)
{
	// Noop
}


// Print timers of Fsync.
void print_all_thpool_fsync_stats(
	int th_num, threadpool th_pool, void (*function_p)(void *))
{
	for (uint64_t i = 0; i < th_num; i++) {
		thpool_add_work(th_pool,
				function_p, (void *)i);
	}
}

void print_all_fsync_stat(void *arg) {
#ifdef PROFILE_PIPELINE
	print_log_fetch_from_local_nvm_stat((void*)99);
	print_log_fetch_from_primary_nic_dram_stat((void*)99);
	print_coalesce_stat((void*)99);
	print_compress_stat((void*)99);
	print_copy_to_last_replica_stat((void*)99);
	print_free_log_buf_stat((void*)99);
	print_handle_copy_done_ack_stat((void*)99);
	print_thpool_stat((void*)99);
#endif
}

static void print_all_pipeline_stage_stats(void)
{
#ifdef PROFILE_PIPELINE
	printf("==========_Pipeline_Stage_Stat === === ===\n");
	PRINT_TL_HDR();
	PRINT_ALL_PIPELINE_STATS(log_fetch_from_local_nvm,
				 mlfs_conf.thread_num_log_prefetch);
	PRINT_ALL_PIPELINE_STATS(log_fetch_from_primary_nic_dram,
				 mlfs_conf.thread_num_log_fetch);
	PRINT_ALL_PIPELINE_STATS(coalesce, mlfs_conf.thread_num_coalesce);
	PRINT_ALL_PIPELINE_STATS(loghdr_build,
				 mlfs_conf.thread_num_loghdr_build);
	PRINT_ALL_PIPELINE_STATS(loghdr_fetch,
				 mlfs_conf.thread_num_loghdr_fetch);
	print_all_thpool_build_memcpy_list_stats();
	print_all_thpool_host_memcpy_req_stats();
	PRINT_ALL_PIPELINE_STATS(pipeline_end, 1);
	PRINT_ALL_PIPELINE_STATS(compress, mlfs_conf.thread_num_compress);
	PRINT_ALL_PIPELINE_STATS(copy_to_local_nvm,
				 mlfs_conf.thread_num_copy_log_to_local_nvm);
	PRINT_ALL_PIPELINE_STATS(copy_to_last_replica,
				 mlfs_conf.thread_num_copy_log_to_last_replica);
	print_all_thpool_manage_fsync_ack_stats();
	PRINT_ALL_PIPELINE_STATS(free_log_buf, 1);
	PRINT_ALL_PIPELINE_STATS(handle_copy_done_ack, 1);
	usleep(100000);

	printf("--_Fsync_stat_------ --- --- ---\n");
	print_all_thpool_fsync_stats(mlfs_conf.thread_num_log_prefetch,
				     thread_pool_log_prefetch,
				     print_all_fsync_stat);
	print_all_thpool_fsync_stats(mlfs_conf.thread_num_log_fetch,
				     thread_pool_fsync, print_all_fsync_stat);
	usleep(100000);
	printf("-------------------- --- --- ---\n");

	// Reset
#endif
}

void show_kernfs_stats(void)
{
	float clock_speed_mhz = get_cpu_clock_speed();
	uint64_t n_digest =
		g_perf_stats.n_digest == 0 ? 1.0 : g_perf_stats.n_digest;

	printf("\n");
	// printf("CPU clock : %.3f MHz\n", clock_speed_mhz);
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
	       ((float)g_perf_stats.n_digest_skipped * 100.0) /
		       (float)n_digest);
	printf("path search    : %.3f ms\n",
	       g_perf_stats.path_search_tsc / (clock_speed_mhz * 1000.0));
	printf("total migrated : %lu MB\n", g_perf_stats.total_migrated_mb);
#ifdef MLFS_LEASE
	printf("nr lease rpc (local)	: %lu\n",
	       g_perf_stats.lease_rpc_local_nr);
	printf("nr lease rpc (remote)	: %lu\n",
	       g_perf_stats.lease_rpc_remote_nr);
	printf("nr lease migration	: %lu\n",
	       g_perf_stats.lease_migration_nr);
	printf("nr lease contention	: %lu\n",
	       g_perf_stats.lease_contention_nr);
#endif
	printf("--------------------------------------\n");
}

void show_storage_stats(void)
{
	printf("------------------------------ storage stats\n");
	printf("NVM - %.2f %% used (%.2f / %.2f GB) - # of used blocks %lu\n",
	       (100.0 * (float)sb[g_root_dev]->used_blocks) /
		       (float)disk_sb[g_root_dev].ndatablocks,
	       (float)(sb[g_root_dev]->used_blocks << g_block_size_shift) /
		       (1 << 30),
	       (float)(disk_sb[g_root_dev].ndatablocks << g_block_size_shift) /
		       (1 << 30),
	       sb[g_root_dev]->used_blocks);

#ifdef USE_SSD
	printf("SSD - %.2f %% used (%.2f / %.2f GB) - # of used blocks %lu\n",
	       (100.0 * (float)sb[g_ssd_dev]->used_blocks) /
		       (float)disk_sb[g_ssd_dev].ndatablocks,
	       (float)(sb[g_ssd_dev]->used_blocks << g_block_size_shift) /
		       (1 << 30),
	       (float)(disk_sb[g_ssd_dev].ndatablocks << g_block_size_shift) /
		       (1 << 30),
	       sb[g_ssd_dev]->used_blocks);
#endif

#ifdef USE_HDD
	printf("HDD - %.2f %% used (%.2f / %.2f GB) - # of used blocks %lu\n",
	       (100.0 * (float)sb[g_hdd_dev]->used_blocks) /
		       (float)disk_sb[g_hdd_dev].ndatablocks,
	       (float)(sb[g_hdd_dev]->used_blocks << g_block_size_shift) /
		       (1 << 30),
	       (float)(disk_sb[g_hdd_dev].ndatablocks << g_block_size_shift) /
		       (1 << 30),
	       sb[g_hdd_dev]->used_blocks);
#endif

	mlfs_info("--- lru_list count %lu, %lu, %lu\n", g_lru[g_root_dev].n,
		  g_lru[g_ssd_dev].n, g_lru[g_hdd_dev].n);
}

/*void read_log_header_from_nic_dram(loghdr_meta_t *loghdr_meta, addr_t
hdr_blkno) { uint64_t nr_cur_log_blks;

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
#if defined(NIC_OFFLOAD) && defined(NIC_SIDE)
	// It shouldn't be called in NIC kernfs.
	mlfs_assert(0);
#endif

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
	mlfs_log("ts %ld.%06ld\n", _loghdr->mtime.tv_sec,
		 _loghdr->mtime.tv_usec);
	mlfs_log("blknr %lu\n", hdr_addr);
	mlfs_log("size %u\n", _loghdr->nr_log_blocks);
	mlfs_log("inuse %x\n", _loghdr->inuse);
	for (int i = 0; i < _loghdr->n; i++) {
		mlfs_log("type[%d]: %u\n", i, _loghdr->type[i]);
		mlfs_log("inum[%d]: %u\n", i, _loghdr->inode_no[i]);
	}

	/*
	for (i = 0; i < _loghdr->n; i++) {
		mlfs_debug("types %d blocks %lx\n",
				_loghdr->type[i], _loghdr->blocks[i] +
	loghdr_meta->hdr_blkno);
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

void shutdown_nic_fs(void)
{
#ifdef PROFILE_THPOOL
	print_all_thpool_profile_results();
#endif
#ifdef PIPELINE_RATE_LIMIT
	destroy_rate_limiter();
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
	pool_space = (uint8_t *)mmap(0, pool_size, PROT_READ | PROT_WRITE,
				     MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE,
				     -1, 0);

	mlfs_assert(pool_space);

	if (madvise(pool_space, pool_size, MADV_HUGEPAGE) < 0)
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
	log_fd = open(LOG_PATH, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
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
	pthread_mutex_init(&digest_mutex, &attr); // prevent concurrent digests
#endif
}

#ifdef NIC_OFFLOAD
void get_superblock(uint8_t dev, struct disk_superblock *sbp)
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
		  "[inode start %lu bmap start %lu datablock start %lu log "
		  "start %lu]\n",
		  dev, sbp->size, sbp->ndatablocks, sbp->ninodes,
		  sbp->inode_start, sbp->bmap_start, sbp->datablock_start,
		  sbp->log_start);
	mlfs_free(bh->b_data);
	bh_release(bh);
}

void handle_tcp_client_req(int cli_sock_fd)
{
	char buf[MAX_SOCK_BUF + 1] = { 0 };
	char cmd_hdr[TCP_HDR_SIZE] = { 0 };
	char resp_buf[TCP_BUF_SIZE + 1] = { 0 };
	int n, tcp_connected;
	int dev;

	tcp_connected = 1;
	while (tcp_connected) {
		if ((n = read(cli_sock_fd, buf, MAX_SOCK_BUF)) > 0) { // read
								      // cmd.
			sscanf(buf, "|%s |", cmd_hdr);
			buf[n] = '\0';
			mlfs_info("received tcp with body: %s\n", buf);

			switch (cmd_hdr[0]) {
			case 's': // sb : Request superblock.
				printf("Server: cmd_hdr is s\n");

				sscanf(buf, "|%s |%d|", cmd_hdr, &dev);

				// Read ondisk superblock.
				memset(resp_buf, 0, sizeof(resp_buf));
				get_superblock(
					dev,
					(struct disk_superblock *)resp_buf);
				mlfs_info("Server: superblock data: %s\n",
					  resp_buf);
				mlfs_assert(sizeof(struct disk_superblock) <=
					    TCP_BUF_SIZE);

				struct disk_superblock *sb;
				sb = (struct disk_superblock *)resp_buf;
				mlfs_info("Server send resp: superblock: size "
					  "%lu nblocks %lu ninodes %u\n"
					  "\t[inode start %lu bmap start %lu "
					  "datablock start %lu log start "
					  "%lu]\n",
					  sb->size, sb->ndatablocks,
					  sb->ninodes, sb->inode_start,
					  sb->bmap_start, sb->datablock_start,
					  sb->log_start);

				// Send response to client.
				tcp_send_client_resp(
					cli_sock_fd, resp_buf,
					sizeof(struct disk_superblock));
				break;

			case 'r': // rinode :  Request root inode.
				mlfs_info("%s", "Server: cmd_hdr is r\n");
				// Read ondisk root_inode.
				memset(resp_buf, 0, sizeof(resp_buf));
				read_ondisk_inode(ROOTINO,
						  (struct dinode *)resp_buf);

				mlfs_info("Server: ROOTINO:%d\n", ROOTINO);
				mlfs_info("Server: root_inode data size: %d\n",
					  sizeof(struct dinode));
				mlfs_assert(sizeof(struct dinode) <=
					    TCP_BUF_SIZE);

				struct dinode *di;
				di = (struct dinode *)resp_buf;
				/*
				mlfs_info("Server send resp: dinode: "
					"\t[ itype %d nlink %d size %lu atime
				%ld.%06ld ctime %ld.%06ld mtime %ld.%06ld]\n",
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
				tcp_send_client_resp(cli_sock_fd, resp_buf,
						     sizeof(struct dinode));
				break;
			case 'b': // bitmapb : Request bitmap block.
				mlfs_info("%s", "Server: cmd_hdr is b\n");
				int err;
				mlfs_fsblk_t blk_nr;

				sscanf(buf, "|%s |%d|%lu|", cmd_hdr, &dev,
				       &blk_nr);

				// Read ondisk block.
				memset(resp_buf, 0, sizeof(resp_buf));
				err = read_bitmap_block(dev, blk_nr,
							(uint8_t *)resp_buf);
				if (err) {
					panic("Server: Error: cannot read "
					      "bitmap block.\n");
				}

				tcp_send_client_resp(cli_sock_fd, resp_buf,
						     g_block_size_bytes);
				break;

			case 'c': // close : Close TCP connection.
				// tcp connection should be closed in the caller
				// of this function.
				tcp_connected = 0;
				break;

			case 'd': // devba : Request device base address.
				mlfs_info("%s", "Server: cmd_hdr is b\n");
				sscanf(buf, "|%s |%d", cmd_hdr, &dev);

				memset(resp_buf, 0, sizeof(resp_buf));
				memmove(resp_buf, &g_bdev[dev]->map_base_addr,
					sizeof(uint8_t *));
				mlfs_info("Server: dev base addr: %p\n",
					  g_bdev[dev]->map_base_addr);
				tcp_send_client_resp(cli_sock_fd, resp_buf,
						     sizeof(uint8_t *));
				break;

			default:
				printf("Error: Unknown cmd_hdr for tcp "
				       "connection.\n");
				break;
			}
		}
	}
}

void set_dram_superblock(uint8_t dev)
{
	sb[dev]->ondisk = &disk_sb[dev];

	// set all rb tree roots to NULL
	for (int i = 0; i < (MAX_LIBFS_PROCESSES + g_n_nodes); i++)
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

	// single partitioned allocation, used for debugging.
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
	shutdown_nic_fs();
}
#endif

// Propagates meta data following the chain.
static void send_pipeline_kernfs_meta(void)
{
	int next_kernfs = get_next_kernfs(g_kernfs_id);
	rpc_send_pipeline_kernfs_meta_to_next_replica(
		g_kernfs_peers[next_kernfs]->sockfd[SOCK_BG]);
}

void init_nic_fs(void)
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

#ifdef USE_SLAB
	mlfs_slab_init(3UL << 30);
#endif

	device_init();

	debug_init();

	init_device_lru_list();

	// shared_memory_init();

	cache_init(g_root_dev);

	locks_init();

	for (i = 0; i < g_n_devices + 1; i++)
		sb[i] = mlfs_zalloc(sizeof(struct super_block));

	// Setup node ids and g_self_id.
	set_self_ip();

	// Set g_node_id and g_host_node_id.
	// set_self_ip() should be called before calling it.
	set_self_node_id();

#ifdef NIC_OFFLOAD // TODO Currently, metadata is managed by host kernfs. It
		   // will be managed by NIC kernfs. JYKIM.
	// Setup TCP socket for getting metadata from host.
	char buf[TCP_BUF_SIZE + 1] = { 0 };
	int cli_sock_fd;

#ifdef NIC_SIDE
	// Setup client tcp socket.
	tcp_setup_client(&cli_sock_fd);

	// Request superblock. (root dev, log dev, log dev+1)
	struct disk_superblock *dsb;
	memset(buf, 0, sizeof(buf));
	tcp_client_req_sb(cli_sock_fd, buf, g_root_dev); // root dev
	dsb = (struct disk_superblock *)buf;
	memcpy(&disk_sb[g_root_dev], dsb, sizeof(struct disk_superblock));
	set_dram_superblock(g_root_dev);

	memset(buf, 0, sizeof(buf));
	tcp_client_req_sb(cli_sock_fd, buf, g_log_dev); // log dev
	dsb = (struct disk_superblock *)buf;
	memcpy(&disk_sb[g_log_dev], dsb, sizeof(struct disk_superblock));
	set_dram_superblock(g_log_dev);

	// Request root inode.
	memset(buf, 0, sizeof(buf));
	tcp_client_req_root_inode(cli_sock_fd, buf);
	struct dinode *di;
	di = (struct dinode *)buf;
	set_dram_root_inode(di);

	// Alloc bitmap and get ondisk bitmap from host.
	memset(buf, 0, sizeof(buf));
	balloc_init(g_root_dev, sb[g_root_dev], 0, cli_sock_fd);

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
	// Get base_map_addr for each dev.
	for (i = 1; i < g_n_devices + 1; i++) {
		if (i == g_root_dev) {
			memset(buf, 0, sizeof(buf));
			tcp_client_req_dev_base_addr(cli_sock_fd, buf, i);
			g_bdev[i]->map_base_addr = (uint8_t *)*(addr_t *)buf;
			mlfs_info("g_bdev[%d]->map_base_addr : 0x%p\n", i,
				  g_bdev[i]->map_base_addr);
		} else if (i == g_hdd_dev || i == g_ssd_dev) {
			continue; // hdd and ssd is not supported. TODO
		} else { // libfs logs starting from devid 4
			memset(buf, 0, sizeof(buf));
			tcp_client_req_dev_base_addr(cli_sock_fd, buf, i);
			g_bdev[i]->map_base_addr = (uint8_t *)*(addr_t *)buf;
			mlfs_info("g_bdev[%d]->map_base_addr : 0x%p\n", i,
				  g_bdev[i]->map_base_addr);
		}
	}

	// Request server to close.
	tcp_client_req_close(cli_sock_fd);

	// Close client tcp connection.
	tcp_close(cli_sock_fd);

#else /* Not NIC_SIDE == host side */
	int serv_sock_fd;

	read_superblock(g_root_dev); // FIXME need to be called for
				     // read_root_inode().
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

#endif /* NIC_SIDE */

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

	mlfs_assert(g_log_size * MAX_LIBFS_PROCESSES <=
		    disk_sb[g_log_dev].nlog);

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


#ifdef DISTRIBUTED
	bitmap_set(g_log_bitmap, 0, MAX_LIBFS_PROCESSES);

	mlfs_assert(disk_sb[g_log_dev].nlog >= g_log_size * g_n_hot_rep);
	// for(int i=0; i<g_n_hot_rep; i++)
	//	init_log(i);

	// set memory regions to be registered by rdma device
	int n_regions = 3;
	struct mr_context *mrs = (struct mr_context *)mlfs_zalloc(
		sizeof(struct mr_context) * n_regions);

	int ret;
	for (int i = 0; i < n_regions; i++) {
		switch (i) {
		case MR_NVM_LOG: {
#if defined(NIC_OFFLOAD) && defined(NIC_SIDE)
#define DRAM_MR_SIZE (256 * 1024 * 1024) // 256MB
			// FIXME: share all log entries; current impl shares
			// only a single log mr
			// Allocate dram memory space for mrs. (DRAM is used for
			// RDMA MR.)
			mrs[i].type = MR_NVM_LOG;
			mrs[i].addr = (addr_t)mlfs_zalloc(DRAM_MR_SIZE);
			mrs[i].length = DRAM_MR_SIZE;
#else
			// FIXME: share all log entries; current impl shares
			// only a single log mr
			mrs[i].type = MR_NVM_LOG;
			mrs[i].addr =
				(uint64_t)g_bdev[g_log_dev]->map_base_addr;
			mrs[i].length = ((disk_sb[g_log_dev].nlog)
					 << g_block_size_shift);
#endif
			break;
		}
		case MR_NVM_SHARED: {
#if defined(NIC_OFFLOAD) && defined(NIC_SIDE)
			// FIXME: share all log entries; current impl shares
			// only a single log mr
			// Allocate dram memory space for mrs. (DRAM is used for
			// RDMA MR.)
			mrs[i].type = MR_NVM_SHARED;
			mrs[i].addr = (addr_t)mlfs_zalloc(DRAM_MR_SIZE);
			mrs[i].length = DRAM_MR_SIZE;
#else
			mrs[i].type = MR_NVM_SHARED;
			mrs[i].addr =
				(uint64_t)g_bdev[g_root_dev]->map_base_addr;
			// mrs[i].length = dev_size[g_root_dev];
			// mrs[i].length = (1UL << 30);
			mrs[i].length = (sb[g_root_dev]->ondisk->size
					 << g_block_size_shift); // All possible
								 // address of
								 // device. Ref)
								 // mkfs.c
			// mrs[i].length = (disk_sb[g_root_dev].datablock_start
			// - disk_sb[g_root_dev].inode_start <<
			// g_block_size_shift);  // data blocks only.
#endif
			break;
		}
			/*
			case MR_DRAM_CACHE: {
				mrs[i].type = MR_DRAM_CACHE;
				mrs[i].addr = (uint64_t) g_fcache_base;
				mrs[i].length = (g_max_read_cache_blocks <<
			g_block_size_shift); break;
			}
			*/

		case MR_DRAM_BUFFER: {
			// Check nic_slab_pool does not exceed 13GB. Note that
			// the size of DRAM in SmartNIC is 16GB.
			mlfs_assert((g_max_nicrpc_buf_blocks
				     << g_block_size_shift) <=
				    (13ULL * 1024 * 1024 * 1024));

			nic_slab_init((g_max_nicrpc_buf_blocks
				       << g_block_size_shift));
			mrs[i].length =
				(g_max_nicrpc_buf_blocks << g_block_size_shift);

			mrs[i].type = MR_DRAM_BUFFER;
			mrs[i].addr = (uint64_t)nic_slab_pool->addr;

			pr_dram_alloc("[DRAM_ALLOC] MR_DRAM_BUFFER "
				      "size=%lu(MB)",
				      mrs[i].length / 1024 / 1024);
			break;
		}
		default:
			break;
		}
		printf("mrs[%d] type %d, addr 0x%lx - 0x%lx, length %lu(%lu "
		       "MB)\n",
		       i, mrs[i].type, mrs[i].addr, mrs[i].addr + mrs[i].length,
		       mrs[i].length, mrs[i].length / 1024 / 1024);
	}

#ifdef NIC_SIDE
	// NIC waits server being ready to receive RDMA CM request.
	sleep(2);
#endif

	// A fixed thread for using SPDK.
	thread_pool_ssd = thpool_init(1, "ssd");
	print_thread_init(1, "ssd");
	thread_pool_misc = thpool_init(1, "misc");
	print_thread_init(1, "misc");

	thread_pool_log_prefetch = init_log_fetch_from_local_nvm_thpool();
	thread_pool_log_fetch = init_log_fetch_from_primary_nic_dram_thpool();
	thread_pool_fsync = init_fsync_thpool();
	thread_pool_loghdr_fetch = init_loghdr_fetch_thpool();
	thread_pool_copy_done_handle = init_copy_done_ack_handle_thpool();

#ifdef PIPELINE_RATE_LIMIT
	thread_pool_rate_limiter = thpool_init(1, "rate_limit");
	init_rate_limiter();
	thpool_add_work(thread_pool_rate_limiter, rate_limit_worker, NULL);
#endif
#ifdef PREFETCH_FLOW_CONTROL 
	init_prefetch_rate_limiter();
#endif
#ifdef BACKUP_RDMA_MEMCPY
	thread_pool_handle_heartbeat = thpool_init(1, "hb_handle");
	thread_pool_heartbeat_checker = thpool_init(1, "hb_check");
	thpool_add_work(thread_pool_heartbeat_checker, check_heartbeat_worker,
			NULL);
#endif

#ifdef NVM_WRITE_CONCURRENT
	thread_pool_nvm_write = thpool_init(8);
	print_thread_init(8, "nvm_write");
#endif

	init_pipeline_common();

	// Init pipeline
	init_host_memcpy();

	// initialize rpc module
	init_rpc(mrs, n_regions, mlfs_conf.port, signal_callback,
		 low_lat_signal_callback);

#ifdef NIC_SIDE
	// Init remote mrs for nicrpc storage engine..
	remote_mrs = (struct mr_context *)mlfs_zalloc(
		sizeof(struct mr_context) * n_regions);
	remote_n_regions = n_regions;

	addr_t start_addr;
	uint64_t len;
	for (i = 0; i < n_regions; i++) {
		switch (i) {
		case MR_NVM_LOG:
			start_addr = (uint64_t)g_bdev[g_log_dev]->map_base_addr;
			len = ((disk_sb[g_log_dev].nlog) << g_block_size_shift);
			set_remote_mr(start_addr, len, i);
			break;

		case MR_NVM_SHARED:
			start_addr =
				(uint64_t)g_bdev[g_root_dev]->map_base_addr;
			len = (sb[g_root_dev]->ondisk->size
			       << g_block_size_shift); // All possible address
						       // of device. Ref) mkfs.c
			set_remote_mr(start_addr, len, i);
			break;

		case MR_DRAM_BUFFER:
			// We don't need this information.
			break;
		}
		printf("remote_mrs[%d] type=%d, addr=0x%lx - 0x%lx, "
		       "length=%lu\n",
		       i, remote_mrs[i].type, remote_mrs[i].addr,
		       remote_mrs[i].addr + remote_mrs[i].length,
		       remote_mrs[i].length);
	}
#endif

	rpc_request_host_memcpy_buffer_addr(
		g_kernfs_peers[nic_kid_to_host_kid(g_kernfs_id)]
			->sockfd[SOCK_BG]);

	// Print global variables.
	print_rpc_setup();
	print_sync_ctx();
	print_g_peers();
	print_g_kernfs_peers();

#endif

	send_ready_signal("nicfs");

	// TODO sleep forever???
	while (1) {
		sleep(100000);
	}
	printf("Bye!\n");
}

#ifdef NVM_WRITE_CONCURRENT // Used in host side.
struct nvm_write_arg {
	int msg_sockfd;
	uint32_t msg_id;
	int dev;
	uint8_t *buf_addr;
	addr_t blockno;
	uint32_t io_size;
	uint32_t offset;
};

static void nvm_write_worker(void *arg)
{
	struct nvm_write_arg *nw_arg = (struct nvm_write_arg *)arg;

	/* To make all writes synchronous. Currently, response is sent as
	nvm_write_worker runs.
	// setup response for ACK.
	int buffer_id = rpc_nicrpc_setup_response(nw_arg->msg_sockfd, "|nrresp
	|", nw_arg->msg_id);
	*/

	int ret;
	// write data.
	if (nw_arg->offset) {
		ret = g_bdev[nw_arg->dev]->storage_engine->write_opt_unaligned(
			nw_arg->dev, nw_arg->buf_addr, nw_arg->blockno,
			nw_arg->offset, nw_arg->io_size);
	} else {
		ret = g_bdev[nw_arg->dev]->storage_engine->write_opt(
			nw_arg->dev, nw_arg->buf_addr, nw_arg->blockno,
			nw_arg->io_size);
	}
	if (ret != nw_arg->io_size)
		panic("Failed to write storage: Write size and io_size "
		      "mismatch.\n");

	/* To make all writes synchronous.
	// send response.
	rpc_nicrpc_send_response(nw_arg->msg_sockfd, buffer_id);
	*/

	mlfs_free(arg);
}

#endif /* NVM_WRITE_CONCURRENT */

static void print_memcpy_meta (memcpy_meta_t *meta, uint64_t i)
{
	printf("%lu HOST_MEMCPY i=%lu is_single_blk=%d to_dev=%u blk_nr=%lu "
	       "data=%p size=%d offset=%d\n",
	       get_tid(), i, meta->is_single_blk, meta->to_dev, meta->blk_nr,
	       meta->data, meta->size, meta->offset_in_blk);
}

// Send pipeline metadata to the next kernfs. It is called when a new libfs is
// bootstrapped.
// static void send_pipeline_libfs_meta(int libfs_id) {
//         int next_kernfs = get_next_kernfs(g_kernfs_id);
//         rpc_send_pipeline_meta_to_next_replica(
//                 g_kernfs_peers[next_kernfs]->sockfd[SOCK_BG], libfs_id);
// }

static void handle_bootstrap(void *arg)
{
	int sockfd, i = 0, n = 0;
	uint64_t seqn;
	uint64_t *ack_bit = 0;
	struct peer_id *libfs_peer;
	struct bootstrap_arg *bs_arg = (struct bootstrap_arg *)arg;

	sockfd = bs_arg->sockfd;
	seqn = bs_arg->seqn;

	// libfs bootstrap calls are forwarded to all other KernFS instances
	libfs_peer = g_rpc_socks[sockfd]->peer;
	register_peer_log(libfs_peer, 1);
	for (i = g_kernfs_id; i < g_n_nodes + g_kernfs_id; i++) {
		n = i % g_n_nodes;

		if (n == g_kernfs_id)
			continue;

#ifdef NIC_OFFLOAD
		// Do not send to Host kernfs of replicas.
		if (!is_kernfs_on_nic(n) &&
		    !is_local_kernfs(libfs_peer->id, n)) {
			continue;
		}
#endif
		ack_bit = rpc_alloc_ack_bit();
		rpc_register_log(g_kernfs_peers[n]->sockfd[SOCK_BG], libfs_peer,
				 ack_bit);
		rpc_wait_ack(ack_bit);
	}

	// send_pipeline_libfs_meta(libfs_peer->id);
	send_pipeline_kernfs_meta();

	g_sync_ctx[libfs_peer->id]->libfs_rate_limit_addr =
		bs_arg->libfs_rate_limit_addr;

	rpc_bootstrap_response(sockfd, seqn);
	mlfs_free(arg);
}

static void handle_peer_register(void *arg)
{
	struct peer_register_arg *pr_arg = (struct peer_register_arg *)arg;
	struct peer_id *peer;

	peer = _find_peer(pr_arg->ip, pr_arg->pid);
	mlfs_assert(peer);
	peer->id = pr_arg->id;

	register_peer_log(peer, 0);

	if (mlfs_conf.persist_nvm_with_clflush &&
	    is_last_kernfs(peer->id, g_kernfs_id)) {
		// Send an RPC to register libfs peer in host kernfs.
		uint64_t *ack_bit = 0;
		int host_kernfs_id, sockfd;

		host_kernfs_id = nic_kid_to_host_kid(g_kernfs_id);
		sockfd = g_kernfs_peers[host_kernfs_id]->sockfd[SOCK_BG];

		ack_bit = rpc_alloc_ack_bit();
		rpc_register_remote_libfs_peer(sockfd, peer, ack_bit);
		rpc_wait_ack(ack_bit);
	}

	// Send ack.
	rpc_set_remote_bit(pr_arg->sockfd, pr_arg->ack_bit_p);
	mlfs_free(arg);
}

// Deprecated.
//static void register_pipe_meta(void *arg)
//{
//	struct register_pipe_meta_arg *pm_arg =
//		(struct register_pipe_meta_arg *)arg;
//
//	// Wait until libfs is registered.
//	while (g_sync_ctx[pm_arg->libfs_id] == 0) {
//		;
//	}
//
//	// g_sync_ctx[pm_arg->libfs_id]->seqn_completed_update_addr =
//	//         pm_arg->seqn_compl_update_addr;
//
//	// printf("libfs=%d seqn_completed_udpate_addr=0x%lx\n", pm_arg->libfs_id,
//	//        pm_arg->seqn_compl_update_addr);
//	printf("libfs=%d rate_limit_addr=0x%lx\n", pm_arg->libfs_id,
//	       pm_arg->rate_limit_addr);
//
//	if (!is_last_kernfs(pm_arg->libfs_id, g_kernfs_id)) {
//		send_pipeline_libfs_meta(pm_arg->libfs_id);
//	}
//
//	mlfs_free(arg);
//}

void print_all_thpool_profile_results(void)
{
#ifdef PROFILE_THPOOL
	int i;

	// Prints all thpool profile results.
	printf("===Thread_Scheduling_Stat ==========\n");
	// Pipeline thpools.
	print_fetch_log_thpool_stat();
	print_coalesce_thpool_stat();
	print_loghdr_thpool_stat();
	print_build_memcpy_list_thpool_stat();
	// Print build_memcpy_list thpool for all LibFSes.
	for (i = g_n_nodes; i < g_n_nodes + MAX_LIBFS_PROCESSES; i++) {
		if (g_sync_ctx[i])
			print_profile_result(
				g_sync_ctx[i]->thpool_build_memcpy_list);
	}

	// Print host_memcpy thpool for all LibFSes.
	for (i = g_n_nodes; i < g_n_nodes + MAX_LIBFS_PROCESSES; i++) {
		if (g_sync_ctx[i])
			print_profile_result(
				g_sync_ctx[i]->thpool_host_memcpy_req);
	}
	print_host_memcpy_thpool_stat();
	print_compress_thpool_stat();
	print_copy_log_thpool_stat();

	// Print manage_fsync_ack thpool for all LibFSes.
	for (i = g_n_nodes; i < g_n_nodes + MAX_LIBFS_PROCESSES; i++) {
		if (g_sync_ctx[i])
			print_profile_result(
				g_sync_ctx[i]->thpool_manage_fsync_ack);
	}

	// Other thpools.
	print_profile_result(thread_pool_ssd);
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
	if (msg->data) {
		sscanf(msg->data, "|%s |", cmd_hdr);
		pr_rpc("RECV: %s sockfd=%d seqn=%lu", msg->data, msg->sockfd,
		       msg->id);
		mlfs_info("received rpc with body: %s on sockfd %d\n",
			  msg->data, msg->sockfd);
	} else {
		cmd_hdr[0] = 'i';
		mlfs_info("received imm with id %u on sockfd %d\n", msg->id,
			  msg->sockfd);
	}

	if (cmd_hdr[0] == 'c') {
		if (cmd_hdr[1] == 'd') // fh : RPC_LOG_COPY_DONE
			goto log_copy_done;

	} else if (cmd_hdr[0] == 'f') {
		if (cmd_hdr[1] == 'h') // fh : RPC_FETCH_LOGHDR
			goto fetch_loghdr;
		else if (cmd_hdr[1] == 'l')
			goto fetch_log; // fl : RPC_FETCH_LOG

	} else if (cmd_hdr[0] == 'h') {
		if (cmd_hdr[1] == 'b') // hb : RPC_HEARTBEAT
			goto heartbeat;

	} else if (cmd_hdr[0] == 'l') {
		if (cmd_hdr[1] == 'p') // lp : RPC_LOG_PREFETCH
			goto log_prefetch;

	} else if (cmd_hdr[0] == 'm') {
		if (cmd_hdr[1] == 'a') // ma : RPC_MEMCPY_BUF_ADDR
			goto memcpy_buf_addr;

	} else if (cmd_hdr[0] == 'p') {
		if (cmd_hdr[1] == 'k') // mc : RPC_PIPELINE_KERNFS_META
			goto pipeline_kernfs_meta;
		// else if (cmd_hdr[1] == 'm') // mc : RPC_PIPELINE_META
		//         goto pipeline_meta;
		else if (cmd_hdr[1] == 'r') // tr : RPC_RESET_BREAKDOWN_TIMER
			goto publish_remains;

	}  else if (cmd_hdr[0] == 't') {
		if (cmd_hdr[1] == 'p') // tp : RPC_PRINT_BREAKDOWN_TIMER
			goto timer_print;
		else if (cmd_hdr[1] == 'r') // tr : RPC_RESET_BREAKDOWN_TIMER
			goto timer_reset;
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
	if (cmd_hdr[0] == 'l') {
#if MLFS_LEASE
		// char path[MAX_PATH];
		int type;
		uint32_t req_id;
		uint32_t inum;
		uint32_t version;
		addr_t blknr;
		sscanf(msg->data, "|%s |%u|%u|%d|%u|%lu", cmd_hdr, &req_id,
		       &inum, &type, &version, &blknr);
		// mlfs_debug("received remote lease acquire with inum %u |
		// type[%d]\n", inum, type);

		int ret =
			modify_lease_state(req_id, inum, type, version, blknr);

		// If ret < 0 due to incorrect lease manager
		// (a) For read/write lease RPCs, return 'invalid lease request'
		// to LibFS [Only for LEASE_MIGRATION] (b) For lease
		// revocations, simply forward to correct lease manager
		if (ret < 0) {
			rpc_lease_invalid(msg->sockfd,
					  g_rpc_socks[msg->sockfd]->peer->id,
					  inum, msg->id);
		} else {
			rpc_send_lease_ack(msg->sockfd, msg->id);
		}
#ifdef LESAE_MIGRATION
		if (ret < 0) {
			rpc_lease_change(abs(ret), req_id, inum, type, version,
					 blknr, 0);
		}
#endif
#else
		panic("invalid code path\n");
#endif
	} else if (cmd_hdr[0] == 'm') { // migrate lease (enforce)
#if MLFS_LEASE
		int digest;
		uint32_t inum;
		uint32_t new_kernfs_id;
		sscanf(msg->data, "|%s |%u|%d", cmd_hdr, &inum, &new_kernfs_id);
		update_lease_manager(inum, new_kernfs_id);
		// rpc_send_ack(msg->sockfd, msg->id);
#else
		panic("invalid code path\n");
#endif
	} else if (cmd_hdr[0] == 'b') {
		uint32_t pid;
		uintptr_t libfs_rate_limit_addr;
		struct bootstrap_arg *bs_arg;

		sscanf(msg->data, "|%s |%u|%lu", cmd_hdr, &pid,
		       &libfs_rate_limit_addr);

		bs_arg = (struct bootstrap_arg *)mlfs_alloc(
			sizeof(struct bootstrap_arg));
		bs_arg->sockfd = msg->sockfd;
		bs_arg->seqn = msg->id;
		bs_arg->libfs_rate_limit_addr = libfs_rate_limit_addr;
		mlfs_printf("handling bootstrap: %s\n", msg->data);
		thpool_add_work(thread_pool_misc, handle_bootstrap,
				(void *)bs_arg);

	} else if (cmd_hdr[0] == 'p') { // log registration
		struct peer_register_arg *pr_arg;
		pr_arg = (struct peer_register_arg *)mlfs_alloc(
			sizeof(struct peer_register_arg));

		sscanf(msg->data, "|%s |%d|%u|%lu|%s", cmd_hdr, &pr_arg->id,
		       &pr_arg->pid, &pr_arg->ack_bit_p, pr_arg->ip);
		pr_arg->sockfd = msg->sockfd;

		thpool_add_work(thread_pool_misc, handle_peer_register,
				(void *)pr_arg);
	} else if (cmd_hdr[0] == 'a') { // ack (ignore)
		goto ret;
	}

#ifdef NIC_OFFLOAD
	else if (cmd_hdr[0] == 'n' && cmd_hdr[1] == 'r') { // nic_rpc
							   // request/response
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
		uint8_t *data_buf = 0, *buf_addr = 0;
		int buffer_id, ret, len;
		// struct app_context *resp_msg;
		struct buffer_head *bh_data;
		char prefix[1000]; // enough to include prefix of data.
		struct rdma_meta_entry *resp;
		uintptr_t src_addr, dst_addr; // Used for RDMA write.

		if (strcmp(cmd_hdr, "nrwol") == 0) { // Write opt local
			sscanf(msg->data, "|%s |%u|%lu|%lu|%u", cmd_hdr, &dev,
			       &buf_addr_ul, &blockno, &io_size);
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
			nw_arg->offset = 0;

			thpool_add_work(thread_pool_nvm_write, nvm_write_worker,
					(void *)nw_arg);

			// setup response for ACK.
			buffer_id = rpc_nicrpc_setup_response(
				msg->sockfd, "|nrresp |", msg->id);
			// send response. Asynchronously. NVM memcpy is done by
			// nvm_write_worker.
			rpc_nicrpc_send_response(msg->sockfd, buffer_id);

#else /* NVM_WRITE_CONCURRENT */
			// setup response for ACK.
			buffer_id = rpc_nicrpc_setup_response(
				msg->sockfd, "|nrresp |", msg->id);

			// write data.
			ret = g_bdev[dev]->storage_engine->write_opt(
				dev, buf_addr, blockno, io_size);
			if (ret != io_size)
				panic("Failed to write storage: Write size and "
				      "io_size mismatch.\n");

			// send response.
			rpc_nicrpc_send_response(msg->sockfd, buffer_id);
#endif /* NVM_WRITE_CONCURRENT */

		} else if (strcmp(cmd_hdr, "nrwolu") == 0) { // Write opt local
							     // unaligned
			sscanf(msg->data, "|%s |%u|%lu|%lu|%u|%u", cmd_hdr,
			       &dev, &buf_addr_ul, &blockno, &offset, &io_size);
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

			thpool_add_work(thread_pool_nvm_write, nvm_write_worker,
					(void *)nw_arg);

			// setup response for ACK.
			buffer_id = rpc_nicrpc_setup_response(
				msg->sockfd, "|nrresp |", msg->id);
			// send response.
			rpc_nicrpc_send_response(msg->sockfd, buffer_id);

#else /* NVM_WRITE_CONCURRENT */
			// setup response for ACK.
			buffer_id = rpc_nicrpc_setup_response(
				msg->sockfd, "|nrresp |", msg->id);

			// write data.
			ret = g_bdev[dev]->storage_engine->write_opt_unaligned(
				dev, buf_addr, blockno, offset, io_size);
			if (ret != io_size)
				panic("Failed to write storage: Write size and "
				      "io_size mismatch.\n");

			// send response.
			rpc_nicrpc_send_response(msg->sockfd, buffer_id);
#endif /* NVM_WRITE_CONCURRENT */

		} else if (strcmp(cmd_hdr, "nrsw") == 0) { // Send and write
			mlfs_assert(0); // Not used any more.

		} else if (strcmp(cmd_hdr, "nrswu") == 0) { // Send and write
							    // unaligned
			mlfs_assert(0); // Not used any more.

		} else if (strcmp(cmd_hdr, "nrcommit") == 0) {
			sscanf(msg->data, "|%s |%u", cmd_hdr, &dev);

			// commit.
			mlfs_commit(dev);

			// send response.
			buffer_id = rpc_nicrpc_setup_response(
				msg->sockfd, "|nrresp |", msg->id);
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

log_copy_done : { // RPC_LOG_COPY_DONE
	copy_done_arg *cd_arg = mlfs_zalloc(sizeof(copy_done_arg));
	sscanf(msg->data, "|%s |%d|%lu|%lu|%lu|%lu|%d", cmd_hdr, &cd_arg->libfs_id,
	       &cd_arg->seqn, &cd_arg->start_blknr, &cd_arg->log_size,
	       &cd_arg->orig_log_size, &cd_arg->fsync);
	atomic_init(&cd_arg->processed, 0);
	thpool_add_work(thread_pool_copy_done_handle, handle_copy_done_ack,
			(void *)cd_arg);
	goto ret;
}

fetch_loghdr : { // RPC_FETCH_LOGHDR
	fetch_loghdrs_arg *fh_arg = mlfs_zalloc(sizeof(fetch_loghdrs_arg));
	sscanf(msg->data, "|%s |%d|%lu|%lu|%u|%lu|%lu|%lu", cmd_hdr,
	       &fh_arg->libfs_id, &fh_arg->seqn, &fh_arg->remote_loghdr_buf,
	       &fh_arg->n_loghdrs, &fh_arg->digest_blk_cnt,
	       &fh_arg->fetch_start_blknr, &fh_arg->fetch_loghdr_done_addr);

	// mlfs_printf("RECV fetch_loghdr: libfs_id=%d seqn=%lu\n",
	//             fh_arg->libfs_id, fh_arg->seqn);

	thpool_add_work(thread_pool_loghdr_fetch, fetch_loghdrs,
			(void *)fh_arg);
	goto ret;
}

fetch_log : {
	log_fetch_from_nic_arg *fl_arg =
		mlfs_zalloc(sizeof(log_fetch_from_nic_arg));
	sscanf(msg->data, "|%s |%d|%lu|%lu|%lu|%lu|%lu|%lu|%d|%lu", cmd_hdr,
	       &fl_arg->libfs_id, &fl_arg->seqn, &fl_arg->remote_log_buf_addr,
	       &fl_arg->log_size, &fl_arg->orig_log_size, &fl_arg->start_blknr,
	       &fl_arg->fetch_log_done_addr, &fl_arg->fsync,
	       &fl_arg->fsync_ack_addr);

#ifdef NO_PIPELINING
	thpool_add_work(thread_pool_log_fetch,
			fetch_log_from_primary_nic_dram_bg,
			(void *)fl_arg);
#else
	thpool_add_work(thread_pool_log_fetch,
			fetch_log_from_primary_nic_dram_bg, (void *)fl_arg);
	mlfs_assert(!fl_arg->fsync);	// On fsync, msg is sent via low-lat channel.
#endif
	goto ret;
}

heartbeat: {
		   uint64_t heartbeat_seqn;
		   sscanf(msg->data, "|%s |%lu", cmd_hdr, &heartbeat_seqn);
		   thpool_add_work(thread_pool_handle_heartbeat,
				   handle_heartbeat, (void*)heartbeat_seqn);
		   goto ret;
	   }

log_prefetch : {
	log_fetch_from_local_nvm_arg *lf_arg =
		mlfs_zalloc(sizeof(log_fetch_from_local_nvm_arg));

	sscanf(msg->data, "|%s |%d|%lu|%lu|%u|%lu|%lu|%d|%d|%lu", cmd_hdr,
	       &lf_arg->libfs_id, &lf_arg->seqn, &lf_arg->prefetch_start_blknr,
	       &lf_arg->n_to_prefetch_loghdr, &lf_arg->n_to_prefetch_blk,
	       &lf_arg->libfs_base_addr, &lf_arg->reset_meta, &lf_arg->fsync,
	       &lf_arg->fsync_ack_addr);

	// Prefetch log in background.
	thpool_add_work(thread_pool_log_prefetch, fetch_log_from_local_nvm_bg,
			(void *)lf_arg);

	// mlfs_assert(!lf_arg->fsync);	// On fsync, msg is sent via low-lat channel.

	goto ret;
}

memcpy_buf_addr : {
	uintptr_t host_memcpy_addr;

	sscanf(msg->data, "|%s |%lu", cmd_hdr, &host_memcpy_addr);

	register_host_memcpy_buf_addrs(host_memcpy_addr);

	goto ret;
}

pipeline_kernfs_meta : {
	if (!primary_rate_limit_addr)
		sscanf(msg->data, "|%s |%lu", cmd_hdr,
		       &primary_rate_limit_addr);

	goto ret;
}

//pipeline_meta : { // deprecated.
//	struct register_pipe_meta_arg *pm_arg =
//		mlfs_zalloc(sizeof(struct register_pipe_meta_arg));
//
//	sscanf(msg->data, "|%s |%d|%lu", cmd_hdr, &pm_arg->libfs_id,
//	       &pm_arg->rate_limit_addr);
//	       // &pm_arg->seqn_compl_update_addr);
//
//	thpool_add_work(thread_pool_misc, register_pipe_meta, (void *)pm_arg);
//
//	goto ret;
//}
publish_remains : {
	int libfs_id;
	uint64_t seqn;
	char cmd[50];
	sscanf(msg->data, "|%s |%d|%lu", cmd_hdr, &libfs_id, &seqn);
	sprintf(cmd, "|" TO_STR(RPC_PUBLISH_REMAINS) " |%d|%lu|", libfs_id,
		seqn);
	if (!is_last_kernfs(libfs_id, g_kernfs_id))
		rpc_forward_msg(g_sync_ctx[libfs_id]->next_digest_sockfd, cmd);

	publish_all_remains(g_sync_ctx[libfs_id]);

	goto ret;
}

timer_reset : {
	int libfs_id;
	char cmd[50];
	sscanf(msg->data, "|%s |%d", cmd_hdr, &libfs_id);
	sprintf(cmd, "|" TO_STR(RPC_RESET_BREAKDOWN_TIMER) " |%d|", libfs_id);
	if (!is_last_kernfs(libfs_id, g_kernfs_id))
		rpc_forward_msg(g_sync_ctx[libfs_id]->next_digest_sockfd, cmd);

	reset_breakdown_timers();
	goto ret;
}

timer_print : {
	// relay msg to the next peer.
	int libfs_id;
	char cmd[50];
	sscanf(msg->data, "|%s |%d", cmd_hdr, &libfs_id);
	sprintf(cmd, "|" TO_STR(RPC_PRINT_BREAKDOWN_TIMER) " |%d|", libfs_id);

	if (libfs_id != g_n_nodes) {
		printf("Ignore timer_print message from libfs %d.\n", libfs_id);
		goto ret;
	}

	if (!is_last_kernfs(libfs_id, g_kernfs_id))
		rpc_forward_msg(g_sync_ctx[libfs_id]->next_digest_sockfd, cmd);

	print_all_thpool_profile_results();

#ifdef PROFILE_CIRCBUF
	print_all_circ_buf_stats();
#endif

	print_all_pipeline_stage_stats();

	nic_slab_print_stat();

	print_breakdown_timers();
	reset_breakdown_timers(); // Reset after printing times.

	goto ret;
}

ret : {
	// END_TIMER(evt_k_signal_callback);
	return;
}
}

void low_lat_signal_callback(struct app_context *msg)
{
#ifndef NIC_SIDE
	mlfs_assert(0); // low_lat channel should not be used without
			// NIC-offloading.
#endif

	char cmd_hdr[12];
	if (msg->data) {
		sscanf(msg->data, "|%s |", cmd_hdr);
		pr_rpc("RECV(low_lat): %s sockfd=%d seqn=%lu", msg->data,
		       msg->sockfd, msg->id);
		mlfs_info("(low_lat) received rpc with body: %s on sockfd %d\n",
			  msg->data, msg->sockfd);
	} else {
		cmd_hdr[0] = 'i';
		mlfs_assert(0); // No imm request to low_lat.
	}

	// if (cmd_hdr[0] == 'r') {
	if (cmd_hdr[0] == 'f') {
		if (cmd_hdr[1] == 'l')
			goto fetch_log; // fl : RPC_FETCH_LOG

	} else if (cmd_hdr[0] == 'l') {
		if (cmd_hdr[1] == 'p') // lp : RPC_LOG_PREFETCH
			goto log_prefetch;

	} else {
		mlfs_printf("(low_lat) RECV: %s\n", msg->data);
		panic("Unidentified remote signal\n");
	}

	goto ret;

fetch_log : {
	log_fetch_from_nic_arg *fl_arg =
		mlfs_zalloc(sizeof(log_fetch_from_nic_arg));
	sscanf(msg->data, "|%s |%d|%lu|%lu|%lu|%lu|%lu|%lu|%d|%lu", cmd_hdr,
	       &fl_arg->libfs_id, &fl_arg->seqn, &fl_arg->remote_log_buf_addr,
	       &fl_arg->log_size, &fl_arg->orig_log_size, &fl_arg->start_blknr,
	       &fl_arg->fetch_log_done_addr, &fl_arg->fsync,
	       &fl_arg->fsync_ack_addr);

#ifdef NO_PIPELINING
	thpool_add_work(thread_pool_log_fetch,
			fetch_log_from_primary_nic_dram_bg,
			(void *)fl_arg);
#else
	thpool_add_work(thread_pool_fsync, fetch_log_from_primary_nic_dram_bg,
			(void *)fl_arg);

	mlfs_assert(fl_arg->fsync); // Low-lat channel is used only on fsync.
#endif
	goto ret;
}

log_prefetch : {
	log_fetch_from_local_nvm_arg *lf_arg =
		mlfs_zalloc(sizeof(log_fetch_from_local_nvm_arg));

	sscanf(msg->data, "|%s |%d|%lu|%lu|%u|%lu|%lu|%d|%d|%lu", cmd_hdr,
	       &lf_arg->libfs_id, &lf_arg->seqn, &lf_arg->prefetch_start_blknr,
	       &lf_arg->n_to_prefetch_loghdr, &lf_arg->n_to_prefetch_blk,
	       &lf_arg->libfs_base_addr, &lf_arg->reset_meta, &lf_arg->fsync,
	       &lf_arg->fsync_ack_addr);

	thpool_add_work(thread_pool_fsync, fetch_log_from_local_nvm_bg,
			(void *)lf_arg);
	// mlfs_assert(lf_arg->fsync); // Low-lat channel is used only on fsync.
	goto ret;
}


ret : { // goto is used to measure time spent in this function.
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
	// addr_t size = nr_blks_between_ptrs(g_log_sb->start_persist,
	// end_blk+1);
	int flags = 0;

	flags |= (PMEM_PERSIST_FLUSH | PMEM_PERSIST_DRAIN);

	uint32_t size = 0;

	// wrap around
	// FIXME: pass log start_blk
	if (g_peers[id]->log_sb->start_persist + n_log_blk > g_log_size)
		g_peers[id]->log_sb->start_persist = g_sync_ctx[id]->begin;

	// move_log_ptr(&g_log_sb->start_persist, 0, n_log_blk);

	// mlfs_commit(id, g_peers[id]->log_sb->start_persist, 0, (n_log_blk <<
	// g_block_size_shift), flags);

	g_peers[id]->log_sb->start_persist += n_log_blk;

	// TODO: apply to individual logs to avoid flushing any unused memory
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

		loghdr_blk =
	next_loghdr_blknr(loghdr->hdr_blkno,loghdr->nr_log_blocks,
				g_fs_log->log_sb_blk+1,
	atomic_load(&g_log_sb->end)); loghdr = read_log_header(g_fs_log->dev,
	loghdr_blk);
	}
	*/
#endif
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
		  "[inode start %lu bmap start %lu datablock start %lu log "
		  "start %lu]\n",
		  disk_sb[dev].size, disk_sb[dev].ndatablocks,
		  disk_sb[dev].ninodes, disk_sb[dev].inode_start,
		  disk_sb[dev].bmap_start, disk_sb[dev].datablock_start,
		  disk_sb[dev].log_start);

	sb[dev]->ondisk = &disk_sb[dev];

	// set all rb tree roots to NULL
	for (int i = 0; i < (MAX_LIBFS_PROCESSES + g_n_nodes); i++)
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

	// single partitioned allocation, used for debugging.
	mlfs_info("dev %u: # of segment %u\n", dev, sb[dev]->n_partition);
	sb[dev]->num_blocks = disk_sb[dev].ndatablocks;
	sb[dev]->reserved_blocks = disk_sb[dev].datablock_start;
	sb[dev]->s_bdev = g_bdev[dev];

	mlfs_free(bh->b_data);
	bh_release(bh);
}

static void handle_heartbeat(void *arg) {
	update_host_heartbeat((uint64_t)arg);
}
// #endif /* __aarch64__ */