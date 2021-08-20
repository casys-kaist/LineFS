#ifndef _EXTENTS_BH_H
#define _EXTENTS_BH_H

#include "mlfs/kerncompat.h"
#include "io/block_io.h"
#include "extents.h"

#ifdef __cplusplus
extern "C" {
#endif

void fs_start_trans(struct super_block *sb);
void fs_stop_trans(struct super_block *sb);

struct buffer_head *fs_bread(uint8_t dev_id, mlfs_fsblk_t block, int *ret);
void fs_brelse(struct buffer_head *bh);
struct buffer_head *fs_get_bh(uint8_t dev_id, mlfs_fsblk_t block, int *ret);

void fs_mark_buffer_dirty(struct buffer_head *bh);
void fs_bforget(struct buffer_head *bh);
void fs_bh_showstat(void);

#ifdef __cplusplus
}
#endif

#endif
