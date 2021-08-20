#include <sys/syscall.h>
#include <pthread.h>
#include <stdatomic.h>
#include "agent.h"
#include <assert.h>

int rdma_initialized = 0; // TODO do we need it?

struct event_loop_arg {
    char *port;
    int low_lat;
};

void free_evt_loop_arg(struct event_loop_arg* arg) {
    if (arg->port != NULL)
	free(arg->port);
    free(arg);
}

//initialize memory region information
void init_rdma_agent(char *listen_port, struct mr_context *regions,
		int region_count, uint16_t buffer_size,
		app_conn_cb_fn app_connect,
		app_disc_cb_fn app_disconnect,
		app_recv_cb_fn app_receive, int low_lat)
{
	//pthread_mutex_lock(&global_mutex);

	if(region_count > MAX_MR)
		rc_die("region count is greater than MAX_MR");

	mrs = regions;
	num_mrs = region_count;
	msg_size = buffer_size;

	app_conn_event = app_connect;
	app_disc_event = app_disconnect;
	if (low_lat)
	    app_recv_event_low_lat = app_receive;
	else
	    app_recv_event = app_receive;

	set_seed(5);

	struct event_loop_arg *el_arg =
	    (struct event_loop_arg *)calloc(1, sizeof(struct event_loop_arg));

	el_arg->low_lat = low_lat;

	if(listen_port) {
		el_arg->port = (char*)calloc(10, sizeof(char));
		snprintf(el_arg->port, sizeof(el_arg->port), "%s", listen_port);
	} else {
		el_arg->port = NULL;
	}

#if 0
	cq_lock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(cq_lock, &attr);
	wr_lock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(wr_lock, &attr);
	wr_completed = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
#endif

	if (low_lat) {
	    rc_init(on_pre_conn,
		    on_connection,
		    on_completion_low_lat,
		    on_disconnect, low_lat);

	    g_ec_low_lat = rdma_create_event_channel();

	} else {
	    rc_init(on_pre_conn,
		    on_connection,
		    on_completion,
		    on_disconnect, low_lat);

	    g_ec = rdma_create_event_channel();
	}

        if(!listen_port)
		pthread_create(&comm_thread, NULL, client_loop, el_arg);
	else
		pthread_create(&comm_thread, NULL, server_loop, el_arg);

	rdma_initialized++;
	if (rdma_initialized > 2)
	    printf("Error incorrect RDMA initialization.\n");
	assert(rdma_initialized <= 2); // one for normal, the other for low_lat.
}

void shutdown_rdma_agent()
{
#if 0
	void *ret;
	int sockfd = -1;

	int n = rc_connection_count();

	for(int i=0; i<n; i++) {
		sockfd = rc_next_connection(sockfd);
		if(rc_terminated(sockfd) != RC_CONNECTION_TERMINATED)
			rc_disconnect(get_connection(sockfd));
	}

   	//if(pthread_join(comm_thread, &ret) != 0)
	//	rc_die("pthread_join() error");
#endif
}

static void* client_loop(void *el_arg)
{
	int low_lat = ((struct event_loop_arg*)el_arg)->low_lat;

	if (low_lat) {
	    event_loop(g_ec_low_lat, 0, 1, low_lat); /* exit upon disconnect */
	    rdma_destroy_event_channel(g_ec_low_lat);
	} else {
	    event_loop(g_ec, 0, 1, low_lat); /* exit upon disconnect */
	    rdma_destroy_event_channel(g_ec);
	}

	debug_print("exiting rc_client_loop\n");

	free_evt_loop_arg((struct event_loop_arg*)el_arg);

	return NULL;
}

