#include "verbs.h"
#include "messaging.h"
#include <assert.h>
#include <time.h>

IBV_WRAPPER_FUNC(READ)
IBV_WRAPPER_FUNC(WRITE)
IBV_WRAPPER_FUNC(WRITE_WITH_IMM)
IBV_WRAPPER_FUNC_WITH_FLAG(READ)
IBV_WRAPPER_FUNC_WITH_FLAG(WRITE)

void IBV_WRAPPER_OP_SYNC_ALL(rdma_meta_t *meta, int local_id, int remote_id, int opcode)
{
	int i=0;
	struct rdma_cm_id *id = NULL;
	struct rdma_cm_id *conn_ids[rc_connection_count()];
	uint32_t wr_ids[rc_connection_count()];
	
	for(id=find_next_connection(NULL), i=0; id!=NULL; id=find_next_connection(id), i++) {
		wr_ids[i] = send_rdma_operation(id, meta, local_id, remote_id, opcode);
		conn_ids[i] = id;
	}

	for(int j=0; j<i; j++) {
		spin_till_completion(conn_ids[j], wr_ids[j]);
	}
}

//TODO: implement a waiting function for broadcast operations
void IBV_WRAPPER_OP_ASYNC_ALL(rdma_meta_t *meta, int local_id, int remote_id, int opcode)
{
	struct rdma_cm_id *id = NULL;

	for(id=find_next_connection(NULL); id!=NULL; id=find_next_connection(id)) {
		send_rdma_operation(id, meta, local_id, remote_id, opcode);
	}
}

void IBV_WRAPPER_OP_SYNC(int sockfd, rdma_meta_t *meta, int local_id, int remote_id, int opcode)
{
	uint32_t wr_id = send_rdma_operation(get_connection(sockfd), meta, local_id, remote_id, opcode);
        spin_till_completion(get_connection(sockfd), wr_id);
}

uint32_t IBV_WRAPPER_OP_ASYNC(int sockfd, rdma_meta_t *meta,
		int local_id, int remote_id, int opcode)
{
	uint32_t wr_id = send_rdma_operation(get_connection(sockfd), meta, local_id, remote_id, opcode);
	return wr_id;
}

void IBV_WRAPPER_OP_ASYNC_UNSIGNALED(int sockfd, rdma_meta_t *meta, int local_id, int remote_id, int opcode)
{
	uint32_t wr_id = send_rdma_operation_with_flag(get_connection(sockfd),
		meta, local_id, remote_id, opcode, 0);
}

void IBV_WRAPPER_OP_SYNC_WITH_FLAG(int sockfd, rdma_meta_t *meta, int local_id, int remote_id, int opcode, unsigned int send_flag)
{
        uint32_t wr_id = send_rdma_operation_with_flag(get_connection(sockfd), meta, local_id, remote_id, opcode, send_flag);
        spin_till_completion(get_connection(sockfd), wr_id);
}

uint32_t IBV_WRAPPER_OP_ASYNC_WITH_FLAG(int sockfd, rdma_meta_t *meta,
		int local_id, int remote_id, int opcode, unsigned int send_flag)
{
        uint32_t wr_id = send_rdma_operation_with_flag(get_connection(sockfd), meta, local_id, remote_id, opcode, send_flag);
        return wr_id;
}

// Wrapper
__attribute__((visibility ("hidden")))
uint32_t send_rdma_operation(struct rdma_cm_id *id, rdma_meta_t *meta, int local_id, int remote_id, int opcode) {
    return __send_rdma_operation(id, meta, local_id, remote_id, opcode, IBV_SEND_SIGNALED);
}

__attribute__((visibility ("hidden")))
uint32_t send_rdma_operation_with_flag(struct rdma_cm_id *id, rdma_meta_t *meta, int local_id, int remote_id, int opcode, unsigned int send_flag) {
   __send_rdma_operation(id, meta, local_id, remote_id, opcode, send_flag);
}

