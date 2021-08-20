#ifndef _INODE_H_
#define _INODE_H_

#include "global/types.h"
#include "global/global.h"
#include "ds/list.h"
#include "ds/rbtree.h"
#include "ds/bitmap.h"
#include "ds/khash.h"
#include "ds/stdatomic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RANGENODE_PER_PAGE  254

//#ifdef DISTRIBUTED
struct mlfs_reply {
	uint8_t *dst;
	int remote;
	int sockfd;
	uint64_t seqn;
};
//#endif

struct mlfs_interval {
	uint32_t start;
	uint32_t size;
};

struct mlfs_range_node {
	struct rb_node node;
	unsigned long range_low;
	unsigned long range_high;
};

struct free_list {
	uint32_t id;
	pthread_mutex_t mutex;
	struct rb_root  block_free_tree;
	struct mlfs_range_node *first_node;
	unsigned long   block_start;
	unsigned long   block_end;
	unsigned long   num_free_blocks;
	unsigned long   num_blocknode;

	/* Statistics */
	unsigned long   alloc_log_count;
	unsigned long   alloc_data_count;
	unsigned long   free_log_count;
	unsigned long   free_data_count;
	unsigned long   alloc_log_pages;
	unsigned long   alloc_data_pages;
	unsigned long   freed_log_pages;
	unsigned long   freed_data_pages;
};

// in-memory format of superblock
struct super_block
{
	struct disk_superblock *ondisk;
	struct block_device *s_bdev;
	struct block_bitmap *s_blk_bitmap;

	unsigned long *s_inode_bitmap;

	// have an RB tree allocated for each LibFS process
	struct rb_root s_dirty_root[MAX_LIBFS_PROCESSES * 2];
	uint64_t used_blocks;
	uint64_t last_block_allocated;

	struct free_list *free_lists;

	/* Shared free block list */
	unsigned long per_list_blocks;
	struct free_list shared_free_list;
	uint64_t num_blocks;
	uint32_t reserved_blocks;

	// Logical partition for block allocator.
	uint32_t n_partition;
};

// mkfs computes the super block and builds an initial file system.
// The superblock describes the disk layout:
struct disk_superblock {
	uint64_t size;			// Size of file system image (blocks)
	uint64_t ndatablocks;		// Number of data blocks
	uint32_t ninodes;			// Number of inodes.
	uint64_t nlog;			// Number of log blocks
	addr_t inode_start;		// Block number of first inode block
//#if MLFS_LEASE
	addr_t lease_start;
//#endif
	addr_t bmap_start;		// Block number of first free map block
	addr_t datablock_start;	// Block number of first data block
	addr_t log_start;		// Block number of first log block
};

#define L_TYPE_DIR_ADD         1
#define L_TYPE_DIR_RENAME      2
#define L_TYPE_DIR_DEL         3
#define L_TYPE_INODE_CREATE    4
#define L_TYPE_INODE_UPDATE    5
#define L_TYPE_FILE            6
#define L_TYPE_UNLINK          7
#define L_TYPE_ALLOC	       8

#define NTYPE_I 1
#define NTYPE_D 2
#define NTYPE_F 3
#define NTYPE_U 4
#define NTYPE_V 5

#define LH_COMMIT_MAGIC	0x1FB9


// On-disk metadata of log area
struct log_superblock {
	// block number of the first undigested logheader.
	addr_t start_digest;
	// # of loghdr to digest
	atomic_uint n_digest;
        // # of blocks to digest (Used to sync btw replication & digestion)
	atomic_uint n_digest_blks; // FIXME ulong?

	addr_t loghdr_expect_to_digest;

	// block number of the first unpersisted logheader.
	addr_t start_persist;

	// last log blk before wrap around
	atomic_ulong end;
};

// On-disk contents of logheader block.
/* Log header information of each FS operation.
 - append or overwrite: <type, metadata id, offset>.
 - delete: <type, metadata id>
 - non-zero ftruncate: <type, metadata id, offset>
 - rename: <type, metadata id>
 - create: <type, metadata id>
 - symlink: not supported
 */
/* ! Note that The size of logheader is critical.
 * The smaller size, the better performance */
