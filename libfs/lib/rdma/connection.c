#include "connection.h"
#include <assert.h>
#include <sys/prctl.h>

//event channel. used to setup rdma RCs and communicate mr keys
struct rdma_event_channel *g_ec = NULL;
struct rdma_event_channel *g_ec_low_lat = NULL;

//local memory regions (for rdma reads/writes)
int num_mrs;
struct mr_context *mrs = NULL;

int msg_size; //msg buffer size

pthread_mutexattr_t attr;
pthread_mutex_t cq_lock;

const int TIMEOUT_IN_MS = 500;

//static uint32_t cqe_archive[ARCHIVE_SIZE] = {0};
static struct context *s_ctx = NULL;
static int *s_conn_bitmap = NULL;
static int *s_conn_low_lat_bitmap = NULL;
static struct rdma_cm_id **s_conn_ids = NULL;
static pre_conn_cb_fn s_on_pre_conn_cb = NULL;
static connect_cb_fn s_on_connect_cb = NULL;
static completion_cb_fn s_on_completion_cb = NULL;
static completion_cb_fn s_low_lat_on_completion_cb = NULL;
static disconnect_cb_fn s_on_disconnect_cb = NULL;

#ifdef NIC_OFFLOAD
static pthread_t *g_cq_polling_thread = NULL;
static pthread_t *g_low_lat_cq_polling_thread = NULL;
#endif

int exit_rc_loop = 0;

#ifdef PRINT_ASYNC_EVENT
void *poll_async_events(void *con);
void init_async_event_print_thread(struct ibv_context *i_ctx, struct rdma_cm_id *id)
{
    struct conn_context *ctx = (struct conn_context *)id->context;
    pthread_create(&ctx->async_event_polling_thread, NULL, poll_async_events, i_ctx);
}
#endif /* PRINT_ASYNC_EVENT */

__attribute__((visibility ("hidden"))) 
void build_connection(struct rdma_cm_id *id, int low_lat)
{
	struct ibv_qp_init_attr qp_attr;
        int ret;

	build_shared_context(id->verbs);

	build_cq_channel(id, low_lat);

#ifdef PRINT_ASYNC_EVENT
        init_async_event_print_thread(s_ctx->ctx, id);
#endif

	build_qp_attr(id, &qp_attr);

	ret = rdma_create_qp(id, s_ctx->pd, &qp_attr);
        if (ret < 0)
	    // Do it once again (after destroying qp).
	    ret = rdma_create_qp(id, s_ctx->pd, &qp_attr);

        if (ret < 0) {
	    printf("rdma_create_qp ret=%d\n", ret);
            rc_die("rdma_create_qp failed.\n");
	}

	// update connection hash tables
	struct id_record *entry = (struct id_record *)calloc(1, sizeof(struct id_record));
	struct sockaddr_in *addr_p = copy_ipv4_sockaddr(&id->route.addr.src_storage);

	if(addr_p == NULL)
		rc_die("compatibility issue: can't use a non-IPv4 address");

	entry->addr = *addr_p;
	entry->qp_num = id->qp->qp_num;
	entry->id = id;

	//add the structure to both hash tables
	HASH_ADD(qp_hh, s_ctx->id_by_qp, qp_num, sizeof(uint32_t), entry);
	HASH_ADD(addr_hh, s_ctx->id_by_addr, addr, sizeof(struct sockaddr_in), entry);
}

__attribute__((visibility ("hidden"))) 
void build_shared_context(struct ibv_context *verbs)
{
	if (s_ctx) {
		if (s_ctx->ctx != verbs)
			rc_die("cannot handle events in more than one device context.");
		return;
	}

	s_ctx = (struct context *)malloc(sizeof(struct context));

	s_ctx->ctx = verbs;
	s_ctx->pd = ibv_alloc_pd(s_ctx->ctx);
	s_ctx->id_by_addr = NULL;
	s_ctx->id_by_qp = NULL;
}

__attribute__((visibility ("hidden"))) 
void build_conn_context(struct rdma_cm_id *id, int always_poll)
{
	struct conn_context *ctx = (struct conn_context *)calloc(1, sizeof(struct conn_context));
	ctx->local_mr = (struct ibv_mr **)calloc(MAX_MR, sizeof(struct ibv_mr*));
	ctx->remote_mr = (struct mr_context **)calloc(MAX_MR, sizeof(struct mr_context*));
	ctx->remote_mr_ready = (int *)calloc(MAX_MR, sizeof(int));
	ctx->local_mr_ready = (int *)calloc(MAX_MR, sizeof(int));
	ctx->local_mr_sent = (int *)calloc(MAX_MR, sizeof(int));
	ctx->msg_send_mr = (struct ibv_mr **)calloc(MAX_BUFFER, sizeof(struct ibv_mr*));
	ctx->msg_rcv_mr = (struct ibv_mr **)calloc(MAX_BUFFER, sizeof(struct ibv_mr*));
	ctx->msg_send = (struct message **)calloc(MAX_BUFFER, sizeof(struct message*));
	ctx->msg_rcv = (struct message **)calloc(MAX_BUFFER, sizeof(struct message*));
	ctx->send_slots = (int *)calloc(MAX_BUFFER, sizeof(int));
	ctx->pendings = NULL;
	ctx->buffer_bindings = NULL;
	ctx->poll_permission = 1;
	ctx->poll_always = always_poll;
	ctx->poll_enable = 1;

	id->context = ctx;
	ctx->id = id;

	pthread_mutex_init(&ctx->wr_lock, NULL);
	pthread_cond_init(&ctx->wr_completed, NULL);
	pthread_spin_init(&ctx->post_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&ctx->buffer_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&ctx->acquire_buf_lock, PTHREAD_PROCESS_PRIVATE);
}