__attribute__((visibility ("hidden")))
uint32_t __send_rdma_operation(struct rdma_cm_id *id, rdma_meta_t *meta, int local_id, int remote_id, int opcode, unsigned int send_flag)
{
	int timeout = 5; //set bootstrap timeout to 5 sec
	struct mr_context *remote_mr = NULL;
	struct ibv_mr *local_mr = NULL;
	int one_sided = op_one_sided(opcode);
	struct conn_context *ctx = (struct conn_context *)id->context;
	rdma_meta_t *next_meta = NULL;
	struct ibv_send_wr *wr_head = NULL;
	struct ibv_send_wr *wr = NULL;
	struct ibv_send_wr *bad_wr = NULL;
	int opcount = 0;
	int ret;

	if(local_id > MAX_MR || remote_id > MAX_MR)
		rc_die("invalid memory regions specified");

	while(!mr_local_ready(ctx, local_id) || (one_sided && !mr_remote_ready(ctx, remote_id))) {
		if(timeout == 0)
			rc_die("failed to issue sync; no metadata available for remote mr\n");
		debug_print("keys haven't yet been received; sleeping for 1 sec...\n");
		timeout--;
		sleep(1);
	}

	local_mr = ctx->local_mr[local_id];
	if(one_sided)
		remote_mr = ctx->remote_mr[remote_id];


#ifdef IBV_RATE_LIMITER
	unsigned long cnt = 0;
	uint32_t cur_wr_id = 0;
	struct timespec start_time, end_time;
	clock_gettime(CLOCK_MONOTONIC, &start_time);

	cur_wr_id = current_wr_id(ctx, 1);

	while (diff_counters(cur_wr_id, last_compl_wr_id(ctx, 1)) >=
	       MAX_SEND_QUEUE_SIZE) {
		if (cnt == 0) {
			printf("%lu [RATE_LIMITER] initiated in sending RDMA: "
			       "cur_wr_id %d last_compl_wr_id(ctx, 1): %ld "
			       "cnt=%lu\n",
			       get_tid(), cur_wr_id, last_compl_wr_id(ctx, 1),
			       cnt);
			cnt = 1;
		}

		clock_gettime(CLOCK_MONOTONIC, &end_time);
		if (end_time.tv_sec - start_time.tv_sec > cnt) {
			printf("%lu [RATE_LIMITER] in sending RDMA: cur_wr_id "
			       "%d last_compl_wr_id(ctx, 1): %ld "
			       "cnt=%lu\n",
			       get_tid(), cur_wr_id, last_compl_wr_id(ctx, 1),
			       cnt);
			cnt++;
		}

		ibw_cpu_relax();
	}
#endif


	pthread_mutex_lock(&ctx->wr_lock);
	//pthread_spin_lock(&ctx->post_lock);
	do {
		opcount++;

		if(wr) {
			wr->next = (struct ibv_send_wr*) malloc(sizeof(struct ibv_send_wr));
			wr = wr->next;
		}
		else {
			wr = (struct ibv_send_wr*) malloc(sizeof(struct ibv_send_wr));
			wr_head = wr;
		}

		memset(wr, 0, sizeof(struct ibv_send_wr));

		for(int i=0; i<meta->sge_count; i++) {
			meta->sge_entries[i].lkey = local_mr->lkey;
		}

		if(one_sided) {
			wr->wr.rdma.remote_addr = meta->addr;
			wr->wr.rdma.rkey = remote_mr->rkey;
		}

#ifdef SANITY_CHECK
		uint64_t total_len = 0;
		for(int i=0; i<meta->sge_count; i++)
		{
			total_len += meta->sge_entries[i].length;
			if(!IBV_WITHIN_MR_RANGE((&meta->sge_entries[i]), local_mr)) {
				debug_print("failed to sync. given[addr:%lx, len:%u] - mr[addr:%lx, len:%lu]\n",
						meta->sge_entries[i].addr, meta->sge_entries[i].length,
						remote_mr->addr, remote_mr->length);
				rc_die("request outside bounds of registered mr");
			}
		}

		if(meta->length != total_len)
			exit(EXIT_FAILURE);

		if(one_sided && total_len && !IBV_WITHIN_MR_RANGE(meta, remote_mr)) {
			debug_print("failed to sync. given[addr:%lx, len:%lu] - mr[addr:%lx, len:%lu]\n",
					meta->addr, meta->length, remote_mr->addr, remote_mr->length);
			rc_die("request outside bounds of registered mr");
		}
#endif

		//struct ibv_send_wr wr, *bad_wr = NULL;
		//memset(&wr, 0, sizeof(wr));

		wr->wr_id = next_wr_id(ctx, 1);
		wr->opcode = opcode;

                wr->send_flags = send_flag;

#ifdef IBV_WRAPPER_INLINE
		//FIXME: find an appropriate cut-off point
		//hardcoded to a sane value for now
		if(meta->length < IBV_INLINE_THRESHOLD && opcode != IBV_WR_RDMA_READ) {
			wr->send_flags |= IBV_SEND_INLINE;
			debug_print("OPTIMIZE - inlining msg with size = %lu bytes\n", meta->length);
		}
#endif

		wr->num_sge = meta->sge_count;

		if(wr->num_sge)
			wr->sg_list = meta->sge_entries;
		else
			wr->sg_list = NULL;

		if(opcode == IBV_WR_RDMA_WRITE_WITH_IMM)
			wr->imm_data = htonl(meta->imm);

		debug_print("%s (SEND WR #%lu) [opcode %d, remote addr %lx, len %lu, qp_num %u]\n",
				stringify_verb(opcode), wr->wr_id, opcode, meta->addr, meta->length, id->qp->qp_num);

		// printf("%lu %s (SEND WR #%lu) [opcode %d, remote addr 0x%lx, len %lu, qp_num %u]\n",
		//         get_tid(), stringify_verb(opcode), wr->wr_id, opcode, meta->addr, meta->length, id->qp->qp_num);

		for(int i=0; i<wr->num_sge; i++)
			// printf("%lu ----------- sge%d [addr %lx, length %u]\n", get_tid(), i, wr->sg_list[i].addr, wr->sg_list[i].length);
			debug_print("----------- sge%d [addr %lx, length %u]\n", i, wr->sg_list[i].addr, wr->sg_list[i].length);

		meta = meta->next;

	} while(meta); //loop to batch rdma operations

	debug_print("POST --> %s (SEND WR %lu) [batch size = %d]\n", stringify_verb(opcode), wr_head->wr_id, opcount);
	// printf("%lu POST --> %s (SEND WR %lu) [batch size = %d]\n", get_tid(), stringify_verb(opcode), wr_head->wr_id, opcount);
	ret = ibv_post_send(id->qp, wr_head, &bad_wr);

	pthread_mutex_unlock(&ctx->wr_lock);
	//pthread_spin_unlock(&ctx->post_lock);

	if(ret) {
		printf("ibv_post_send: errno = %d\n", ret);
		rc_die("failed to post rdma operation");
	}

	uint32_t wr_id = wr_head->wr_id;

	while(wr_head) {
		wr = wr_head;
		wr_head = wr_head->next;
		free(wr);
	}

	return wr_id;
}

