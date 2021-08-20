#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/epoll.h>

#include "device.h"

#include "global/global.h"
#include "global/mem.h"
#include "global/ncx_slab.h"
#include "io/block_io.h"
#include "storage/storage.h"
#include "concurrency/synchronization.h"

#ifdef KERNFS

#ifdef __x86_64__
#include "fs.h"
#elif __aarch64__
#include "nic/nic_fs.h"
#else
#error "Unsupported architecture."
#endif

#elif LIBFS
#include "filesystem/fs.h"
#endif

// static parameters
//static int buffer_free_threshold = 9;
//static int buffer_dirty_threshold = 9;
static int buffer_free_threshold = 90000;
static int buffer_dirty_threshold = 10000;
static int buffer_dirty_count = 0;
static int buffer_writeback_ms = 10000;

//struct storage_operations *storage_engine;
struct buffer_head *bh_hash[g_n_devices + 1];
pthread_rwlock_t *bcache_rwlock;

static void reclaim_buffer(struct buffer_head *bh);
struct buffer_head *buffer_alloc(struct block_device *bdev,
		addr_t block_nr, int page_size);

void device_init(void)
{
	int i;
	pthread_rwlockattr_t rwlattr;

	// dev_id = 1 - NVMM
	// dev_id = 2 - SSD
	// dev_id = 3 - HDD
	// dev_id > 4 - Per-application log
	// ...
	for (i = 1; i < g_n_devices + 1; i++) {
		mlfs_debug("dev id %d\n", i);
#ifndef USE_SSD
		if (i == g_ssd_dev)
			continue;
#endif
#ifndef USE_HDD
		if (i == g_hdd_dev)
			continue;
#endif
		if (i == g_root_dev) {
			g_bdev[i] = bdev_alloc(i, 12);
#if defined(NIC_OFFLOAD) && defined(NIC_SIDE)
                        // NIC RPC storage engine.
                        g_bdev[i]->storage_engine = &storage_nic_rpc;
                        g_bdev[i]->map_base_addr = NULL;
                        // g_bdev[i]->storage_engine->init(i, g_dev_path[i]); 

#else /* Not defined(NIC_OFFLOAD) && defined(NIC_SIDE) */
			g_bdev[i]->storage_engine = &storage_dax;
			g_bdev[i]->map_base_addr =
				g_bdev[i]->storage_engine->init(i, g_dev_path[i]);
#endif
		} else if (i == g_hdd_dev || i == g_ssd_dev) {
			g_bdev[i] = bdev_alloc(i, 12);
#if defined(NIC_OFFLOAD) && defined(NIC_SIDE)
                        // NIC : NIC has no block device.
                        assert(false); // TODO SSD and HDD not supported.
#else
			g_bdev[i]->storage_engine = &storage_aio;
			g_bdev[i]->map_base_addr = NULL;
			g_bdev[i]->storage_engine->init(i, g_dev_path[i]);
#endif
		}
		// libfs logs starting from devid 4
		else {
			printf("Initializing dev[%d]\n", i);
			g_bdev[i] = bdev_alloc_fast(i, 12);
#if defined(NIC_OFFLOAD) && defined(NIC_SIDE)
                        // NIC RPC storage engine.
                        /* It is required  because some storage engine operations
                         * are called with libfs devid.
                         */
                        g_bdev[i]->storage_engine = &storage_nic_rpc;
                        g_bdev[i]->map_base_addr = NULL;
                        // g_bdev[i]->storage_engine->init(i, g_dev_path[i]); 
#else
			g_bdev[i]->storage_engine = &storage_dax;
			g_bdev[i]->map_base_addr =
				g_bdev[i]->storage_engine->init(i, g_dev_path[i]);
#endif
		}
		bh_hash[i] = NULL;
	}

	bcache_rwlock = (pthread_rwlock_t *)mlfs_zalloc(sizeof(pthread_rwlock_t));
	pthread_rwlockattr_setpshared(&rwlattr, PTHREAD_PROCESS_SHARED);
	pthread_rwlock_init(bcache_rwlock, &rwlattr);

	return;
}

void device_shutdown(void)
{
	int i;
	for (i = 1; i < g_n_devices + 1; i++) {
		if (g_bdev[i])
			bdev_free(g_bdev[i]);
	}

	return;
}

///////////////////////////////////////////////////////////////////////////
// synchronous IO APIs

struct block_device *bdev_alloc_fast(uint8_t dev_id, int blocksize_bits)
{
	struct block_device *bdev;

	bdev = (struct block_device *)mlfs_alloc(sizeof(struct block_device));
	memset(bdev, 0, sizeof(struct block_device));

	bdev->b_devid = dev_id;
	bdev->bd_flags = 0;

