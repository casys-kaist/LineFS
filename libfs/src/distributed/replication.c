#include <time.h>

#include "distributed/rpc_interface.h"
#include "replication.h"
#include "storage/storage.h" // for nic_slab

#ifdef KERNFS

#ifdef __x86_64__
#include "fs.h"
#elif __aarch64__
#include "nic/nic_fs.h"
#include "nic/host_memcpy.h"
#include "nic/build_memcpy_list.h"
#include "nic/copy_log.h"
#else
#error "Unsupported architecture."
#endif

#include "ds/asm-generic/barrier.h"

#ifdef HYPERLOOP
#include "hyperloop/hyperloop_shm.h"
#endif

#else
#include "filesystem/fs.h"
#include "global/util.h"
#ifdef HYPERLOOP
#include "hyperloop/hyperloop_client.h"
#endif
#endif

#ifdef DISTRIBUTED

// int g_rsync_chunk = 256;	//default: 1 MB replication chunk
int g_rsync_chunk = 4*256;	// 4 MB replication chunk
// int g_rsync_chunk = 64*256;	// 64 MB replication chunk
int g_enable_rpersist = 0;	//default: persistence disabled
int g_rsync_rf = g_n_replica;	//default: replicate to all nodes

int log_idx = 0;

#ifdef KERNFS
struct replication_context *g_sync_ctx[MAX_LIBFS_PROCESSES + g_n_nodes];
#else
//TODO: change size to accommodate multiple logs
#ifdef NIC_OFFLOAD
// Replcation requires both Host kernfs and NIC kernfs peers.
struct replication_context *g_sync_ctx[g_n_nodes];
#else
struct replication_context *g_sync_ctx[g_n_replica];
#endif /* NIC_OFFLOAD */
#endif /* KERNFS */

static int initialized = 0;
#if defined(HYPERLOOP) && defined(KERNFS)
static peer_meta_t *hyperloop_server_shm;
#endif

__thread sync_meta_t *session;

threadpool thread_pool;
threadpool thread_pool_log_prefetch_req; // Used by libfs.
threadpool thread_pool_copy_log_to_local_host; // Used by kernfs on the second
                                               // replica.

static void replication_worker(void *arg);
void print_replay_list(struct replay_list *replay_list);    // For debugging.

#if defined(HYPERLOOP) && defined(KERNFS)
static void alloc_shm_to_g_sync_ctx_peer(void) {
    int i;
    size_t size; // alloc size.

    size = (MAX_LIBFS_PROCESSES + g_n_nodes) * sizeof(peer_meta_t);
    hyperloop_server_shm =
        (peer_meta_t*)create_hyperloop_server_shm(size);

    for (i = 0; i < (MAX_LIBFS_PROCESSES + g_n_nodes); i++) {
	g_sync_ctx[i] = (struct replication_context *)mlfs_zalloc(
		sizeof(struct replication_context));
	g_sync_ctx[i]->peer = &hyperloop_server_shm[i];
	mlfs_printf("g_sync_ctx[%d]->peer=%p\n", i, g_sync_ctx[i]->peer);
    }
    mlfs_printf("shm_addr=%p nvm_size=%lu\n", hyperloop_server_shm, size);
}
#endif

void init_replication(int remote_log_id, struct peer_id *peer, addr_t begin,
                      addr_t size, addr_t addr, atomic_ulong *end,
                      int next_rep_data_sockfd[], int next_rep_msg_sockfd[],
		      int next_digest_sockfd, int next_loghdr_sockfd) {
	int ret, idx;
	const char *env;
	pthread_mutexattr_t attr;

#ifdef KERNFS
	idx = remote_log_id;
#else
	idx = 0;
#endif

	// init thread pools.
	if (!initialized) {
		if (mlfs_conf.async_replication) {
			thread_pool = thpool_init(N_RSYNC_THREADS, "rsync");
		}
#ifdef LIBFS
		if (mlfs_conf.log_prefetching) {
			thread_pool_log_prefetch_req = thpool_init(
				mlfs_conf.thread_num_log_prefetch_req, "lpre_"
								       "req");
		}
#else // KERNFS
#ifdef NIC_SIDE
		thread_pool_copy_log_to_local_host =
			thpool_init(mlfs_conf.thread_num_copy_log_to_local_nvm,
				    "local_cpy");
#endif
#endif
	}

#if defined(HYPERLOOP) && defined(KERNFS)
	// Metadata should be shared with Hyperloop Server.
	// Allocate shared memory to all g_sync_ctx entries.
	if (!initialized) {
	    alloc_shm_to_g_sync_ctx_peer();
	}
#else
	g_sync_ctx[idx] = (struct replication_context *)mlfs_zalloc(sizeof(struct replication_context));
	g_sync_ctx[idx]->peer = (peer_meta_t *)mlfs_zalloc(sizeof(peer_meta_t));
#endif
	g_sync_ctx[idx]->begin = begin;
	g_sync_ctx[idx]->size = size;
	g_sync_ctx[idx]->end = end;
	g_sync_ctx[idx]->base_addr = addr;
        g_sync_ctx[idx]->next_rep_msg_sockfd[0] = next_rep_msg_sockfd[0];
        g_sync_ctx[idx]->next_rep_msg_sockfd[1] = next_rep_msg_sockfd[1];
        g_sync_ctx[idx]->next_rep_data_sockfd[0] = next_rep_data_sockfd[0];
        g_sync_ctx[idx]->next_rep_data_sockfd[1] = next_rep_data_sockfd[1];
        g_sync_ctx[idx]->next_digest_sockfd = next_digest_sockfd;
#ifdef NIC_SIDE
        g_sync_ctx[idx]->next_loghdr_sockfd = next_loghdr_sockfd;
#else
        g_sync_ctx[idx]->next_loghdr_sockfd = -1; // Not used in host kernfs and libfs.
#endif

#ifdef BATCH_MEMCPY_LIST
	g_sync_ctx[idx]->mcpy_list_batch_meta.r_meta = alloc_rdma_meta();
#endif

	g_sync_ctx[idx]->peer->id = idx;
	g_sync_ctx[idx]->peer->info = peer;
	g_sync_ctx[idx]->peer->local_start = begin;
	g_sync_ctx[idx]->peer->remote_start = begin;
	g_sync_ctx[idx]->peer->start_digest = g_sync_ctx[idx]->peer->remote_start;

        pr_rep("initializing replication module (begin: %lu | end: %lu | log "
               "size: %lu | base_addr: %lx)",
               g_sync_ctx[idx]->begin, atomic_load(g_sync_ctx[idx]->end),
               g_sync_ctx[idx]->size, g_sync_ctx[idx]->base_addr);

        atomic_init(&g_sync_ctx[idx]->peer->cond_check, begin);
	atomic_init(&g_sync_ctx[idx]->peer->n_digest, 0); //we assume that replica's log is empty
	atomic_init(&g_sync_ctx[idx]->peer->n_pending, 0);
	// atomic_init(&g_sync_ctx[idx]->peer->n_used, 0);
	// atomic_init(&g_sync_ctx[idx]->peer->n_used_blk, 0);
	atomic_init(&g_sync_ctx[idx]->peer->n_unsync, 0);
	atomic_init(&g_sync_ctx[idx]->peer->n_unsync_blk, 0);
	atomic_init(&g_sync_ctx[idx]->peer->n_unsync_upto_end, 0);

	atomic_init(&g_sync_ctx[idx]->peer->recently_issued_seqn, 0);
	atomic_init(&g_sync_ctx[idx]->peer->recently_acked_seqn, 0);

	// locks used for updating rsync metadata
	g_sync_ctx[idx]->peer->shared_rsync_addr_lock = (pthread_mutex_t *)mlfs_zalloc(sizeof(pthread_mutex_t));
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(g_sync_ctx[idx]->peer->shared_rsync_addr_lock, &attr);

	g_sync_ctx[idx]->peer->n_unsync_blk_handle_lock = (pthread_mutex_t *)mlfs_zalloc(sizeof(pthread_mutex_t));
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(g_sync_ctx[idx]->peer->n_unsync_blk_handle_lock, &attr);


	g_sync_ctx[idx]->peer->shared_rsync_n_lock = (pthread_mutex_t *)mlfs_zalloc(sizeof(pthread_mutex_t));
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(g_sync_ctx[idx]->peer->shared_rsync_n_lock, &attr);

	g_sync_ctx[idx]->peer->shared_rsync_order = (pthread_mutex_t *)mlfs_zalloc(sizeof(pthread_mutex_t));
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(g_sync_ctx[idx]->peer->shared_rsync_order, &attr);

	g_sync_ctx[idx]->peer->shared_rsync_cond_wait = (pthread_cond_t *)mlfs_zalloc(sizeof(pthread_cond_t));

	if(peer)
	    g_sync_ctx[idx]->peer->base_addr = mr_remote_addr(g_sync_ctx[idx]->peer->info->sockfd[SOCK_IO], MR_NVM_LOG);
	//	+ idx * (g_log_size << g_block_size_shift);
	//g_sync_ctx->peer->base_addr = mr_remote_addr(g_sync_ctx->peer->info->sockfd[SOCK_IO], MR_NVM_LOG);

#ifdef NIC_SIDE
	// Build memcpy list related.
	g_sync_ctx[idx]->thpool_build_memcpy_list = init_build_memcpy_list_thpool(idx);

	// Host memcpy related.
	atomic_init(&g_sync_ctx[idx]->next_host_memcpy_seqn, 1);
	g_sync_ctx[idx]->thpool_host_memcpy_req = init_host_memcpy_thpool(idx);

	// Host memcpy request circular buffer.
	g_sync_ctx[idx]->host_memcpy_req_buf.buf =
		(host_memcpy_arg *)mlfs_zalloc(sizeof(host_memcpy_arg) *
					       CIRC_BUF_HOST_MEMCPY_REQ_SIZE);
	pthread_mutex_init(&g_sync_ctx[idx]->host_memcpy_req_buf.produce_mutex, PTHREAD_PROCESS_PRIVATE);
	pthread_mutex_init(&g_sync_ctx[idx]->host_memcpy_req_buf.consume_mutex, PTHREAD_PROCESS_PRIVATE);

	// Copy done circular buffer.
	atomic_init(&g_sync_ctx[idx]->next_copy_done_seqn, 1);
	g_sync_ctx[idx]->local_copy_done_buf.buf = (copy_done_arg *)mlfs_zalloc(
		sizeof(copy_done_arg) * CIRC_BUF_COPY_DONE_SIZE);
	pthread_mutex_init(&g_sync_ctx[idx]->local_copy_done_buf.produce_mutex, PTHREAD_PROCESS_PRIVATE);
	pthread_mutex_init(&g_sync_ctx[idx]->local_copy_done_buf.consume_mutex, PTHREAD_PROCESS_PRIVATE);

	g_sync_ctx[idx]->thpool_manage_fsync_ack = init_manage_fsync_ack_thpool(idx);
	bitmap_clear(g_sync_ctx[idx]->log_persisted_bitmap, 0, COPY_NEXT_BITMAP_SIZE_IN_BIT);
	g_sync_ctx[idx]->log_persisted_bitmap_start_seqn = 1; // seqn starts from 1
	pthread_spin_init(&g_sync_ctx[idx]->log_persisted_bitmap_lock, PTHREAD_PROCESS_PRIVATE);

	// Alloc host_rctx.
	struct replication_context *host_rctx;
	host_rctx = (struct replication_context *)mlfs_zalloc(sizeof(struct replication_context));
	host_rctx->peer = (peer_meta_t *)mlfs_zalloc(sizeof(peer_meta_t));
	host_rctx->begin = begin;
	host_rctx->size = size;
	host_rctx->end = end;
	host_rctx->base_addr = addr;	// host NVM map_base_addr is passed.
        host_rctx->peer->id =
            nic_kid_to_host_kid(g_kernfs_id); // Local host kernfs id.
        host_rctx->peer->info =
            g_kernfs_peers[host_rctx->peer->id]; // It should be local host
                                                 // kernfs.
        host_rctx->next_rep_msg_sockfd[0] = -1;	    // not used.
        host_rctx->next_rep_msg_sockfd[1] = -1;	    // not used.
        host_rctx->next_rep_data_sockfd[0] = next_rep_data_sockfd[0];
        host_rctx->next_rep_data_sockfd[1] = -1;    // not used.
        host_rctx->next_digest_sockfd = -1;	// not used.
	host_rctx->next_loghdr_sockfd = -1;	// not used.
        host_rctx->peer->local_start = begin;
	host_rctx->peer->remote_start = begin;
	host_rctx->peer->start_digest = begin;
	host_rctx->host_rctx = NULL;

        atomic_init(&host_rctx->peer->cond_check, begin);
	atomic_init(&host_rctx->peer->n_digest, 0); //we assume that replica's log is empty
	atomic_init(&host_rctx->peer->n_pending, 0);
	// atomic_init(&host_rctx->peer->n_used, 0);
	// atomic_init(&host_rctx->peer->n_used_blk, 0);
	atomic_init(&host_rctx->peer->n_unsync, 0);
	atomic_init(&host_rctx->peer->n_unsync_blk, 0);
	atomic_init(&host_rctx->peer->n_unsync_upto_end, 0);

	// host_rctx locks used for updating rsync metadata
	host_rctx->peer->shared_rsync_addr_lock = (pthread_mutex_t *)mlfs_zalloc(sizeof(pthread_mutex_t));
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(host_rctx->peer->shared_rsync_addr_lock, &attr);

	host_rctx->peer->shared_rsync_n_lock = (pthread_mutex_t *)mlfs_zalloc(sizeof(pthread_mutex_t));
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(host_rctx->peer->shared_rsync_n_lock, &attr);

	host_rctx->peer->shared_rsync_order = (pthread_mutex_t *)mlfs_zalloc(sizeof(pthread_mutex_t));
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(host_rctx->peer->shared_rsync_order, &attr);

	host_rctx->peer->shared_rsync_cond_wait = (pthread_cond_t *)mlfs_zalloc(sizeof(pthread_cond_t));

	host_rctx->peer->base_addr = mr_remote_addr(host_rctx->peer->info->sockfd[SOCK_IO], MR_NVM_LOG);

	g_sync_ctx[idx]->host_rctx = host_rctx;
#endif

	env = getenv("MLFS_RF");

	if(env)
		g_rsync_rf = atoi(env);

	env = getenv("MLFS_CHUNK_SIZE");

	if(env)
		g_rsync_chunk = atoi(env);

	// env = getenv("MLFS_ENABLE_RPERSIST");

	g_enable_rpersist = mlfs_conf.persist_nvm;

	mlfs_assert(g_rsync_rf <= g_n_nodes);
	if (peer)
	    mlfs_assert(g_sync_ctx[idx]->peer->base_addr != 0);

        print_sync_ctx();

	pr_meta("%s", "<INITIAL STATE>");
	PR_METADATA(g_sync_ctx[idx]);

	if (!initialized)
		initialized = 1;
}

