#include "mlfs/mlfs_user.h"
#include "digest.h"
#include "extents.h"
#include "storage/storage.h"
#include "io/device.h"
#include "io/balloc.h"
#ifdef DIGEST_OPT_MERGE_WRITES
#include "nic/host_memcpy.h"
#include "storage/storage.h" // nic_slab
#endif

#define _min(a, b)                                                             \
	({                                                                     \
		__typeof__(a) _a = a;                                          \
		__typeof__(b) _b = b;                                          \
		_a < _b ? _a : _b;                                             \
	})
#define ALLOC_SIZE (1 << 15)

threadpool thread_pool_digest_rdma_memcpy;
threadpool file_digest_thread_pool;
uint16_t *inode_version_table;
pthread_spinlock_t g_ext_get_block_lock;

int persist_dirty_objects_nvm(int log_id)
{
	struct rb_node *node;

	mlfs_debug("persisting dirty objects for peer %d\n", log_id);

	pthread_spin_lock(&g_ext_get_block_lock);

	// flush extent tree changes
	sync_all_buffers(g_bdev[g_root_dev]);

	// save dirty inodes
	// pthread_mutex_lock(&inode_dirty_mutex);
	pthread_spin_lock(&inode_dirty_mutex);

	// flush writes to NVM (only relevant if writes are issued
	// asynchronously using a DMA engine)
	// mlfs_commit(g_root_dev);

	for (node = rb_first(&sb[g_root_dev]->s_dirty_root[log_id]); node;
	     node = rb_next(node)) {
		struct inode *ip = rb_entry(node, struct inode, i_rb_node);
		mlfs_debug("[dev %d] write dirty inode %d size %lu for log_id "
			   "%d\n",
			   g_root_dev, ip->inum, ip->size, log_id);
		rb_erase(&ip->i_rb_node,
			 &get_inode_sb(g_root_dev, ip)->s_dirty_root[log_id]);
		write_ondisk_inode(ip);
	}

	pthread_spin_unlock(&inode_dirty_mutex);

	// save block allocation bitmap
	store_all_bitmap(g_root_dev, sb[g_root_dev]->s_blk_bitmap);

	pthread_spin_unlock(&g_ext_get_block_lock);

	return 0;
}

void digest_init(void)
{
	inode_version_table =
		(uint16_t *)mlfs_zalloc(sizeof(uint16_t) * NINODES);

	//TODO global locks are used.. It may limit throughput. Investigate these locks again to make per-libfs digestion parallel.
	pthread_spin_init(&g_ext_get_block_lock, PTHREAD_PROCESS_PRIVATE);

	if (mlfs_conf.digest_opt_fconcurrent) {
		file_digest_thread_pool = thpool_init(
			mlfs_conf.thread_num_digest_fconcurrent, "fconcur");
	}

	if (mlfs_conf.thread_num_digest_rdma_memcpy) {
		thread_pool_digest_rdma_memcpy = thpool_init(
			mlfs_conf.thread_num_digest_rdma_memcpy, "rdma_memcpy");
	}
}

#ifdef PROFILE_THPOOL
void print_digest_thpool_stat(void) {
	print_profile_result(thread_pool_digest_rdma_memcpy);
	print_profile_result(file_digest_thread_pool);
}
#endif

#ifdef DIGEST_OPT_MERGE_WRITES
#define DIGEST_OPT_MERGE_WRITES_RDMA_ENTRY_THRESHOLD 50

/**
 * Send digest rdma entries.
 */
void send_digest_rdma_entries(struct list_head *rdma_read_list,
			      struct list_head *rdma_write_list)
{
	struct rdma_meta_entry *meta_e, *_meta_e;
	uint32_t host_kernfs_id;
	int sock_fd;

	host_kernfs_id = nic_kid_to_host_kid(g_kernfs_id);
	sock_fd = g_kernfs_peers[host_kernfs_id]->sockfd[SOCK_BG];

	// START_TIMER(evt_dc);

	// Post read list.
	list_for_each_entry (meta_e, rdma_read_list, head) {
		IBV_WRAPPER_READ_ASYNC(sock_fd, meta_e->meta, MR_DRAM_BUFFER,
				       MR_NVM_LOG);
	}

	// Post write list.
	list_for_each_entry (meta_e, rdma_write_list, head) {
		if (list_is_last(&meta_e->head, rdma_write_list) &&
		    !mlfs_conf.persist_nvm)
			IBV_WRAPPER_WRITE_SYNC(sock_fd, meta_e->meta,
					       MR_DRAM_BUFFER, MR_NVM_SHARED);
		else
			IBV_WRAPPER_WRITE_ASYNC(sock_fd, meta_e->meta,
						MR_DRAM_BUFFER, MR_NVM_SHARED);
	}

	// flush to NVM. TODO OPTIMIZE it: currently read all entries again.
	if (mlfs_conf.persist_nvm) {
		list_for_each_entry (meta_e, rdma_write_list, head) {
			if (list_is_last(&meta_e->head, rdma_write_list))
				IBV_WRAPPER_READ_SYNC(sock_fd, meta_e->meta,
						      MR_DRAM_BUFFER,
						      MR_NVM_SHARED);
			else
				IBV_WRAPPER_READ_ASYNC(sock_fd, meta_e->meta,
						       MR_DRAM_BUFFER,
						       MR_NVM_SHARED);
		}
	}

	// END_TIMER(evt_dc);

	// free rdma entries.
	list_for_each_entry_safe (meta_e, _meta_e, rdma_read_list, head) {
		list_del(&meta_e->head);
		mlfs_free(meta_e->meta);
		mlfs_free(meta_e);
	}
	list_for_each_entry_safe (meta_e, _meta_e, rdma_write_list, head) {
		list_del(&meta_e->head);
		nic_slab_free((void *)meta_e->meta->sge_entries[0].addr);
		mlfs_free(meta_e->meta);
		mlfs_free(meta_e);
	}
}

static void send_rdma_entries_worker(void *arg)
{
	struct rdma_memcpy_meta *rm_meta;

	rm_meta = (struct rdma_memcpy_meta *)arg;
	send_digest_rdma_entries(rm_meta->rdma_read_list,
				 rm_meta->rdma_write_list);

	mlfs_free(rm_meta->rdma_read_list);
	mlfs_free(rm_meta->rdma_write_list);
	mlfs_free(arg);
}

void start_send_rdma_entries_workers(struct rdma_memcpy_meta *rm_meta)
{
	struct rdma_memcpy_meta *m_arg;

	m_arg = mlfs_zalloc(sizeof(struct rdma_memcpy_meta));
	m_arg->rdma_read_list = rm_meta->rdma_read_list;
	m_arg->rdma_write_list = rm_meta->rdma_write_list;
	// We don't need it in send_digest_rdma_entries()
	// m_arg->rdma_entry_cnt = rm_meta->rdma_entry_cnt;

	thpool_add_work(thread_pool_digest_rdma_memcpy,
			send_rdma_entries_worker, (void *)m_arg);
}

void alloc_and_init_rdma_memcpy_list_pair(struct rdma_memcpy_meta *rm_meta)
{
	// Freed in send_rdma_entries_worker().
	rm_meta->rdma_read_list = mlfs_zalloc(sizeof(struct list_head));
	rm_meta->rdma_write_list = mlfs_zalloc(sizeof(struct list_head));
	INIT_LIST_HEAD(rm_meta->rdma_read_list);
	INIT_LIST_HEAD(rm_meta->rdma_write_list);
}

static int is_rdma_list_filled(struct rdma_memcpy_meta *meta)
{
	return meta->rdma_entry_cnt >
	       DIGEST_OPT_MERGE_WRITES_RDMA_ENTRY_THRESHOLD;
}

#endif /* DIGEST_OPT_MERGE_WRITES */