static void* server_loop(void *el_arg)
{
	struct sockaddr_in6 addr;
	struct rdma_cm_id *cm_id = NULL;
	char *port = ((struct event_loop_arg *)el_arg)->port;
	int low_lat = ((struct event_loop_arg*)el_arg)->low_lat;

	memset(&addr, 0, sizeof(addr));
	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(atoi(port));

	if (low_lat) {
	    rdma_create_id(g_ec_low_lat, &cm_id, NULL, RDMA_PS_TCP);
	} else {
	    rdma_create_id(g_ec, &cm_id, NULL, RDMA_PS_TCP);
	}

	rdma_bind_addr(cm_id, (struct sockaddr *)&addr);
	rdma_listen(cm_id, 100); /* backlog=10 is arbitrary */

	printf("[RDMA-Server] Listening on port %d for connections. interrupt (^C) to exit.\n", atoi(port));

	if (low_lat) {
	    event_loop(g_ec_low_lat, 0, 0, low_lat); /* do not exit upon disconnect */
	} else {
	    event_loop(g_ec, 0, 0, low_lat); /* do not exit upon disconnect */
	}

	rdma_destroy_id(cm_id);
	if (low_lat) {
	    rdma_destroy_event_channel(g_ec_low_lat);
	} else {
	    rdma_destroy_event_channel(g_ec);
	}

	free_evt_loop_arg((struct event_loop_arg*)el_arg);

	debug_print("exiting rc_server_loop\n");

	return 0;
}

//request connection to another RDMA agent (non-blocking)
//returns socket descriptor if successful, otherwise -1
int add_connection(char* ip, char *port, int app_type, int polling_loop, int sock_type, int low_lat)
{
	struct addrinfo *addr;
	struct rdma_cm_id *cm_id = NULL;
	struct rdma_conn_param cm_params;
	int ret;
	__be32 s_type;

	debug_print("attempting to add connection to %s:%s\n", ip, port);

	if(!rdma_initialized)
		rc_die("can't add connection; client must be initialized first\n");

	getaddrinfo(ip, port, NULL, &addr);

	if (low_lat) {
	    rdma_create_id(g_ec_low_lat, &cm_id, NULL, RDMA_PS_TCP);
	} else {
	    rdma_create_id(g_ec, &cm_id, NULL, RDMA_PS_TCP);
	}

	// Add connection on client.
	int sockfd = rc_add(cm_id, app_type, polling_loop, sock_type, low_lat);

	ret = rdma_resolve_addr(cm_id, NULL, addr->ai_addr, TIMEOUT_IN_MS);
	if (ret) {
		rc_die("rdma_resolve_addr failed.\n");
	}

	struct conn_context * ctx = cm_id->context;
	sem_wait(&ctx->conn_sem);

	build_params(&cm_params);
	s_type = htobe32((uint32_t)sock_type);
	cm_params.private_data = &s_type;
	cm_params.private_data_len = sizeof(s_type);

	// Connect to server.
	ret = rdma_connect(cm_id, &cm_params);
	if (ret) {
		rc_die("rdma_connect failed.\n");
	}

	freeaddrinfo(addr);

	printf("[RDMA-Client] Creating connection (status:pending) to %s:%s on sockfd %d\n", ip, port, sockfd);

	return sockfd;
}

#if 0
struct sockaddr create_connection(char* ip, uint16_t port)
{
	struct addrinfo *addr;
	getaddrinfo(host, port, NULL, &addr);

	rdma_create_id(ec, &cm_id, NULL, RDMA_PS_TCP);
	rdma_resolve_addr(cm_id, NULL, addr->ai_addr, TIMEOUT_IN_MS);
	return addr;
}
#endif

static void on_pre_conn(struct rdma_cm_id *id)
{
	struct conn_context *ctx = (struct conn_context *)id->context;

	// if no MRs provided, trigger registeration callback
	//if(mrs == NULL)
	mr_register(ctx, mrs, num_mrs, msg_size);
	for(int i=0; i<MAX_BUFFER; i++)
		receive_message(id, i);
}

