#ifdef __x86_64__
#ifndef _FS_H_
#define _FS_H_

#include "global/global.h"
#include "global/types.h"
#include "global/defs.h"
#include "global/mem.h"
#include "global/ncx_slab.h"
#include "ds/uthash.h"
#include "ds/rbtree.h"
#include "filesystem/shared.h"
#include "concurrency/thpool.h"
#include "concurrency/synchronization.h"
#include "distributed/rpc_interface.h"
#include "experimental/leases.h"
#include "digest.h"

#ifdef NIC_OFFLOAD
#include "nic/fetch_log.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// libmlfs Disk layout:
// [ boot block | sb block | inode blocks | free bitmap | data blocks | log blocks ]
// [ inode block | free bitmap | data blocks | log blocks ] is a block group.
// If data blocks is full, then file system will allocate a new block group.
// Block group expension is not implemented yet.

typedef struct mlfs_kernfs_stats {
	uint64_t digest_time_tsc; 
	uint64_t path_search_tsc;
	uint64_t replay_time_tsc;
	uint64_t apply_time_tsc;
	uint64_t digest_dir_tsc;
	uint64_t digest_inode_tsc;
	uint64_t digest_file_tsc;
	uint64_t persist_time_tsc;
	uint64_t n_digest;
	uint64_t n_digest_skipped;
	uint64_t total_migrated_mb;
#ifdef MLFS_LEASE
	uint64_t lease_rpc_local_nr;
	uint64_t lease_rpc_remote_nr;
	uint64_t lease_contention_nr;
	uint64_t lease_migration_nr;
#endif
} kernfs_stats_t;

extern struct disk_superblock disk_sb[g_n_devices + 1];
extern struct super_block *sb[g_n_devices + 1];
extern kernfs_stats_t g_perf_stats;
extern uint8_t enable_perf_stats;

// Inodes per block.
#define IPB           (g_block_size_bytes / sizeof(struct dinode))

struct mlfs_range_node *mlfs_alloc_blocknode(struct super_block *sb);
struct mlfs_range_node *mlfs_alloc_inode_node(struct super_block *sb);

extern pthread_spinlock_t icache_spinlock;
extern pthread_spinlock_t dcache_spinlock;

extern struct dirent_block *dirent_hash[g_n_devices + 1];
extern struct inode *inode_hash;

struct bootstrap_arg
{
    int sockfd;
    uint64_t seqn;
};

struct peer_register_arg
{
    int sockfd;
    int id;
    uint32_t pid;
    char ip[NI_MAXHOST];
    uintptr_t ack_bit_p;
};

struct replicate_loghdr_arg
{
    int libfs_id;
    uint32_t n_hdrs; // log headers replicated.
};

struct build_and_send_loghdrs_arg
{
    int libfs_id;
    uint64_t cur;
    uint32_t ret;
};

struct persist_log_arg
{
	int libfs_id;
	uint64_t seqn;
	addr_t log_area_begin_blknr;
	addr_t log_area_end_blknr;
	addr_t start_blknr;
	uint64_t n_log_blks;
	uintptr_t fsync_ack_addr;
};

static inline struct inode *icache_find(uint32_t inum)
{
	struct inode *inode;

	pthread_spin_lock(&icache_spinlock);

	HASH_FIND(hash_handle, inode_hash, &inum,
        		sizeof(uint32_t), inode);

	pthread_spin_unlock(&icache_spinlock);

	return inode;
}

static inline struct inode *icache_alloc_add(uint32_t inum)
{
	struct inode *inode;

#ifdef __cplusplus
	inode = static_cast<struct inode *>(mlfs_zalloc(sizeof(*inode)));
#else
	inode = mlfs_zalloc(sizeof(*inode));
#endif

	if (!inode)
		panic("Fail to allocate inode\n");

	inode->inum = inum;
	inode->i_ref = 1;
	inode->flags = 0;
	inode->i_dirty_dblock = RB_ROOT;
	inode->_dinode = (struct dinode *)inode;

	//FIXME: LOCKO
	//pthread_rwlockattr_t rwlattr;
	//pthread_rwlockattr_setpshared(&rwlattr, PTHREAD_PROCESS_SHARED);
	//pthread_rwlock_init(&inode->de_cache_rwlock, &rwlattr);

	inode->i_sb = sb;

	pthread_mutex_init(&inode->i_mutex, NULL);
	pthread_spin_init(&inode->truncate_lock, PTHREAD_PROCESS_SHARED);

	INIT_LIST_HEAD(&inode->i_slru_head);
	
	pthread_spin_lock(&icache_spinlock);

	HASH_ADD(hash_handle, inode_hash, inum,
	 		sizeof(uint32_t), inode);

	pthread_spin_unlock(&icache_spinlock);

	return inode;
}