static void digest_each_log_entries(uint8_t from_dev, int libfs_id,
				    loghdr_meta_t *loghdr_meta)
{
	int i, ret;
	loghdr_t *loghdr;
	uint16_t nr_entries;
	uint64_t tsc_begin;

	nr_entries = loghdr_meta->loghdr->n;
	loghdr = loghdr_meta->loghdr;

	for (i = 0; i < nr_entries; i++) {
		if (enable_perf_stats)
			g_perf_stats.n_digest++;

		// mlfs_printf("digesting log entry with inum %d peer_id %d\n",
		// loghdr->inode_no[i], libfs_id);
		// parse log entries on types.
		switch (loghdr->type[i]) {
		case L_TYPE_INODE_CREATE:
		// ftruncate is handled by this case.
		case L_TYPE_INODE_UPDATE: {
			if (enable_perf_stats)
				tsc_begin = asm_rdtscp();

			ret = digest_inode(from_dev, g_root_dev, libfs_id,
					   loghdr->inode_no[i],
					   loghdr->blocks[i] +
						   loghdr_meta->hdr_blkno);
			mlfs_assert(!ret);

			if (enable_perf_stats)
				g_perf_stats.digest_inode_tsc +=
					asm_rdtscp() - tsc_begin;
			break;
		}
		case L_TYPE_DIR_ADD:
		case L_TYPE_DIR_RENAME:
		case L_TYPE_DIR_DEL:
		case L_TYPE_FILE: {
			uint8_t dest_dev = g_root_dev;
			int rand_val;
			lru_key_t k;

			if (enable_perf_stats)
				tsc_begin = asm_rdtscp();
#ifdef USE_SSD
				// for NVM bypassing test
				// dest_dev = g_ssd_dev;
#endif
#ifndef DIGEST_OPT_MERGE_WRITES
			ret = digest_file(from_dev, dest_dev, libfs_id,
					  loghdr->inode_no[i], loghdr->data[i],
					  loghdr->length[i],
					  loghdr->blocks[i] +
						  loghdr_meta->hdr_blkno);
#else /* ! DIGEST_OPT_MERGE_WRITES */
			// DIGEST_OPT_MERGE_WRITES does not take this path.
			// It always goes with DIGEST_OPT flag.
			mlfs_assert(false);
#endif /* ! DIGEST_OPT_MERGE_WRITES */
			mlfs_assert(!ret);

			if (enable_perf_stats)
				g_perf_stats.digest_file_tsc +=
					asm_rdtscp() - tsc_begin;
			break;
		}
		case L_TYPE_UNLINK: {
			if (enable_perf_stats)
				tsc_begin = asm_rdtscp();

			ret = digest_unlink(from_dev, g_root_dev, libfs_id,
					    loghdr->inode_no[i]);
			mlfs_assert(!ret);

			if (enable_perf_stats)
				g_perf_stats.digest_inode_tsc +=
					asm_rdtscp() - tsc_begin;
			break;
		}
		case L_TYPE_ALLOC: {
			ret = digest_allocate(from_dev, g_root_dev, libfs_id,
					      loghdr->inode_no[i],
					      loghdr->data[i]);
			mlfs_assert(!ret);
			break;
		}
		default: {
			printf("%s: digest type %d\n", __func__,
			       loghdr->type[i]);
			panic("unsupported type of operation\n");
			break;
		}
		}
	}
}

void digest_replay_and_optimize(uint8_t from_dev, loghdr_meta_t *loghdr_meta,
				struct replay_list *replay_list)
{
	// START_TIMER(evt_dro);
	int i, ret;
	loghdr_t *loghdr;
	uint16_t nr_entries;

	nr_entries = loghdr_meta->loghdr->n;
	loghdr = loghdr_meta->loghdr;

