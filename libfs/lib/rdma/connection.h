#ifndef RDMA_CONNECTION_H
#define RDMA_CONNECTION_H

#include <pthread.h>
#include <netdb.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>

#include "uthash.h"
#include "messaging.h"
#include "utils.h"

/*
enum mr_type
{
	MR_SYNC = 0, //memory regions of type 'MR_SYNC' are used by replication functions
	MR_CACHE,    //others can be used in any manner specified by the application
	MR_BUFFER,
};
*/

extern const int TIMEOUT_IN_MS;

//extern int *conn_bitmap;
//extern struct rdma_cm_id **conn_ids;
extern struct rdma_event_channel *g_ec;
extern struct rdma_event_channel *g_ec_low_lat;
extern int num_mrs;
extern struct mr_context *mrs;
extern int msg_size;
extern int cq_loop;
extern pthread_mutexattr_t attr;
extern pthread_mutex_t cq_lock;

//extern int archive_idx;
extern int exit_rc_loop;

// agent callbacks
typedef void(*pre_conn_cb_fn)(struct rdma_cm_id *id);
typedef void(*connect_cb_fn)(struct rdma_cm_id *id);
typedef void(*completion_cb_fn)(struct ibv_wc *wc);
typedef void(*disconnect_cb_fn)(struct rdma_cm_id *id, int low_lat);

// user callbacks
typedef void(*app_conn_cb_fn)(int sockfd, int sock_type);
typedef void(*app_disc_cb_fn)(int sockfd, int low_lat);
typedef void(*app_recv_cb_fn)(struct app_context *msg);

enum connection_state
{
	RC_CONNECTION_TERMINATED = -1,
	RC_CONNECTION_PENDING,
	RC_CONNECTION_ACTIVE,
	RC_CONNECTION_READY,

};

//rdma_cm_id hashmap value (defined below in 'context')
struct id_record {
	struct sockaddr_in addr;
	uint32_t qp_num;
	struct rdma_cm_id *id;
	UT_hash_handle addr_hh;
	UT_hash_handle qp_hh;
};

struct buffer_record {
	uint32_t wr_id;
	int buff_id;
	UT_hash_handle hh;
};

static inline uint64_t last_compl_wr_id(struct conn_context *ctx, int send)
{
	//we maintain seperate wr_ids for send/rcv queues since
	//there is no ordering between their work requests
	if(send)
		return ctx->last_send_compl;
	else
		return ctx->last_rcv_compl;
}

struct thread_arg {
	int id;
	char name[128];
};

//connection building
void build_connection(struct rdma_cm_id *id, int low_lat);
void build_shared_context(struct ibv_context *verbs);
void build_conn_context(struct rdma_cm_id *id, int polling_loop);
void build_cq_channel(struct rdma_cm_id *id, int low_lat);
void build_qp_attr(struct rdma_cm_id *id, struct ibv_qp_init_attr *qp_attr);
void build_params(struct rdma_conn_param *params);

//event handling
void event_loop(struct rdma_event_channel *ec, int exit_on_connect, int exit_on_disconnect, int low_lat);

//request completions
void update_completions(struct ibv_wc *wc);
void spin_till_response(struct rdma_cm_id *id, uint64_t seqn);
void spin_till_per_libfs_response(struct rdma_cm_id *id, uint64_t seqn, int libfs_id);
void poll_till_bit_set(struct rdma_cm_id *id, uint32_t *bit);
void spin_till_bit_set(struct rdma_cm_id *id, uint32_t *bit);
void spin_till_response_and_bit(struct rdma_cm_id *id, uint64_t seqn, uintptr_t sync_bit_addr);
void poll_till_completion(struct rdma_cm_id *id, uint32_t wr_id, int low_lat);
void spin_till_completion(struct rdma_cm_id *id, uint32_t wr_id);
void poll_cq_debug(struct ibv_cq *cq, struct ibv_wc *wc);
void poll_cq(struct ibv_cq *cq, struct ibv_wc *wc, int low_lat);
void* poll_cq_spinning_loop();
void* poll_cq_spinning_loop_global();
void* poll_low_lat_cq_spinning_loop();
void* poll_cq_blocking_loop();

//helper functions
struct rdma_cm_id* find_next_connection(struct rdma_cm_id* id);
struct rdma_cm_id* find_connection_by_addr(struct sockaddr_in *addr);
struct rdma_cm_id* find_connection_by_wc(struct ibv_wc *wc);
struct rdma_cm_id* get_connection(int sockfd);

//connection handling

//setup
void rc_init(pre_conn_cb_fn, connect_cb_fn, completion_cb_fn, disconnect_cb_fn, int low_lat);
int rc_add(struct rdma_cm_id *id, int app_type, int polling_loop, int sock_type, int low_lat);

//state
void rc_set_state(struct rdma_cm_id *id, int new_state);
int rc_ready(int sockfd);
int rc_active(int sockfd);
int rc_terminated(int sockfd);
int rc_connection_count();
int rc_low_lat_connection_count();
int rc_next_connection(int cur);
int rc_connection_meta(int sockfd);
char* rc_connection_ip(int sockfd);
struct ibv_pd * rc_get_pd();
int is_low_lat_connection(int sockfd);

//buffers
int _rc_acquire_buffer(int sockfd, void ** ptr, int user);
int rc_acquire_buffer(int sockfd, struct app_context ** ptr);
int rc_bind_buffer(struct rdma_cm_id *id, int buffer, uint32_t wr_id);
int rc_release_buffer(int sockfd, uint32_t wr_id);

//remove
void rc_disconnect(struct rdma_cm_id *id);
void rc_clear(struct rdma_cm_id *id);
void rc_die(const char *message);

// int get_type_with_addr(addr_t addr);
#endif
