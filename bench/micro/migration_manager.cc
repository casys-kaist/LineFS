#include "migration_manager.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "utlist.h"

#include "roaring.hh"
#include "roaring.c"

/********************************************************

            BLOCK METHODS

/********************************************************/



/********************************************************

            BLOCK BATCH METHODS

/********************************************************/


/********************************************************

            CACHE DEVICE METHODS

/********************************************************/

cache_device::cache_device(const char* filepath, uint64_t max_size)
{  
    //open file and truncate to max size
    this->fd = open(filepath, O_RDWR | O_CREAT, 0666);
    ftruncate(this->fd, max_size);

    //printf("%llu   %llu   %llu\n", max_size, block_size, max_size / (uint64_t)block_size);
    this->slots = max_size / (uint64_t)block_size;
    uint64_t x = (this->slots*sizeof(block))/mb;
    this->hash = NULL;
    this->blocks = new block[this->slots];

    this->lru_head = NULL;
    uint32_t* in = (uint32_t*) malloc(this->slots*sizeof(uint32_t));
    for (int i = 0 ; i < this->slots ; i++, in++) {
        *in = i; 
    }

    //init bitmap to all slots value set
    this->bitmap = new Roaring ();

    for (int i = 0 ; i < this->slots ; i++) {
        this->bitmap->add (i);
    }
    this->nbatches = this->slots/BLOCKS_PER_BATCH;
    this->batches = new block_batch[this->nbatches];

    for (int i = 0 ; i < this->nbatches ; i++) {
        block_batch* bb = this->batches + i;
        CDL_PREPEND (this->lru_head, bb);
    }
}

block* 
cache_device::get_block(uint32_t file_offset)
{
    block* b;
    HASH_FIND_INT(this->hash, &file_offset, b);

    return b;
}

uint64_t 
cache_device::offset_from_addr(block* cache_block)
{
    return (((char*)cache_block - (char*)this->blocks)/sizeof(block))*block_size;
}

block_batch* 
cache_device::batch_from_addr(block* cache_block)
{
    uint32_t offset = ((char*)cache_block - (char*)this->blocks) 
                        / (BLOCKS_PER_BATCH*sizeof(block));
    return this->batches + offset;
}

uint32_t 
cache_device::first_block_from_addr(block_batch* bb)
{
    uint32_t offset = 
        ((char*)bb - (char*)this->batches) / sizeof(block_batch);
    return offset;
}

void 
cache_device::write_block(block* cache_block, const void* buf)
{
    uint64_t offset = this->offset_from_addr(cache_block);
    lseek(this->fd, offset, SEEK_SET);
    write(this->fd, buf, block_size);

    //dbg ("Wrote to file at block %lld\n", offset/block_size);

    this->touch_lru(cache_block);
}

void 
cache_device::read_block(block* cache_block, void* buf)
{
    uint64_t offset = this->offset_from_addr(cache_block);
    lseek(this->fd, offset, SEEK_SET);
    read(this->fd, buf, block_size);

    //dbg ("Read file at block %lld\n", offset/block_size);

    this->touch_lru(cache_block);
}

bool 
cache_device::is_full()
{
    return (this->bitmap->cardinality() == 0);
}

void 
cache_device::touch_lru(block* cache_block)
{
    block_batch* bb = batch_from_addr(cache_block);
    CDL_DELETE(this->lru_head, bb);
    CDL_PREPEND(this->lru_head, bb);
}

void 
cache_device::migrate_to_file(migration_manager* mm)
{
    dbg("I'm full, so I'll migrate down to device\n", 1);
    
    block_batch* bb = this->lru_head->prev;
    uint32_t source_block_offset = this->first_block_from_addr (bb);
    block* source = this->blocks + source_block_offset;

    char* buf = new char[block_size];

    for (int i = 0 ; i < BLOCKS_PER_BATCH ; i++) {
        //read data from source block
        this->read_block(source, buf);
        //write to disk
        mm->write_to_file(source->file_offset, buf);
        
        //mark our slot as empty
        this->bitmap->add (source_block_offset);

        //update hash
        HASH_DEL (this->hash, source);

        //step
        ++source_block_offset;
        ++source;
    }

    delete buf;
}

void 
cache_device::migrate_to_cache_dev(cache_device* cd)
{
    dbg("GET READY FOR SOME MIGRATION\n", 1);
    block_batch* bb = this->lru_head->prev;
    uint32_t source_block_offset = this->first_block_from_addr (bb);
    block* source = this->blocks + source_block_offset;

    uint32_t slot_next_dev = cd->bitmap->minimum ();
    block* target = cd->blocks + slot_next_dev;

    char* buf = new char[block_size];

    for (int i = 0 ; i < BLOCKS_PER_BATCH ; i++) {
        //read data from source block
        this->read_block(source, buf);
        //write to target
        cd->write_block(target, buf);
    
        //mark our slot as empty
        this->bitmap->add (source_block_offset);
        
        //mark next dev's as used
        cd->bitmap->remove (slot_next_dev);

        //update hashes
        HASH_DEL (this->hash, source);
        //move on file offset
        target->file_offset = source->file_offset;
        HASH_ADD_INT (cd->hash, file_offset, target);

        //step
        ++source_block_offset;
        ++slot_next_dev;
        ++source;
        ++target;
    }

    delete buf;
}

