#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
void syscall_init (void);
void exit(int status);
struct lock filesys_lock;
typedef int pid_t;
#define USER_VADDR_BOTTOM ((void*)0x08048000)
#define STACK_HEURISTIC 32
#define CLOSE_ALL 0
#endif /* userprog/syscall.h */