//------RPC Implementation------

//Send a message using an IBV_WR_SEND operation on a pre-allocated memory buffer [synchronous]
void IBV_WRAPPER_SEND_MSG_SYNC(int sockfd, int buffer_id, int solicit)
{
	struct rdma_cm_id *id = get_connection(sockfd);
	struct conn_context *ctx = (struct conn_context *)id->context;

	if(solicit && !ctx->msg_send[buffer_id]->meta.app.id)
		rc_die("app_id must be greater than 0 to match request to response");

	debug_print("%lu sending synchronous message on buffer[%d] - [RPC #%lu] [Data: %s]\n",
			get_tid(),
			buffer_id, ctx->msg_send[buffer_id]->meta.app.id,
			ctx->msg_send[buffer_id]->meta.app.data);

	if(solicit) {
		//register_pending(id, ctx->msg_send[buffer_id]->meta.app.id);
	}

	ctx->msg_send[buffer_id]->id = MSG_CUSTOM;

	spin_till_completion(id, send_message(id, buffer_id));
}

//Send a message using an IBV_WR_SEND operation on a pre-allocated memory buffer [asynchronous]
uint32_t IBV_WRAPPER_SEND_MSG_ASYNC(int sockfd, int buffer_id, int solicit)
{
	struct rdma_cm_id *id = get_connection(sockfd);
	struct conn_context *ctx = (struct conn_context *)id->context;

	debug_print("%lu sending asynchronous message on buffer[%d] - [RPC #%lu] [Data: %s]\n",
			get_tid(),
			buffer_id, ctx->msg_send[buffer_id]->meta.app.id,
			ctx->msg_send[buffer_id]->meta.app.data);

	ctx->msg_send[buffer_id]->id = MSG_CUSTOM;

	uint32_t wr_id = send_message(id, buffer_id);

	if(solicit) {
		//register_pending(id, ctx->msg_send[buffer_id]->meta.app.id);
	}

	return wr_id;
}