	for (i = 0; i < nr_entries; i++) {
		switch (loghdr->type[i]) {
		case L_TYPE_INODE_CREATE:
		case L_TYPE_INODE_UPDATE: {
			i_replay_t search, *item;
			memset(&search, 0, sizeof(i_replay_t));

			search.key.inum = loghdr->inode_no[i];

			if (loghdr->type[i] == L_TYPE_INODE_CREATE)
				inode_version_table[search.key.inum]++;
			search.key.ver = inode_version_table[search.key.inum];

			HASH_FIND(hh, replay_list->i_digest_hash, &search.key,
				  sizeof(replay_key_t), item);
			if (!item) {
				item = (i_replay_t *)mlfs_zalloc(
					sizeof(i_replay_t));
				item->key = search.key;
				item->node_type = NTYPE_I;
				list_add_tail(&item->list, &replay_list->head);

				// tag the inode coalecing starts from inode
				// creation. This is crucial information to
				// decide whether unlink can skip or not.
				if (loghdr->type[i] == L_TYPE_INODE_CREATE)
					item->create = 1;
				else
					item->create = 0;

				HASH_ADD(hh, replay_list->i_digest_hash, key,
					 sizeof(replay_key_t), item);
				mlfs_debug("[INODE] inum %u (ver %u) - create "
					   "%d\n",
					   item->key.inum, item->key.ver,
					   item->create);
			}
			// move blknr to point the up-to-date inode snapshot in
			// the log.
			item->blknr =
				loghdr->blocks[i] + loghdr_meta->hdr_blkno;
			if (enable_perf_stats)
				g_perf_stats.n_digest++;
			break;
		}
		case L_TYPE_DIR_DEL:
		case L_TYPE_DIR_RENAME:
		case L_TYPE_DIR_ADD:
		case L_TYPE_FILE: {
			f_replay_t search, *item;
			f_iovec_t *f_iovec;
			f_blklist_t *_blk_list;
			lru_key_t k;
			offset_t iovec_key;
			int found = 0;

			memset(&search, 0, sizeof(f_replay_t));
			search.key.inum = loghdr->inode_no[i];
			search.key.ver =
				inode_version_table[loghdr->inode_no[i]];

			HASH_FIND(hh, replay_list->f_digest_hash, &search.key,
				  sizeof(replay_key_t), item);
			if (!item) {
				item = (f_replay_t *)mlfs_zalloc(
					sizeof(f_replay_t));
				item->key = search.key;

				HASH_ADD(hh, replay_list->f_digest_hash, key,
					 sizeof(replay_key_t), item);

				INIT_LIST_HEAD(&item->iovec_list);
				item->node_type = NTYPE_F;
				item->iovec_hash = NULL;
				list_add_tail(&item->list, &replay_list->head);
			}
#ifndef EXPERIMENTAL
#ifdef IOMERGE
			// IO data is merged if the same offset found.
			// Reduce amount IO when IO data has locality such as
			// Zipf dist.
			// FIXME: currently iomerge works correctly when IO size
			// is 4 KB and aligned.
			iovec_key = ALIGN_FLOOR(loghdr->data[i],
						g_block_size_bytes);

			if (loghdr->data[i] % g_block_size_bytes != 0 ||
			    loghdr->length[i] != g_block_size_bytes)
				panic("IO merge is not support current IO "
				      "pattern\n");

			HASH_FIND(hh, item->iovec_hash, &iovec_key,
				  sizeof(offset_t), f_iovec);

			if (f_iovec && (f_iovec->length == loghdr->length[i])) {
				f_iovec->offset = iovec_key;
				f_iovec->blknr = loghdr->blocks[i] +
						 loghdr_meta->hdr_blkno;
				// TODO: merge data from loghdr->blocks to
				// f_iovec buffer.
				found = 1;
			}

			if (!found) {
				f_iovec = (f_iovec_t *)mlfs_zalloc(
					sizeof(f_iovec_t));
				f_iovec->length = loghdr->length[i];
				f_iovec->offset = loghdr->data[i];
				f_iovec->blknr = loghdr->blocks[i] +
						 loghdr_meta->hdr_blkno;
				INIT_LIST_HEAD(&f_iovec->list);
				list_add_tail(&f_iovec->list,
					      &item->iovec_list);

				f_iovec->hash_key = iovec_key;
				HASH_ADD(hh, item->iovec_hash, hash_key,
					 sizeof(offset_t), f_iovec);
			}
#else
			f_iovec = (f_iovec_t *)mlfs_zalloc(sizeof(f_iovec_t));
			f_iovec->length = loghdr->length[i];
			f_iovec->offset = loghdr->data[i];
			f_iovec->blknr =
				loghdr->blocks[i] + loghdr_meta->hdr_blkno;
			INIT_LIST_HEAD(&f_iovec->list);
			list_add_tail(&f_iovec->list, &item->iovec_list);
#endif // IOMERGE

#else // EXPERIMENTAL
      // Experimental feature: merge contiguous small writes to
      // a single write one.
			mlfs_debug("new log block %lu\n",
				   loghdr->blocks[i] + loghdr_meta->hdr_blkno);
			_blk_list =
				(f_blklist_t *)mlfs_zalloc(sizeof(f_blklist_t));
			INIT_LIST_HEAD(&_blk_list->list);

			// FIXME: Now only support 4K aligned write.
			_blk_list->n =
				(loghdr->length[i] >> g_block_size_shift);
			_blk_list->blknr =
				loghdr->blocks[i] + loghdr_meta->hdr_blkno;

			if (!list_empty(&item->iovec_list)) {
				f_iovec = list_last_entry(&item->iovec_list,
							  f_iovec_t, list);

				// Find the case where io_vector can be
				// coalesced.
				if (f_iovec->offset + f_iovec->length ==
				    loghdr->data[i]) {
					f_iovec->length += loghdr->length[i];
					f_iovec->n_list++;

					mlfs_debug("block is merged %u\n",
						   _blk_list->blknr);
					list_add_tail(&_blk_list->list,
						      &f_iovec->iov_blk_list);
				} else {
					mlfs_debug("new f_iovec %lu\n",
						   loghdr->data[i]);
					// cannot coalesce io_vector. allocate
					// new one.
					f_iovec = (f_iovec_t *)mlfs_zalloc(
						sizeof(f_iovec_t));
					f_iovec->length = loghdr->length[i];
					f_iovec->offset = loghdr->data[i];
					f_iovec->blknr = loghdr->blocks[i] +
							 loghdr_meta->hdr_blkno;
					INIT_LIST_HEAD(&f_iovec->list);
					INIT_LIST_HEAD(&f_iovec->iov_blk_list);

					list_add_tail(&_blk_list->list,
						      &f_iovec->iov_blk_list);
					list_add_tail(&f_iovec->list,
						      &item->iovec_list);
				}
			} else {
				f_iovec = (f_iovec_t *)mlfs_zalloc(
					sizeof(f_iovec_t));
				f_iovec->length = loghdr->length[i];
				f_iovec->offset = loghdr->data[i];
				f_iovec->blknr = loghdr->blocks[i] +
						 loghdr_meta->hdr_blkno;
				INIT_LIST_HEAD(&f_iovec->list);
				INIT_LIST_HEAD(&f_iovec->iov_blk_list);

				list_add_tail(&_blk_list->list,
					      &f_iovec->iov_blk_list);
				list_add_tail(&f_iovec->list,
					      &item->iovec_list);
			}
#endif
			if (enable_perf_stats)
				g_perf_stats.n_digest++;
			break;
		}
		case L_TYPE_UNLINK: {
			// Got it! Kernfs can skip digest of related items.
			// clean-up inode, directory, file digest operations for
			// the inode.
			uint32_t inum = loghdr->inode_no[i];
			i_replay_t i_search, *i_item;
			f_replay_t f_search, *f_item;
			u_replay_t u_search, *u_item;
			// d_replay_key_t d_key;
			f_iovec_t *f_iovec, *tmp;

			replay_key_t key = {
				.inum = loghdr->inode_no[i],
				.ver = inode_version_table[loghdr->inode_no[i]],
			};

			// This is required for structure key in UThash.
			memset(&i_search, 0, sizeof(i_replay_t));
			memset(&f_search, 0, sizeof(f_replay_t));
			memset(&u_search, 0, sizeof(u_replay_t));

			mlfs_debug("%s\n", "-------------------------------");

			// check inode digest info can skip.
			i_search.key.inum = key.inum;
			i_search.key.ver = key.ver;
			HASH_FIND(hh, replay_list->i_digest_hash, &i_search.key,
				  sizeof(replay_key_t), i_item);

			if (i_item && i_item->create) {
				mlfs_debug("[INODE] inum %u (ver %u) --> "
					   "SKIP\n",
					   i_item->key.inum, i_item->key.ver);
				// the unlink can skip and erase related i_items
				HASH_DEL(replay_list->i_digest_hash, i_item);
				list_del(&i_item->list);
				mlfs_free(i_item);

				if (enable_perf_stats)
					g_perf_stats.n_digest_skipped++;

			} else {
				// the unlink must be applied. create a new
				// unlink item.
				u_item = (u_replay_t *)mlfs_zalloc(
					sizeof(u_replay_t));
				u_item->key = key;
				u_item->node_type = NTYPE_U;
				HASH_ADD(hh, replay_list->u_digest_hash, key,
					 sizeof(replay_key_t), u_item);
				list_add_tail(&u_item->list,
					      &replay_list->head);
				mlfs_debug("[ULINK] inum %u (ver %u)\n",
					   u_item->key.inum, u_item->key.ver);
				if (enable_perf_stats)
					g_perf_stats.n_digest++;
			}

#if 0
			HASH_FIND(hh, replay_list->u_digest_hash, &key,
				sizeof(replay_key_t), u_item);
			if (u_item) {
			    // previous unlink can skip.
			    mlfs_debug("[ULINK] inum %u (ver %u) --> SKIP\n",
				    u_item->key.inum, u_item->key.ver);
			    HASH_DEL(replay_list->u_digest_hash, u_item);
			    list_del(&u_item->list);
			    mlfs_free(u_item);
			}
#endif

			// delete file digest info.
			f_search.key.inum = key.inum;
			f_search.key.ver = key.ver;

			HASH_FIND(hh, replay_list->f_digest_hash, &f_search.key,
				  sizeof(replay_key_t), f_item);

			if (f_item) {
				list_for_each_entry_safe (f_iovec, tmp,
							  &f_item->iovec_list,
							  list) {
					list_del(&f_iovec->list);
					mlfs_free(f_iovec);

					if (enable_perf_stats)
						g_perf_stats.n_digest_skipped++;
				}

				HASH_DEL(replay_list->f_digest_hash, f_item);
				list_del(&f_item->list);
				mlfs_free(f_item);
			}

			mlfs_debug("%s\n", "-------------------------------");
			break;
		}
		default: {
			printf("%s: digest type %d\n", __func__,
			       loghdr->type[i]);
			hexdump(loghdr, sizeof(struct logheader));
			panic("unsupported type of operation\n");
			break;
		}
		}
	}
	// END_TIMER(evt_dro);
}

static void print_memcpy_meta (memcpy_meta_t *meta, uint64_t i)
{
	addr_t src_blknr;
	src_blknr = (((uintptr_t)meta->data) -
		     (uintptr_t)g_bdev[g_log_dev]->map_base_addr) >>
		    g_block_size_shift;

	printf("%lu HOST_MEMCPY i=%lu is_single_blk=%d to_dev=%u blk_nr=%lu "
	       "src_data=%p(%lu) size=%d offset=%d\n",
	       get_tid(), i, meta->is_single_blk, meta->to_dev, meta->blk_nr,
	       meta->data, src_blknr, meta->size, meta->offset_in_blk);
}

static void add_memcpy_meta_to_array(struct memcpy_meta_array *memcpy_meta_list,
				     memcpy_meta_t *memcpy_meta)
{
	// print_memcpy_meta (memcpy_meta, memcpy_meta_list->id);

	memcpy(&memcpy_meta_list->buf[memcpy_meta_list->id], memcpy_meta,
	       sizeof(memcpy_meta_t));
	memcpy_meta_list->id++;
}

static uint32_t get_blk_cnt_from_length(uint32_t length)
{
	uint32_t nr_blocks;

	if (length < g_block_size_bytes) {
		nr_blocks = 1;
	} else {
		nr_blocks = (length >> g_block_size_shift);
		if (length % g_block_size_bytes != 0)
			nr_blocks++;
	}

	return nr_blocks;
}

