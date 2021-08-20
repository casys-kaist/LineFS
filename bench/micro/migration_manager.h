#include <inttypes.h>
#include "uthash.h"
#include "roaring.hh"

#define kb (uint32_t)1024
#define mb (uint32_t)1024*1024
#define gb (uint32_t)1024*1024*1024

//#define dev0_size 32 * 1024 *mb
const uint64_t dev0_size = 32 * 1024 *mb;
#define dev0_path "./pmem/cache"

const uint32_t block_size = 4*kb;
#define BLOCKS_PER_BATCH (uint16_t)256  //1mb

#define DEBUG 0

#define dbg(fmt, ...) \
            do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

struct block {
    uint32_t file_offset;
    UT_hash_handle hh;
};

struct block_batch {
    //pointers for LRU
    block_batch *prev, *next;
};

class migration_manager;

class cache_device
{
public:
    int fd;
    block* hash;
    block* blocks;

    block_batch* batches;
    block_batch* lru_head;

    uint32_t slots;
    uint32_t nbatches;
    
    Roaring* bitmap;

    cache_device(const char* filepath, uint64_t max_size);

    //check if block is in this device
    block* get_block(uint32_t file_offset);

    uint64_t offset_from_addr(block* cache_block);
    block_batch* batch_from_addr(block* cache_block);
    uint32_t first_block_from_addr(block_batch* bb);

    
    bool is_full();
    void touch_lru(block* cache_block);

    void write_block(block* cache_block, const void* buf);
    void read_block(block* cache_block, void* buf);
    void put_block(uint32_t file_block, const void* buf);

    void migrate_to_file(migration_manager* mm);
    void migrate_to_cache_dev(cache_device* cd);    
};


class migration_manager
{
public:
    uint64_t seek_ptr;
    cache_device* cds[1];
    int file_fd;


    int get_fd () {
        return this->file_fd;
    }

    migration_manager(const char* filepath, int flags);
    
    uint32_t get_block_from_offset();
        
    void read_from_file(uint32_t file_block, void* buf);
    void write_to_file(uint32_t file_block, void* buf);
    
    ssize_t _write(const void *buf, size_t count);
    ssize_t _read(void *buf, size_t count);
    off_t _lseek(off_t offset, int whence);
    ssize_t _fsync();
    int _close();
};