/* Add an entry for a pending msg response

   This entry is used by waiting threads to check if a response for an rpc has been received
   Note: applications using this must avoid duplicates of app_id per connection
*/
void register_pending(struct rdma_cm_id *id, uint64_t app_id)
{
	struct conn_context *ctx = (struct conn_context *)id->context;
	struct app_response *p = (struct app_response *) calloc(1, sizeof(struct app_response));

	debug_print("awaiting response for (RPC #%lu) [qp_num %u\n", app_id, id->qp->qp_num);
	p->id = app_id;
	HASH_ADD(hh, ctx->pendings, id, sizeof(p->id), p);
}

/* Remove pending rpc response entry

   Called by thread waiting for rpc response
*/
void remove_pending(struct rdma_cm_id *id, struct app_response *p)
{
	struct conn_context *ctx = (struct conn_context *)id->context;
	/*
	struct app_response *p;

	debug_print("[APP] deleting response hook for id #%u\n", app_id);
	HASH_FIND_INT(ctx->pendings, app_id, p);
	*/

	debug_print("response received for (RPC #%u) [qp_num %u]\n", p->id, id->qp->qp_num);
	if(p)
		HASH_DEL(ctx->pendings, p);
	else
		rc_die("failed to remove pending app response; undefined behavior");
}

/* Update pending entry to notify waiting threads that rpc response has been received

   This function can be called by the cq_poll background thread, or the thread waiting
   waiting for the rpc completion (if spin_till_response_completion is used)
*/
#if 0
void update_pending(struct rdma_cm_id *id, uint32_t app_id)
{
	struct conn_context *ctx = (struct conn_context *)id->context;
	struct app_response *p;

	debug_print("notify waiting thread(s) of resposne arrival for (RPC #%u) [qp_num %u]\n",
		       	app_id, id->qp->qp_num);
#if defined(DEBUG) && defined(SANITYCHECK)
	struct app_response *current_p, *tmp_p;
	debug_print("%s\n", "---- printing current pending RPC entries ----");
	HASH_ITER(hh, ctx->pendings, current_p, tmp_p) {
		debug_print("(RPC #%u) resp received:%s\n",
				current_p->id, current_p->ready?"Yes":"No");
	}
	debug_print("%s\n", "----------------------------------------------");
#endif

	HASH_FIND(hh, ctx->pendings, &app_id, sizeof(app_id), p);

	if(p) {
		p->ready = 1;
	}
	else {
		debug_print("no threads waiting on response for (RPC #%u)\n", app_id);
	}
}
#endif

void update_pending(struct rdma_cm_id *id, uint64_t app_id)
{
	struct conn_context *ctx = (struct conn_context *)id->context;

	// Do not upodate last_rcv_compl if app_id is 0.
	if (app_id == 0)
	    return;

        // TODO check whether waiting thread starts before it gets synced.
	// ctx->last_rcv_compl is updated without mutual exclusion guarantee.
	// Multi-processing may result in an inconsistent synchronization
	// (or barrier) with the current implementation.
	if (app_id <= ctx->last_rcv_compl) {
		printf("****************************************\n");
		printf("[Warn] decreasing seqn: prev=%lu current=%lu\n",
		       ctx->last_rcv_compl, app_id);
		printf("[Warn] Some waiting thread may start earlier than when "
		       "it gets notified.\n");
		printf("****************************************\n");
	}

	debug_print("%lu Update last_rcv_compl from %lu to %lu ctx:%p id:%p\n",
		get_tid(), ctx->last_rcv_compl, app_id, ctx, id);

	ctx->last_rcv_compl = app_id;

	debug_print("Received response with seqn %lu [qp_num %u]\n",
			app_id, id->qp->qp_num);
}