typedef struct logheader {
	uint8_t n;
	uint8_t type[g_max_blocks_per_operation];
	uint32_t inode_no[g_max_blocks_per_operation];
	// opaque data.
	// file: offset.
	// directory: inode number of dirent.
	// inode, unlink: no used.
	offset_t data[g_max_blocks_per_operation];
	uint32_t length[g_max_blocks_per_operation];
	// relative block number of on-disk log data blocks (from header). 
	addr_t blocks[g_max_blocks_per_operation];
	// size of log transaction
	uint16_t nr_log_blocks;
	mlfs_time_t mtime;
	uint16_t inuse;
} loghdr_t;

typedef struct io_vec {
	uint8_t *base;
	uint32_t size;
} io_vec_t;

// In-memory contents of logheader block.
typedef struct logheader_meta {
	struct list_head link;
	// addr_t blocks[g_max_blocks_per_operation];
	struct logheader *loghdr;
	struct logheader *previous_loghdr;
	// flag whether io_buf is allocated or not
	uint8_t is_hdr_allocated;

	// flag whether hdr is off-limits for coalescing or not
	uint8_t is_hdr_locked;

	// block number of on-disk logheader.
	addr_t hdr_blkno;

	// block number of previous on-disk logheader.
	addr_t prev_hdr_blkno;

	uint32_t pos; // 0 <= pos <= nr_log_blocks
	// pre-allocated starting log blocks in atomic append.
	//addr_t log_blocks;

	uint32_t nr_iovec;
	// io vector for user buffer. used for log write.
	io_vec_t io_vec[10];

	uint32_t ext_used;
	// 2048 bytes that can be piggybacked to logheader.
	// used for dirent name and very small write.
	uint8_t loghdr_ext[2048];
} loghdr_meta_t;

/**
 * Args:
 *      current : current block number.
 *      size: number of log blocks this loghdr includes. (loghdr->nr_log_blocks)
 *      start: start block number of the device.
 *      end: end block number of the device.
 */
static inline addr_t next_loghdr_blknr(addr_t current, addr_t size, addr_t start, addr_t end)
{
	if(current + size - 1 == end)
		return start;
	else
		return current + size;
}

static inline int find_intersection(struct mlfs_interval *i1,
		struct mlfs_interval *i2, struct mlfs_interval *res)
{
	if(i2->start > (i1->start + i1->size - 1) ||
		       i1->start > (i2->start + i2->size - 1)) {
		return 0;
	}
	else {
		res->start = max(i1->start, i2->start);
		res->size = min((i1->start+i1->size), (i2->start+i2->size)) - res->start;
		return 1;
	}
}

//#ifndef KERNFS
//TODO: refactor and merge KERNFS replay structs with those defined here
typedef struct replay_node_key
{
	uint32_t inum;
	uint16_t ver;
} replay_key_t;

/* It is important to place node_type right after struct list_head
 * since digest_log_from_replay_list compute node_type like this:
 * node_type = &list + sizeof(struct list_head)
 */
typedef struct inode_replay {
	replay_key_t key;
	mlfs_hash_t hh;
	struct list_head list;
	uint8_t node_type;
	addr_t blknr;
	uint8_t create;
} i_replay_t;

typedef struct block_list {
	struct list_head list;
	uint32_t n;
	addr_t blknr;
} f_blklist_t;

typedef struct file_io_vector {
	offset_t hash_key;
	struct list_head list;
	uint8_t node_type;
	addr_t blknr;
	offset_t offset;
	uint32_t length;
	struct list_head iov_list;
	uint32_t n_list;
	struct list_head iov_blk_list;
	mlfs_hash_t hh;
} f_iovec_t;

typedef struct file_replay {
	replay_key_t key;
	struct list_head iovec_list;
	f_iovec_t *iovec_hash;
	mlfs_hash_t hh;
	struct list_head list;
	uint8_t node_type;
} f_replay_t;

typedef struct unlink_replay {
	replay_key_t key;
	mlfs_hash_t hh;
	struct list_head list;
	uint8_t node_type;
	addr_t blknr;
} u_replay_t;

typedef struct inode_version {
	uint32_t inum;
	mlfs_hash_t hh;
	uint16_t ver;
} id_version_t;

//#endif

struct replay_list {
	i_replay_t *i_digest_hash;
	f_replay_t *f_digest_hash;
	u_replay_t *u_digest_hash;
	id_version_t *id_version_hash; //used in lieu of inode_version_table
	struct list_head head;
};

// On-disk inode structure
struct dinode {
	uint8_t itype;		// File type
	uint8_t nlink;		// Number of links to inode in file system

#if 0
	uint8_t lstate;		// Lease state for inode (read, write, digest_to_acquire, free)
	uint8_t lholders;	// Number of LibFSes holding a lease (only read leases allow multiple holders)

