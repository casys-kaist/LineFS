#ifndef _NIC_LIMIT_RATE_H_
#define _NIC_LIMIT_RATE_H_
#include "global/global.h"
#include "global/types.h"
#include "ds/stdatomic.h"
#include "pipeline_common.h"

extern atomic_uint rate_limit_on;
extern uint64_t *primary_rate_limit_flag; // Used by primary. Limit flag set/unset by the next replica.
extern uint64_t primary_rate_limit_addr; // Used by replica 1. Rate limit flag address of the previous replica (primary). RDMA write to this address.

void init_rate_limiter(void);
void init_prefetch_rate_limiter(void);
void destroy_rate_limiter(void);
int nic_slab_full(void);
int nic_slab_empty(void);
void limit_all_libfs_req_rate(void);
void unlimit_all_libfs_req_rate(void);
void rate_limit_worker(void *arg);

static void set_rate_limit_flag(uintptr_t target_addr, int sockfd);
static void unset_rate_limit_flag(uintptr_t target_addr, int sockfd);
static void limit_req_rate_of_primary(void);
static void unlimit_req_rate_of_primary(void);
void limit_prefetch_rate(uint64_t sent_bytes);
#endif
