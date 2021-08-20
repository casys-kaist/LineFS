#include <sys/mman.h>
#include "extents_bh.h"

static int fs_bh_alloc = 0;
static int fs_bh_freed = 0;

void fs_start_trans(struct super_block *sb) { UNUSED(sb); }

void fs_stop_trans(struct super_block *sb) { UNUSED(sb); }

struct buffer_head *fs_bread(uint8_t dev, mlfs_fsblk_t block,
		int *ret)
{
	int err = 0;
	struct buffer_head *bh;

	// FIXME: This is scalability bottlneck.
	bh = sb_getblk(dev, block);
	if (!bh)
		return NULL;

	if (buffer_uptodate(bh)) 
		goto out;

	err = bh_submit_read_sync_IO(bh);
	if (bh->b_dev == g_ssd_dev)
		mlfs_io_wait(g_ssd_dev, 1);

	if (ret)
		*ret = err;

	fs_bh_alloc++;

out:
	return bh;
}

struct buffer_head *fs_get_bh(uint8_t dev, mlfs_fsblk_t block,
		int *ret)
{
	int err = 0;
	struct buffer_head *bh;
	bh = sb_getblk(dev, block);
	if (ret)
		*ret = err;

	if (bh)
		fs_bh_alloc++;

	return bh;
}

void fs_brelse(struct buffer_head *bh)
{
	fs_bh_freed++;
	brelse(bh);
}

void fs_mark_buffer_dirty(struct buffer_head *bh)
{
	mlfs_debug("buffer %lu is dirty\n", bh->b_blocknr);
	move_buffer_to_writeback(bh);
	set_buffer_uptodate(bh);
	set_buffer_dirty(bh);
}

void fs_bforget(struct buffer_head *bh)
{
	clear_buffer_uptodate(bh);
	clear_buffer_dirty(bh);
	fs_brelse(bh);
}

void fs_bh_showstat(void)
{
	printf("fs_bh_alloc: %d, fs_bh_freed: %d\n", fs_bh_alloc, fs_bh_freed);
}