	bdev->bd_blocksize = g_block_size_bytes;
	// hard-coded
	bdev->bd_blocksize_bits = 12;

	return bdev;
}

static struct buffer_head *buffer_alloc_fast(struct block_device *bdev,
		addr_t block_nr, uint8_t mode)
{
	// TODO: to offload buffer head copies to NVM, we can pre-register a
	// pre-allocated buffer cache
	struct buffer_head *bh;

	bh = (struct buffer_head *)mlfs_alloc(sizeof(struct buffer_head));
	if (!bh)
		return NULL;

	bh->b_bdev = bdev;
	bh->b_state = 0;
	bh->b_dev = bdev->b_devid;
	bh->b_data = NULL;
	bh->b_blocknr = block_nr;
	bh->b_count = 0;
	bh->b_size = 0;
	bh->b_offset = 0;

	INIT_LIST_HEAD(&bh->b_io_list);

	//pthread_spin_init(&bh->b_spinlock, PTHREAD_PROCESS_SHARED);

	return bh;
}

static inline struct buffer_head *bh_alloc_add(uint8_t dev,
		addr_t block_nr, uint32_t size, uint8_t mode)
{
	struct buffer_head *bh;
	struct block_device *bdev;

	bdev = g_bdev[dev];

	if (mode == BH_NO_DATA_ALLOC) {
		bh = buffer_alloc_fast(bdev, block_nr, mode);
		set_buffer_data_ref(bh);
	} else
		panic("deprecated code path\n");

	if (!bh)
		panic("Fail to allocate buffer cache\n");

	pthread_rwlock_wrlock(bcache_rwlock);

	HASH_ADD(hash_handle, bh_hash[dev], b_blocknr, sizeof(addr_t), bh);

	pthread_rwlock_unlock(bcache_rwlock);

	return bh;
}

struct buffer_head *bh_get_sync_IO(uint8_t dev, addr_t block_nr, uint8_t mode)
{
	struct buffer_head *bh;

	if (mode == BH_NO_DATA_ALLOC) {
		bh = buffer_alloc_fast(g_bdev[dev], block_nr, mode);
		set_buffer_data_ref(bh);
	} else
		panic("deprecated code path\n");

	__sync_fetch_and_add(&bh->b_count, 1);

	return bh;
}

#if 0
/* hash-based buffer management version. used for synchronous IO */
// This function will be retired. Fixing the code now.
struct buffer_head *get_bh_from_cache(block_key_t key, uint8_t mode)
{
	struct buffer_head *bh;

	if (mode == BUF_CACHE_NO_DATA_ALLOC)
		goto alloc;

	bh = bh_find(key);

