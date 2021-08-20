#ifdef __x86_64__
#include "fs.h"
#elif __aarch64__
#include "nic/nic_fs.h"
#else
#error "Unsupported architecture."
#endif

#include "ds/list.h"
#include "global/util.h"
#include "migrate.h"
#include "extents.h"
#include "filesystem/slru.h"

lru_node_t *g_lru_hash[g_n_devices + 1];
struct lru g_lru[g_n_devices + 1];
struct lru g_stage_lru[g_n_devices + 1];
struct lru g_swap_lru[g_n_devices + 1];
pthread_spinlock_t lru_spinlock;

// 0: not used
// 1: g_root_dev
// 2: g_ssd_dev
// 3: g_hdd_dev (not used)
// 4~ unused
int wb_threshold[g_n_devices + 1] = {0, 60, 80, 100, 0};
int migrate_threshold[g_n_devices + 1] = {0, 90, 95, 100, 0};

static inline uint8_t get_lower_dev(uint8_t dev)
{
	uint8_t lower_dev;

	if (dev == g_root_dev) {
#ifdef USE_SSD
		lower_dev = g_ssd_dev;
#else
		lower_dev = 0;
#endif
	} else if (dev == g_ssd_dev) {
#ifdef USE_HDD
		lower_dev = g_hdd_dev;
#else
		lower_dev = 0;
#endif
	} else if (dev == g_hdd_dev) {
		lower_dev = 0;
	}

	return lower_dev;
}

#if 0
static inline uint8_t get_upper_dev(uint8_t dev)
{
	uint8_t upper_dev;

	if (dev == g_root_dev)
		upper_dev = 0;
	else if (dev == g_ssd_dev)
		upper_dev = g_root_dev;
	else if (dev == g_hdd_dev) {
#ifdef USE_SSD
		upper_dev = g_ssd_dev;
#else
		upper_dev = g_root_dev;
#endif
	}

	return upper_dev;
}
#endif

static void evict_slru_lists(void)
{
  uint8_t dev;

  for(dev = g_root_dev; get_lower_dev(dev) != 0; dev = get_lower_dev(dev)) {
    // Evict last one if over limit, move to next lower LRU
    while(g_lru[dev].n >= (disk_sb[dev].ndatablocks * migrate_threshold[dev]) / 100) {
      lru_node_t *n = list_last_entry(&g_lru[dev].lru_head, lru_node_t, list);
      uint8_t ndev = get_lower_dev(dev);

      /* mlfs_info("Evicting %u -> %u.\n", dev, ndev); */

      list_move(&n->list, &g_lru[ndev].lru_head);
      HASH_DEL(g_lru_hash[dev], n);
      HASH_ADD(hh, g_lru_hash[ndev], key, sizeof(lru_key_t), n);

      g_lru[dev].n--;
      g_lru[ndev].n++;
    }
  }
}

static void update_staging_slru_list(uint8_t dev, lru_node_t *node)
{
#if MLFS_REPLICA

#else
	panic("unexpected code path\n");
#endif

}	

#if 0
/* Add node to device LRU list.

   Note: This function should only be called after data
   is physically migrated.
*/ 
static int update_slru_list(uint8_t dev, lru_node_t *node)
{
	/* For reservce replicas, NVM LRUs are treated as a
	   special case. if NVM LRU exceeds threshold, remove head
	   and schedule for next migration cycle.

	   This is a corner case, and should only occur if ww are
	   digesting during a previous migration cycle.
	*/

	HASH_ADD(hh, g_lru_hash[dev], key, sizeof(lru_key_t), node);
	node->sync = 0;
	list_add(&node->list, &g_lru[dev]->lru_head);
	lru->n++;
}

#endif

