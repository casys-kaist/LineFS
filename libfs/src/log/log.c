#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <unistd.h>

#include "distributed/posix_wrapper.h"
//#include "posix/posix_interface.h"
#include "distributed/replication.h"
#include "mlfs/mlfs_user.h"
#include "log/log.h"
#include "concurrency/thread.h"
#include "filesystem/fs.h"
#include "filesystem/slru.h"
#include "io/block_io.h"
#include "global/mem.h"
#include "global/util.h"
#include "global/global.h"
#include "mlfs/mlfs_interface.h"
#include "storage/storage.h"
#include "concurrency/thpool.h"

#if MLFS_LEASE
#include "experimental/leases.h"
#endif

/**
 A system call should call start_log_tx()/commit_log_tx() to mark
 its start and end. Usually start_log_tx() just increments
 the count of in-progress FS system calls and returns.

 Log appends are synchronous.
 For crash consistency, log blocks are persisted first and
 then log header is perstisted in commit_log_tx().
 Digesting happens as a unit of a log block group
 (each logheader for each group).
 n in the log header indicates # of live log blocks.
 After digesting all log blocks in a group, the digest thread
 unsets inuse bit, indicating the group  can be garbage-collected.
 Note that the log header must be less than
 a block size for crash consistency.

 On-disk format of log area
 [ log superblock | log header | log blocks | log header | log blocks ... ]
 [ log header | log blocks ] is a transaction made by a system call.

 Each logheader describes a log block group created by
 a single transaction. Different write syscalls use different log group.
 start_log_tx() start a new log group and commit_log_tx()
 serializes writing multiple log groups to log area.

 TODO: This mechanism is currently unnecessary. Crash consistency guarantees
 are now tied to fsync calls. We no longer need to persist log headers for each
 and every system call. commit_log_tx() can be called after an fsync and transaction
 groups can be arbitrarily sized (not bounded at 3). This will help reduce space
 amplification (not a big deal) but most importantly the # of rdma operations that
 need to be posted to replicate the logs efficiently.

 We still need to think about workloads that trigger fsyncs after small writes.
 This potentially wastes space if a logheader needs to consume an entire block.
 */

volatile struct fs_log *g_fs_log[g_n_devices];
volatile struct log_superblock *g_log_sb[g_n_devices];

// for communication with kernel fs.
//int g_sock_fd;

#ifdef DISTRIBUTED
//FIXME: temporary fix; remove asap
int pending_sock_fd;

static void persist_replicated_logs(int dev, addr_t n_log_blk);
#endif

//static struct sockaddr_un g_srv_addr, g_addr;

static void read_log_superblock(int dev, struct log_superblock *log_sb);
static void write_log_superblock(int dev, struct log_superblock *log_sb);
static void update_log_superblock(int dev, struct log_superblock *log_sb);
static void commit_log(void);
static void digest_log(void);
static void request_log_fetch(int is_fsync);

pthread_mutex_t *g_log_mutex_shared;

//pthread_t is unsigned long
static unsigned long digest_thread_id[g_n_devices];
//Thread entry point
void *digest_thread(void *arg);

mlfs_time_t start_time;
mlfs_time_t end_time;
int started = 0;

void init_log(int id)
{
	int ret;
	//int volatile done = 0;
	pthread_mutexattr_t attr;
	char* env;

	int dev = 0;

	if (sizeof(struct logheader) > g_block_size_bytes) {
		printf("log header size %lu block size %lu\n",
				sizeof(struct logheader), g_block_size_bytes);
		panic("initlog: too big logheader");
	}

	if (dev < 0 || dev > g_n_devices) {
		printf("log dev %d should be between %d and %d\n",
				dev, 0, g_n_devices);
		panic("initlog: invalid log dev");
	}

	g_fs_log[dev] = (struct fs_log *)mlfs_zalloc(sizeof(struct fs_log));
	g_log_sb[dev] = (struct log_superblock *)mlfs_zalloc(sizeof(struct log_superblock));

	g_fs_log[dev]->log_sb_blk = disk_sb[g_log_dev].log_start + id * g_log_size;
	//g_fs_log[dev]->size = disk_sb[dev].nlog;

	//FIXME: this is not the actual log size (rename variable!)
	g_fs_log[dev]->size = g_fs_log[dev]->log_sb_blk + g_log_size;

	// FIXME: define usage of log dev
	g_fs_log[dev]->dev = g_log_dev;
	g_fs_log[dev]->id = dev;
	g_fs_log[dev]->nloghdr = 0;

	ret = pipe((int*)g_fs_log[dev]->digest_fd);
	if (ret < 0) 
		panic("cannot create pipe for digest\n");

	read_log_superblock(dev, (struct log_superblock *)g_log_sb[dev]);

	g_fs_log[dev]->log_sb = g_log_sb[dev];

	// Assuming all logs are digested by recovery.
	//g_fs_log[dev]->next_avail_header = disk_sb[dev].log_start + 1; // +1: log superblock
	//g_fs_log[dev]->start_blk = disk_sb[dev].log_start + 1;

	g_fs_log[dev]->next_avail_header = g_fs_log[dev]->log_sb_blk + 1;
	g_fs_log[dev]->start_blk = g_fs_log[dev]->log_sb_blk + 1;

	mlfs_debug("end of the log %lx\n", g_fs_log[dev]->size);

	g_log_sb[dev]->start_digest = g_fs_log[dev]->next_avail_header;
	g_log_sb[dev]->start_persist = g_fs_log[dev]->next_avail_header;

	write_log_superblock(dev, (struct log_superblock *)g_log_sb[dev]);

	atomic_init(&g_log_sb[dev]->n_digest, 0);
	atomic_init(&g_log_sb[dev]->end, 0);

	//g_fs_log->outstanding = 0;
	g_fs_log[dev]->start_version = g_fs_log[dev]->avail_version = 0;

	pthread_spin_init(&g_fs_log[dev]->log_lock, PTHREAD_PROCESS_SHARED);

	// g_log_mutex_shared is shared mutex between parent and child.
	g_log_mutex_shared = (pthread_mutex_t *)mlfs_zalloc(sizeof(pthread_mutex_t));
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(g_log_mutex_shared, &attr);

	g_fs_log[dev]->shared_log_lock = (pthread_mutex_t *)mlfs_zalloc(sizeof(pthread_mutex_t));
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(g_fs_log[dev]->shared_log_lock, &attr);

#ifdef LIBFS
	//digest_thread_id[dev] = mlfs_create_thread(digest_thread, &dev);

        // enable/disable statistics for log
	enable_perf_stats = 0;

	/* wait until the digest thread get started */
	//while(!g_fs_log[dev]->ready);

#endif

	printf("Initialized log[%d] dev %d start_blk %lu end %lu\n", id, g_log_dev,
			g_fs_log[dev]->start_blk, g_fs_log[dev]->size);
}

void shutdown_log(int id)
{
	mlfs_info("Shutting down log with id: %d\n", id);

	int dev=0;
#if defined(DISTRIBUTED) && !defined(MASTER)
	//Slave node should recalculate n_digest
	//this value might not be updated since slaves never call commit_log
	//update_log_superblock(g_log_sb);

#if 1
	mlfs_time_t sub;
	timersub(&start_time, &end_time, &sub)
        mlfs_printf("MAX TS = %ld\n", start_time.tv_sec);
        mlfs_printf("MIN TS = %ld\n", end_time.tv_sec);
    	mlfs_printf("Elapsed time (s): %ld %ld\n", sub.tv_sec, sub.tv_usec);
#endif

#endif

#ifdef NIC_OFFLOAD
	uint32_t remains;
	remains = atomic_load(&g_fs_log[dev]->log_sb->n_digest);
	if (remains) {
		printf("Publishing remaining log data. n_loghdr=%u\n", remains);
		replicate_log_by_fsync();
	}

#ifdef BATCH_MEMCPY_LIST
	sleep(1);
	// flush remaining host_memcpy requests.
	request_publish_remains();
#endif

	// TODO wait on publishing / fsync
	uint64_t issued_seqn =
		atomic_load(&get_next_peer()->recently_issued_seqn);
	printf("Wait until we get all acks of issued requests. issued_seqn=%lu "
	       "acked_seqn=%lu\n",
	       issued_seqn, atomic_load(&get_next_peer()->recently_acked_seqn));
	while (issued_seqn !=
	       atomic_load(&get_next_peer()->recently_acked_seqn)) {
	}
	printf("All acks received. issued_seqn=%lu acked_seqn=%lu\n",
	       issued_seqn, atomic_load(&get_next_peer()->recently_acked_seqn));
	printf("File System is going to shut down after 2 sec.\n");

	// FIXME: Do we need to wait? Sleeping here makes filebench hang at the
	// end of the execution.
	// sleep(2); // Wait a while for not completed fetch requests.
	return;
#endif

	// wait until the digest_thread finishes job.
	if (g_fs_log[dev]->digesting) {
		mlfs_info("%s", "[L] Wait finishing on-going digest\n");
		wait_on_digesting_seg(dev);
	}

#if MLFS_MASTER && g_n_replica > 1
	// wait until the peer finishes all outstanding replication requests.
	if (get_next_peer()->outstanding) {
		mlfs_info("%s", "[L] Wait finishing on-going peer replication\n");
		wait_on_peer_replicating(get_next_peer());
	}

	// wait until the peer's digest_thread finishes job.
	if (get_next_peer()->digesting) {
		mlfs_info("%s", "[L] Wait finishing on-going peer digest\n");
		wait_on_peer_digesting(get_next_peer());
	}
#endif

	if (atomic_load(&g_fs_log[dev]->log_sb->n_digest)) {
                mlfs_info("%s", "[L] Digesting remaining log data\n");
		while(make_digest_seg_request_async(dev, 100) != -EBUSY);
		m_barrier();
		wait_on_digesting_seg(dev);
#if MLFS_MASTER
		wait_on_peer_digesting(get_next_peer());
#endif
	}
}