/* helper function to print the content of the async event */
static void print_async_event(struct ibv_context *ctx,
                  struct ibv_async_event *event)
{
    switch (event->event_type) {
    /* QP events */
    case IBV_EVENT_QP_FATAL:
        printf("[ASYNC_EVENT] QP fatal event for QP with handle %p\n", event->element.qp);
        break;
    case IBV_EVENT_QP_REQ_ERR:
        printf("[ASYNC_EVENT] QP Requestor error for QP with handle %p\n", event->element.qp);
        break;
    case IBV_EVENT_QP_ACCESS_ERR:
        printf("[ASYNC_EVENT] QP access error event for QP with handle %p\n", event->element.qp);
        break;
    case IBV_EVENT_COMM_EST:
        printf("[ASYNC_EVENT] QP communication established event for QP with handle %p\n", event->element.qp);
        break;
    case IBV_EVENT_SQ_DRAINED:
        printf("[ASYNC_EVENT] QP Send Queue drained event for QP with handle %p\n", event->element.qp);
        break;
    case IBV_EVENT_PATH_MIG:
        printf("[ASYNC_EVENT] QP Path migration loaded event for QP with handle %p\n", event->element.qp);
        break;
    case IBV_EVENT_PATH_MIG_ERR:
        printf("[ASYNC_EVENT] QP Path migration error event for QP with handle %p\n", event->element.qp);
        break;
    case IBV_EVENT_QP_LAST_WQE_REACHED:
        printf("[ASYNC_EVENT] QP last WQE reached event for QP with handle %p\n", event->element.qp);
        break;

    /* CQ events */
    case IBV_EVENT_CQ_ERR:
        printf("[ASYNC_EVENT] CQ error for CQ with handle %p\n", event->element.cq);
        break;

    /* SRQ events */
    case IBV_EVENT_SRQ_ERR:
        printf("[ASYNC_EVENT] SRQ error for SRQ with handle %p\n", event->element.srq);
        break;
    case IBV_EVENT_SRQ_LIMIT_REACHED:
        printf("[ASYNC_EVENT] SRQ limit reached event for SRQ with handle %p\n", event->element.srq);
        break;

    /* Port events */
    case IBV_EVENT_PORT_ACTIVE:
        printf("[ASYNC_EVENT] Port active event for port number %d\n", event->element.port_num);
        break;
    case IBV_EVENT_PORT_ERR:
        printf("[ASYNC_EVENT] Port error event for port number %d\n", event->element.port_num);
        break;
    case IBV_EVENT_LID_CHANGE:
        printf("[ASYNC_EVENT] LID change event for port number %d\n", event->element.port_num);
        break;
    case IBV_EVENT_PKEY_CHANGE:
        printf("[ASYNC_EVENT] P_Key table change event for port number %d\n", event->element.port_num);
        break;
    case IBV_EVENT_GID_CHANGE:
        printf("[ASYNC_EVENT] GID table change event for port number %d\n", event->element.port_num);
        break;
    case IBV_EVENT_SM_CHANGE:
        printf("[ASYNC_EVENT] SM change event for port number %d\n", event->element.port_num);
        break;
    case IBV_EVENT_CLIENT_REREGISTER:
        printf("[ASYNC_EVENT] Client reregister event for port number %d\n", event->element.port_num);
        break;

    /* RDMA device events */
    case IBV_EVENT_DEVICE_FATAL:
        printf("[ASYNC_EVENT] Fatal error event for device %s\n", ibv_get_device_name(ctx->device));
        break;

    default:
        printf("[ASYNC_EVENT] Unknown event (%d)\n", event->event_type);
    }
}

void * poll_async_events(void *con) {
    /* the actual code that reads the events in the loop and prints it */
    int ret;
    struct ibv_context *ctx = (struct ibv_context *)con;
    struct ibv_async_event event;

    while (1) {
        /* wait for the next async event */
        ret = ibv_get_async_event(ctx, &event);
        if (ret) {
            fprintf(stderr, "Error, ibv_get_async_event() failed.\n");
            rc_die("Cannot get async event.\n");
        }

        /* print the event */
        print_async_event(ctx, &event);

        /* ack the event */
        ibv_ack_async_event(&event);
    }

    return NULL;
}

__attribute__((visibility ("hidden")))
void build_cq_channel(struct rdma_cm_id *id, int low_lat)
{
	struct conn_context *ctx = (struct conn_context *)id->context;

	// create completion queue channel
	ctx->comp_channel = ibv_create_comp_channel(s_ctx->ctx);

	if (!ctx->comp_channel)
		rc_die("ibv_create_comp_channel() failed\n");

	ctx->cq = ibv_create_cq(s_ctx->ctx, MAX_CQE, NULL, ctx->comp_channel, 0);

	debug_print("cq created for ctx->sockfd=%d sock_type=%d low_lat=%d\n",
		    ctx->sockfd, ctx->sock_type, low_lat);

	if (!ctx->cq)
		rc_die("ibv_create_cq() failed\n");

	ibv_req_notify_cq(ctx->cq, 0);

	// poll completion queues in the background (always or only during bootstrap)
#ifdef NIC_OFFLOAD
	if (low_lat && !g_low_lat_cq_polling_thread) { // low latency channel.
	    g_low_lat_cq_polling_thread = (pthread_t *)calloc(
		    1, sizeof(pthread_t) * N_LOW_LAT_POLLING_THREAD);

	    struct thread_arg th_arg[N_LOW_LAT_POLLING_THREAD];

	    for (int i = 0; i < N_LOW_LAT_POLLING_THREAD; i++) {
		    th_arg[i].id = i;
		    sprintf(th_arg[i].name, "cq-poll-lat_%d", i);
		    pthread_create(&g_low_lat_cq_polling_thread[i], NULL,
				   poll_low_lat_cq_spinning_loop, &th_arg[i]);
	    }

	    printf("creating polling threads to poll completions (global-latency sensitive channel)\n");

	} else if (!g_cq_polling_thread) {
	    g_cq_polling_thread = (pthread_t*)calloc(1, sizeof(pthread_t));
	    pthread_create(g_cq_polling_thread, NULL, poll_cq_spinning_loop_global, NULL);
	    printf("creating background global thread to poll completions (global-spinning)\n");
	}
	// else {
	//         printf("creating background thread to poll completions (blocking)\n");
	//         pthread_create(&ctx->cq_poller_thread, NULL, poll_cq_blocking_loop, ctx);

	// }
#else
	printf("creating background thread to poll completions (blocking)\n");
	pthread_create(&ctx->cq_poller_thread, NULL, poll_cq_blocking_loop, ctx);
#endif

#if 0
	//bind polling thread to a specific core to avoid contention
	cpu_set_t cpuset;
	int max_cpu_available = 0;
	int core_id = 1;

	CPU_ZERO(&cpuset);
	sched_getaffinity(0, sizeof(cpuset), &cpuset);

	// Search for the last / highest cpu being set
	for (int i = 0; i < 8 * sizeof(cpu_set_t); i++) {
		if (!CPU_ISSET(i, &cpuset)) {
			max_cpu_available = i;
			break;
		}
	}
	if(max_cpu_available <= 0)
		rc_die("unexpected config; # of available cpu cores must be > 0");

	core_id = (ctx->sockfd) % (max_cpu_available-1) + 2;

	printf("assigning poll_cq loop for sockfd %d to core %d\n", ctx->sockfd, core_id); 

	CPU_ZERO(&cpuset);
	CPU_SET(core_id, &cpuset); //try to bind pollers to different cores

	int ret = pthread_setaffinity_np(ctx->cq_poller_thread, sizeof(cpu_set_t), &cpuset);
	if(ret != 0)
		rc_die("failed to bind polling thread to a CPU core");
#endif
}