int update_slru_list_from_digest(uint8_t dev, lru_key_t k, lru_val_t v)
{
	struct inode *inode;
	lru_node_t *node;
	struct lru *lru;

	if(dev != g_root_dev)
		panic("invalid code path; digesting to non-NVM device");

	pthread_spin_lock(&lru_spinlock);

#if MLFS_REPLICA
	uint64_t used_blocks = sb[dev]->used_blocks;
	uint64_t datablocks = disk_sb[dev].ndatablocks;

	if (used_blocks > (migrate_threshold[dev] * datablocks) / 100)
		lru = &g_stage_lru[dev];
	else
		lru = &g_lru[dev];
#else
	lru = &g_lru[dev];
#endif

	HASH_FIND(hh, g_lru_hash[dev], &k, sizeof(lru_key_t), node);

	if (node) {
		list_del_init(&node->list);
		//node->access_freq[(ALIGN_FLOOR(search_key.offset, g_block_size_bytes)) >> g_block_size_shift]++;

		list_add(&node->list, &lru->lru_head);
	} else {
		node = (lru_node_t *)mlfs_zalloc(sizeof(lru_node_t));

		node->key = k;
		node->val = v;
		//memset(&node->access_freq, 0, LRU_ENTRY_SIZE >> g_block_size_shift);
		INIT_LIST_HEAD(&node->list);

		HASH_ADD(hh, g_lru_hash[dev], key, sizeof(lru_key_t), node);
		node->sync = 0;
		list_add(&node->list, &lru->lru_head);
		lru->n++;

		//evict_slru_lists();
	}

	//printf("Adding LRU inum: %u lblk: %lu\n", node->val.inum, node->val.lblock);

	pthread_spin_unlock(&lru_spinlock);

#if 0
	inode = icache_find(k.inum);
	
	if (inode) {
		// update per-inode lru list.
		if (!is_del_entry(&node->per_inode_list))
			list_del(&node->per_inode_list);

		list_add(&node->per_inode_list, &inode->i_slru_head);
	}
#endif
}

int do_migrate_blocks(uint8_t from_dev, uint8_t to_dev, uint32_t file_inum, 
		offset_t offset, uint32_t length, addr_t blknr)
{
	int ret;
	struct inode *file_inode;
	struct dinode *file_dinode;
	struct buffer_head *bh_data, *bh;
	uint8_t *data;
	struct mlfs_ext_path *path = NULL;
	struct mlfs_map_blocks map;
	uint32_t nr_blocks = 0, nr_digested_blocks = 0;
	offset_t cur_offset;

	/* block migration is much simpler case than file digest */
	mlfs_assert((length >= g_block_size_bytes) && 
			(offset % g_block_size_bytes == 0) &&
			(length % g_block_size_bytes == 0));

	nr_blocks = (length >> g_block_size_shift);

	mlfs_assert(nr_blocks > 0);

	/* optimization: it does not need to read blocks from NVM.
	 * Instead, it is possible to storage to storage copy.  */
	if (from_dev == g_root_dev) {
		data = g_bdev[from_dev]->map_base_addr + (blknr << g_block_size_shift);
		bh = NULL;
	} else {
		uint8_t migrate_buffer[LRU_ENTRY_SIZE];
		struct buffer_head *bh;

		bh = bh_get_sync_IO(from_dev, blknr, BH_NO_DATA_ALLOC);

		bh->b_data = (uint8_t *)migrate_buffer;
		bh->b_size = LRU_ENTRY_SIZE;

		bh_submit_read_sync_IO(bh);
		if (from_dev == g_ssd_dev)
			mlfs_io_wait(g_ssd_dev, 1);

		data = bh->b_data;
		bh_release(bh);
	}

	file_inode = icache_find(file_inum);
	mlfs_assert(file_inode);

	nr_digested_blocks = 0;
	cur_offset = offset;

	// multiple trials of block writing.
	// when extent tree has holes in a certain offset (due to data migration),
	// an extent is split at the hole. Kernfs should call mlfs_ext_get_blocks()
	// with setting m_lblk to the offset having a the hole to fill it.
	while (nr_digested_blocks < nr_blocks) {
		int nr_block_get = 0;
		handle_t handle = {.dev = to_dev};

		mlfs_assert((cur_offset % g_block_size_bytes) == 0);

		map.m_lblk = (cur_offset >> g_block_size_shift);
		map.m_pblk = 0;
		map.m_len = nr_blocks - nr_digested_blocks;
		map.m_flags = 0;

		// find block address of offset and update extent tree
		nr_block_get = mlfs_ext_get_blocks(&handle, file_inode, &map, 
				MLFS_GET_BLOCKS_CREATE);

		/*
		printf("[migrate] inum %d, offset %lu len %u (dev %d:%lu) -> (dev %d:%lu)\n", 
				file_inode->inum, cur_offset, nr_block_get << g_block_size_shift, 
				from_dev, blknr, to_dev, map.m_pblk);
		*/
		if(!map.m_pblk)
			fflush(stdout);

		mlfs_assert(map.m_pblk != 0);

		mlfs_assert(nr_block_get <= (nr_blocks - nr_digested_blocks));
		mlfs_assert(nr_block_get > 0);

		nr_digested_blocks += nr_block_get;

		mlfs_debug("[migrate] inum %d, offset %lu len %u (dev %d:%lu) -> (dev %d:%lu)\n", 
				file_inode->inum, cur_offset, nr_block_get << g_block_size_shift, 
				from_dev, blknr, to_dev, map.m_pblk);

		bh_data = bh_get_sync_IO(to_dev, map.m_pblk, BH_NO_DATA_ALLOC);

		bh_data->b_data = data;
		bh_data->b_blocknr = map.m_pblk;
		bh_data->b_size = nr_block_get * g_block_size_bytes;
		bh_data->b_offset = 0;

#ifdef NIC_OFFLOAD
                // TODO
                // migrate --> It would be better to call nic_rpc_write_local().
                // But it should not use IOAT because migration might
                // move data between different media(ex. NVM to SSD).
                assert(false); // Migration is not supported yet.
		//ret = mlfs_write_no_dma(bh_data);
#else
		ret = mlfs_write(bh_data);
#endif
		mlfs_assert(!ret);
		clear_buffer_uptodate(bh_data);
		bh_release(bh_data);

		cur_offset += nr_block_get * g_block_size_bytes;
		data += nr_block_get * g_block_size_bytes;
	}

	mlfs_assert(nr_blocks == nr_digested_blocks);

	if (file_inode->size < offset + length)
		file_inode->size = offset + length;

	return 0;
}

