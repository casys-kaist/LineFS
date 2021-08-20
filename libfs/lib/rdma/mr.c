#include "mr.h"
#include "connection.h"

__attribute__((visibility ("hidden"))) 
int mr_all_recv(struct conn_context *ctx)
{
	if(ctx->remote_mr_total == find_bitmap_weight(ctx->remote_mr_ready,
				MAX_MR) && ctx->mr_init_recv)
		return 1;
	else
		return 0;
}

__attribute__((visibility ("hidden"))) 
int mr_all_sent(struct conn_context *ctx)
{
#if 0
	if(ctx->local_mr_to_sync == -1)
		return 1;
	else
		return 0;
#else
	if(num_mrs == find_bitmap_weight(ctx->local_mr_sent,
				MAX_MR))
		return 1;
	else
		return 0;
#endif
}

__attribute__((visibility ("hidden"))) 
int mr_all_synced(struct conn_context *ctx)
{
	if(mr_all_recv(ctx) && mr_all_sent(ctx))
		return 1;
	else
		return 0;
}

__attribute__((visibility ("hidden"))) 
int mr_local_ready(struct conn_context *ctx, int mr_id)
{
	if(mr_id > MAX_MR)
		rc_die("invalid memory region id; must be less than MAX_MR");

	if(ctx->local_mr_ready[mr_id])
		return 1;
	else
		return 0;
}

__attribute__((visibility ("hidden"))) 
int mr_remote_ready(struct conn_context *ctx, int mr_id)
{
	if(mr_id > MAX_MR)
		rc_die("invalid memory region id; must be less than MAX_MR");

	if(ctx->remote_mr_ready[mr_id])
		return 1;
	else
		return 0;
}

//FIXME: for now, we just hardcode permissions for memory registration
// (all provided mrs are given local/remote write permissions)
__attribute__((visibility ("hidden"))) 
void mr_register(struct conn_context *ctx, struct mr_context *mrs, int num_mrs, int msg_size)
{
	// printf("%lu [DRAM_ALLOC] registering %d memory regions & %d send/rcv buffers\n", get_tid(), num_mrs, MAX_BUFFER*2);
	debug_print("%lu registering %d memory regions & %d send/rcv buffers\n", get_tid(), num_mrs, MAX_BUFFER*2);

	//if(num_mrs <= 0)
	//	return;

	for(int i=0; i<num_mrs; i++) {
		// printf("[DRAM_ALLOC] registering mr #%d with addr:%lu and size:%lu\n", i, mrs[i].addr, mrs[i].length);
		debug_print("registering mr #%d with addr:%lu and size:%lu\n", i, mrs[i].addr, mrs[i].length);
		int idx = mrs[i].type;
		if(idx > MAX_MR-1)
			rc_die("memory region type outside of MAX_MR");
		ctx->local_mr[idx] = ibv_reg_mr(rc_get_pd(), (void*)mrs[i].addr, mrs[i].length, 
				IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);
		if(!ctx->local_mr[idx]) {
			debug_print("registeration failed with errno: %d\n", errno);
			rc_die("ibv_reg_mr failed");
		}
		ctx->local_mr_ready[idx] = 1;
		// printf("%lu [DRAM_ALLOC] registered local_mr[addr:%lx, len:%lu, rkey:%u, lkey:%u]\n", get_tid(),
		debug_print("%lu registered local_mr[addr:%lx, len:%lu, rkey:%u, lkey:%u]\n", get_tid(),
				(uintptr_t)ctx->local_mr[idx]->addr, ctx->local_mr[idx]->length,
			       	ctx->local_mr[idx]->rkey, ctx->local_mr[idx]->lkey);
	}

	//update local_mr_to_sync idx
	ctx->local_mr_to_sync = find_first_set_bit(ctx->local_mr_ready, MAX_MR);

	for(int i=0; i<MAX_BUFFER; i++) {
		//ctx->msg_send[i] = (struct message*) calloc(1, sizeof(struct message));
		//ctx->msg_rcv[i] = (struct message*) calloc(1, sizeof(struct message));

		if(posix_memalign((void **)&ctx->msg_send[i], sysconf(_SC_PAGESIZE), sizeof(*ctx->msg_send[i])+sizeof(char)*msg_size))
			rc_die("posix_memalign failed");

		ctx->msg_send_mr[i] = ibv_reg_mr(rc_get_pd(), ctx->msg_send[i], (sizeof(*ctx->msg_send[i])+sizeof(char)*msg_size),
				IBV_ACCESS_LOCAL_WRITE);

		if(!ctx->msg_send_mr[i])
			rc_die("ibv_reg_mr failed");

		debug_print("%lu registered msg_send_mr[addr:%lx, len:%lu]\n", get_tid(),
				(uintptr_t)ctx->msg_send_mr[i]->addr, ctx->msg_send_mr[i]->length);

		if(posix_memalign((void **)&ctx->msg_rcv[i], sysconf(_SC_PAGESIZE), sizeof(*ctx->msg_rcv[i])+sizeof(char)*msg_size))
			rc_die("posix_memalign failed");

		ctx->msg_rcv_mr[i] = ibv_reg_mr(rc_get_pd(), ctx->msg_rcv[i], (sizeof(*ctx->msg_rcv[i])+sizeof(char)*msg_size),
				IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);

		if(!ctx->msg_rcv_mr[i])
			rc_die("ibv_reg_mr failed");

		//debug_print("CHECK X - [%d] DATA1 %p - DATA2 %p\n", i,
		//		ctx->msg_send[i]->meta.app.data, ctx->msg_send[i]->data);

		debug_print("%lu registered msg_rcv_mr[addr:%lx, len:%lu]\n", get_tid(),
				(uintptr_t)ctx->msg_rcv_mr[i]->addr, ctx->msg_rcv_mr[i]->length);
	}
}

