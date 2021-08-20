#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "aligned_block_migration.h"
#include "uthash.h"
#include "utlist.h"
/*******************************************************
 *******************************************************
 *************** Variables below need setting **********
 *******************************************************
 *******************************************************/

static uint32_t block_size = 4*kb;

static char* dev_paths[] = {
    "./tmp0",
    "./tmp1",
    "./tmp2",
};

static uint64_t dev_sizes[] = {
    8*kb, 
    8*kb,
    32*mb,
};


/*******************************************************
 *******************************************************
 *******************************************************
 *******************************************************
 *******************************************************/

static uint32_t block_slots[3];

typedef struct block_data {
    //LRU list ptrs
    struct block_data *prev, *next;
    //offset from start of db file 
    uint32_t block_offset_on_file;
    UT_hash_handle hh;
} block_t;

/*
 *  STATIC variables, to maintain state
 */

static char bm_inited = 0;

static struct bm_state_t {
    block_t* block_hash[2];

    block_t* lru[2];
    block_t* blocks_data[2];
    
    uint32_t free_blocks[2];
    uint64_t seek_ptr;
    
    int fds[3];
} bm_state;


/*
 *  Helper functions
 */
static void bm_setup(void)
{
    if (bm_inited) return;
    
    bm_inited = 1;

    memset(&bm_state, 0, sizeof(struct bm_state_t));

    bm_state.free_blocks[0] = dev_sizes[0]/block_size;
    bm_state.free_blocks[1] = dev_sizes[1]/block_size;

    //create and initialize all the stuff we need
    //meta data for each cache block
    bm_state.blocks_data[0] = malloc (bm_state.free_blocks[0] * sizeof(block_t));
    bm_state.blocks_data[1] = malloc (bm_state.free_blocks[1] * sizeof(block_t));

    //open db file
    //bm_state.fds[0] = open(dev_paths[0], O_RDWR | O_CREAT | O_TRUNC, 0666);
    //bm_state.fds[1] = open(dev_paths[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
    //bm_state.fds[2] = open(dev_paths[2], O_RDWR | O_CREAT | O_TRUNC, 0666);

    bm_state.fds[0] = open(dev_paths[0],  O_CREAT | O_RDWR);
    bm_state.fds[1] = open(dev_paths[1],  O_CREAT | O_RDWR);
    bm_state.fds[2] = open(dev_paths[2],  O_CREAT | O_RDWR);

    //make the cache take the whole sizes
    ftruncate(bm_state.fds[0], dev_sizes[0]);
    ftruncate(bm_state.fds[1], dev_sizes[1]);
}

static uint32_t block_from_offset(uint64_t offset) 
{
    return offset/block_size;
}

static uint64_t slot_from_offset(block_t* addr, uint8_t dev) 
{
    return ((void*)addr - (void*)bm_state.blocks_data[dev])/sizeof(block_t);
}

static void bm_write_block_data_to_dev(block_t* blk, const void* buf, uint8_t device)
{
    uint64_t cache_seek = slot_from_offset(blk, device)*block_size;
    lseek(bm_state.fds[device], cache_seek, SEEK_SET);
    write(bm_state.fds[device], buf, block_size);

    dbg("Wrote data to dev %d, slot %d\n", device, slot_from_offset(blk, device));
}

static block_t* bm_write_device_with_space(uint32_t file_block, const void* buf, uint8_t device)
{
    uint32_t empty_slot = (dev_sizes[device]/block_size) 
        - bm_state.free_blocks[device];
    bm_state.free_blocks[device] -= 1;

    block_t* target_slot = bm_state.blocks_data[device] + empty_slot;
    target_slot->block_offset_on_file = file_block;

    //add to LRU
    CDL_PREPEND(bm_state.lru[device], target_slot);
    HASH_ADD_INT(bm_state.block_hash[device], block_offset_on_file, target_slot);

    //write data
    bm_write_block_data_to_dev(target_slot, buf, device);
}

static void bm_read_block_data(block_t* blk, void* buf, uint8_t device)
{
    uint64_t cache_seek = slot_from_offset(blk, device)*block_size;
    lseek(bm_state.fds[device], cache_seek, SEEK_SET);
    read(bm_state.fds[device], buf, block_size);

    dbg("Read data from dev %d, slot %d\n", device, slot_from_offset(blk, device));
}

static void touch_lru(block_t* blk, uint8_t dev)
{
    CDL_DELETE(bm_state.lru[dev], blk);
    CDL_PREPEND(bm_state.lru[dev], blk);
}   

/*
 * POSIX overloading, actual code
 */

