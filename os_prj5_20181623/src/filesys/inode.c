#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"


#define INODE_MAGIC 0x494e4f44
#define DIRECT_BLOCK_ENTRIES 123
#define INDIRECT_BLOCK_ENTRIES 128
#define SECTOR_MAGIC 0xFFFFFFFF
/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
{
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    //proj5
    block_sector_t direct[DIRECT_BLOCK_ENTRIES];
    block_sector_t indirect;
    block_sector_t double_indirect;
    bool is_dir;
};

//proj5
struct sector_info{
    int direct_num;
    off_t fst_index;
    off_t snd_index;
};

struct inode_indirect_block{
    block_sector_t mapping[INDIRECT_BLOCK_ENTRIES];
};

/* In-memory inode. */
struct inode 
{
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct lock lock;
};
/* Identifies an inode. */

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
    static inline size_t
bytes_to_sectors (off_t size)
{
    return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}



/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */

//proj5
void compute_location(off_t pos, struct sector_info* sec_info)
{
    off_t pos_sector = pos/BLOCK_SECTOR_SIZE;
    if(pos_sector < DIRECT_BLOCK_ENTRIES){
        sec_info->direct_num = 0;
        sec_info->fst_index = pos_sector;
    }
    else if(pos_sector < DIRECT_BLOCK_ENTRIES + INDIRECT_BLOCK_ENTRIES){
        sec_info->direct_num = 1;
        sec_info->fst_index = pos_sector - DIRECT_BLOCK_ENTRIES;
    }
    else if(pos_sector < DIRECT_BLOCK_ENTRIES + INDIRECT_BLOCK_ENTRIES + INDIRECT_BLOCK_ENTRIES*INDIRECT_BLOCK_ENTRIES){
        sec_info->direct_num = 2;
        pos_sector = pos_sector - INDIRECT_BLOCK_ENTRIES - DIRECT_BLOCK_ENTRIES;
        sec_info->fst_index = pos_sector/INDIRECT_BLOCK_ENTRIES;
        sec_info->snd_index = pos_sector%INDIRECT_BLOCK_ENTRIES;
    }
    else
        sec_info->direct_num = -1;
}

//proj5
bool make_new_sector(struct inode_disk* inode_disk, block_sector_t new, struct sector_info sec_info)
{
    struct inode_indirect_block fst, snd;
    if(sec_info.direct_num == 0)
        inode_disk->direct[sec_info.fst_index] = new;

    else if(sec_info.direct_num == 1){
        if(inode_disk->indirect == SECTOR_MAGIC){
            if(free_map_allocate(1, &inode_disk->indirect) == false)
                return false;
            memset (&snd, -1, sizeof (struct inode_indirect_block));
        }
        else
            buffer_cache_read(inode_disk->indirect, &snd, 0, sizeof(struct inode_indirect_block), 0);
        if(snd.mapping[sec_info.fst_index] == SECTOR_MAGIC)
            snd.mapping[sec_info.fst_index] = new;
        buffer_cache_write(inode_disk->indirect, &snd, 0, sizeof(struct inode_indirect_block), 0);
    }

    else if(sec_info.direct_num == 2){
        if(inode_disk->double_indirect == SECTOR_MAGIC){
            if(free_map_allocate(1, &inode_disk->double_indirect) == false)
                return false;
            memset (&fst, -1, sizeof (struct inode_indirect_block));
        }
        else
            buffer_cache_read(inode_disk->double_indirect, &fst, 0, sizeof(struct inode_indirect_block), 0);

        if(fst.mapping[sec_info.fst_index] == SECTOR_MAGIC){
            if(free_map_allocate(1, &fst.mapping[sec_info.fst_index]) == false)
                return false;
            memset (&snd, -1, sizeof (struct inode_indirect_block));
            if(snd.mapping[sec_info.snd_index] == SECTOR_MAGIC)
                snd.mapping[sec_info.snd_index] = new;
            buffer_cache_write(inode_disk->double_indirect, &fst, 0, sizeof(struct inode_indirect_block), 0);
            buffer_cache_write(fst.mapping[sec_info.fst_index], &snd, 0, sizeof(struct inode_indirect_block), 0);
        }
        else{
            buffer_cache_read(fst.mapping[sec_info.fst_index], &snd, 0, sizeof(struct inode_indirect_block), 0);
            if(snd.mapping[sec_info.snd_index] == SECTOR_MAGIC)
                snd.mapping[sec_info.snd_index] = new;
            buffer_cache_write(fst.mapping[sec_info.fst_index], &snd, 0, sizeof(struct inode_indirect_block), 0);
        }
    }
    else
        return false;
    return true;
}

    static block_sector_t
