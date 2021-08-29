#ifdef DISTRIBUTED

#include "io/device.h"
#include "io/block_io.h"
#include "distributed/peer.h"
//#include "log/log.h"
#include "storage/storage.h" // for nic_slab

#ifdef KERNFS

#ifdef __x86_64__
#include "fs.h"
#elif __aarch64__
#include "nic/nic_fs.h"
#else
#error "Unsupported architecture."
#endif

#else
#include "filesystem/fs.h"
#endif

#ifdef KERNFS
struct peer_socket *g_rpc_socks[sock_bitmap_size];
#else
struct peer_socket *g_rpc_socks[sock_bitmap_size];
#endif

struct fetch_seqn g_fetch_seqn; // Used by LibFS.

struct peer_id *g_kernfs_peers[g_n_nodes];
struct peer_id *g_peers[peer_bitmap_size];
int g_peer_count = 0;
int g_sock_count = 0;

DECLARE_BITMAP(peer_bitmap, peer_bitmap_size);
DECLARE_BITMAP(sock_bitmap, sock_bitmap_size);
pthread_mutex_t peer_bitmap_mutex = PTHREAD_MUTEX_INITIALIZER;

static void print_bits(unsigned int x);

#ifdef LIBFS
void init_fetch_seqn() {
	g_fetch_seqn.n = 0;
	pthread_spin_init(&g_fetch_seqn.fetch_seqn_lock, PTHREAD_PROCESS_SHARED);
}
#endif

void peer_init()
{
	bitmap_zero(peer_bitmap, peer_bitmap_size);
	bitmap_zero(sock_bitmap, sock_bitmap_size);

	//pthread_mutexattr_t attr;
	//peer_bitmap_mutex = (pthread_mutex_t *)mlfs_zalloc(sizeof(pthread_mutex_t));
	//pthread_mutexattr_init(&attr);
	//pthread_mutex_init(peer_bitmap_mutex, NULL);
#ifdef LIBFS
	printf("INIT fetch seqn to 0\n");
	init_fetch_seqn();
#endif
}

void lock_peer_access()
{
	pthread_mutex_lock(&peer_bitmap_mutex);
}

void unlock_peer_access()
{
	pthread_mutex_unlock(&peer_bitmap_mutex);
}

/**
 * @Synopsis
 *
 * @Param libfs_id
 *
 * @Returns Return local kernfs id. Return -1 if there is no such peer.
 */
int local_kernfs_id(int libfs_id)
{
	char * ip;
	if (libfs_id == g_self_id)
		return g_kernfs_id;

	if (!g_peers[libfs_id])
		return -1;

	ip = g_peers[libfs_id]->ip;

	return _find_peer(ip, 0)->id;
}

int get_next_peer_id(int id) {
	return find_next_bit(peer_bitmap, MAX_LIBFS_PROCESSES, 0);
}

void add_peer_socket(int sockfd, int sock_type)
{
	mlfs_debug("found socket %d\n", sockfd);
	struct peer_socket *psock = mlfs_zalloc(sizeof(struct peer_socket));

	//if kernfs
	struct peer_id *peer = find_peer(sockfd);

	lock_peer_access();

	bitmap_set(sock_bitmap, sockfd, 1);
	//if peer is not found, then this is a LibFS process
	if(!peer) {
#ifdef LIBFS
		// only KernFS processes accept connections from LibFSes
		mlfs_assert(false);
#endif
		peer = mlfs_zalloc(sizeof(struct peer_id));
		strncpy(peer->ip, rc_connection_ip(sockfd), NI_MAXHOST);
		peer->pid = rc_connection_meta(sockfd);
		//g_peer_count++;
		mlfs_debug("Adding new peer (ip: %s, pid: %u)\n", peer->ip, peer->pid);
		//peer->type = 
		//peer->role = 
		//peer->sockfd[0] = sockfd;
	}

	if(!peer->sockcount) {
		//register_peer_log(peer);
		//g_peers[peer->id] = peer;
	}

	peer->sockfd[sock_type] = sockfd;
	peer->sockcount++;
	psock->fd = peer->sockfd[sock_type];
	psock->seqn = 0;
        pthread_spin_init(&psock->seqn_lock, PTHREAD_PROCESS_SHARED);
	psock->type = sock_type;
	psock->peer = peer;
	g_rpc_socks[peer->sockfd[sock_type]] = psock;
	g_sock_count++;

	unlock_peer_access();

	mlfs_printf("Established connection: peer_id=%d ip=%s sock=%d "
		    "sock_type=%d peer:%p\n",
		    peer->id, peer->ip, psock->fd, psock->type,
		    g_rpc_socks[psock->fd]->peer);
	//print_peer_id(peer);
}

