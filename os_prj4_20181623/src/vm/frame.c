#include <stdlib.h>
#include "frame.h"
#include "page.h"
#include "swap.h"
#include "lib/kernel/list.h"
#include "userprog/syscall.h"

void free_page(void* kaddr)
{
	ASSERT (kaddr != NULL);
	struct page* temp = NULL;
	for(struct list_elem* e = list_begin(&lru_list); e != list_end(&lru_list); e = list_next(e)){
		temp = list_entry(e, struct page, lru);
		if(temp->kaddr == kaddr){
			if(lru_clock == e && lru_clock != NULL){
				if(list_size(&lru_list) != 1){
					if(list_end(&lru_list) != lru_clock)
						lru_clock = list_next(lru_clock);
					else 
						lru_clock = list_front(&lru_list);
				}
				else
					lru_clock = NULL;
			}
			ASSERT (list_empty(&lru_list) == false);
			ASSERT (temp != NULL);
			lock_acquire(&lru_list_lock);
			list_remove(&temp->lru);
			lock_release(&lru_list_lock);
			palloc_free_page(temp->kaddr);
			free(temp);
			break;
		}
	}
}

struct list_elem* next_lru_clock(void)
{
	lock_acquire(&lru_list_lock);
	if(lru_clock == NULL){
		if(list_empty(&lru_list) == false)
			lru_clock = list_front(&lru_list);
		else {
			lock_release(&lru_list_lock);
			return lru_clock;
		}
	}
	if(list_empty(&lru_list) == true){
		lru_clock = NULL;
		lock_release(&lru_list_lock);
		return lru_clock;
	}
	struct page* page = list_entry((struct list_elem*)lru_clock, struct page, lru);
	while(1){
		if(page->vme->is_pin == false){
			if(pagedir_is_accessed(page->thread->pagedir, page->vme->vaddr))
				pagedir_set_accessed(page->thread->pagedir, page->vme->vaddr, false);
			else
				break;
		}
		if(lru_clock == list_back(&lru_list))
			lru_clock = list_front(&lru_list);
		else 
			lru_clock = list_next(lru_clock);
		page = list_entry((struct list_elem*)lru_clock, struct page, lru);
	}
	lock_release(&lru_list_lock);
	return lru_clock;
}

void* get_free_pages(enum palloc_flags flags)
{
	struct list_elem* e = next_lru_clock();
	struct page* target = list_entry(e, struct page, lru);
	void* uaddr = target->vme->vaddr;
	void* kaddr = target->kaddr;
	if(target->vme->type == VM_BIN && pagedir_is_dirty(target->thread->pagedir, uaddr)){
		target->vme->type = VM_ANON;
		target->vme->swap_slot = swap_out(kaddr);
	}
	else if (target->vme->type == VM_ANON)
		target->vme->swap_slot = swap_out(kaddr);
	target->vme->is_load = false;
	pagedir_clear_page(target->thread->pagedir, uaddr);
	free_page(kaddr);
	return palloc_get_page(flags);
}

struct page* alloc_page(enum palloc_flags flags)
{
	void* kaddr = palloc_get_page(flags);
	if(kaddr == NULL)
		kaddr = get_free_pages(flags);
	struct page* page = (struct page*)calloc(1, sizeof(struct page));
	page->vme = NULL;
	page->kaddr = kaddr;
	page->thread = thread_current();
	lock_acquire(&lru_list_lock);
	list_push_back(&lru_list, &(page->lru));
	lock_release(&lru_list_lock);
	return page;
}