	if (bh) {
		// clean up previous buffer_head used for a different-sized IO.
		if (bh->b_size != size) {
			int refcount;
			refcount = put_bh_and_read(bh);

			//mlfs_assert(refcount == 0);
			bh_del(bh);

			if (!buffer_data_ref(bh))
				mlfs_free(bh->b_data);

			goto alloc;
		} else {
			/* mlfs_assert(bh->b_blocknr == blockno); */
			__sync_fetch_and_add(&bh->b_count, 1);
			return bh;
		/* } */
	}

alloc:
	bh = bh_alloc_add(key, mode);
	__sync_fetch_and_add(&bh->b_count, 1);

	return bh;
}
#endif

int bh_submit_read_sync_IO(struct buffer_head *bh)
{
	int ret;
	struct storage_operations *storage_engine;

	mlfs_assert(bh);

	storage_engine = g_bdev[bh->b_dev]->storage_engine;

	if (bh->b_offset) {
		// unaligned IO is only allowed in NVM.
		// mlfs_assert(bh->b_dev != g_ssd_dev);
		// mlfs_assert(bh->b_dev != g_hdd_dev);
		ret = storage_engine->read_unaligned(bh->b_dev, bh->b_data,
				bh->b_blocknr, bh->b_offset, bh->b_size);
	} else {
		ret = storage_engine->read(bh->b_dev, bh->b_data,
				bh->b_blocknr, bh->b_size);
	}

	if (ret != bh->b_size)
		panic("fail to read storage\n");

	set_buffer_uptodate(bh);

	return ret;
}

#if 0
struct buffer_head *mlfs_read_sync_IO(uint8_t dev, addr_t block_nr,
		uint32_t size, int *ret)
{
	struct buffer_head *b;
	struct storage_operations *storage_engine;

	assert(size == g_block_size_bytes);
	//mlfs_assert(blockno <= disk_sb[dev].size);

	b = get_bh_sync_IO(dev, block_nr, BUF_CACHE_ALLOC);

	//pthread_spin_lock(&b->b_spinlock);

	/* if (b->b_size != size) { */
	/*  mlfs_debug("block size %u, requested %u\n", b->b_size, size); */
	/*  panic("block size mismatch\n"); */
	/* } */

	if (buffer_uptodate(b)) {
		*ret = 0;
		return b;
	}

	storage_engine = g_bdev[dev]->storage_engine;
	*ret = storage_engine->read(dev, b->b_data, blockno, size);
	/* if (*ret != b->b_size) */
	/*  panic("fail to read storage\n"); */
	assert(*ret == g_block_size_bytes);

	//mlfs_debug("READ: %lx size %u\n", blockno, size);

	*ret = 0;
	set_buffer_uptodate(b);

	//pthread_spin_unlock(&b->b_spinlock);

	return b;
}
#endif

int mlfs_write(struct buffer_head *b)
{
	int ret;
	struct storage_operations *storage_engine;

	mlfs_assert(b->b_size > 0);

	//mlfs_assert(b->b_blocknr + (b->b_size >> g_block_size_shift) <= disk_sb[b->b_dev].size);

	storage_engine = g_bdev[b->b_dev]->storage_engine;
	if (b->b_offset)
		ret = storage_engine->write_unaligned(b->b_dev, b->b_data,
				b->b_blocknr, b->b_offset, b->b_size);
	else
		ret = storage_engine->write(b->b_dev, b->b_data, b->b_blocknr, b->b_size);

	if (ret != b->b_size)
		panic("fail to write storage\n");

	set_buffer_uptodate(b);

	ret = 0;

	return ret;
}


int mlfs_write_opt(struct buffer_head *b)
{
	int ret;
	struct storage_operations *storage_engine;

	mlfs_assert(b->b_size > 0);

	//mlfs_assert(b->b_blocknr + (b->b_size >> g_block_size_shift) <= disk_sb[b->b_dev].size);

	storage_engine = g_bdev[b->b_dev]->storage_engine;
	if (b->b_offset)
		ret = storage_engine->write_opt_unaligned(b->b_dev, b->b_data, 
	b->b_blocknr, b->b_offset, b->b_size);
	else
		ret = storage_engine->write_opt(b->b_dev, b->b_data, b->b_blocknr, b->b_size);

	if (ret != b->b_size)
		panic("fail to write storage\n");

	set_buffer_uptodate(b);

	ret = 0;

	return ret;
}

void bh_release(struct buffer_head *bh)
{
	int refcount;

	if (bh == NULL)
		return;

	refcount = put_bh_and_read(bh);
	if (refcount >= 1)
		goto out;

	mlfs_assert(refcount == 0);

	//bh_del(bh);

	//TODO: Add to freelist and batching the free operation.
	mlfs_free(bh);
out:
	return;
}

int mlfs_io_wait(uint8_t dev, int read)
{
#ifndef USE_SSD
	if (dev == g_ssd_dev)
		return 0;
#endif

	struct storage_operations *storage_engine;

	storage_engine = g_bdev[dev]->storage_engine;

	if (storage_engine->wait_io)
		storage_engine->wait_io(dev, read);

	return 0;
}

int mlfs_readahead(uint8_t dev, addr_t blockno, uint32_t io_size)
{
#ifndef USE_SSD
	if (dev == g_ssd_dev)
		return 0;
#endif

	struct storage_operations *storage_engine;
	storage_engine = g_bdev[dev]->storage_engine;

	if (storage_engine->wait_io)
		storage_engine->readahead(dev, blockno, io_size);

	return 0;
}

#if 0
int mlfs_commit(uint8_t dev, addr_t blockno, uint32_t offset,
		uint32_t io_size, int flags)
{
	assert(dev == g_log_dev || dev == g_root_dev);

	struct storage_operations *storage_engine;
	storage_engine = g_bdev[dev]->storage_engine;

	storage_engine->commit(dev, blockno, offset, io_size, flags);

	return 0;
}
#endif

int mlfs_commit(uint8_t dev)
{
	assert(dev == g_log_dev || dev == g_root_dev);

	struct storage_operations *storage_engine;
	storage_engine = g_bdev[dev]->storage_engine;

	storage_engine->commit(dev);

	return 0;
}

/**
 * Used to persist replicated log to NVM.
 */
int mlfs_persist(uint8_t dev, addr_t blockno, uint32_t offset,
		uint32_t io_size)
{
	assert(dev == g_log_dev);

	struct storage_operations *storage_engine;
	storage_engine = g_bdev[dev]->storage_engine;

        storage_engine->persist(dev, blockno, offset, io_size);

	return 0;
}

// synchronous IO APIs
///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
// asynchronous IO APIs
static int device_read(uint8_t dev, addr_t blocknr, int count, int blk_size,
					 void *buf)
{
	int ret;
	struct storage_operations *storage_engine;

	mlfs_assert(blk_size == g_block_size_bytes);
	mlfs_assert(count == 1);

	storage_engine = g_bdev[dev]->storage_engine;
	ret = storage_engine->read(dev, (uint8_t *)buf, blocknr, g_block_size_bytes);
	if (ret < 0) {
		return -1;
	}

	return ret;
}

static int device_write(uint8_t dev, addr_t blocknr, int count, int blk_size,
			void *buf)
{
	int ret;
	struct storage_operations *storage_engine;

	mlfs_assert(blk_size == g_block_size_bytes);
	mlfs_assert(count == 1);

	storage_engine = g_bdev[dev]->storage_engine;
	ret = storage_engine->write(dev, (uint8_t *)buf, blocknr, g_block_size_bytes);

	if (ret < 0) {
		return -1;
	}
	return ret;
}

#if 1
static struct buffer_head *__buffer_search(struct rb_root *root,
						 uint64_t blocknr)
{
	struct rb_node *_new = root->rb_node;

	/* Figure out where to put new node */
	while (_new) {
		struct buffer_head *bh =
				container_of(_new, struct buffer_head, b_rb_node);
		int64_t result = blocknr - bh->b_blocknr;

		if (result < 0)
			_new = _new->rb_left;
		else if (result > 0)
			_new = _new->rb_right;
		else
			return bh;
	}

	return NULL;
}

static int buffer_blocknr_cmp(struct rb_node *a, struct rb_node *b)
{
	struct buffer_head *a_bh, *b_bh;
	a_bh = container_of(a, struct buffer_head, b_rb_node);
	b_bh = container_of(b, struct buffer_head, b_rb_node);

	if ((int64_t)(a_bh->b_blocknr - b_bh->b_blocknr) < 0)
		return -1;
	if ((int64_t)(a_bh->b_blocknr - b_bh->b_blocknr) > 0)
		return 1;
	return 0;
}
#endif

static struct buffer_head *buffer_search(struct block_device *bdev,
					 addr_t blocknr)
{
	struct buffer_head *bh;
	struct rb_root *root;
	root = &bdev->bd_bh_root;
	pthread_mutex_lock(&bdev->bd_bh_root_lock);
	bh = __buffer_search(root, blocknr);
	pthread_mutex_unlock(&bdev->bd_bh_root_lock);

	return bh;
}

static void buffer_insert(struct block_device *bdev,
				struct buffer_head *bh)
{
	pthread_mutex_lock(&bdev->bd_bh_root_lock);
	rb_insert(&bdev->bd_bh_root, &bh->b_rb_node, buffer_blocknr_cmp);
	pthread_mutex_unlock(&bdev->bd_bh_root_lock);
}

static void __buffer_remove(struct block_device *bdev,
				struct buffer_head *bh)
{
	rb_erase(&bh->b_rb_node, &bdev->bd_bh_root);
}

static void *buffer_io_thread(void *arg);
static void *buffer_writeback_thread(void *arg);

struct block_device *bdev_alloc(uint8_t dev_id, int blocksize_bits)
{
	int ret;
	struct block_device *bdev;