// FIXME: to_dev is now used. change APIs
int digest_inode(uint8_t from_dev, uint8_t to_dev, int libfs_id, uint32_t inum,
		 addr_t blknr)
{
	struct buffer_head *bh;
	struct dinode *src_dinode;
	struct inode *inode;
	int ret;

	bh = bh_get_sync_IO(from_dev, blknr, BH_NO_DATA_ALLOC);
	bh->b_size = sizeof(struct dinode);
	bh->b_data = mlfs_alloc(bh->b_size);

	bh_submit_read_sync_IO(bh);
	mlfs_io_wait(from_dev, 1);

	src_dinode = (struct dinode *)bh->b_data;

	mlfs_debug("[INODE] dev %u type %u inum %u size %lu\n", g_root_dev,
		   src_dinode->itype, inum, src_dinode->size);

	// Inode exists only in NVM layer.
	to_dev = g_root_dev;

	inode = icache_find(inum);

	if (!inode)
		inode = ialloc(src_dinode->itype, inum);

	mlfs_assert(inode);

	if (inode->flags & I_DELETING) {
		// reuse deleting inode.
		// digest_unlink cleaned up old contents already.

		inode->flags &= ~I_DELETING;
		inode->flags |= I_VALID;
		inode->itype = src_dinode->itype;
		inode->i_sb = sb;
		inode->i_generation = 0;
		inode->i_data_dirty = 0;
	}

	if (inode->itype == T_FILE || inode->itype == T_DIR) {
		struct mlfs_extent_header *ihdr;
		handle_t handle = { .libfs = libfs_id, .dev = to_dev };

		ihdr = ext_inode_hdr(&handle, inode);

		// The first creation of dinode of file
		if (ihdr->eh_magic != MLFS_EXT_MAGIC) {
			mlfs_ext_tree_init(&handle, inode);

			// For testing purpose, those data are hard-coded.
			inode->i_writeback = NULL;
			memset(inode->i_uuid, 0xCC, sizeof(inode->i_uuid));
			inode->i_csum = mlfs_crc32c(~0, inode->i_uuid,
						    sizeof(inode->i_uuid));
			inode->i_csum = mlfs_crc32c(inode->i_csum, &inode->inum,
						    sizeof(inode->inum));
			inode->i_csum =
				mlfs_crc32c(inode->i_csum, &inode->i_generation,
					    sizeof(inode->i_generation));
		}

		if (inode->itype == T_FILE) {
			// ftruncate (shrink length)
			if (src_dinode->size < inode->size) {
				handle_t handle;
				handle.dev = g_root_dev;
				handle.libfs = libfs_id;

				// fix truncate segmentation fault by bad end
				// block calculation
				mlfs_lblk_t start_blk =
					(src_dinode->size >>
					 g_block_size_shift) +
					((src_dinode->size &
					  g_block_size_mask) != 0);
				mlfs_lblk_t end_blk =
					(inode->size >> g_block_size_shift);

				ret = mlfs_ext_truncate(&handle, inode,
							start_blk, end_blk);

				mlfs_assert(!ret);
			}
		}
	}

	inode->size = src_dinode->size;

	mlfs_debug("[INODE] (%d->%d) inode inum %u type %d, size %lu\n",
		   from_dev, to_dev, inode->inum, inode->itype, inode->size);

	mlfs_mark_inode_dirty(libfs_id, inode);

	clear_buffer_uptodate(bh);

	mlfs_free(bh->b_data);
	bh_release(bh);

#if 0
	// mark inode digested
	if(inode->lstate == LEASE_DIGEST_TO_ACQUIRE || inode->lstate == LEASE_DIGEST_TO_READ) {
	  inode->lstate = LEASE_FREE;
	}
#endif
	return 0;
}

#ifdef DIGEST_OPT_MERGE_WRITES
/**
 * @Synopsis Digest log entries.
 *
 * @Param rdma_memcpy_meta for sending rdma entries
 * @Param memcpy_meta_list for mlfs_conf.digest_opt_parallel_rdma_memcpy
 */
int digest_file(uint8_t from_dev, uint8_t to_dev, int libfs_id,
		uint32_t file_inum, offset_t offset, uint32_t length,
		addr_t blknr, struct rdma_memcpy_meta *rdma_memcpy_meta,
		struct memcpy_meta_array *memcpy_meta_list)
#else
int digest_file(uint8_t from_dev, uint8_t to_dev, int libfs_id,
		uint32_t file_inum, offset_t offset, uint32_t length,
		addr_t blknr)
