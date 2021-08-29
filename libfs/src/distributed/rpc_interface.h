#ifndef _RPC_INTERFACE_H_
#define _RPC_INTERFACE_H_

#ifdef DISTRIBUTED
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "concurrency/synchronization.h"
#include "filesystem/shared.h"
#include "global/global.h"
#include "global/types.h"
#include "global/util.h"
#include "ds/list.h"
#include "ds/stdatomic.h"
#include "agent.h"
#include "replication.h"
#include "peer.h"

// Replica IP Mappings --------------------------------------------

static struct peer_id hot_replicas[g_n_hot_rep] = {
#ifdef NIC_OFFLOAD
        // 1st machine - Bluefield NIC
	{ .ip = "192.168.13.114", .role = HOT_REPLICA, .type = KERNFS_NIC_PEER},
        // 1st machine - Host
	{ .ip = "192.168.13.113", .role = HOT_REPLICA, .type = KERNFS_PEER},
        // 2nd machine - Bluefield NIC
        { .ip = "192.168.13.118", .role = HOT_REPLICA, .type = KERNFS_NIC_PEER},
        // 2nd machine - Host
        { .ip = "192.168.13.117", .role = HOT_REPLICA, .type = KERNFS_PEER},
        // 3rd machine - Bluefield NIC
	{ .ip = "192.168.13.116", .role = HOT_REPLICA, .type = KERNFS_NIC_PEER},
        // 3rd machine - Host
	{ .ip = "192.168.13.115", .role = HOT_REPLICA, .type = KERNFS_PEER},

#else   // No offloading

	// 1st machine
	{ .ip = "192.168.13.113", .role = HOT_REPLICA, .type = KERNFS_PEER },
	// 2nd machine
	{ .ip = "192.168.13.117", .role = HOT_REPLICA, .type = KERNFS_PEER },
	// 3rd machine
	{ .ip = "192.168.13.115", .role = HOT_REPLICA, .type = KERNFS_PEER },

//	// 1st machine
//	{ .ip = "192.168.14.113", .role = HOT_REPLICA, .type = KERNFS_PEER },
//	// 2nd machine
//	{ .ip = "192.168.14.117", .role = HOT_REPLICA, .type = KERNFS_PEER },
//	// 3rd machine
//	{ .ip = "192.168.14.115", .role = HOT_REPLICA, .type = KERNFS_PEER },

#endif

};

static struct peer_id hot_backups[g_n_hot_bkp] = {
//	{ .ip = "172.17.15.4", .role = HOT_BACKUP, .type = KERNFS_PEER},
};

static struct peer_id cold_backups[g_n_cold_bkp] = {
//	{ .ip = "172.17.15.6", .role = COLD_BACKUP, .type = KERNFS_PEER},
};

static struct peer_id external_replicas[g_n_ext_rep + 1] = {
//	{ .ip = "172.17.15.10", .role = LOCAL_NODE, .type = KERNFS_PEER, .namespace_id = "sdp5", .inum_prefix = (1<<31) },
//	{ .ip = "172.17.15.8", .role = EXTERNAL_NODE, .type = KERNFS_PEER, .namespace_id = "sdp4", .inum_prefix = (1<<30) }
};

// ------------------------------------------------------------------

extern char g_self_ip[NI_MAXHOST];
extern int g_self_id;
extern int g_kernfs_id;
extern int g_node_id;
extern int g_host_node_id;

static inline void set_self_ip(void)
{
	printf("%s\n", "fetching node's IP address..");
#ifdef NIC_SIDE
	// arm
	fetch_intf_ip(mlfs_conf.arm_net_interface_name, g_self_ip);
	printf("ip address on interface \'%s\' is %s\n",
	       mlfs_conf.arm_net_interface_name, g_self_ip);
#else
	// x86
	fetch_intf_ip(mlfs_conf.x86_net_interface_name, g_self_ip);
	printf("ip address on interface \'%s\' is %s\n",
	       mlfs_conf.x86_net_interface_name, g_self_ip);
#endif
}

static inline void set_self_node_id(void)
{
    for(int i = 0; i < g_n_nodes; i++) {
        if (!strcmp(hot_replicas[i].ip, g_self_ip)) {
#ifdef NIC_OFFLOAD
            g_node_id = i / 2;
#ifdef NIC_SIDE
            if (i + 1 == g_n_nodes)
                panic("NIC kernfs cannot be the last entry in hot_replicas.\n");
            g_host_node_id = i + 1;   // NIC entry should be followed by host entry
	    pr_setup("g_node_id=%d g_host_node_id=%d\n",
		    g_node_id,
		    g_host_node_id);
#else
            pr_setup ("g_node_id: %d\n", i);
#endif /* NIC_SIDE */
#else
            g_node_id = i;
            pr_setup ("g_node_id: %d\n", i);
#endif
        }
    }
}