#if 0
int writeback_blocks(uint8_t from_dev, uint8_t to_dev, isolated_list_t *wb_list)
{
	int ret;
	uint8_t lower_dev;
	lru_node_t *node, *l, *tmp;
	mlfs_fsblk_t blknr;
	struct inode *file_inode;
	struct mlfs_map_blocks map;
	offset_t cur_lblk;
	lru_key_t search_key;
	uint32_t nr_blocks, nr_done = 0;
	uint32_t writeback_success = 0;
	struct list_head wb_success_list;

	INIT_LIST_HEAD(&wb_success_list);

	list_for_each_entry_safe_reverse(l, tmp, &wb_list->head, list) {
		file_inode = icache_find(l->key.inum);
		mlfs_assert(file_inode);

		mlfs_assert(l->key.offset % g_block_size_bytes == 0);
	
		cur_lblk = (l->key.offset >> g_block_size_shift);
		nr_blocks = (LRU_ENTRY_SIZE >> g_block_size_shift);
		nr_done = 0;
again:
		map.m_len = nr_blocks - nr_done;
		map.m_lblk = cur_lblk;
		ret = mlfs_ext_get_blocks(NULL, file_inode, &map, 0);

		mlfs_assert(ret >= 0);

		mlfs_assert(map.m_pblk != 0);

		nr_done += ret;

		mlfs_debug("writeback (%d->%d): inum %d offset %lu(0x%lx)\n", 
				from_dev, to_dev, l->key.inum, l->key.offset, l->key.offset);

		do_migrate_blocks(from_dev, to_dev, 
				l->key.inum, 
				cur_lblk << g_block_size_shift, 
				ret << g_block_size_shift, 
				map.m_pblk);

		/*
		if (to_dev == g_ssd_dev)
			mlfs_io_wait(g_ssd_dev);
		*/

		if (nr_blocks > nr_done) {
			cur_lblk += nr_done;
			goto again;
		}

		l->sync = 1;

		writeback_success++;

		memset(&search_key, 0, sizeof(lru_key_t));
		search_key.inum = l->key.inum;
		search_key.offset = l->key.offset;

		mlfs_assert(l->key.offset % LRU_ENTRY_SIZE == 0);

		// put successful entries to lru list of to_dev

		HASH_FIND(hh, g_lru_hash[to_dev], &search_key, sizeof(lru_key_t), node);

		if (node) { 
			list_del_init(&node->list);
			node->sync = 0;
			list_add(&node->list, &g_lru[to_dev].lru_head);
		} else {
			node = (lru_node_t *)mlfs_zalloc(sizeof(lru_node_t));

			node->key.inum = search_key.inum;
			node->key.offset = ALIGN_FLOOR(search_key.offset, LRU_ENTRY_SIZE);

			INIT_LIST_HEAD(&node->list);

			HASH_ADD(hh, g_lru_hash[to_dev], key, sizeof(lru_key_t), node);
			node->sync = 0;
			list_add(&node->list, &g_lru[to_dev].lru_head);
		}

		g_lru[to_dev].n++;
	}

	mlfs_info("Data writeback (%d -> %d) is done (%u / %u): %u MB\n", 
			from_dev, to_dev,
			writeback_success, wb_list->n,
			(writeback_success * LRU_ENTRY_SIZE) >> 20);

	// cascading data migration to lower than to_dev
	lower_dev = get_lower_dev(to_dev);

#if 1
	if (lower_dev != 0) 
		try_migrate_blocks(to_dev, lower_dev, 0, 0, 0);
#endif

	return 0;
}
#endif