static loghdr_t *read_log_header(uint16_t dev, addr_t blkno)
{
       int ret;
       struct buffer_head *bh;
       loghdr_t *hdr_data = mlfs_alloc(sizeof(struct logheader));

       bh = bh_get_sync_IO(g_log_dev, blkno, BH_NO_DATA_ALLOC);
       bh->b_size = sizeof(struct logheader);
       bh->b_data = (uint8_t *)hdr_data;

       bh_submit_read_sync_IO(bh);

       return hdr_data;
}

static void read_log_superblock(int dev, struct log_superblock *log_sb)
{
	int ret;
	struct buffer_head *bh;

	bh = bh_get_sync_IO(g_log_dev, g_fs_log[dev]->log_sb_blk, 
			BH_NO_DATA_ALLOC);

	bh->b_size = sizeof(struct log_superblock);
	bh->b_data = (uint8_t *)log_sb;

	bh_submit_read_sync_IO(bh);

	bh_release(bh);

	return;
}

static void write_log_superblock(int dev, struct log_superblock *log_sb)
{
	int ret;
	struct buffer_head *bh;

	bh = bh_get_sync_IO(g_log_dev, g_fs_log[dev]->log_sb_blk, 
			BH_NO_DATA_ALLOC);

	bh->b_size = sizeof(struct log_superblock);
	bh->b_data = (uint8_t *)log_sb;

	mlfs_write(bh);

	bh_release(bh);

	return;
}

static void update_log_superblock(int dev, struct log_superblock *log_sb)
{
	loghdr_t *loghdr;
	addr_t loghdr_blkno = log_sb->start_digest;

	int wrap = 0;
	uint8_t n_digest = 0;

	assert(loghdr_blkno > disk_sb[dev].log_start);

	while(loghdr_blkno != 0)
	{
		loghdr = read_log_header(dev, loghdr_blkno);
		//FIXME: possible issue if LH_COMMIT_MAGIC matches despite a non-existent loghdr
		if (loghdr->inuse != LH_COMMIT_MAGIC) {
			if(log_sb->start_digest > disk_sb[dev].log_start + 1 && !wrap) {
				atomic_store(&log_sb->end, loghdr_blkno - 1);
				loghdr_blkno = disk_sb[dev].log_start + 1;
				wrap = 1;
			}
			else
				break;
		}
		else {
			n_digest++;
			loghdr_blkno += loghdr->nr_log_blocks;
			assert(loghdr_blkno <= g_fs_log[dev]->size);
		}
	}

	//check that we wrapped around correctly
	if(wrap) {
		addr_t remaining_blocks = g_fs_log[dev]->size - atomic_load(&log_sb->end);
		loghdr = read_log_header(dev, g_fs_log[dev]->log_sb_blk + 1);
		assert(loghdr->nr_log_blocks > remaining_blocks);
	}

	atomic_store(&log_sb->n_digest, n_digest);
	//we don't care about persisting log_sb; since we digest everything anyways
	//write_log_superblock(g_log_sb);
}

inline addr_t log_alloc(uint32_t nr_blocks)
{
	int ret;

	/* g_fs_log->start_blk : header
	 * g_fs_log->next_avail_header : tail
	 *
	 * There are two cases:
	 *
	 * <log_begin ....... log_start .......  next_avail_header .... log_end>
	 *	      digested data      available data       empty space
	 *
	 *	                    (ver 10)              (ver 9)
	 * <log_begin ...... next_avail_header ....... log_start ...... log_end>
	 *	       available data       digested data       available data
	 *
	 */
	//mlfs_assert(g_fs_log->avail_version - g_fs_log->start_version < 2);

#ifdef NIC_OFFLOAD
	// Report exceeding threshold.
	// if (nr_used_blk > ((mlfs_conf.digest_threshold * g_log_size) / 100)) {
	//         pr_digest("Exceeding digest threshold. Log status: "
	//                   "%.3f%% (threshold: %d%%)",
	//                   (double)nr_used_blk * 100 / (uint64_t)g_log_size,
	//                   mlfs_conf.digest_threshold);
	// }
#else
	// Log is getting full. make asynchronous digest request.
	if (!g_fs_log[0]->digesting) {
		addr_t nr_used_blk = 0;
		if (g_fs_log[0]->avail_version == g_fs_log[0]->start_version) {
			mlfs_assert(g_fs_log[0]->next_avail_header >= g_fs_log[0]->start_blk);
			nr_used_blk = g_fs_log[0]->next_avail_header - g_fs_log[0]->start_blk;
		} else {
			nr_used_blk = (g_fs_log[0]->size - g_fs_log[0]->start_blk);
			nr_used_blk += (g_fs_log[0]->next_avail_header - g_fs_log[0]->log_sb_blk);
		}

		if (nr_used_blk > ((mlfs_conf.digest_threshold * g_log_size) / 100)) {
                        pr_digest("Exceeded digest threshold. nr_used_blk: %lu (threshold: %lu)",
					nr_used_blk, mlfs_conf.digest_threshold * g_log_size / 100);
                        // pr_digest("g_fs_log[]-> next_avail_header=%lu "
                        //           "start_blk=%lu size=%lu",
                        //           g_fs_log[0]->next_avail_header,
                        //           g_fs_log[0]->start_blk, g_fs_log[0]->size);
                        while(make_digest_seg_request_async(0, 100) != -EBUSY)
                        pr_digest("%s", "[L] log is getting full. asynchronous digest!\n");
		}
	}
#endif

	// next_avail_header reaches the end of log.
	//if (g_fs_log->next_avail_header + nr_blocks > g_fs_log->log_sb_blk + g_fs_log->size) {
	if (g_fs_log[0]->next_avail_header + nr_blocks > g_fs_log[0]->size) {
		// Store n_unsync before wrap around.
		// It is used in replication with NIC-offloading.
		if (get_next_peer())
		    atomic_store(&get_next_peer()->n_unsync_upto_end,
			    atomic_load(&get_next_peer()->n_unsync));
		atomic_store(&g_log_sb[0]->end, g_fs_log[0]->next_avail_header - 1);

		g_fs_log[0]->next_avail_header = g_fs_log[0]->log_sb_blk + 1;

		atomic_add(&g_fs_log[0]->avail_version, 1);

		pr_digest("-- log tail is rotated: new start %lu new end %lu start_version %u avail_version %u",
				g_fs_log[0]->next_avail_header, atomic_load(&g_log_sb[0]->end),
				g_fs_log[0]->start_version, g_fs_log[0]->avail_version);
	}

	addr_t next_log_blk =
		__sync_fetch_and_add(&g_fs_log[0]->next_avail_header, nr_blocks);

	// TODO profile time of waiting for data publish.

	// This has many policy questions.
	// Current implmentation is very converative.
	// Pondering the way of optimization.
#ifdef NIC_OFFLOAD
	struct timespec sync_publish_start;
	struct timespec sync_publish_end;
	uint64_t sync_pub_cnt = 0;
	double dur = 0;

	clock_gettime(CLOCK_MONOTONIC, &sync_publish_start);

	while (g_fs_log[0]->avail_version > g_fs_log[0]->start_version &&
	       g_fs_log[0]->next_avail_header > g_fs_log[0]->start_blk) {

		bool log_full;
		int avail_ver, start_ver;
		addr_t next_avail_header, start_blk;

		avail_ver = g_fs_log[0]->avail_version;
		start_ver = g_fs_log[0]->start_version;
		next_avail_header = g_fs_log[0]->next_avail_header;
		start_blk = g_fs_log[0]->start_blk;

		log_full = (avail_ver > start_ver) &&
			   (next_avail_header > start_blk);

		if (!log_full)
			break;

		clock_gettime(CLOCK_MONOTONIC, &sync_publish_end);
		dur = get_duration(&sync_publish_start, &sync_publish_end);
		if (dur > 0.5) {
			sync_pub_cnt++;
			mlfs_printf(ANSI_COLOR_RED "Log is full. Waiting time: "
						   "%lu sec. avail_version=%d "
						   "start_version=%d "
						   "next_avail_header=%lu "
						   "start_blk=%lu" ANSI_COLOR_RESET "\n",
				    sync_pub_cnt / 2, avail_ver, start_ver,
				    next_avail_header, start_blk);
			mlfs_printf(
				"issued_seqn=%lu acked_seqn=%lu\n",
				atomic_load(
					&get_next_peer()->recently_issued_seqn),
				atomic_load(
					&get_next_peer()->recently_acked_seqn));

			// Reset.
			clock_gettime(CLOCK_MONOTONIC, &sync_publish_start);
			clock_gettime(CLOCK_MONOTONIC, &sync_publish_end);
		}
	}

	return next_log_blk;
#endif


retry:
	if (g_fs_log[0]->avail_version > g_fs_log[0]->start_version) {

		if (g_fs_log[0]->next_avail_header > g_fs_log[0]->start_blk ||
			g_fs_log[0]->start_blk - g_fs_log[0]->next_avail_header
				< (g_log_size/ 5)) {
			// mlfs_info("%s", "\x1B[31m [L] synchronous digest request and wait! \x1B[0m\n");
			mlfs_printf("%s", "\x1B[31m [L] synchronous digest request and wait! \x1B[0m\n");
			// mlfs_printf("g_fs_log[]-> next_avail_header=%lu start_blk=%lu size=%lu start_version=%u avail_version=%u\n",
				// g_fs_log[0]->next_avail_header,
				// g_fs_log[0]->start_blk, g_fs_log[0]->size,
				// g_fs_log[0]->start_version, g_fs_log[0]->avail_version);

			while (make_digest_seg_request_async(0, 95) != -EBUSY);

			m_barrier();
			wait_on_digesting_seg(0);
		}
	}

	if (g_fs_log[0]->avail_version > g_fs_log[0]->start_version &&
	    g_fs_log[0]->next_avail_header > g_fs_log[0]->start_blk) {
		goto retry;
	}

	if (0) {
		int i;
		for (i = 0; i < nr_blocks; i++) {
			mlfs_info("alloc %lu\n", next_log_blk + i);
		}
	}

	return next_log_blk;
}