#endif
{
	int ret;
	uint32_t offset_in_block = 0;
	struct inode *file_inode;
	struct buffer_head *bh_data, *bh;
	uint8_t *data;
	struct mlfs_ext_path *path = NULL;
	struct mlfs_map_blocks map;
	uint32_t nr_blocks = 0, nr_digested_blocks = 0;
	offset_t cur_offset;
	handle_t handle = { .libfs = libfs_id, .dev = to_dev };
#ifdef DIGEST_OPT_MERGE_WRITES
	struct rdma_meta_entry *meta_e;
	uintptr_t read_addr, local_addr, write_addr;
	int buf_pos, buf_blk_cnt;
	memcpy_meta_t memcpy_meta = {0};
#endif
	mlfs_debug("[FILE] (%d->%d) inum %d offset %lu(0x%lx) length %u\n",
		   from_dev, to_dev, file_inum, offset, offset, length);

	nr_blocks = get_blk_cnt_from_length(length);
	mlfs_assert(nr_blocks > 0);

	if ((from_dev == g_ssd_dev) || (from_dev == g_hdd_dev))
		panic("does not support this case yet\n");

	// Storage to storage copy.
	// FIXME: this does not work if migrating block from SSD to NVM.
	data = g_bdev[from_dev]->map_base_addr + (blknr << g_block_size_shift);
	mlfs_info("[NIC_RPC] map_base_addr: %p, blknr=%lu addr=0x%lx data: "
		  "%p\n",
		  g_bdev[from_dev]->map_base_addr, blknr,
		  blknr << g_block_size_shift, data);
	// mlfs_printf("[DIGEST_FILE] src: map_base_addr=%p blknr=%lu(0x%lx) data="
	//             "%p\n",
	//             g_bdev[from_dev]->map_base_addr, blknr,
	//             blknr << g_block_size_shift, data);
	// START_TIMER(evt_icache);
	file_inode = icache_find(file_inum);
	if (!file_inode) {
		struct dinode dip;
		file_inode = icache_alloc_add(file_inum);

		read_ondisk_inode(file_inum, &dip);
		mlfs_assert(dip.itype != 0);

		sync_inode_from_dinode(file_inode, &dip);
	}

	// update file inode length and mtime.
	if (file_inode->size < offset + length) {
		/* Inode size should be synchronized among other layers.
		 * So, update both inodes */
		file_inode->size = offset + length;
		mlfs_mark_inode_dirty(libfs_id, file_inode);
	}
	// END_TIMER(evt_icache);

	nr_digested_blocks = 0;
	cur_offset = offset;
	offset_in_block = offset % g_block_size_bytes;

	// case 1. a single block writing: small size (< 4KB)
	// or a heading block of unaligned starting offset.
	if ((length < g_block_size_bytes) || offset_in_block != 0) {
		int _len = _min(length,
				(uint32_t)g_block_size_bytes - offset_in_block);

		map.m_lblk = (cur_offset >> g_block_size_shift);
		map.m_pblk = 0;
		map.m_len = 1;
		map.m_flags = 0;

		// START_TIMER(evt_geb);
		pthread_spin_lock(&g_ext_get_block_lock);
		ret = mlfs_ext_get_blocks(&handle, file_inode, &map,
					  MLFS_GET_BLOCKS_CREATE);
		pthread_spin_unlock(&g_ext_get_block_lock);
		// END_TIMER(evt_geb);

#ifdef DIGEST_OPT_MERGE_WRITES
		if (mlfs_conf.digest_opt_host_memcpy) {
			memcpy_meta.is_single_blk = 1;
			memcpy_meta.to_dev = to_dev;
			memcpy_meta.blk_nr = map.m_pblk;
			memcpy_meta.data = data + offset_in_block;
			memcpy_meta.size = _len;
			memcpy_meta.offset_in_blk = offset_in_block;

			add_memcpy_meta_to_array(memcpy_meta_list,
						 &memcpy_meta);

		} else {
			buf_blk_cnt = 1; // case 1: small size (< 4KB)
			read_addr = (uintptr_t)(data + offset_in_block);
			local_addr =
				(uintptr_t)nic_slab_alloc_in_blk(buf_blk_cnt);
			write_addr = (uintptr_t)(
				(addr_t)g_bdev[to_dev]->map_base_addr +
				(map.m_pblk << g_block_size_shift) +
				offset_in_block);

			// START_TIMER(evt_cre);
			meta_e = create_rdma_entry(local_addr, read_addr, _len,
						   MR_DRAM_BUFFER, MR_NVM_LOG);
			list_add_tail(&meta_e->head,
				      rdma_memcpy_meta->rdma_read_list);

			meta_e = create_rdma_entry(local_addr, write_addr, _len,
						   MR_DRAM_BUFFER,
						   MR_NVM_SHARED);
			list_add_tail(&meta_e->head,
				      rdma_memcpy_meta->rdma_write_list);
			// END_TIMER(evt_cre);

			rdma_memcpy_meta->rdma_entry_cnt++;
			if (is_rdma_list_filled(rdma_memcpy_meta)) {
				if (mlfs_conf.digest_opt_parallel_rdma_memcpy) {
					start_send_rdma_entries_workers(
						rdma_memcpy_meta);
					alloc_and_init_rdma_memcpy_list_pair(
						rdma_memcpy_meta);
				} else {
					send_digest_rdma_entries(
						rdma_memcpy_meta->rdma_read_list,
						rdma_memcpy_meta
							->rdma_write_list);
				}
				rdma_memcpy_meta->rdma_entry_cnt = 0;
			}
		}

#ifdef MIGRATION // Check MIGRATION path. FIXME
		lru_key_t k = {
			.dev = to_dev,
			.block = map.m_pblk,
		};
		lru_val_t v = {
			.inum = file_inum,
			.lblock = map.m_lblk,
		};
		update_slru_list_from_digest(to_dev, k, v);
#endif
#else /* DIGEST_OPT_MERGE_WRITES */

		mlfs_assert(ret == 1);
		bh_data = bh_get_sync_IO(to_dev, map.m_pblk, BH_NO_DATA_ALLOC);

		mlfs_assert(bh_data);

		bh_data->b_data = data + offset_in_block;
		bh_data->b_size = _len;
		bh_data->b_offset = offset_in_block;

#ifdef MIGRATION
		lru_key_t k = {
			.dev = to_dev,
			.block = map.m_pblk,
		};
		lru_val_t v = {
			.inum = file_inum,
			.lblock = map.m_lblk,
		};
		update_slru_list_from_digest(to_dev, k, v);
#endif /* MIGRATION */
		// mlfs_debug("File data : %s\n", bh_data->b_data);

		// START_TIMER(evt_dc);
		ret = mlfs_write_opt(bh_data);
		// END_TIMER(evt_dc);

		mlfs_assert(!ret);
		bh_release(bh_data);
#endif /* DIGEST_OPT_MERGE_WRITES */

		mlfs_debug("inum %d, offset %lu len %u (dev %d:%lu) -> (dev "
			   "%d:%lu)\n",
			   file_inode->inum, cur_offset, _len, from_dev, blknr,
			   to_dev, map.m_pblk);

		nr_digested_blocks++;
		cur_offset += _len;
		data += _len;
	}

	// case 2. multiple trial of block writing.
	// when extent tree has holes in a certain offset (due to data
	// migration), an extent is split at the hole. Kernfs should call
	// mlfs_ext_get_blocks() with setting m_lblk to the offset having a the
	// hole to fill it.
	while (nr_digested_blocks < nr_blocks) {
		int nr_block_get = 0, i;

		mlfs_assert((cur_offset % g_block_size_bytes) == 0);

		map.m_lblk = (cur_offset >> g_block_size_shift);
		map.m_pblk = 0;
		map.m_len = nr_blocks - nr_digested_blocks;
		map.m_flags = 0;

		// START_TIMER(evt_geb);
		// find block address of offset and update extent tree
		if (to_dev == g_ssd_dev || to_dev == g_hdd_dev) {
			// make kernelFS do log-structured update.
			// map.m_flags |= MLFS_MAP_LOG_ALLOC;
			nr_block_get = mlfs_ext_get_blocks(
				&handle, file_inode, &map,
				MLFS_GET_BLOCKS_CREATE_DATA);
		} else {
			nr_block_get = mlfs_ext_get_blocks(
				&handle, file_inode, &map,
				MLFS_GET_BLOCKS_CREATE_DATA);
		}

		// END_TIMER(evt_geb);

		mlfs_assert(map.m_pblk != 0);

		mlfs_assert(nr_block_get <= (nr_blocks - nr_digested_blocks));
		mlfs_assert(nr_block_get > 0);

		nr_digested_blocks += nr_block_get;

#ifdef DIGEST_OPT_MERGE_WRITES
		if (mlfs_conf.digest_opt_host_memcpy) {
			memcpy_meta.is_single_blk = 0;
			memcpy_meta.to_dev = to_dev;
			memcpy_meta.blk_nr = map.m_pblk;
			memcpy_meta.data = data;
			memcpy_meta.size = nr_block_get * g_block_size_bytes;
			memcpy_meta.offset_in_blk = 0;

			add_memcpy_meta_to_array(memcpy_meta_list,
						 &memcpy_meta);
		} else {
			buf_blk_cnt = nr_block_get; // case 2: multiple blocks.
			read_addr = (uintptr_t)data; // offset = 0
			local_addr =
				(uintptr_t)nic_slab_alloc_in_blk(buf_blk_cnt);
			write_addr = (uintptr_t)(
				(addr_t)g_bdev[to_dev]->map_base_addr +
				(map.m_pblk << g_block_size_shift)); // offset =
								     // 0

			// START_TIMER(evt_cre);
			meta_e = create_rdma_entry(local_addr, read_addr,
						   nr_block_get *
							   g_block_size_bytes,
						   MR_DRAM_BUFFER, MR_NVM_LOG);
			list_add_tail(&meta_e->head,
				      rdma_memcpy_meta->rdma_read_list);

			meta_e = create_rdma_entry(
				local_addr, write_addr,
				nr_block_get * g_block_size_bytes,
				MR_DRAM_BUFFER, MR_NVM_SHARED);
			list_add_tail(&meta_e->head,
				      rdma_memcpy_meta->rdma_write_list);
			// END_TIMER(evt_cre);

			rdma_memcpy_meta->rdma_entry_cnt++;
			if (is_rdma_list_filled(rdma_memcpy_meta)) {
				if (mlfs_conf.digest_opt_parallel_rdma_memcpy) {
					start_send_rdma_entries_workers(
						rdma_memcpy_meta);
					alloc_and_init_rdma_memcpy_list_pair(
						rdma_memcpy_meta);
				} else {
					send_digest_rdma_entries(
						rdma_memcpy_meta->rdma_read_list,
						rdma_memcpy_meta
							->rdma_write_list);
				}
				rdma_memcpy_meta->rdma_entry_cnt = 0;
			}
		}

#ifdef MIGRATION // Check MIGRATION path. FIXME
		// TODO: important note: this scheme only works if
		// LRU_ENTRY_SIZE is equal to g_block_size_bytes. To use larger
		// LRU_ENTRY_SIZES and keep the g_lru_hash map small, we should
		// align the keys by LRU_ENTRY_SIZE
		for (i = 0; i < nr_block_get; i++) {
			lru_key_t k = {
				.dev = to_dev,
				.block = map.m_pblk + i,
			};
			lru_val_t v = {
				.inum = file_inum,
				.lblock = map.m_lblk + i,
			};
			update_slru_list_from_digest(to_dev, k, v);
		}
#endif
#else /* DIGEST_OPT_MERGE_WRITES */

		// update data block
		bh_data = bh_get_sync_IO(to_dev, map.m_pblk, BH_NO_DATA_ALLOC);

		bh_data->b_data = data;
		bh_data->b_size = nr_block_get * g_block_size_bytes;
		bh_data->b_offset = 0;

#ifdef MIGRATION
		// TODO: important note: this scheme only works if
		// LRU_ENTRY_SIZE is equal to g_block_size_bytes. To use larger
		// LRU_ENTRY_SIZES and keep the g_lru_hash map small, we should
		// align the keys by LRU_ENTRY_SIZE
		for (i = 0; i < nr_block_get; i++) {
			lru_key_t k = {
				.dev = to_dev,
				.block = map.m_pblk + i,
			};
			lru_val_t v = {
				.inum = file_inum,
				.lblock = map.m_lblk + i,
			};
			update_slru_list_from_digest(to_dev, k, v);
		}
#endif

		// mlfs_debug("File data : %s\n", bh_data->b_data);

		// START_TIMER(evt_dc);
		ret = mlfs_write_opt(bh_data);
		// END_TIMER(evt_dc);
		mlfs_assert(!ret);
		clear_buffer_uptodate(bh_data);
		bh_release(bh_data);

		if (0) {
			struct buffer_head *bh;
			uint8_t tmp_buf[4096];
			bh = bh_get_sync_IO(to_dev, map.m_pblk,
					    BH_NO_DATA_ALLOC);

			bh->b_data = tmp_buf;
			bh->b_size = g_block_size_bytes;
			bh_submit_read_sync_IO(bh);
			mlfs_io_wait(bh->b_dev, 1);

			GDB_TRAP;

			bh_release(bh);
		}

#endif /* DIGEST_OPT_MERGE_WRITES */

		mlfs_debug("inum %d, offset %lu len %u (dev %d:%lu) -> (dev "
			   "%d:%lu)\n",
			   file_inode->inum, cur_offset, bh_data->b_size,
			   from_dev, blknr, to_dev, map.m_pblk);

		cur_offset += nr_block_get * g_block_size_bytes;
		data += nr_block_get * g_block_size_bytes;
	}

	mlfs_assert(nr_blocks == nr_digested_blocks);

	if (file_inode->size < offset + length)
		file_inode->size = offset + length;

	return 0;
}

