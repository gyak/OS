#include "frame.h"
#include "page.h"
#include "swap.h"
#include "devices/block.h"

struct bitmap* swap_bitmap;
struct block* swap_block;
const size_t SECTORS_PER_SIZE = PGSIZE / BLOCK_SECTOR_SIZE;
struct lock swap_lock;

void swap_init(void)
{
	swap_block = block_get_role(BLOCK_SWAP);
	swap_bitmap = bitmap_create(block_size(swap_block)/SECTORS_PER_SIZE);
	lock_init(&swap_lock);
	ASSERT(swap_bitmap != NULL);
}

size_t swap_out(void* kaddr)
{
	int check = 0;
	lock_acquire(&swap_lock);
	int index = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);
	ASSERT(index != BITMAP_ERROR);
	int sector_index = index * SECTORS_PER_SIZE;
	for(int i = 0; i < SECTORS_PER_SIZE; i++){
		check++;
		block_write(swap_block, sector_index + i, kaddr);
		kaddr = kaddr + BLOCK_SECTOR_SIZE;
	}
	//printf("swap_out check = %d\n", check);
	lock_release(&swap_lock);
	return index;
}

void swap_in(size_t used_index, void* kaddr)
{
	int check = 0;
	lock_acquire(&swap_lock);
	int sector_index = used_index * SECTORS_PER_SIZE;
	for(int i = 0; i < SECTORS_PER_SIZE; i++){
		check++;
		block_read(swap_block, sector_index + i, kaddr);
		kaddr = kaddr + BLOCK_SECTOR_SIZE;
	}
	//printf("swap_in check = %d\n", check);
	bitmap_set(swap_bitmap, used_index, false);
	lock_release(&swap_lock);
}
