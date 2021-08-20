#ifndef _BLOCK_IO_H_
#define _BLOCK_IO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "mlfs/kerncompat.h"
#include "mlfs/mlfs_user.h"
#include "global/global.h"
#include "global/mem.h"
#include "global/types.h"
#include "global/defs.h"
#include "global/util.h"
#include "ds/list.h"
#include "ds/rbtree.h"
#include "buffer_head.h"
#include "filesystem/shared.h"

#define BH_CACHE_ALLOC 1
#define BH_NO_DATA_ALLOC 2

extern struct buffer_head *bh_hash[g_n_devices + 1];

void adjust_buffer_cache(struct buffer_head *bh);

extern struct block_device *g_bdev[g_n_devices + 1];

static inline int trylock_buffer(struct buffer_head *bh)
{
	return !pthread_mutex_trylock(&bh->b_lock);
}

static inline void lock_buffer(struct buffer_head *bh)
{
	might_sleep();
	if (!trylock_buffer(bh))
		pthread_mutex_lock(&bh->b_lock);
}

static inline void unlock_buffer(struct buffer_head *bh)
{
	pthread_mutex_unlock(&bh->b_lock);
}

static inline void get_bh(struct buffer_head *bh)
{
	__sync_fetch_and_add(&bh->b_count, 1);
}

static inline void put_bh(struct buffer_head *bh)
{
	__sync_fetch_and_sub(&bh->b_count, 1);
}

static inline int put_bh_and_read(struct buffer_head *bh)
{
	return __sync_sub_and_fetch(&bh->b_count, 1);
}

struct buffer_head *__getblk(struct block_device *, uint64_t, int);
static inline struct buffer_head *sb_getblk(uint8_t dev,
					    uint64_t block)
{
	return __getblk(g_bdev[dev], block, g_block_size_bytes);
}

void device_init(void);
void device_shutdown(void);

struct block_device *bdev_alloc(uint8_t dev_id, int blocksize_bits);
struct block_device *bdev_alloc_fast(uint8_t dev_id, int blocksize_bits);
void bdev_free(struct block_device *bdev);
void invalidate_bh_cache(uint8_t dev);
struct buffer_head *bh_get_sync_IO(uint8_t dev, addr_t block_nr, uint8_t mode);
int bh_submit_read_sync_IO(struct buffer_head *bh);
//struct buffer_head *get_bh_from_cache(uint8_t dev, addr_t block_nr, uint8_t mode);
void bh_release(struct buffer_head *bh);

int mlfs_write(struct buffer_head *bh);
int mlfs_write_opt(struct buffer_head *bh);
int mlfs_io_wait(uint8_t dev, int isread);
int mlfs_readahead(uint8_t dev, addr_t blockno, uint32_t io_size);
//int mlfs_commit(uint8_t dev, addr_t blockno, uint32_t offset,
//		uint32_t io_size, int flags);
int mlfs_commit(uint8_t dev);
int mlfs_persist(uint8_t dev, addr_t blockno, uint32_t offset, uint32_t io_size);
int submit_bh(int is_write, struct buffer_head *bh);
int write_dirty_buffer(struct buffer_head *bh);
int bh_submit_read(struct buffer_head *bh);
void brelse(struct buffer_head *bh);
void wait_on_buffer(struct buffer_head *bh, int isread);
void sync_all_buffers(struct block_device *bdev);
void sync_writeback_buffers(struct block_device *bdev);
void move_buffer_to_writeback(struct buffer_head *bh);
void remove_buffer_from_writeback(struct buffer_head *bh);

extern pthread_rwlock_t *bcache_rwlock;

static inline struct buffer_head *bh_find(block_key_t key)
{
	uint64_t tsc_begin;
	struct buffer_head *bh;

	pthread_rwlock_rdlock(bcache_rwlock);

	HASH_FIND(hash_handle, bh_hash[0], &key, sizeof(block_key_t), bh);

	pthread_rwlock_unlock(bcache_rwlock);

	return bh;
}

#if 0
static inline void *bh_add(uint8_t dev, struct buffer_head *bh)
{
	pthread_rwlock_wrlock(bcache_rwlock);

	HASH_ADD(hash_handle, bh_hash[0], b_blocknr, sizeof(addr_t), bh);

	pthread_rwlock_unlock(bcache_rwlock);
}
#endif

static inline void *bh_del(struct buffer_head *bh)
{
	pthread_rwlock_wrlock(bcache_rwlock);

	HASH_DELETE(hash_handle, bh_hash[0], bh);

	pthread_rwlock_unlock(bcache_rwlock);
}

extern char *g_dev_path[];

#ifdef __cplusplus
}
#endif

#endif
