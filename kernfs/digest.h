#ifndef _DIGEST_H_
#define _DIGEST_H_
#include "global/global.h"
#include "concurrency/thpool.h"
#include "filesystem/shared.h"

struct rdma_memcpy_meta {
	uint64_t rdma_entry_cnt;
	struct list_head *rdma_read_list; // source
	struct list_head *rdma_write_list; // destination
};

typedef struct memcpy_meta {
	int is_single_blk;
	uint8_t to_dev;
	addr_t blk_nr;
	uint8_t *data;
	uint32_t size;
	uint32_t offset_in_blk; // Used only in unaligned case.
} memcpy_meta_t;

typedef struct memcpy_meta_array {
	uint64_t id; // Used for storing index of array.
	memcpy_meta_t *buf;
} memcpy_meta_array_t;

struct f_digest_worker_arg {
	uint8_t from_dev;
	uint8_t to_dev;
	f_replay_t *f_item;
	int libfs_id;
};

struct mc_arg {
	int libfs_id;
	uint64_t seqn;
	uint64_t fetch_seqn;
	int sender_sockfd;
	// uintptr_t buf_addr; // Not used any more.
	uint64_t n_memcpy_entries;
	uintptr_t ack_bit_addr;
#ifdef BATCH_MEMCPY_LIST
	int sge_cnt;
#endif

	// Pass to LibFS
	addr_t digest_start_blknr;
	uint32_t n_digested;
	uint64_t n_digested_blks;
	int reset_meta;
};

void digest_init(void);
#ifdef DIGEST_OPT_MERGE_WRITES
int digest_file(uint8_t from_dev, uint8_t to_dev, int libfs_id,
		uint32_t file_inum, offset_t offset, uint32_t length,
		addr_t blknr, struct rdma_memcpy_meta *rdma_memcpy_meta,
		struct memcpy_meta_array *memcpy_meta_list);
#else
int digest_file(uint8_t from_dev, uint8_t to_dev, int libfs_id,
		uint32_t file_inum, offset_t offset, uint32_t length,
		addr_t blknr);
#endif
int digest_inode(uint8_t from_dev, uint8_t to_dev, int libfs_id, uint32_t inum,
		 addr_t blknr);
int digest_unlink(uint8_t from_dev, uint8_t to_dev, int libfs_id,
		  uint32_t inum);
int digest_file_iovec(uint8_t from_dev, uint8_t to_dev, int libfs_id,
		      uint32_t file_inum, f_iovec_t *iovec);
void alloc_and_init_rdma_memcpy_list_pair(struct rdma_memcpy_meta *rm_meta);
void file_digest_worker(void *arg);
void send_digest_rdma_entries(struct list_head *rdma_read_list,
			      struct list_head *rdma_write_list);
void start_send_rdma_entries_workers(struct rdma_memcpy_meta *rm_meta);
int digest_allocate(uint8_t from_dev, uint8_t to_dev, int libfs_id,
		    uint32_t inum, uint64_t length);
void digest_replay_and_optimize(uint8_t from_dev, loghdr_meta_t *loghdr_meta,
				struct replay_list *replay_list);
void digest_log_from_replay_list(uint8_t from_dev, int libfs_id,
				 struct replay_list *replay_list,
				 struct memcpy_meta_array *memcpy_meta_list);
void print_digest_thpool_stat(void);
int persist_dirty_objects_nvm(int log_id);
#endif