int digest_unlink(uint8_t from_dev, uint8_t to_dev, int libfs_id, uint32_t inum)
{
	struct buffer_head *bh;
	struct inode *inode;
	struct dinode dinode;
	int ret = 0;

	mlfs_assert(to_dev == g_root_dev);

	mlfs_debug("[UNLINK] (%d->%d) inum %d\n", from_dev, to_dev, inum);

	inode = icache_find(inum);
	if (!inode) {
		struct dinode dip;
		inode = icache_alloc_add(inum);

		read_ondisk_inode(inum, &dip);
		sync_inode_from_dinode(inode, &dip);
	}

	mlfs_assert(inode);

	if (inode->size > 0) {
		handle_t handle = { .libfs = libfs_id, .dev = to_dev };
		mlfs_lblk_t end = (inode->size) >> g_block_size_shift;

		ret = mlfs_ext_truncate(&handle, inode, 0,
					end == 0 ? end : end - 1);
		mlfs_assert(!ret);
	}

	memset(inode->_dinode, 0, sizeof(struct dinode));

	inode->flags = 0;
	inode->flags |= I_DELETING;
	inode->size = 0;
	inode->itype = 0;

	mlfs_mark_inode_dirty(libfs_id, inode);

	return 0;
}

// FIXME: this function is not synchronized with up-to-date
// changes. Refer digest_file to update this function.
int digest_file_iovec(uint8_t from_dev, uint8_t to_dev, int libfs_id,
		      uint32_t file_inum, f_iovec_t *iovec)
{
	int ret;
	uint32_t offset_in_block = 0;
	struct inode *file_inode;
	struct buffer_head *bh_data, *bh;
	uint8_t *data;
	struct mlfs_ext_path *path = NULL;
	struct mlfs_map_blocks map;
	uint32_t nr_blocks = 0, nr_digested_blocks = 0;
	offset_t cur_offset;
	uint32_t length = iovec->length;
	offset_t offset = iovec->offset;
	f_blklist_t *_blk_list;
	handle_t handle = { .libfs = libfs_id, .dev = to_dev };

	mlfs_debug("[FILE] (%d->%d) inum %d offset %lu(0x%lx) length %u\n",
		   from_dev, to_dev, file_inum, offset, offset, length);

	if (length < g_block_size_bytes)
		nr_blocks = 1;
	else {
		nr_blocks = (length >> g_block_size_shift);

		if (length % g_block_size_bytes != 0)
			nr_blocks++;
	}

	mlfs_assert(nr_blocks > 0);

	if (from_dev == g_ssd_dev)
		panic("does not support this case\n");

	file_inode = icache_find(file_inum);
	if (!file_inode) {
		struct dinode dip;
		file_inode = icache_alloc_add(file_inum);

		read_ondisk_inode(file_inum, &dip);
		mlfs_assert(dip.itype != 0);

		sync_inode_from_dinode(file_inode, &dip);
	}

#ifdef USE_SSD
	// update file inode length and mtime.
	if (file_inode->size < offset + length) {
		/* Inode size should be synchronized among NVM and SSD layer.
		 * So, update both inodes */
		uint8_t sync_dev = 3 - to_dev;
		handle_t handle;
		struct inode *sync_file_inode = icache_find(file_inum);
		if (!sync_file_inode) {
			struct dinode dip;
			sync_file_inode = icache_alloc_add(file_inum);

			read_ondisk_inode(file_inum, &dip);

			mlfs_assert(dip.itype != 0);
			sync_inode_from_dinode(sync_file_inode, &dip);

			file_inode->i_sb = sb;
		}

		file_inode->size = offset + length;
		sync_file_inode->size = file_inode->size;

		handle.libfs = libfs_id;

		handle.dev = to_dev;
		mlfs_mark_inode_dirty(libfs_id, file_inode);

		handle.dev = sync_dev;
		mlfs_mark_inode_dirty(libfs_id, sync_file_inode);
	}
#endif

	nr_digested_blocks = 0;
	cur_offset = offset;
	offset_in_block = offset % g_block_size_bytes;

	_blk_list = list_first_entry(&iovec->iov_blk_list, f_blklist_t, list);

	// case 1. a single block writing: small size (< 4KB)
	// or a heading block of unaligned starting offset.
	if ((length < g_block_size_bytes) || offset_in_block != 0) {
		int _len = _min(length,
				(uint32_t)g_block_size_bytes - offset_in_block);

		// mlfs_assert(_blk_list->n == 1);
		data = g_bdev[from_dev]->map_base_addr +
		       (_blk_list->blknr << g_block_size_shift);

		map.m_lblk = (cur_offset >> g_block_size_shift);
		map.m_pblk = 0;
		map.m_len = 1;

		ret = mlfs_ext_get_blocks(&handle, file_inode, &map,
					  MLFS_GET_BLOCKS_CREATE);

		mlfs_assert(ret == 1);

		bh_data = bh_get_sync_IO(to_dev, map.m_pblk, BH_NO_DATA_ALLOC);

		bh_data->b_data = data + offset_in_block;
		bh_data->b_size = _len;
		bh_data->b_offset = offset_in_block;

		// mlfs_debug("File data : %s\n", bh_data->b_data);

		ret = mlfs_write_opt(bh_data);
		mlfs_assert(!ret);
		clear_buffer_uptodate(bh_data);
		bh_release(bh_data);

		mlfs_debug("inum %d, offset %lu (dev %d:%lx) -> (dev %d:%lx)\n",
			   file_inode->inum, cur_offset, from_dev, iovec->blknr,
			   to_dev, map.m_pblk);

		nr_digested_blocks++;
		cur_offset += _len;
		data += _len;
	}

	// case 2. multiple trial of block writing.
	// when extent tree has holes in a certain offset (due to data
	// migration), an extent is split at the hole. Kernfs should call
	// mlfs_ext_get_blocks() with setting m_lblk to the offset having a the
	// hole to fill it.
	while (nr_digested_blocks < nr_blocks) {
		int nr_block_get = 0, i, j;
		int rand_val;

		mlfs_assert((cur_offset % g_block_size_bytes) == 0);

		map.m_lblk = (cur_offset >> g_block_size_shift);
		map.m_pblk = 0;
		map.m_len = min(nr_blocks - nr_digested_blocks, (1 << 15));

		// find block address of offset and update extent tree
		nr_block_get = mlfs_ext_get_blocks(&handle, file_inode, &map,
						   MLFS_GET_BLOCKS_CREATE);

		mlfs_assert(map.m_pblk != 0);

		mlfs_assert(nr_block_get > 0);

		nr_digested_blocks += nr_block_get;

		if (_blk_list->n > nr_block_get) {
			data = g_bdev[from_dev]->map_base_addr +
			       (_blk_list->blknr << g_block_size_shift);

			mlfs_debug("inum %d, offset %lu len %u (dev %d:%lu) -> "
				   "(dev %d:%lu)\n",
				   file_inode->inum, cur_offset,
				   _blk_list->n << g_block_size_shift, from_dev,
				   _blk_list->blknr, to_dev, map.m_pblk);

			bh_data = bh_get_sync_IO(to_dev, map.m_pblk,
						 BH_NO_DATA_ALLOC);

			bh_data->b_data = data;
			bh_data->b_size = (_blk_list->n << g_block_size_shift);
			bh_data->b_offset = 0;

			// mlfs_debug("File data : %s\n", bh_data->b_data);

			ret = mlfs_write_opt(bh_data);
			mlfs_assert(!ret);
			clear_buffer_uptodate(bh_data);
			bh_release(bh_data);

			cur_offset += (nr_block_get << g_block_size_shift);

			_blk_list->n -= nr_block_get;
			_blk_list->blknr += nr_block_get;

			continue;
		}

		for (i = 0, j = _blk_list->n; j <= nr_block_get;
		     i += _blk_list->n, j += _blk_list->n) {
			data = g_bdev[from_dev]->map_base_addr +
			       (_blk_list->blknr << g_block_size_shift);

			mlfs_debug("inum %d, offset %lu len %u (dev %d:%lu) -> "
				   "(dev %d:%lu)\n",
				   file_inode->inum, cur_offset,
				   _blk_list->n << g_block_size_shift, from_dev,
				   _blk_list->blknr, to_dev, map.m_pblk + i);

			// update data block
			bh_data = bh_get_sync_IO(to_dev, map.m_pblk + i,
						 BH_NO_DATA_ALLOC);

			bh_data->b_data = data;
			bh_data->b_blocknr = map.m_pblk + i;
			bh_data->b_size = (_blk_list->n << g_block_size_shift);
			bh_data->b_offset = 0;

			// mlfs_debug("File data : %s\n", bh_data->b_data);

			ret = mlfs_write_opt(bh_data);
			mlfs_assert(!ret);
			clear_buffer_uptodate(bh_data);
			bh_release(bh_data);

			cur_offset += (_blk_list->n << g_block_size_shift);

			_blk_list = list_next_entry(_blk_list, list);

			if (&_blk_list->list == &iovec->iov_blk_list)
				break;
		}
	}

	mlfs_assert(nr_blocks == nr_digested_blocks);

	if (file_inode->size < offset + length)
		file_inode->size = offset + length;

	return 0;
}

