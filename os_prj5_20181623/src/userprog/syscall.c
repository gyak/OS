#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "threads/vaddr.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "pagedir.h"

#include "filesys/filesys.h"
#include "threads/synch.h"
#include <string.h>
#include "filesys/directory.h"
#include "filesys/inode.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);
void halt(void);
pid_t exec(const char* cmd_line);
int wait(pid_t pid);
int read(int fd, void* buffer, unsigned length);
int write(int fd, const void* buffer, unsigned length);
int fibonacci(int n);
int max_of_four_int(int a, int b, int c, int d);
void exit(int status);
void addr_check(void *addr);
void get_argument(void *esp, void *args[], int count);

bool create(const char* file, unsigned initial_size);
bool remove(const char* file);
int open(const char* file);
void close(int fd);
int filesize(int fd);
void seek(int fd, unsigned position);
unsigned tell(int fd);
//proj5
int inumber(int fd);
bool isdir(int fd);
int readdir(int fd, char* name);
int mkdir(char* dir);
int chdir(char* path);

struct lock filesys_lock;

//proj5
struct dir
{
    struct inode *inode; 
    off_t pos;          
};

struct file 
  {
    struct inode *inode;        
    off_t pos;                 
    bool deny_write;          
  };

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  
  void* args[4];

  switch(*(unsigned*)(f->esp)){//syscall#
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		get_argument(f->esp, args, 1);
		exit((int)*(int*)args[0]);
		break;
	case SYS_EXEC:
		get_argument(f->esp, args, 1);
		f->eax = exec((const char*)*(uint32_t*)args[0]);
		break;
	case SYS_WAIT:
		get_argument(f->esp, args, 1);
		f->eax = wait((pid_t)*(uint32_t*)args[0]);
		break;
	case SYS_READ:
		get_argument(f->esp, args, 3);
		f->eax = read((int)*(uint32_t*)args[0], (void*)*(uint32_t*)args[1], (unsigned)*(uint32_t*)args[2]);
		break;
	case SYS_WRITE:
		get_argument(f->esp, args, 3);
		f->eax = write((int)*(uint32_t*)args[0], (void*)*(uint32_t*)args[1], (unsigned)*(uint32_t*)args[2]);
		break;
	case SYS_MAX:
		get_argument(f->esp, args, 4);
		f->eax = max_of_four_int((int)*(uint32_t*)args[0], (int)*(uint32_t*)args[1], (int)*(uint32_t*)args[2], (int)*(uint32_t*)args[3]);
		break;
	case SYS_FIBO:
		get_argument(f->esp, args, 1);
		f->eax = fibonacci((int)*(uint32_t*)args[0]);
		break;
    //proj2
    case SYS_CREATE:
		get_argument(f->esp, args, 2);
		f->eax = create((const char*)*(uint32_t*)args[0], (unsigned)*(uint32_t*)args[1]);
		break;
	case SYS_REMOVE:
		get_argument(f->esp, args, 1);
		f->eax = remove((const char*)*(uint32_t*)args[0]);
		break;
	case SYS_OPEN:
		get_argument(f->esp, args, 1);
		f->eax = open((const char*)*(uint32_t*)args[0]);
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
	//proj5
	case SYS_MKDIR:
        get_argument(f->esp, args, 1);
        f->eax = mkdir ((const char *)*(uint32_t*)args[0]);
        break;
    case SYS_CHDIR:
        get_argument(f->esp, args, 1);
        f->eax = chdir ((const char *)*(uint32_t*)args[0]);
        break;
    case SYS_READDIR:
        get_argument(f->esp, args, 2);
        f->eax = readdir ((int)*(uint32_t*)args[0], (char *)*(uint32_t*)args[1]);
        break;
    case SYS_ISDIR:
        get_argument(f->esp, args, 1);
        f->eax = isdir ((int)*(uint32_t*)args[0]);
        break;
    case SYS_INUMBER:
        get_argument(f->esp, args, 1);
        f->eax = inumber ((int)*(uint32_t*)args[0]);
        break;
	default:
		thread_exit();
  }
}


void halt(){
	shutdown_power_off();
}

void exit(int status)
{
	thread_current()->exit_status = status;
	printf("%s: exit(%d)\n", thread_current()->name, status);
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
	addr_check(buffer);
	struct file* file = process_get_file(fd);
	if(file != NULL){
		fr = file_read(file, buffer, length);
	}
    else{
        lock_release(&filesys_lock);
        exit(-1);
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
	if(file != NULL){
		fw = file_write(file, buffer, length);
	}
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

void addr_check(void *addr){
	if(!is_user_vaddr(addr)) 
        exit(-1);
}

void get_argument(void *esp, void *args[], int count)
{
	for(int i = 1; i <= count; i++){
		args[i-1] = (void*)((unsigned*)(esp + 4*i));
		addr_check(args[i-1]);
	}
}


/* proj2 */
bool create(const char* file, unsigned initial_size)
{
	if(file == NULL)
		exit(-1);
    addr_check((void*)file);
	lock_acquire(&filesys_lock);	
	bool fc = filesys_create(file, initial_size);
	lock_release(&filesys_lock);
	return fc;
}

bool remove(const char* file)
{
    if(file == NULL)
        exit(-1);
    addr_check((void*)file);
	lock_acquire(&filesys_lock);
	bool fr = filesys_remove(file);
	lock_release(&filesys_lock);
	return fr;
}

int open(const char* file)
{
	if(file == NULL)
		return -1;
    addr_check((void*)file);
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
        if(2 < fd && fd < 128)
            file = thread_current()->fd[fd];
        if(file != NULL){
            file_close(thread_current()->fd[fd]);
            thread_current()->fd[fd] = NULL;
        }
    }
}

int filesize(int fd)
{
	struct file* file = process_get_file(fd);
	if(file == NULL){
		return -1;
	}
	return file_length(file);
}

void seek(int fd, unsigned position)
{
	struct file* file = process_get_file(fd);
    if(file == NULL)
        exit(-1);
    file_seek(file, position);
}

unsigned tell(int fd)
{
	struct file* file = process_get_file(fd);
    if(file == NULL)
        exit(-1);
    return file_tell(file);
}

//proj5
int chdir(char* path){
    return change_dir(path);
}

int mkdir(char* dir){
    return create_dir(dir);
}

int readdir(int fd, char* name)
{
    struct file *cur_file = thread_current()->fd[fd];
    struct inode *cur_inode = file_get_inode(cur_file);
    if (cur_inode == NULL || inode_is_dir(cur_inode) == false)
        return false;
    struct dir *cur_dir = dir_open(cur_inode);
    cur_dir->pos = file_tell(cur_file);
    file_seek(cur_file, cur_dir->pos);
    return dir_readdir(cur_dir, name);
}

bool isdir(int fd){
	struct file* cur_file = thread_current()->fd[fd];
	return inode_is_dir(cur_file->inode);
}

int inumber(int fd){
	struct file* cur_file = thread_current()->fd[fd];
	return inode_get_inumber(cur_file->inode);
}