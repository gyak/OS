#include <stddef.h>
#include "lib/kernel/bitmap.h"

void swap_init(void);
size_t swap_out(void* kaddr);
void swap_in(size_t used_index, void* kaddr);