void destroy_replication(struct replication_context *rctx) {
#ifdef NIC_SIDE
	// thpool_destroy(rctx->thpool_build_memcpy_list);
	// thpool_destroy(rctx->thpool_host_memcpy_req);
	// thpool_destroy(rctx->thpool_manage_fsync_ack);
	// mlfs_free(rctx->host_memcpy_req_buf.buf);
	// mlfs_free(rctx->local_copy_done_buf.buf);

	// mlfs_free(rctx->host_rctx->peer->shared_rsync_addr_lock);
	// mlfs_free(rctx->host_rctx->peer->shared_rsync_n_lock);
	// mlfs_free(rctx->host_rctx->peer->shared_rsync_order);
	// mlfs_free(rctx->host_rctx->peer->shared_rsync_cond_wait);
	// mlfs_free(rctx->host_rctx->peer);
	// mlfs_free(rctx->host_rctx);
#endif
}

uint32_t addr_to_logblk(int id, addr_t addr)
{
	uint32_t logblk = ((addr - g_sync_ctx[id]->peer->base_addr) >> g_block_size_shift);
	return logblk;
}

uint32_t generate_rsync_metadata(int peer_id, uint16_t seqn, addr_t size, int solicit_ack)
{
    mlfs_assert(0); // do not use imm.
	uint16_t block_nr = ((ALIGN(size, g_block_size_bytes)) >> g_block_size_shift);
        mlfs_debug("generating rsync metadata: peer_id %d, seqn %u block_nr "
                "%u solicit_ack %u persist %u\n",
                peer_id, seqn, block_nr, solicit_ack, g_enable_rpersist);

        return (((uint32_t) seqn << 20) | ((uint32_t) block_nr << 8) | ((uint8_t) (g_rsync_rf - 1) << 6)
                | ((uint16_t) peer_id << 2) | ((uint8_t) solicit_ack << 1) | ((uint8_t) g_enable_rpersist));
}

uint32_t generate_rsync_metadata_from_rpcmsg(struct rpcmsg_replicate *rep)
{
    mlfs_assert(0); // do not use imm.
        int peer_id = rep->libfs_id;
        uint16_t seqn = rep->common.seqn;
        int solicit_ack = rep->ack;
	uint16_t block_nr = rep->n_log_blks;

        mlfs_debug("generating rsync metadata: peer_id %d, seqn %u block_nr "
                "%u solicit_ack %u persist %u\n",
                peer_id, seqn, block_nr, solicit_ack, g_enable_rpersist);

        return (((uint32_t) seqn << 20) | ((uint32_t) block_nr << 8) | ((uint8_t) (g_rsync_rf - 1) << 6)
                | ((uint16_t) peer_id << 2) | ((uint8_t) solicit_ack << 1) | ((uint8_t) g_enable_rpersist));
}

int decode_rsync_metadata(uint32_t meta, uint16_t *seq_n, addr_t *n_log_blk, int *steps, int *requester_id, int *sync, int *persist)
{
    mlfs_assert(0); // Do not use imm.
	if(seq_n)
		*seq_n = (uint16_t) ((meta & 0xFFF00000uL) >> 20);

	if(n_log_blk)
		*n_log_blk = (addr_t) ((meta & 0x000FFF00uL) >> 8);

	if(steps)
		*steps = (uint8_t) ((meta & 0x000000C0uL) >> 6);

	if(requester_id)
		*requester_id = (uint8_t) ((meta & 0x0000003CuL) >> 2);

	if(sync)
       		*sync = (int) ((meta & 2) >> 1);

	if(persist)
		*persist = (int) (meta & 1);

        mlfs_debug("decoding rsync metadata: peer_id %d seqn %d block_nr %lu solicit_ack %d persist %d\n",
                        requester_id    ?   *requester_id  : -1,
                        seq_n           ?   (int)*seq_n    : -1,
                        n_log_blk       ?   *n_log_blk     :  0,
                        sync            ?   *sync          : -1,
                        persist         ?   *persist       : -1);

	return 0;
}

// Update # of blocks to be replicated before starting replication.
void update_peer_sync_state(peer_meta_t *peer, uint64_t nr_log_blocks,
	uint32_t nr_unsync)
{
        uint64_t n_unsync_blk = 0;
	uint32_t n_unsync = 0;
	if(!peer)
		return;

	//FIXME: the order of increments matter: peer's n_unsync
	//should be incremented before the local n_digest; otherwise,
	//we might run into an issue where log transactions are
	//digested before being synced
	pthread_mutex_lock(peer->shared_rsync_n_lock);
#ifdef KERNFS
        mlfs_info(
            "add peer->n_unsync_blk: id=%d peer->n_unsync_blk=%lu by=%lu\n",
            peer->id, atomic_load(&peer->n_unsync_blk), nr_log_blocks);
#endif
	n_unsync_blk = atomic_fetch_add(&peer->n_unsync_blk, nr_log_blocks);
	n_unsync = atomic_fetch_add(&peer->n_unsync, nr_unsync);
        // n_unsync_blk = atomic_load(&peer->n_unsync_blk);
	pthread_mutex_unlock(peer->shared_rsync_n_lock);

        // mlfs_printf("Updating peer->n_unsync_blk from %lu to %lu "
        //             "peer->n_unsync from %u to %u\n",
        //             n_unsync_blk, n_unsync_blk + nr_log_blocks, n_unsync,
        //             n_unsync + nr_unsync);
}

void update_peer_digest_state(peer_meta_t *peer, addr_t start_digest, int n_digested, int rotated)
{
        addr_t start_digest_prev;
	if(!peer)
		return;

	//printf("%s\n", "received digestion notification from slave");
        pthread_mutex_lock(peer->shared_rsync_addr_lock);

	// rotations can sometimes be recorded by LibFS; detect them here
	if(peer->start_digest > start_digest)
		rotated = 1;

        start_digest_prev = peer->start_digest;
	peer->start_digest = start_digest;
	clear_peer_digesting(peer);
	//peer->digesting = 0;

#ifdef NIC_SIDE
	// n_digest is updated at make_digest_seg_request_sync in Libfs.
	if(g_n_replica > 1)
		atomic_fetch_sub(&peer->n_digest, n_digested);
#endif

	//FIXME: do not set to zero (this is incorrect); instead return and subtract n_digested_blks
	//atomic_store(&peer->n_used_blk, 0);

	if(rotated && (++peer->start_version == peer->avail_version)) {
		pr_digest("-- remote log tail is rotated: start %lu end %lu",
				peer->remote_start, 0L);
		peer->remote_end = 0;
	}

        pr_digest("update peer %d digest state: version %u "
                "peer->start_digest(block_nr) %lu start_digest_prev=%lu n_digest=%u",
                peer->id, peer->start_version, peer->start_digest,
                start_digest_prev, atomic_load(&peer->n_digest));
	PR_METADATA(g_sync_ctx[peer->id]);
        pthread_mutex_unlock(peer->shared_rsync_addr_lock);
}

void wait_on_peer_digesting(peer_meta_t *peer)
{
	mlfs_info("%s\n", "waiting till peer finishes digesting");
	uint64_t tsc_begin, tsc_end;

	if(!peer)
		return;

#ifdef LIBFS
	if (enable_perf_stats)
		tsc_begin = asm_rdtsc();
#endif

	while(peer->digesting)
		cpu_relax();

	mlfs_info("%s\n", "ending wait for peer digest");

#ifdef LIBFS
	if (enable_perf_stats) {
		tsc_end = asm_rdtsc();
		g_perf_stats.digest_wait_tsc += (tsc_end - tsc_begin);
		g_perf_stats.digest_wait_nr++;
	}
#endif
}

void wait_till_digest_checkpoint(peer_meta_t *peer, int version, addr_t block_nr)
{
	mlfs_printf("wait till digest current (v:%d,tail:%lu) required (v:%d,tail:%lu)\n",
			peer->start_version, peer->start_digest, version, block_nr);

#if 0
	while(peer->digesting || peer->start_version < version ||
	    peer->start_version == version && block_nr > peer->start_digest) {
		//mlfs_printf("[WAIT] digest current (v:%d,tail:%lu) required (v:%d,tail:%lu) digesting:%s\n",
		//	peer->start_version, peer->start_digest, version, block_nr, peer->digesting?"YES":"NO");
		cpu_relax();
	}
#else
	// skip if we no more data needs to be digested
	//if(peer->start_version > version || (peer->start_version == version && peer->start_digest >= block_nr))
	//	return;

	while(peer->digesting || peer->start_version < version ||
	    peer->start_version == version && block_nr >= peer->start_digest) {
		cpu_relax();
	}

	//while(peer->digesting)
	//	cpu_relax();

	// otherwise, wait for digestion to complete

	/*
	if(block_nr) {
	while(!peer->digesting)
		cpu_relax();

	while(peer->digesting)
		cpu_relax();
	}
	*/
#endif

}

void wait_on_peer_replicating(peer_meta_t *peer)
{

	if(!peer)
		return;

	mlfs_info("%s\n", "waiting till peer finishes replication");
	/*
	uint64_t tsc_begin, tsc_end;
	if (enable_perf_stats)
		tsc_begin = asm_rdtsc();
	*/

	while(peer->outstanding)
		cpu_relax();
	mlfs_info("%s\n", "ending wait for peer replication");

	/*
	if (enable_perf_stats) {
		tsc_end = asm_rdtsc();
		g_perf_stats.digest_wait_tsc += (tsc_end - tsc_begin);
		g_perf_stats.digest_wait_nr++;
	}
	*/
}

/** Get the number of nodes in the replicaion chain.
 *  It excludes the number of kernfs on SmartNIC.
 */
/*
int get_replica_num(void)
{
    return g_n_nodes - g_n_kernfs_nic;
    // For now, we added g_n_kernfs_nic global variable.
    // Otherwise, we can count the entry with .type == KERNFS_NIC_PEER in hot_replicas[].
}
*/

/**
 * Returns the next peer. If NIC_OFFLOAD is set, NIC kernfs is returned.
 * It should be called only in LibFS.
 */
peer_meta_t* get_next_peer()
{
#ifdef KERNFS
    mlfs_assert(false);  // This function is incorrect in NIC kernfs.
#endif
	if(g_n_replica > 1)
		return g_sync_ctx[0]->peer;
	else
		return NULL;
}

// TODO BOOKMARK implement async copy to replica2's NVM.
struct copy_to_local_host_arg
{
    int node_id;
    uintptr_t local_base_addr;
    uintptr_t host_base_addr;
    int inc_avail_ver;
};

// forward rsync to the next node in the chain
int mlfs_do_rsync_forward(struct rpcmsg_replicate *rep)
{
	struct list_head *rdma_entries;
	uint64_t tsc_begin;
	uint32_t n_loghdrs;
        int node_id;
	uintptr_t local_base_addr;
	uintptr_t peer_base_addr;
	struct replication_context *rctx;

        node_id = rep->libfs_id;
	rctx = g_sync_ctx[node_id];

	// print meta before replication.
	PR_METADATA(rctx);

	local_base_addr = rctx->base_addr;
	peer_base_addr = rctx->peer->base_addr;

        pr_rep("g_sync_ctx[libfs]->size=%lu local_base_addr=0x%lx "
               "peer_base_addr=0x%lx",
               rctx->size, local_base_addr, peer_base_addr);

        print_rpcmsg_replicate(rep, __func__);

        //generate rsync metadata
	START_TIMER(evt_rep_create_rsync);
        n_loghdrs = create_rsync(rctx, &rdma_entries, 0, 0,
                                 local_base_addr, peer_base_addr, 0, 0);
	END_TIMER(evt_rep_create_rsync);

        //generate mock rsync metadata (useful for debugging)-----
	//ret = create_rsync_mock(&rdma_entries, g_fs_log->start_blk);
	//--------------------------------------------------------

	//no data to rsync
	if(!n_loghdrs) {
		mlfs_debug("%s\n", "Ignoring rsync call; slave node is up-to-date");
		return -1;
	}

#ifdef LIBFS
	if (enable_perf_stats) {
	        g_perf_stats.rsync_ops += 1;
		tsc_begin = asm_rdtscp();
	}
#endif

        rep->steps--;
        mlfs_assert(rep->steps >= 0);

	rpc_replicate_log_relay(rctx->peer, rdma_entries, rep);

	pr_rep("Update n_digest: n_digest(prev)=%u n_loghdrs=%u n_digest(now)=%u",
		atomic_load(&rctx->peer->n_digest), n_loghdrs,
		atomic_load(&rctx->peer->n_digest) + n_loghdrs);
	atomic_fetch_add(&rctx->peer->n_digest, n_loghdrs);
	atomic_fetch_sub(&rctx->peer->n_pending, n_loghdrs);

#ifdef LIBFS
	if (enable_perf_stats)
		g_perf_stats.rdma_write_time_tsc += (asm_rdtscp() - tsc_begin);
#endif
	//if (enable_perf_stats)
	//	show_libfs_stats();

	PR_METADATA(rctx);

	return 0;
}

/* replication_worker thread pool */
struct libfs_rep_th_arg
{
    peer_meta_t *peer;
    uint32_t n_blk;
};
static void replication_worker(void *arg)
{
    struct libfs_rep_th_arg *rep_arg;

    rep_arg = (struct libfs_rep_th_arg*)arg;

    make_replication_request_sync(
	    rep_arg->peer, g_sync_ctx[rep_arg->peer->id]->base_addr,
	    g_sync_ctx[rep_arg->peer->id]->peer->base_addr, rep_arg->n_blk, 0,
	    0, 0, 0, 0);

    mlfs_free(arg);
}

int make_replication_request_async(uint32_t n_blk)
{
	uint8_t n = 0;

	if(g_n_replica > 1 && atomic_load(&get_next_peer()->n_unsync_blk) > 0) {
#if 0
		while(n < N_RSYNC_THREADS && cmpxchg(&peer->outstanding, n, n+1) != n) {
			n = peer->outstanding;
		}

		if(n < N_RSYNC_THREADS) {
			mlfs_muffled("inc - remaining rsyncs: %d\n", n+1);
			thpool_add_work(thread_pool, replication_worker, (void *)peer);
			return 0;
		} else
			return -EBUSY;
#else
                struct libfs_rep_th_arg *arg;
                arg = (struct libfs_rep_th_arg*)mlfs_alloc(
                        sizeof(struct libfs_rep_th_arg));
                arg->peer = g_sync_ctx[0]->peer;
                arg->n_blk = n_blk;

                thpool_add_work(thread_pool, replication_worker, (void *)arg);
		return 0;
#endif
	}
	else
		return -EBUSY;

#if 0
	if (peer->outstanding < N_RSYNC_THREADS && atomic_load(&peer->n_unsync_blk) > 0) {
		peer->outstanding++;

	} else
		return -EBUSY;
#endif
}

/**
 * Send an rpc to the next node in a replication chain and wait for CQ.
 * Function for NIC-offloading.
 */
static void send_rpc_to_next_node_and_wait_cq(struct rpcmsg_replicate *rep,
	int next_rpc_sock)
{
    char msg[MAX_SOCK_BUF];

    rpcmsg_build_replicate(msg, rep);
    print_rpcmsg_replicate(rep, __func__);

    END_TIMER(evt_rep_critical_nic); // replica 1

    // Send sync message to guarantee all the previous RDMA entries are
    // complete. Note that all the RDMA entries are sent as ASYNC.
    rpc_forward_msg_with_per_libfs_seqn_and_wait_cq(next_rpc_sock, msg, rep->libfs_id);
}

