#include "userprog/syscall.h"
#include "pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "lib/user/syscall.h"

extern struct list lru_list;
static void syscall_handler (struct intr_frame *);
void halt(void);
pid_t exec(const char* cmd_line);
int wait(pid_t pid);
int read(int fd, void* buffer, unsigned size);
int write(int fd, const void* buffer, unsigned size);
int fibonacci(int n);
int max_of_four_int(int a, int b, int c, int d);
void get_argument(void *esp, void *args[], int count);
struct vm_entry* addr_check(void *addr, void* esp);
void check_valid_buffer(void* buffer, unsigned size, void* esp, bool to_write);
void check_valid_string(const void* str, void* esp);
void unpin_ptr(void* addr);
void unpin_string(void* str);
void unpin_buffer(void* buffer, unsigned size);
mapid_t mmap(int fd, void* addr);
void munmap(mapid_t mapping);
bool create(const char* file, unsigned initial_size);
bool remove(const char* file);
int open(const char* file);
void close(int fd);
int filesize(int fd);
void seek(int fd, unsigned position);
unsigned tell(int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
	// proj4
	addr_check((const void*) f->esp, f->esp);
  /* get stack pointer from interrupt frame */
  /* get system call number from stack */
  unsigned sys_num = *(unsigned*)(f->esp);
  void* args[4];
	//proj4
  switch(sys_num){
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		get_argument(f->esp, args, 1);
		exit((int)*(int*)args[0]);
		break;
	case SYS_EXEC:
		get_argument(f->esp, args, 1);
		check_valid_string((char*)*(uint32_t*)args[0], f->esp);
		f->eax = exec((const char*)*(uint32_t*)args[0]);
		unpin_string((void*)args[0]);
		break;
	case SYS_WAIT:
		get_argument(f->esp, args, 1);
		f->eax = wait((pid_t)*(uint32_t*)args[0]);
		break;
	case SYS_READ:
		get_argument(f->esp, args, 3);
		check_valid_buffer((void*)*(uint32_t*)args[1], (size_t)*(uint32_t*)args[2], f->esp, true);
		f->eax = read((int)*(uint32_t*)args[0], (void*)*(uint32_t*)args[1], (unsigned)*(uint32_t*)args[2]);
		unpin_buffer((void*)args[1], (unsigned)args[2]);
		break;
	case SYS_WRITE:
		get_argument(f->esp, args, 3);
		check_valid_buffer((void*)*(uint32_t*)args[1], (size_t)*(uint32_t*)args[2], f->esp, true);
		f->eax = write((int)*(uint32_t*)args[0], (void*)*(uint32_t*)args[1], (unsigned)*(uint32_t*)args[2]);
		unpin_buffer((void*)args[1], (unsigned)args[2]);
		break;
	case SYS_MAX:
		get_argument(f->esp, args, 4);
		f->eax = max_of_four_int((int)*(int*)args[0], (int)*(int*)args[1], (int)*(int*)args[2], (int)*(int*)args[3]);
		break;
	case SYS_FIBO:
		get_argument(f->esp, args, 1);
		f->eax = fibonacci((int)*(uint32_t*)args[0]);
		break;
	case SYS_CREATE:
		get_argument(f->esp, args, 2);
		check_valid_string((const void*)args[0], f->esp);
		f->eax = create((const char*)*(uint32_t*)args[0], (unsigned)*(uint32_t*)args[1]);
		break;
	case SYS_REMOVE:
		get_argument(f->esp, args, 1);
		check_valid_string((const void*)args[0], f->esp);
		f->eax = remove((const char*)*(uint32_t*)args[0]);
		break;
	case SYS_OPEN:
		get_argument(f->esp, args, 1);
		check_valid_string((const char*)*(uint32_t*)args[0], f->esp);
		f->eax = open((const char*)*(uint32_t*)args[0]);
		unpin_string((void*)args[0]);
		break;
	case SYS_FILESIZE:
		get_argument(f->esp, args, 1);
		f->eax = filesize((int)*(uint32_t*)args[0]);
		break;
	case SYS_SEEK:
		get_argument(f->esp, args, 2);
		seek((int)*(uint32_t*)args[0], (unsigned)*(uint32_t*)args[1]);
		break;
	case SYS_TELL:
		get_argument(f->esp, args, 1);
		f->eax = tell((int)*(uint32_t*)args[0]);
		break;
	case SYS_CLOSE:
		get_argument(f->esp, args, 1);
		close((int)*(uint32_t*)args[0]);
		break;
	case SYS_MMAP:
		get_argument(f->esp, args, 2);
		f->eax = mmap((int)*(uint32_t*)args[0], (void*)*(uint32_t*)args[1]);
		break;
	case SYS_MUNMAP:
		get_argument(f->esp, args, 1);
		munmap((int)*(uint32_t*)args[0]);
		break;
	default:
		thread_exit();
  }
  // proj4
  unpin_ptr(f->esp);
}

