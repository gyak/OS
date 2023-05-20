#include <stdlib.h>
#include "frame.h"
#include "page.h"
#include "swap.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"

unsigned vm_hash_func(const struct hash_elem* e)
{
	struct vm_entry* vme = hash_entry(e, struct vm_entry, elem);
	return hash_int((int)vme->vaddr);
}

bool vm_less_func(const struct hash_elem* a, const struct hash_elem* b)
{
	struct vm_entry* vme_a = hash_entry(a, struct vm_entry, elem);
	struct vm_entry* vme_b = hash_entry(b, struct vm_entry, elem);
	if(vme_a->vaddr < vme_b->vaddr)
		return true;
	else
		return false;
}

bool insert_vme(struct hash* vm, struct vm_entry* vme)
{
	if(hash_insert(vm, &(vme->elem)) == NULL) 	
		return true;
	else
		return false;
}

struct vm_entry *find_vme(void* vaddr)
{
	struct hash_elem* he = NULL;
	struct vm_entry vme;

	vme.vaddr = pg_round_down(vaddr);
	he = hash_find(&thread_current()->vm, &vme.elem);
	if(he == NULL) 
		return NULL;
	return hash_entry(he, struct vm_entry, elem);
}

void vm_destructor(struct hash_elem* e)
{
	const struct vm_entry* vme = hash_entry(e, struct vm_entry, elem);
	if(vme->is_load == false){
		free(vme);
		return;
	}
	free_page(pagedir_get_page(thread_current()->pagedir, vme->vaddr));
	pagedir_clear_page(thread_current()->pagedir, vme->vaddr);
	free(vme);
}

bool load_file(void* kaddr, struct vm_entry* vme)
{
	off_t read_bytes = 0;
	if(vme->read_bytes == 0)
		return true;
	lock_acquire(&filesys_lock);
	file_seek(vme->file, vme->offset);
	read_bytes = file_read(vme->file, kaddr, vme->read_bytes);
	lock_release(&filesys_lock);
	memset(kaddr + vme->read_bytes, 0, vme->zero_bytes);
	if(read_bytes == vme->read_bytes)
		return true;
	else
		return false;
}