#if defined(LIBFS) && defined(HYPERLOOP)
/**
 * Replicate log using hyperloop chain replication.
 */
static void replicate_log_by_hyperloop(peer_meta_t *peer,
                                       struct list_head *rdma_entries)
{
    struct rdma_meta_entry *rdma_entry, *rdma_tmp;

    list_for_each_entry_safe(rdma_entry, rdma_tmp, rdma_entries, head) {
	do_hyperloop_gwrite_data();
	do_hyperloop_gwrite_meta();
    }
}
#endif

/**
 *  seqn : Seqn generated by libfs on replication rpc. Only used in NIC-offloading.
 *  ack : If 1, send ack to libfs. Only used in NIC-offloading.
 */
int make_replication_request_sync(peer_meta_t *peer, uintptr_t local_base_addr,
	uintptr_t peer_base_addr, uint32_t n_blk, int inc_avail_ver,
	uint64_t seqn, int ack, uintptr_t is_meta_updated_p,
	uintptr_t is_ack_received)
{
	struct list_head *rdma_entries;
	uint64_t tsc_begin;
	uint32_t n_loghdrs;
	uint64_t n_unsync_blk;	// Used to pass metadata to libfs (nic-offloading)
	uint32_t n_unsync;  // Used to pass metadata to libfs (nic_offloading)

	// no need to replicate if we only have a single node
	if(g_n_replica == 1)
		return -EBUSY;

	// print meta before replication.
	PR_METADATA(g_sync_ctx[peer->id]);

	//generate rsync metadata
	START_TIMER(evt_rep_create_rsync);
	n_loghdrs = create_rsync(g_sync_ctx[peer->id], &rdma_entries, n_blk, 0,
		local_base_addr, peer_base_addr, &n_unsync_blk, &n_unsync);
	END_TIMER(evt_rep_create_rsync);

	//no data to rsync
	if(!n_loghdrs) {
		mlfs_debug("%s\n", "Ignoring rsync call; slave node is up-to-date");
		return -1;
	}

#ifdef LIBFS
	if (enable_perf_stats) {
	        g_perf_stats.rsync_ops += 1;
		tsc_begin = asm_rdtscp();
	}
#endif

#if defined(HYPERLOOP) && defined(LIBFS)
	replicate_log_by_hyperloop(peer, rdma_entries);
#else
	rpc_replicate_log_initiate(peer, rdma_entries);
#endif

	atomic_fetch_add(&g_sync_ctx[peer->id]->peer->n_digest, n_loghdrs);
	atomic_fetch_sub(&g_sync_ctx[peer->id]->peer->n_pending, n_loghdrs);

#ifdef LIBFS
	if (enable_perf_stats)
		g_perf_stats.rdma_write_time_tsc += (asm_rdtscp() - tsc_begin);
#endif

	//if (enable_perf_stats)
	//	show_libfs_stats();

	return 0;
}

struct prefetch_arg {
    addr_t prefetch_start;
    uint64_t n_prefetch_blk;
    uint32_t n_prefetch;
    int reset_meta;
    int is_fsync;
    uint64_t *fsync_ack_flag;
};

static inline uint64_t generate_fetch_seqn(struct fetch_seqn *fetch_seqn)
{
    uint64_t ret;
    pthread_spin_lock(&fetch_seqn->fetch_seqn_lock);
    ret = ++fetch_seqn->n;
    // printf("[SEQN] sockfd=%d fetch_seqn=%lu issued.\n", sock->fd, ret);
    pthread_spin_unlock(&fetch_seqn->fetch_seqn_lock);
    return ret;
}

/**
 * @Synopsis  Send a message of prefetch request. It is a thread worker.
 *
 * @Param arg thread argument.
 */
static void send_log_prefetch_msg(void *arg)
{
    struct prefetch_arg *pf_arg;
    int nic_kernfs_id, sockfd;
    uint64_t seqn;
    char msg[MAX_SOCK_BUF];

    pf_arg = (struct prefetch_arg*) arg;

    nic_kernfs_id = host_kid_to_nic_kid(g_kernfs_id);
    if (pf_arg->is_fsync) {
	    // low-latency channel.
	    sockfd = g_kernfs_peers[nic_kernfs_id]->sockfd[SOCK_IO];
    } else {
	    sockfd = g_kernfs_peers[nic_kernfs_id]->sockfd[SOCK_BG];
    }

    seqn = generate_fetch_seqn(&g_fetch_seqn);
    mlfs_assert(seqn > atomic_load(&get_next_peer()->recently_issued_seqn));
    atomic_store(&get_next_peer()->recently_issued_seqn, seqn);

    sprintf(msg,
	    "|" TO_STR(RPC_LOG_PREFETCH) " |%d|%lu|%lu|%u|%lu|%lu|%d|%d|%lu|",
	    g_self_id, seqn, pf_arg->prefetch_start, pf_arg->n_prefetch,
	    pf_arg->n_prefetch_blk, g_sync_ctx[0]->base_addr, pf_arg->reset_meta,
	    pf_arg->is_fsync, (uintptr_t)pf_arg->fsync_ack_flag);

//     if (pf_arg->fsync_ack_flag) {
// 	    struct fsync_acks *acks =
// 		    (struct fsync_acks *)pf_arg->fsync_ack_flag;
// 	    mlfs_printf("FSYNC request: seqn=%lu fsync_ack_flag=%p(%lx) "
// 			"replica1=%p(%hhu) replica2=%p(%hhu))\n",
// 			seqn, pf_arg->fsync_ack_flag, *pf_arg->fsync_ack_flag,
// 			&acks->replica1, acks->replica1, &acks->replica2,
// 			acks->replica2);
//     } else {
// 	    mlfs_printf("PREFETCH request: seqn=%lu\n", seqn);
//     }
//     fflush(stdout);
//     fflush(stderr);

    rpc_forward_msg_no_seqn(sockfd, msg);
    // rpc_forward_msg_no_seqn_sync(sockfd, msg);

    // Print progress roughly.
//     if (!(seqn % 500)) {
// 	    printf("Prefetch requested. seqn=%lu\n", seqn);
//     }

    mlfs_free(arg);
}

/**
 * @Synopsis  Waits for two flags : set by NIC of replica 1 and set by host of
 * replica 2.
 *
 * @Param fsync_ack_flag
 */
static void await_fsync_acks(uint64_t *fsync_ack_flag)
{
	struct fsync_acks *acks = (struct fsync_acks*)fsync_ack_flag;

	pr_lpref("Waiting fsync ack. fsync_ack_flag=%p(%lx) replica1=%p(%hhu) "
		 "replica2=%p(%hhu))",
		 fsync_ack_flag, *fsync_ack_flag, &acks->replica1,
		 acks->replica1, &acks->replica2, acks->replica2);
	// fflush(stdout);
	// fflush(stderr);
	START_TIMER(evt_wait_fsync_ack);

	while (1) {
		if (mlfs_conf.persist_nvm_with_clflush) {
			if (acks->replica1 && acks->replica2)
				break;
		}
		if (mlfs_conf.persist_nvm_with_rdma_read) {
			if (acks->replica1)
				break;
		}
		cpu_relax();
	}

	END_TIMER(evt_wait_fsync_ack);
	pr_lpref("fsync ack received. fsync_ack_flag=%p(%lx)", fsync_ack_flag,
		 *fsync_ack_flag);

	nic_slab_free(fsync_ack_flag);
}

static void reset_prefetch_meta(void) {
	atomic_store(&get_next_peer()->n_unsync_upto_end, 0);
	get_next_peer()->local_start = g_sync_ctx[0]->begin;

	pr_rep("Prefetch meta rotated. New values: "
	       "local_start=%lu n_unsync_upto_end=%u",
	       get_next_peer()->local_start,
	       atomic_load(&get_next_peer()->n_unsync_upto_end));
}

/**
 * @Synopsis  Calculate and update prefetch meta and send prefetch request.
 *
 * @Param prefetch_start_blknr
 * @Param n_unsync_blks The number of blocks can be prefetched.
 * @Param n_unsync The number of log headers can be prefetched.
 * @Param n_unsync_upto_end The number of log headers until the end of log.
 * @Param end_blknr If there is a rotation, the last block number is used.
 * @Param is_fsync
 * Otherwise, it is 0.
 */
int calculate_prefetch_meta(addr_t fetch_start_blknr,
			     uint64_t n_unsync_blks, uint32_t n_unsync,
			     uint32_t n_unsync_upto_end, addr_t end_blknr,
			     int is_fsync)
{
	int rotated = 0, reset_meta = 0, ret = 0;
	uint64_t n_prefetch_blk, n_unsync_blk_upto_end = 0;
	uint32_t n_prefetch;
	uint64_t *fsync_ack_flag = NULL;
	addr_t prefetch_start_blknr = fetch_start_blknr;

	reset_meta = (end_blknr && prefetch_start_blknr > end_blknr);

	if (reset_meta) { // log group starts from the beginning.
		// reset meta.
		reset_prefetch_meta();

		// reset local variables.
		prefetch_start_blknr = g_sync_ctx[0]->begin;
		n_unsync_upto_end = 0;
		n_unsync_blk_upto_end = 0;

		n_prefetch_blk = n_unsync_blks;
		n_prefetch = n_unsync;
	}

	if (n_unsync_upto_end) { // last log group before rotation.
		mlfs_assert(end_blknr);
		mlfs_assert(prefetch_start_blknr < end_blknr);

		n_unsync_blk_upto_end =
			end_blknr - prefetch_start_blknr + 1;

		// Do prefetch upto end_blknr.
		n_prefetch_blk = n_unsync_blk_upto_end;
		n_prefetch = n_unsync_upto_end;

	} else {
		n_prefetch_blk = n_unsync_blks;
		n_prefetch = n_unsync;
	}

	// Update meta: Forward local_start by n_prefetch_blk.
	get_next_peer()->local_start += n_prefetch_blk;

	// Update meta: n_unsync and n_unsync_blk
	atomic_fetch_sub(&get_next_peer()->n_unsync_blk, n_prefetch_blk);
	atomic_fetch_sub(&get_next_peer()->n_unsync, n_prefetch);

	pr_lpref("[PREFETCH] META_UPDATED: local_start=%lu n_unsync_upto_end=%u "
		    "n_unsync_blk=%lu n_unsync=%u",
		    get_next_peer()->local_start,
		    atomic_load(&get_next_peer()->n_unsync_upto_end),
		    atomic_load(&get_next_peer()->n_unsync_blk),
		    atomic_load(&get_next_peer()->n_unsync));

	pthread_mutex_unlock(get_next_peer()->shared_rsync_addr_lock);
	pthread_mutex_unlock(get_next_peer()->shared_rsync_n_lock);

	pr_lpref("[PREFETCH] Caculate meta. prefetch_start_blknr=%lu "
		 "n_unsync=%u n_unsync_blk=%lu n_unsync_upto_end=%u "
		 "n_unsync_blk_upto_end=%lu end_blknr=%lu n_prefetch=%u "
		 "n_prefetch_blk=%lu reset_meta=%d",
		 prefetch_start_blknr, n_unsync, n_unsync_blks,
		 n_unsync_upto_end, n_unsync_blk_upto_end, end_blknr,
		 n_prefetch, n_prefetch_blk, reset_meta);

	if (is_fsync && n_unsync == n_prefetch) {
		fsync_ack_flag = rpc_alloc_ack_bit();
	}

	send_log_prefetch_request(prefetch_start_blknr, n_prefetch_blk,
				  n_prefetch, reset_meta, is_fsync,
				  fsync_ack_flag);

	if (is_fsync) {
		if (n_prefetch < n_unsync) {
			// Call this function again to prefetch remaining data
			// until when fsync is called. (on rotation)
			return -EAGAIN;
		} else {
			// All data before fsync is requested.
			await_fsync_acks(fsync_ack_flag);
		}
	}

	return 0;
}

static void limit_prefetch_rate(void)
{
#ifdef PIPELINE_RATE_LIMIT
	uint64_t issued_recent;
	uint64_t acked_recent;
	struct timespec start_time, end_time;
	double duration = 0.0;
	int seconds = 0;

	issued_recent = atomic_load(&get_next_peer()->recently_issued_seqn);
	acked_recent = atomic_load(&get_next_peer()->recently_acked_seqn);

	// if (*libfs_rate_limit_flag) {
	//         mlfs_printf("Prefetch limited: issued=%lu acked=%lu "
	//                     "libfs_rate_limit_flag=%lu\n",
	//                     issued_recent, acked_recent, *libfs_rate_limit_flag);
	// }

	clock_gettime(CLOCK_MONOTONIC, &start_time);
	while (*libfs_rate_limit_flag) {
		// TODO to be deleted. It should be 0 or 1.
		mlfs_assert(*libfs_rate_limit_flag == 0 || *libfs_rate_limit_flag == 1);

		clock_gettime(CLOCK_MONOTONIC, &end_time);
		// Print every 1 sec.
		duration = get_duration(&start_time, &end_time);
		if (duration > 1.0) {
			mlfs_printf("libfs_id=%d Prefetch limited for %d "
				    "seconds. rate_limit_flag=%lu\n",
				    g_self_id, ++seconds,
				    *libfs_rate_limit_flag);
			// reset start time.
			clock_gettime(CLOCK_MONOTONIC, &start_time);
		}
	}
#endif

#ifdef LIMIT_PREFETCH

#define LIMIT_PREFETCH_THRESHOLD 60UL // hit almost max throughput with 1.
// #define LIMIT_PREFETCH_THRESHOLD 3UL // hit almost max throughput with 1.
// #define LIMIT_PREFETCH_THRESHOLD 1UL

	clock_gettime(CLOCK_MONOTONIC, &start_time);
	while (issued_recent - acked_recent >= LIMIT_PREFETCH_THRESHOLD) {
		clock_gettime(CLOCK_MONOTONIC, &end_time);
		// Print every 1 sec.
		if (end_time.tv_sec - start_time.tv_sec > 1) {
			mlfs_printf("Prefetch limited: issued=%lu acked=%lu\n",
				    issued_recent, acked_recent);
			// reset start time.
			clock_gettime(CLOCK_MONOTONIC, &start_time);
		}
		acked_recent =
			atomic_load(&get_next_peer()->recently_acked_seqn);
	}
#endif
}

static void send_log_prefetch_request(addr_t prefetch_start_blknr,
				      uint64_t n_prefetch_blk,
				      uint32_t n_prefetch, int reset_meta,
				      int is_fsync, uint64_t *fsync_ack_flag)
{
	struct prefetch_arg *pf_arg;

#ifdef PIPELINE_RATE_LIMIT
	limit_prefetch_rate();
#endif
	pf_arg = (struct prefetch_arg *)mlfs_alloc(sizeof(struct prefetch_arg));
	pf_arg->prefetch_start = prefetch_start_blknr;
	pf_arg->n_prefetch_blk = n_prefetch_blk;
	pf_arg->n_prefetch = n_prefetch;
	pf_arg->reset_meta = reset_meta;
	pf_arg->is_fsync = is_fsync;
	pf_arg->fsync_ack_flag = fsync_ack_flag;

	if (is_fsync) {
		// Send in the same thread.
		send_log_prefetch_msg((void *)pf_arg);
	} else {
		// Send in another thread.
		thpool_add_work(thread_pool_log_prefetch_req,
				send_log_prefetch_msg, (void *)pf_arg);
	}
}