__attribute__((visibility ("hidden"))) 
void build_params(struct rdma_conn_param *params)
{
	memset(params, 0, sizeof(*params));

	params->initiator_depth = params->responder_resources = 1;
	params->rnr_retry_count = 7; /* infinite retry */
	params->retry_count = 7;     /* Required for RoCE connection via switch.
                                      * 0 retry_count fails.
                                      */
}

__attribute__((visibility ("hidden"))) 
void build_qp_attr(struct rdma_cm_id *id, struct ibv_qp_init_attr *qp_attr)
{
	struct conn_context *ctx = (struct conn_context *)id->context;

	memset(qp_attr, 0, sizeof(*qp_attr));

	qp_attr->send_cq = ctx->cq;
	qp_attr->recv_cq = ctx->cq;
	qp_attr->qp_type = IBV_QPT_RC;

	qp_attr->cap.max_send_wr = MAX_SEND_QUEUE_SIZE;
	qp_attr->cap.max_recv_wr = MAX_RECV_QUEUE_SIZE;
	qp_attr->cap.max_send_sge = MAX_SEND_SGE_SIZE;
	qp_attr->cap.max_recv_sge = MAX_RECV_SGE_SIZE;
}

/**
 * @Synopsis  
 *
 * @Param ec
 * @Param exit_on_connect
 * @Param exit_on_disconnect
 * @Param low_lat Low latency mode. Polling CQ.
 */
__attribute__((visibility ("hidden")))
void event_loop(struct rdma_event_channel *ec, int exit_on_connect, int exit_on_disconnect, int low_lat)
{
	struct rdma_cm_event *event = NULL;
	struct rdma_conn_param cm_params;
	build_params(&cm_params);

	debug_print("event_loop: is_client=%d low_lat=%d\n", exit_on_disconnect, low_lat);

	while (rdma_get_cm_event(ec, &event) == 0) {
		struct rdma_cm_event event_copy;

		memcpy(&event_copy, event, sizeof(*event));
		debug_print("received event[%d]: %s\n", event_copy.event, rdma_event_str(event_copy.event));

		rdma_ack_cm_event(event);

		if (event_copy.event == RDMA_CM_EVENT_ADDR_RESOLVED) {
			build_connection(event_copy.id, low_lat);
			if (s_on_pre_conn_cb) {
				debug_print("trigger pre-connection callback\n");
				s_on_pre_conn_cb(event_copy.id);
			}

			rdma_resolve_route(event_copy.id, TIMEOUT_IN_MS);

		}
		else if (event_copy.event == RDMA_CM_EVENT_ROUTE_RESOLVED) {
			struct conn_context *ctx = event_copy.id->context;
			sem_post(&ctx->conn_sem);

		}
		else if (event_copy.event == RDMA_CM_EVENT_CONNECT_REQUEST) {
			// Add connection on server.
			uint32_t sock_type = 0;
			sock_type = be32toh(*(__be32 *) event_copy.param.conn.private_data);
			rc_add(event_copy.id, -1, 1, sock_type, low_lat);
			build_connection(event_copy.id, low_lat);

			if (s_on_pre_conn_cb) {
				debug_print("trigger pre-connection callback\n");
				s_on_pre_conn_cb(event_copy.id);
			}

			rdma_accept(event_copy.id, &cm_params);
		}
		else if (event_copy.event == RDMA_CM_EVENT_ESTABLISHED) {
			rc_set_state(event_copy.id, RC_CONNECTION_ACTIVE);
			if (s_on_connect_cb) {
				debug_print("trigger post-connection callback\n");
				s_on_connect_cb(event_copy.id);
			}

			if (exit_on_connect)
				break;
		}
		else if (event_copy.event == RDMA_CM_EVENT_DISCONNECTED) {
			if (s_on_disconnect_cb) {
				debug_print("trigger disconnection callback\n");
				s_on_disconnect_cb(event_copy.id, low_lat);
			}

			rc_disconnect(event_copy.id);

			if (exit_on_disconnect) {
				rc_clear(event_copy.id);
				break;
			}

		}
		else if (event_copy.event == RDMA_CM_EVENT_TIMEWAIT_EXIT) {
			//this event indicates that the recently destroyed queue pair is ready to be reused
			//at this point, clean up any allocated memory for connection
			rc_clear(event_copy.id);
		}
		else {
			rc_die("unknown event");
		}
	}
}

__attribute__((visibility ("hidden"))) 
void update_completions(struct ibv_wc *wc)
{
	//FIXME: one hash search is performed for every send completion. Optimize!
	struct rdma_cm_id *id = find_connection_by_wc(wc);
	struct conn_context *ctx = (struct conn_context *)id->context;

	//signal any threads blocking on wr.id
	if (wc->opcode & IBV_WC_RECV) {
		debug_print("COMPLETION --> (RECV WR #%lu) [qp_num %u]\n", wc->wr_id, id->qp->qp_num);
		// printf("%lu COMPLETION --> (RECV WR #%lu) [qp_num %u]\n", get_tid(), wc->wr_id, id->qp->qp_num);
		//ctx->last_rcv_compl = 1;
	}
	else {
		debug_print("COMPLETION --> (SEND WR #%lu) [qp_num %u]\n", wc->wr_id, id->qp->qp_num);
                // printf("%lu COMPLETION --> (SEND WR #%lu) [qp_num %u]\n", get_tid(), wc->wr_id, id->qp->qp_num);
		// printf("COMPLETION --> (SEND WR #%lu) [qp_num %u] ctx->sockfd %d\n", wc->wr_id, id->qp->qp_num, ctx->sockfd);

		pthread_mutex_lock(&ctx->wr_lock); // TODO JYKIM Is it required?
                if (ctx->last_send_compl >= wc->wr_id) {
                    debug_print("ctx->last_send_compl %lu\n", ctx->last_send_compl);
                } else {
                    debug_print("Update last_send_compl from %lu to %lu\n", ctx->last_send_compl, wc->wr_id);
                    ctx->last_send_compl = wc->wr_id;
                }
		//pthread_cond_broadcast(&ctx->wr_completed);
		pthread_mutex_unlock(&ctx->wr_lock); // TODO JYKIM Is it required?
	}
}