	bdev = (struct block_device *)mlfs_alloc(sizeof(struct block_device));
	memset(bdev, 0, sizeof(struct block_device));

	bdev->b_devid = dev_id;
	bdev->bd_flags = 0;

	bdev->bd_blocksize = g_block_size_bytes;
	// hard-coded
	bdev->bd_blocksize_bits = 12;

	INIT_LIST_HEAD(&bdev->bd_bh_free);
	INIT_LIST_HEAD(&bdev->bd_bh_dirty);
	INIT_LIST_HEAD(&bdev->bd_bh_ioqueue);

	pthread_mutex_init(&bdev->bd_bh_free_lock, NULL);
	pthread_mutex_init(&bdev->bd_bh_dirty_lock, NULL);
	pthread_mutex_init(&bdev->bd_bh_ioqueue_lock, NULL);
	pthread_mutex_init(&bdev->bd_bh_root_lock, NULL);

#if 0
	ret = pipe(bdev->bd_bh_writeback_wakeup_fd);
	if (ret < 0)
		panic("cannot create writeback wakeup fd\n");

	pthread_create(&bdev->bd_bh_writeback_thread, NULL,
			buffer_writeback_thread, bdev);

	ret = pipe(bdev->bd_bh_io_wakeup_fd);
	if (ret < 0)
		panic("cannot create io wakeup fd\n");

	pthread_create(&bdev->bd_bh_io_thread, NULL,
			buffer_io_thread, bdev);
#endif