void file_digest_worker(void *arg)
{
	struct f_digest_worker_arg *_arg = (struct f_digest_worker_arg *)arg;
	f_iovec_t *f_iovec, *iovec_tmp;
	f_replay_t *f_item;
	lru_key_t k;

	// START_TIMER(evt_dig_fcon_in);
	PRINT_START_TIME(evt_dig_fcon_in, "fcon_in_start");

#ifdef DIGEST_OPT_MERGE_WRITES
	struct rdma_meta_entry *meta_e, *_meta_e;
	struct rdma_memcpy_meta rdma_memcpy_meta;

	alloc_and_init_rdma_memcpy_list_pair(&rdma_memcpy_meta);
	rdma_memcpy_meta.rdma_entry_cnt = 0;
#endif

	f_item = _arg->f_item;

	list_for_each_entry_safe (f_iovec, iovec_tmp, &f_item->iovec_list,
				  list) {
#ifndef EXPERIMENTAL
#ifdef DIGEST_OPT_MERGE_WRITES
		if (mlfs_conf.digest_opt_host_memcpy)
			mlfs_assert(0); // batch_memcpy is not supported with
					// fconcurrent.

		digest_file(_arg->from_dev, _arg->to_dev, _arg->libfs_id,
			    f_item->key.inum, f_iovec->offset, f_iovec->length,
			    f_iovec->blknr, &rdma_memcpy_meta, 0);
#else /* DIGEST_OPT_MERGE_WRITES */
		digest_file(_arg->from_dev, _arg->to_dev, _arg->libfs_id,
			    f_item->key.inum, f_iovec->offset, f_iovec->length,
			    f_iovec->blknr);
#endif
		mlfs_free(f_iovec);
#else
		digest_file_iovec(_arg->from_dev, _arg->to_dev, _arg->libfs_id,
				  f_item->key.inum, f_iovec);
#endif // EXPERIMENTAL
	}

	mlfs_free(_arg);

	// END_TIMER(evt_dig_fcon_in);
	PRINT_END_TIME(evt_dig_fcon_in, "fcon_in_end");
}

int digest_allocate(uint8_t from_dev, uint8_t to_dev, int libfs_id,
		    uint32_t inum, uint64_t length)
{
	struct inode *ip;
	struct mlfs_map_blocks map;
	handle_t handle;
	uint64_t first_pblk;
	uint64_t blk_alloced;
	uint64_t blk_length;
	int ret;

	mlfs_printf("[ALLOCATE] from %u to %u, inum %u, size %lu\n", from_dev,
		    to_dev, inum, length);

	ip = icache_find(inum);
	if (!ip) {
		struct dinode dip;
		ip = icache_alloc_add(inum);
		read_ondisk_inode(inum, &dip);
		mlfs_assert(dip.itype != 0);
		sync_inode_from_dinode(ip, &dip);
	}

	if (ip->size >= length)
		panic("error: fallocate to smaller size!");

	blk_length = length >> g_block_size_shift;
	if (length % g_block_size_bytes)
		blk_length++;

	mlfs_printf("allocating %lu blocks!\n", blk_length);

	handle.dev = to_dev;
	handle.libfs = libfs_id;
	blk_alloced = 0;
	while (blk_alloced < blk_length) {
		map.m_lblk = blk_alloced;
		map.m_pblk = 0;
		map.m_flags = 0;

		if ((blk_length - blk_alloced) > ALLOC_SIZE)
			map.m_len = ALLOC_SIZE;
		else
			map.m_len = blk_length - blk_alloced;

		mlfs_printf("try allocate %u blocks, lblk start %u\n",
			    map.m_len, map.m_lblk);
		ret = mlfs_ext_get_blocks(&handle, ip, &map,
					  MLFS_GET_BLOCKS_CREATE);
		mlfs_printf("allocated %d blocks from %lu\n", ret, map.m_pblk);

		if (ret <= 0) {
			mlfs_printf("mlfs_ext_get_blocks failed, ret %d!", ret);
			panic("fallocate");
		}

		if (map.m_lblk == 0)
			first_pblk = map.m_pblk;
		else if (map.m_pblk != first_pblk + blk_alloced) {
			mlfs_printf("non-contiguous allocation! segment start "
				    "%lu, first pblk %lu, contiguous length "
				    "%lu",
				    map.m_pblk, first_pblk, blk_alloced);
			panic("fallocate");
		}

		blk_alloced += ret;
	}

	mlfs_mark_inode_dirty(libfs_id, ip);
	return 0;
}