__attribute__((visibility ("hidden")))
void spin_till_response(struct rdma_cm_id *id, uint64_t seqn)
{
	struct ibv_wc wc;

	struct conn_context *ctx = (struct conn_context *)id->context;

	debug_print("spinning till response with seqn %lu (last received seqn -> %lu)\n",
			seqn, last_compl_wr_id(ctx, 0));

	while(ctx->last_rcv_compl < seqn || ((ctx->last_rcv_compl - seqn) > MAX_PENDING)) {
		ibw_cpu_relax();
	}
	return;
}

__attribute__((visibility ("hidden")))
void spin_till_per_libfs_response(struct rdma_cm_id *id, uint64_t seqn, int libfs_id)
{
	struct ibv_wc wc;

	struct conn_context *ctx = (struct conn_context *)id->context;

	debug_print("spinning till response with seqn %lu (last received seqn -> %lu)\n",
			seqn, last_compl_wr_id(ctx, 0));

	while (ctx->per_libfs_last_rcv_compl[libfs_id] < seqn ||
	       ((ctx->per_libfs_last_rcv_compl[libfs_id] - seqn) >
		MAX_PENDING)) {
		ibw_cpu_relax();
	}
	return;
}

__attribute__((visibility ("hidden")))
void poll_till_bit_set(struct rdma_cm_id *id, uint32_t *bit)
{
	struct ibv_wc wc;
	struct conn_context *ctx = (struct conn_context *)id->context;
	while(*bit == 0) {
		// Not used in low_lat channel.
		poll_cq(ctx->cq, &wc, 0);
		ibw_cpu_relax();
	}
	return;
}

/**
 * @Synopsis  Use this function carefully. The other request can be handled
 * while waiting for the completion. It may incur deadlock or unexpected
 * latency increase.
 *
 * @Param id
 * @Param wr_id
 * @Param low_lat 1 if it is low_lat channel.
 */
void poll_till_completion(struct rdma_cm_id *id, uint32_t wr_id, int low_lat)
{
    struct ibv_wc wc;
    struct conn_context *ctx = (struct conn_context *)id->context;

    debug_print("polling cq till WR %u completes (last completed WR -> %lu)\n",
	    wr_id, last_compl_wr_id(ctx, 1));
    while(cmp_counters(wr_id, last_compl_wr_id(ctx, 1)) > 0) {
	poll_cq(ctx->cq, &wc, low_lat);
	ibw_cpu_relax();
    }
}

__attribute__((visibility ("hidden")))
void spin_till_bit_set(struct rdma_cm_id *id, uint32_t *bit)
{
	struct ibv_wc wc;
	struct conn_context *ctx = (struct conn_context *)id->context;
	while(*bit == 0) {
		ibw_cpu_relax();
	}
	return;
}

//spin till we receive a completion with wr_id (overrides poll_cq loop)
void spin_till_completion(struct rdma_cm_id *id, uint32_t wr_id)
{
	struct ibv_wc wc;
	struct conn_context *ctx = (struct conn_context *)id->context;

	debug_print("spinning till WR %u completes (last completed WR -> %lu)\n",
			wr_id, last_compl_wr_id(ctx, 1));
       // printf("spinning till WR %u completes (last completed WR -> %lu) ctx->sockfd %d\n",
       //                  wr_id, last_compl_wr_id(ctx, 1), ctx->sockfd);

#if 0
        while(!ibw_cmpxchg(&ctx->poll_permission, 1, 0))
                ibw_cpu_relax();

	while(cmp_counters(wr_id, last_compl_wr_id(ctx, 1)) > 0) {
		poll_cq(ctx->cq, &wc);
		//printf("cmp_counters: input: (wr_id: %u, last_wr_id: %u) | output: %u\n",
		//	       wr_id, last_compl_wr_id(ctx, 1), cmp_counters(wr_id, last_compl_wr_id(ctx, 1)));
		ibw_cpu_relax();
	}

        if(ibw_cmpxchg(&ctx->poll_permission, 0, 1))
                rc_die("failed to reset permission; possible race condition");
#else
	while(cmp_counters(wr_id, last_compl_wr_id(ctx, 1)) > 0) {
		ibw_cpu_relax();
	}
#endif
}

__attribute__((visibility ("hidden")))
void spin_till_response_and_bit(struct rdma_cm_id *id, uint64_t seqn, uintptr_t sync_bit_addr)
{
	struct ibv_wc wc;

	struct conn_context *ctx = (struct conn_context *)id->context;

	debug_print("spinning till response with seqn %lu (last received seqn -> %lu)\n",
			seqn, last_compl_wr_id(ctx, 0));

	// printf ("spinning till response with seqn %u (last received seqn -> %u) sync_bit_addr %p cmd_bit %d\n",
			// seqn, last_compl_wr_id(ctx, 0), sync_bit_addr, (int)*(char *)sync_bit_addr);

        // *sync_bit_addr must be changed in the signal_callback.
	while((ctx->last_rcv_compl < seqn || ((ctx->last_rcv_compl - seqn) > MAX_PENDING)) || (*(char*)sync_bit_addr) == 0) {
		// poll_cq(ctx->cq, &wc);
		ibw_cpu_relax();
	}

	// printf ("spinning response DONE with seqn %u (last received seqn -> %u) sync_bit_addr %p cmd_bit %c\n",
			// seqn, last_compl_wr_id(ctx, 0), sync_bit_addr, *(char *)sync_bit_addr);
}

//poll completions in a looping (blocking)
__attribute__((visibility ("hidden"))) 
void * poll_cq_blocking_loop(void *ctx)
{
	//if(stick_this_thread_to_core(POLL_THREAD_CORE_ID) == EINVAL)
	//	rc_die("failed to pin poll thread to core");

	struct ibv_cq *cq;
	struct ibv_wc wc;
	void *ev_ctx;
        int ret;

	//int ran_once = 0;
	while(((struct conn_context*)ctx)->poll_enable) {
		ret = ibv_get_cq_event(((struct conn_context*)ctx)->comp_channel, &cq, &ev_ctx);
                if (ret) {
                    printf("(Error) Failed to get CQ event. in poll_cq_blocking_loop()\n");
                    rc_die("(Error) Failed to get CQ event. in poll_cq_blocking_loop()");
                }

		ibv_ack_cq_events(cq, 1);       // TODO Expensive. Can optimize it.
                                                // Refer to https://www.rdmamojo.com/2013/03/09/ibv_get_cq_event/#Do_I_have_to_work_with_Completion_events

		ret = ibv_req_notify_cq(cq, 0);
                if (ret) {
                    printf("(Error) Failed to request CQ notification. in poll_cq_blocking_loop()\n");
                    rc_die("(Error) Failed to request CQ notification. in poll_cq_blocking_loop()");
                }

		poll_cq(((struct conn_context*)ctx)->cq, &wc, 0);
	}

	printf("end poll_cq loop for sockfd %d\n", ((struct conn_context*)ctx)->sockfd);
	return NULL;
}