	return bdev;
}

static void bdev_writeback_thread_notify(struct block_device *bdev)
{
	int ret;
	signed char test_byte = 1;
	ret = write(bdev->bd_bh_writeback_wakeup_fd[1], &test_byte, sizeof(test_byte));
}

static void bdev_writeback_thread_notify_exit(struct block_device *bdev)
{
	int ret;
	signed char test_byte = -1;
	ret = write(bdev->bd_bh_writeback_wakeup_fd[1], &test_byte, sizeof(test_byte));
}

static signed char bdev_writeback_thread_read_notify(struct block_device *bdev)
{
	int ret;
	signed char test_byte = 0;
	ret = read(bdev->bd_bh_writeback_wakeup_fd[0], &test_byte, sizeof(test_byte));
	return test_byte;
}

static int bdev_is_notify_exiting(signed char byte)
{
	if (byte < 0)
		return 1;
	return 0;
}

static void bdev_io_thread_notify(struct block_device *bdev)
{
	int ret;
	signed char test_byte = 1;
	ret = write(bdev->bd_bh_io_wakeup_fd[1], &test_byte, sizeof(test_byte));
}

static void bdev_io_thread_notify_exit(struct block_device *bdev)
{
	int ret;
	signed char test_byte = -1;
	ret = write(bdev->bd_bh_io_wakeup_fd[1], &test_byte, sizeof(test_byte));
}

static signed char bdev_io_thread_read_notify(struct block_device *bdev)
{
	int ret;
	signed char test_byte = 0;
	ret = read(bdev->bd_bh_io_wakeup_fd[0], &test_byte, sizeof(test_byte));
	return test_byte;
}

static int sync_dirty_buffer(struct buffer_head *bh);
static void detach_bh_from_freelist(struct buffer_head *bh);
static void buffer_free(struct buffer_head *bh);

void bdev_free(struct block_device *bdev)
{
	struct rb_node *node;

#if 0
	bdev_writeback_thread_notify_exit(bdev);
	pthread_join(bdev->bd_bh_writeback_thread, NULL);

	bdev_io_thread_notify_exit(bdev);
	pthread_join(bdev->bd_bh_io_thread, NULL);
#endif

	pthread_mutex_lock(&bdev->bd_bh_root_lock);
	while ((node = rb_first(&bdev->bd_bh_root))) {
		struct buffer_head *bh = rb_entry(
				node, struct buffer_head, b_rb_node);
		//wait_on_buffer(bh);
		detach_bh_from_freelist(bh);
		__buffer_remove(bdev, bh);
		buffer_free(bh);
	}
	pthread_mutex_unlock(&bdev->bd_bh_root_lock);

	if (bdev->bd_nr_free != 0)
		fprintf(stderr, "bdev->bd_nr_free == %d", bdev->bd_nr_free);

#if 0
	close(bdev->bd_bh_io_wakeup_fd[0]);
	close(bdev->bd_bh_io_wakeup_fd[1]);

	close(bdev->bd_bh_writeback_wakeup_fd[0]);
	close(bdev->bd_bh_writeback_wakeup_fd[1]);
#endif

	pthread_mutex_destroy(&bdev->bd_bh_free_lock);
	pthread_mutex_destroy(&bdev->bd_bh_dirty_lock);
	pthread_mutex_destroy(&bdev->bd_bh_ioqueue_lock);
	pthread_mutex_destroy(&bdev->bd_bh_root_lock);

	mlfs_free(bdev);
}

void invalidate_bh_cache(uint8_t dev)
{
	struct block_device *bdev;
	struct buffer_head *bh;
	struct rb_node *node;
	bdev = g_bdev[dev];

	pthread_mutex_lock(&bdev->bd_bh_root_lock);
	for (node = rb_first(&bdev->bd_bh_root); node; node = rb_next(node)) {
		bh = rb_entry(node, struct buffer_head, b_rb_node);
		clear_buffer_uptodate(bh);
	}
	pthread_mutex_unlock(&bdev->bd_bh_root_lock);
}


void sync_writeback_buffers(struct block_device *bdev)
{
	if (buffer_dirty_count > buffer_dirty_threshold)
		bdev_writeback_thread_notify(bdev);
}

struct buffer_head *buffer_alloc(struct block_device *bdev,
		addr_t block_nr, int page_size)
{
	int ret;
	struct buffer_head *bh;
	bh = (struct buffer_head *)mlfs_alloc(
			sizeof(struct buffer_head) + g_block_size_bytes);
	if (!bh)
		return NULL;

	memset(bh, 0, sizeof(struct buffer_head));
	bh->b_bdev = bdev;
	bh->b_dev = bdev->b_devid;
	bh->b_data = (uint8_t *)(bh + 1);
	bh->b_size = page_size;
	bh->b_offset = 0;
	bh->b_blocknr = block_nr;
	bh->b_count = 0;

	//pthread_spin_init(&bh->b_spinlock, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&bh->b_lock, NULL);

	pthread_mutex_init(&bh->b_wait_mutex, NULL);

	INIT_LIST_HEAD(&bh->b_freelist);
	INIT_LIST_HEAD(&bh->b_dirty_list);
	INIT_LIST_HEAD(&bh->b_io_list);

	return bh;
}

static void buffer_free(struct buffer_head *bh)
{
	pthread_mutex_destroy(&bh->b_lock);

	pthread_mutex_destroy(&bh->b_wait_mutex);

	if (bh->b_count != 0)
		fprintf(stderr, "bh: %p b_count != 0, my pid: %d", bh, getpid());

	mlfs_free(bh);
}

static void attach_bh_to_freelist(struct buffer_head *bh)
{
	struct block_device *bdev = bh->b_bdev;
	if (list_empty(&bh->b_freelist)) {
		pthread_mutex_lock(&bdev->bd_bh_free_lock);
		list_add_tail(&bh->b_freelist, &bh->b_bdev->bd_bh_free);
		bh->b_bdev->bd_nr_free++;
		pthread_mutex_unlock(&bdev->bd_bh_free_lock);
	}
}

static void detach_bh_from_freelist(struct buffer_head *bh)
{
	struct block_device *bdev = bh->b_bdev;
	if (!list_empty(&bh->b_freelist)) {
		pthread_mutex_lock(&bdev->bd_bh_free_lock);
		list_del_init(&bh->b_freelist);
		bh->b_bdev->bd_nr_free--;
		pthread_mutex_unlock(&bdev->bd_bh_free_lock);
	}
}

static void remove_first_bh_from_freelist(struct block_device *bdev)
{
	struct buffer_head *bh;

	pthread_mutex_lock(&bdev->bd_bh_free_lock);
	if (list_empty(&bdev->bd_bh_free)) {
		pthread_mutex_unlock(&bdev->bd_bh_free_lock);
		return;
	}
	bh = list_first_entry(&bdev->bd_bh_free,
						struct buffer_head, b_freelist);
	list_del_init(&bh->b_freelist);
	bh->b_bdev->bd_nr_free--;
	pthread_mutex_unlock(&bdev->bd_bh_free_lock);

	pthread_mutex_lock(&bdev->bd_bh_root_lock);
	__buffer_remove(bdev, bh);
	pthread_mutex_unlock(&bdev->bd_bh_root_lock);
	buffer_free(bh);
}

void move_buffer_to_writeback(struct buffer_head *bh)
{
	struct block_device *bdev = bh->b_bdev;
	if (list_empty(&bh->b_dirty_list)) {
		pthread_mutex_lock(&bdev->bd_bh_dirty_lock);
		list_add_tail(&bh->b_dirty_list, &bh->b_bdev->bd_bh_dirty);
		buffer_dirty_count++;
		pthread_mutex_unlock(&bdev->bd_bh_dirty_lock);
	}
}

static struct buffer_head *
remove_first_buffer_from_writeback(struct block_device *bdev)
{
	struct buffer_head *bh;

	pthread_mutex_lock(&bdev->bd_bh_dirty_lock);
	if (list_empty(&bdev->bd_bh_dirty)) {
		pthread_mutex_unlock(&bdev->bd_bh_dirty_lock);
		return NULL;
	}
	bh = list_first_entry(&bdev->bd_bh_dirty,
						struct buffer_head, b_dirty_list);
	list_del_init(&bh->b_dirty_list);
	buffer_dirty_count--;
	pthread_mutex_unlock(&bdev->bd_bh_dirty_lock);

	return bh;
}

void remove_buffer_from_writeback(struct buffer_head *bh)
{
	struct block_device *bdev = bh->b_bdev;
	pthread_mutex_lock(&bdev->bd_bh_dirty_lock);
	if (!list_empty(&bh->b_dirty_list)) {
		list_del_init(&bh->b_dirty_list);
		buffer_dirty_count--;
	}
	pthread_mutex_unlock(&bdev->bd_bh_dirty_lock);
}

static void add_buffer_to_ioqueue(struct buffer_head *bh)
{
	struct block_device *bdev = bh->b_bdev;
	pthread_mutex_lock(&bdev->bd_bh_ioqueue_lock);
	if (list_empty(&bh->b_io_list))
		list_add_tail(&bh->b_io_list, &bh->b_bdev->bd_bh_ioqueue);

	pthread_mutex_unlock(&bdev->bd_bh_ioqueue_lock);
}

static struct buffer_head *
remove_first_buffer_from_ioqueue(struct block_device *bdev)
{
	struct buffer_head *bh;

	pthread_mutex_lock(&bdev->bd_bh_ioqueue_lock);
	if (list_empty(&bdev->bd_bh_ioqueue)) {
		pthread_mutex_unlock(&bdev->bd_bh_ioqueue_lock);
		return NULL;
	}
	bh = list_first_entry(&bdev->bd_bh_ioqueue,
						struct buffer_head, b_io_list);
	list_del_init(&bh->b_io_list);
	pthread_mutex_unlock(&bdev->bd_bh_ioqueue_lock);

	return bh;
}

static void try_to_drop_buffers(struct block_device *bdev)
{
	while (bdev->bd_nr_free > buffer_free_threshold) {
		remove_first_bh_from_freelist(bdev);
	}
}

static void reclaim_buffer(struct buffer_head *bh)
{
	try_to_drop_buffers(bh->b_bdev);
	attach_bh_to_freelist(bh);
}

void wait_on_buffer(struct buffer_head *bh, int isread)
{
	int ret;

	m_barrier();

	pthread_mutex_lock(&bh->b_wait_mutex);
	pthread_mutex_unlock(&bh->b_wait_mutex);

	mlfs_io_wait(bh->b_dev, isread);
}

/*
 * Submit an IO request.
 */
int __submit_bh(int is_write, struct buffer_head *bh)
{
	struct block_device *bdev = bh->b_bdev;

	if (is_write == 0) {
		int ret;
		ret = device_read(bdev->b_devid, bh->b_blocknr, 1,
					 bh->b_size, bh->b_data);
		if (ret < 0)
			bh->b_end_io(bh, 0);
		else
			bh->b_end_io(bh, 1);
	} else {
		int ret;
		ret = device_write(bdev->b_devid, bh->b_blocknr, 1,
					 bh->b_size, bh->b_data);
		if (ret < 0)
			bh->b_end_io(bh, 0);
		else
			bh->b_end_io(bh, 1);
	}

	return 0;
}

int submit_bh(int is_write, struct buffer_head *bh)
{
	if (is_write)
		set_buffer_async_write(bh);

	add_buffer_to_ioqueue(bh);
	bdev_io_thread_notify(bh->b_bdev);

	return 0;
}

void after_buffer_sync(struct buffer_head *bh, int uptodate)
{
	if (uptodate)
		set_buffer_uptodate(bh);

	put_bh(bh);
	remove_buffer_from_writeback(bh);
	clear_buffer_dirty(bh);
	attach_bh_to_freelist(bh);
	unlock_buffer(bh);

	m_barrier();

	pthread_mutex_unlock(&bh->b_wait_mutex);
}

void after_buffer_write(struct buffer_head *bh, int uptodate)
{
	if (uptodate)
		set_buffer_uptodate(bh);

	put_bh(bh);
	remove_buffer_from_writeback(bh);
	clear_buffer_dirty(bh);
	unlock_buffer(bh);

	m_barrier();

	pthread_mutex_unlock(&bh->b_wait_mutex);
}

void after_submit_read(struct buffer_head *bh, int uptodate)
{
	if (uptodate)
		set_buffer_uptodate(bh);

	put_bh(bh);
	unlock_buffer(bh);

	m_barrier();
	pthread_mutex_unlock(&bh->b_wait_mutex);
}

int bh_submit_read(struct buffer_head *bh)
{
	int ret;

	lock_buffer(bh);

	if (buffer_uptodate(bh)) {
		unlock_buffer(bh);
		return 0;
	}

	get_bh(bh);
	clear_buffer_dirty(bh);
	remove_buffer_from_writeback(bh);
	bh->b_end_io = after_submit_read;

	pthread_mutex_lock(&bh->b_wait_mutex);
	ret = submit_bh(READ, bh);
	return ret;
}

/*
 * return: 0 means synced and no longer dirty
 *         1 means still has refcount and shouldn't be delete from dirty_list
 */
static int sync_dirty_buffer(struct buffer_head *bh)
{
	if (!trylock_buffer(bh))
		return 1;

	if (buffer_dirty(bh)) {
		int ret = 1;
		mlfs_write(bh);
		if (bh->b_count == 0) {
			set_buffer_uptodate(bh);
			clear_buffer_dirty(bh);
			attach_bh_to_freelist(bh);
			ret = 0;
		}
		// shouldn't remove again since outer side will handle deletion
		// remove_buffer_from_writeback(bh);
		unlock_buffer(bh);
		return ret;
	} else {
		unlock_buffer(bh);
		return 0;
	}
}

/* Direct write without using the writeback thread */
int write_dirty_buffer(struct buffer_head *bh)
{
	int ret = 0;

	if (!trylock_buffer(bh))
		return ret;

	if (buffer_dirty(bh)) {
		get_bh(bh);
		bh->b_end_io = after_buffer_write;

		pthread_mutex_lock(&bh->b_wait_mutex);
		ret = submit_bh(WRITE, bh);
	} else {
		unlock_buffer(bh);
	}
	return ret;
}

struct buffer_head *__getblk(struct block_device *bdev, uint64_t block,
					 int bsize)
{
	struct buffer_head *bh;

	bh = buffer_search(bdev, block);
	if (bh) {
		detach_bh_from_freelist(bh);
		// comment out on purpose, otherwise dirty buffer still won't be written back
		//remove_buffer_from_writeback(bh);
		get_bh(bh);
		return bh;
	}

	/* buffer_alloc may perform synchronous writeback of dirty buffers */
	bh = buffer_alloc(bdev, block, bsize);
	if (bh == NULL)
		return NULL;

	buffer_insert(bdev, bh);

	get_bh(bh);
	return bh;
}

/*
 * Release the buffer_head.
 */
void brelse(struct buffer_head *bh)
{
	int refcount;
	if (bh == NULL)
		return;
	refcount = put_bh_and_read(bh);
	if (refcount >= 1)
		goto out;

	if (buffer_sync_read(bh)) {
		mlfs_free(bh);
		goto out;
	}

	mlfs_assert(refcount == 0);

	if (!buffer_dirty(bh))
		reclaim_buffer(bh);
	else
		move_buffer_to_writeback(bh);
out:
	return;
}

static void try_to_sync_buffers(struct block_device *bdev)
{
	uint32_t i = 0;
	while (1) {
		struct buffer_head *cur;
		cur = remove_first_buffer_from_writeback(bdev);
		if (cur) {
			sync_dirty_buffer(cur);
			//wait_on_buffer(cur, 0);
		} else
			break;
		i++;
	}

	if (bdev->b_devid == g_ssd_dev)
		mlfs_io_wait(g_ssd_dev, 0);

	mlfs_debug("%u blocks are synced\n", i);
}

void sync_all_buffers(struct block_device *bdev)
{
	uint32_t i = 0;
	struct buffer_head *cur;
	struct buffer_head *next;
	pthread_mutex_lock(&bdev->bd_bh_dirty_lock);
	list_for_each_entry_safe(cur, next, &bdev->bd_bh_dirty, b_dirty_list) {
		if (!sync_dirty_buffer(cur)) {
			list_del_init(&cur->b_dirty_list);
			buffer_dirty_count--;
			mlfs_debug("[dev %d], block %lu synced\n",
					cur->b_dev, cur->b_blocknr);
			++i;
		}
	}
	pthread_mutex_unlock(&bdev->bd_bh_dirty_lock);

	if (bdev->b_devid == g_ssd_dev)
		mlfs_io_wait(g_ssd_dev, 0);

	mlfs_debug("%u blocks are synced\n", i);
}

static void *buffer_writeback_thread(void *arg)
{
	struct epoll_event epev = {0};
	struct block_device *bdev = (struct block_device *)arg;
	int epfd = epoll_create(1);
	if (epfd < 0)
		return NULL;

	epev.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, bdev->bd_bh_writeback_wakeup_fd[0], &epev);