	// only relevant for 'digest_to_acquire' lease state
	uint8_t lhid;		// LibFS ID for last lease holder
	uint8_t lmid;		// KernFS ID for lease manager
#endif

	uint64_t size;		// Size of file (bytes)

	mlfs_time_t atime;
	mlfs_time_t ctime;
	mlfs_time_t mtime;

	addr_t l1_addrs[NDIRECT+1];	//direct block addresses: 64 B
	addr_t l2_addrs[NDIRECT+1];	
	addr_t l3_addrs[NDIRECT+1];	
}; // 256 bytes.

#define setup_ondisk_inode(dip, type) \
	memset(dip, 0, sizeof(struct dinode)); \
	((struct dinode *)dip)->itype = type;

//inode flags
#define I_BUSY 0x1
#define I_VALID 0x2
#define I_DIRTY 0x4
#define I_DELETING 0x8
#define I_RESYNC 0x10

//inode valid flag for _dinode
#define DI_INVALID 0x0
#define DI_VALID 0x1

#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Device

//modes for bmap
#define BMAP_GET_NO_ALLOC 1
#define BMAP_GET 2
#define BMAP_SET 3

#define DIRBITMAP_SIZE 1024

#ifdef KLIB_HASH
KHASH_MAP_INIT_INT64(fcache, struct fcache_block *);
KHASH_MAP_INIT_STR(de_cache, struct dirent_data *);
#endif

// in-memory copy of an inode
struct inode {
	/////////////////////////////////////////////////////////////////
	uint8_t itype;      // File type
	uint8_t nlink;      // Number of links to inode in file system

#if 0
	uint8_t lstate;		// Lease state for inode (free, read, write)
	uint8_t lholders;	// Number of LibFSes holding a lease (only read leases allow multiple holders)

	// only relevant for 'write' lease state to decide if digestions are necessary before a transfer
	uint8_t lhid;		// LibFS ID for last lease holder
	uint8_t lmid;		// KernFS ID for lease manager
#endif

	uint64_t size;      // Size of file (bytes)

	mlfs_time_t atime;
	mlfs_time_t ctime;
	mlfs_time_t mtime;

	union {
		/* the first 12 bytes of i_data is root extent_header */
		uint32_t i_data[15]; 
		uint32_t i_block[15];

		addr_t addrs[NDIRECT+1];    // Data block addresses
	} l1;

	union {
		/* the first 12 bytes of i_data is root extent_header */
		uint32_t i_data[15]; 
		uint32_t i_block[15];

		addr_t addrs[NDIRECT+1];    // Data block addresses
	} l2;

	union {
		/* the first 12 bytes of i_data is root extent_header */
		uint32_t i_data[15]; 
		uint32_t i_block[15];

		addr_t addrs[NDIRECT+1];    // Data block addresses
	} l3;

	// This must be identical to struct dinode
	// When in-memory inode is initialized, the contents are synced with dinode.
	/////////////////////////////////////////////////////////////////

	// This contains up-to-date dinode information
	struct dinode *_dinode;

	struct super_block **i_sb;
	uint8_t dinode_flags; //flag to see whether dinode is loaded or not
	uint32_t inum;      // Inode number
	int i_ref;          // Reference count
	uint8_t flags;      // I_BUSY, I_VALID

	uint8_t i_uuid[16]; // For compability only.
	uint32_t i_generation;
	uint32_t i_csum;
	uint8_t	 i_data_dirty;

	mlfs_hash_t hash_handle;

        // Sizes of pthread_mutex_t are different in Host(40) and SmartNIC(48).
        // Add padding to make them have the same size.
        // The size should be the same when calling mlfs_ext_get_blocks() from
        // LibFS which sends RPC to NIC kernfs.
        // TODO check whether this data structure is accessed during mlfs_ext_get_block()
        // execution on NIC kernfs.
#ifndef NIC_SIDE
        char pthread_mutex_pad[8];
#endif
        pthread_mutex_t i_mutex;
	pthread_spinlock_t truncate_lock;

	// For extent tree search optimization.
	struct mlfs_ext_path *previous_path;
	uint8_t invalidate_path;

        pthread_rwlock_t de_cache_rwlock;
	// per-directory hash, mapping name to inode.
	struct dirent_data *de_cache;
#ifdef KLIB_HASH    // Only defined in LibFS.
	khash_t(de_cache) *de_cache_hash;
#else
        // To make struct inode compatible between kernfs and libfs,
        // declare an unused pointer with the same size of de_cache_hash.
        uint8_t* de_cache_hash;
#endif
	uint32_t n_de_cache_entry;