int migrate_blocks(uint8_t from_dev, uint8_t to_dev, isolated_list_t *migrate_list, int swap)
{
	int ret;
	int migrate_down = from_dev < to_dev;
	int migrate_up = !migrate_down;
	lru_node_t *l, *tmp;
	struct lru *from_lru, *to_lru;
	mlfs_fsblk_t blknr;
	struct inode *file_inode;
	struct mlfs_map_blocks map;
	offset_t cur_lblk;
	uint32_t nr_blocks, nr_done = 0;
	uint32_t migrated_success = 0;
	uint8_t lower_dev, upper_dev;
	struct list_head migrate_success_list;
	handle_t handle = {.dev = from_dev};

	INIT_LIST_HEAD(&migrate_success_list);

#if MLFS_REPLICA
		if(from_dev == g_root_dev && swap) {
			from_lru = &g_stage_lru[from_dev];
			to_lru = &g_swap_lru[to_dev];
		}
		else if(migrate_up && swap) {
			from_lru = &g_swap_lru[from_dev];
			to_lru = &g_lru[to_dev];
		}
		else {
			from_lru = &g_lru[from_dev];
			to_lru = &g_lru[to_dev];
		}
#else
		from_lru = &g_lru[from_dev];
		to_lru = &g_lru[to_dev];
#endif

	list_for_each_entry_safe_reverse(l, tmp, &migrate_list->head, list) {
#if 0
		// Skip migrating blocks if it is already writebacked.
		if (l->sync) {
			list_del_init(&l->list);
			list_add(&l->list, &migrate_success_list);

			g_lru[from_dev].n--;

			migrated_success++;
			continue;
		}
#endif

		file_inode = icache_find(l->val.inum);
		mlfs_assert(file_inode);

		/* mlfs_assert(l->key.offset % g_block_size_bytes == 0); */
	
		cur_lblk = l->val.lblock;
		nr_blocks = (LRU_ENTRY_SIZE >> g_block_size_shift);
		
		nr_done = 0;
again:
		map.m_len = nr_blocks - nr_done;
		map.m_lblk = cur_lblk;
		map.m_flags = 0;

		ret = mlfs_ext_get_blocks(&handle, file_inode, &map, 0);

		mlfs_assert(ret >= 0);
		//FIXME: figure out when this condition does not meet.
		//mlfs_assert(map.m_pblk == l->key.block);

		if (ret == 0) {
			list_del_init(&l->list);
			list_add_tail(&l->list, &migrate_list->fail_head);

			mlfs_debug("migrate fail (%d->%d): inum %d offset %lu(0x%lx)\n", 
					from_dev, to_dev, l->val.inum, 
					cur_lblk << g_block_size_shift, 
					cur_lblk << g_block_size_shift);
			continue;
		}

		mlfs_assert(map.m_pblk != 0);

		nr_done += ret;

		mlfs_debug("migrate (%d->%d): inum %d offset %lu(0x%lx)\n", 
				from_dev, to_dev, l->val.inum, 
				cur_lblk << g_block_size_shift, 
				cur_lblk << g_block_size_shift);

		do_migrate_blocks(from_dev, to_dev, 
				l->val.inum, 
				cur_lblk << g_block_size_shift, 
				ret << g_block_size_shift, 
				map.m_pblk);


		if (nr_blocks > nr_done) {
			cur_lblk += nr_done;
			mlfs_debug("current extent is small than LRY_ENTRY_SIZE: block got %u\n",
					nr_done);
			goto again;
		}

		list_del_init(&l->list);
		list_add(&l->list, &migrate_success_list);
		
		from_lru->n--;
		migrated_success++;
	}

	// Wait for finishing all outstanding IO.
	if (to_dev == g_ssd_dev)
		mlfs_io_wait(g_ssd_dev, 1);

	list_for_each_entry_safe(l, tmp, &migrate_success_list, list) {
		handle_t handle;
		mlfs_lblk_t start, end;
		file_inode = icache_find(l->val.inum);
		mlfs_assert(file_inode);

		start = l->val.lblock;
		end = start + (LRU_ENTRY_SIZE >> g_block_size_shift);

		mlfs_assert((end << g_block_size_shift) <= file_inode->size);

		handle.dev = from_dev;
		ret = mlfs_ext_truncate(&handle, file_inode, start, end - 1);

		if (ret != 0) {
			list_del_init(&l->list);
			list_add(&l->list, &migrate_list->fail_head);
			continue;
		}

		mlfs_debug("[truncate] dev %d inum = %d offset %u ~ %u\n",
				from_dev, file_inode->inum, start, end);

		// The followings are nothing but checking if
		// the truncate is done correctly
#if 0
		mlfs_debug("%s", "********************************\n");
		map.m_len = 1;
		map.m_lblk = (l->val.lblock >> g_block_size_shift);
		ret = mlfs_ext_get_blocks(&handle, file_inode, &map, 0);
		//mlfs_debug("truncated offset %lu ret %d\n", l->offset, ret);

		mlfs_assert(ret == 0);
		mlfs_debug("get_blocks(dev = %d): offset %lu(0x%lx) ret %d\n", 
				g_root_dev, l->offset, l->offset, ret);

		file_inode = icache_find(l->inum);
		mlfs_assert(file_inode);

		ret = mlfs_ext_get_blocks(NULL, file_inode, &map, 0);
		mlfs_assert(ret == map.m_len);
		mlfs_debug("get_blocks(dev = %d): offset %lu(0x%lx) ret %d\n", 
				g_ssd_dev, l->offset, l->offset, ret);
#endif

		list_del_init(&l->list);
		HASH_DEL(g_lru_hash[from_dev], l);

		//TODO: for coordinated migration to work, we need to maintain
		//hdd lru_list as well (since data can move from hdd to ssd/nvm)
		if (to_dev == g_hdd_dev) {
			mlfs_free(l);
		} else {
			// put successful entries to lru list of to_dev
			HASH_ADD(hh, g_lru_hash[to_dev], key, sizeof(lru_key_t), l);
			l->sync = 0;
			list_add(&l->list, &to_lru->lru_head);
			to_lru->n++;
		}
	}

	mlfs_info("Data migration (%d -> %d) is done (%u / %u): %u MB\n", 
			from_dev, to_dev,
			migrated_success, migrate_list->n,
			(migrated_success * LRU_ENTRY_SIZE) >> 20);

	g_perf_stats.total_migrated_mb += 
		((migrated_success * LRU_ENTRY_SIZE) >> 20);

	show_storage_stats();

	// cascading data migration to lower than to_dev
	lower_dev = get_lower_dev(to_dev);

	if (lower_dev != 0) 
		try_migrate_blocks(to_dev, lower_dev, 0, 0, 0);

#if MLFS_REPLICA
	uint64_t used_blocks = sb[to_dev]->used_blocks;
	uint64_t datablocks = disk_sb[to_dev].ndatablocks;

	//migrate up data that is about to be migrated down by primary
	if (migrate_down && swap &&
			used_blocks > (migrate_threshold[from_dev] * datablocks) / 100) {
		mlfs_debug("try migrate up - replica [from_dev: %d to_dev: %d]\n", to_dev, from_dev);
		try_migrate_blocks(to_dev, from_dev, migrated_success, 1, 1);
	}

#endif

	return migrated_success;
}