// allocate logheader meta and attach logheader.
static inline struct logheader_meta *loghd_alloc(struct logheader *lh)
{
	struct logheader_meta *loghdr_meta;

	loghdr_meta = (struct logheader_meta *)
		mlfs_zalloc(sizeof(struct logheader_meta));

	if (!loghdr_meta)
		panic("cannot allocate logheader_meta");

	INIT_LIST_HEAD(&loghdr_meta->link);

	loghdr_meta->loghdr = lh;

	return loghdr_meta;
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void persist_log_header(struct logheader_meta *loghdr_meta,
		addr_t hdr_blkno)
{
	struct logheader *loghdr = loghdr_meta->loghdr;
	struct buffer_head *io_bh;
	int i;
	uint64_t start_tsc;

	if (enable_perf_stats)
		start_tsc = asm_rdtscp();

	io_bh = bh_get_sync_IO(g_log_dev, hdr_blkno, BH_NO_DATA_ALLOC);

	if (enable_perf_stats) {
		g_perf_stats.bcache_search_tsc += (asm_rdtscp() - start_tsc);
		g_perf_stats.bcache_search_nr++;
	}
	//pthread_spin_lock(&io_bh->b_spinlock);

	mlfs_get_time(&loghdr->mtime);
	io_bh->b_data = (uint8_t *)loghdr;
	io_bh->b_size = sizeof(struct logheader);

	mlfs_write(io_bh);

	mlfs_debug("pid %u [log header] inuse %d blkno %lu next_hdr_blockno %lu\n", 
			getpid(),
			loghdr->inuse, io_bh->b_blocknr, 
			next_loghdr_blknr(loghdr_meta->hdr_blkno,
				loghdr->nr_log_blocks, g_fs_log[0]->log_sb_blk+1,
				atomic_load(&g_log_sb[0]->end)));

	if (loghdr_meta->ext_used) {
		io_bh->b_data = loghdr_meta->loghdr_ext;
		io_bh->b_size = loghdr_meta->ext_used;
		io_bh->b_offset = sizeof(struct logheader);
		mlfs_write(io_bh);
	}

	bh_release(io_bh);

	//pthread_spin_unlock(&io_bh->b_spinlock);
}

// called at the start of each FS system call.
void start_log_tx(void)
{
	struct logheader_meta *loghdr_meta;

#ifndef CONCURRENT
	pthread_mutex_lock(g_log_mutex_shared);
	g_fs_log[0]->outstanding++;

	mlfs_debug("start log_tx %u\n", g_fs_log[0]->outstanding);
	mlfs_assert(g_fs_log[0]->outstanding == 1);
#endif

	loghdr_meta = get_loghdr_meta();

#ifdef LOG_OPT
	struct logheader *prev_loghdr = loghdr_meta->previous_loghdr;
	addr_t prev_hdr_blkno = loghdr_meta->prev_hdr_blkno;
#endif

	memset(loghdr_meta, 0, sizeof(struct logheader_meta));

#ifdef LOG_OPT
	loghdr_meta->previous_loghdr = prev_loghdr;
	loghdr_meta->prev_hdr_blkno = prev_hdr_blkno;
#endif

	if (!loghdr_meta)
		panic("cannot locate logheader_meta\n");

	loghdr_meta->hdr_blkno = 0;
	INIT_LIST_HEAD(&loghdr_meta->link);

	/*
	if (g_fs_log[0]->outstanding == 0 ) {
		mlfs_debug("outstanding %d\n", g_fs_log->outstanding);
		panic("outstanding\n");
	}
	*/
}

void abort_log_tx(void)
{
	struct logheader_meta *loghdr_meta;

	loghdr_meta = get_loghdr_meta();

	if (loghdr_meta->is_hdr_allocated)
		mlfs_free(loghdr_meta->loghdr);

#ifndef CONCURRENT
	pthread_mutex_unlock(g_log_mutex_shared);
	g_fs_log[0]->outstanding--;
#endif

	return;
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
void commit_log_tx(void)
{
	int do_commit = 0;

	/*
	if(g_fs_log->outstanding > 0) {
		do_commit = 1;
	} else {
		panic("commit when no outstanding tx\n");
	}
	*/

	do_commit = 1;

	if(do_commit) {
		uint64_t tsc_begin;
		struct logheader_meta *loghdr_meta;

		if (enable_perf_stats)
			tsc_begin = asm_rdtscp();

		commit_log();

		loghdr_meta = get_loghdr_meta();


#if LOG_OPT
                if (loghdr_meta->is_hdr_allocated) {
                    if(loghdr_meta->previous_loghdr)
                        mlfs_free(loghdr_meta->previous_loghdr);

                    loghdr_meta->previous_loghdr = loghdr_meta->loghdr;
                    loghdr_meta->prev_hdr_blkno = loghdr_meta->hdr_blkno;
                }
#else
                if (loghdr_meta->is_hdr_allocated)
                    mlfs_free(loghdr_meta->loghdr);
#endif

#ifndef CONCURRENT
		g_fs_log[0]->outstanding--;
		pthread_mutex_unlock(g_log_mutex_shared);
		mlfs_debug("commit log_tx %u\n", g_fs_log[0]->outstanding);
#endif
		if (enable_perf_stats) {
			g_perf_stats.log_commit_tsc += (asm_rdtscp() - tsc_begin);
			g_perf_stats.log_commit_nr++;
		}
	} else {
		panic("it has a race condition\n");
	}
}

static int persist_log_inode(struct logheader_meta *loghdr_meta, uint32_t idx)
{
	struct inode *ip;
	addr_t logblk_no;
	uint32_t nr_logblocks = 0;
	struct buffer_head *log_bh;
	struct logheader *loghdr = loghdr_meta->loghdr;
	uint64_t start_tsc;

	logblk_no = loghdr_meta->hdr_blkno + loghdr_meta->pos;
	loghdr->blocks[idx] = loghdr_meta->pos;
	loghdr_meta->pos++;

	mlfs_assert(loghdr_meta->pos <= loghdr->nr_log_blocks);

	if (enable_perf_stats)
		start_tsc = asm_rdtscp();

	log_bh = bh_get_sync_IO(g_log_dev, logblk_no, BH_NO_DATA_ALLOC);

	if (enable_perf_stats) {
		g_perf_stats.bcache_search_tsc += (asm_rdtscp() - start_tsc);
		g_perf_stats.bcache_search_nr++;
	}

	//loghdr->blocks[idx] = logblk_no;

	ip = icache_find(loghdr->inode_no[idx]);
	mlfs_assert(ip);

	log_bh->b_data = (uint8_t *)ip->_dinode;
	log_bh->b_size = sizeof(struct dinode);
	log_bh->b_offset = 0;

	if (ip->flags & I_DELETING) {
		// icache_del(ip);
		// ideleted_add(ip);
		ip->flags &= ~I_VALID;
		bitmap_clear(sb[g_root_dev]->s_inode_bitmap, ip->inum, 1);
	}

	nr_logblocks = 1;

	mlfs_assert(log_bh->b_blocknr < g_fs_log[0]->next_avail_header);
	mlfs_assert(log_bh->b_dev == g_fs_log[0]->dev);

	mlfs_debug("inum %u offset %lu @ blockno %lu\n",
				loghdr->inode_no[idx], loghdr->data[idx], logblk_no);

	mlfs_write(log_bh);

	bh_release(log_bh);

#if MLFS_LEASE
	//surrender_lease(ip->inum);
#endif

	//mlfs_assert((log_bh->b_blocknr + nr_logblocks) == g_fs_log->next_avail);

	return 0;
}

/* This is a critical path for write performance.
 * Stay optimized and need to be careful when modifying it */
static int persist_log_file(struct logheader_meta *loghdr_meta, 
		uint32_t idx, uint8_t n_iovec)
{
	uint32_t k, l, size;
	offset_t key;
	struct fcache_block *fc_block;
	addr_t logblk_no;
	uint32_t nr_logblocks = 0;
	struct buffer_head *log_bh;
	struct logheader *loghdr = loghdr_meta->loghdr;
	uint32_t io_size;
	struct inode *inode;
	lru_key_t lru_entry;
	uint64_t start_tsc, start_tsc_tmp;
	int ret;

	inode = icache_find(loghdr->inode_no[idx]);

	mlfs_assert(inode);

	size = loghdr_meta->io_vec[n_iovec].size;

	// Handling small write (< 4KB).
	if (size < g_block_size_bytes) {
		/* fc_block invalidation and coalescing.

		   1. find fc_block -> if not exist, allocate fc_block and perform writes
				    -> if exist, fc_block may or may not be valid.

		   2. if fc_block is valid, then do coalescing (disabled for now)
		   Note: Reasons for disabling coalescing:
			(a) the 'blocks' array in the loghdr now stores the relative block #
			    to the loghdr blk # (and not absolute). In other words, We don't
			    have a way of pointing at an older log entry.
			(b) current coalescing implementation doesn't account for gaps in partial
		            block writes. Loghdr metadata isn't expressive enough.
			(c) coalecsing can result in data loss when rsyncing (if the block we're
			    coalescing to has already been rsynced.
		   TODO: Address the aforementioned issues and re-enable coalescing

		   3. if fc_block is not valid, then skip coalescing and update fc_block.
		*/

		uint32_t offset_in_block;

		key = (loghdr->data[idx] >> g_block_size_shift);
		offset_in_block = (loghdr->data[idx] % g_block_size_bytes);

		if (enable_perf_stats)
			start_tsc = asm_rdtscp();

		fc_block = fcache_find(inode, key);

		if (enable_perf_stats) {
			g_perf_stats.l0_search_tsc += (asm_rdtscp() - start_tsc);
			g_perf_stats.l0_search_nr++;
		}

		logblk_no = loghdr_meta->hdr_blkno + loghdr_meta->pos;
		loghdr->blocks[idx] = loghdr_meta->pos;
		loghdr_meta->pos++;

//disabling coalescing
#if 0
		if (fc_block) {
			ret = check_log_invalidation(fc_block);
			// fc_block is invalid. update it
			if (ret) {
				fc_block->log_version = g_fs_log[0]->avail_version;
				fc_block->log_addr = logblk_no;
			}
			// fc_block is valid
			else {
				if (fc_block->log_addr)  {
					logblk_no = fc_block->log_addr;
					fc_block->log_version = g_fs_log[0]->avail_version;
					mlfs_debug("write is coalesced %lu @ %lu\n", loghdr->data[idx], logblk_no);
				}
			}
		}
#endif

		if(fc_block)
			ret = check_log_invalidation(fc_block);

		if (!fc_block) {
			//mlfs_assert(loghdr_meta->pos <= loghdr->nr_log_blocks);

			fc_block = fcache_alloc_add(inode, key);
		}

		fc_block->log_version = g_fs_log[0]->avail_version;

		if(inode->itype == T_DIR)
			fcache_log_replace(fc_block, logblk_no, offset_in_block, size, g_fs_log[0]->avail_version);
		else
			fcache_log_add(fc_block, logblk_no, offset_in_block, size, g_fs_log[0]->avail_version);

		if (enable_perf_stats)
			start_tsc = asm_rdtscp();

		log_bh = bh_get_sync_IO(g_log_dev, logblk_no, BH_NO_DATA_ALLOC);

		if (enable_perf_stats) {
			g_perf_stats.bcache_search_tsc += (asm_rdtscp() - start_tsc);
			g_perf_stats.bcache_search_nr++;
		}

		// the logblk_no could be either a new block or existing one (patching case).
		//loghdr->blocks[idx] = logblk_no;

		// case 1. the IO fits into one block.
		if (offset_in_block + size <= g_block_size_bytes)
			io_size = size;
		// case 2. the IO incurs two blocks write (unaligned).
		else 
			panic("do not support this case yet\n");

		log_bh->b_data = loghdr_meta->io_vec[n_iovec].base;
		log_bh->b_size = io_size;
		log_bh->b_offset = offset_in_block;

		mlfs_assert(log_bh->b_dev == g_fs_log[0]->dev);

		mlfs_debug("inum %u offset %lu @ blockno %lu (partial io_size=%u)\n",
				loghdr->inode_no[idx], loghdr->data[idx], logblk_no, io_size);

		mlfs_write(log_bh);

		bh_release(log_bh);

#if 0
		// write sanity check
		struct buffer_head *debug_bh;
		char *debug_str = (char*) mlfs_alloc(io_size);
		debug_bh = bh_get_sync_IO(g_log_dev, logblk_no, BH_NO_DATA_ALLOC);
		debug_bh->b_data = (uint8_t *) debug_str;
		debug_bh->b_size = io_size;
		debug_bh->b_offset = offset_in_block;
		m_barrier();
		bh_submit_read_sync_IO(debug_bh);
		mlfs_debug("Sanity check. Reread log block after write. Output str: %.*s\n",
				io_size, debug_str);
		bh_release(debug_bh);
		mlfs_free(debug_str);
#endif

	} 
	// Handling large (possibly multi-block) write.
	else {
		offset_t cur_offset;

		//if (enable_perf_stats)
		//	start_tsc_tmp = asm_rdtscp();

		cur_offset = loghdr->data[idx];

		/* logheader of multi-block is always 4K aligned.
		 * It is guaranteed by mlfs_file_write() */
		mlfs_assert((loghdr->data[idx] % g_block_size_bytes) == 0);
		mlfs_assert((size % g_block_size_bytes) == 0);

		nr_logblocks = size >> g_block_size_shift; 

		mlfs_assert(nr_logblocks > 0);

		logblk_no = loghdr_meta->hdr_blkno + loghdr_meta->pos;
		loghdr->blocks[idx] = loghdr_meta->pos;
		loghdr_meta->pos += nr_logblocks;

		mlfs_assert(loghdr_meta->pos <= loghdr->nr_log_blocks);

		log_bh = bh_get_sync_IO(g_log_dev, logblk_no, BH_NO_DATA_ALLOC);

		log_bh->b_data = loghdr_meta->io_vec[n_iovec].base;
		log_bh->b_size = size;
		log_bh->b_offset = 0;

		//loghdr->blocks[idx] = logblk_no;

		// Update log address hash table.
		// This is performance bottleneck of sequential write.
		for (k = 0, l = 0; l < size; l += g_block_size_bytes, k++) {
			key = (cur_offset + l) >> g_block_size_shift;
			//mlfs_debug("updating log fcache: inode %u blknr %lu\n", inode->inum, key);
			mlfs_assert(logblk_no);

			if (enable_perf_stats)
				start_tsc = asm_rdtscp();

			fc_block = fcache_find(inode, key);

			if (enable_perf_stats) {
				g_perf_stats.l0_search_tsc += (asm_rdtscp() - start_tsc);
				g_perf_stats.l0_search_nr++;
			}

			if (!fc_block)
				fc_block = fcache_alloc_add(inode, key);


			fcache_log_del_all(fc_block); //delete any existing log patches (overwrite)

			fc_block->log_version = g_fs_log[0]->avail_version;
			fcache_log_add(fc_block, logblk_no + k, 0, g_block_size_bytes, g_fs_log[0]->avail_version);
		}

		mlfs_debug("inum %u offset %lu size %u @ blockno %lx (aligned)\n",
				loghdr->inode_no[idx], cur_offset, size, logblk_no);

		mlfs_write(log_bh);

		bh_release(log_bh);

		//if (enable_perf_stats) {
		//	g_perf_stats.tmp_tsc += (asm_rdtscp() - start_tsc_tmp);
		//}
	}

	return 0;
}

static uint32_t compute_log_blocks(struct logheader_meta *loghdr_meta)
{
	struct logheader *loghdr = loghdr_meta->loghdr; 
	uint8_t type, n_iovec; 
	uint32_t nr_log_blocks = 0;
	int i;

	for (i = 0, n_iovec = 0; i < loghdr->n; i++) {
		type = loghdr->type[i];

		switch(type) {
			case L_TYPE_UNLINK:
			case L_TYPE_INODE_CREATE:
			case L_TYPE_INODE_UPDATE: {
				nr_log_blocks++;
				break;
			} 
			case L_TYPE_DIR_ADD:
			case L_TYPE_DIR_RENAME:
			case L_TYPE_DIR_DEL:
			case L_TYPE_FILE: {
				uint32_t size;
				size = loghdr_meta->io_vec[n_iovec].size;

				if (size < g_block_size_bytes)
					nr_log_blocks++;
				else
					nr_log_blocks += 
						(size >> g_block_size_shift);
				n_iovec++;
				break;
			}
			default: {
				panic("unsupported log type\n");
				break;
			}
		}
	}

	return nr_log_blocks;
}

// Copy modified blocks from cache to log.
static void persist_log_blocks(struct logheader_meta *loghdr_meta)
{ 
	struct logheader *loghdr = loghdr_meta->loghdr; 
	uint32_t i, nr_logblocks = 0; 
	uint8_t type, n_iovec; 
	addr_t logblk_no; 

	//mlfs_assert(hdr_blkno >= g_fs_log->start);


	for (i = 0, n_iovec = 0; i < loghdr->n; i++) {
		type = loghdr->type[i];

		//mlfs_printf("commit log %u of type %u block_nr %lu\n", i, type, loghdr_meta->hdr_blkno);
		switch(type) {
			case L_TYPE_UNLINK:
			case L_TYPE_INODE_CREATE:
			case L_TYPE_INODE_UPDATE: {
				persist_log_inode(loghdr_meta, i);
				break;
			} 
				/* Directory information is piggy-backed in 
				 * log header */
			case L_TYPE_DIR_ADD:
			case L_TYPE_DIR_RENAME:
			case L_TYPE_DIR_DEL:
#if MLFS_LEASE
				//m_barrier();
				//mark_lease_revocable(loghdr->inode_no[i]);
#endif
			case L_TYPE_FILE: {
				persist_log_file(loghdr_meta, i, n_iovec);
				n_iovec++;
				break;
			}
			default: {
				panic("unsupported log type\n");
				break;
			}
		}
	}
}

static void request_log_fetch(int is_fsync)
{
	uint64_t n_unsync_blks = 0;
	addr_t end_blknr;
	uint32_t n_unsync, n_unsync_upto_end;
	addr_t local_start;
	int ret;

	// Load and update prefetch meta atomically. Locks can be unlocked in
	// calculate_prefetch_meta() function.
	// TODO merge two locks?
	pthread_mutex_lock(get_next_peer()->shared_rsync_n_lock);
	pthread_mutex_lock(get_next_peer()->shared_rsync_addr_lock);

retry:
	n_unsync_blks = atomic_load(&get_next_peer()->n_unsync_blk);
	n_unsync = atomic_load(&get_next_peer()->n_unsync);
	n_unsync_upto_end = atomic_load(&get_next_peer()->n_unsync_upto_end);
	end_blknr = atomic_load(g_sync_ctx[0]->end);
	local_start = get_next_peer()->local_start;

	pr_digest("[FETCH META] n_unsync_blks=%lu n_unsync=%u "
		  "n_unsync_upto_end=%u end_blknr=%lu local_start=%lu "
		  "g_fs_log[]->start_blk=%lu g_log_sb[]->start_digest=%lu "
		  "g_log_sb[]->n_digest=%u g_log_sb[]->n_digest_blks=%u",
		  n_unsync_blks, n_unsync, n_unsync_upto_end, end_blknr,
		  local_start, g_fs_log[0]->start_blk,
		  g_log_sb[0]->start_digest,
		  atomic_load(&g_log_sb[0]->n_digest),
		  atomic_load(&g_log_sb[0]->n_digest_blks));

	if (is_fsync || n_unsync_blks > mlfs_conf.log_prefetch_threshold) {
		ret = calculate_prefetch_meta(local_start, n_unsync_blks,
					      n_unsync, n_unsync_upto_end,
					      end_blknr, is_fsync);
		if (ret == -EAGAIN)
			goto retry;
	} else {
		pthread_mutex_unlock(get_next_peer()->shared_rsync_addr_lock);
		pthread_mutex_unlock(get_next_peer()->shared_rsync_n_lock);
	}
}

static void commit_log(void)
{
	struct logheader_meta *loghdr_meta;
	struct logheader *loghdr, *previous_loghdr;
	uint64_t tsc_begin, tsc_end, tsc_begin_tmp;

	//if (enable_perf_stats)
	//	tsc_begin_tmp = asm_rdtscp();

	// loghdr_meta is stored in TLS.
	loghdr_meta = get_loghdr_meta();
	mlfs_assert(loghdr_meta);

	/* There was no log update during transaction */
	if (!loghdr_meta->is_hdr_allocated)
		return;

	mlfs_assert(loghdr_meta->loghdr);
	loghdr = loghdr_meta->loghdr;
	previous_loghdr = loghdr_meta->previous_loghdr;

	if (loghdr->n <= 0)
		panic("empty log header\n");

	if (loghdr->n > 0) {
		uint32_t nr_log_blocks;
		uint32_t pos = 1;
		uint8_t do_coalesce = 0;
		uint32_t nr_extra_blocks = 0;

		// Pre-compute required log blocks for atomic append.
		nr_log_blocks = compute_log_blocks(loghdr_meta);
		nr_log_blocks++; // +1 for a next log header block;

		pthread_mutex_lock(g_fs_log[0]->shared_log_lock);
		//FIXME: temporarily changing to spinlocks
		//pthread_spin_lock(&g_fs_log->log_lock);

#ifdef LOG_OPT
		// check if loghdr entry can be coalesced
		if(!loghdr_meta->is_hdr_locked && loghdr->n == 1 &&
				loghdr->type[0] == L_TYPE_FILE && previous_loghdr &&
				previous_loghdr->type[0] == L_TYPE_FILE &&
				previous_loghdr->inode_no[0] == loghdr->inode_no[0]) {

			// two cases for coalescing: overwrite or append
			// TODO: support overwrites at non-identical offsets
			// TODO: Do not coalesce if previous_loghdr has been fsynced and/or replicated

			// overwrites
			if(previous_loghdr->data[0] == loghdr->data[0]) {
				if(previous_loghdr->length[0] >= loghdr->length[0])
					nr_extra_blocks = 0;
				else
					nr_extra_blocks = (loghdr->length[0] - previous_loghdr->length[0]
								>> g_block_size_shift);

				mlfs_debug("coalesce - overwrite existing log entry - block %lu nr_extra_blocks %u\n",
						loghdr_meta->prev_hdr_blkno, nr_extra_blocks);
			}
			// appends
			else if(previous_loghdr->data[0] + previous_loghdr->length[0] == loghdr->data[0]) {
				pos += (loghdr->data[0] - previous_loghdr->data[0] >> g_block_size_shift);
				nr_extra_blocks = (loghdr->data[0] + loghdr->length[0] >> g_block_size_shift) -
					(previous_loghdr->data[0] + previous_loghdr->length[0] >> g_block_size_shift);

				mlfs_debug("coalesce - append to existing log entry - block %lu nr_extra_blocks %u pos %u\n",
						loghdr_meta->prev_hdr_blkno, nr_extra_blocks, pos);
			}
			else {
				nr_extra_blocks = -1;
			}

			//FIXME: this currently does not support multi-threading
			if (nr_extra_blocks >= 0 && g_fs_log[0]->next_avail_header + nr_extra_blocks <= g_fs_log[0]->size) {
				do_coalesce = 1;
			}

		}

#endif
		// atomic log allocation.

		if(do_coalesce) {
			nr_log_blocks = nr_extra_blocks;
			log_alloc(nr_log_blocks);
			loghdr_meta->hdr_blkno = loghdr_meta->prev_hdr_blkno;
		}
		else
			loghdr_meta->hdr_blkno = log_alloc(nr_log_blocks);
		//loghdr_meta->nr_log_blocks = nr_log_blocks;
		// loghdr_meta->pos = 0 is used for log header block.
		loghdr_meta->pos = pos;

		//loghdr_meta->hdr_blkno = g_fs_log->next_avail_header;
		//g_fs_log->next_avail_header = loghdr_meta->hdr_blkno + loghdr_meta->nr_log_blocks;

		loghdr->nr_log_blocks = nr_log_blocks;
		loghdr->inuse = LH_COMMIT_MAGIC;

		pthread_mutex_unlock(g_fs_log[0]->shared_log_lock);
		//pthread_spin_unlock(&g_fs_log->log_lock);

		//if (enable_perf_stats)
		//	g_perf_stats.tmp_tsc += (asm_rdtscp() - tsc_begin_tmp);

		mlfs_debug("pid %u [commit] log block %lu nr_log_blocks %u\n",
				getpid(), loghdr_meta->hdr_blkno, loghdr->nr_log_blocks);
		mlfs_debug("pid %u [commit] current header %lu next header %lu\n",
				getpid(), loghdr_meta->hdr_blkno, g_fs_log[0]->next_avail_header);

		if (enable_perf_stats)
			tsc_begin = asm_rdtscp();

		// TODO: try to optimize this.
		// this guarantee is no longer necessary since
		// consistency is now tied to fsyncs

		/* Crash consistent order: log blocks write
		 * is followed by log header write */
		persist_log_blocks(loghdr_meta);

		if (enable_perf_stats) {
			tsc_end = asm_rdtscp();
			g_perf_stats.log_write_tsc += (tsc_end - tsc_begin);
		}

#if 0
		if(loghdr->next_loghdr_blkno != g_fs_log->next_avail_header) {
			printf("loghdr_blkno %lu, next_avail %lu\n",
					loghdr->next_loghdr_blkno, g_fs_log->next_avail_header);
			panic("loghdr->next_loghdr_blkno is tainted\n");
		}
#endif

		if (enable_perf_stats)
			tsc_begin = asm_rdtscp();

		// Write log header to log area (real commit)
		persist_log_header(loghdr_meta, loghdr_meta->hdr_blkno);

		if (enable_perf_stats) {
			tsc_end = asm_rdtscp();
			g_perf_stats.loghdr_write_tsc += (tsc_end - tsc_begin);
		}

#if defined(DISTRIBUTED) && defined(MASTER)
		//if (enable_perf_stats)
		//	tsc_begin = asm_rdtscp();

		update_peer_sync_state(get_next_peer(), loghdr->nr_log_blocks, 1);

#ifdef NIC_OFFLOAD
		/////// Prefetching log data in background by NIC kernfs //////
		if (mlfs_conf.log_prefetching && g_n_replica > 1) {
			START_TIMER(evt_prefetch_req);
			request_log_fetch(0);
			END_TIMER(evt_prefetch_req);
		}
#else
		//trigger asynchronous replication if we exceed our chunk size
		if(mlfs_conf.async_replication && g_n_replica > 1 &&
			g_rsync_chunk &&
			atomic_load(&get_next_peer()->n_unsync_blk) >
			g_rsync_chunk)
		{
			make_replication_request_async(g_rsync_chunk);
#ifdef LOG_OPT
			struct logheader_meta *loghdr_meta;
			loghdr_meta = get_loghdr_meta();
			mlfs_assert(loghdr_meta);
			loghdr_meta->is_hdr_locked = true;
#endif
			mlfs_info("%s", "[L] received remote replication signal. asynchronous replication!\n");
                }

#endif
		//if (enable_perf_stats)
		//	g_perf_stats.tmp_tsc += (asm_rdtscp() - tsc_begin);
#endif
                atomic_fetch_add(&g_log_sb[0]->n_digest, 1);
                atomic_fetch_add(&g_log_sb[0]->n_digest_blks, loghdr->nr_log_blocks);
                // mlfs_printf("n_digest %u n_digest_blks %u\n",
                        // atomic_load(&g_log_sb[0]->n_digest),
                        // atomic_load(&g_log_sb[0]->n_digest_blks));
	}
}

/* FIXME: Use of parameter and name of that are very confusing.
 * data: 
 *	file_inode - offset in file
 *	dir_inode - parent inode number
 * length: 
 *	file_inode - file size 
 *	dir_inode - offset in directory
 */
void add_to_loghdr(uint8_t type, struct inode *inode, offset_t data, 
		uint32_t length, void *extra, uint16_t extra_len)
{
	uint32_t i;
	struct logheader *loghdr;
	struct logheader_meta *loghdr_meta;

#if 1
	if(!started) {
		mlfs_get_time(&start_time);
		started = 1;
	}
	else {
		mlfs_get_time(&end_time);
	}
#endif

	loghdr_meta = get_loghdr_meta();

	mlfs_assert(loghdr_meta);

	if (!loghdr_meta->is_hdr_allocated) {
		loghdr = (struct logheader *)mlfs_zalloc(sizeof(*loghdr));

		loghdr_meta->loghdr = loghdr;
		loghdr_meta->is_hdr_allocated = 1;
	}

	loghdr = loghdr_meta->loghdr;

	if (loghdr->n >= g_fs_log[0]->size)
		panic("too big a transaction for log");

	/*
		 if (g_fs_log->outstanding < 1)
		 panic("add_to_loghdr: outside of trans");
	*/

	i = loghdr->n;

	if (i >= g_max_blocks_per_operation)
		panic("log header is too small\n");

	loghdr->type[i] = type;
	loghdr->inode_no[i] = inode->inum;

	if (type == L_TYPE_FILE ||
			type == L_TYPE_DIR_ADD ||
			type == L_TYPE_DIR_RENAME ||
			type == L_TYPE_DIR_DEL) 
		// offset in file.
		loghdr->data[i] = (offset_t)data;
	else
		loghdr->data[i] = 0;

	loghdr->length[i] = length;
	loghdr->n++;

	if (extra_len) {
		uint16_t ext_used = loghdr_meta->ext_used;

		loghdr_meta->loghdr_ext[ext_used] = '0' + i;
		ext_used++;
		memmove(&loghdr_meta->loghdr_ext[ext_used], extra, extra_len);
		ext_used += extra_len;
		strncat((char *)&loghdr_meta->loghdr_ext[ext_used], "|", 1);
		ext_used++;
		loghdr_meta->loghdr_ext[ext_used] = '\0';
		loghdr_meta->ext_used = ext_used;
	
		mlfs_assert(ext_used <= 2048);
	}

	mlfs_debug("add_to_loghdr [%s] type %u inum %u\n", (type == L_TYPE_FILE? "FILE" :
				type == L_TYPE_DIR_ADD? "DIR_ADD" : type == L_TYPE_DIR_RENAME? "DIR_RENAME" :
				type == L_TYPE_DIR_DEL? "DIR_DEL" : "UNKNOWN"), type, inode->inum);

	/*
		 if (type != L_TYPE_FILE)
		 mlfs_debug("[loghdr-add] dev %u, type %u inum %u data %lu\n",
		 inode->dev, type, inode->inum, data);
	 */
}

int mlfs_do_rdigest(uint32_t n_digest, uint32_t n_blk_digest)
{
#if defined(DISTRIBUTED) && defined(MASTER)
	if(g_n_replica == 1)
		return 0;

	//mlfs_do_rsync();
	//make_replication_request_async(0);

	IBV_AWAIT_PENDING_WORK_COMPLETIONS(get_next_peer()->info->sockfd[SOCK_IO]);
	m_barrier();

	//mlfs_assert(get_next_peer()->start_digest <= get_next_peer()->remote_start);
	//struct rpc_pending_io *pending[g_n_nodes];
	//for(int i=0; i<g_n_nodes; i++)
	rpc_remote_digest_async(get_next_peer()->info->id, get_next_peer(), n_digest, n_blk_digest, 0);

	//for(int i=0; i<g_n_nodes; i++)

	//if (enable_perf_stats)
	//	show_libfs_stats();

#endif
	return 0;
}

/**
 * Parameters
 *  triggered_by_digest : 1 if it is called on digest path.
 */
static void replicate_log(int triggered_by_digest)
{
#if defined(DISTRIBUTED) && defined(MASTER)
	START_TIMER(evt_rep_critical_host); // critical path starts at libfs.
	make_replication_request_sync(get_next_peer(),
		g_sync_ctx[get_next_peer()->id]->base_addr,
		g_sync_ctx[get_next_peer()->id]->peer->base_addr,
		0, 0, 0, 1, 0, 0);

	END_TIMER(evt_rep_critical_host2);

#ifdef LOG_OPT
	struct logheader_meta *loghdr_meta;
	loghdr_meta = get_loghdr_meta();
	mlfs_assert(loghdr_meta);
	loghdr_meta->is_hdr_locked = true;
#endif /* LOG_OPT */
#endif /* DISTRIBUTED & MASTER */
}

void replicate_log_by_digestion(void)
{
#ifdef NIC_OFFLOAD
	mlfs_assert(0); // deprecated path.
#endif
    if (!get_next_peer())
	return;

    START_TIMER(evt_rep_by_digest);
    replicate_log(1);
    END_TIMER(evt_rep_by_digest);
}

void replicate_log_by_fsync(void)
{
#ifdef NIC_OFFLOAD
	if (!get_next_peer())
		return;

	START_TIMER(evt_rep_by_fsync);
	request_log_fetch(1);
	END_TIMER(evt_rep_by_fsync);
#else
    if (!get_next_peer())
	return;

    START_TIMER(evt_rep_by_fsync);
    replicate_log(0);
    END_TIMER(evt_rep_by_fsync);
#endif
}

//////////////////////////////////////////////////////////////////////////
// Libmlfs signal callback (used to handle signaling between replicas)

#ifdef DISTRIBUTED
void signal_callback(struct app_context *msg)
{
	char cmd_hdr[12];

	if(msg->data) {
		sscanf(msg->data, "|%s |", cmd_hdr);
		pr_rpc("peer recv: %s sockfd=%d msg->id(seqn in ack)=%lu",
			msg->data, msg->sockfd, msg->id);
        } else {   // replication ack (imm).
		pr_rpc("peer recv: replication ACK seqn=%lu sockfd=%d",
			msg->id, msg->sockfd);
		return;
        }

	if (cmd_hdr[0] == 'r') {
#ifdef NO_BUSY_WAIT
	    if (cmd_hdr[1] == 'a')  // RPC_REPLICATE_ACK
		goto replicate_ack;
#endif

	} else if (cmd_hdr[0] == 'l') {
	    if (cmd_hdr[1] == 'a')  // RPC_LEASE_ACK
		goto lease_ack;
	}

	// master/slave callbacks
	// handles 2 message types (digest)
	if (cmd_hdr[0] == 'b') {
		uint32_t peer_id;
		sscanf(msg->data, "|%s |%u", cmd_hdr, &peer_id);
		g_self_id = peer_id;
		printf("Assigned LibFS ID=%u\n", g_self_id);
	}
#ifndef MASTER
	//slave callback
	//handles 2 types of messages (read, immediate)

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
	else if(cmd_hdr[0] == 'r') { //replicate and digest
		addr_t n_log_blk;
		int node_id, ack, steps, persist;
		uint64_t seqn;

		mlfs_debug("received log data from replica %d | n_log_blk: %u | steps: %u | ack_required: %s | persist: %s\n",
				node_id, n_log_blk, steps, ack?"yes":"no", persist?"yes":"no");	

	}
	else
		panic("unidentified remote signal\n");
#else
	//master callback
	//handles 1 type of messages (complete, lease, replicate)

	//digestion completion notification
	else if (cmd_hdr[0] == 'c') {
		//FIXME: this callback currently does not support coalescing;
		//it assumes remote and local kernfs digests the same amount
		//of data.
		int log_id, dev, rotated, lru_updated, sender_id;
		uint32_t n_digested;
		addr_t start_digest;

                sender_id = g_rpc_socks[msg->sockfd]->peer->id;

#ifdef NIC_OFFLOAD
		mlfs_assert(sender_id == g_kernfs_id);
		handle_digest_response(msg->data);
#else
                sscanf(msg->data, "|%s |%d|%d|%d|%lu|%d|%u|", cmd_hdr, &log_id,
                       &dev, &n_digested, &start_digest, &rotated,
                       &lru_updated);

		// local kernfs (or kernfs on NIC)
		if (sender_id == host_kid_to_nic_kid(g_kernfs_id))
		    handle_digest_response(msg->data);
		else
		    update_peer_digest_state(get_next_peer(), start_digest,
			    n_digested, rotated);
#endif
	}
	else if(cmd_hdr[0] == 'l') {
#if 0
		char path[MAX_PATH];
		int type;
		sscanf(msg->data, "|%s |%s |%d", cmd_hdr, path, &type);
		mlfs_debug("received remote lease acquire with path %s | type[%d]\n", path, type);
		resolve_lease_conflict(msg->sockfd, path, type, msg->id);
#else
		panic("invalid code path\n");
#endif
	}
	else if(cmd_hdr[0] == 'f') { // flush cache / revoke lease
#if MLFS_LEASE
		int digest;
		uint32_t inum;
		uint32_t n_digest;
		sscanf(msg->data, "|%s |%u|%d", cmd_hdr, &inum, &digest);

		// thpool_add_work(thread_pool, revoke_lease, NULL);
		revoke_lease(msg->sockfd, msg->id, inum);
#if 0
		if(digest) {
			// wait until the digest_thread finishes job.
			if (g_fs_log[0]->digesting) {
				mlfs_info("%s", "[L] Wait finishing on-going digest\n");
				wait_on_digesting_seg(0);
			}

			while(make_digest_request_async(100) == -EBUSY)
				cpu_relax();

			mlfs_printf("%s", "[L] lease contention detected. asynchronous digest!\n");
		}
#endif

		purge_dir_caches(inum);
		rpc_send_lease_ack(msg->sockfd, msg->id);
#else
		panic("invalid code path\n");
#endif
	}
	else if(cmd_hdr[0] == 'e') { // lease error
#if MLFS_LEASE
		uint32_t inum;
		sscanf(msg->data, "|%s |%u", cmd_hdr, &inum);
		report_lease_error(inum);
#else
		panic("invalid code path\n");
#endif
	}
	else if(cmd_hdr[0] == 'i') { //rdma immediate notification (ignore)
		mlfs_assert(0); // Is this path taken?
				// If it is, we need to add timer end for
				// replication breakdown.
		return;
        }
        else if (cmd_hdr[0] == 'n' && cmd_hdr[1] == 'r') {  // nic_rpc
                if (strcmp(cmd_hdr, "nrresp") == 0)
                    mlfs_info("%s", "[NIC RPC] nrresp Do nothing.\n");
                else
                    panic("unidentified nicrpc msg\n");
        }
	else {
		mlfs_printf("undefined msg:%s\n", cmd_hdr);
		panic("unidentified remote signal\n");
	}

	return;

#endif

lease_ack:
	{
	    // Do nothing.
	    uint32_t success;
	    uint64_t seqn;
	    sscanf(msg->data, "|%s |%d", cmd_hdr, &success);
	    pr_rep("Lease ack received. seqn=%lu sock=%d\n",
		    msg->id, msg->sockfd);
	    return;
	}
#ifdef NO_BUSY_WAIT
replicate_ack:
	{
		uint64_t ack_bit_addr;
		sscanf(msg->data, "|%s |%lu", cmd_hdr, &ack_bit_addr);

		// wakeup waiting thread.
		pthread_mutex_unlock((pthread_mutex_t *)ack_bit_addr);
		return;
	}
#endif
// Add more...
}
#endif

/////////////////////////////////////////////////////////////////////////

// Libmlfs digest thread.

void wait_on_digesting()
{
#ifdef NIC_OFFLOAD
	mlfs_assert(0);
#endif
	wait_on_digesting_seg(0);
}

void wait_on_digesting_seg(int dev)
{
	uint64_t tsc_begin, tsc_end;

	START_TIMER(evt_dig_wait_sync);

	if (enable_perf_stats)
		tsc_begin = asm_rdtsc();

	while(g_fs_log[dev]->digesting)
		cpu_relax();

	if (enable_perf_stats) {
		tsc_end = asm_rdtsc();
		g_perf_stats.digest_wait_tsc += (tsc_end - tsc_begin);
		g_perf_stats.digest_wait_nr++;
	}

	END_TIMER(evt_dig_wait_sync);
}

int make_digest_request_async(int percent)
{
#ifdef NIC_OFFLOAD
	// It is called directly by filebench. Files should be digested at
	// creation time before running the main bench. We just send an fsync
	// to digest all data.
	request_log_fetch(1);
#else
	return make_digest_seg_request_async(0, percent);
#endif
}

int make_digest_seg_request_async(int dev, int percent)
{
	//char cmd_buf[MAX_CMD_BUF] = {0};
	int ret = 0;

	//sprintf(cmd_buf, "|digest |%d|%d|", dev, percent);

	pr_digest("try digest async | digesting %d n_digest %u",
			g_fs_log[dev]->digesting, atomic_load(&g_log_sb[dev]->n_digest));

	if (!g_fs_log[dev]->digesting &&
		atomic_load(&g_log_sb[dev]->n_digest) > 0 &&
		(!get_next_peer() || !get_next_peer()->digesting)) { // no rep or peer->digesting == 0
		set_digesting(dev);
		//mlfs_debug("Send digest command: %s\n", cmd_buf);
		//ret = write(g_fs_log[dev]->digest_fd[1], cmd_buf, MAX_CMD_BUF);
                make_digest_request_sync(100);
#ifdef LOG_OPT
	struct logheader_meta *loghdr_meta;
	loghdr_meta = get_loghdr_meta();
	mlfs_assert(loghdr_meta);
	loghdr_meta->is_hdr_locked = true;
#endif
		return 0;
	} else
		return -EBUSY;
}

uint32_t make_digest_request_sync(int percent)
{
	return make_digest_seg_request_sync(0, percent);
}

uint32_t make_digest_seg_request_sync(int dev, int percent)
{
	int ret, i, target_id;
	char cmd[MAX_SOCK_BUF];
	uint32_t digest_count = 0, n_digest, n_blk_digest;
	loghdr_t *loghdr;
	struct inode *ip;

#ifndef LIBFS
        assert(false); // Only Libfs can send digest requests.
#endif
	// update start_digest (in case its pointing to a value higher than 'end')
	// reason: start_digest is returned from digest response and might not take wraparound
	// into account

	addr_t end = atomic_load(&g_log_sb[0]->end);
	if(end && g_fs_log[0]->start_blk > end) {
		g_fs_log[dev]->start_blk = g_fs_log[0]->log_sb_blk + 1;
		atomic_add(&g_fs_log[0]->start_version, 1);
	}

	//addr_t loghdr_blkno = g_fs_log[dev]->start_blk;
	//int log_id = 0;
	g_log_sb[dev]->start_digest = g_fs_log[dev]->start_blk;
	write_log_superblock(dev, (struct log_superblock *)g_log_sb[dev]);
	//set_digesting(dev);

#if defined(DISTRIBUTED) && defined(MASTER)
	//sync log before digesting (to avoid data loss)
	//FIXME: (1) Should we modify this to adhere to the specified 'percent'?
	//FIXME: (2) This delays the digestion process and blocks its thread;
	// 		contemplating possible optimizations

	//n_digest = atomic_load(&get_next_peer()->n_digest);
	n_digest = atomic_load(&g_log_sb[dev]->n_digest);

	mlfs_debug("sanity check: n_digest (local) %u n_digest (remote) %u\n",
			atomic_load(&g_log_sb[dev]->n_digest), n_digest);

	g_fs_log[dev]->n_digest_req = (percent * n_digest) / 100;

	//if(!atomic_compare_exchange_strong(&get_next_peer()->n_digest, &n_digest, 0))
	//	assert("race condition on remote n_digest");

	//////////////////// Replication : sync up if we are behind
	// FIXME: implement mlfs_do_rsync that replicates up to a certain number of log txs
	if(get_next_peer() && atomic_load(&get_next_peer()->n_digest) < g_fs_log[dev]->n_digest_req){
		replicate_log_by_digestion();
        }

	//FIXME: just for testing; remove this line
	//g_fs_log[dev]->n_digest_req = 10 * n_digest / 100;
	if(get_next_peer())
		atomic_fetch_sub(&get_next_peer()->n_digest, g_fs_log[dev]->n_digest_req);

	//log_id = g_self_id;
        n_blk_digest = atomic_load(&g_log_sb[dev]->n_digest_blks);

#else
	n_digest = atomic_load(&g_log_sb[dev]->n_digest);
	g_fs_log[dev]->n_digest_req = (percent * n_digest) / 100;
#endif
	socklen_t len = sizeof(struct sockaddr_un);

        // Set message and target id.
        sprintf(cmd, "|digest |%d|%d|%u|%lu|%lu|%lu|%u", g_self_id, g_log_dev,
                g_fs_log[dev]->n_digest_req, g_log_sb[dev]->start_digest,
                g_fs_log[dev]->log_sb_blk + 1, atomic_load(&g_log_sb[dev]->end),
                n_blk_digest);

#ifdef NIC_OFFLOAD
        target_id = host_kid_to_nic_kid(g_kernfs_id);
	mlfs_info("[NIC_OFFLOAD] Send digest req to %d\n", target_id);
#else
        target_id = g_kernfs_id;     // local kernfs
	mlfs_info("[NIC_OFFLOAD] Do local digestion. kernfs id: %d\n", target_id);
#endif

        pr_digest("Local digest request: log_id=%d dev_id=%d n_loghdr_digest=%u "
                "digest_start=%lu log_start=%lu log_end=%lu n_blk_digest=%u "
		"sock=%d",
                g_self_id, g_log_dev, g_fs_log[dev]->n_digest_req,
                g_log_sb[dev]->start_digest, g_fs_log[dev]->log_sb_blk + 1,
                atomic_load(&g_log_sb[dev]->end), n_blk_digest,
		g_kernfs_peers[target_id]->sockfd[SOCK_BG]);

	PR_METADATA(g_sync_ctx[dev]);
#if 0
	// send digest command
	ret = sendto(g_fs_log[dev]->kernfs_fd, cmd, MAX_SOCK_BUF, 0,
			(struct sockaddr *)&g_fs_log[dev]->kernfs_addr, len);
#else
        // send rpc request.
	rpc_forward_msg(g_kernfs_peers[target_id]->sockfd[SOCK_BG], cmd);
#endif

#ifndef NIC_OFFLOAD
	// In nic-offload case, digest request is relayed at local NIC kernfs.
	mlfs_do_rdigest(g_fs_log[dev]->n_digest_req, n_blk_digest);
#endif

	return n_digest;
}

static void cleanup_lru_list(int lru_updated)
{
	lru_node_t *node, *tmp;
	int i = 0;

	pthread_rwlock_wrlock(shm_lru_rwlock);

	list_for_each_entry_safe_reverse(node, tmp, &lru_heads[g_log_dev], list) {
		HASH_DEL(lru_hash, node);
		list_del(&node->list);
		mlfs_free_shared(node);
	}

	pthread_rwlock_unlock(shm_lru_rwlock);
}

#ifdef NIC_OFFLOAD
static void reset_publish_meta(void)
{
	atomic_add(&g_fs_log[0]->start_version, 1);

	if (g_fs_log[0]->start_version == g_fs_log[0]->avail_version)
		atomic_store(&g_log_sb[0]->end, 0);

	pr_digest("-- log head is rotated: new start %lu new end %lu "
		  "start_version %u avail_version %u",
		  g_fs_log[0]->next_avail_header,
		  atomic_load(&g_log_sb[0]->end), g_fs_log[0]->start_version,
		  g_fs_log[0]->avail_version);
}

void handle_digest_response(char *ack_cmd)
{
	char cmd_hdr[30] = {0};
	addr_t next_hdr_of_digested_hdr, digest_start_blknr;
	int libfs_id;
	uint64_t fetch_seqn;
	int log_id = 0;
	uint32_t n_digested;
	uint64_t n_digested_blks;
	struct inode *inode, *tmp;
	int reset_meta; // Do meta data reset. True if it is the first digestion
			// after rotation.

#ifdef BATCH_MEMCPY_LIST
	int sge_cnt;
	sscanf(ack_cmd, "|%s |%d|%lu|%lu|%u|%lu|%d|%d|", cmd_hdr, &libfs_id,
	       &fetch_seqn, &digest_start_blknr, &n_digested, &n_digested_blks,
	       &reset_meta, &sge_cnt);

	// mlfs_printf("DIGEST_RESP libfs_id=%d fetch_seqn=%lu "
	pr_digest("DIGEST_RESP libfs_id=%d fetch_seqn=%lu "
		  "digest_start_blknr=%lu n_digested=%u n_digested_blks=%lu "
		  "reset_meta=%d sge_cnt=%d",
		  libfs_id, fetch_seqn, digest_start_blknr, n_digested,
		  n_digested_blks, reset_meta, sge_cnt);

#else
	sscanf(ack_cmd, "|%s |%d|%lu|%lu|%u|%lu|%d|", cmd_hdr, &libfs_id,
	       &fetch_seqn, &digest_start_blknr, &n_digested, &n_digested_blks,
	       &reset_meta);

	// mlfs_printf("DIGEST_RESP libfs_id=%d fetch_seqn=%lu "
	pr_digest("DIGEST_RESP libfs_id=%d fetch_seqn=%lu "
		  "digest_start_blknr=%lu n_digested=%u n_digested_blks=%lu "
		  "reset_meta=%d",
		  libfs_id, fetch_seqn, digest_start_blknr, n_digested,
		  n_digested_blks, reset_meta);
#endif

	if (reset_meta) {
		reset_publish_meta();
	}

	next_hdr_of_digested_hdr = digest_start_blknr + n_digested_blks;

	mlfs_debug("g_fs_log->start_blk %lx, next_hdr_of_digested_hdr %lx\n",
		   g_fs_log[log_id]->start_blk, next_hdr_of_digested_hdr);

	// change start_blk
	g_fs_log[log_id]->start_blk = next_hdr_of_digested_hdr;
	g_log_sb[log_id]->start_digest = next_hdr_of_digested_hdr;

	// adjust g_log_sb->n_digest properly
	atomic_fetch_sub(&g_log_sb[log_id]->n_digest, n_digested);
	atomic_fetch_sub(&g_log_sb[log_id]->n_digest_blks, n_digested_blks);

	pr_digest("META UPDATED: g_fs_log[]->start_blk=%lu "
		  "g_log_sb[]->start_digest=%lu g_log_sb[]->n_digest=%u "
		  "g_log_sb[]->n_digest_blks=%u",
		  g_fs_log[log_id]->start_blk, g_log_sb[log_id]->start_digest,
		  atomic_load(&g_log_sb[log_id]->n_digest),
		  atomic_load(&g_log_sb[log_id]->n_digest_blks));

	// Start cleanup process after digest is done.

	// cleanup_lru_list(lru_updated);

	// FIXME: uncomment this line!
	// update our inode cache
	sync_all_inode_ext_trees();

	// persist log superblock.
	write_log_superblock(log_id, (struct log_superblock *)g_log_sb[log_id]);

	// Update fetch seqn. NOTE recently_acked_seqn does not mean all the
	// preceding fetch requests are completed. Anyway, this variable is used
	// only at shutdown_log() which doesn't bring severe consistency issue.
	// For further safety, we wait unfinished fetch requests with sleep()
	// before shutting FS down. (Ref.shutdown_log())
#ifdef BATCH_MEMCPY_LIST
	uint64_t last_seqn = fetch_seqn + sge_cnt - 1;
	if (last_seqn > atomic_load(&get_next_peer()->recently_acked_seqn)) {
		atomic_store(&get_next_peer()->recently_acked_seqn, last_seqn);
	}
#else
	if (fetch_seqn > atomic_load(&get_next_peer()->recently_acked_seqn)) {
		atomic_store(&get_next_peer()->recently_acked_seqn, fetch_seqn);
	}
#endif

	pr_digest("%s", "-----------------------------------");
#if 0

	// this is a response to a remotely triggered digestion
	if(log_id > 0) {
		rpc_remote_digest_response(pending_sock_fd, g_log_sb[log_id]->start_digest, n_digested, rotated, 0);
#if MLFS_LEASE
		notify_replication_compl();
#endif
	}
#endif

	if (enable_perf_stats)
		show_libfs_stats();
}
#else

void handle_digest_response(char *ack_cmd)
{
	char ack[30] = {0};
	addr_t next_hdr_of_digested_hdr;
	int dev, log_id, libfs_id, n_digested, rotated, lru_updated, n_digested_blks;
	struct inode *inode, *tmp;

	log_id = 0;

	sscanf(ack_cmd, "|%s |%d|%d|%d|%lu|%d|%d|%u|", ack, &libfs_id, &dev, &n_digested,
			&next_hdr_of_digested_hdr, &rotated, &lru_updated, &n_digested_blks);

	if (g_fs_log[log_id]->n_digest_req == n_digested)  {
		pr_digest("%s", "digest is done correctly");
	} else {
		mlfs_printf("digest is done insufficiently: req %u | done %u\n",
				g_fs_log[log_id]->n_digest_req, n_digested);
		panic("Digest was incorrect!\n");
	}

	mlfs_debug("g_fs_log->start_blk %lx, next_hdr_of_digested_hdr %lx\n",
			g_fs_log[log_id]->start_blk, next_hdr_of_digested_hdr);

	if (rotated) {
		atomic_add(&g_fs_log[log_id]->start_version, 1);
		pr_digest("-- log head is rotated: new start %lu new end %lu start_version %u avail_version %u",
				g_fs_log[log_id]->next_avail_header, atomic_load(&g_log_sb[log_id]->end),
				g_fs_log[log_id]->start_version, g_fs_log[log_id]->avail_version);
		if(g_fs_log[log_id]->start_version == g_fs_log[log_id]->avail_version)
			atomic_store(&g_log_sb[log_id]->end, 0);
	}

	// change start_blk
	g_fs_log[log_id]->start_blk = next_hdr_of_digested_hdr;
	g_log_sb[log_id]->start_digest = next_hdr_of_digested_hdr;

	// adjust g_log_sb->n_digest properly
	atomic_fetch_sub(&g_log_sb[log_id]->n_digest, n_digested);
	atomic_fetch_sub(&g_log_sb[log_id]->n_digest_blks, n_digested_blks);

	//Start cleanup process after digest is done.

	//cleanup_lru_list(lru_updated);

	//FIXME: uncomment this line!
	//update our inode cache
	sync_all_inode_ext_trees();

	// persist log superblock.
	write_log_superblock(log_id, (struct log_superblock *)g_log_sb[log_id]);

	pr_digest("clear digesting state for log segment %d\n", log_id);
	//xchg_8(&g_fs_log->digesting, 0);
	clear_digesting(log_id);

	pr_digest("%s", "-----------------------------------");
#if 0

	// this is a response to a remotely triggered digestion
	if(log_id > 0) {
		rpc_remote_digest_response(pending_sock_fd, g_log_sb[log_id]->start_digest, n_digested, rotated, 0);
#if MLFS_LEASE
		notify_replication_compl();
#endif
	}
#endif

	if (enable_perf_stats)
		show_libfs_stats();
}
#endif