///////////////////////// Functions for COALESCING ////////////////////////////
/** Structure for checking loghdr duplication. */
struct loghdr_dedup_key {
    addr_t loghdr_nr;
};

struct loghdr_dedup {
    struct loghdr_dedup_key key;
    int val;
    UT_hash_handle hh;
};

// Add a new entry to the loghdr_dedup hash table.
void add_loghdr_dedup_entry (struct loghdr_dedup** hash_table, addr_t loghdr_nr, int val) {
    struct loghdr_dedup *p;

    p = (struct loghdr_dedup *)mlfs_zalloc(sizeof *p);
    p->key.loghdr_nr = loghdr_nr;
    p->val = val;
    HASH_ADD(hh, *hash_table, key, sizeof(struct loghdr_dedup_key), p);
}

// Return 1 if entry exists.
int lookup_loghdr_dedup_entry (struct loghdr_dedup** hash_table, addr_t loghdr_nr) {
    struct loghdr_dedup e, *p;

    memset(&e, 0, sizeof(struct loghdr_dedup));
    e.key.loghdr_nr = loghdr_nr;
    HASH_FIND(hh, *hash_table, &e.key, sizeof(struct loghdr_dedup_key), p);

    if (p){
        pr_rcoal("p=%p lookup: key=%lu val=%d", p, p->key.loghdr_nr, p->val);
        return 1;   // exists.
    } else{
        pr_rcoal("lookup: key=%lu not exists.", e.key.loghdr_nr);
        return 0;   // not exists.
    }
}

// Free all hash entries.
void free_loghdr_dedup_entires (struct loghdr_dedup** hash_table) {
    struct loghdr_dedup *p, *tmp;
    HASH_ITER(hh, *hash_table, p, tmp) {
        HASH_DEL(*hash_table, p);
        free(p);
    }
}

/*
 * replay_list might have duplicated entries after coalescing. The order of
 * rsync messages doesn't matter in replication, whereas it matters in digestion.
 * It is because, being different from digestion, LibFS just copies loghdrs and
 * logs to remote node. So, we can eliminate duplicated entries from reply_list.
 * Invariant: In coalescing, a loghdr of an entry is replaced to a recent one.
 * It means,
 *      loghdr # (old) < loghdr # (recent)
 */
void dedup_replay_list(struct replay_list *replay_list, addr_t start_blk, addr_t last_loghdr_blknr) {
    // Iterating from the end of the replay_list, add to hash table and erase
    // duplicated entries.

    struct list_head *l, *tmp, *last;
    uint8_t *node_type;
    struct loghdr_dedup *loghdr_hash = NULL;    // hash table to check duplication.
    struct loghdr_dedup *ld;

    pr_rcoal("start_blk=%lu", start_blk);
    pr_rcoal("last_loghdr_blknr=%lu", last_loghdr_blknr);

    //print_replay_list(replay_list);

    pr_rcoal("%s", "************ replay_list DEDUP START *****************");
    pr_rcoal("Start_blk=%lu last_loghdr_blk=%lu", start_blk, last_loghdr_blknr);

    int cnt=0;
    list_for_each_prev_safe(l, tmp, &replay_list->head) {
        cnt++;
        node_type = (uint8_t *)l + sizeof(struct list_head);
        switch(*node_type) {
            case NTYPE_I:
                {
                    i_replay_t *item;
                    item = (i_replay_t *)container_of(l, i_replay_t, list);

                    if (lookup_loghdr_dedup_entry(&loghdr_hash, item->blknr)) {
                        pr_rcoal("%d [INODE] Duplicated. remove it. "
                                "inum=%u (ver=%u) create=%d loghdr_blknu=%lu",
                                cnt,
                                item->key.inum, item->key.ver,
                                item->create, item->blknr);
                        HASH_DEL(replay_list->i_digest_hash, item);
                        list_del(l);
                        mlfs_free(item);

                    } else {
                        pr_rcoal("%d [INODE] Add to hash table. "
                                "inum=%u (ver=%u) create=%d loghdr_blknu=%lu",
                                cnt,
                                item->key.inum, item->key.ver,
                                item->create, item->blknr);
                        add_loghdr_dedup_entry(&loghdr_hash, item->blknr, cnt);
                    }
                    break;
                }

            case NTYPE_D:
                {
                    // deprecated.
                    break;
                }

            case NTYPE_F:
                {
                    // Replication requires NTYPE_V entries only. Just delete it.
                    f_replay_t *item;
                    item = (f_replay_t *)container_of(l, f_replay_t, list);

                    pr_rcoal("%d [FILE-NEW] inum %u (ver %u)",
                            cnt,
                            item->key.inum, item->key.ver);

                    f_iovec_t *f_iovec, *f_tmp;
                    HASH_DEL(replay_list->f_digest_hash, item);
                    list_for_each_entry_safe(f_iovec, f_tmp,
                            &item->iovec_list, iov_list) {
                        list_del(&f_iovec->iov_list);
                        //no need to free here; will call later on  // JYKIM where? NTYPE_V case?
                        //mlfs_free(f_iovec);
                    }
                    list_del(l);
                    mlfs_free(item);
                    break;
                }

            case NTYPE_U:
                {
                    u_replay_t *item;
                    item = (u_replay_t *)container_of(l, u_replay_t, list);
                    if (lookup_loghdr_dedup_entry(&loghdr_hash, item->blknr)) {
                        pr_rcoal("%d [ULINK] Duplicated. remove it. "
                                "inum=%u (ver=%u) loghdr_blknu=%lu",
                                cnt,
                                item->key.inum, item->key.ver, item->blknr);
                        HASH_DEL(replay_list->u_digest_hash, item);
                        list_del(l);
                        mlfs_free(item);
                    } else {
                        pr_rcoal("%d [ULINK] Add to hash table. "
                                "inum=%u (ver=%u) loghdr_blknu=%lu",
                                cnt,
                                item->key.inum, item->key.ver, item->blknr);
                        add_loghdr_dedup_entry(&loghdr_hash, item->blknr, cnt);
                    }
                    break;
                }

            case NTYPE_V:
                {
                    f_iovec_t *item;
                    item = (f_iovec_t *)container_of(l, f_iovec_t, list);
                    if (lookup_loghdr_dedup_entry(&loghdr_hash, item->blknr)) {
                        pr_rcoal("%d [FILE-DATA] Duplicated. remove it. "
                                "loghdr_blknr=%lu", cnt, item->blknr);
                        list_del(l);
                        mlfs_free(item);
                    } else {
                        pr_rcoal("%d [FILE-DATA] Add to hash table. loghdr_blknr=%lu",
                                cnt, item->blknr);
                        add_loghdr_dedup_entry(&loghdr_hash, item->blknr, cnt);
                    }
                    break;
                }

            default:
                {
                    printf("unsupported node type.\n");
                }
        }
    }

    // Free loghdr_dedup_hash.
    free_loghdr_dedup_entires(&loghdr_hash);
    pr_rcoal("%s", "************ replay_list DEDUP END *****************");
}
///////////////////////////////////////////////////////////////////////////////

uint32_t create_rsync(struct replication_context *rctx,
                      struct list_head **rdma_entries, uint32_t n_blk,
                      int force_digest, uintptr_t local_base_addr,
                      uintptr_t peer_base_addr, uint64_t *n_unsync_blk,
                      uint32_t *n_unsync) {
    uint64_t tsc_begin;
    int ret;
    peer_meta_t *peer;
    addr_t current_local_start, current_remote_start, cond_value;
    pid_t tid = syscall(SYS_gettid);

    peer = rctx->peer;

    if (enable_perf_stats)
	tsc_begin = asm_rdtscp();

    pthread_mutex_lock(peer->shared_rsync_addr_lock);
    // DEBUG
    // mlfs_printf("DEBUG RSYNC START peer->rsync_start=%lu "
    //             "peer->digest_start=%lu n_blk=%u\n",
    //             peer->local_start, peer->start_digest, n_blk);

    START_TIMER(evt_rep_start_session);
    // take a snapshot of sync parameters (necessary since rsyncs can be issued
    // concurrently)
    while ((ret = start_rsync_session(rctx, n_blk, peer_base_addr)) == -EBUSY) {
	// release lock to prevent race condition. Otherwise,
	// peer->digesting is not updated.
	pthread_mutex_unlock(peer->shared_rsync_addr_lock);
	end_rsync_session(peer);
	cpu_relax();
	pthread_mutex_lock(peer->shared_rsync_addr_lock);
    }
    pthread_mutex_unlock(peer->n_unsync_blk_handle_lock);

    END_TIMER(evt_rep_start_session);

    // nothing to rsync
    if (ret) {
	mlfs_info("nothing to rsync. n_unsync = %lu n_threshold = %u\n",
		session->n_unsync_blk, g_rsync_chunk);
	pthread_mutex_unlock(peer->shared_rsync_addr_lock);
	end_rsync_session(peer);
	return 0;
    }

    // we save the value of local_start before rsync to escape conditional mutex
    // (in case of coalescing)
    cond_value = session->local_start;

    // ensure correctness of local_start for rsync; sometimes, it'll be invalid if
    // the append at local_start resulted in a wrap around
    if (session->end && session->local_start > session->end) {
	reset_log_ptr(rctx, &session->local_start);
	reset_log_ptr(rctx, &peer->local_start);
#ifdef NIC_OFFLOAD
	// reset remote_start too.
	reset_log_ptr(rctx, &session->remote_start);
	reset_log_ptr(rctx, &peer->remote_start);
#endif
    }

	current_local_start = session->local_start;
	current_remote_start = session->remote_start;

	//update peer metadata - 3 globals (local_start, n_unsync_blk, n_unsync)
	//we know their values in advance
	move_log_ptr(rctx, &peer->local_start, 0, session->n_unsync_blk, local_base_addr);

#ifdef KERNFS
        mlfs_info(
            "sub peer->n_unsync_blk: id=%d peer->n_unsync_blk=%lu by=%lu\n",
            peer->id, atomic_load(&peer->n_unsync_blk),
            session->n_unsync_blk);
#endif
        atomic_fetch_sub(&peer->n_unsync_blk, session->n_unsync_blk);
	atomic_fetch_sub(&peer->n_unsync, session->n_unsync);

	mlfs_assert(atomic_load(&peer->n_unsync) >= 0);

	// Save metadata values.
	if (n_unsync_blk)
	    *n_unsync_blk = session->n_unsync_blk;
	if (n_unsync)
	    *n_unsync = session->n_unsync;

	// Do coalesce only on the first node. The next node does not need to
	// coalesce log because it has already been coalesced.
	if(mlfs_conf.log_coalesce &&
		host_kid_to_nic_kid(local_kernfs_id(peer->id)) == g_self_id) {
	    //--------------------[Coalescing]-------------------------
	    //---------------------------------------------------------

	    mlfs_assert(0);

	    pthread_mutex_unlock(peer->shared_rsync_addr_lock);

	    struct replay_list *replay_list = (struct replay_list *) mlfs_zalloc(
		    sizeof(struct replay_list));

	    addr_t last_loghdr_blknr = coalesce_log_and_produce_replay(replay_list, local_base_addr);

#ifdef LIBFS
	    if(enable_perf_stats) {
		g_perf_stats.coalescing_log_time_tsc += asm_rdtscp() - tsc_begin;
		tsc_begin = asm_rdtscp();
	    }
#endif

	    //------Ordering barrier------
	    //
	    //the follwoing conditional wait insures that threads will exit in the same order they have acquired
	    //the shared_rsync_addr_lock; this allows g_sync_ctx->peer metadata to be updated correctly

	    //Example on race condition:
	    //
	    //Starting state:
	    //local_start = remote_start = 1
	    //n_unsync = n_tosync = 4 (for both threads)
	    //
	    //Execution:
	    //Thread 1 - Updates (local_start) 1-->5
	    //Thread 2 - Updates (local_start) 5-->9
	    //Thread 2 - Coalesces & Updates (remote_start) 1-->5
	    //Thread 1 - Coalesces & Updates (remote_start) 5-->9
	    //
	    //Outcome:
	    //Thread 1 sends log segment [1,5) to be appended to remote node in region [5,9)
	    //Thread 2 sends log segment [5,9) to be appended to remote node in region [1,5)
	    //
	    //Local and remote logs now have different transaction ordering and are inconsistent

	    mlfs_debug("[%d] checking wait condition. cond_value: %lu, cond_check: %lu\n",
		    tid, cond_value, atomic_load(&peer->cond_check));
	    //TODO: is there a more efficient way to implement this?
	    pthread_mutex_lock(peer->shared_rsync_order);
	    while (cond_value != atomic_load(&peer->cond_check))
		pthread_cond_wait(peer->shared_rsync_cond_wait, peer->shared_rsync_order);

	    pthread_mutex_unlock(peer->shared_rsync_order);
	    //---------End barrier--------
	    //After escaping the barrier we use the remote_start value set by the previous thread
	    session->remote_start = peer->remote_start;

	    // Remove duplicated replay list.
	    dedup_replay_list(replay_list, session->local_start, last_loghdr_blknr);

	    generate_rsync_msgs_using_replay(rctx, replay_list, 1, local_base_addr);

	    //update the 4 remaining globals (only known after coalescing)
	    //atomic_fetch_add(&peer->n_digest, *n_digest_update);
	    // atomic_fetch_add(&peer->n_used, session->n_tosync);
	    // atomic_fetch_add(&peer->n_used_blk, session->n_tosync_blk);
	    peer->remote_start = session->remote_start;
	    if(session->remote_end) {
		peer->remote_end = session->remote_end;
		peer->avail_version++;
	    }

	    //updating cond_check will allow the thread immediately behind us to get across the 'Ordering barrier'
	    //note: we need to update the rest of the globals BEFORE we perform this update
	    pthread_mutex_lock(peer->shared_rsync_order);
	    mlfs_debug("[%d] updating cond_check to %lu\n", tid, session->local_start);
	    atomic_store(&peer->cond_check, session->local_start);
	    pthread_cond_broadcast(peer->shared_rsync_cond_wait);
	    pthread_mutex_unlock(peer->shared_rsync_order);

	} else {
	    //------------------[No coalescing]------------------------
	    //---------------------------------------------------------
	    generate_rsync_msgs(rctx, 0, 0, local_base_addr);

	    mlfs_assert(session->n_unsync_blk == session->n_tosync_blk);
	    mlfs_assert(session->local_start == session->remote_start);

            //printf("debug: remote_log_used %u remote_log_total %u to_send %u\n",
            //       atomic_load(&peer->n_used_blk),
            //       g_sync_ctx->size - 1, session->n_tosync_blk);

            //update the 4 remaining globals before unlocking
	    //atomic_fetch_add(&peer->n_digest, *n_digest_update);
	    // atomic_fetch_add(&peer->n_used, session->n_tosync);
	    // atomic_fetch_add(&peer->n_used_blk, session->n_tosync_blk);
	    peer->remote_start = session->remote_start;
	    if(session->remote_end) {
		peer->remote_end = session->remote_end;
		peer->avail_version++;
	    }

	    pthread_mutex_unlock(peer->shared_rsync_addr_lock);
	}

	mlfs_assert(session->intervals->count == 0);

	*rdma_entries = &session->rdma_entries;

	//mlfs_debug("rsync local |%lu|%u\n", current_local_start, session->n_unsync);
        pr_rep("Update metadata current_remote_start=%lu n_tosync=%u "
                "peer->remote_start=%lu peer->remote_end=%lu "
                "peer->avail_version=%d",
                current_remote_start, session->n_tosync,
                peer->remote_start,
                peer->remote_end,
                peer->avail_version);

#ifdef LIBFS
	if (enable_perf_stats) {
		g_perf_stats.n_rsync_blks += session->n_unsync_blk;
		g_perf_stats.n_rsync_blks_skipped += (session->n_unsync_blk - session->n_tosync_blk);
		g_perf_stats.calculating_sync_interval_time_tsc += asm_rdtscp() - tsc_begin;
	}
#endif

	uint32_t n_loghdrs = session->n_tosync;

	end_rsync_session(peer);

	return n_loghdrs;
}

