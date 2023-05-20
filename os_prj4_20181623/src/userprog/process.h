#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "lib/user/syscall.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
struct file *process_get_file(int fd);
bool grow_stack(void* kaddr);
void do_munmap(mapid_t mapping);

#endif /* userprog/process.h */
