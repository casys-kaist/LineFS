#ifndef _DEVICE_H_
#define _DEVICE_H_

#include "mlfs/kerncompat.h"
#include "mlfs/mlfs_user.h"
#include "ds/list.h"
#include "ds/rbtree.h"

#ifdef __cplusplus
extern "C" {
#endif

struct block_device {
	uint8_t b_devid ;
	unsigned long bd_flags; /* flags */

	uint32_t bd_blocksize;
	uint32_t bd_blocksize_bits;

	int bd_nr_free;
	pthread_mutex_t bd_bh_free_lock;
	struct list_head bd_bh_free;

	pthread_mutex_t bd_bh_dirty_lock;
	struct list_head bd_bh_dirty;

	pthread_mutex_t bd_bh_ioqueue_lock;
	struct list_head bd_bh_ioqueue;

	pthread_mutex_t bd_bh_root_lock;
	struct rb_root bd_bh_root;

	pthread_t bd_bh_io_thread;
	pthread_t bd_bh_writeback_thread;
	int bd_bh_io_wakeup_fd[2];
	int bd_bh_writeback_wakeup_fd[2];

	uint8_t *map_base_addr;
	struct storage_operations *storage_engine;
};


#ifdef __cplusplus
}
#endif

#endif
