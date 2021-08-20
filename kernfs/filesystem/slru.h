#ifndef _SLRU_H_
#define _SLRU_H_

#include "filesystem/shared.h"
#include "global/types.h"
#include "ds/list.h"
#include "ds/uthash.h"

#ifdef __cplusplus
extern "C" {
#endif

/* LRU list for file data migration. 
 * For bulk IO, LRU list is coarse grained access tracking by the bin size.
 * It means eviction unit to lower layer of storage is the bin size.
 * If any offset with the bin size is accessed, 
 * then it moves to head of the list.
 */
#define LRU_ENTRY_SIZE 4096

typedef struct lru_node {
	lru_key_t key;
	lru_val_t val;

	mlfs_hash_t hh;
	// list for global LRU list
	struct list_head list;
	// list for per-inode list
	struct list_head per_inode_list;
	//uint32_t access_freq[LRU_ENTRY_SIZE / 4096];
	uint8_t sync;
} lru_node_t;

struct lru {
	struct list_head lru_head;
	uint64_t n;
};

extern lru_node_t *lru_hash;

int slru_upsert(struct inode *inode, struct list_head *lru_head, lru_key_t k, lru_val_t v);

#ifdef __cplusplus
}
#endif

#endif