__attribute__((visibility ("hidden"))) 
void * poll_cq_spinning_loop(void *ctx)
{
	//if(stick_this_thread_to_core(POLL_THREAD_CORE_ID) == EINVAL)
	//	rc_die("failed to pin poll thread to core");

	struct ibv_wc wc;
	//int ran_once = 0;

	while(((struct conn_context*)ctx)->poll_enable) {
		if(((struct conn_context*)ctx)->poll_permission)
			poll_cq(((struct conn_context*)ctx)->cq, &wc, 0);
		ibw_cpu_relax();
	}

	printf("end poll_cq loop for sockfd %d\n", ((struct conn_context*)ctx)->sockfd);
	return NULL;
}

/**
 * Poll one CQ.
 */
static void poll_one_cq(struct conn_context *ctx, int low_lat)
{
    struct ibv_wc wc;

    if(ctx->poll_enable && ctx->poll_permission && ctx->cq)
	poll_cq(ctx->cq, &wc, low_lat);
}

/**
 * Polling all the CQs.
 */
__attribute__((visibility ("hidden")))
void * poll_cq_spinning_loop_global(void *args)
{
	int cnt, i;
	struct rdma_cm_id *id;

	while(1) {
	    cnt = rc_connection_count();
	    i = 0;

	    while(i < cnt) {
		if (i >= MAX_CONNECTIONS) {
		    printf("(Warn) Some CQs might not be polled.\n");
		    break;
		}

		id = get_connection(i);

		if (!id) {
		    // debug_print("(Warn) Connection of sockfd %d is not constructed "
		    //         "yet.\n", i);
		    i++;
		    continue;
		}

		if (is_low_lat_connection(i)) {
		    i++;
		    continue;
		}

		poll_one_cq((struct conn_context *)id->context, 0);

		i++;
	    }
	}
	return NULL;
}

/**
 * Polling all the low latency CQs.
 */
__attribute__((visibility ("hidden")))
void * poll_low_lat_cq_spinning_loop(void *args)
{
	int cnt, i;
	struct rdma_cm_id *id;
	struct thread_arg *th_arg;

	th_arg = (struct thread_arg *)args;
	prctl(PR_SET_NAME, th_arg->name);

	while(1) {
	    cnt = rc_connection_count();
	    i = 0;

	    while(i < cnt) {
		if (i >= MAX_CONNECTIONS) {
		    printf("(Warn) Some CQs of low latency channel might not be polled.\n");
		    break;
		}

		id = get_connection(i);
		if (!id) {
		    // debug_print("(Warn) Connection of low latency sockfd %d is not constructed "
		    //         "yet.\n", i);
		    i++;
		    continue;
		}

		if (!is_low_lat_connection(i)) {
		    i++;
		    continue;
		}

		poll_one_cq((struct conn_context *)id->context, 1);

		i++;
	    }
	}
	return NULL;
}

__attribute__((visibility ("hidden")))
inline void poll_cq(struct ibv_cq *cq, struct ibv_wc *wc, int low_lat)
{
        int ret;
	while(ret = ibv_poll_cq(cq, 1, wc)) {
                if (ret < 0) {
                    printf ("(Error) Failed to poll completions from CQ. ret: %d\n", ret);
                    rc_die ("(Error) Failed to poll completions from CQ.");
                }

		if (wc->status == IBV_WC_SUCCESS) {
			update_completions(wc);

                        //debug_print("trigger completion callback\n");
			if (low_lat)
			    s_low_lat_on_completion_cb(wc);
			else
			    s_on_completion_cb(wc);
		}
		else {
			const char *descr;
			descr = ibv_wc_status_str(wc->status);
                        printf("[tid:%lu][%s():%d] COMPLETION FAILURE (%s WR #%lu)"
                                " status[%d] = %s qp_num=%u\n",
                                get_tid(), __func__, __LINE__,
                                (wc->opcode & IBV_WC_RECV)?"RECV":"SEND",
                                wc->wr_id, wc->status, descr, wc->qp_num);
			rc_die("poll_cq: status is not IBV_WC_SUCCESS");
		}
	}
}

__attribute__((visibility ("hidden"))) 
inline void poll_cq_debug(struct ibv_cq *cq, struct ibv_wc *wc)
{
	while(ibv_poll_cq(cq, 1, wc)) {
		if (wc->status == IBV_WC_SUCCESS) {
			update_completions(wc);

			//debug_print("trigger completion callback\n");
			s_on_completion_cb(wc);

			if(wc->opcode & IBV_WC_RECV)
				return;
		}
		else {
#ifdef DEBUG
			const char *descr;
			descr = ibv_wc_status_str(wc->status);
			debug_print("COMPLETION FAILURE (%s WR #%lu) status[%d] = %s\n",
					(wc->opcode & IBV_WC_RECV)?"RECV":"SEND",
					wc->wr_id, wc->status, descr);
#endif
			rc_die("poll_cq: status is not IBV_WC_SUCCESS");
		}
	}
}