void update_per_libfs_pending(struct rdma_cm_id *id, uint64_t app_id, int libfs_id)
{
	struct conn_context *ctx = (struct conn_context *)id->context;
	uint64_t last_rcv_compl;

	last_rcv_compl = ctx->per_libfs_last_rcv_compl[libfs_id];

	// Do not upodate last_rcv_compl if app_id is 0.
	if (app_id == 0)
	    return;

        // TODO check whether waiting thread starts before it gets synced.
	// ctx->per_libfs_last_rcv_compl is updated without mutual exclusion guarantee.
	// Multi-threading in one libfs process may result in an inconsistent
	// synchronization (or barrier) with the current implementation.
	if (app_id <= last_rcv_compl) {
	    printf("[Error] decreasing seqn(prev=%lu current=%lu) libfs_id=%d\n",
		    last_rcv_compl, app_id, libfs_id);
            printf("[Error] Some waiting thread may start earlier than when it "
                   "gets notified.\n");
        }

        // debug_print(
        // printf(
        //     "%lu Update per_libfs_last_rcv_compl of libfs(%d) from %lu to "
        //     "%lu ctx=%p id=%p\n",
        //     get_tid(), libfs_id, last_rcv_compl, app_id, ctx, id);

        ctx->per_libfs_last_rcv_compl[libfs_id] = app_id;

	debug_print("Received response with seqn %lu [qp_num %u]\n",
			app_id, id->qp->qp_num);
}

/* Perform a post_send for an RDMA message

   TODO MERGE with function 'send_rdma_operation'. 
*/

__attribute__((visibility ("hidden"))) 
uint32_t send_message(struct rdma_cm_id *id, int buffer)
{
	struct conn_context *ctx = (struct conn_context *)id->context;

	struct ibv_send_wr wr, *bad_wr = NULL;
	struct ibv_sge sge;
	int ret;

	memset(&wr, 0, sizeof(wr));

	//wr.wr_id = (uintptr_t)id;
	wr.wr_id = next_wr_id(ctx, 2);
	wr.opcode = IBV_WR_SEND;
	wr.sg_list = &sge;
	wr.num_sge = 1;
	wr.send_flags = IBV_SEND_SIGNALED;

	sge.addr = (uintptr_t)ctx->msg_send[buffer];
	sge.length = sizeof(*ctx->msg_send[buffer]) + sizeof(char)*msg_size;
	sge.lkey = ctx->msg_send_mr[buffer]->lkey;

	if(rc_bind_buffer(id, buffer, wr.wr_id)) {
#ifdef IBV_RATE_LIMITER
		unsigned long cnt = 0;
		struct timespec start_time, end_time;
		clock_gettime(CLOCK_MONOTONIC, &start_time);
                while(diff_counters(wr.wr_id, last_compl_wr_id(ctx, 1)) >= MAX_SEND_QUEUE_SIZE) {
		    if (cnt == 0) {
			printf("%lu [RATE_LIMITER] initiated in sending message: "
				"wr.wr_id %ld last_compl_wr_id(ctx, 1): %lu "
				"cnt=%lu\n",
				get_tid(), wr.wr_id, last_compl_wr_id(ctx, 1),
				cnt);
                      cnt = 1;
		    }

		    clock_gettime(CLOCK_MONOTONIC, &end_time);
		    if (end_time.tv_sec - start_time.tv_sec > cnt){
			printf("%lu [RATE_LIMITER] in sending message: "
				"wr.wr_id %ld last_compl_wr_id(ctx, 1): %lu "
				"cnt=%lu\n",
				get_tid(), wr.wr_id, last_compl_wr_id(ctx, 1),
				cnt);
			cnt++;
		    }

		    ibw_cpu_relax();
		}
#endif
		debug_print("POST --> (SEND WR #%lu) [addr %lx, len %u, qp_num %u]\n",
			wr.wr_id, sge.addr, sge.length, id->qp->qp_num);

		ret = ibv_post_send(id->qp, &wr, &bad_wr);
		if(ret) {
			printf("ibv_post_send: errno = %d\n", ret);
			rc_die("failed to post rdma operation");
		}
	}
	else
		rc_die("failed to bind send buffer");

	return wr.wr_id;
}

__attribute__((visibility ("hidden"))) 
void receive_message(struct rdma_cm_id *id, int buffer)
{
	struct conn_context *ctx = (struct conn_context *)id->context;

	struct ibv_recv_wr wr, *bad_wr = NULL;
	struct ibv_sge sge;

	memset(&wr, 0, sizeof(wr));

	wr.wr_id = buffer;
	wr.sg_list = &sge;
	wr.num_sge = 1;

	sge.addr = (uintptr_t)ctx->msg_rcv[buffer];
	sge.length = sizeof(*ctx->msg_rcv[buffer]) + sizeof(char)*msg_size;
	sge.lkey = ctx->msg_rcv_mr[buffer]->lkey;

	debug_print("POST --> (RECV WR #%lu) [addr %lx, len %u, qp_num %u]\n",
			wr.wr_id, sge.addr, sge.length, id->qp->qp_num);

#ifdef MSG_BUFFER_PROFILE
	ctx->rcv_buf_posted_cnt++;
	// printf("[BUF_PROFILE] sockfd=%d rcv_buf_cnt=%lu (after add)\n",
	//                 ctx->sockfd, ctx->rcv_buf_posted_cnt);
#endif
	ibv_post_recv(id->qp, &wr, &bad_wr);
}

