#ifndef RDMA_VERBS_H
#define RDMA_VERBS_H

#include "connection.h"
#include "mr.h"
#include "utils.h"

#define IBV_INLINE_THRESHOLD 400

#define IBV_TEXT(STR) #STR
#define IBV_ENUM(PREFIX) IBV_WR_RDMA_ ## PREFIX
#define IBV_STR(PREFIX, SUFFIX) IBV_TEXT(PREFIX) IBV_TEXT(SUFFIX)
#define IBV_WITHIN_MR_RANGE(inner, outer) range_valid((addr_t)inner->addr, inner->length, (addr_t)outer->addr, outer->length)

#define IBV_WRAPPER_HEADER(x)	 uint32_t IBV_WRAPPER_ ## x ## _ASYNC(int sockfd, rdma_meta_t *meta,\
					int local_id, int remote_id);\
				 void IBV_WRAPPER_ ## x ## _SYNC(int sockfd, rdma_meta_t *meta,\
					int local_id, int remote_id);\
				 void IBV_WRAPPER_ ## x ## _ASYNC_ALL(rdma_meta_t *meta,\
					int local_id, int remote_id);\
				 void IBV_WRAPPER_ ## x ## _SYNC_ALL(rdma_meta_t *meta,\
					int local_id, int remote_id);

#define IBV_WRAPPER_FUNC(x)      uint32_t IBV_WRAPPER_ ## x ## _ASYNC(int sockfd, rdma_meta_t *meta,\
					int local_id, int remote_id){\
					IBV_WRAPPER_OP_ASYNC(sockfd, meta, local_id, remote_id, IBV_ENUM(x));}\
				 void IBV_WRAPPER_ ## x ## _SYNC(int sockfd, rdma_meta_t *meta,\
					int local_id, int remote_id) {\
				 	IBV_WRAPPER_OP_SYNC(sockfd, meta, local_id, remote_id, IBV_ENUM(x));}\
				 void IBV_WRAPPER_ ## x ## _ASYNC_ALL(rdma_meta_t *meta,\
					int local_id, int remote_id) {\
					IBV_WRAPPER_OP_ASYNC_ALL(meta, local_id, remote_id, IBV_ENUM(x));}\
				 void IBV_WRAPPER_ ## x ## _SYNC_ALL(rdma_meta_t *meta,\
						int local_id, int remote_id) {\
					IBV_WRAPPER_OP_SYNC_ALL(meta, local_id, remote_id, IBV_ENUM(x));}

#define IBV_WRAPPER_HEADER_WITH_FLAG(x)	 uint32_t IBV_WRAPPER_ ## x ## _ASYNC_WITH_FLAG(int sockfd, rdma_meta_t *meta,\
					int local_id, int remote_id, unsigned int send_flag);\
				 void IBV_WRAPPER_ ## x ## _SYNC_WITH_FLAG(int sockfd, rdma_meta_t *meta,\
					int local_id, int remote_id, unsigned int send_flag);\

#define IBV_WRAPPER_FUNC_WITH_FLAG(x)      uint32_t IBV_WRAPPER_ ## x ## _ASYNC_WITH_FLAG(int sockfd, rdma_meta_t *meta,\
					int local_id, int remote_id, unsigned int send_flag){\
					IBV_WRAPPER_OP_ASYNC_WITH_FLAG(sockfd, meta, local_id, remote_id, IBV_ENUM(x), send_flag);}\
				 void IBV_WRAPPER_ ## x ## _SYNC_WITH_FLAG(int sockfd, rdma_meta_t *meta,\
					int local_id, int remote_id, unsigned int send_flag) {\
				 	IBV_WRAPPER_OP_SYNC_WITH_FLAG(sockfd, meta, local_id, remote_id, IBV_ENUM(x), send_flag);}\

IBV_WRAPPER_HEADER(READ)
IBV_WRAPPER_HEADER(WRITE)
IBV_WRAPPER_HEADER(WRITE_WITH_IMM)
IBV_WRAPPER_HEADER_WITH_FLAG(READ)
IBV_WRAPPER_HEADER_WITH_FLAG(WRITE)

static inline int range_valid(addr_t inner_addr, addr_t inner_len,
		addr_t outer_addr, addr_t outer_len)
{
	if((inner_addr + inner_len > outer_addr + outer_len)
			|| inner_addr < outer_addr) {
#ifdef DEBUG
		char ineq_str[2];
		if(inner_addr < outer_addr)
			ineq_str[0] = '<';
		else if(inner_addr == outer_addr)
			ineq_str[0] = '=';
		else
			ineq_str[0] = '>';

		if(inner_addr + inner_len > outer_addr + outer_len)
			ineq_str[1] = '>';
		else if(inner_addr + inner_len == outer_addr + outer_len)
			ineq_str[1] = '=';
		else
			ineq_str[1] = '<';

		debug_print("inner_start[%lx] %c outer_start[%lx] | inner_end[%lx] %c outer_end[%lx]\n",
				inner_addr, ineq_str[0], outer_addr, inner_addr + inner_len, ineq_str[1],
				outer_addr + outer_len);
#endif
		return 0;
	}
	else
		return 1;
}