// Not used.
static inline struct inode *icache_add(struct inode *inode)
{
	uint32_t inum = inode->inum;

	pthread_mutex_init(&inode->i_mutex, NULL);
	pthread_spin_init(&inode->truncate_lock, PTHREAD_PROCESS_SHARED);
	
	pthread_spin_lock(&icache_spinlock);

	HASH_ADD(hash_handle, inode_hash, inum,
	 		sizeof(uint32_t), inode);

	pthread_spin_unlock(&icache_spinlock);

	return inode;
}

static inline int icache_del(struct inode *ip)
{
	pthread_spin_lock(&icache_spinlock);

	HASH_DELETE(hash_handle, inode_hash, ip);

	pthread_spin_unlock(&icache_spinlock);

	return 0;
}

//forward declaration
struct fs_stat;

//APIs
#ifdef USE_SLAB
void mlfs_slab_init(uint64_t pool_size);
#endif
#if NIC_OFFLOAD
void set_dram_root_inode(struct dinode *dip);
#endif
void read_superblock(uint8_t dev);
void read_root_inode();
int read_ondisk_inode(uint32_t inum, struct dinode *dip);
int write_ondisk_inode(struct inode *ip);
#ifdef DISTRIBUTED
void signal_callback(struct app_context *msg);
void low_lat_signal_callback(struct app_context *msg);
void persist_replicated_logs(int dev, addr_t n_log_blk);
void update_remote_ondisk_inode(uint8_t node_id, struct inode *ip);
#endif
struct inode* ialloc(uint8_t, uint32_t);
struct inode* idup(struct inode*);
void cache_init(uint8_t dev);
void ilock(struct inode*);
void iput(struct inode*);
void iunlock(struct inode*);
void iunlockput(struct inode*);
void iupdate(struct inode*);
struct inode* namei(char*);
struct inode* nameiparent(char*, char*);
addr_t readi(struct inode*, char*, offset_t, addr_t);
void stati(struct inode*, struct fs_stat*);
int bmap(uint8_t mode, struct inode *ip, offset_t offset, addr_t *block_no);
void itrunc(struct inode*);
struct inode* iget(uint32_t inum);
int mlfs_mark_inode_dirty(int id, struct inode *inode);
int persist_dirty_dirent_block(struct inode *inode);
int persist_dirty_object(void);
void persist_log(addr_t start_blk, addr_t n_log_blk);
#ifdef NIC_SIDE
uint64_t read_log(int libfs_id, uintptr_t local_base_addr, uintptr_t remote_base_addr,
        addr_t start_blknr, addr_t end_blknr, int sock_fd);
static uint32_t build_loghdr_list(int libfs_id, char *log, uint64_t log_size);
static void handle_replicate_request(void *arg);
static void do_replicate(void *arg);
#endif
static void print_memcpy_meta (memcpy_meta_t *meta, uint64_t i);

static void handle_replicate_msg(void *arg);
static void persist_log_worker(void *arg);

int reserve_log(struct peer_id *peer);
void show_storage_stats(void);
void print_all_thpool_profile_results(void);

//APIs for debugging.
uint32_t dbg_get_iblkno(uint32_t inum);
void dbg_dump_inode(uint8_t dev, uint32_t inum);
void dbg_check_inode(void *data);
void dbg_check_dir(void *data);
void dbg_dump_dir(uint8_t dev, uint32_t inum);
struct inode* dbg_dir_lookup(struct inode *dir_inode,
		char *name, uint32_t *poff);
void dbg_path_walk(char *path);

extern uint8_t g_ssd_dev;
extern uint8_t g_log_dev;
extern uint8_t g_hdd_dev;

#if MLFS_LEASE

#define LPB           (g_block_size_bytes / sizeof(mlfs_lease_t))

// Block containing inode i
static inline addr_t get_lease_block(uint8_t dev, uint32_t inum)
{
	return (inum / LPB) + disk_sb[dev].lease_start;
}

#endif

// Block containing inode i
static inline addr_t get_inode_block(uint8_t dev, uint32_t inum)
{
	return (inum / IPB) + disk_sb[dev].inode_start;
}

// Bitmap bits per block
#define BPB           (g_block_size_bytes*8)

// Block of free map containing bit for block b
#define BBLOCK(b, disk_sb) (b/BPB + disk_sb.bmap_start)

#ifdef __cplusplus
}
#endif

#endif
#endif /* __x86_64__ */