/**
 * msg: output string.
 * rep: input data.
 */
static inline void rpcmsg_build_replicate (char* msg, struct rpcmsg_replicate* rep)
{
    START_TIMER(evt_rep_build_msg);
    sprintf(msg, "|repmsg |%d|%lu|%lu|%u|%d|%d|%d|%d|%lu|%lu|",
            rep->libfs_id,
            rep->common.seqn,
            rep->n_log_blks,
            rep->n_log_hdrs,
            rep->ack,
            rep->persist,
            rep->steps,
	    rep->inc_avail_ver,
	    rep->end_blknr,
	    rep->ack_bit_p);
    END_TIMER(evt_rep_build_msg);
}

/**
 * msg: input string.
 * rep: output data.
 */
static inline void rpcmsg_parse_replicate (char* msg, struct rpcmsg_replicate* rep)
{
    char cmd_hdr[12];
    sscanf(msg, "|%s |%d|%lu|%lu|%u|%d|%d|%d|%d|%lu|%lu|",
            cmd_hdr,
            &rep->libfs_id,
            &rep->common.seqn,
            &rep->n_log_blks,
            &rep->n_log_hdrs,
            &rep->ack,
            &rep->persist,
            &rep->steps,
	    &rep->inc_avail_ver,
	    &rep->end_blknr,
	    &rep->ack_bit_p);
    rep->is_imm = 0; // not used currently.
    atomic_store(&rep->processed, 0);
}

/**
 * msg: input string.
 * rth_arg: output data.
 */
static inline void rpcmsg_parse_repreq (char* msg, struct rep_th_arg* rth_arg)
{
	char cmd_hdr[12];
	sscanf(msg, "|%s |%d|%d|%lu|%lu|%lu|%u|%lu|%u|%d|%lu|%d|%lu|%lu|",
	       cmd_hdr,
	       &rth_arg->libfs, // Libfs id.
	       &rth_arg->triggered_by_digest, // Used for latency breakdown.
	       &rth_arg->base_addr, // base address of log in libfs.

	       &rth_arg->n_unsync_blk, // # of unsynced blocks
	       &rth_arg->n_blk_prefetch_requested, // # of prefetched blks
	       &rth_arg->n_unsync, // # of unsynced block groups (loghdrs)

	       &rth_arg->end, // end block num of current version
	       &rth_arg->n_blk, // # of blocks (Used only in async call)
	       &rth_arg->inc_avail_ver, // Increase avail version.

	       &rth_arg->seqn, // Sequence number libfs is going to wait for
	       &rth_arg->ack, // send ack to libfs.
	       &rth_arg->is_meta_updated_p, // Address of meta_updated bit.
					    // (passed to libfs again)
	       &rth_arg->is_ack_received); // Address of ack_received bit.
}

static inline void print_rpcmsg_replicate (struct rpcmsg_replicate* rep, const char *parent)
{
    pr_rep("%s RPCMSG(repmsg) seqn=%lu libfs_id=%d n_log_blks=%lu ack=%d steps=%d "
	    "persist=%d inc_avail_ver=%d end_blknr=%lu is_imm=%d "
	    "ack_bit_p=%lu(0x%lx)",
	    parent,
	    rep->common.seqn, rep->libfs_id, rep->n_log_blks, rep->ack,
	    rep->steps, rep->persist, rep->inc_avail_ver, rep->end_blknr,
	    rep->is_imm, rep->ack_bit_p, rep->ack_bit_p);
}

static inline uint64_t generate_rpc_seqn(struct peer_socket *sock)
{
    uint64_t ret;

    pthread_spin_lock(&sock->seqn_lock);
    ret = ++sock->seqn;
    // printf("[SEQN] sockfd=%d seqn=%lu issued.\n", sock->fd, ret);
    pthread_spin_unlock(&sock->seqn_lock);

    return ret;
}

static inline uint64_t generate_rpc_per_libfs_seqn(struct peer_socket *sock, int libfs_id)
{
    uint64_t ret;

    pthread_spin_lock(&sock->seqn_lock);
    ret = ++sock->per_libfs_seqns[libfs_id];
    pthread_spin_unlock(&sock->seqn_lock);

    return ret;
}