void 
cache_device::put_block(uint32_t file_block, const void* buf)
{
    //assumption: we have space
    uint32_t slot = this->bitmap->minimum ();
    dbg("put block bitmap now has %llu slots\n", this->bitmap->cardinality());

    //dbg("Putting block at slot %d\n", slot);

    this->bitmap->remove (slot);

    dbg("bitmap now has %llu slots\n", this->bitmap->cardinality());

    block* cache_block = this->blocks + slot;
    cache_block->file_offset = file_block;

    //write data
    this->write_block (cache_block, buf);
    //add to hash
    HASH_ADD_INT (this->hash, file_offset, cache_block);
    //touch LRU
    touch_lru (cache_block);
}



/********************************************************

            MIGRATION MANAGER METHODS

/********************************************************/

migration_manager::migration_manager(const char* filepath, int flags)
{
    dbg("Path is %s\n", filepath);
    this->file_fd = open(filepath, flags, 0666);
    this->seek_ptr = 0;

    this->cds[0] = new cache_device(dev0_path, dev0_size);
}

void 
migration_manager::read_from_file(uint32_t file_block, void* buf)
{
    uint64_t seek = file_block*block_size;
    lseek(this->file_fd, seek, SEEK_SET);
    read(this->file_fd, buf, block_size);
}

void 
migration_manager::write_to_file(uint32_t file_block, void* buf)
{
    uint64_t seek = file_block*block_size;

    //dbg("Writing to file, block %u\n", file_block);

    uint64_t sz = lseek (this->file_fd, 0, SEEK_END);
    if (seek + block_size > sz)
        ftruncate(this->file_fd, seek+block_size);

    lseek (this->file_fd, seek, SEEK_SET);
    write (this->file_fd, buf, block_size);
}

uint32_t
migration_manager::get_block_from_offset()
{
    //printf("%u  %u  %u\n", this->seek_ptr, block_size, this->seek_ptr/block_size);
    return this->seek_ptr/block_size;
}

ssize_t 
migration_manager::_write(const void *buf, size_t count)
{
    //dbg("In write, seek %u, count %u\n",this->seek_ptr, count);
    //split in 4kb pieces
    for (uint32_t buf_offset = 0 ; 
            buf_offset < count ; 
            buf_offset += block_size, this->seek_ptr += block_size) {
    
        uint32_t file_block = this->get_block_from_offset();

        //check if cached
        block* cache_block = this->cds[0]->get_block(file_block);
        if (cache_block) {
            dbg("Block %u is cached at dev 0\n", file_block);
            this->cds[0]->write_block(cache_block, buf+buf_offset);
            continue;
        }
        
        //block is not cached, we need to write to nvm
        
        //cd0 is not full, write to it and move on
        if (!cds[0]->is_full()) {
            cds[0]->put_block(file_block, buf+buf_offset);
            continue;
        }
        dbg("It's full\n",0);

        //move one batch from 1 to file
        cds[0]->migrate_to_file(this);

        //now theres also space on cd0, write
        cds[0]->put_block(file_block, buf+buf_offset);
    }
    return count;
}


ssize_t 
migration_manager::_read(void *buf, size_t count)
{
    //split in 4kb pieces
    for (uint32_t buf_offset = 0 ; 
            buf_offset < count ; 
            buf_offset += block_size, this->seek_ptr += 4) {
    
        uint32_t file_block = get_block_from_offset ();

        //check if cached
        block* cache_block = cds[0]->get_block (file_block);
        if (cache_block) {
            (this->cds[0])->read_block (cache_block, buf+buf_offset);
            continue;
        }

        //not cached, read from file
        read_from_file (file_block, buf+buf_offset);
    }
}   


off_t 
migration_manager::_lseek(off_t offset, int whence)
{
    if (whence == SEEK_SET)
        this->seek_ptr = offset;
    else if (whence == SEEK_CUR)
        this->seek_ptr += offset;
    else if (whence == SEEK_END) {
        printf("seek END NIY\n");
        exit(1);
    }
    return this->seek_ptr;
}


ssize_t 
migration_manager::_fsync()
{
    block* b;
    char* buf = new char[block_size];

    dbg("fsync'ing device 0\n", 0);
    for(b = this->cds[0]->hash ; b != NULL ; b = (block*) b->hh.next) {
        this->cds[0]->read_block(b, buf);
        this->write_to_file(b->file_offset, buf);
    }
}

int 
migration_manager::_close()
{
    
}