void remove_peer_socket(int sockfd, int low_lat)
{
	//struct peer_id *peer = find_peer(sockfd);
	struct peer_id *peer;

	// Already removed.
	if (!g_rpc_socks[sockfd])
	    return;

	peer = g_rpc_socks[sockfd]->peer;
	mlfs_assert(peer);

#if MLFS_LEASE && defined(KERNFS)
	// if peer disconnects, cancel all related leases
	if(g_rpc_socks[sockfd]->type == SOCK_LS)
		discard_leases(peer->id);
#endif

	//lock_peer_access();
	bitmap_clear(sock_bitmap, sockfd, 1);
	peer->sockfd[g_rpc_socks[sockfd]->type] = -1;
	peer->sockcount--;
	mlfs_info("Disconnected with %s on sock:%d of type:%d\n",
			peer->ip, sockfd, g_rpc_socks[sockfd]->type);
	free(g_rpc_socks[sockfd]);
	g_sock_count--;
	//unlock_peer_access();
	if(!peer->sockcount) {
		if (!low_lat)
		    unregister_peer_log(peer);
		mlfs_info("Peer %d disconnected\n", peer->id);
		free(peer);
	}

}



struct peer_id * clone_peer(struct peer_id *input)
{
	mlfs_assert(input != 0);
	struct peer_id *cloned = (struct peer_id *)
		mlfs_zalloc(sizeof(struct peer_id));

	cloned->id = input->id;
	strncpy(cloned->ip, input->ip, NI_MAXHOST);
	cloned->pid = input->pid;
	cloned->type = input->type;
	cloned->role = input->role;
	
	for(int i=0; i<SOCK_TYPE_COUNT; i++) {
		// Initialize to -1
		cloned->sockfd[i] = -1;
	}

	cloned->sockcount = input->sockcount;
	cloned->log_sb = input->log_sb;

	return cloned;
}
struct peer_id * find_peer(int sockfd)
{
	char* ip = rc_connection_ip(sockfd);

#ifdef LIBFS
	//KernFS processes have 0 pids
	uint32_t pid = 0;
#else
	uint32_t pid = rc_connection_meta(sockfd);
#endif

	return _find_peer(ip, pid);
}

struct peer_id * _find_peer(char* ip, uint32_t pid)
{
	mlfs_debug("trying to find peer with ip %s and pid %u (peer count: %d | sock count: %d)\n",
			ip, pid, g_peer_count, g_sock_count);

	//lock_peer_access();

	// next, check any sockets for yet-to-be-registered peers
	int idx = 0;
	for(int i=0; i<g_sock_count; i++) {
		idx = find_next_bit(sock_bitmap, sock_bitmap_size, idx);
		//mlfs_assert(g_rpc_socks[idx]);
		//mlfs_assert(g_rpc_socks[idx]->peer);
		if(g_rpc_socks[idx] && g_rpc_socks[idx]->peer == 0) {
			//idx++;
			continue;
		}
		mlfs_debug("sockfd[%d]: ip %s pid %u\n", i, g_rpc_socks[idx]->peer->ip, g_rpc_socks[idx]->peer->pid);
		if(!strcmp(g_rpc_socks[idx]->peer->ip, ip) && g_rpc_socks[idx]->peer->pid == pid) {
			//unlock_peer_access();
			return g_rpc_socks[idx]->peer;
		}
		idx++;
	}

	idx = 0;
	// first check the g_peers array
	for(int i=0; i<g_peer_count; i++) {
		idx = find_next_bit(peer_bitmap, peer_bitmap_size, idx);
		//mlfs_assert(g_peers[idx]);
		//if(g_peers[i] == 0)
		//	continue;
		mlfs_debug("peer[%d]: ip %s pid %u\n", idx, g_peers[idx]->ip, g_peers[idx]->pid);
		if(!strcmp(g_peers[idx]->ip, ip) && g_peers[idx]->pid == pid) {
			//unlock_peer_access();
			return g_peers[idx];
		}
		idx++;
	}
	//unlock_peer_access();