//static inline rdma_meta_t * create_rdma_meta_sg (uintptr_t dst, uint64_t n_sg)
//{
//	rdma_meta_t *meta = (rdma_meta_t*) mlfs_zalloc(sizeof(rdma_meta_t) + n_sg * sizeof(struct ibv_sge));
//	meta->addr = dst;
//	// meta->length = io_size;      // need to set in caller.
//	meta->sge_count = n_sg;
//	// meta->sge_entries[0].addr = src;     // need to set in caller.
//	// meta->sge_entries[0].length = io_size;       // need to set in caller.
//	return meta;
//}

static inline rdma_meta_t * create_rdma_meta(uintptr_t src,
		uintptr_t dst, uint32_t io_size)
{
	rdma_meta_t *meta = (rdma_meta_t*) mlfs_zalloc(sizeof(rdma_meta_t) + sizeof(struct ibv_sge));
	meta->addr = dst;
	meta->length = io_size;
	meta->sge_count = 1;
	meta->sge_entries[0].addr = src;
	meta->sge_entries[0].length = io_size;
	return meta;
}

// Alloc rdma meta with 1 sge.
static inline rdma_meta_t *alloc_single_sge_rdma_meta(void) {
	return (rdma_meta_t*) mlfs_zalloc(sizeof(rdma_meta_t) + sizeof(struct ibv_sge));
}

// Alloc rdma meta with max sge buffer.
static inline rdma_meta_t * alloc_rdma_meta(void)
{
	return (rdma_meta_t*) mlfs_zalloc(sizeof(rdma_meta_t) + MAX_SEND_SGE_SIZE * sizeof(struct ibv_sge));
}

// Reset rdma meta that stores MAX_SEND_SGE_SIZE sge entries.
static inline void reset_sge_rdma_meta(rdma_meta_t *meta)
{
	memset(meta, 0, sizeof(rdma_meta_t) + MAX_SEND_SGE_SIZE * sizeof(struct ibv_sge));
}

static inline void set_rdma_meta_dst(rdma_meta_t *meta, uintptr_t dst)
{
	meta->addr = dst;
}

// To reuse rdma_meta.
static inline void reset_single_sge_rdma_meta(uintptr_t src, uintptr_t dst,
					    uint32_t io_size, rdma_meta_t *meta)
{
	memset(meta, 0, sizeof(rdma_meta_t) + sizeof(struct ibv_sge));

	meta->addr = dst;
	meta->length = io_size;
	meta->sge_count = 1;
	meta->sge_entries[0].addr = src;
	meta->sge_entries[0].length = io_size;
}

/**
 * @Synopsis  Add an sge entry to rdma meta.
 *
 * @Param meta
 * @Param src
 * @Param io_size
 */
static inline void add_sge_entry(rdma_meta_t *meta, uintptr_t src,
				 uint32_t io_size)
{
	int id = meta->sge_count;

	meta->length += io_size;
	meta->sge_entries[id].addr = src;
	meta->sge_entries[id].length = io_size;
	meta->sge_count++;

	// printf("%lu add_sge_entry: meta=%p id=%d addr=0x%lx length=%u cnt=%d\n",
	//        get_tid(), meta, id, meta->sge_entries[id].addr,
	//        meta->sge_entries[id].length, meta->sge_count);
}

static inline struct rdma_meta_entry*
create_rdma_entry_sg (uintptr_t dst, uint64_t n_sg)
{
	struct rdma_meta_entry *rdma_entry = (struct rdma_meta_entry *)
		mlfs_zalloc(sizeof(struct rdma_meta_entry));
	rdma_entry->meta = (rdma_meta_t*) mlfs_zalloc(sizeof(rdma_meta_t) + n_sg * sizeof(struct ibv_sge));
	rdma_entry->meta->addr = dst;
	// rdma_entry->meta->length = io_size;          // need to set in caller.
	rdma_entry->meta->sge_count = n_sg;
	// rdma_entry->meta->sge_entries[0].addr = src;         // need to set in caller.
	// rdma_entry->meta->sge_entries[0].length = io_size;   // need to set in caller.
	// rdma_entry->local_mr = local_mr;             // Not used. MR is designated at IBV_WRAPPER_*
	// rdma_entry->remote_mr = remote_mr;           // Not used. MR is designated at IBV_WRAPPER_*
	INIT_LIST_HEAD(&rdma_entry->head);
	return rdma_entry;
}