// digest must be applied in order since unlink and creation cannot commute.
void digest_log_from_replay_list(uint8_t from_dev, int libfs_id,
				 struct replay_list *replay_list,
				 struct memcpy_meta_array *memcpy_meta_list)
{
	// START_TIMER(evt_dlfrl);
	struct list_head *l, *tmp;
	uint8_t *node_type;
	f_iovec_t *f_iovec, *iovec_tmp;
	uint64_t tsc_begin;
#ifdef DIGEST_OPT_MERGE_WRITES
	struct rdma_meta_entry *meta_e, *_meta_e;
	struct rdma_memcpy_meta rdma_memcpy_meta;

	alloc_and_init_rdma_memcpy_list_pair(&rdma_memcpy_meta);
	rdma_memcpy_meta.rdma_entry_cnt = 0;
#endif

	list_for_each_safe (l, tmp, &replay_list->head) {
		node_type = (uint8_t *)l + sizeof(struct list_head);
		mlfs_assert(*node_type < 5);

		switch (*node_type) {
		case NTYPE_I: {
			// START_TIMER(evt_ntype_i);
			i_replay_t *i_item;
			i_item =
				(i_replay_t *)container_of(l, i_replay_t, list);

			if (enable_perf_stats)
				tsc_begin = asm_rdtscp();

			digest_inode(from_dev, g_root_dev, libfs_id,
				     i_item->key.inum, i_item->blknr);

			HASH_DEL(replay_list->i_digest_hash, i_item);
			list_del(l);
			mlfs_free(i_item);

			if (enable_perf_stats)
				g_perf_stats.digest_inode_tsc +=
					asm_rdtscp() - tsc_begin;
			// END_TIMER(evt_ntype_i);
			break;
		}
		case NTYPE_D: {
			panic("deprecated operation: NTYPE_D\n");
			break;
		}
		case NTYPE_F: {
			uint8_t dest_dev = g_root_dev;
			f_replay_t *f_item, *t;
			f_item =
				(f_replay_t *)container_of(l, f_replay_t, list);
			lru_key_t k;

			// START_TIMER(evt_ntype_f);
			if (enable_perf_stats)
				tsc_begin = asm_rdtscp();

			if (mlfs_conf.digest_opt_fconcurrent) {
#ifdef NIC_OFFLOAD
				/* FIXME
				 * Need to change memcpy_meta_list.
				 * Make memcpy_meta_list to be per thread data
				 * structure. Otherwise, locking is required
				 * because multiple threads are trying to change
				 * memcpy_meta_list.
				 */
				mlfs_assert(0);
#endif
				// START_TIMER(evt_dig_fcon);
				PRINT_END_TIME(evt_dig_fcon, "fcon_global_"
							     "start");
				HASH_ITER(hh, replay_list->f_digest_hash,
					  f_item, t)
				{
					struct f_digest_worker_arg *arg;

					// Digest worker thread will free the
					// arg.
					arg = (struct f_digest_worker_arg *)
						mlfs_alloc(sizeof(
							struct f_digest_worker_arg));

					arg->from_dev = from_dev;
					arg->to_dev = g_root_dev;
					arg->f_item = f_item;
					arg->libfs_id = libfs_id;

					thpool_add_work(file_digest_thread_pool,
							file_digest_worker,
							(void *)arg);
				}

				// if
				// (thpool_num_threads_working(file_digest_thread_pool))
				thpool_wait(file_digest_thread_pool);
				// END_TIMER(evt_dig_fcon);
				PRINT_END_TIME(evt_dig_fcon, "fcon_global_end");

				HASH_ITER(hh, replay_list->f_digest_hash,
					  f_item, t)
				{
					HASH_DEL(replay_list->f_digest_hash,
						 f_item);
					mlfs_free(f_item);
				}

			} else {
				list_for_each_entry_safe (f_iovec, iovec_tmp,
							  &f_item->iovec_list,
							  list) {
#ifndef EXPERIMENTAL
#ifdef DIGEST_OPT_MERGE_WRITES
					digest_file(from_dev, dest_dev,
						    libfs_id, f_item->key.inum,
						    f_iovec->offset,
						    f_iovec->length,
						    f_iovec->blknr,
						    &rdma_memcpy_meta,
						    memcpy_meta_list);
#else /* DIGEST_OPT_MERGE_WRITES */
					digest_file(from_dev, dest_dev,
						    libfs_id, f_item->key.inum,
						    f_iovec->offset,
						    f_iovec->length,
						    f_iovec->blknr);
#endif /* DIGEST_OPT_MERGE_WRITES */
					mlfs_free(f_iovec);
#else // EXPERIMENTAL
					digest_file_iovec(from_dev, dest_dev,
							  libfs_id,
							  f_item->key.inum,
							  f_iovec);
#endif // EXPERIMETNAL
					if (dest_dev == g_ssd_dev)
						mlfs_io_wait(g_ssd_dev, 0);
				}

				HASH_DEL(replay_list->f_digest_hash, f_item);
				mlfs_free(f_item);
			}

			list_del(l);

			if (enable_perf_stats)
				g_perf_stats.digest_file_tsc +=
					asm_rdtscp() - tsc_begin;
			// END_TIMER(evt_ntype_f);
			break;
		}
		case NTYPE_U: {
			u_replay_t *u_item;
			u_item =
				(u_replay_t *)container_of(l, u_replay_t, list);

			// START_TIMER(evt_ntype_u);
			if (enable_perf_stats)
				tsc_begin = asm_rdtscp();

			digest_unlink(from_dev, g_root_dev, libfs_id,
				      u_item->key.inum);

			HASH_DEL(replay_list->u_digest_hash, u_item);
			list_del(l);
			mlfs_free(u_item);

			if (enable_perf_stats)
				g_perf_stats.digest_inode_tsc +=
					asm_rdtscp() - tsc_begin;
			// END_TIMER(evt_ntype_u);
			break;
		}
		default:
			panic("unsupported node type!\n");
		}
	}

#ifdef DIGEST_OPT_MERGE_WRITES
	if (mlfs_conf.digest_opt_host_memcpy) {
		// TODO optimize by pipelining it.
		// send_memcpy_req_to_host(libfs_id, memcpy_meta_list);

	} else {
		// Send remains.
		if (mlfs_conf.digest_opt_parallel_rdma_memcpy) {
			start_send_rdma_entries_workers(&rdma_memcpy_meta);
			// START_TIMER(evt_wait_rdma_memcpy);
			thpool_wait(thread_pool_digest_rdma_memcpy);
			// END_TIMER(evt_wait_rdma_memcpy);
		} else {
			send_digest_rdma_entries(
				rdma_memcpy_meta.rdma_read_list,
				rdma_memcpy_meta.rdma_write_list);
		}
	}

#endif /* DIGEST_OPT_MERGE_WRITES */

	HASH_CLEAR(hh, replay_list->i_digest_hash);
	HASH_CLEAR(hh, replay_list->f_digest_hash);
	HASH_CLEAR(hh, replay_list->u_digest_hash);

#if 0
	i_replay_t *i_item, *i_tmp;
	f_replay_t *f_item, *f_tmp;
	f_iovec_t *f_iovec, *iovec_tmp;

	HASH_ITER(hh, replay_list->i_digest_hash, i_item, i_tmp) {
		digest_inode(from_dev, g_root_dev, i_item->inum, i_item->blknr);
		mlfs_free(i_item);
	}

	HASH_ITER(hh, replay_list->f_digest_hash, f_item, f_tmp) {
		list_for_each_entry_safe(f_iovec, iovec_tmp, &f_item->iovec_list, list) {
			digest_file(from_dev, g_root_dev, f_item->inum, f_iovec->offset,
					f_iovec->length, f_iovec->blknr);
			mlfs_free(f_iovec);
		}
		mlfs_free(f_item);
	}
#endif

	// END_TIMER(evt_dlfrl);
}