void halt(){
	shutdown_power_off();
}

void exit(int status)
{
	struct thread *cur = thread_current();
	cur->exit_status = status;

	printf("%s: exit(%d)\n", cur->name, status);
	for(int i = 3; i<128; i++){
        if(thread_current()->fd[i] != NULL)
            close(i);
    }
	thread_exit();
}

pid_t exec(const char* cmd_line){
	return process_execute(cmd_line);
}

int wait(pid_t pid){
	return process_wait(pid);
}

int read(int fd, void* buffer, unsigned length)
{
	lock_acquire(&filesys_lock);
	int fr = -1;
	if(fd == 0){
		for(unsigned i = 0; i < length; i++){
			((char*)buffer)[i] = (char)input_getc();
		}
		lock_release(&filesys_lock);
		return length;
	}
	struct file* file = process_get_file(fd);
	if(file != NULL)
		fr = file_read(file, buffer, length);
	else{
		lock_release(&filesys_lock);
		return -1;
	}
	lock_release(&filesys_lock);
	return fr;
}

int write(int fd, const void* buffer, unsigned length)
{
	lock_acquire(&filesys_lock);
	int fw = -1;
	if(fd == 1){
		putbuf((char*)buffer, length);
		lock_release(&filesys_lock);
		return length;
	}
	struct file* file = process_get_file(fd);
	if(file != NULL)
		fw = file_write(file, buffer, length);
	else{
		lock_release(&filesys_lock);
		exit(-1);
	}

	lock_release(&filesys_lock);
	return fw;
}

int fibonacci(int n)
{
	int a=1, b=1, c;

	if(n<0)
		return -1;
    else if(n==0)
        return 0;
    else if(n==1)
        return 1;
    else if(n==2)
        return 1;
    else{
        for(int i=2; i<n; i++){
            c = a +b;
            a = b;
            b =c;
        }
        return c;
    }
}

int max_of_four_int(int a, int b, int c, int d)
{
	if(a>=b && a>=c && a>=d)
        return a;
    else if(b>=a && b>=c && b>=d)
        return b;
    else if(c>=a && c>=b && c>=d)
        return c;
    else 
        return d;
}

// proj4
struct vm_entry* addr_check(void *addr, void* esp)
{
	if((is_user_vaddr(addr) == false) || addr < USER_VADDR_BOTTOM) 
		exit(-1);
	struct vm_entry* vme = find_vme(addr);
	if(vme){
		handle_mm_fault(vme);
		if(vme->is_load == false)
			exit(-1);
	}
	else if((addr >= esp - STACK_HEURISTIC) && (grow_stack((void*)addr) == false))
		exit(-1);
	return vme;
}

void get_argument(void *esp, void *args[], int count)
{
	for(int i = 1; i <= count; i++){
		args[i-1] = (void*)((unsigned*)(esp + 4*i));
		addr_check(args[i-1], esp);
	}
}

// proj4
void check_valid_buffer(void* buffer, unsigned size, void* esp, bool to_write)
{
	struct vm_entry* vme;
	for(void* i = buffer; i < buffer+size; i++){
		vme = addr_check(i, esp);
		if((vme == NULL) || (vme->is_write == false)) 
			exit(-1);
	}
}

// proj4
void check_valid_string(const void* str, void* esp)
{
	for(; *(char*)str != 0; str = (char*)str + 1)
		addr_check(str, esp);
}

// proj4
void unpin_ptr(void* addr)
{
	struct vm_entry* vme = find_vme(addr);
	if(vme)
		vme->is_pin = false;
}

// proj4
void unpin_buffer(void* buffer, unsigned size)
{
	for(void* i = buffer; i < buffer + size; i++)
		unpin_ptr(i);
}

// proj4
void unpin_string(void* str)
{
	for(; *(char*)str != 0; str = (char*)str + 1)
		unpin_ptr(str);
}

