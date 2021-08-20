#ifndef _FS_H_
#define _FS_H_

#include "global/global.h"
#include "global/types.h"
#include "global/defs.h"
#include "log/log.h"
#include "filesystem/stat.h"
#include "filesystem/shared.h"
#include "global/mem.h"
#include "global/ncx_slab.h"
#include "filesystem/extents.h"
#include "filesystem/extents_bh.h"
#include "ds/uthash.h"
#include "ds/khash.h"

//#if MLFS_LEASE
#include "experimental/leases.h"
//#endif

#ifdef __cplusplus
extern "C" {
#endif

// libmlfs Disk layout:
// [ boot block | sb block | inode blocks | free bitmap | data blocks | log blocks ]
// [ inode block | free bitmap | data blocks | log blocks ] is a block group.
// If data blocks is full, then file system will allocate a new block group.
// Block group expension is not implemented yet.

// directory entry cache
struct dirent_data {
	//mlfs_hash_t hh;
	char name[DIRSIZ]; // key
	struct inode *inode;
	offset_t offset;
};

struct fcache_block {
	offset_t key;
	mlfs_hash_t hash_handle;
	uint32_t inum;
	uint8_t invalidate;
	uint8_t is_data_cached;
	uint8_t *data;
	struct list_head l;		// entry for global list

	//log pointers
	uint32_t log_entries;		// # of log blocks for patching cache
	uint32_t log_version;		// version of last log entry
	struct list_head log_head;	// list of log patches
};

//patch entries pointing to log
struct fcache_log {
	addr_t addr;    	//block # of log data
	addr_t hdr_addr;	//block # of log header
	uint32_t offset;  	//data offset
	uint32_t size;		//data size
	uint32_t version;	//log version
	struct list_head l;	//entry for fcache block list
};

struct cache_copy_list {
	uint8_t *dst_buffer;
	uint8_t *cached_data;
	uint32_t size;
	struct list_head l;
};

struct dlookup_data {
	mlfs_hash_t hh;
	char path[MAX_PATH];	// key: canonical path
	struct inode *inode;
};

typedef struct bmap_request {
	// input 
	offset_t start_offset;
	uint32_t blk_count;
	// output
	addr_t block_no;
	uint32_t blk_count_found;
	uint8_t dev;
} bmap_req_t;

// statistics
typedef struct mlfs_libfs_stats {
	uint64_t calculating_sync_interval_time_tsc;
	uint64_t coalescing_log_time_tsc; 
	uint64_t rdma_write_time_tsc;
	uint64_t digest_wait_tsc;
	uint32_t digest_wait_nr;
	uint64_t l0_search_tsc;
	uint32_t l0_search_nr;
	uint64_t tree_search_tsc;
	uint32_t tree_search_nr;
	uint64_t log_write_tsc;
	uint64_t loghdr_write_tsc;
	uint32_t log_write_nr;
	uint64_t log_commit_tsc;
	uint32_t log_commit_nr;
	uint64_t posix_rename_tsc;
	uint64_t read_data_tsc;
	uint32_t read_data_nr;
	uint64_t dir_search_tsc;
	uint32_t dir_search_nr_hit;
	uint32_t dir_search_nr_miss;
	uint32_t dir_search_nr_notfound;
	uint64_t ialloc_tsc;
	uint32_t ialloc_nr;
	uint64_t tmp_tsc;
	uint64_t bcache_search_tsc;
	uint32_t bcache_search_nr;
	uint32_t n_rsync;
	uint32_t n_rsync_skipped;
	uint32_t n_rsync_blks;
	uint32_t n_rsync_blks_skipped;
	uint32_t rsync_ops;
	uint64_t read_rpc_wait_tsc;
	uint32_t read_rpc_nr;
	uint64_t lease_rpc_wait_tsc;
	uint64_t lease_lpc_wait_tsc;
	uint32_t lease_rpc_nr;
	uint32_t lease_lpc_nr;
	uint64_t lease_revoke_wait_tsc;
	uint64_t local_contention_tsc;
	uint64_t local_digestion_tsc;
} libfs_stat_t;

extern struct lru g_fcache_head;

extern libfs_stat_t g_perf_stats;
extern uint8_t enable_perf_stats;

extern pthread_rwlock_t *icache_rwlock;
extern pthread_rwlock_t *icreate_rwlock;
extern pthread_rwlock_t *dcache_rwlock;
extern pthread_rwlock_t *dlookup_rwlock;
extern pthread_rwlock_t *invalidate_rwlock;
extern pthread_rwlock_t *g_fcache_rwlock;

extern struct inode *inode_hash;
extern struct dlookup_data *dlookup_hash;

static inline struct inode *icache_find(uint32_t inum)
{
	struct inode *inode;

	pthread_rwlock_rdlock(icache_rwlock);

	HASH_FIND(hash_handle, inode_hash, &inum,
        		sizeof(uint32_t), inode);

	pthread_rwlock_unlock(icache_rwlock);

	return inode;
}

static inline struct inode *icache_alloc_add(uint32_t inum)
{
	struct inode *inode;

	inode = (struct inode *)mlfs_zalloc(sizeof(*inode));

	if (!inode)
		panic("Fail to allocate inode\n");

	inode->inum = inum;
	inode->i_ref = 1;

	pthread_mutex_init(&inode->i_mutex, NULL);

	inode->_dinode = (struct dinode *)inode;

	pthread_rwlock_wrlock(icache_rwlock);

	HASH_ADD(hash_handle, inode_hash, inum,
	 		sizeof(uint32_t), inode);

	pthread_rwlock_unlock(icache_rwlock);

	return inode;
}

static inline struct inode *icache_add(struct inode *inode)
{
	uint32_t inum = inode->inum;

	pthread_mutex_init(&inode->i_mutex, NULL);
	
	pthread_rwlock_wrlock(icache_rwlock);

	HASH_ADD(hash_handle, inode_hash, inum,
	 		sizeof(uint32_t), inode);

	pthread_rwlock_unlock(icache_rwlock);

	return inode;
}

static inline int icache_del(struct inode *ip)
{
	pthread_rwlock_wrlock(icache_rwlock);

	HASH_DELETE(hash_handle, inode_hash, ip);

	pthread_rwlock_unlock(icache_rwlock);

	return 0;
}

static inline void fcache_log_add(struct fcache_block *fc_block, addr_t addr,
		uint32_t offset, uint32_t size, uint32_t version)
{
	struct fcache_log *fc_log;

	fc_log = (struct fcache_log *)mlfs_zalloc(sizeof(*fc_log));

	if (!fc_log)
		panic("Fail to allocate fcache log entry\n");

	fc_log->addr = addr;
	fc_log->offset = offset;
	fc_log->size = size;
	fc_log->version = version;

	INIT_LIST_HEAD(&fc_log->l);

	fc_block->log_entries++;

	list_move(&fc_log->l, &fc_block->log_head);
}

static inline void fcache_log_replace(struct fcache_block *fc_block, addr_t addr,
		uint32_t offset, uint32_t size, uint32_t version)
{
	struct fcache_log *fc_log, *entry, *tmp;

	fc_log = (struct fcache_log *)mlfs_zalloc(sizeof(*fc_log));
	if (!fc_log)
		panic("Fail to allocate fcache log entry\n");

	fc_log->addr = addr;
	fc_log->offset = offset;
	fc_log->size = size;
	fc_log->version = version;
	INIT_LIST_HEAD(&fc_log->l);
	
	list_for_each_entry_safe(entry, tmp, &fc_block->log_head, l) {
		if (entry->offset == fc_log->offset && entry->size <= fc_log->size) {
			list_del(&entry->l);
			mlfs_free(entry);
			fc_block->log_entries--;
			break;
		}
	}

	fc_block->log_entries++;
	list_move(&fc_log->l, &fc_block->log_head);
}


static inline void fcache_log_del(struct fcache_block *fc_block,
		struct fcache_log *fc_log)
{
	list_del(&fc_log->l);
	mlfs_free(fc_log);

	fc_block->log_entries--;

	//mlfs_assert(fc_block->log_entries > 0);

	if(!fc_block->log_entries)
		fc_block->log_version = 0;
}

static inline void fcache_log_del_all(struct fcache_block *fc_block)
{
	struct fcache_log *entry, *tmp;

	//TODO: assign a thread in parallel to do this
	//might be necessary for small writes
	list_for_each_entry_safe(entry, tmp, &fc_block->log_head, l) {
		list_del(&entry->l);
		mlfs_free(entry);
	}

	fc_block->log_entries = 0;
	fc_block->log_version = 0;
}

#ifdef KLIB_HASH
static inline struct fcache_block *fcache_find(struct inode *inode, offset_t key)
{
	khiter_t k;
	struct fcache_block *fc_block = NULL;

	if (inode->fcache_hash == NULL) {
		pthread_rwlock_wrlock(&inode->fcache_rwlock);
		inode->fcache_hash = kh_init(fcache);
		pthread_rwlock_unlock(&inode->fcache_rwlock);
	}

	pthread_rwlock_rdlock(&inode->fcache_rwlock);

	k = kh_get(fcache, inode->fcache_hash, key);
	if (k == kh_end(inode->fcache_hash)) {
		pthread_rwlock_unlock(&inode->fcache_rwlock);
		return NULL;
	}

	fc_block = kh_value(inode->fcache_hash, k);

	pthread_rwlock_unlock(&inode->fcache_rwlock);

	return fc_block;
}

static inline struct fcache_block *fcache_alloc_add(struct inode *inode, 
		offset_t key)
{
	struct fcache_block *fc_block;
	khiter_t k;
	int ret;

	fc_block = (struct fcache_block *)mlfs_zalloc(sizeof(*fc_block));
	if (!fc_block)
		panic("Fail to allocate fcache block\n");

	fc_block->key = key;
	fc_block->invalidate = 0;
	fc_block->is_data_cached = 0;
	fc_block->inum = inode->inum;
	inode->n_fcache_entries++;
	INIT_LIST_HEAD(&fc_block->l);
	INIT_LIST_HEAD(&fc_block->log_head);

	pthread_rwlock_wrlock(&inode->fcache_rwlock);

	if (inode->fcache_hash == NULL) {
		inode->fcache_hash = kh_init(fcache);
	}

	k = kh_put(fcache, inode->fcache_hash, key, &ret);
	if (ret < 0)
		panic("fail to insert fcache value");
	/*
	else if (!ret) {
		kh_del(fcache, inode->fcache_hash, k);
		k = kh_put(fcache, inode->fcache_hash, key, &ret);
	}
	*/

	kh_value(inode->fcache_hash, k) = fc_block;
	//mlfs_info("add key %u @ inode %u\n", key, inode->inum);

	pthread_rwlock_unlock(&inode->fcache_rwlock);

	return fc_block;
}

static inline int fcache_del(struct inode *inode, 
		struct fcache_block *fc_block)
{
	khiter_t k;
	//struct fcache_block *fc_block;

	pthread_rwlock_wrlock(&inode->fcache_rwlock);

	k = kh_get(fcache, inode->fcache_hash, fc_block->key);

	if (kh_exist(inode->fcache_hash, k)) {
		fc_block = kh_value(inode->fcache_hash, k);
		fcache_log_del_all(fc_block);
		kh_del(fcache, inode->fcache_hash, k);
		inode->n_fcache_entries--;
	}

	/*
	if (k != kh_end(inode->fcache_hash)) {
		kh_del(fcache, inode->fcache_hash, k);
		inode->n_fcache_entries--;
		mlfs_debug("del key %u @ inode %u\n", fc_block->key, inode->inum);
	}
	*/

	pthread_rwlock_unlock(&inode->fcache_rwlock);

	return 0;
}

static inline int fcache_del_all(struct inode *inode)
{
	khiter_t k;
	struct fcache_block *fc_block;

	for (k = kh_begin(inode->fcache_hash); 
			k != kh_end(inode->fcache_hash); k++) {
		if (kh_exist(inode->fcache_hash, k)) {
			fc_block = kh_value(inode->fcache_hash, k);
			fcache_log_del_all(fc_block);
			if (fc_block && fc_block->is_data_cached) {
				list_del(&fc_block->l);
				mlfs_free(fc_block->data);
			}
			kh_del(fcache, inode->fcache_hash, k);
			//mlfs_free(fc_block);
		}
	}

	mlfs_debug("destroy hash %u\n", inode->inum);
	kh_destroy(fcache, inode->fcache_hash);
	inode->fcache_hash = NULL;
	return 0;
}
// UTHash version
#else
static inline struct fcache_block *fcache_find(struct inode *inode, offset_t key)
{
	struct fcache_block *fc_block = NULL;

	pthread_rwlock_rdlock(&inode->fcache_rwlock);

	HASH_FIND(hash_handle, inode->fcache, &key,
        		sizeof(offset_t), fc_block);
	
	pthread_rwlock_unlock(&inode->fcache_rwlock);

	return fc_block;
}

static inline struct fcache_block *fcache_alloc_add(struct inode *inode, 
		offset_t key)
{
	struct fcache_block *fc_block;

	fc_block = (struct fcache_block *)mlfs_zalloc(sizeof(*fc_block));
	if (!fc_block)
		panic("Fail to allocate fcache block\n");

	fc_block->key = key;
	fc_block->inum = inode->inum;
	fc_block->invalidate = 0;
	fc_block->is_data_cached = 0;
	inode->n_fcache_entries++;
	INIT_LIST_HEAD(&fc_block->l);
	INIT_LIST_HEAD(&fc_block->log_head);

	pthread_rwlock_wrlock(&inode->fcache_rwlock);

	HASH_ADD(hash_handle, inode->fcache, key,
	 		sizeof(offset_t), fc_block);

	pthread_rwlock_unlock(&inode->fcache_rwlock);

	return fc_block;
}

static inline int fcache_del(struct inode *inode, 
		struct fcache_block *fc_block)
{
	pthread_rwlock_wrlock(&inode->fcache_rwlock);
	assert(fc_block);
	fcache_log_del_all(fc_block);
	HASH_DELETE(hash_handle, inode->fcache, fc_block);
	inode->n_fcache_entries--;

	pthread_rwlock_unlock(&inode->fcache_rwlock);

	return 0;
}

static inline int fcache_del_all(struct inode *inode)
{
	struct fcache_block *item, *tmp;

	pthread_rwlock_wrlock(&inode->fcache_rwlock);

	HASH_ITER(hash_handle, inode->fcache, item, tmp) {
		HASH_DELETE(hash_handle, inode->fcache, item);
		fcache_log_del_all(item);
		if (item->is_data_cached) {
			list_del(&item->l);
			mlfs_free(item->data);
		}
		mlfs_free(item);
	}
	HASH_CLEAR(hash_handle, inode->fcache);

	inode->n_fcache_entries = 0;

	pthread_rwlock_unlock(&inode->fcache_rwlock);

	return 0;
}
#endif

#ifdef KLIB_HASH
static inline struct inode *de_cache_find(struct inode *dir_inode, 
		const char *_name, offset_t *offset)
{
	khiter_t k;
	struct dirent_data *dirent_data;

	pthread_rwlock_rdlock(&dir_inode->de_cache_rwlock);

	k = kh_get(de_cache, dir_inode->de_cache_hash, _name);

	if (k == kh_end(dir_inode->de_cache_hash)) {
		*offset = 0;
		pthread_rwlock_unlock(&dir_inode->de_cache_rwlock);
		return NULL;
	}

	dirent_data = kh_value(dir_inode->de_cache_hash, k);

	pthread_rwlock_unlock(&dir_inode->de_cache_rwlock);

	mlfs_assert(dirent_data);

	*offset = dirent_data->offset;

	return dirent_data->inode;
}

static inline struct inode *de_cache_add(struct inode *dir_inode, 
		const char *name, struct inode *inode, offset_t _offset)
{
	int absent;
	khiter_t k;
	struct dirent_data *_dirent_data;

	if (!strncmp(name, ".", DIRSIZ) || !strncmp(name, "..", DIRSIZ))
		return dir_inode;

	pthread_rwlock_wrlock(&dir_inode->de_cache_rwlock);

	k = kh_get(de_cache, dir_inode->de_cache_hash, name);
	if (k == kh_end(dir_inode->de_cache_hash)) {
		_dirent_data = (struct dirent_data *)mlfs_zalloc(sizeof(*_dirent_data));
		if (!_dirent_data)
			panic("Fail to allocate dirent data\n");

		k = kh_put(de_cache, dir_inode->de_cache_hash, name, &absent);
		if (absent)
			kh_key(dir_inode->de_cache_hash, k) = strdup(name);

		kh_value(dir_inode->de_cache_hash, k) = _dirent_data;
	} else {
		_dirent_data = kh_value(dir_inode->de_cache_hash, k);
	}

	strcpy(_dirent_data->name, name);
	_dirent_data->inode = inode;
	_dirent_data->offset = _offset;

	pthread_rwlock_unlock(&dir_inode->de_cache_rwlock);
	dir_inode->n_de_cache_entry++;

	return dir_inode;
}

static inline int de_cache_del(struct inode *dir_inode, const char *_name)
{
	khiter_t k;
	struct dirent_data *_dirent_data;
	k = kh_get(de_cache, dir_inode->de_cache_hash, _name);

	if (k != kh_end(dir_inode->de_cache_hash)) {
		_dirent_data = kh_value(dir_inode->de_cache_hash, k);
		pthread_rwlock_wrlock(&dir_inode->de_cache_rwlock);

		kh_del(de_cache, dir_inode->de_cache_hash, k);

		pthread_rwlock_unlock(&dir_inode->de_cache_rwlock);

		mlfs_free((char *)kh_key(dir_inode->de_cache_hash, k));

		mlfs_free(_dirent_data);

		dir_inode->n_de_cache_entry--;
	}
	return 0;
}

static inline int de_cache_del_all(struct inode *inode)
{
	khiter_t k;
	struct dirent_data *_dirent_data;

	for (k = kh_begin(inode->de_cache_hash); 
			k != kh_end(inode->de_cache_hash); k++) {
		if (kh_exist(inode->de_cache_hash, k)) {
			_dirent_data = kh_value(inode->de_cache_hash, k);
			mlfs_free(_dirent_data);
		}

		inode->n_de_cache_entry--;
	}
	kh_destroy(de_cache, inode->de_cache_hash);
	return 0;
}
#else
	//panic("de_cache requires KLIB_HASH\n");
#endif

static inline struct inode *dlookup_find(char *path)
{
	struct dlookup_data *_dlookup_data;

	pthread_rwlock_rdlock(dlookup_rwlock);

	HASH_FIND_STR(dlookup_hash, path, _dlookup_data);

	pthread_rwlock_unlock(dlookup_rwlock);

	if (!_dlookup_data)
		return NULL;
	else
		return _dlookup_data->inode;
}

static inline struct inode *dlookup_alloc_add(struct inode *inode, const char *_path)
{
	struct dlookup_data *_dlookup_data;

	_dlookup_data = (struct dlookup_data *)mlfs_zalloc(sizeof(*_dlookup_data));
	if (!_dlookup_data)
		panic("Fail to allocate dlookup data\n");

	strcpy(_dlookup_data->path, _path);

	_dlookup_data->inode = inode;

	pthread_rwlock_wrlock(dlookup_rwlock);

	HASH_ADD_STR(dlookup_hash, path, _dlookup_data);

	pthread_rwlock_unlock(dlookup_rwlock);

	return inode;
}

static inline int dlookup_del(const char *path)
{
	struct dlookup_data *_dlookup_data;

	pthread_rwlock_wrlock(dlookup_rwlock);

	HASH_FIND_STR(dlookup_hash, path, _dlookup_data);
	if (_dlookup_data) {
		HASH_DEL(dlookup_hash, _dlookup_data);
		mlfs_free(_dlookup_data);
	}

	pthread_rwlock_unlock(dlookup_rwlock);

	return 0;
}

// global variables
extern uint8_t fs_dev_id;
extern struct disk_superblock *disk_sb;
extern struct super_block *sb[g_n_devices + 1];

//forward declaration
struct fs_stat;

void shared_slab_init(uint8_t shm_slab_index);

void read_superblock(uint8_t dev);
void read_root_inode();

int read_ondisk_inode(uint32_t inum, struct dinode *dip);
int sync_all_inode_ext_trees();
int sync_inode_ext_tree(struct inode *inode);
struct inode* icreate(uint8_t type);
struct inode* ialloc(uint32_t inum, struct dinode *dip);
int idealloc(struct inode *inode);
struct inode* idup(struct inode*);
struct inode* iget(uint32_t inum);
void ilock(struct inode*);
void iput(struct inode*);
void iunlock(struct inode*);
void iunlockput(struct inode*);
void iupdate(struct inode*);
int itrunc(struct inode *inode, offset_t length);
int bmap(struct inode *ip, struct bmap_request *bmap_req);

struct inode* dir_lookup(struct inode*, char*, offset_t *);
struct mlfs_dirent *dir_add_links(struct inode *dir_inode, uint32_t inum, uint32_t parent_inum);
struct mlfs_dirent *dir_add_entry(struct inode *dir_inode, char *name, struct inode *inode);
struct mlfs_dirent *dir_change_entry(struct inode *dir_inode, char *oldname, char *newname);
struct mlfs_dirent *dir_remove_entry(struct inode *dir_inode,char *name, struct inode **found);
int dir_get_entry(struct inode *dir_inode, struct linux_dirent *buf, offset_t off);
int dir_get_entry64(struct inode *dir_inode, struct linux_dirent64 *buf, offset_t off);
int namecmp(const char*, const char*);
struct inode* namei(char*);
struct inode* nameiparent(char*, char*);
int readi_unopt(struct inode*, uint8_t *, offset_t, uint32_t);
int readi(struct inode*, struct mlfs_reply*, offset_t, uint32_t, char *);
void stati(struct inode*, struct stat *);
int add_to_log(struct inode*, uint8_t*, offset_t, uint32_t, uint8_t);
int check_log_invalidation(struct fcache_block *_fcache_block);
int get_dirent(struct inode *dir_inode, struct mlfs_dirent *buf, offset_t offset);
void show_libfs_stats(void);

//APIs for debugging.
uint32_t dbg_get_iblkno(uint32_t inum);
void dbg_dump_inode(uint8_t dev, uint32_t inum);
void dbg_check_inode(void *data);
void dbg_check_dir(void *data);
void dbg_dump_dir(uint8_t dev, uint32_t inum);
void dbg_path_walk(char *path);

// mempool slab for libfs
extern ncx_slab_pool_t *mlfs_slab_pool;
// mempool on top of shared memory
extern ncx_slab_pool_t *mlfs_slab_pool_shared;
extern uint8_t shm_slab_index;

extern pthread_rwlock_t *shm_slab_rwlock; 
extern pthread_rwlock_t *shm_lru_rwlock; 

extern uint64_t *bandwidth_consumption;

// Inodes per block.
#define IPB           (g_block_size_bytes / sizeof(struct dinode))

// Block containing inode i
/*
#define IBLOCK(i, disk_sb)  ((i/IPB) + disk_sb.inode_start)
*/
static inline addr_t get_inode_block(uint8_t dev, uint32_t inum)
{
	return (inum / IPB) + disk_sb[dev].inode_start;
}


#define LPB           (g_block_size_bytes / sizeof(mlfs_lease_t))

#if MLFS_LEASE
// Block containing inode i
static inline addr_t get_lease_block(uint8_t dev, uint32_t inum)
{
	return (inum / LPB) + disk_sb[dev].lease_start;
}

#endif

// Bitmap bits per block
#define BPB           (g_block_size_bytes*8)

// Block of free map containing bit for block b
#define BBLOCK(b, disk_sb) (b/BPB + disk_sb.bmap_start)

#ifdef __cplusplus
}
#endif

#endif
