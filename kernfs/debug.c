#include "io/block_io.h"

void dbg_ssd_dump(addr_t blockno) 
{
	struct buffer_head *bh;
	uint8_t tmp_buf[4096];
	bh = bh_get_sync_IO(g_ssd_dev, blockno, BH_NO_DATA_ALLOC);

	bh->b_data = tmp_buf;
	bh->b_size = g_block_size_bytes;
	bh_submit_read_sync_IO(bh);
	mlfs_io_wait(bh->b_dev, 1);

	GDB_TRAP;

	bh_release(bh);
}

