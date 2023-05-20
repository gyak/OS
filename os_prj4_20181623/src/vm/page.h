#include <stdlib.h>
#include <string.h>
#include "lib/kernel/hash.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/palloc.h"

#define VM_BIN		0
#define VM_FILE		1
#define VM_ANON		2

#define MAX_STACK_SIZE (1<<23)

struct vm_entry{
	uint8_t type;		
	bool is_write;		
	bool is_load;		
	bool is_pin;
	void *vaddr;	
	struct file* file;		
	struct hash_elem elem;	
	size_t offset;			
	size_t read_bytes;		
	size_t zero_bytes;		
	size_t swap_slot;		

};

struct page{
	void* kaddr;
	struct vm_entry* vme;
	struct thread* thread;
	struct list_elem lru;
	bool is_pin;
};

struct mmap_file {
	int mapid;
	struct list_elem elem;
	struct vm_entry* vme;	
};

struct list lru_list;		
struct lock lru_list_lock;
void* lru_clock;

//unsigned vm_hash_func(const struct hash_elem* e, void* aux);
//bool vm_less_func(const struct hash_elem* a, const struct hash_elem* b, void* aux);	
//void vm_destructor(struct hash_elem* e, void* aux);
unsigned vm_hash_func(const struct hash_elem* e);
bool vm_less_func(const struct hash_elem* a, const struct hash_elem* b);	
void vm_destructor(struct hash_elem* e);
bool insert_vme(struct hash* vm, struct vm_entry* vme);
struct vm_entry *find_vme(void* vaddr);
bool load_file(void* kaddr, struct vm_entry* vme);