static void generate_rsync_msgs_using_replay(struct replication_context *rctx,
	struct replay_list *replay_list, int destroy, uintptr_t local_base_addr)
{

	mlfs_debug("local_start: %lu, remote_start: %lu, sync_size: %lu\n",
		session->local_start, session->remote_start,
		session-> n_unsync_blk);
	struct list_head *l, *tmp;
	loghdr_t *current_loghdr;
	loghdr_t *prev_loghdr;
	addr_t current_loghdr_blk;
	addr_t prev_loghdr_blk;
	uint8_t *node_type;
	addr_t blocks_to_sync = 0;
	addr_t blocks_skipped = 0;
	int initialized = 0;

	prev_loghdr_blk = rctx->begin-1;

        // print_replay_list(replay_list);

	list_for_each_safe(l, tmp, &replay_list->head) {
		node_type = (uint8_t *)l + sizeof(struct list_head);
		mlfs_assert(*node_type < 6);

		switch(*node_type) {
			case NTYPE_I: {
				i_replay_t *item;
				item = (i_replay_t *)container_of(l, i_replay_t, list);
				current_loghdr_blk = item->blknr;
				if(destroy){
					HASH_DEL(replay_list->i_digest_hash, item);
					list_del(l);
					mlfs_free(item);
				}
				break;
			}
			case NTYPE_D: {
				panic("deprecated operation: NTYPE_D\n");
				break;
			}
			case NTYPE_F: {
				f_replay_t *item;
				item = (f_replay_t *)container_of(l, f_replay_t, list);
				if(destroy) {
					f_iovec_t *f_iovec, *f_tmp;
					HASH_DEL(replay_list->f_digest_hash, item);
					list_for_each_entry_safe(f_iovec, f_tmp,
						&item->iovec_list, iov_list) {
					//printf("item->key.inum: %u | item->key.ver: %u | f_iovec->node_type: %d\n",
					//		item->key.inum, item->key.ver, f_iovec->node_type);
					list_del(&f_iovec->iov_list);
					//no need to free here; will call later on
					//mlfs_free(f_iovec);
					}
					list_del(l);
					mlfs_free(item);
				}
				break;
			}
			case NTYPE_U: {
				u_replay_t *item;
				item = (u_replay_t *)container_of(l, u_replay_t, list);
				current_loghdr_blk = item->blknr;
				if(destroy) {
					HASH_DEL(replay_list->u_digest_hash, item);
					list_del(l);
					mlfs_free(item);
				}
				break;
			}
			case NTYPE_V: {
				f_iovec_t *item;
				item = (f_iovec_t *)container_of(l, f_iovec_t, list);
				current_loghdr_blk = item->blknr;
				if(destroy) {
					list_del(l);
					mlfs_free(item);
				}
				break;
			}
			default:
				panic("unsupported node type!\n");
		}

		//file type inodes aren't linked to log entries; only their iovecs. Skip!
		//we also ignore nodes pointing to the same log entries
		if(*node_type == NTYPE_F || current_loghdr_blk == prev_loghdr_blk)
			continue;

		//log header pointers should never point to superblock
		mlfs_assert(current_loghdr_blk != rctx->begin-1);

		current_loghdr = (loghdr_t *)(local_base_addr +
				(current_loghdr_blk << g_block_size_shift));

		if(!initialized) {
			blocks_skipped += nr_blks_between_ptrs(rctx,
				session->local_start, current_loghdr_blk,
				local_base_addr);
			session->local_start = current_loghdr_blk;
			initialized = 1;
		}
		else if(next_loghdr(prev_loghdr_blk, prev_loghdr) != current_loghdr_blk) {
			mlfs_debug("detecting a log break; prev_blk: %lu, prev_loghdr->next_blk:"
				       "%lu, current_loghdr: %lu\n", prev_loghdr_blk,
				        next_loghdr(prev_loghdr_blk, prev_loghdr), current_loghdr_blk);
			generate_rsync_msgs(rctx, blocks_to_sync, 1, local_base_addr);

			//update local_start due to gap resulting from coalesced txs
                        blocks_skipped += nr_blks_between_ptrs(
                            rctx, session->local_start, current_loghdr_blk,
                            local_base_addr);
                        session->local_start = current_loghdr_blk;
			//reset blocks_to_sync
			blocks_to_sync = 0;
		}

		blocks_to_sync += current_loghdr->nr_log_blocks;

		prev_loghdr_blk = current_loghdr_blk;
		prev_loghdr = current_loghdr;
	}

	//process any remaining blocks and convert to rdma msg metadata
	generate_rsync_msgs(rctx, blocks_to_sync, 0, local_base_addr);

        mlfs_debug("shifting loghdr pointer to final position. shift by "
                "(n_unsync_blk[%lu] - n_tosync_blk[%lu] - blocks_skipped[%lu]) "
                "= %lu\n",
                session->n_unsync_blk, session->n_tosync_blk, blocks_skipped,
                (session->n_unsync_blk - session->n_tosync_blk - blocks_skipped));

        mlfs_assert((int)(session->n_unsync_blk - session->n_tosync_blk - blocks_skipped) >= 0);

	//move local_start to end
        move_log_ptr(rctx, &session->local_start, 0, session->n_unsync_blk -
                        session->n_tosync_blk - blocks_skipped, local_base_addr);

	//verify that # of coalesced blocks is <= uncoalesced blocks
	mlfs_assert(session->n_tosync_blk <= session->n_unsync_blk);

	//if (enable_perf_stats) {
	//	g_perf_stats.n_rsync_blks += session->n_tosync_blk;
	//	g_perf_stats.n_rsync_blks_skipped += (session->n_unsync_blk - session->n_tosync_blk);
	//}

	if(destroy) {
		id_version_t *id_v, *tmp;
		HASH_ITER(hh, replay_list->id_version_hash, id_v, tmp) {
			HASH_DEL(replay_list->id_version_hash, id_v);
			mlfs_free(id_v);
		}
		HASH_CLEAR(hh, replay_list->i_digest_hash);
		HASH_CLEAR(hh, replay_list->f_digest_hash);
		HASH_CLEAR(hh, replay_list->u_digest_hash);
		HASH_CLEAR(hh, replay_list->id_version_hash);

		list_del(&replay_list->head);
	}
}

static addr_t coalesce_log_and_produce_replay(struct replay_list *replay_list,
	uintptr_t local_base_addr)
{
	mlfs_debug("%s\n", "coalescing log segment");

	replay_list->i_digest_hash = NULL;
	replay_list->f_digest_hash = NULL;
	replay_list->u_digest_hash = NULL;
	replay_list->id_version_hash = NULL;

	INIT_LIST_HEAD(&replay_list->head);

	addr_t loghdr_blk = session->local_start;
        addr_t last_loghdr_blknr = 0;
	loghdr_t *loghdr;
	addr_t counted_blks = 0;
	uint16_t logtx_size;

	// digest log entries
	while (counted_blks < session->n_unsync_blk) {
		//pid_t tid = syscall(SYS_gettid);
		loghdr = (loghdr_t *)(local_base_addr +
					(loghdr_blk << g_block_size_shift));

		if (loghdr->inuse != LH_COMMIT_MAGIC)
			panic("CRITICAL ISSUE: undefined log entry during replay list processing");

		counted_blks += loghdr->nr_log_blocks;
		coalesce_log_entry(loghdr_blk, loghdr, replay_list);

                last_loghdr_blknr = loghdr_blk; // Used in dedup_replay_list().
		loghdr_blk = next_loghdr(loghdr_blk, loghdr);
	}
        mlfs_debug("last_loghdr_blknr: %lu\n", last_loghdr_blknr);

	mlfs_assert(session->n_unsync_blk == counted_blks);
        return last_loghdr_blknr;
}

#if 0
// count # of log transactions inside a log segment
static uint16_t nrs_loghdrs(addr_t start, addr_t size)
{
	addr_t acc_size;
	addr_t loghdr_counter;
	uint16_t current_size;
	addr_t current_blk;

	assert(start + size <= g_sync_ctx->size);
	mlfs_debug("counting loghdrs starting from %lu in the next %lu blks\n", start, size);
	loghdr_counter = 1;
	current_blk = start;

	loghdr_t *current_loghdr = (loghdr_t *)(g_sync_ctx->base_addr + 
			(current_blk << g_block_size_shift));

	acc_size = current_loghdr->nr_log_blocks;

	//count # of log tx beginning from 'start' blk
	while(size > acc_size) {
		current_blk = next_loghdr(current_blk, current_loghdr);
		current_loghdr = (loghdr_t *)(g_sync_ctx->base_addr + 
			(current_blk << g_block_size_shift));

		if (current_loghdr->inuse != LH_COMMIT_MAGIC) {
			mlfs_debug("Possible inconsistency in loghdr %lu\n", current_blk);
			assert(current_loghdr->inuse == 0);
			mlfs_free(current_loghdr);
			break;
		}
		acc_size += current_loghdr->nr_log_blocks;
		//mlfs_debug("acc_size: %lu\n", acc_size);
		loghdr_counter++;
	}

	if(acc_size != size) {
		mlfs_debug("acc_size: %lu | size: %lu | current_blk: %lu | next_blk: %lu\n",
				acc_size, size, current_blk, next_loghdr(current_blk, current_loghdr));
		panic("unable to count loghdrs - log ended prematurely\n");

	}
	assert(acc_size == size);
	return loghdr_counter;	
}

#endif

void move_log_ptr_without_session(int id, addr_t *log_ptr, addr_t lag, addr_t size,
	uintptr_t base_addr, addr_t remaining_blks) {
	if(size == 0)
		return;

	mlfs_debug("moving log blk %lu with lag[%lu] by %lu blks\n", *log_ptr, lag, size);

	//compute remaining blk count from 'start' until wrap around
	mlfs_debug("log id %d before = %lu\n", id, remaining_blks);
	if(*log_ptr + size > g_sync_ctx[id]->size) {
		*log_ptr = g_sync_ctx[id]->begin + (size - remaining_blks);
		mlfs_debug("log blk moved to %lu\n", *log_ptr);
	}
	else {
		*log_ptr = *log_ptr + size;
		mlfs_debug("log blk moved to %lu\n", *log_ptr);
	}
}

//shifts loghdr blk_nr to new location by 'size' blks, taking into account wraparound
//note: for remote logs, we use the 'lag' parameter to account for lag between local & 
//remote log ptrs (in case of coalescing).
void move_log_ptr(struct replication_context *rctx, addr_t *log_ptr, addr_t lag,
	addr_t size, uintptr_t base_addr)
{
	if(size == 0)
		return;

	mlfs_debug("moving log blk %lu with lag[%lu] by %lu blks\n", *log_ptr, lag, size);

	/*
	 *
	 *  Here we have two cases:
	 *
	 *  (i) log_ptr + size > ctx->end
	 *
	 *  <ctx->begin ........ log_ptr(new) ....... log_ptr(old) ........ ctx->end> *WRAP*
	 *		  after			            	    before
	 *
	 *   N.B. size = before + after
	 *
	 *  (ii) log_ptr + size <= ctx->end
	 *
	 *  <ctx->begin ....... log_ptr(old) ........ log_ptr(new) ........ ctx->end>
	 *				       size
	 *
	 */

	//compute remaining blk count from 'start' until wrap around
	addr_t before = nr_blks_before_wrap(rctx, *log_ptr, lag, size, base_addr);
	mlfs_debug("log id %d before = %lu\n", rctx->peer->id, before);
	if(*log_ptr + size > rctx->size) {
		*log_ptr = rctx->begin + (size - before);
		mlfs_debug("log blk moved to %lu\n", *log_ptr);
	}
	else {
		*log_ptr = *log_ptr + size;
		mlfs_debug("log blk moved to %lu\n", *log_ptr);
	}
}

void reset_log_ptr(struct replication_context *rctx, addr_t *blk_nr)
{
	*blk_nr = rctx->begin;
}

addr_t nr_blks_between_ptrs(struct replication_context *rctx, addr_t start,
	addr_t target, uintptr_t base_addr)
{
	//FIXME: the sum of blocks between two pointers still includes
	//uncommitted logs; can this lead to inconsistencies?
	addr_t blks_in_between;

	if(start == target)
		return 0UL;

	//check if there's a wrap around
	if(target < start) {
		//get block size till cut-off point
		blks_in_between = nr_blks_before_wrap(rctx, start, 0,
			rctx->size - start + 2, base_addr);

		blks_in_between += (target - rctx->begin);
	}
	else
		blks_in_between = (target - start);

	mlfs_debug("%lu blks in between %lu and %lu\n", blks_in_between, start, target);
	return blks_in_between;
}