	mlfs_debug("%s", "peer not found\n");
	return NULL;
}

static void print_bits(unsigned int x)
{
    int i;
    for(i=8*sizeof(x)-1; i>=0; i--) {
        (x & (1 << i)) ? putchar('1') : putchar('0');
    }
    printf(" <- bitmap\n");
}

/**
 * @Synopsis  It only registers peer to g_peers without register a log area. It
 * is used in the host kernfs of replica 2 when NIC-offloading is on. It is for
 * sending fsync ack to libfs after flushing log data.
 *
 * @Param peer libfs peer.
 * @Param find_id
 */
void register_peer(struct peer_id *peer, int find_id)
{
	lock_peer_access();

	uint32_t idx;
	if(find_id) {
		idx = find_first_zero_bit(peer_bitmap, peer_bitmap_size);
		mlfs_debug("peer id: %u bitmap: %lu\n", idx, peer_bitmap[0]);
		print_bits(peer_bitmap[0]);
	}
	else {
		idx = peer->id;
	}

	mlfs_printf("assigning peer (ip: %s pid: %u) to log id %d\n", peer->ip, peer->pid, idx);
	peer->id = idx;
        set_peer_id(peer);
	g_peers[peer->id] = peer;
	g_peer_count++;

	struct log_superblock *log_sb = (struct log_superblock *)
		mlfs_zalloc(sizeof(struct log_superblock));

	//read_log_superblock(peer->id, (struct log_superblock *)log_sb);

        addr_t start_blk = disk_sb[g_log_dev].log_start + idx * g_log_size + 1;
	log_sb->start_digest = start_blk;
	log_sb->start_persist = start_blk;

	//write_log_superblock(peer->id, (struct log_superblock *)log_sb);

	atomic_init(&log_sb->n_digest, 0);
	atomic_init(&log_sb->end, 0);

	peer->log_sb = log_sb;
	unlock_peer_access();

        print_g_peers();
}