#if 0
__attribute__((visibility ("hidden"))) 
int poll_cq_for_nuance(uint64_t wr_id)
{
	//int total_cq_poll_count = 0;
        struct ibv_cq *cq = s_ctx->cq;
	struct ibv_wc wc;
	int success = 0;
	int cq_poll_count = 0;

	debug_print("----check WR completion (id:%lu)", wr_id);
#if 1
	int i;
	pthread_mutex_lock(shared_cq_lock);
	
	//first, we do a quick search through the archive
	//we start from the last written archive entry
	for(i = max(archive_idx-1, 0); i >= 0; i--) {
		debug_print("checking archive entry[%d] for WR id. given: %u, exp: %lu\n",
		        i, cqe_archive[i], wr_id);
		if(wr_id == cqe_archive[i]) {
			success = 1;
			cqe_archive[i] = 0;
			break;
		}
	}

	//we lookup the rest of the entries if we didn't find wr_id
	for(i = archive_idx;  i < ARCHIVE_SIZE && !success; i++) {
		debug_print("checking archive entry[%d] for WR id. given: %u, exp: %lu\n",
			i, cqe_archive[i], wr_id);
		if(wr_id == cqe_archive[i]) {
			success = 1;
			cqe_archive[i] = 0;
			break;
		}
	}

	if(success) {
		debug_print("found nuance %lu\n", wr_id);
		pthread_mutex_unlock(shared_cq_lock);
		return cq_poll_count;
	}

#endif

	//printf("checking CQ for nuance %lu\n", wr_id);
	if (ibv_req_notify_cq(cq, 0))
		rc_die("Could not request CQ notification");

	if (ibv_get_cq_event(s_ctx->comp_channel, &cq, (void *)&s_ctx->ctx))
		rc_die("Failed to get cq_event");

 	while(ibv_poll_cq(cq, 1, &wc)) {
		cq_poll_count++;
		if (wc.status == IBV_WC_SUCCESS) {
			debug_print("polling CQ for WR id. given: %lu, exp: %lu\n",
					wr_id, wc.wr_id);
			if((!success && wc.wr_id == wr_id) || !wr_id) {	
				success = 1;
			}
			else {
				cqe_archive[archive_idx] = wc.wr_id;
				archive_idx = (archive_idx + 1) % sizeof(cqe_archive);
			}
		}
		else
			rc_die("poll_cq: status is not IBV_WC_SUCCESS");
	}

	ibv_ack_cq_events(cq, 1);

	pthread_mutex_unlock(shared_cq_lock);

	if(success)
		return cq_poll_count;
	else {
		fprintf(stderr, "[error] wrong completion event. expected: %lu, found: %lu\n",
				wr_id, wc.wr_id);
		exit(EXIT_FAILURE);
	}
}
#endif

__attribute__((visibility ("hidden"))) 
struct rdma_cm_id* find_next_connection(struct rdma_cm_id* id)
{
	int i = 0;

	if(id == NULL)
		i = find_first_set_bit(s_conn_bitmap, MAX_CONNECTIONS);
	else {
		struct conn_context *ctx = (struct conn_context *) id->context;
		i = find_next_set_bit(ctx->sockfd, s_conn_bitmap, MAX_CONNECTIONS);
	}

	if(i >= 0)
		return get_connection(i);
	else
		return NULL;	 
}

__attribute__((visibility ("hidden"))) 
struct rdma_cm_id* find_connection_by_addr(struct sockaddr_in *addr)
{
	struct id_record *entry = NULL;
	// debug_print("%lu [hash] looking up id with sockaddr: %s:%hu\n",
	//                 get_tid(), inet_ntoa(addr->sin_addr), addr->sin_port);
	HASH_FIND(addr_hh, s_ctx->id_by_addr, addr, sizeof(*addr), entry);
	if(!entry)
		rc_die("hash lookup failed; id doesn't exist");
	return entry->id;
}

__attribute__((visibility ("hidden"))) 
struct rdma_cm_id* find_connection_by_wc(struct ibv_wc *wc)
{
	struct id_record *entry = NULL;
	//debug_print("%lu [hash] looking up id with qp_num: %u\n", get_tid(), wc->qp_num);
	HASH_FIND(qp_hh, s_ctx->id_by_qp, &wc->qp_num, sizeof(wc->qp_num), entry);
	if(!entry)
		rc_die("hash lookup failed; id doesn't exist");
	return entry->id;
}

__attribute__((visibility ("hidden"))) 
struct rdma_cm_id* get_connection(int sockfd)
{
	if(sockfd > MAX_CONNECTIONS)
		rc_die("invalid sockfd; must be less than MAX_CONNECTIONS");

	if(s_conn_bitmap[sockfd])
		return s_conn_ids[sockfd];
	else
		return NULL;
}

//iterate over hashtable and execute anonymous function for each id
//note: function has to accept two arguments of type: rdma_cm_id* & void* (in that order)
__attribute__((visibility ("hidden"))) 
void execute_on_connections(void* custom_arg, void custom_func(struct rdma_cm_id*, void*))
{
	struct id_record *i = NULL;

	for(i=s_ctx->id_by_qp; i!=NULL; i=i->qp_hh.next) {
		custom_func(i->id, custom_arg);
	}
}

__attribute__((visibility ("hidden")))
void rc_init(pre_conn_cb_fn pc, connect_cb_fn conn, completion_cb_fn comp, disconnect_cb_fn disc, int low_lat)
{
	debug_print("initializing RC module\n");

	s_conn_bitmap = calloc(MAX_CONNECTIONS, sizeof(int));
	s_conn_low_lat_bitmap = calloc(MAX_CONNECTIONS, sizeof(int));
	s_conn_ids = (struct rdma_cm_id **)calloc(MAX_CONNECTIONS, sizeof(struct rdma_cm_id*));

	s_on_pre_conn_cb = pc;
	s_on_connect_cb = conn;
	if (low_lat)
	    s_low_lat_on_completion_cb = comp;
	else
	    s_on_completion_cb = comp;
	s_on_disconnect_cb = disc;
}

__attribute__((visibility ("hidden")))
int rc_add(struct rdma_cm_id *id, int app_type, int poll_loop, int sock_type, int low_lat)
{
	int sockfd = find_first_empty_bit_and_set(s_conn_bitmap, MAX_CONNECTIONS);

	if(sockfd < 0)
		rc_die("can't open new connection; number of open sockets == MAX_CONNECTIONS");

	debug_print("Adding connection on sock_fd=%d sock_type=%d low_lat=%d\n",
		    sockfd, sock_type, low_lat);

	//pre-emptively build ctx for connection; allows clients to poll state of connection
	build_conn_context(id, poll_loop);

	struct conn_context *ctx = (struct conn_context *) id->context;

	s_conn_bitmap[sockfd] = 1;
	if (low_lat) {
	    s_conn_low_lat_bitmap[sockfd] = 1;
	}

	ctx->sockfd = sockfd;
	ctx->app_type = app_type;
	ctx->sock_type = sock_type;
	s_conn_ids[sockfd] = id;
	sem_init(&ctx->conn_sem, 0, 0);
#ifdef PRINT_PREV_MSG
	ctx->prev_app_data = (char *)calloc(1, msg_size);
	printf("%d bytes allocated for prev_app_data sockfd=%d\n", msg_size,
	       ctx->sockfd);
#endif

	return sockfd;
}