static inline struct rdma_meta_entry * create_rdma_entry(uintptr_t src,
		uintptr_t dst, uint32_t io_size, int local_mr, int remote_mr)
{
	struct rdma_meta_entry *rdma_entry = (struct rdma_meta_entry *)
		mlfs_zalloc(sizeof(struct rdma_meta_entry));
	rdma_entry->meta = (rdma_meta_t*) mlfs_zalloc(sizeof(rdma_meta_t) + sizeof(struct ibv_sge));
	rdma_entry->meta->addr = dst;
	rdma_entry->meta->length = io_size;
	rdma_entry->meta->sge_count = 1;
	rdma_entry->meta->sge_entries[0].addr = src;
	rdma_entry->meta->sge_entries[0].length = io_size;
	rdma_entry->local_mr = local_mr;
	rdma_entry->remote_mr = remote_mr;
	INIT_LIST_HEAD(&rdma_entry->head);
	return rdma_entry;
}

static inline rdma_meta_t * create_rdma_ack()
{
	rdma_meta_t *meta = (rdma_meta_t*) mlfs_zalloc(sizeof(rdma_meta_t) + sizeof(struct ibv_sge));
	meta->addr = (uintptr_t)0;
	meta->length = 0;
	meta->sge_count = 0;
	meta->next = 0;
	return meta;

}


static inline uint32_t translate_namespace_inum(uint32_t inum, char *path_id)
{
	for(int i = 0; i < g_n_ext_rep + 1; i++) {
		if (!strncmp(external_replicas[i].namespace_id, path_id, DIRSIZ))
			return (inum | external_replicas[i].inum_prefix);
	}

	mlfs_printf("Error: didn't find matching namespace for id '%s'\n", path_id);
	panic("namespace parsing failed");

	return 0;
}

/* Used for initialization of NIC_OFFLOAD case. */
#ifdef NIC_OFFLOAD
#define TCP_BUF_SIZE 4096   // bitmap block request requires 4096.
#define TCP_HDR_SIZE 50
int tcp_setup_client (int *cli_sock_fd);
int tcp_setup_server (int *serv_sock_fd, int *cli_sock_fd);
void tcp_client_req_sb(int serv_sock_fd, char *data, int dev);
void tcp_client_req_root_inode(int serv_sock_fd, char *data);
void tcp_client_req_bitmap_block(int sock_fd, char *data, int dev, mlfs_fsblk_t blk_nr);
int tcp_client_req_close (int sock_fd);
void tcp_close(int sock_fd);
void tcp_send_client_resp (int sock_fd, char *buf, int len);
void tcp_client_req_dev_base_addr(int sock_fd, char *data, int devid);
#endif

int init_rpc(struct mr_context *regions, int n_regions, char *port, signal_cb_fn callback, signal_cb_fn low_lat_callback);
int shutdown_rpc();

int rpc_connect(struct peer_id *peer, char *listen_port, int type, int poll_cq);
int rpc_listen(int sockfd, int count);
void rpc_add_socket(int sockfd);
void rpc_remove_socket(int sockfd);
int rpc_bootstrap(int sockfd);
int rpc_bootstrap_response(int sockfd, uint64_t seq_n);
void rpc_register_log(int sockfd, struct peer_id *peer, uint64_t *ack_bit_p);
void rpc_register_remote_libfs_peer(int sockfd, struct peer_id *peer, uint64_t *ack_bit_p);
int rpc_register_log_response(int sockfd, uint64_t seq_n);
struct rpc_pending_io * rpc_remote_read_async(char *path, loff_t offset, uint32_t io_size, uint8_t *dst, int rpc_wait);
struct rpc_pending_io * rpc_remote_digest_async(int peer_id, peer_meta_t *peer, uint32_t n_digest, uint32_t n_blk_digest, int rpc_wait);
int rpc_remote_read_sync(char *path, loff_t offset, uint32_t io_size, uint8_t *dst);
int rpc_remote_read_response(int sockfd, rdma_meta_t *meta, int mr_local, uint64_t seqn);
int rpc_remote_digest_response(int sockfd, int id, int dev, addr_t start_digest, int n_digested, int rotated, uint64_t seq_n, uint32_t n_digested_blks);
int rpc_forward_msg(int sockfd, char* data);
uint32_t rpc_forward_msg_with_per_libfs_seqn(int sockfd, char* data, int libfs_id);
int rpc_forward_msg_sync(int sockfd, char* data);
int rpc_forward_msg_no_seqn(int sockfd, char* data);
int rpc_forward_msg_no_seqn_sync(int sockfd, char* data);
int rpc_forward_msg_and_wait_cq(int sockfd, char* data);
int rpc_forward_msg_with_per_libfs_seqn_and_wait_cq(int sockfd, char* data,
						    int libfs_id);
// int rpc_forward_msg_and_wait_resp(int sockfd, char* data);
int rpc_forward_msg_with_per_libfs_seqn_and_wait_resp(int sockfd, char* data,
						    int libfs_id);
