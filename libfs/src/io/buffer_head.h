#ifndef _BUFFER_HEAD_H_
#define _BUFFER_HEAD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ds/list.h"
#include "ds/bitmap.h"
#include "mlfs/kerncompat.h"
#include "ds/rbtree.h"

#define READ 0
#define WRITE 1

#define might_sleep()
enum bh_state_bits {
	BH_Uptodate,      /* Contains valid data */
	BH_Dirty,	 /* Is dirty */
	BH_Verified,	 /* Is verified */
	BH_Lock,	  /* Is locked */
	BH_Req,		  /* Has been submitted for I/O */
	BH_Uptodate_Lock, /* Used by the first bh in a page, to serialise
			   * IO completion of other buffers in the page
			   */
	BH_Mapped,		/* Has a disk mapping */
	BH_New,		     /* Disk mapping was newly created by get_block */
	BH_Sync_Read,       
	BH_Sync_Write,      
	BH_Async_Read,       /* Is under end_buffer_async_read I/O */
	BH_Async_Write,      /* Is under end_buffer_async_write I/O */
	BH_Delay,	    /* Buffer is not yet allocated on disk */
	BH_Boundary,	 /* Block is followed by a discontiguity */
	BH_Write_EIO,	/* I/O error on write */
	//BH_Unwritten,	/* Buffer is allocated on disk but not written */
	//BH_Quiet,	    /* Buffer Error Prinks to be quiet */
	BH_Meta,	     /* Buffer contains metadata */
	//BH_Prio,	     /* Buffer should be submitted with REQ_PRIO */
	//BH_Defer_Completion, /* Defer AIO completion to workqueue */
	BH_PrivateStart,     /* not a state bit, but the first bit available
	      * for private allocation by other entities
	      */
	BH_Ordered,
	BH_Pin,				/* Do not evict this buffer */
	BH_DataRef,			/* b_data has data reference */
	BH_Eopnotsupp
};

struct super_block;
struct buffer_head;

typedef void(bh_end_io_t)(struct buffer_head *, int);

/*
 * Historically, a buffer_head was used to map a single block
 * within a page, and of course as the unit of I/O through the
 * filesystem and block layers.  Nowadays the basic I/O unit
 * is the bio, and buffer_heads are used for extracting block
 * mappings (via a get_block_t call), for tracking state within
 * a page (via a page_mapping) and for wrapping bio submission
 * for backward compatibility reasons (e.g. submit_bh).
 */
typedef int atomic_t;

struct buffer_head
{
	uint8_t b_dev;
	unsigned long b_state; /* buffer state bitmap (see above) */

	addr_t b_blocknr;	/* start block number */
	uint32_t b_size;	/* size of mapping */
	uint32_t b_offset;	/* offset in a block (used for unaligned write) */
	uint8_t *b_data;       /* pointer to data within the page */

	struct block_device *b_bdev;
	bh_end_io_t *b_end_io; /* I/O completion */

	atomic_t b_count; /* users using this buffer_head */
	pthread_mutex_t b_lock;
	//pthread_spinlock_t b_spinlock;

	pthread_mutex_t b_wait_mutex;

	struct list_head b_io_list;
	struct list_head b_dirty_list;
	struct list_head b_freelist;

	/* FIXME: hash table? */
	struct rb_node b_rb_node;

	mlfs_hash_t hash_handle;
};

typedef struct buffer_head  bh_t;

/*
 * macro tricks to expand the set_buffer_foo(), clear_buffer_foo()
 * and buffer_foo() functions.
 */
#define BUFFER_FNS(bit, name)                                                  \
	static inline void set_buffer_##name(struct buffer_head *bh)           \
	{                                                                      \
		__set_bit(BH_##bit, &(bh)->b_state);                           \
	}                                                                      \
	static inline void clear_buffer_##name(struct buffer_head *bh)         \
	{                                                                      \
		__clear_bit(BH_##bit, &(bh)->b_state);                         \
	}                                                                      \
	static inline int buffer_##name(const struct buffer_head *bh)          \
	{                                                                      \
		return test_bit(BH_##bit, &(bh)->b_state);                     \
	}

/*
 * test_set_buffer_foo() and test_clear_buffer_foo()
 */
#define TAS_BUFFER_FNS(bit, name)                                              \
	static inline int test_set_buffer_##name(struct buffer_head *bh)       \
	{                                                                      \
		return __test_and_set_bit(BH_##bit, &(bh)->b_state);           \
	}                                                                      \
	static inline int test_clear_buffer_##name(struct buffer_head *bh)     \
	{                                                                      \
		return __test_and_clear_bit(BH_##bit, &(bh)->b_state);         \
	}

/*
 * Emit the buffer bitops functions.   Note that there are also functions
 * of the form "mark_buffer_foo()".  These are higher-level functions which
 * do something in addition to setting a b_state bit.
 */
BUFFER_FNS(Uptodate, uptodate)
BUFFER_FNS(Verified, verified)
BUFFER_FNS(Dirty, dirty)
TAS_BUFFER_FNS(Dirty, dirty)
BUFFER_FNS(Lock, locked)
TAS_BUFFER_FNS(Lock, locked)
BUFFER_FNS(Req, req)
TAS_BUFFER_FNS(Req, req)
BUFFER_FNS(Mapped, mapped)
BUFFER_FNS(Meta, meta)
BUFFER_FNS(Sync_Read, sync_read)
BUFFER_FNS(Sync_Write, sync_write)
BUFFER_FNS(Async_Read, async_read)
BUFFER_FNS(Async_Write, async_write)
TAS_BUFFER_FNS(Async_Write, async_write)
BUFFER_FNS(Delay, delay)
BUFFER_FNS(Boundary, boundary)
BUFFER_FNS(Write_EIO, write_io_error)
BUFFER_FNS(Ordered, ordered)
BUFFER_FNS(Eopnotsupp, eopnotsupp)
BUFFER_FNS(Pin, pin)
BUFFER_FNS(DataRef, data_ref)

#if 0
BUFFER_FNS(Unwritten, unwritten)
BUFFER_FNS(New, new)
BUFFER_FNS(Prio, prio)
#endif

#ifdef __cplusplus
}
#endif

#endif
