#ifndef _MIGRATE_H_
#define _MIGRATE_H_

#include "filesystem/slru.h"

#ifdef __cplusplus
extern "C" {
#endif

//#define MIN_MIGRATE_ENTRY 512 
#define MIN_MIGRATE_ENTRY 1024
#define BLOCKS_PER_LRU_ENTRY (LRU_ENTRY_SIZE >> g_block_size_shift)
#define BLOCKS_TO_LRU_ENTRIES(x) ((x) / BLOCKS_PER_LRU_ENTRY)


typedef struct isolated_list {
	uint32_t n;
	struct list_head head;
	struct list_head fail_head;
} isolated_list_t;

int try_migrate_blocks(uint8_t from_dev, uint8_t to_dev, uint32_t nr_blocks, uint8_t force, int swap);
int migrate_blocks(uint8_t from_dev, uint8_t to_dev, isolated_list_t *migrate_list, int swap);
int try_writeback_blocks(uint8_t from_dev, uint8_t to_dev);
//int writeback_blocks(uint8_t from_dev, uint8_t to_dev, isolated_list_t *wb_list);
int update_slru_list(uint8_t dev, lru_node_t node);
int update_slru_list_from_digest(uint8_t dev, lru_key_t k, lru_val_t v);

extern lru_node_t *g_lru_hash[g_n_devices + 1];
extern struct lru g_lru[g_n_devices + 1];
extern struct lru g_stage_lru[g_n_devices + 1];
extern struct lru g_swap_lru[g_n_devices + 1];

extern pthread_spinlock_t lru_spinlock;


#ifdef __cplusplus
}
#endif

#endif