byte_to_sector (const struct inode_disk *inode_disk, off_t pos) 
{
    //proj5
    if (pos >= inode_disk->length)
        return -1;
    struct inode_indirect_block *temp = (struct inode_indirect_block *)malloc(BLOCK_SECTOR_SIZE);
    block_sector_t result;
    struct sector_info sec_info;
    compute_location(pos, &sec_info);

    if (sec_info.direct_num == 0){
        free(temp);
        return inode_disk->direct[sec_info.fst_index];
    }

    else if (sec_info.direct_num == 1){
        if (inode_disk->indirect == SECTOR_MAGIC){
            free(temp);
            return -1;
        }
        buffer_cache_read(inode_disk->indirect, temp, 0, sizeof(struct inode_indirect_block), 0);
        result = temp->mapping[sec_info.fst_index];
        free(temp);
        return result;
    }

    else if (sec_info.direct_num == 2){
        if (inode_disk->double_indirect == SECTOR_MAGIC){
            free(temp);
            return -1;
        }
        buffer_cache_read(inode_disk->double_indirect, temp, 0, sizeof(struct inode_indirect_block), 0);
        if (temp->mapping[sec_info.fst_index] == SECTOR_MAGIC){
            free(temp);
            return -1;
        }
        buffer_cache_read(temp->mapping[sec_info.fst_index], temp, 0, sizeof(struct inode_indirect_block), 0);
        result = temp->mapping[sec_info.snd_index];
        free(temp);
        return result;
    }
    else{
        free(temp);
        return -1;
    }
}

//proj5
bool compute_file_length(struct inode_disk *inode_disk, off_t start, off_t end)
{
    static char zero[BLOCK_SECTOR_SIZE];
    inode_disk->length = end;
    start = (start / BLOCK_SECTOR_SIZE) * BLOCK_SECTOR_SIZE;
    end = ((end - 1) / BLOCK_SECTOR_SIZE) * BLOCK_SECTOR_SIZE;
    for(;start<=end; start = start + BLOCK_SECTOR_SIZE){
        block_sector_t sector = byte_to_sector(inode_disk, start);
        if(sector == SECTOR_MAGIC){
            if(free_map_allocate(1, &sector) == false)
                return false;
            struct sector_info sec_info;
            compute_location(start, &sec_info);
            if(make_new_sector(inode_disk, sector, sec_info) == false)
                return false;
            buffer_cache_write(sector, zero, 0, BLOCK_SECTOR_SIZE, 0);            
        }
    }
    return true;
}