int bm_open(const char *pathname, int flags, mode_t mode)
{
    bm_setup();
    
    //theres only one file, fake fd
    return 1;
}


ssize_t bm_write(int fd, const void *buf, size_t count) 
{
    dbg("Read %lld   %d\n", bm_state.seek_ptr, count);
    //ignore fd, we only have one file
    uint32_t buf_offset = 0;
    
    //file offset we will write
    uint64_t* offset = &bm_state.seek_ptr;
    //ugly, but necessary for the first line of the loop
    (*offset) -= block_size;

    //assuming bytes are aligned, and a multiple of block size 
    for ( ; buf_offset < count ; buf_offset += block_size) {
        //every round we write block_size
        (*offset) += block_size;

        //find at which block of the file we are writing
        uint32_t file_block = block_from_offset(*offset);
        dbg("Looking to WRITE for block %d\n", file_block);
    
        //check if cached on first layer
        block_t* elt;
        HASH_FIND_INT(bm_state.block_hash[0], &file_block, elt);
        //block is cached on first layer, overwrite and done
        if (elt != NULL) {
            dbg("Block is cached on first layer, done%s", "\n");
            bm_write_block_data_to_dev (elt, buf+buf_offset, 0);
            touch_lru (elt, 0);
            continue;
        }

        //check if cached on second layer
        HASH_FIND_INT(bm_state.block_hash[1], &file_block, elt);
        //block is cached on second layer, overwrite and done
        if (elt != NULL) {
            dbg("Block is cached on second layer, done%s", "\n");
            bm_write_block_data_to_dev(elt, buf+buf_offset, 1);
            touch_lru (elt, 1);
            continue;
        }
        dbg("Not cached anywhere%s", "\n");
        /*
         *  At this point we know its not cached, so we need to write to dev0
         */

        //so its not on first neither second layer.. is there space on first?
        //yes, device 0 is not full
        if (bm_state.free_blocks[0] > 0) {
            bm_write_device_with_space(file_block, buf+buf_offset, 0);
            dbg("Theres space on dev0, we now have %d slots left\n", bm_state.free_blocks[0]);
            continue;
        }

        //no, device 0 is full
        
        /*
         *   MIGRATION FROM 0 to 1
         */
        //is there space on dev1?
        
        //fetch from lru
        block_t* blk_0_to_1 = bm_state.lru[0]->prev;
        HASH_DEL(bm_state.block_hash[0], blk_0_to_1);
        CDL_DELETE(bm_state.lru[0], blk_0_to_1);

        //yes, device 1 is not full
        if (bm_state.free_blocks[1] > 0) {
            //move from 0 to 1
            dbg("Dev0 is full, but dev1 isnt, we migrate slot %d down\n", 
                slot_from_offset(blk_0_to_1, 0));

            uint64_t cache_seek = slot_from_offset(blk_0_to_1, 0)*block_size;
            char rbuf[block_size];
            lseek(bm_state.fds[0], cache_seek, SEEK_SET);
            read(bm_state.fds[0], rbuf, block_size);
            bm_write_device_with_space(blk_0_to_1->block_offset_on_file, rbuf, 1);
        }

        //no, dev1 is full
        else {
            dbg("Both are full%s", "\n");
            /*
             *   MIGRATION FROM 1 to 2, dev0 and dev1 are full
             */
            block_t* blk_1_to_file = bm_state.lru[1]->prev;
            CDL_DELETE(bm_state.lru[1], blk_1_to_file);
            HASH_DEL(bm_state.block_hash[1], blk_1_to_file);

            //read from dev1
            uint64_t cache_seek = slot_from_offset(blk_1_to_file, 1)*block_size;
            char rbuf[block_size];
            lseek(bm_state.fds[1], cache_seek, SEEK_SET);
            read(bm_state.fds[1], rbuf, block_size);
            dbg("Start by moving slot %d down from 1 to file (dev 2)\n", 
                slot_from_offset(blk_1_to_file, 1));

            //check if file has enough space, if not truncate
            uint64_t file_seek = blk_1_to_file->block_offset_on_file * block_size;
            uint32_t sz = lseek(bm_state.fds[2], 0, SEEK_END);
            if (file_seek+block_size > sz) 
                ftruncate(bm_state.fds[2], file_seek+block_size);

            //seek and write to dev2
            lseek(bm_state.fds[2], file_seek, SEEK_SET);
            write(bm_state.fds[2], rbuf, block_size);
            dbg("Wrote to device. Block %d  Byte offset %lld\n", 
                blk_1_to_file->block_offset_on_file, file_seek);

            //now move from 0 to 1
            blk_1_to_file->block_offset_on_file = blk_0_to_1->block_offset_on_file;
            
            dbg("Moving from dev 0 (slot %d) to dev 1 (slot %d)\n",
                slot_from_offset(blk_0_to_1, 0), slot_from_offset(blk_1_to_file, 1));

            //read from dev0
            cache_seek = slot_from_offset(blk_0_to_1, 0)*block_size;
            lseek(bm_state.fds[0], cache_seek, SEEK_SET);
            read(bm_state.fds[0], rbuf, block_size);
            //write to dev1
            bm_write_block_data_to_dev(blk_1_to_file, rbuf, 1);
            CDL_PREPEND(bm_state.lru[1], blk_1_to_file);
            HASH_ADD_INT(bm_state.block_hash[1], block_offset_on_file, blk_1_to_file);
        }
        
        //finally write to dev0
        blk_0_to_1->block_offset_on_file = file_block;
        bm_write_block_data_to_dev(blk_0_to_1, buf+buf_offset, 0);
        CDL_PREPEND(bm_state.lru[0], blk_0_to_1);
        HASH_ADD_INT(bm_state.block_hash[0], block_offset_on_file, blk_0_to_1);
    }
    (*offset) += block_size;
    dbg("Write done!%s", "\n\n\n");
    return count;
}


