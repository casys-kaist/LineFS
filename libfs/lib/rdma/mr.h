#ifndef RDMA_MR_H
#define RDMA_MR_H

#include "common.h"
#include "connection.h"

//memory region state
int mr_all_synced(struct conn_context *ctx);
int mr_all_recv(struct conn_context *ctx);
int mr_all_sent(struct conn_context *ctx);
int mr_local_ready(struct conn_context *ctx, int mr_id);
int mr_remote_ready(struct conn_context *ctx, int mr_id);
int mr_next_to_sync(struct conn_context *ctx);
void mr_register(struct conn_context *ctx, struct mr_context *mrs, int num_mrs, int msg_size);
void mr_prepare_msg(struct conn_context *ctx, int buffer, int msg_type);
uint64_t mr_local_addr(int sockfd, int mr_id);
uint64_t mr_remote_addr(int sockfd, int mr_id);

#endif