__attribute__((visibility ("hidden"))) 
void rc_set_state(struct rdma_cm_id *id, int new_state)
{
	struct conn_context *ctx = (struct conn_context *) id->context;

	if(ctx->state == new_state)
		return;

	debug_print("modify state for socket #%d from %d to %d\n", ctx->sockfd, ctx->state, new_state);

	ctx->state = new_state;
	
	if((ctx->state == RC_CONNECTION_READY && !ctx->poll_always) || ctx->state == RC_CONNECTION_TERMINATED) {
		ctx->poll_enable = 0;	
  		//void *ret;
    		//if(pthread_join(ctx->cq_poller_thread, &ret) != 0)
		//	rc_die("pthread_join() error");
		if (ctx->cq_poller_thread)
		    pthread_cancel(ctx->cq_poller_thread);
	}
}


int rc_connection_count()
{
	//return HASH_CNT(qp_hh, s_ctx->id_by_qp);
	return find_bitmap_weight(s_conn_bitmap, MAX_CONNECTIONS);
}

int rc_low_lat_connection_count()
{
    return find_bitmap_weight(s_conn_low_lat_bitmap, MAX_CONNECTIONS);
}


int rc_next_connection(int cur)
{
	int i = 0;

	if(cur < 0)
		i = find_first_set_bit(s_conn_bitmap, MAX_CONNECTIONS);
	else {
		i = find_next_set_bit(cur, s_conn_bitmap, MAX_CONNECTIONS);
	}

	if(i >= 0)
		return i;
	else
		return -1;	 
}

int rc_connection_meta(int sockfd)
{
	if(get_connection(sockfd)) {
		struct conn_context *ctx = (struct conn_context *) s_conn_ids[sockfd]->context;
		return ctx->app_type;
	}
	else
		return -1;
}

char* rc_connection_ip(int sockfd) {
	if(get_connection(sockfd)) {
		struct sockaddr_in *addr_in = copy_ipv4_sockaddr(&s_conn_ids[sockfd]->route.addr.dst_storage);
		char *s = malloc(sizeof(char)*INET_ADDRSTRLEN);
		s = inet_ntoa(addr_in->sin_addr);
		return s;
	}
	else
		return NULL;
}
int rc_ready(int sockfd)
{
	if(get_connection(sockfd)) {
		struct conn_context *ctx = (struct conn_context *) s_conn_ids[sockfd]->context;
		if(ctx->state == RC_CONNECTION_READY)
			return 1;
		else
			return 0;
	}
	else
		return 0;
}

int rc_active(int sockfd)
{
	if(get_connection(sockfd)) {
		struct conn_context *ctx = (struct conn_context *) s_conn_ids[sockfd]->context;
		if(ctx->state >= RC_CONNECTION_ACTIVE)
			return 1;
		else
			return 0;
	}
	else
		return 0;
}

int rc_terminated(int sockfd)
{
	if(get_connection(sockfd)) {
		struct conn_context *ctx = (struct conn_context *) s_conn_ids[sockfd]->context;
		if(ctx->state == RC_CONNECTION_TERMINATED)
			return 1;
		else
			return 0;
	}
	else
		return 0;
}

//FIXME: need a synchronization mechanism in case of simultaneous acquisitions
__attribute__((visibility ("hidden"))) 
int _rc_acquire_buffer(int sockfd, void ** ptr, int user)
{
#if 0
	int i;
	int timeout = 5;

	while(!rc_active(sockfd)) {
		if(timeout == 0)
			rc_die("failed to get acquire msg buffer; connection is inactive");
		debug_print("connection is inactive; sleeping for 1 sec...\n");
		timeout--;
		sleep(1);
	}

	struct conn_context *ctx = (struct conn_context *) get_connection(sockfd)->context;

	/*
	//busy wait, since this is likely on the fast path
	do {
		i = find_first_empty_bit(ctx->send_slots, MAX_BUFFER);
		ibw_cpu_relax();
	} while(i < 0);
	*/

	i = find_first_empty_bit(ctx->send_slots, MAX_BUFFER);

	//FIXME: consider a more robust approach for buffer acquisition instead
	//of simply failing.
	//As it stands, busy waiting for buffers can be problematic if buffer
	//acquisitions are happening during 'on_application_event' callbacks.
	//Doing so will stop the CQ polling loop, resulting in a deadlock.
	if(i < 0) {
		rc_die("failed to acquire buffer. consider increasing MAX_BUFFER");
	}

	ctx->send_slots[i] = 1;

	if(user) {
		//if this is not called by our internal bootstrap protocol, we provide data buffer for caller
		ctx->msg_send[i]->meta.app.id = 0; //set app.id to zero
		ctx->msg_send[i]->meta.app.data = ctx->msg_send[i]->data; //set a convenience pointer for data
		*ptr = (void *) &ctx->msg_send[i]->meta.app; //doesn't matter if we return app or mr
	}

	return i;
#endif
#if 1
	struct conn_context *ctx = (struct conn_context *) get_connection(sockfd)->context;
	pthread_spin_lock(&ctx->acquire_buf_lock);
	int i = ctx->send_idx++ % MAX_BUFFER;
	pthread_spin_unlock(&ctx->acquire_buf_lock);

	debug_print("acquire buffer ID = %d on sockfd %d\n", i, sockfd);

	if(user) {
		ctx->msg_send[i]->meta.app.id = 0; //set app.id to zero
		ctx->msg_send[i]->meta.app.libfs_id = 0; //set app.libfs_id to zero
		ctx->msg_send[i]->meta.app.data = ctx->msg_send[i]->data; //set a convenience pointer for data
		*ptr = (void *) &ctx->msg_send[i]->meta.app; //doesn't matter if we return app or mr
	}

	return i;
#endif
	//return 0;
}

int rc_acquire_buffer(int sockfd, struct app_context ** ptr)
{
	return _rc_acquire_buffer(sockfd, (void **) ptr, 1);
}

//bind acquired buffer to specific wr id
__attribute__((visibility ("hidden"))) 
int rc_bind_buffer(struct rdma_cm_id *id, int buffer, uint32_t wr_id)
{
#if 1
	debug_print("binding buffer[%d] --> (SEND WR #%u)\n", buffer, wr_id);
	struct conn_context *ctx = (struct conn_context *) id->context;

	struct buffer_record *rec = calloc(1, sizeof(struct buffer_record));
	rec->wr_id = wr_id;
	rec->buff_id = buffer;

	/*
	struct buffer_record *current_b, *tmp_b;
	HASH_ITER(hh, ctx->buffer_bindings, current_b, tmp_b) {
		debug_print("buffer_binding record: wr_id:%u buff_id:%u\n", current_b->wr_id, current_b->buff_id);
	}
	*/

	pthread_spin_lock(&ctx->buffer_lock);
	HASH_ADD(hh, ctx->buffer_bindings, wr_id, sizeof(rec->wr_id), rec);
	pthread_spin_unlock(&ctx->buffer_lock);
	return 1;
#endif
	//return 1;
}

