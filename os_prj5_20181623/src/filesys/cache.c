#include "cache.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "threads/synch.h"
#include "filesys/inode.h"
#include "filesys/filesys.h"


#define NUM_CACHE 64

static struct buffer_cache_entry cache[NUM_CACHE];
static struct lock buffer_cache_lock;
int clock;

void buffer_cache_init();
void buffer_cache_terminate();
void buffer_cache_read(block_sector_t, void*, off_t, int, int);
void buffer_cache_write(block_sector_t, void*, off_t, int, int);
struct buffer_cache_entry* buffer_cache_lookup(block_sector_t);
struct buffer_cache_entry* buffer_cache_select_victim();
void buffer_cache_flush_entry(struct buffer_cache_entry*);
void buffer_cache_flush_all();

//proj5
void buffer_cache_init()
{
    for(int i=0; i<NUM_CACHE; i++){
        memset(cache+i, 0, sizeof(struct buffer_cache_entry));
        lock_init(&cache[i].entry_lock);
    }
    lock_init(&buffer_cache_lock);
    clock = 0;
}

//proj5
void buffer_cache_terminate()
{
    for(int i=0; i<NUM_CACHE; i++){
        lock_acquire(&cache[i].entry_lock);
        if(&cache[i].dirty == true)
            buffer_cache_flush_entry(&cache[i]);
        lock_release(&cache[i].entry_lock);
    }
    clock = -1;
}

//proj5
void buffer_cache_read(block_sector_t sector_index, void* buffer, off_t offset, int chunk_size, int sector_ofs)
{
    struct buffer_cache_entry* target = buffer_cache_lookup(sector_index);
    if(target == NULL){
        target = buffer_cache_select_victim();
        lock_acquire(&target->entry_lock);
        if(target->dirty == true)
            buffer_cache_flush_entry(target);
        target->dirty = false;
        target->valid = true;
        target->refer = true;
        target->disk_sector = sector_index;
        block_read(fs_device, sector_index, target->buffer);
    }
    else{
        lock_acquire(&target->entry_lock);
        target->refer = true;
    }
    memcpy(buffer+offset, target->buffer+sector_ofs, chunk_size);
    lock_release(&target->entry_lock);
    lock_release(&buffer_cache_lock);
}

//proj5
void buffer_cache_write(block_sector_t sector_index, void* buffer, off_t offset, int chunk_size, int sector_ofs)
{
    struct buffer_cache_entry* target = buffer_cache_lookup(sector_index);
    if(target == NULL){
        target = buffer_cache_select_victim();
        lock_acquire(&target->entry_lock);
        if(target->dirty == true)
            buffer_cache_flush_entry(target);
        target->dirty = true;
        target->valid = true;
        target->refer = true;
        target->disk_sector = sector_index;
        block_read(fs_device, sector_index, target->buffer);
    }
    else{
        lock_acquire(&target->entry_lock);
        target->refer = true;
        target->dirty = true;
    }
    memcpy(target->buffer+sector_ofs, buffer+offset, chunk_size);
    lock_release(&target->entry_lock);
    lock_release(&buffer_cache_lock);
}

//proj5
struct buffer_cache_entry* buffer_cache_lookup(block_sector_t target)
{
    lock_acquire(&buffer_cache_lock);
    for(int i = 0; i<NUM_CACHE; i++){
        lock_acquire(&cache[i].entry_lock);
        if(cache[i].valid == true && cache[i].disk_sector == target){
            lock_release(&cache[i].entry_lock);
            return &cache[i];
        }
        lock_release(&cache[i].entry_lock);
    }
    return NULL;
}

//proj5
struct buffer_cache_entry* buffer_cache_select_victim()
{
    while(1){
        lock_acquire(&cache[clock].entry_lock);
        if(cache[clock].valid == false || cache[clock].refer == false){
            struct buffer_cache_entry* temp = &cache[clock];
            lock_release(&cache[clock].entry_lock);
            clock = (clock + 1) % NUM_CACHE;
            //if(++clock == NUM_CACHE)
            //    clock = 0;
            return temp;
        }
        cache[clock].refer = false;
        lock_release(&cache[clock].entry_lock);
        clock = (clock + 1) % NUM_CACHE;
        //if(++clock == NUM_CACHE)
        //    clock = 0;
    }
}

//proj5
void buffer_cache_flush_entry(struct buffer_cache_entry* target)
{
    block_write(fs_device, target->disk_sector, target->buffer);
    memset(target->buffer, 0, sizeof(uint8_t)*BLOCK_SECTOR_SIZE);
    target->dirty = false;
}