static inline char* stringify_verb(int opcode)
{
	switch(opcode) {
		case IBV_WR_RDMA_WRITE:
		       return "RDMA_WRITE";
		       break;
		case IBV_WR_RDMA_WRITE_WITH_IMM:
		       return "RDMA_WRITE_IMM";
		       break;
		case IBV_WR_SEND:
		       return "RDMA_SEND";
		       break;
		case IBV_WR_SEND_WITH_IMM:
		       return "RDMA_SEND_IMM";
		       break;
		case IBV_WR_RDMA_READ:
		       return "RDMA_READ";
		       break;
		case IBV_WR_ATOMIC_FETCH_AND_ADD:
		       return "ATOMIC_FETCH_AND_ADD";
		       break;
		case IBV_WR_ATOMIC_CMP_AND_SWP:
		       return "ATOMIC_CMP_AND_SWP";
		       break;
		default:
		       return "UNDEFINED OPCODE";
	}
}

static inline int op_one_sided(int opcode)
{
	if((opcode == IBV_WR_RDMA_READ) || (opcode == IBV_WR_RDMA_WRITE)
		       || (opcode == IBV_WR_RDMA_WRITE_WITH_IMM)) {
		return 1;
	}
	else
		return 0;
}

//increments last work request id for a specified connection
//send == 0 --> wr type is receive
//send == 1 --> wr type is send
static inline uint32_t next_wr_id(struct conn_context *ctx, int send)
{
	//we maintain seperate wr_ids for send/rcv queues since
	//there is no ordering between their work requests
	if(send) {
		if(send >= 1) {
			return __sync_add_and_fetch(&ctx->last_send, 0x00000001); 
			//return ++ctx->last_send;
		}
#if 0
		else if(send == 2) {
			ctx->last_send++;
			ctx->last_msg = ctx->last_send;
			return ctx->last_send;
		}
#endif
		else
			rc_die("undefined 'send' flag");
	}
	else
		return ++ctx->last_rcv;

	return 0;
}

static inline uint32_t current_wr_id(struct conn_context *ctx, int send) {
	//we maintain seperate wr_ids for send/rcv queues since
	//there is no ordering between their work requests
	if(send) {
		if(send >= 1) {
			return __sync_add_and_fetch(&ctx->last_send, 0x00000000);
			//return ++ctx->last_send;
		}
#if 0
		else if(send == 2) {
			ctx->last_send++;
			ctx->last_msg = ctx->last_send;
			return ctx->last_send;
		}
#endif
		else
			rc_die("undefined 'send' flag");
	}
	else
		return ctx->last_rcv;

	return 0;
}

//verb wrappers
void IBV_WRAPPER_OP_SYNC_ALL(rdma_meta_t *meta, int local_id, int remote_id, int opcode);
void IBV_WRAPPER_OP_ASYNC_ALL(rdma_meta_t *meta, int local_id, int remote_id, int opcode);
void IBV_WRAPPER_OP_SYNC(int sockfd, rdma_meta_t *meta, int local_id, int remote_id, int opcode);
uint32_t IBV_WRAPPER_OP_ASYNC(int sockfd, rdma_meta_t *meta, int local_id, int remote_id, int opcode);
void IBV_WRAPPER_OP_SYNC_WITH_FLAG(int sockfd, rdma_meta_t *meta, int local_id, int remote_id, int opcode, unsigned int send_flag);
uint32_t IBV_WRAPPER_OP_ASYNC_WITH_FLAG(int sockfd, rdma_meta_t *meta, int local_id, int remote_id, int opcode, unsigned int send_flag);

//messaging wrappers
void IBV_WRAPPER_SEND_MSG_SYNC(int sockfd, int buffer_id, int solicit);
uint32_t IBV_WRAPPER_SEND_MSG_ASYNC(int sockfd, int buffer_id, int solicit);

//verb waiting
void IBV_AWAIT_RESPONSE(int sockfd, uint64_t app_id);
void IBV_AWAIT_PER_LIBFS_RESPONSE(int sockfd, uint64_t app_id, int libfs_id);
void IBV_AWAIT_ACK_POLLING(int sockfd, uint32_t *bit);
void IBV_AWAIT_WORK_COMPLETION(int sockfd, uint32_t wr_id);
void IBV_AWAIT_WORK_COMPLETION_WITH_POLLING(int sockfd, uint32_t wr_id, int low_lat);
void IBV_AWAIT_ACK_SPINNING(int sockfd, uint32_t *bit);
void IBV_AWAIT_PENDING_WORK_COMPLETIONS(int sockfd);

//pending messages
void register_pending(struct rdma_cm_id *id, uint64_t app_id);
void remove_pending(struct rdma_cm_id *id, struct app_response *p);
void update_pending(struct rdma_cm_id *id, uint64_t app_id);
void update_per_libfs_pending(struct rdma_cm_id *id, uint64_t app_id, int libfs_id);

//basic post operations
uint32_t send_rdma_operation(struct rdma_cm_id *id, rdma_meta_t *meta, int local_id, int remote_id, int opcode);
uint32_t send_rdma_operation_with_flag(struct rdma_cm_id *id, rdma_meta_t *meta, int local_id, int remote_id, int opcode, unsigned int send_flag);
uint32_t __send_rdma_operation(struct rdma_cm_id *id, rdma_meta_t *meta, int local_id, int remote_id, int opcode, unsigned int send_flag);
uint32_t send_message(struct rdma_cm_id *id, int buffer_id);
void receive_message(struct rdma_cm_id *id, int buffer_id);
void receive_imm(struct rdma_cm_id *id, int buffer_id);

#endif
