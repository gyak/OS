#include <stdio.h>
#include <stdlib.h>
#include "devices/block.h"
#include "threads/synch.h"
#include "filesys/inode.h"


struct buffer_cache_entry{
    bool valid;
    bool refer;
    bool dirty;
    block_sector_t disk_sector;
    uint8_t buffer[BLOCK_SECTOR_SIZE];//512*1B
    struct lock entry_lock;
};

void buffer_cache_init();
void buffer_cache_terminate();
void buffer_cache_read(block_sector_t, void*, off_t, int, int);
void buffer_cache_write(block_sector_t, void*, off_t, int, int);
struct buffer_cache_entry* buffer_cache_lookup(block_sector_t);
struct buffer_cache_entry* buffer_cache_select_victim();
void buffer_cache_flush_entry(struct buffer_cache_entry*);
void buffer_cache_flush_all();