void register_peer_log(struct peer_id *peer, int find_id)
{
	lock_peer_access();

	uint32_t idx;
	if(find_id) {
		idx = find_first_zero_bit(peer_bitmap, peer_bitmap_size);
		mlfs_debug("peer id: %u bitmap: %lu\n", idx, peer_bitmap[0]);
		print_bits(peer_bitmap[0]);
	}
	else {
		idx = peer->id;
	}

	mlfs_printf("assigning peer (ip: %s pid: %u) to log id %d\n", peer->ip, peer->pid, idx);
	peer->id = idx;
        set_peer_id(peer);
	g_peers[peer->id] = peer;
	g_peer_count++;

	struct log_superblock *log_sb = (struct log_superblock *)
		mlfs_zalloc(sizeof(struct log_superblock));

	//read_log_superblock(peer->id, (struct log_superblock *)log_sb);

        addr_t start_blk = disk_sb[g_log_dev].log_start + idx * g_log_size + 1;
	log_sb->start_digest = start_blk;
	log_sb->start_persist = start_blk;

	//write_log_superblock(peer->id, (struct log_superblock *)log_sb);

	atomic_init(&log_sb->n_digest, 0);
	atomic_init(&log_sb->end, 0);

	peer->log_sb = log_sb;

	//NOTE: for now, don't register logs for kernfs peers
	if(peer->type != KERNFS_PEER && peer->type != KERNFS_NIC_PEER) {
	    struct peer_id *next_rep_peer = 0, *next_digest_peer = 0;
	    int next_rep_data_sockfd[2] = {-1, -1};
	    int next_rep_msg_sockfd[2] = {-1, -1};
	    int next_digest_sockfd, next_loghdr_sockfd;
	    assert(g_kernfs_id == g_self_id); // It is kernfs.
#ifdef NIC_SIDE
	    int start_kernfs; // start of chain
	    int next_kernfs;

	    start_kernfs = host_kid_to_nic_kid(local_kernfs_id(peer->id));
	    next_kernfs = get_next_kernfs(g_kernfs_id);

	    // If the next kernfs is the last in the chain, copy to host NVM
	    // directly. Otherwise, copy to DRAM of SmartNIC.
	    if (next_kernfs_is_last(peer->id)) {
		next_rep_peer = g_kernfs_peers[nic_kid_to_host_kid(next_kernfs)];

		// 1. RPC is sent to libfs for ACK.
		next_rep_msg_sockfd[0] = peer->sockfd[SOCK_IO];

		// 2. RPC is sent to NIC-kernfs of the last replica.
                next_rep_msg_sockfd[1] =
                    g_kernfs_peers[next_kernfs]->sockfd[SOCK_IO];

		// 1. log data is copied to local host kernfs.
                next_rep_data_sockfd[0] =
                    g_kernfs_peers[nic_kid_to_host_kid(g_kernfs_id)]
                        ->sockfd[SOCK_IO];

                // 2. log data is copied to the next(last) host kernfs.
                next_rep_data_sockfd[1] =
                    g_kernfs_peers[nic_kid_to_host_kid(next_kernfs)]
			->sockfd[SOCK_IO];

	    } else if (cur_kernfs_is_last(start_kernfs, g_kernfs_id)) {
		// next_rep_peer = g_kernfs_peers[next_kernfs]; // FIXME required?
		; // No next replica.

	    } else { // first replica when # of replicas >= 3
		next_rep_peer = g_kernfs_peers[next_kernfs];

		// RPC is sent to the next NIC kernfs.
		next_rep_msg_sockfd[0] = next_rep_peer->sockfd[SOCK_IO];
		next_rep_data_sockfd[0] = next_rep_peer->sockfd[SOCK_IO];
	    }

            // Digest RPC is sent to the next NIC kernfs.
	    next_digest_peer = g_kernfs_peers[next_kernfs];
	    next_digest_sockfd = next_digest_peer->sockfd[SOCK_BG];
	    next_loghdr_sockfd = next_digest_peer->sockfd[SOCK_LH];
#else
	    next_rep_peer = g_kernfs_peers[get_next_kernfs(g_self_id)];
	    next_rep_msg_sockfd[0] = next_rep_peer->sockfd[SOCK_IO];
	    next_rep_data_sockfd[0] = next_rep_peer->sockfd[SOCK_IO];
	    next_digest_sockfd = next_rep_peer->sockfd[SOCK_BG];
	    next_loghdr_sockfd = -1; // Not used.
#endif
            init_replication(
                idx, next_rep_peer, start_blk, start_blk + g_log_size - 1,
                ((uintptr_t)g_bdev[g_log_dev]->map_base_addr), &log_sb->end,
                next_rep_data_sockfd, next_rep_msg_sockfd, next_digest_sockfd,
		next_loghdr_sockfd);
        }
	unlock_peer_access();

        print_g_peers();
}

void unregister_peer_log(struct peer_id *peer)
{
	struct replication_context *rctx;
	mlfs_assert(peer->id >= 0);

	mlfs_printf("unregistering peer (ip: %s pid: %u) with log id %d\n", peer->ip, peer->pid, peer->id);
	rctx = g_sync_ctx[peer->id];
	g_sync_ctx[peer->id] = NULL;

	//lock_peer_access();
	clear_peer_id(peer);
	g_peer_count--;
	//free(rctx->peer); // LIBFS only.
	g_peers[peer->id] = NULL;
#ifdef NIC_SIDE
	destroy_replication(rctx);

	// Libfs only. (There is no Kernfs unregisteration.)
	// if (peer->type != KERNFS_PEER && peer->type != KERNFS_NIC_PEER &&
	//     rctx) {
//		nic_slab_free(rctx->log_buf->base);
//		nic_slab_free(rctx->log_hdrs);
//		nic_slab_free(rctx->cur_loghdr_id);
//		mlfs_free(rctx->peer->log_buf);
//		mlfs_free(rctx->log_buf);
//	}

	// Alloc curcular buffer. NIC kernfs only.
//	if (mlfs_conf.m_to_n_rep_thread && is_kernfs_on_nic(g_self_id)) {
//		if (is_first_kernfs(peer->id, g_kernfs_id)) {
//			// repreq on primary.
//			mlfs_free(rctx->repreq_buf.buf);
//		} else {
//			// repmsg on replicas.
//			mlfs_free(rctx->repmsg_buf.buf);
//		}
//	}
#endif

	// TODO
	// free resources allocated at init_replication().

#ifndef HYPERLOOP
	free(rctx);
#endif
	free(peer->log_sb);
	//free(peer);
	//unlock_peer_access();
}