#if 0
// TODO: Background writeback does not work yet.
int try_writeback_blocks(uint8_t from_dev, uint8_t to_dev)
{
	uint32_t n_entries, i = 0, ret;
	uint64_t used_blocks, datablocks;
	struct list_head writeback_list;
	lru_node_t *node, *tmp;

	used_blocks = sb[from_dev].used_blocks;
	datablocks = disk_sb[from_dev].ndatablocks;

	INIT_LIST_HEAD(&writeback_list);

	list_for_each_entry_safe_reverse(node, tmp, 
			&g_lru[from_dev].lru_head, list) {
		// isolate from per-device lru
		list_del_init(&node->list);

		list_add(&node->list, &writeback_list);
		i++;

		if (node->sync)
			continue;

		if (i >= (30 * g_lru[from_dev].n) / 100)
			break;
	}

	writeback_blocks(from_dev, to_dev, &writeback_list);

	// put writeback list to per-device lru
	list_for_each_entry_safe(node, tmp, &writeback_list, list) {
		list_del_init(&node->list);
		mlfs_debug("putback (%d->%d): inum %d offset %lu(0x%lx)\n", 
				from_dev, to_dev, 
				node->key.inum, node->key.offset, node->key.offset);
		list_add_tail(&node->list, &g_lru[from_dev].lru_head);
	}

	return 0;
}
#endif