static void on_connection(struct rdma_cm_id *id)
{
	struct conn_context *ctx = (struct conn_context *)id->context;
	printf("Connection established [sockfd=%d sock_type=%d]\n", ctx->sockfd,
	       ctx->sock_type);

	if(!ibw_cmpxchg(&ctx->mr_init_sent, 0, 1)) {
		int i = create_message(id, MSG_INIT, num_mrs);
		printf("SEND --> MSG_INIT [advertising %d memory regions]\n", num_mrs);
		send_message(id, i);
	}
}

static void on_disconnect(struct rdma_cm_id *id, int low_lat)
{
	struct conn_context *ctx = (struct conn_context *)id->context;
	if (app_disc_event)
	    app_disc_event(ctx->sockfd, low_lat);
	printf("Connection terminated [sockfd:%d]\n", ctx->sockfd);
	//free(ctx);
}

static void handle_completion(struct ibv_wc *wc, int low_lat)
{
	struct rdma_cm_id *id = find_connection_by_wc(wc);
	struct conn_context *ctx = (struct conn_context *)id->context;

	if (wc->opcode & IBV_WC_RECV) {
		uint32_t rcv_i = wc->wr_id;
#ifdef MSG_BUFFER_PROFILE
		ctx->rcv_buf_posted_cnt--;
		if (ctx->rcv_buf_posted_cnt < 2) {
			printf("[BUF_PROFILE] Insufficient rcv buffer: "
			       "sockfd=%d rcv_buf_cnt=%lu (after sub)\n",
			       ctx->sockfd, ctx->rcv_buf_posted_cnt);
			rc_die("[BUF_PROFILE] Run out of posted RECV WR.");
		}
#endif

		//Immediate completions can serve as ACKs or messages
		if(wc->opcode == IBV_WC_RECV_RDMA_WITH_IMM) { 
			uint32_t app_id = ntohl(wc->imm_data);
			debug_print("application callback: seqn = %u (%x)\n", app_id, app_id);

                        if(app_id) {
                                update_pending(id, app_id);
                                struct app_context imm_msg;
                                imm_msg.id = app_id;
                                imm_msg.sockfd = ctx->sockfd;
                                imm_msg.data = 0;
				if (low_lat)
				    app_recv_event_low_lat(&imm_msg);
				else
				    app_recv_event(&imm_msg);
			}
		}
		else if (ctx->msg_rcv[rcv_i]->id == MSG_INIT) {
			if(ctx->app_type < 0)
				ctx->app_type = ctx->msg_rcv[rcv_i]->meta.mr.addr;
			ctx->remote_mr_total = ctx->msg_rcv[rcv_i]->meta.mr.length;
			ctx->mr_init_recv = 1;
			printf("RECV <-- MSG_INIT [remote node advertises %d memory regions]\n",
						ctx->remote_mr_total);

			if(!ibw_cmpxchg(&ctx->mr_init_sent, 0, 1)) {
				int send_i = create_message(id, MSG_INIT, num_mrs);
				printf("SEND --> MSG_INIT [advertising %d memory regions]\n", num_mrs);
				send_message(id, send_i);
			}

#if 0
			//if all local memory region keys haven't yet been synced, send the next
			if(!mr_all_sent(ctx)) {	
				int send_i = create_message(id, MSG_MR, 0);
				send_message(id, send_i);
				printf("%s", "SEND --> MSG_MR\n");
			}
			else {
				debug_print("finished syncing local MRs\n");
			}
#endif

			// FIXME remove. this is only for testing
			// pre-post 100 imm receives
			//for(int i=0 ; i<100; i++)
			//	receive_imm(id, 0);
		}
		else if (ctx->msg_rcv[rcv_i]->id == MSG_MR) {
			int idx = ctx->msg_rcv[rcv_i]->meta.mr.type;
			//printf("%s", "RECV <-- MSG_MR\n");
			if(idx > MAX_MR-1)
				rc_die("memory region number outside of MAX_MR");
			ctx->remote_mr[idx] = (struct mr_context *)calloc(1, sizeof(struct mr_context));
			ctx->remote_mr[idx]->type = idx;
			ctx->remote_mr[idx]->addr = ctx->msg_rcv[rcv_i]->meta.mr.addr;
			ctx->remote_mr[idx]->length = ctx->msg_rcv[rcv_i]->meta.mr.length;
			ctx->remote_mr[idx]->rkey = ctx->msg_rcv[rcv_i]->meta.mr.rkey;
			ctx->remote_mr_ready[idx] = 1;

			if(mr_all_synced(ctx)) {
				debug_print("[DEBUG] RECV COMPL - ALL SYNCED: buffer %d\n", rcv_i);
				rc_set_state(id, RC_CONNECTION_READY);
				if (app_conn_event)
				    app_conn_event(ctx->sockfd, ctx->msg_rcv[rcv_i]->meta.mr.sock_type);
			}

			//send an ACK
			//create_message(id, MSG_MR_ACK);
			//send_message(id);
		}
		else if (ctx->msg_rcv[rcv_i]->id == MSG_READY) {
			//do nothing
		}
		else if (ctx->msg_rcv[rcv_i]->id == MSG_DONE) {
			rc_disconnect(id);
			return;
		}
		else if (ctx->msg_rcv[rcv_i]->id == MSG_CUSTOM) {
			ctx->msg_rcv[rcv_i]->meta.app.sockfd = ctx->sockfd;

			//adding convenience pointers to data blocks
			ctx->msg_rcv[rcv_i]->meta.app.data = ctx->msg_rcv[rcv_i]->data;

			uint64_t app_id = ctx->msg_rcv[rcv_i]->meta.app.id;

			// If it has per-libfs seqn, check and update per-libfs state.
			int libfs_id = ctx->msg_rcv[rcv_i]->meta.app.libfs_id;
			uint64_t last_rcv_compl;

			// printf("%lu libfs_id=%d app_id=%lu sockfd=%d msg=%s\n", get_tid(),
			//                 libfs_id, app_id, ctx->msg_rcv[rcv_i]->meta.app.sockfd,
			//                 ctx->msg_rcv[rcv_i]->meta.app.data);

			if (libfs_id)  // per-libfs seqn.
			    last_rcv_compl = ctx->per_libfs_last_rcv_compl[libfs_id];
			else
			    last_rcv_compl = ctx->last_rcv_compl;

#ifdef CHECK_SEQN_INCREASE_BY_ONE
			if (app_id && app_id != last_rcv_compl + 1) {
				printf("%lu [Error] seqn does not increase by one."
				       "libfs_id=%d seqn(prev=%lu current=%lu) "
				       "msg=%s sockfd=%d\n",
				       get_tid(), libfs_id, last_rcv_compl,
				       app_id,
				       ctx->msg_rcv[rcv_i]->meta.app.data,
				       ctx->msg_rcv[rcv_i]->meta.app.sockfd);
#ifdef PRINT_PREV_MSG
				printf("%lu [Error] prev_msg=%s\n", get_tid(),
				       ctx->prev_app_data);
#endif
				rc_die("Invalid sequence number: Not increasing by one.\n");
			}
#endif /* CHECK_SEQN_INCREASE_BY_ONE */

			// FIXME Duplicate messages are delivered (repmsg) if async replication is enabled.
			if (app_id && app_id == last_rcv_compl) {
#ifdef CHECK_DUP_MSG
				printf("%lu [Warn] Duplicate message. Ignore "
				       "it. "
				       "libfs_id=%d seqn(prev=%lu current=%lu) "
				       "cur_msg=%s sockfd=%d\n",
				       get_tid(), libfs_id, last_rcv_compl,
				       app_id,
				       ctx->msg_rcv[rcv_i]->meta.app.data,
				       ctx->msg_rcv[rcv_i]->meta.app.sockfd);
#ifdef PRINT_PREV_MSG
				printf("%lu [Warn] prev_msg=%s\n", get_tid(),
				       ctx->prev_app_data);
#endif
#ifdef DIE_ON_DUP_MSG
			    rc_die("Duplicate message(seqn).");
#endif
#endif /* CHECK_DUP_MSG */
			}
#ifdef CHECK_INCORRECT_SEQN
			else if (app_id && app_id < last_rcv_compl) {
				printf("%lu [Error] Incorrect order of "
				       "delivery. "
				       "libfs_id=%d seqn(prev=%lu current=%lu) "
				       "msg=%s sockfd=%d\n",
				       get_tid(), libfs_id, last_rcv_compl,
				       app_id,
				       ctx->msg_rcv[rcv_i]->meta.app.data,
				       ctx->msg_rcv[rcv_i]->meta.app.sockfd);
#ifdef PRINT_PREV_MSG
				printf("%lu [Error] prev_msg=%s\n", get_tid(),
				       ctx->prev_app_data);
#endif
#ifdef DIE_ON_INCORRECT_SEQN
				rc_die("Incorrect order of delivery(seqn).");
#endif
			}
#endif /* CHECK_INCORRECT_SEQN */
			else {
				debug_print("application callback: seqn = %lu, "
					    "rcv_i = %u\n",
					    app_id, rcv_i);
#ifdef PRINT_PREV_MSG
			    memcpy(ctx->prev_app_data,
				   ctx->msg_rcv[rcv_i]->meta.app.data,
				   msg_size);
#endif
			    //debug_print("trigger application callback\n");
			    if (low_lat)
				app_recv_event_low_lat(&ctx->msg_rcv[rcv_i]->meta.app);
			    else
				app_recv_event(&ctx->msg_rcv[rcv_i]->meta.app);

			    //only trigger delivery notifications for app_ids greater than 0
			    if(app_id) {
				if (libfs_id)
				    update_per_libfs_pending(id, app_id, libfs_id);
				else
				    update_pending(id, app_id);
			    }
			}
		}
		else {
			printf("invalid completion event with undefined id (%i) \n", ctx->msg_rcv[rcv_i]->id);
			rc_die("");
		}

		receive_message(id, rcv_i);
#if 0
		if(ctx->poll_enable)
			receive_message(id, rcv_i);
		else
			receive_imm(id, rcv_i);
#endif
	}
	else if(wc->opcode == IBV_WC_RDMA_WRITE) {
	}
	else if(wc->opcode == IBV_WC_SEND) {
		int i = rc_release_buffer(ctx->sockfd, wc->wr_id);
		if (ctx->msg_send[i]->id == MSG_INIT || ctx->msg_send[i]->id == MSG_MR) {
			//printf("received MSG_MR_ACK\n");
			if(ctx->msg_send[i]->id == MSG_MR) {
				int idx = ctx->msg_send[i]->meta.mr.type;
				ctx->local_mr_sent[idx] = 1;
			}

			//if all local memory region keys haven't yet been synced, send the next
			if(!mr_all_sent(ctx)) {
				int j = create_message(id, MSG_MR, 0);
				send_message(id, j);
				//printf("%s", "SEND --> MSG_MR\n");
			}
			else if(mr_all_synced(ctx)) {
					debug_print("[DEBUG] SEND COMPL - ALL SYNCED: wr_id %lu buffer %d\n", wc->wr_id, i);
					rc_set_state(id, RC_CONNECTION_READY);
					if (app_conn_event)
					    app_conn_event(ctx->sockfd, ctx->sock_type);
			}
		}
	}
	else {
		//debug_print("skipping message with opcode:%i, wr_id:%lu \n", wc->opcode, wc->wr_id);
		return;
	}
}

static void on_completion(struct ibv_wc *wc)
{
    handle_completion(wc, 0);
}

static void on_completion_low_lat(struct ibv_wc *wc)
{
    handle_completion(wc, 1);
}

