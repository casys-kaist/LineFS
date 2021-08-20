#include "slru.h"
#include "filesystem/shared.h"

#ifdef __x86_64__
#include "fs.h"
#elif __aarch64__
#include "nic/nic_fs.h"
#else
#error "Unsupported architecture."
#endif

#include "global/util.h"

lru_node_t *lru_hash;

int slru_upsert(struct inode *inode, struct list_head *lru_head, lru_key_t k, lru_val_t v) 
{
	lru_node_t *node, search;

	memset(&search, 0, sizeof(lru_node_t));

	search.key = k;

#ifdef LIBFS
	pthread_rwlock_wrlock(shm_lru_rwlock);
#endif

	HASH_FIND(hh, lru_hash, &search.key, sizeof(lru_key_t), node);

	if (!node) {
		//node = (lru_node_t *)mlfs_alloc(sizeof(lru_node_t));
		node = (lru_node_t *)mlfs_alloc_shared(sizeof(lru_node_t));
		// if forgot to this memset, UThash does not work.
		memset(node, 0, sizeof(lru_node_t));

		mlfs_debug("add a new key: dev %u, block%lx\n", k.dev, k.block);

		node->key = k;
		node->val = v;
		//memset(&node->access_freq, 0, LRU_ENTRY_SIZE >> g_block_size_shift);

		INIT_LIST_HEAD(&node->list);
		INIT_LIST_HEAD(&node->per_inode_list);

		HASH_ADD(hh, lru_hash, key, sizeof(lru_key_t), node);

		node->sync = 0;
	}

	/*
	if (inode) {
		if (!is_del_entry(&node->per_inode_list))
			list_del(&node->per_inode_list);

		list_add(&node->per_inode_list, &inode->i_slru_head);
	}
	*/

	// Add to head of lru_list.
	list_del_init(&node->list);
	list_add(&node->list, lru_head);

	// update access frequency information.
	//node->access_freq[(ALIGN_FLOOR(k.offset, g_block_size_bytes)) >> g_block_size_shift]++;

#ifdef LIBFS
	pthread_rwlock_unlock(shm_lru_rwlock);
#endif

	return 0;
}