/* nr_blocks: minimum amount of blocks to migrate */
int try_migrate_blocks(uint8_t from_dev, uint8_t to_dev, uint32_t nr_blocks, uint8_t force, int swap)
{
#ifdef MIGRATION
	int migrate_down = from_dev < to_dev;
	int migrate_up = !migrate_down;
	uint32_t n_entries = 0, i = 0, ret, do_migrate = 0;
	uint64_t used_blocks, datablocks;
	struct isolated_list migrate_list;
	lru_node_t *node, *tmp;

#ifndef USE_SSD
	return 0;
#endif

	if (force) {
		do_migrate = 1;
		goto do_force_migration;
	}

	used_blocks = sb[from_dev]->used_blocks;
	datablocks = disk_sb[from_dev].ndatablocks;

	if (used_blocks > (migrate_threshold[from_dev] * datablocks) / 100) {
		//printf("accept [%d to %d]: used_block: %lu threshold*datablocks: %lu\n",
		//		from_dev, to_dev, used_blocks, threshold*datablocks);
		n_entries = BLOCKS_TO_LRU_ENTRIES(
				used_blocks - ((migrate_threshold[from_dev] * datablocks) / 100));
		do_migrate = 1;
	} else {
		//printf("ignore [%d to %d]: used_block: %lu threshold*datablocks: %lu\n",
		//		from_dev, to_dev, used_blocks, threshold*datablocks);
		// do not support writeback.
		return 0;

		if (used_blocks > (wb_threshold[from_dev] * datablocks) / 100) {
			n_entries = 0;
			do_migrate = 0;
		} else
			return 0;
	}

do_force_migration:
	if (nr_blocks == 0) 
		nr_blocks = BLOCKS_PER_LRU_ENTRY * MIN_MIGRATE_ENTRY;

	INIT_LIST_HEAD(&migrate_list.head);
	INIT_LIST_HEAD(&migrate_list.fail_head);

	n_entries = max(n_entries, BLOCKS_TO_LRU_ENTRIES(nr_blocks));
	mlfs_debug("migration: %d->%d n_entries %u\n", from_dev, to_dev, n_entries);

	migrate_list.n = 0;

	struct lru *from_lru;
#if MLFS_REPLICA
	if(from_dev == g_root_dev && swap)
		from_lru = &g_stage_lru[from_dev];
	else if(migrate_up && swap)
		from_lru = &g_swap_lru[from_dev];
	else
		from_lru = &g_lru[from_dev];
#else
	from_lru = &g_lru[from_dev];
#endif

	ret = 0;
	while (ret < n_entries) {
		list_for_each_entry_safe_reverse(node, tmp, &from_lru->lru_head, list) {
			// isolate list from per-device lru list.
			list_del_init(&node->list);
				
			if (node->val.inum == ROOTINO)
				continue;

			list_add(&node->list, &migrate_list.head);
			migrate_list.n++;

			i++;

			mlfs_debug("try migrate (%d->%d): iter %d inum %d offset %lu(0x%lx) phys %lu\n", 
					i, from_dev, to_dev, node->val.inum, node->val.lblock, 
					node->val.lblock, node->key.block);

			if (i >= n_entries)
				break;
		}
		ret += migrate_blocks(from_dev, to_dev, &migrate_list, swap);
	}

	// TODO: figure out why the list fails.
	// Put back the failed list to lru_head
	list_for_each_entry_safe_reverse(node, tmp, &migrate_list.fail_head, list) {
		mlfs_debug("migrate fail (%d->%d): inum %d offset %lu(0x%lx)\n", 
				from_dev, to_dev, node->val.inum, 
				node->val.lblock, node->val.lblock);

		list_del_init(&node->list);
		HASH_DEL(g_lru_hash[from_dev], node);
		mlfs_free(node);
		//list_add_tail(&node->list, &from_lru->lru_head);
	}

	return 0;
#endif // MIGRATION
}
