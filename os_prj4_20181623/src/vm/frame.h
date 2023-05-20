#include "threads/palloc.h"

void free_page(void* kaddr);
struct list_elem* next_lru_clock(void);
void* get_free_pages(enum palloc_flags flags);
struct page* alloc_page(enum palloc_flags flags);