//proj5
void free_sectors(struct inode_disk *inode_disk)
{
    if (inode_disk->double_indirect != SECTOR_MAGIC){
        struct inode_indirect_block *first_block = (struct inode_indirect_block *)malloc(BLOCK_SECTOR_SIZE);
        buffer_cache_read(inode_disk->double_indirect, first_block, 0, sizeof(struct inode_indirect_block), 0);
        for(int i = 0; first_block->mapping[i] != SECTOR_MAGIC; i++){
            struct inode_indirect_block *second_block = (struct inode_indirect_block *)malloc(BLOCK_SECTOR_SIZE);
            buffer_cache_read(second_block->mapping[0], second_block, 0, sizeof(struct inode_indirect_block), 0);
            for(int j = 0; second_block->mapping[j] != SECTOR_MAGIC; j++)
                free_map_release(second_block->mapping[j], 1);
            free_map_release(first_block->mapping[i], 1);
            free(second_block);    
        }
        free(first_block);
    }
    else if (inode_disk->indirect != SECTOR_MAGIC){
        struct inode_indirect_block *first_block = (struct inode_indirect_block *)malloc(BLOCK_SECTOR_SIZE);
        buffer_cache_read(inode_disk->indirect, first_block, 0, sizeof(struct inode_indirect_block), 0);
        for(int i = 0; first_block->mapping[i] != SECTOR_MAGIC; i++)
            free_map_release(first_block->mapping[i], 1);
        free(first_block);
    }
    else{
        for(int i = 0; inode_disk->direct[i] != SECTOR_MAGIC; i++)
            free_map_release(inode_disk->direct[i], 1);
    }
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
    void
inode_init (void) {
    list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
    bool
inode_create (block_sector_t sector, off_t length, bool is_dir)
{
    //proj5
    struct inode_disk *disk_inode = (struct inode_disk*)calloc(1, sizeof *disk_inode);
    ASSERT(length >= 0);
    ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);
    if(disk_inode == NULL)
        return false;
    memset (disk_inode, -1, sizeof (struct inode_disk));
    disk_inode->magic = INODE_MAGIC;
    disk_inode->is_dir = is_dir;
    if (compute_file_length(disk_inode, disk_inode->length, length) == false){
        free(disk_inode);
        return false;
    }
    buffer_cache_write(sector, disk_inode, 0, BLOCK_SECTOR_SIZE, 0);
    free(disk_inode);
    return true;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
    struct inode *
inode_open (block_sector_t sector)
{
    struct list_elem *e;
    struct inode *inode;

    /* Check whether this inode is already open. */
    for (e = list_begin (&open_inodes); e != list_end (&open_inodes); e = list_next (e)) {
        inode = list_entry (e, struct inode, elem);
        if (inode->sector == sector) {
            inode_reopen (inode);
            return inode; 
        }
    }

    /* Allocate memory. */
    inode = malloc (sizeof *inode);
    if (inode == NULL)
        return NULL;

    /* Initialize. */
    list_push_front (&open_inodes, &inode->elem);
    inode->sector = sector;
    inode->open_cnt = 1;
    inode->deny_write_cnt = 0;
    inode->removed = false;
    //proj5
    lock_init(&inode->lock);
    return inode;
}

/* Reopens and returns INODE. */
    struct inode *
inode_reopen (struct inode *inode)
{
    if (inode != NULL)
        inode->open_cnt++;
    return inode;
}

/* Returns INODE's inode number. */
    block_sector_t
inode_get_inumber (const struct inode *inode){
    return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
    void
inode_close (struct inode *inode) 
{
    /* Ignore null pointer. */
    if (inode == NULL)
        return;
    /* Release resources if this was the last opener. */
    if (--inode->open_cnt == 0)
    {
        /* Remove from inode list and release lock. */
        list_remove (&inode->elem);
        /* Deallocate blocks if removed. */
        if (inode->removed){
            //proj5
            struct inode_disk inode_disk;
            buffer_cache_read(inode->sector, &inode_disk, 0, BLOCK_SECTOR_SIZE, 0);
            free_sectors(&inode_disk);
            free_map_release (inode->sector, 1);
        }
        free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
    void
inode_remove (struct inode *inode) 
{
    ASSERT (inode != NULL);
    inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
    off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
    struct inode_disk inode_disk;
    uint8_t *buffer = buffer_;
    off_t bytes_read = 0;
    uint8_t *bounce = NULL;
    //proj5
    lock_acquire(&inode->lock);
    buffer_cache_read(inode->sector, &inode_disk, 0, sizeof(struct inode_disk), 0);
    lock_release(&inode->lock);
    while (size > 0) {
        /* Disk sector to read, starting byte offset within sector. */
        //proj5
        block_sector_t sector_idx = byte_to_sector (&inode_disk, offset);
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;
        /* Bytes left in inode, bytes left in sector, lesser of the two. */
        //proj5
        off_t inode_left = inode_disk.length - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int min_left = inode_left < sector_left ? inode_left : sector_left;
        /* Number of bytes to actually copy out of this sector. */
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0)
            break;
        //proj5
        buffer_cache_read(sector_idx, buffer, bytes_read, chunk_size, sector_ofs);
        /* Advance. */
        size -= chunk_size;
        offset += chunk_size;
        bytes_read += chunk_size;
    }
    return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
    off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size, off_t offset) 
{
    struct inode_disk inode_disk;
    const uint8_t *buffer = buffer_;
    off_t bytes_written = 0;
    uint8_t *bounce = NULL;

    if (inode->deny_write_cnt)
        return 0;
    //proj5
    lock_acquire(&inode->lock);
    buffer_cache_read(inode->sector, &inode_disk, 0, sizeof(struct inode_disk), 0);
    if (inode_disk.length < offset + size){
        if(compute_file_length(&inode_disk, inode_disk.length, offset + size) == true)
            buffer_cache_write(inode->sector, &inode_disk, 0, BLOCK_SECTOR_SIZE, 0);
    }
    lock_release(&inode->lock);

    while (size > 0) {
        /* Sector to write, starting byte offset within sector. */
        //proj5
        block_sector_t sector_idx = byte_to_sector (&inode_disk, offset);
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;
        /* Bytes left in inode, bytes left in sector, lesser of the two. */
        //proj5
        off_t inode_left = inode_disk.length - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int min_left = inode_left < sector_left ? inode_left : sector_left;
        /* Number of bytes to actually write into this sector. */
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0)
            break;
        //proj5
        buffer_cache_write(sector_idx, buffer, bytes_written, chunk_size, sector_ofs);
        /* Advance. */
        size -= chunk_size;
        offset += chunk_size;
        bytes_written += chunk_size;
    }
    return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
    void
inode_deny_write (struct inode *inode) 
{
    inode->deny_write_cnt++;
    ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
    void
inode_allow_write (struct inode *inode) 
{
    ASSERT (inode->deny_write_cnt > 0);
    ASSERT (inode->deny_write_cnt <= inode->open_cnt);
    inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
    off_t
inode_length (const struct inode *inode)
{
    //proj5
    struct inode_disk* temp = (struct inode_disk*)malloc(BLOCK_SECTOR_SIZE);
    buffer_cache_read(inode->sector, temp, 0, BLOCK_SECTOR_SIZE, 0);
    off_t length = temp->length;
    free(temp);
    return length;
}

//proj5
bool inode_is_dir(struct inode *inode)
{
    if (inode->removed)
        return false;
    struct inode_disk* temp = (struct inode_disk*)malloc(BLOCK_SECTOR_SIZE);
    buffer_cache_read(inode->sector, temp, 0, BLOCK_SECTOR_SIZE, 0);
    bool check = temp->is_dir;
    free(temp);
    return check;
}