void set_peer_id(struct peer_id *peer)
{
	bitmap_set(peer_bitmap, peer->id, 1);
}

void clear_peer_id(struct peer_id *peer)
{
	bitmap_clear(peer_bitmap, peer->id, 1);
}

/**********************************************************
 * NIC offloading related functions.
 * ********************************************************/

/**
 * To check whether it is kernFS on SmartNIC or not.
 * If NIC_OFFLOAD is not set, return true.
 *
 * Parameter: peer id.
 * Returns:
 *      True: If the peer id is the same as the id of kernFS on SmartNIC.
 *      False: Otherwise.
 */
bool is_kernfs_on_nic (int id)
{
	// false if it is libfs.
	if (id >= g_n_nodes)
		return false;

#ifdef NIC_OFFLOAD
    assert (id >= 0); // id is not set if it is -1.

    // Even entries of g_hot_replicas are Bluefield. Ex) 0, 2
    if (id % 2 == 0)
        return true;
    else
        return false;
#else
    return true;
#endif
}

/*
 * Returns the first kernfs id in a replication chain.
 * libfs_id decides the starting kernFS in the replication chain.
 */
int first_kernfs_id(int libfs_id) {
	int local_kid;
	local_kid = local_kernfs_id(libfs_id);

	if (local_kid == -1)
		return -1;

	return host_kid_to_nic_kid(local_kid);
}

bool is_first_kernfs(int libfs_id, int current_nic_kernfs_id) {
    return current_nic_kernfs_id == first_kernfs_id(libfs_id);
}

bool is_local_kernfs(int libfs_id, int current_nic_kernfs_id) {
    return current_nic_kernfs_id == local_kernfs_id(libfs_id);
}

bool is_last_kernfs(int libfs_id, int current_nic_kernfs_id) {
    return cur_kernfs_is_last(first_kernfs_id(libfs_id), current_nic_kernfs_id);
}

bool is_middle_in_rep_chain(int libfs_id, int current_nic_kernfs_id) {
    return (!is_first_kernfs(libfs_id, current_nic_kernfs_id) &&
	    !is_last_kernfs(libfs_id, current_nic_kernfs_id));
}

bool is_last_host_kernfs(int libfs_id, int cur_host_kernfs_id)
{
	return cur_kernfs_is_last(first_kernfs_id(libfs_id),
				  host_kid_to_nic_kid(cur_host_kernfs_id));
}

/**
 * Get host kernfs id with nic kernfs id.
 */
int nic_kid_to_host_kid (int nic_kernfs_id)
{
    assert (nic_kernfs_id >= 0); // id is not set if it is -1.
#ifdef NIC_OFFLOAD
    return nic_kernfs_id + 1;     // FIXME (id of kernFS on its Host) = (id of kernFS on SmartNIC) + 1
#else
    // mlfs_info("No need to call this function if NIC_OFFLOAD is not set. Just return the same id.\n");
    return nic_kernfs_id;
#endif
}

/**
 * Get NIC kernfs id with host kernfs id.
 */
inline int host_kid_to_nic_kid (int host_kernfs_id)
{
    assert (host_kernfs_id >= 0); // id is not set if it is -1.
#ifdef NIC_OFFLOAD
    return host_kernfs_id - 1;     // FIXME (id of kernFS on SmartNIC) = (id of kernFS on its Host) - 1
#else
    // mlfs_info("No need to call this function if NIC_OFFLOAD is not set. Just return the same id.\n");
    return host_kernfs_id;
#endif
}

/**
 * Get NIC kernfs id with node id.
 */
int nodeid_to_nic_kid (int node_id)
{
#ifdef NIC_OFFLOAD
    // Ex)
    //  node_id     nic_kernfs_id   (host_kernfs_id)
    //  0           0               1
    //  1           2               3
    //  2           4               5
    return node_id*2;
#else
    return node_id;
#endif
}

/**
 * Get host kernfs id with node id.
 */
int nodeid_to_host_kid (int node_id)
{
    int ret;
#ifdef NIC_OFFLOAD
    // Ex)
    //  node_id     nic_kernfs_id   (host_kernfs_id)
    //  0           0               1
    //  1           2               3
    //  2           4               5
    ret = node_id*2+1;
    assert (ret < g_n_nodes);
    return ret;
#else
    return node_id;
#endif
}