__attribute__((visibility ("hidden"))) 
void mr_prepare_msg(struct conn_context *ctx, int buffer, int msg_type)
{
	int i = buffer;
	if(msg_type == MSG_MR) {
		int id = mr_next_to_sync(ctx);
		if(!mr_local_ready(ctx, id))
			rc_die("failed to prepare MSG_MR; memory region metadata unavailable");

		ctx->msg_send[i]->id = msg_type;
		ctx->msg_send[i]->meta.mr.type = id;
		ctx->msg_send[i]->meta.mr.sock_type = ctx->sock_type;
		ctx->msg_send[i]->meta.mr.addr = (uintptr_t)ctx->local_mr[id]->addr;
		ctx->msg_send[i]->meta.mr.length = ctx->local_mr[id]->length;
		ctx->msg_send[i]->meta.mr.rkey = ctx->local_mr[id]->rkey;
	}
	else
		rc_die("failed to prepare msg; undefined type");
}

__attribute__((visibility ("hidden"))) 
int mr_next_to_sync(struct conn_context *ctx)
{
	int idx = ctx->local_mr_to_sync;

	if(!ctx->local_mr_ready[idx])
		rc_die("failed to find mr to sync; invalid local_mr index");

	//find next local_mr_to_sync
	for(int i=idx+1; i<MAX_MR; i++) {
		if(ctx->local_mr_ready[i])
			ctx->local_mr_to_sync = i; 
	}

	ctx->local_mr_to_sync = find_next_set_bit(idx, ctx->local_mr_ready, MAX_MR);

	return idx;
}

uint64_t mr_local_addr(int sockfd, int mr_id)
{
	int timeout = 5;
	debug_print("fetching local mr metadata\n");
	while(!rc_active(sockfd)) {
		if(timeout == 0)
			rc_die("failed to get local memory address; connection is not active\n");
		debug_print("connection isn't currently active; sleeping for 1 sec...\n");
		timeout--;
		sleep(1);
	}
	
	struct rdma_cm_id *id = get_connection(sockfd);
	struct conn_context *ctx = (struct conn_context *)id->context;
	while(!mr_local_ready(ctx, mr_id)) {
		if(timeout == 0)
			rc_die("failed to get local memory address; no metadata available for region\n");
		debug_print("mr metadata haven't yet been received; sleeping for 1 sec...\n");
		timeout--;
		sleep(1);
	}

	return (uintptr_t) ctx->local_mr[mr_id]->addr;
}

uint64_t mr_remote_addr(int sockfd, int mr_id)
{
	int timeout = 5;
	debug_print("fetching remote mr metadata\n");
	while(!rc_active(sockfd)) {
		if(timeout == 0)
			rc_die("failed to get remote memory address; connection is not active\n");
		debug_print("connection isn't currently active; sleeping for 1 sec...\n");
		timeout--;
		sleep(1);
	}
	
	struct rdma_cm_id *id = get_connection(sockfd);
	struct conn_context *ctx = (struct conn_context *)id->context;
	while(!mr_remote_ready(ctx, mr_id)) {
		if(timeout == 0)
			rc_die("failed to get remote memory address; no metadata available for region\n");
		debug_print("mr metadata haven't yet been received; sleeping for 1 sec...\n");
		timeout--;
		sleep(1);
	}

	return ctx->remote_mr[mr_id]->addr;
}