//TODO: rework function so that nr of blocks replicated == n_blk (currently it's >=)
static int start_rsync_session(struct replication_context *rctx, uint32_t n_blk,
	uintptr_t peer_base_addr)
{
	int id, ret;
	peer_meta_t *peer;

	peer = rctx->peer;

	id = peer->id;
	//setvbuf(stdout, (char*)NULL, _IONBF, 0);
	session  = (sync_meta_t*) mlfs_zalloc(sizeof(sync_meta_t));
	session->id = id;
	session->intervals = (struct sync_list*) mlfs_zalloc(sizeof(struct sync_list));
	session->end = atomic_load(rctx->end);
	session->local_start = peer->local_start;
	session->remote_start = peer->remote_start;
	session->intervals->remote_start = session->remote_start;

	//make sure both of these values are synchronized for sanity checking
	pthread_mutex_lock(peer->shared_rsync_n_lock);
	session->n_unsync = atomic_load(&peer->n_unsync);
	session->n_unsync_blk = atomic_load(&peer->n_unsync_blk);
	pthread_mutex_unlock(peer->shared_rsync_n_lock);

	//TODO: merge with log_alloc (defined in log.c)
	addr_t nr_used_blk = 0;
	if (peer->avail_version == peer->start_version) {
                // To debug.
                //if (peer->remote_start < peer->start_digest){
                //    mlfs_printf("peer->avail_version=%d start_version=%d\n",
                //            peer->avail_version, peer->start_version);
                //    mlfs_printf("peer->digesting: %d\n", peer->digesting);
                //    mlfs_printf("peer->remote_start:%lu peer->start_digest:%lu\n",
                //            peer->remote_start, peer->start_digest);
                //}
		mlfs_assert(peer->remote_start >= peer->start_digest);
		nr_used_blk = peer->remote_start - peer->start_digest;
	} else {
                // To debug.
                //if (peer->remote_start > peer->start_digest){
                //    mlfs_printf("peer->avail_version=%d start_version=%d\n",
                //            peer->avail_version, peer->start_version);
                //    mlfs_printf("peer->digesting: %d\n", peer->digesting);
                //    mlfs_printf("peer->remote_start:%lu peer->start_digest:%lu\n",
                //            peer->remote_start, peer->start_digest);
                //}
		mlfs_assert(peer->remote_start <= peer->start_digest);
		nr_used_blk = (rctx->size - peer->start_digest);
		nr_used_blk += (peer->remote_start - rctx->begin);
	}

	//check if remote log has undigested data that could be overwritten
	if(nr_used_blk + session->n_unsync_blk + rctx->begin > rctx->size) {
		// This condition shouldn't occur, since we should always send digest requests
		// to peer before its log gets full.
		// if(!peer->digesting) {
		//         panic("can't rsync; peer log is full, yet it is not digesting.\n");
		// }
		// wait_on_peer_digesting(peer);

		return -EBUSY;
	}

	//mlfs_printf("try rsync: chunk_blks %lu remote_used_blks %lu remote_log_size %lu\n",
	//	session->n_unsync_blk, nr_used_blk, rctx->size - rctx->begin + 1);

	if(session->n_unsync_blk && session->n_unsync_blk >= n_blk) {
		mlfs_debug("%s\n", " +++ start rsync session");

		session->n_tosync = session->n_unsync; //start at ceiling and decrement with coalescing
		session->peer_base_addr = peer_base_addr;
		INIT_LIST_HEAD(&session->intervals->head);
		INIT_LIST_HEAD(&session->rdma_entries);

		mlfs_debug("log info : begin %lu size %lu base %lx\n",
				rctx->begin, rctx->size, rctx->base_addr);
                mlfs_debug("sync info: n_unsync %u n_unsync_blk %lu local_start: %lu remote_start: %lu\n",
				session->n_unsync, session->n_unsync_blk, session->local_start,
				session->remote_start);
		ret = 0;
	}
	else
		ret = -1;

	return ret;
}

void end_rsync_session(peer_meta_t *peer)
{
	if(session->n_tosync) {
		atomic_fetch_add(&peer->n_pending, session->n_tosync);
		mlfs_debug("rsync complete: local_start %lu remote_start %lu n_digest %u\n",
				session->local_start, session->remote_start,
				atomic_load(&peer->n_digest));
		mlfs_debug("%s\n", "--- end rsync session");
	}

	mlfs_free(session->intervals); // TODO to be checked.
	mlfs_free(session);
	session = NULL;
}

static void generate_rsync_msgs(struct replication_context *rctx,
	addr_t sync_size, int loop_mode, uintptr_t local_base_addr)
{
	addr_t interval_size, local_interval_size, remote_interval_size;
	addr_t size;

	START_TIMER(evt_rep_gen_rsync_msgs);

	if(sync_size)
		size = sync_size;
	else
		size = session->n_unsync_blk;

	while(size > 0) {
		START_TIMER(evt_rep_grm_nr_blks);

		//only create contiguous sync intervals; limit interval_size if
		//wrap around(s) occur at local and/or remote log
                local_interval_size =
                    nr_blks_before_wrap(rctx, session->local_start, 0,
                                        size, local_base_addr);
                remote_interval_size = nr_blks_before_wrap(
                    rctx, session->remote_start,
                    session->local_start - session->remote_start, size,
                    local_base_addr);

		END_TIMER(evt_rep_grm_nr_blks);

                interval_size = min(local_interval_size, remote_interval_size);

		mlfs_debug("intervals: local->%lu remote->%lu size:%lu\n",
				session->local_start, session->remote_start, interval_size);

		//mlfs_assert(interval_size != 0);

#if 0
		//we hit wrap around on local log; wrap local log pointer
		if(local_interval_size == 0)
			session->local_start = g_sync_ctx->begin;

		//can't find any space to fill from remote pointer until end of log;
		//this occurs when there is space available at remote log but the
		//next log tx is too big to be appended contiguously
		if(remote_interval_size < size) {
			//wrap remote log pointer and convert any existing intervals
			//to rdma msg metadata; this is done since rdma-writes can't
			//perform scatter writes (i.e. write into non-cont. locations
			//at the remote end)

			//TODO need to modify this logic to support rdma verbs that
			//allow scatter writes
			session->remote_start = g_sync_ctx->begin;
			rdma_finalize();
		}

		if(interval_size == 0)
			continue;
#endif
		//adding interval [session->local_start, session->local_start+interval_size] to sync list
		rdma_add_sge(rctx, interval_size);

                // mlfs_printf("remote_intv_size %lu local_intv_size %lu intv_size %lu size %lu sync_size %lu session->n_unsync_blk %lu\n",
                        // remote_interval_size, local_interval_size, interval_size, size,
                        // sync_size, session ? session->n_unsync_blk : 0);

		//convert any existing sge elements to rdma msg. occurs when:
		//(1) remote log wrap around detected (make sure interval != 0; otherwise msg is empty)
		//(2) num of sync_intervals is equal to MAX_RDMA_SGE (i.e. max allowed by rdma write)
		if((remote_interval_size == interval_size && interval_size < size && interval_size != 0)
		//			|| (size == interval_size && !loop_mode)
					|| session->intervals->count == MAX_RDMA_SGE)
			rdma_finalize(rctx, local_base_addr);

		START_TIMER(evt_rep_grm_update_ptrs);

		//update sync metadata (if we wrapped around, reset *_start to beginning of log)
		//remote pointer is modified first, since it relies on value of current local start
		if(remote_interval_size == interval_size && interval_size < size) {
			//before we reset remote_start, calculate remote_end for remote digests
                        session->remote_end = session->remote_start + interval_size - 1;
			reset_log_ptr(rctx, &session->remote_start);
                        pr_rep("-- remote log tail is rotated: start %lu end %lu",
					session->remote_start, session->remote_end);
		}
		else
			move_log_ptr(rctx, &session->remote_start, (session->local_start
					- session->remote_start), interval_size, local_base_addr);

		if(local_interval_size == interval_size && interval_size < size)
			reset_log_ptr(rctx, &session->local_start);
		else
			move_log_ptr(rctx, &session->local_start, 0,
				interval_size, local_base_addr);

		session->n_tosync_blk += interval_size;

		//decrement # of blks to sync
		size -= interval_size;

		END_TIMER(evt_rep_grm_update_ptrs);
	}
	mlfs_assert(size == 0);

	//convert any remaining intervals to rdma msg if we're not calling this function in a loop
 	if(!loop_mode)
		rdma_finalize(rctx, local_base_addr);

	END_TIMER(evt_rep_gen_rsync_msgs);
}

void print_replay_list(struct replay_list *replay_list) {
    struct list_head *l, *tmp;
    uint8_t *node_type;

    mlfs_printf("%s", "************ print replay_list START *****************\n");
    list_for_each_safe(l, tmp, &replay_list->head) {
        node_type = (uint8_t *)l + sizeof(struct list_head);
        switch(*node_type) {
            case NTYPE_I:
                {
                    i_replay_t *item;
                    item = (i_replay_t *)container_of(l, i_replay_t, list);
                    mlfs_printf("[INODE] inum %u (ver %u) - create %d loghdr_blknr %lu\n",
                            item->key.inum, item->key.ver, item->create, item->blknr);
                    break;
                }
            case NTYPE_D:
                {
                    // deprecated.
                    break;
                }
            case NTYPE_F:
                {
                    f_replay_t *item;
                    item = (f_replay_t *)container_of(l, f_replay_t, list);
                    mlfs_printf("[FILE-NEW] inum %u (ver %u)\n",
                            item->key.inum, item->key.ver);
                    break;
                }
            case NTYPE_U:
                {
                    u_replay_t *u_item;
                    u_item = (u_replay_t *)container_of(l, u_replay_t, list);
                    mlfs_printf("[ULINK] inum %u (ver %u) loghdr_blknu %lu\n",
                            u_item->key.inum, u_item->key.ver, u_item->blknr);
                    break;
                }
            case NTYPE_V:
                {
                    f_iovec_t *item;
                    item = (f_iovec_t *)container_of(l, f_iovec_t, list);
                    mlfs_printf("[FILE-DATA] - data loghdr_blknr %lu\n",
                            item->blknr);
                    break;
                }
            default:
                {
                    printf("unsupported node type.\n");
                }
        }
    }
    mlfs_printf("%s", "************ print replay_list END *****************\n");
}

static void coalesce_log_entry(addr_t loghdr_blkno, loghdr_t *loghdr, struct replay_list *replay_list)
{
	int i, ret;
	addr_t blknr;
	uint16_t nr_entries;

	nr_entries = loghdr->n;

	for (i = 0; i < nr_entries; i++) {
		//for rsyncs, we set the blknr of replay_list elements to loghdr_blkno
		blknr = loghdr_blkno;
		switch(loghdr->type[i]) {
			case L_TYPE_INODE_CREATE:
			case L_TYPE_INODE_UPDATE: {
				i_replay_t search, *item;
				memset(&search, 0, sizeof(i_replay_t));

				search.key.inum = loghdr->inode_no[i];

				//USE HASH MAP INSTEAD OF ARRAY FOR VERSION TABLE
				id_version_t *id_v;
				HASH_FIND_INT(replay_list->id_version_hash, &search.key.inum, id_v);

				if (loghdr->type[i] == L_TYPE_INODE_CREATE) {
					//inode_version_table[search.key.inum]++;
					if(id_v == NULL) {
						id_v = (id_version_t *)mlfs_zalloc(sizeof(id_version_t));
						id_v->inum = search.key.inum;
						id_v->ver = 1;
						HASH_ADD_INT(replay_list->id_version_hash, inum, id_v);
					}
					else
						id_v->ver++;
				}

				//search.key.ver = inode_version_table[search.key.inum];

				if(id_v == NULL)
					search.key.ver = 0;
				else
					search.key.ver = id_v->ver;

				HASH_FIND(hh, replay_list->i_digest_hash, &search.key,
						sizeof(replay_key_t), item);
				if (!item) {
					item = (i_replay_t *)mlfs_zalloc(sizeof(i_replay_t));
					item->key = search.key;
					item->node_type = NTYPE_I;
					list_add_tail(&item->list, &replay_list->head);

					// tag the inode coalecing starts from inode creation.
					// This is crucial information to decide whether
					// unlink can skip or not.
					if (loghdr->type[i] == L_TYPE_INODE_CREATE)
						item->create = 1;
					else
						item->create = 0;

					HASH_ADD(hh, replay_list->i_digest_hash, key,
							sizeof(replay_key_t), item);
					mlfs_debug("[INODE] inum %u (ver %u) - create %d loghdr_blknr %lu\n",
							item->key.inum, item->key.ver, item->create, blknr);
                                } else {
                                    mlfs_debug("[INODE-found] inum %u (ver %u) - create %d updated loghdr_blknr %lu\n",
                                            item->key.inum, item->key.ver, item->create, blknr);
                                }

				// move blknr to point the up-to-date inode snapshot in the log.
                                item->blknr = blknr;
#ifdef LIBFS
				if (enable_perf_stats)
					g_perf_stats.n_rsync++;
#endif
				break;
			}
			case L_TYPE_DIR_ADD:
			case L_TYPE_DIR_DEL:
			case L_TYPE_DIR_RENAME:
			case L_TYPE_FILE: {
				f_replay_t search, *item;
				f_iovec_t *f_iovec;
				f_blklist_t *_blk_list;
				lru_key_t k;
				offset_t iovec_key;
				int found = 0;

				memset(&search, 0, sizeof(f_replay_t));
				search.key.inum = loghdr->inode_no[i];
				//search.key.ver = inode_version_table[loghdr->inode_no[i]];

				//USE HASH MAP INSTEAD OF ARRAY FOR VERSION TABLE
				id_version_t *id_v;
				HASH_FIND_INT(replay_list->id_version_hash, &search.key.inum, id_v);
				if(id_v == NULL)
					search.key.ver = 0;
				else
					search.key.ver = id_v->ver;

				HASH_FIND(hh, replay_list->f_digest_hash, &search.key,
						sizeof(replay_key_t), item);
				if (!item) {
					item = (f_replay_t *)mlfs_zalloc(sizeof(f_replay_t));
					item->key = search.key;

					HASH_ADD(hh, replay_list->f_digest_hash, key,
							sizeof(replay_key_t), item);

					INIT_LIST_HEAD(&item->iovec_list);
					item->node_type = NTYPE_F;
					item->iovec_hash = NULL;
					list_add_tail(&item->list, &replay_list->head);

					mlfs_debug("[FILE-NEW] inum %u (ver %u) loghdr_blknu %lu\n",
								item->key.inum, item->key.ver, blknr);
				}

#ifdef IOMERGE
				// IO data is merged if the same offset found.
				// Reduce amount IO when IO data has locality such as Zipf dist.
				// FIXME: currently iomerge works correctly when IO size is
				// 4 KB and aligned.
				iovec_key = ALIGN_FLOOR(loghdr->data[i], g_block_size_bytes);

				if (loghdr->data[i] % g_block_size_bytes !=0 ||
						loghdr->length[i] != g_block_size_bytes)
					panic("IO merge is not support current IO pattern\n");

				HASH_FIND(hh, item->iovec_hash,
						&iovec_key, sizeof(offset_t), f_iovec);

				if (f_iovec &&
						(f_iovec->length == loghdr->length[i])) {
					f_iovec->offset = iovec_key;
					f_iovec->blknr = blknr;
					// TODO: merge data from loghdr->blocks to f_iovec buffer.
					found = 1;
				}

				if (!found) {
					f_iovec = (f_iovec_t *)mlfs_zalloc(sizeof(f_iovec_t));
					f_iovec->length = loghdr->length[i];
					f_iovec->offset = loghdr->data[i];
					f_iovec->blknr = blknr;
					f_iovec->node_type = NTYPE_V;
					INIT_LIST_HEAD(&f_iovec->list);
					INIT_LIST_HEAD(&f_iovec->iov_list);
					list_add_tail(&f_iovec->iov_list, &item->iovec_list);
					list_add_tail(&f_iovec->list, &replay_list->head);

					f_iovec->hash_key = iovec_key;
					HASH_ADD(hh, item->iovec_hash, hash_key,
							sizeof(offset_t), f_iovec);
				}
#else
				f_iovec = (f_iovec_t *)mlfs_zalloc(sizeof(f_iovec_t));
				f_iovec->length = loghdr->length[i];
				f_iovec->offset = loghdr->data[i];
				f_iovec->blknr = blknr;
				f_iovec->node_type = NTYPE_V;
				INIT_LIST_HEAD(&f_iovec->list);
				INIT_LIST_HEAD(&f_iovec->iov_list);
				list_add_tail(&f_iovec->iov_list, &item->iovec_list);
				list_add_tail(&f_iovec->list, &replay_list->head);

#endif	//IOMERGE
				mlfs_debug("[FILE-DATA] inum %u (ver %u) - data loghdr_blknr %lu\n",
							item->key.inum, item->key.ver, f_iovec->blknr);
#ifdef LIBFS
				if (enable_perf_stats)
					g_perf_stats.n_rsync++;
#endif
				break;
			}
			case L_TYPE_UNLINK: {
				// Got it! Kernfs can skip digest of related items.
				// clean-up inode, directory, file digest operations for the inode.
				uint32_t inum = loghdr->inode_no[i];
				i_replay_t i_search, *i_item;
				f_replay_t f_search, *f_item;
				u_replay_t u_search, *u_item;
				//d_replay_key_t d_key;
				f_iovec_t *f_iovec, *tmp;

				replay_key_t key;

				key.inum = loghdr->inode_no[i];
				id_version_t *id_v;

				//USE HASH MAP INSTEAD OF ARRAY FOR VERSION TABLE
				HASH_FIND_INT(replay_list->id_version_hash, &key.inum, id_v);
				if(id_v == NULL)
					key.ver = 0;
				else
					key.ver = id_v->ver;

				// This is required for structure key in UThash.
				memset(&i_search, 0, sizeof(i_replay_t));
				memset(&f_search, 0, sizeof(f_replay_t));
				memset(&u_search, 0, sizeof(u_replay_t));

				mlfs_debug("%s\n", "-------------------------------");

				// check inode digest info can skip.
				i_search.key.inum = key.inum;
				i_search.key.ver = key.ver;
				HASH_FIND(hh, replay_list->i_digest_hash, &i_search.key,
						sizeof(replay_key_t), i_item);
				//printf("UNLINK STATUS: i_item->: %d\n", i_item->create);
				if (i_item && i_item->create) {
					mlfs_debug("[INODE] inum %u (ver %u) --> SKIP\n",
							i_item->key.inum, i_item->key.ver);
					// the unlink can skip and erase related i_items
					HASH_DEL(replay_list->i_digest_hash, i_item);
					list_del(&i_item->list);
					mlfs_free(i_item);
					session->n_tosync--;
#ifdef LIBFS
					if (enable_perf_stats)
						g_perf_stats.n_rsync_skipped++;
#endif
				} else {
					// the unlink must be applied. create a new unlink item.
					u_item = (u_replay_t *)mlfs_zalloc(sizeof(u_replay_t));
					u_item->key = key;
					u_item->node_type = NTYPE_U;
					u_item->blknr = blknr;
					HASH_ADD(hh, replay_list->u_digest_hash, key,
							sizeof(replay_key_t), u_item);
					list_add_tail(&u_item->list, &replay_list->head);
					mlfs_debug("[ULINK] inum %u (ver %u)\n",
							u_item->key.inum, u_item->key.ver);
					session->n_tosync--;        // JYKIM why decrease n_tosync here????
#ifdef LIBFS
					if (enable_perf_stats)
						g_perf_stats.n_rsync++;
#endif
				}

				// delete file digest info.
				f_search.key.inum = key.inum;
				f_search.key.ver = key.ver;

				HASH_FIND(hh, replay_list->f_digest_hash, &f_search.key,
						sizeof(replay_key_t), f_item);
				if (f_item) {
					list_for_each_entry_safe(f_iovec, tmp,
							&f_item->iovec_list, iov_list) {
						list_del(&f_iovec->list);
						list_del(&f_iovec->iov_list);
						mlfs_free(f_iovec);
						session->n_tosync--;
#ifdef LIBFS
						if (enable_perf_stats)
							g_perf_stats.n_rsync_skipped++;
#endif
					}

					HASH_DEL(replay_list->f_digest_hash, f_item);
					list_del(&f_item->list);
					mlfs_free(f_item);
				}

				mlfs_debug("%s\n", "-------------------------------");
				break;
			}
			default: {
				printf("%s: digest type %d\n", __func__, loghdr->type[i]);
				panic("unsupported type of operation\n");
				break;
			}
		}
	}
}