__attribute__((visibility ("hidden"))) 
int rc_release_buffer(int sockfd, uint32_t wr_id)
{
#if 1
	struct conn_context *ctx = (struct conn_context *) s_conn_ids[sockfd]->context;
	struct buffer_record *b;

	pthread_spin_lock(&ctx->buffer_lock);
	HASH_FIND(hh, ctx->buffer_bindings, &wr_id, sizeof(wr_id), b);
	if(b) {
		debug_print("released buffer[%d] --> (SEND WR #%u)\n", b->buff_id, wr_id);
		HASH_DEL(ctx->buffer_bindings, b);
		if(ctx->send_slots[b->buff_id])
			ctx->send_slots[b->buff_id] = 0;
		int ret = b->buff_id;
		free(b);

		pthread_spin_unlock(&ctx->buffer_lock);
		return ret;
	}
	else {
		pthread_spin_unlock(&ctx->buffer_lock);
		rc_die("failed to release buffer. possible race condition.\n");
	}

	return -1;
#endif
	//return 0;
}

__attribute__((visibility ("hidden"))) 
void rc_disconnect(struct rdma_cm_id *id)
{
	struct conn_context *ctx = (struct conn_context *)id->context;

	debug_print("terminating connection on socket #%d\n", ctx->sockfd);

	rc_set_state(id, RC_CONNECTION_TERMINATED);

#if 1
	//delete from hashtables
	struct id_record *entry = NULL;
	HASH_FIND(qp_hh, s_ctx->id_by_qp, &id->qp->qp_num, sizeof(id->qp->qp_num), entry);
	if(!entry)
		rc_die("hash delete failed; id doesn't exist");
	HASH_DELETE(qp_hh, s_ctx->id_by_qp, entry);
	HASH_DELETE(addr_hh, s_ctx->id_by_addr, entry);
	free(entry);

	struct app_response *current_p, *tmp_p;
	struct buffer_record *current_b, *tmp_b;

	HASH_ITER(hh, ctx->pendings, current_p, tmp_p) {
		HASH_DEL(ctx->pendings,current_p);
		free(current_p);
	}

	HASH_ITER(hh, ctx->buffer_bindings, current_b, tmp_b) {
		HASH_DEL(ctx->buffer_bindings,current_b);
		free(current_b);
	}

	//destroy queue pair and disconnect
	rdma_destroy_qp(id);
	rdma_disconnect(id);
#endif
}

__attribute__((visibility ("hidden")))
void rc_clear(struct rdma_cm_id *id)
{
	struct conn_context *ctx = (struct conn_context *)id->context;

	if(!rc_terminated(ctx->sockfd))
		rc_die("can't clear metadata for non-terminated connection");

	debug_print("clearing connection metadata for socket #%d\n", ctx->sockfd);
	s_conn_bitmap[ctx->sockfd] = 0;
	s_conn_low_lat_bitmap[ctx->sockfd] = 0;
	s_conn_ids[ctx->sockfd] = NULL;

	for(int i=0; i<MAX_MR; i++) {
		if(ctx->local_mr_ready[i]) {
			debug_print("deregistering mr[addr:%lx, len:%lu]\n",
					(uintptr_t)ctx->local_mr[i]->addr, ctx->local_mr[i]->length);
			ibv_dereg_mr(ctx->local_mr[i]);
		}
		if(ctx->remote_mr_ready[i])
			free(ctx->remote_mr[i]);
	}

	free(ctx->local_mr);
	free(ctx->local_mr_ready);
	free(ctx->local_mr_sent);
	free(ctx->remote_mr);
	free(ctx->remote_mr_ready);

	for(int i=0; i<MAX_BUFFER; i++) {
		debug_print("deregistering msg_send_mr[addr:%lx, len:%lu]\n",
				(uintptr_t)ctx->msg_send_mr[i]->addr, ctx->msg_send_mr[i]->length);
		debug_print("deregistering msg_rcv_mr[addr:%lx, len:%lu]\n",
				(uintptr_t)ctx->msg_rcv_mr[i]->addr, ctx->msg_rcv_mr[i]->length);
		ibv_dereg_mr(ctx->msg_send_mr[i]);
		ibv_dereg_mr(ctx->msg_rcv_mr[i]);
		free(ctx->msg_send[i]);
		free(ctx->msg_rcv[i]);
	}

	free(ctx->msg_send_mr);
	free(ctx->msg_rcv_mr);
	free(ctx->msg_send);
	free(ctx->msg_rcv);

#ifdef PRINT_PREV_MSG
	free(ctx->prev_app_data);
#endif

	free(ctx);
	free(id);
}

#if (defined(__i386__) || defined(__x86_64__))
#define GDB_TRAP asm("int $3;");
#elif (defined(__aarch64__))
#define GDB_TRAP asm(".inst 0xd4200000"); // FIXME Not working correctly?
// #define GDB_TRAP __builtin_trap();
#else
#error "Not supported architecture."
#endif

void rc_die(const char *reason)
{
    fprintf(stderr, "%lu %s:%d %s(): %s\n", get_tid(), __FILE__, __LINE__,
	    __func__, reason);
    fflush(stdout);
    fflush(stderr);
    GDB_TRAP;
    exit(EXIT_FAILURE);
}

__attribute__((visibility ("hidden"))) 
struct ibv_pd * rc_get_pd()
{
	return s_ctx->pd;
}

__attribute__((visibility ("hidden")))
int is_low_lat_connection(int sockfd)
{
    if(sockfd > MAX_CONNECTIONS)
	rc_die("invalid sockfd; must be less than MAX_CONNECTIONS");

    return is_bit_set(s_conn_low_lat_bitmap, sockfd);
}

/**
 * Get MR type with address.
 */
/*
int get_type_with_addr(addr_t addr)
{
    struct mr_context *mr;
    for (int i = 0; i < MR_END-MR_NVM_LOG; i++) {
        mr = &mrs[i];
        if (addr >= mr.addr && addr < (mr.addr + mr.length)) {
            return mr.type;
        }
    }
    printf ("(Error) MR not found for addr %lu\n", addr);
    return -1;  // Not found.
}
*/