	// kernfs only
	struct rb_node i_rb_node;      // rb node link for s_dirty_root.
	struct rb_root i_dirty_dblock; // rb root for dirty directory block.
	struct list_head i_slru_head;
	//cuckoofilter_t *filter;
	///////////////////////////////////////////////////////////////////

	// libfs only
        pthread_rwlock_t fcache_rwlock;
	struct fcache_block *fcache;
#ifdef KLIB_HASH    // Only defined in LibFS.
	khash_t(fcache) *fcache_hash;
#else
        // To make struct inode compatible between kernfs and libfs,
        // declare an unused pointer with the same size of fcache_hash.
        uint8_t *fcache_hash;
#endif
	uint32_t n_fcache_entries;
	///////////////////////////////////////////////////////////////////

	// kernfs only
#if 0
	// stores libfses holding leases or dirty logs and/or dirty cached metadata
	DECLARE_BITMAP(libfs_ls_bitmap, MAX_LIBFS_PROCESSES);
	mlfs_time_t ltime;	// Time when lease was last acquired
	uint16_t lversion;	// Log version when exclusive lease was released
	uint64_t lblock;	// Log block position when exclusive lease was released
#endif
	///////////////////////////////////////////////////////////////////
	/* for testing */
	struct db_handle *i_db; 
	int (*i_writeback)(struct inode *inode);
	///////////////////////////////////////////////////////////////////
};

static inline struct super_block* get_inode_sb(uint8_t dev, struct inode *inode)
{
	return inode->i_sb[dev];
}

//typedef cuckoohash_map <offset_t, struct fcache_block *, CityHasher<offset_t>> fcache_hash_t;

#define sync_inode_from_dinode(__inode, __dinode) \
	memmove(__inode->_dinode, __dinode, sizeof(struct dinode)); \
	__inode->dinode_flags |= DI_VALID; \

#define SRV_SOCK_PATH "/tmp/digest_socket\0"

//#if defined(DISTRIBUTED) && defined(USE_LEASE)
//#define MAX_SIGNAL_BUF 1500
//#else
#define MAX_SIGNAL_BUF 1000
//#endif

#define MAX_SOCK_BUF 256
#define MAX_CMD_BUF 128

//maximum number of scatter gather elements per message for rdma device
#define MAX_RDMA_SGE 30

struct mlfs_dirent {
  uint32_t inum;
  char name[DIRSIZ];
};

extern uint8_t *shm_base;
#define LRU_HEADS (sizeof(struct list_head)) * 5
#define BLOOM_HEAD 

/* shared memory layout (bytes) for reserved region (the first 4 KB)
 *  0~ LRU_HEADS           : lru_heads region
 *  LRU_HEADS ~ BLOOM_HEAD : bloom filter for lsm tree search
 *  BLOOM_HEAD ~           : unused	
 */ 
struct list_head *lru_heads;

typedef struct lru_key {
	uint8_t dev;
	addr_t block;
} lru_key_t;

typedef struct lru_val {
	uint32_t inum;
	uint64_t lblock;
} lru_val_t;

typedef lru_val_t block_key_t;
typedef block_key_t core_val_t;

typedef struct block_val {
	block_key_t key;
	addr_t block;
} block_val_t;

static inline uint64_t hash1(block_key_t k)
{
	return XXH64(&k, sizeof(block_key_t), 0);
}

static inline uint64_t hash1i(uint64_t k)
{
	return k;
}

static inline uint64_t hash2(block_key_t k)
{
	return XXH64(&k, sizeof(block_key_t), 1);
}

/** Shared functions. **/
/**
 * Args:
 *      start_blknr: start block number.
 *      blk_cnt: count of blocks
 *      log_start_blknr: first block number of log.
 *      log_end_blknr: last block number of log.
 * Returns:
 *      end block number after wrap around.
 */
static inline uint64_t
wrap_arounded_blknr(addr_t start_blknr, addr_t blk_cnt,
        addr_t log_start_blknr, addr_t log_end_blknr)
{
    uint64_t end;
    uint64_t log_size;
    end = start_blknr + blk_cnt -1;
    log_size = log_end_blknr - log_start_blknr + 1;
    return end - log_size;
}

#ifdef __cplusplus
}
#endif

#endif