// proj4
mapid_t mmap(int fd, void* addr)
{
	int check;
	lock_acquire(&filesys_lock);
	struct vm_entry* vme;
	struct mmap_file* m_temp;
	uint32_t page_read_bytes;
	uint32_t page_zero_bytes;
	struct file* file_temp = process_get_file(fd);
	struct file* fp = file_reopen(file_temp);
	uint32_t fl = file_length(file_temp);
	if(fl == 0 || (is_user_vaddr(addr) == false) || fd < 3)
		return -1;
	mapid_t mapid = ++(thread_current()->mapid);
	uint32_t read_bytes = file_length(fp);
	uint32_t ofs = 0;
	bool success = true;
	file_seek(fp, ofs);
	lock_release(&filesys_lock);
	while(read_bytes > 0){
		check++;
		if(read_bytes < PGSIZE)
			page_read_bytes = read_bytes;
		else
			page_read_bytes = PGSIZE;
		page_zero_bytes = PGSIZE - page_read_bytes;
		vme = (struct vm_entry*)malloc(sizeof(struct vm_entry));
		if(vme == NULL){
			success = false;
			break;
		}
		m_temp = (struct mmap_file*)malloc(sizeof(struct mmap_file));
		if(m_temp == NULL){
			free(vme);
			success = false;
			break;
		}
		vme->type = VM_FILE;
		vme->is_write = true;
		vme->is_pin = false;
		vme->is_load = false;
		vme->vaddr = addr;
		vme->file = fp;
		vme->offset = ofs;
	  	vme->swap_slot = 0;
		vme->read_bytes = page_read_bytes;
		vme->zero_bytes = page_zero_bytes;
		m_temp->mapid = mapid;
		m_temp->vme = vme;
		list_push_back(&thread_current()->mmap_list, &m_temp->elem);
		if(insert_vme(&thread_current()->vm, vme) == false){
			free(vme);
			list_remove(m_temp);
			free(m_temp);
			success = false;
			break;
		}
		ofs = ofs + page_read_bytes;
		addr = addr + PGSIZE;
		read_bytes = read_bytes - page_read_bytes;
	}
	if(success == false)
		return -1;
	else
		return mapid;
}

void munmap(mapid_t mapping){
	do_munmap(mapping);
}

bool create(const char* file, unsigned initial_size)
{
	if(file == NULL)
		exit(-1);
	lock_acquire(&filesys_lock);	
	bool fc = filesys_create(file, initial_size);
	lock_release(&filesys_lock);
	return fc;
}

bool remove(const char* file)
{
	if(file == NULL)
		exit(-1);
	lock_acquire(&filesys_lock);
	bool fr = filesys_remove(file);
	lock_release(&filesys_lock);
	return fr;
}

int open(const char* file)
{
	if(file == NULL)
		return -1;
	lock_acquire(&filesys_lock);
	struct file* fp = filesys_open(file);

	if(fp == NULL){
		lock_release(&filesys_lock);
		return -1;
	}
	if(strcmp(thread_current()->name, file) == 0)
		file_deny_write(fp);
	lock_release(&filesys_lock);
	if(fp == NULL)
        return -1;
    thread_current()->fd[++(thread_current()->fd_max)] = fp;
    return thread_current()->fd_max;
}

void close(int fd)
{
	struct file* file = NULL;
	if(thread_current()->fd[fd] != NULL){
		lock_acquire(&filesys_lock);
		if(2 < fd && fd < 128)
            file = thread_current()->fd[fd];
        if(file != NULL){
            file_close(thread_current()->fd[fd]);
            thread_current()->fd[fd] = NULL;
        }
		lock_release(&filesys_lock);
	}
}

int filesize(int fd)
{
	lock_acquire(&filesys_lock);
	struct file* file = process_get_file(fd);
	if(file == NULL){	
		lock_release(&filesys_lock);
		return -1;
	}
	int fl = file_length(file);
	lock_release(&filesys_lock);
	return fl;
}

void seek(int fd, unsigned position)
{
	lock_acquire(&filesys_lock);
	struct file* file = process_get_file(fd);
	if(file == NULL){
		lock_release(&filesys_lock);
		exit(-1);
	}
	file_seek(file, position);
	lock_release(&filesys_lock);
}

unsigned tell(int fd)
{
	lock_acquire(&filesys_lock);
	struct file* file = process_get_file(fd);
	if(file == NULL){
		lock_release(&filesys_lock);
		return -1;
	}
	unsigned ft = file_tell(file);
	lock_release(&filesys_lock);
	return ft;
}