	while (1) {
		int ret;
		ret = epoll_wait(epfd, &epev, 1, buffer_writeback_ms);
		if (ret > 0) {
			/* We got an nofication. */
			signed char command;
			command = bdev_writeback_thread_read_notify(bdev);
			try_to_sync_buffers(bdev);
			if (bdev_is_notify_exiting(command))
				break;

			continue;
		}
		if (ret == 0) {
			/* just flush out the dirty data. */
			try_to_sync_buffers(bdev);
		}
		if (ret < 0 && errno != EINTR)
			break;

	}
	close(epfd);
	return NULL;
}

static void try_to_perform_io(struct block_device *bdev)
{
	while (1) {
		struct buffer_head *cur;
		cur = remove_first_buffer_from_ioqueue(bdev);
		if (cur) {
			__submit_bh(test_clear_buffer_async_write(cur),
						cur);
		} else
			break;

	}
}

static void *buffer_io_thread(void *arg)
{
	struct epoll_event epev = {0};
	struct block_device *bdev = (struct block_device *)arg;
	int epfd = epoll_create(1);
	if (epfd < 0)
		return NULL;

	epev.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, bdev->bd_bh_io_wakeup_fd[0], &epev);

	while (1) {
		int ret;
		ret = epoll_wait(epfd, &epev, 1, -1);
		if (ret > 0) {
			/* We got an nofication. */
			signed char command;
			command = bdev_io_thread_read_notify(bdev);

			try_to_perform_io(bdev);
			if (bdev_is_notify_exiting(command))
				break;

		}
		if (ret < 0 && errno != EINTR)
			break;

	}
	close(epfd);
	return NULL;
}
// asynchronous IO APIs
///////////////////////////////////////////////////////////////////////////