int rpc_setup_msg (struct app_context **app, int sockfd);
int rpc_send_msg_sync (int sockfd, int buffer_id, struct app_context *msg);
int rpc_send_msg_and_wait_ack (int sockfd, char *data, uint64_t *ack_bit_p);
int rpc_nicrpc_msg(int sockfd, char* data, bool sync);
int rpc_nicrpc_setup_response(int sockfd, char *data, uint64_t seq_n);
void rpc_nicrpc_send_response(int sockfd, int buffer_id);
int rpc_lease_change(int mid, int rid, uint32_t inum, int type, uint32_t version, addr_t blknr, int sync);
int rpc_lease_response(int sockfd, uint64_t seqn, int replicate);
int rpc_lease_invalid(int sockfd, int peer_id, uint32_t inum, uint64_t seqn);
int rpc_lease_flush_response(int sockfd, uint64_t seq_n, uint32_t inum, uint32_t version, addr_t blknr);
int rpc_lease_flush(int peer_id, uint32_t inum, int force_digest);
int rpc_lease_migrate(int peer_id, uint32_t inum, uint32_t kernfs_id);
int rpc_replicate_log_sync(peer_meta_t *peer, struct list_head *rdma_entries, uint32_t imm);
void rpc_replicate_log_initiate (peer_meta_t *peer, struct list_head *rdma_entries);
void rpc_replicate_log_relay (peer_meta_t *peer, struct list_head *rdma_entries, struct rpcmsg_replicate *rep);
int rpc_send_lease_ack(int sockfd, uint64_t seqn);
struct mlfs_reply * rpc_get_reply_object(int sockfd, uint8_t *dst, uint64_t seqn);
void rpc_await(struct rpc_pending_io *pending);
static void rpc_send_replicate_rdma_entry(peer_meta_t *peer, struct rdma_meta_entry *rdma_entry,
        int send_sockfd, struct rpcmsg_replicate *rep);
#ifdef NO_BUSY_WAIT
void rpc_send_replicate_ack(int sockfd, uint64_t ack_bit_addr);
#endif
void rpc_set_remote_bit(int sockfd, uintptr_t remote_bit_addr);
void rpc_write_remote_val8(int sockfd, uintptr_t remote_addr, uint8_t val);
void rpc_write_remote_val64(int sockfd, uintptr_t remote_addr, uint64_t val);
void rpc_send_nic_log_buf_addr(int sockfd, int libfs_id);
void rpc_send_pipeline_kernfs_meta_to_next_replica(int sockfd);
uint64_t *rpc_alloc_ack_bit(void);
void rpc_wait_ack(uint64_t *ack_bit_p);
int rpc_check_ack_bit(uint64_t *ack_bit_p);
void rpc_free_ack_bit(uint64_t *ack_bit_p);
// void rpc_send_memcpy_response(int sockfd, uint64_t seqn, int libfs_id);
int rpc_request_host_memcpy_buffer_addr(int sockfd);
int rpc_send_host_memcpy_buffer_addr(int sockfd, uintptr_t buf_addr);
void rpc_send_heartbeat(int sockfd, uint64_t seqn);

int peer_sockfd(int node_id, int type);
void print_peer_id(struct peer_id *peer);
void print_rpc_setup(void);

/* RPC message protocol */
#define TO_STR(x) __STR_VALUE(x)
#define __STR_VALUE(x) #x

#define RPC_LOG_COPY_DONE	    cd
#define RPC_FETCH_LOGHDR	    fh
#define RPC_FETCH_LOG		    fl
#define RPC_HEARTBEAT		    hb
#define RPC_LOG_PREFETCH	    lp
#define RPC_PUBLISH_ACK		    complete
#define RPC_PIPELINE_KERNFS_META    pk
#define RPC_PERSIST_LOG		    pl
#define RPC_PUBLISH_REMAINS	    pr
#define RPC_REGISTER_REMOTE_LIBFS_PEER    rp
#define RPC_PRINT_BREAKDOWN_TIMER   tp
#define RPC_RESET_BREAKDOWN_TIMER   tr
#define RPC_MEMCPY_BUF_ADDR	    ma
#define RPC_REQ_MEMCPY_BUF	    mb
#define RPC_MEMCPY_REQ		    mr
// #define RPC_MEMCPY_COMPLETE	    mc
#define RPC_LEASE_ACK		    la
#ifdef NO_BUSY_WAIT
#define RPC_REPLICATE_ACK	    ra
#endif
// Add more...

#endif

#endif