static void rdma_add_sge(struct replication_context *rctx, addr_t size)
{
	START_TIMER(evt_rep_rdma_add_sge);
	//size should always be a +ve value
	if(size <= 0) {
		END_TIMER(evt_rep_rdma_add_sge);
		return;
	}

	sync_interval_t *interval  = (sync_interval_t *) mlfs_zalloc(sizeof(sync_interval_t));
	interval->start = session->local_start;
	interval->end = interval->start + size - 1;

	//make sure interval is contiguous
	if(interval->end > rctx->size) {
		mlfs_debug("intrv start: %lu | intrv end: %lu | log end: %lu\n",
				interval->start, interval->end, rctx->size);
		panic("error producing sync interval");
	}

	INIT_LIST_HEAD(&interval->head);
	list_add_tail(&interval->head, &session->intervals->head);

	//update intervals list metadata (counters, etc.)
	//if this is the first gather element in rdma msg, set initial remote address
	if(session->intervals->count == 0)
		session->intervals->remote_start = session->remote_start;
	session->intervals->count++;
	session->intervals->n_blk += size;

	END_TIMER(evt_rep_rdma_add_sge);
}

static void rdma_finalize(struct replication_context *rctx, uintptr_t local_base_addr)
{
	START_TIMER(evt_rep_rdma_finalize);
	//ignore calls when there are no sge elements
	if(session->intervals->count == 0) {
		END_TIMER(evt_rep_rdma_finalize);
		return;
	}

	pr_rdma("+++ create rdma - remote_start %lu n_blk %lu n_sge %u",
			session->intervals->remote_start, session->intervals->n_blk,
			session->intervals->count);
	struct rdma_meta_entry *rdma_entry = (struct rdma_meta_entry *)
		mlfs_zalloc(sizeof(struct rdma_meta_entry));
	rdma_entry->meta = (rdma_meta_t *) mlfs_zalloc(sizeof(rdma_meta_t)
			+ session->intervals->count * sizeof(struct ibv_sge));

        rdma_entry->meta->addr = session->peer_base_addr +
		(session->intervals->remote_start << g_block_size_shift);

	rdma_entry->meta->length = session->intervals->n_blk << g_block_size_shift;
	rdma_entry->meta->sge_count = session->intervals->count;
	rdma_entry->meta->next = NULL;

	pr_rdma("%s", "Replication RDMA SGE:");
	pr_rdma("\tpeer base address = 0x%lx", session->peer_base_addr);
	pr_rdma("\tstart_addr (remote_start << g_block_size_shift) = 0x%lx",
		(session->intervals->remote_start << g_block_size_shift));
	pr_rdma("\tlength = %lu", rdma_entry->meta->length);
	pr_rdma("\trdma_entry->meta->addr = 0x%lx", rdma_entry->meta->addr);
	pr_rdma("\trdma_entry->meta->total_len = %lu", (session->intervals->n_blk << g_block_size_shift));

	sync_interval_t *interval, *tmp;
	int i = 0;

        // addr_t total_len = 0;

	list_for_each_entry_safe(interval, tmp, &session->intervals->head, head) {
		mlfs_assert(interval->end <= rctx->size);
		mlfs_assert(interval->start <= interval->end);
                pr_rdma("sge%d - local_start %lu n_blk %lu",
                                i, interval->start, interval->end - interval->start + 1);

		rdma_entry->meta->sge_entries[i].addr =
                    (uint64_t) (local_base_addr + (interval->start << g_block_size_shift));

		rdma_entry->meta->sge_entries[i].length = (interval->end
				- interval->start + 1) << g_block_size_shift;
                pr_rdma("sge - addr 0x%lx length %u", rdma_entry->meta->sge_entries[i].addr,
                                rdma_entry->meta->sge_entries[i].length);
                // total_len += rdma_entry->meta->sge_entries[i].length;
		list_del(&interval->head);
		mlfs_free(interval);
		i++;
	}
        // pr_rep("Total length: %lu", total_len);
	pr_rdma("%s\n", "--- end rdma");

	list_add_tail(&rdma_entry->head, &session->rdma_entries);

	//reset list counters
	session->intervals->count = 0;
	session->intervals->n_blk = 0;

	END_TIMER(evt_rep_rdma_finalize);
}

static addr_t nr_blks_before_wrap(struct replication_context *rctx, addr_t start, addr_t lag, addr_t size,
	uintptr_t base_addr)
{
	addr_t end_blknr;
	end_blknr = atomic_load(rctx->end);

	mlfs_debug("log traverse: id %d type %s start_blk %lu size %lu\n",
                rctx->peer->id, lag?"remote":"local", start+lag, size);

	// TODO lag support is not tested for NIC_OFFLOAD. JYKIM.
#ifdef NIC_OFFLOAD
	mlfs_assert(lag == 0);
#endif

	//check for wrap around and compute block sizes till cut-off
	if (end_blknr && start >= end_blknr) {
	    return 0;

	} else if(start + size > rctx->size) {
		//computationally intensive for larger #s of loghdrs
		//but only happens when wraparounds occur - which is infrequent
		//TODO: need to optimize if prior assumption becomes invalid

		int step = 0;
		addr_t nr_blks = 0;
		addr_t current_blk = start + lag;

#ifdef NIC_SIDE
		loghdr_t *current_loghdr = (loghdr_t *)(base_addr +
				(current_blk << g_block_size_shift));
#else
		loghdr_t *current_loghdr = (loghdr_t *)(rctx->base_addr +
				(current_blk << g_block_size_shift));
#endif

		//compute all log tx sizes beginning from 'start' blk until we reach end
		while((start + nr_blks + current_loghdr->nr_log_blocks <= rctx->size)
				&& nr_blks + current_loghdr->nr_log_blocks <= size) {

                        /*
                        mlfs_debug("current_blk=%lu current_size=%d "
                                "(current_blk + current_size)=%lu original size=%lu "
                                "log_size=%lu log_end=%lu nr_blks=%lu\n",
                                current_blk, current_loghdr->nr_log_blocks,
                                current_blk + current_loghdr->nr_log_blocks,
                                size, rctx->size + lag,
                                atomic_load(rctx->end), nr_blks);
                        */

			step++;

			if (current_loghdr->inuse != LH_COMMIT_MAGIC) {
				mlfs_debug("cond check inuse != LH_COMMIT_MAGIC for loghdr %lu\n", current_blk);
				mlfs_assert(nr_blks == size);
				break;
			}

			nr_blks += current_loghdr->nr_log_blocks;

                        mlfs_debug("step%d: current_blk %lu nr_blks %u acc_size %lu\n",
				step, current_blk, current_loghdr->nr_log_blocks, nr_blks);

			//we are done here
			if(nr_blks == size)
				break;

			current_blk = next_loghdr(current_blk, current_loghdr);

#ifdef NIC_SIDE
			current_loghdr = (loghdr_t *)(base_addr +
				(current_blk << g_block_size_shift));
#else
			current_loghdr = (loghdr_t *)(rctx->base_addr +
				(current_blk << g_block_size_shift));
#endif

                        /*
                        mlfs_debug("current_blk=%lu current_size=%d "
                                "(current_blk + current_size)=%lu original size=%lu "
                                "log_size=%lu log_end=%lu nr_blks=%lu\n",
                                current_blk, current_loghdr->nr_log_blocks,
                                current_blk + current_loghdr->nr_log_blocks,
                                size, rctx->size + lag,
                                atomic_load(rctx->end), nr_blks);
                        */
		}

		// If we are at the end of the log.
		if (start == rctx->size)
		    mlfs_assert(nr_blks == 0);

		return nr_blks;
	}
	else
		return size;
}

// Called by libfs.
void request_publish_remains(void){
	int nic_kernfs_id, sockfd;
	char msg[MAX_SOCK_BUF];

	nic_kernfs_id = host_kid_to_nic_kid(g_kernfs_id);
	sockfd = g_kernfs_peers[nic_kernfs_id]->sockfd[SOCK_BG];

	sprintf(msg, "|" TO_STR(RPC_PUBLISH_REMAINS) " |%d|", g_self_id);

	rpc_forward_msg_no_seqn(sockfd, msg);
	printf("Request to publish remaining memcpy list. libfs_id=%d\n",
	       g_self_id);
}

#if 0
//FIXME: no reason to do this calculation; loghdrs should instead carry
//a size attribute in place of pointers
static uint16_t get_logtx_size(loghdr_t *loghdr, addr_t blk_nr)
{
	uint16_t tx_size;
	if(loghdr->next_loghdr_blkno != 0 && loghdr->next_loghdr_blkno > blk_nr)
		tx_size = loghdr->next_loghdr_blkno - blk_nr;
	else {
		//we can't rely on next_loghdr_blkno to compute current log tx size
		tx_size = 1; //accounts for logheader block
		for (int i = 0; i < loghdr->n; i++)
			tx_size += (loghdr->length[i] >> g_block_size_shift);
	}
	mlfs_debug("[get_logtx_size] loghdr %lu size is %u blk(s)\n", blk_nr, tx_size);
	return tx_size;
}
#endif

//--------------USED FOR DEBUGGING/PROFILING----------------------
//----------------------------------------------------------------

#if 0
int create_rsync_mock(int id, struct list_head **rdma_entries, addr_t start_blk)
{
	uint64_t tsc_begin;
	if (enable_perf_stats)
		tsc_begin = asm_rdtscp();

	int ret = start_rsync_session(g_sync_ctx[id]->peer, 0);

	//nothing to rsync
	if(ret) {
		end_rsync_session(g_sync_ctx->peer);
		return -1;
	}

	mlfs_debug("n_unsync_blk: %lu\n", session->n_unsync_blk);
	atomic_fetch_sub(&g_sync_ctx[id]->peer->n_unsync_blk, session->n_unsync_blk);
	atomic_fetch_sub(&g_sync_ctx[id]->peer->n_unsync, session->n_unsync);

#ifdef COALESCING
	session->local_start = start_blk;
	session->remote_start = start_blk;
	
	struct replay_list *replay_list = (struct replay_list *) mlfs_zalloc(
			sizeof(struct replay_list));

	coalesce_log_and_produce_replay(replay_list);

	if(enable_perf_stats) {
		g_perf_stats.coalescing_log_time_tsc += asm_rdtscp() - tsc_begin;
		tsc_begin = asm_rdtscp();
	}

	//set coalescing factor 'div' and # of sge elements 'sge';
	int div = 2;
	int sge = 2;

	generate_rsync_msgs_using_replay_mock(replay_list, 1, div, sge);

#else
	session->local_start = g_sync_ctx[id]->begin;
	session->remote_start = g_sync_ctx[id]->begin;
	session->n_tosync_blk = session->n_unsync_blk;
	rdma_add_sge(session->n_unsync_blk); 	
	rdma_finalize();
#endif


	assert(session->intervals->count == 0);

	*rdma_entries = &session->rdma_entries;


	if (enable_perf_stats) {
		g_perf_stats.n_rsync_blks += session->n_unsync_blk;
	}

	end_rsync_session(g_sync_ctx[id]->peer);

	return 0;
}