ssize_t bm_read(int fd, void *buf, size_t count)
{
    block_t* elt;
    uint32_t buf_offset = 0;

    //file offset we will write
    uint64_t* offset = &bm_state.seek_ptr;
    //ugly, but necessary for the first line of the loop
    (*offset) -= block_size;

    for ( ; buf_offset < count ; buf_offset += block_size) {
        (*offset) += block_size;

        uint32_t file_block = block_from_offset(*offset);
        dbg("Looking to READ for block %d\n", file_block);

        //check if cached on first layer
        HASH_FIND_INT(bm_state.block_hash[0], &file_block, elt);
        
        //block is cached on first layer, overwrite and done
        if (elt != NULL) {
            dbg("Block is cached on first layer, done%s", "\n");
            bm_read_block_data (elt, buf+buf_offset, 0);
            touch_lru (elt, 0);
            continue;
        }

        //check if cached on second layer
        HASH_FIND_INT(bm_state.block_hash[1], &file_block, elt);
        
        //block is cached on second layer, overwrite and done
        if (elt != NULL) {
            dbg("Block is cached on second layer, done%s", "\n");
            bm_read_block_data (elt, buf+buf_offset, 0);
            touch_lru (elt, 1);
            continue;
        }

        dbg("Not cached, gotta go to disk\n", 2);
        
        uint32_t blk = block_from_offset(*offset);
        uint64_t file_seek = blk * block_size;
        lseek(bm_state.fds[2], file_seek, SEEK_SET);
        read(bm_state.fds[2], buf+buf_offset, block_size);
    }
    (*offset) += block_size;
}

ssize_t bm_pread(int fd, void *buf, size_t count, off_t offset)
{
    bm_lseek(0, offset, SEEK_SET);
    return bm_read(0, buf, count);
}

off_t bm_lseek(int fd, off_t offset, int whence)
{
    if (whence == SEEK_SET)
        bm_state.seek_ptr = offset;
    else if (whence == SEEK_CUR)
        bm_state.seek_ptr += offset;
    else if (whence == SEEK_END) {
        printf("seek END NIY\n");
        exit(1);
    }
    return bm_state.seek_ptr;
}

off_t bm_fsync(int fd)
{
    block_t* b;
    char rbuf[block_size];

    int dev;
    for (dev = 0 ; dev < 2 ; dev++) {
        for (b = bm_state.block_hash[dev] ; b != NULL; b = b->hh.next) {
            dbg("moving down\n", 2);

            //read
            uint64_t cache_seek = slot_from_offset(b, dev)*block_size;
            lseek(bm_state.fds[dev], cache_seek, SEEK_SET);
            read(bm_state.fds[dev], rbuf, block_size);

            //check if file has enough space, if not truncate
            uint64_t file_seek = b->block_offset_on_file * block_size;
            uint32_t sz = lseek(bm_state.fds[2], 0, SEEK_END);
            if (file_seek+block_size > sz) 
                ftruncate(bm_state.fds[2], file_seek+block_size);

            //seek and write to dev2
            lseek(bm_state.fds[2], file_seek, SEEK_SET);
            write(bm_state.fds[2], rbuf, block_size);
        }
    }
}

int bm_close(int fd)
{

}