/**********************************************************/

/**
 * Return true if current kernfs is the last kernfs of the replication chain
 * beginning from starting_kernfs.
 * Ture if a replication chain is :
 *  starting_kernfs -> kernfs 1 -> kernfs 2 -> ... -> current_kernfs
 *
 * For NIC_OFFLOAD case, note that all the kernfs ids in
 * this function are regarded as NIC kernfs's id.
 */
bool cur_kernfs_is_last(int starting_kernfs, int target_kernfs)
{
    // Previous code w/o NIC_OFFLOAD
    // int cur_kernfs_is_last = (((starting_kernfs + g_n_nodes - 1) % g_n_nodes) == g_self_id);
#ifdef NIC_OFFLOAD
    // Assume that all the nodes have two KERNFSes (Host kernfs and NIC kernfs)
    return ((starting_kernfs / 2 + g_n_replica - 1) % g_n_replica) * 2 == target_kernfs;   // 2: a pair of host kernfs and NIC kernfs.
#else
    return ((starting_kernfs + (g_n_replica - 1)) % g_n_replica) == target_kernfs; // g_n_replica-1 = rep factor-1
#endif
}

/**
 * Return true if the next kernfs is the last kernfs of the replication chain.
 * If NIC-offloading case, this function should be called by kernfs on NIC.
 */
bool next_kernfs_is_last(int libfs_id)
{
#ifdef NIC_SIDE
    int start_kernfs; // start of chain
    int next_kernfs;

    start_kernfs = host_kid_to_nic_kid(local_kernfs_id(libfs_id));
    next_kernfs = get_next_kernfs(g_kernfs_id);

    return cur_kernfs_is_last(start_kernfs, next_kernfs);
#else
    assert (false); // FIXME not implemented yet.
#endif
}

/** Get the previous kernfs id of the cur_kernfs_id in the replicaion chain.
 *  For NIC_OFFLOAD case, note that all the kernfs ids in this function are regarded as NIC kernfs's id.
 */
int get_prev_kernfs(int cur_kernfs_id)
{
#ifdef NIC_OFFLOAD
    // TODO It is assumed that all nodes have NIC kernfs.
    return ((cur_kernfs_id/2 + g_n_replica - 1) % g_n_replica) * 2; // 2 is for HOST_KERNFS and NIC_KERNFS.
#else
    return (cur_kernfs_id + g_n_nodes - 1) % g_n_nodes;
#endif
}

/** Get the next kernfs id of the cur_kernfs_id in the replicaion chain.
 *  For NIC_OFFLOAD case, note that all the kernfs ids in this function are regarded as NIC kernfs's id.
 */
int get_next_kernfs(int cur_kernfs_id)
{
#ifdef NIC_OFFLOAD
    // TODO It is assumed that all nodes have NIC kernfs.
    return ((cur_kernfs_id / 2 + 1) % g_n_replica) * 2; // 2 is for HOST_KERNFS and NIC_KERNFS.
#else
    return (cur_kernfs_id + 1) % g_n_nodes;
#endif
}

/**
 * Get the next host kernfs id in the replication chain.
 * This function is only for NIC_OFFLOAD case.
 */
int get_next_host_kernfs(int cur_nic_kernfs_id)
{
    int next_nic_kernfs_id = get_next_kernfs(cur_nic_kernfs_id);
    return nic_kid_to_host_kid(next_nic_kernfs_id);
}

/**
 * Get the last node id in the replication chain.
 */
int get_last_node_id(void)
{
    return (g_node_id + g_rsync_rf -1) % g_rsync_rf;
}

// ---- USED FOR DEBUGGING ----
void print_g_peers(void)
{
    for(int i=0; i<g_peer_count; i++)
    {
        pr_setup("g_peers[%d]: pointer %p ip %s pid %u",
                i, g_peers[i], g_peers[i]->ip, g_peers[i]->pid);
    }
}

void print_g_kernfs_peers(void)
{
    for(int i=0; i<g_peer_count; i++)
    {
        pr_setup("g_kernfs_peers[%d]: pointer %p ip %s pid %u",
                i, g_kernfs_peers[i], g_kernfs_peers[i]->ip,
                g_kernfs_peers[i]->pid);
    }
}
#endif