void generate_rsync_msgs_using_replay_mock(struct replay_list *replay_list, int destroy, int div, int sge)
{
	mlfs_debug("------starting---%lu---\n", session->local_start);

	//printf("local_start: %lu, remote_start: %lu, sync_size: %lu\n", session->local_start, session->remote_start,
	//		session-> n_unsync_blk);
	struct list_head *l, *tmp;
	loghdr_t *current_loghdr;
	loghdr_t *prev_loghdr;
	addr_t current_loghdr_blk;
	addr_t prev_loghdr_blk;
	uint8_t *node_type;
	addr_t blocks_to_sync = 0;
	addr_t blocks_skipped = 0;
	int initialized = 0;
	
	prev_loghdr_blk = g_sync_ctx[id]->begin-1;

	list_for_each_safe(l, tmp, &replay_list->head) {
		node_type = (uint8_t *)l + sizeof(struct list_head);
		mlfs_assert(*node_type < 6);
	
		switch(*node_type) {
			case NTYPE_I: {
				i_replay_t *item;
				item = (i_replay_t *)container_of(l, i_replay_t, list);
				current_loghdr_blk = item->blknr;
				if(destroy){
					HASH_DEL(replay_list->i_digest_hash, item);
					list_del(l);
					mlfs_free(item);
				}
				break;
			}
			case NTYPE_D: {
				d_replay_t *item;
				item = (d_replay_t *)container_of(l, d_replay_t, list);
				current_loghdr_blk = item->blknr;
				if(destroy)
					HASH_DEL(replay_list->d_digest_hash, item);
					list_del(l);
					mlfs_free(item);

				break;
			}
			case NTYPE_F: {
				f_replay_t *item;
				item = (f_replay_t *)container_of(l, f_replay_t, list);
				if(destroy) {
					f_iovec_t *f_iovec, *f_tmp;
					HASH_DEL(replay_list->f_digest_hash, item);
					list_for_each_entry_safe(f_iovec, f_tmp, 
						&item->iovec_list, iov_list) {
					//printf("item->key.inum: %u | item->key.ver: %u | f_iovec->node_type: %d\n",
					//		item->key.inum, item->key.ver, f_iovec->node_type);
					list_del(&f_iovec->iov_list);
					//no need to free here; will call later on
					//mlfs_free(f_iovec);
					}
					list_del(l);
					mlfs_free(item);
				}	
				break;
			}
			case NTYPE_U: {
				u_replay_t *item;
				item = (u_replay_t *)container_of(l, u_replay_t, list);
				current_loghdr_blk = item->blknr;
				if(destroy) {
					HASH_DEL(replay_list->u_digest_hash, item);
					list_del(l);
					mlfs_free(item);
				}
				break;
			}
			case NTYPE_V: {
				f_iovec_t *item;
				item = (f_iovec_t *)container_of(l, f_iovec_t, list);
				current_loghdr_blk = item->blknr;
				if(destroy) {
					list_del(l);
					mlfs_free(item);
				}
				break;
			}
			default:
				panic("unsupported node type!\n");
		}
		prev_loghdr_blk = current_loghdr_blk;
		prev_loghdr = current_loghdr;
	}

	session->local_start = g_sync_ctx[id]->begin;
	session->remote_start = g_sync_ctx[id]->begin;
	//printf("DEBUG size is n_unsync:%lu final_size:%lu\n", session->n_unsync_blk, ((session->n_unsync_blk/div)/sge));

	addr_t size_remaining, size_to_sync;

	//only coalesce if we have 'div' or more loghdrs, otherwise do nothing
	if(session->n_unsync >= div) {
		size_remaining = session->n_unsync_blk/div;
		sge = min(sge, max(session->n_unsync_blk/div,1));
	}
	else {
		size_remaining = session->n_unsync_blk;
		sge = 1;
	}

	//addr_t size_remaining = session->n_unsync_blk/div;
	session->n_tosync_blk = size_remaining;

	size_to_sync = max(size_remaining/sge,1);

	for(int i=0; i<sge; i++) {
		//printf("[rsync-sge] size_to_sync: %lu | total: %lu | sge: %d\n", size_to_sync, session->n_tosync_blk, sge);
		if(size_to_sync == 0)
			break;
		rdma_add_sge(size_to_sync);
		session->local_start += size_to_sync;
		//session->remote_start += size_to_sync;
		size_remaining -= size_to_sync;

		//if(i == sge - 2)
		//	size_to_sync = size_remaining;
		//else
		size_to_sync = min(size_to_sync, size_remaining);
	}

	//if (enable_perf_stats) {
	//	g_perf_stats.n_rsync_blks += session->n_tosync_blk;
	//	g_perf_stats.n_rsync_blks_skipped += (session->n_unsync_blk - session->n_tosync_blk);
	//}

	rdma_finalize();

	if(destroy) {
		id_version_t *id_v, *tmp;
		HASH_ITER(hh, replay_list->id_version_hash, id_v, tmp) {
			HASH_DEL(replay_list->id_version_hash, id_v);
			mlfs_free(id_v);	
		}
		HASH_CLEAR(hh, replay_list->i_digest_hash);
		HASH_CLEAR(hh, replay_list->d_digest_hash);
		HASH_CLEAR(hh, replay_list->f_digest_hash);
		HASH_CLEAR(hh, replay_list->u_digest_hash);
		HASH_CLEAR(hh, replay_list->id_version_hash);

		list_del(&replay_list->head);
	}

}

// call this function to start a nanosecond-resolution timer
struct timespec timer_start(){
	struct timespec start_time;
	clock_gettime(CLOCK_REALTIME, &start_time);
	return start_time;
}

// call this function to end a timer, returning nanoseconds elapsed as a long
long timer_end(struct timespec start_time) {
	struct timespec end_time;
	long sec_diff, nsec_diff, nanoseconds_elapsed;

	clock_gettime(CLOCK_REALTIME, &end_time);

	sec_diff =  end_time.tv_sec - start_time.tv_sec;
	nsec_diff = end_time.tv_nsec - start_time.tv_nsec;

	if(nsec_diff < 0) {
		sec_diff--;
		nsec_diff += (long)1e9;
	}

	nanoseconds_elapsed = sec_diff * (long)1e9 + nsec_diff;

	return nanoseconds_elapsed;
}
#endif

static void print_peer_meta_struct(peer_meta_t *meta) {
	// mlfs_debug("local start: %lu | remote start: %lu | n_used: %u | n_used_blk: %lu | n_digest: %u | n_unsync_blk: %lu | "
	//                 "mutex_check: %lu\n", meta->local_start, meta->remote_start, atomic_load(&meta->n_used),
	//                        atomic_load(&meta->n_used_blk), atomic_load(&meta->n_digest), atomic_load(&meta->n_unsync_blk),
	//                 atomic_load(&meta->cond_check));
	mlfs_debug("local start: %lu | remote start: %lu | n_digest: %u | n_unsync_blk: %lu | "
			"mutex_check: %lu\n", meta->local_start, meta->remote_start,
		       	atomic_load(&meta->n_digest), atomic_load(&meta->n_unsync_blk),
			atomic_load(&meta->cond_check));
}

static void print_sync_meta_struct(sync_meta_t *meta) {
	mlfs_debug("local start: %lu | remote start: %lu | n_unsync_blk: %lu | n_tosync_blk: %lu\n"
			"intervals->count: %u | intervals->n_blk: %lu | intervals->remote_start: %lu | "
			"peer_base_addr: %lu\n",
		       	meta->local_start, meta->remote_start, meta->n_unsync_blk, meta->n_tosync_blk,
			meta->intervals->count, meta->intervals->n_blk, meta->intervals->remote_start,
			meta->peer_base_addr);
}

static void print_sync_intervals_list(sync_meta_t *meta) {
	sync_interval_t *interval, *tmp;
	int i = 0;
	mlfs_printf("------ printing sync list ----%lu-\n", 0UL);
	list_for_each_entry_safe(interval, tmp, &meta->intervals->head, head) {
		mlfs_printf("[%d] start: %lu | end: %lu | len: %lu | log size: %lu\n", i, interval->start,
			       	interval->end, interval->end-interval->start+1, g_sync_ctx[meta->id]->size);
		i++;
	}
	mlfs_printf("------------ end list -------%lu-\n", 0UL);
}

// void print_rctx(struct replication_context *ctx);
void print_rctx(struct replication_context *ctx)
{
    if (ctx) {
	pr_setup("begin=%lu end=%lu log_size=%lu base_addr=0x%lx "
		"next_rep_msg_sockfd[0]=%d next_rep_msg_sockfd[1]=%d "
		"next_rep_data_sockfd[0]=%d next_rep_data_sockfd[1]=%d "
		"next_digest_sockfd=%d",
		ctx->begin, atomic_load(ctx->end),
		ctx->size, ctx->base_addr,
		ctx->next_rep_msg_sockfd[0],
		ctx->next_rep_msg_sockfd[1],
		ctx->next_rep_data_sockfd[0],
		ctx->next_rep_data_sockfd[1],
		ctx->next_digest_sockfd);
	print_peer_id(ctx->peer->info);
#ifdef NIC_SIDE
	if (ctx->host_rctx) {
	    pr_setup("%s", "======== HOST_RCTX ========");
	    print_rctx(ctx->host_rctx);
	    pr_setup("%s", "===========================\n");
	}
#endif
    }
}

void print_sync_ctx(void) {
    int i;
    int len;
    struct replication_context* ctx;
#ifdef KERNFS
    len = MAX_LIBFS_PROCESSES + g_n_replica;
#else
#ifdef NIC_OFFLOAD
    len = g_n_nodes;
#else
    len = g_n_replica;
#endif
#endif
    for (i = 0; i < len; i++) {
	pr_setup("g_sync_ctx[%d]=%p", i, g_sync_ctx[i]);
	print_rctx(g_sync_ctx[i]);
    }
}

//---------------------------------------------------------------
//---------------------------------------------------------------

/**
 * Print metadata.
 * It doesn't hold any lock. Values may be inconsistent
 * according to where it is called.
 */
void print_meta(struct replication_context *sync_ctx, char const *caller_name,
	int line)
{
    if (g_n_replica <= 1)
	return;

    peer_meta_t *peer = sync_ctx->peer;

    pr_meta("META DATA -------------%s():%d", caller_name, line);
    pr_meta("\tlibfs_id=%d", peer->id);

    pr_meta("gsync_ctx (local)\n"
            "\t%-20s= %-20lu"  "%-20s= %-20lu" "%-20s= %lu\n"
            "\t%-20s= %#-20lx" "%-20s= %-20d"  "%-20s= %d\n"
            "\t%-20s= %-20d"   "%-20s= %-20d"  "%-20s= %d\n"
	    "\t%-20s= %-20p"   "%-20s= %-20lu\n"
	    "\t%-20s= %-20p"   "%-20s= %-20p\n",
            "begin", sync_ctx->begin,
	    "last_blk(size)", sync_ctx->size,
	    "end", atomic_load(sync_ctx->end),

	    "base_addr", sync_ctx->base_addr,
	    "next_rep_msg_sock0", sync_ctx->next_rep_msg_sockfd[0],
	    "next_rep_msg_sock1", sync_ctx->next_rep_msg_sockfd[1],

	    "next_rep_data_sock0", sync_ctx->next_rep_data_sockfd[0],
	    "next_rep_data_sock1", sync_ctx->next_rep_data_sockfd[1],
            "next_digest_sock", sync_ctx->next_digest_sockfd,

            "log_hdrs", sync_ctx->log_hdrs,
            "cur_loghdr_id", (sync_ctx->cur_loghdr_id ? *sync_ctx->cur_loghdr_id : 0UL),

            "log_buf->base(nl)", (sync_ctx->log_buf ? sync_ctx->log_buf->base : NULL),
	    "log_buf->cur(nl)", (sync_ctx->log_buf ? sync_ctx->log_buf->cur : NULL));
#ifdef LIBFS
    pr_meta("g_fs_log (local)\n"
            "\t%-20s= %-20lu" "%-20s= %-20lu" "%-20s= %u\n"
            "\t%-20s= %-20u"  "%-20s= %-20u"  "%-20s= %u\n"
	    "\t%-20s= %-20u",
	    "start_blk", g_fs_log[0]->start_blk,
	    "next_avail_hdr", g_fs_log[0]->next_avail_header,
	    "digesting", g_fs_log[0]->digesting,
	    "start_version", g_fs_log[0]->start_version,
	    "avail_version", g_fs_log[0]->avail_version,
	    "n_digest_req", g_fs_log[0]->n_digest_req,
	    "nloghdr", g_fs_log[0]->nloghdr);

    pr_meta("g_log_sb (local)\n"
            "\t%-20s= %-20lu" "%-20s= %-20u" "%-20s= %u\n"
	    "\t%-20s= %-20lu",
	    "start_digest", g_log_sb[0]->start_digest,
	    "n_digest", atomic_load(&g_log_sb[0]->n_digest),
	    "n_digest_blks", atomic_load(&g_log_sb[0]->n_digest_blks),
	    "end", atomic_load(&g_log_sb[0]->end));
#endif

    // (f) : used only in the first node.
    // (nl): not used in the last node.
    pr_meta("g_sync_ctx->peer (peer)\n"
	    "\t%-20s= %-20lu" "%-20s= %-20lu" "%-20s= %lu\n"
            "\t%-20s= %-20lu" "%-20s= %-20u"  "%-20s= %u\n"
	    "\t%-20s= %-20u"  "%-20s= %-20lu" "%-20s= %u\n"
	    "\t%-20s= %-20u"  "%-20s= %-20d"  "%-20s= %lu\n"
	    "\t%-20s= %-20p"  "%-20s= %-20p"  "%-20s= %lu\n"
	    "\t%-20s= %-20p"  "%-20s= %-20p\n",

            "local_start", peer->local_start,
	    "remote_start(nl)", peer->remote_start,
	    "remote_end", peer->remote_end,

	    "start_digest", peer->start_digest,
	    "start_version", peer->start_version,
	    "avail_version", peer->avail_version,

	    "n_digest(f)", atomic_load(&peer->n_digest),
	    "n_unsync_blk(nl)", atomic_load(&peer->n_unsync_blk),
	    "n_unsync(nl)", atomic_load(&peer->n_unsync),

	    "n_unsync_to_end(nl)", atomic_load(&peer->n_unsync_upto_end),
	    "digesting", peer->digesting,
	    "mutex_check", atomic_load(&peer->cond_check),

	    "lghdrs_addr(nl)", (char*)peer->log_hdrs_addr,
	    "lghdrs_curid_add(nl)", (char*)peer->log_hdrs_cur_id_addr,
	    "lghdrs_curid(nl)", atomic_load(&peer->log_hdrs_cur_id),

            "logbuf>buf_base(nl)", (peer->log_buf ? (char*)peer->log_buf->peer_buf_base : NULL),
	    "logbuf>cur_addr(nl)", (peer->log_buf ? (char*)peer->log_buf->peer_cur_addr : NULL));

    pr_meta("%s", "-----------------------------");
}

#endif