__attribute__((visibility ("hidden"))) 
void receive_imm(struct rdma_cm_id *id, int buffer)
{
	struct conn_context *ctx = (struct conn_context *)id->context;

	struct ibv_recv_wr wr, *bad_wr = NULL;

	memset(&wr, 0, sizeof(wr));

	wr.wr_id = buffer;
	wr.sg_list = NULL;
	wr.num_sge = 0;

	debug_print("POST --> (RECV IMM WR #%lu)\n", wr.wr_id);

	ibv_post_recv(id->qp, &wr, &bad_wr);
}



//------Request await functions------

/* Wait till response is received for an RPC
   
   Note: app_id is application-defined and both send & rcv messages
   must share the same app_id for this to work (which is set by user)
*/

void IBV_AWAIT_RESPONSE(int sockfd, uint64_t app_id)
{
	struct rdma_cm_id *id = get_connection(sockfd);

	debug_print("wait till response seqn = %lu\n", app_id);
	spin_till_response(id, app_id);
	debug_print("wait ending. received response with seqn = %lu\n", app_id);
}

void IBV_AWAIT_PER_LIBFS_RESPONSE(int sockfd, uint64_t app_id, int libfs_id)
{
	struct rdma_cm_id *id = get_connection(sockfd);

	debug_print("wait till response seqn = %lu\n", app_id);
	spin_till_per_libfs_response(id, app_id, libfs_id);
	debug_print("wait ending. received response with seqn = %lu\n", app_id);
}

void IBV_AWAIT_ACK_POLLING (int sockfd, uint32_t *bit)
{
	struct rdma_cm_id *id = get_connection(sockfd);

	debug_print("wait and poll cq till ack_bit is set. bit_addr=%lu(0x%lx)\n",
		(uint64_t)bit, (uint64_t)bit);

	poll_till_bit_set(id, bit);
	debug_print("%s", "wait ending.\n");
}

void IBV_AWAIT_ACK_SPINNING (int sockfd, uint32_t *bit)
{
	struct rdma_cm_id *id = get_connection(sockfd);

	debug_print("wait and spin till ack_bit is set. bit_addr=%lu(0x%lx)\n",
		(uint64_t)bit, (uint64_t)bit);

	spin_till_bit_set(id, bit);
	debug_print("%s", "wait ending.\n");
}

/* Wait till a send work request is completed.
   
   Useful for doing synchronous rdma operations
   Note: wr_id of post sends must be monotonically increasing and share the same qp.
   This mechanism is implemented in 'connection.c'.
*/

void IBV_AWAIT_WORK_COMPLETION(int sockfd, uint32_t wr_id)
{
	struct rdma_cm_id *id = get_connection(sockfd);

	spin_till_completion(id, wr_id);
}

/**
 * @Synopsis  Polling CQ till a sending work request is completed.
 * Useful for doing synchronous rdma operations
 * Note: wr_id of post sends must be monotonically increasing and share the same qp.
 * This mechanism is implemented in 'connection.c'.
 *
 * @Param sockfd
 * @Param wr_id
 * @Param low_lat 1 means it is low latency channel.
 */
void IBV_AWAIT_WORK_COMPLETION_WITH_POLLING(int sockfd, uint32_t wr_id, int low_lat)
{
	struct rdma_cm_id *id = get_connection(sockfd);

	poll_till_completion(id, wr_id, low_lat);
}

/* Wait till all current pending send work requests are completed.
   
   Useful for amortizing the cost of spinning.
*/

void IBV_AWAIT_PENDING_WORK_COMPLETIONS(int sockfd)
{
	struct rdma_cm_id *id = get_connection(sockfd);
	struct conn_context *ctx = (struct conn_context *)id->context;

	spin_till_completion(id, ctx->last_send);
}


