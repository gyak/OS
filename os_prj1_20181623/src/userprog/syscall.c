#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <string.h>
#include "threads/vaddr.h"
typedef int pid_t;
static void syscall_handler (struct intr_frame *);
//void halt (void) NO_RETURN;
void exit (int status) NO_RETURN;
pid_t exec (const char *file);
int wait (pid_t);
//bool create (const char *file, unsigned initial_size);
//bool remove (const char *file);
//int open (const char *file);
//int filesize (int fd);
//int read (int fd, void *buffer, unsigned length);
int write (int fd, const void *buffer, unsigned length);
//void seek (int fd, unsigned position);
//unsigned tell (int fd);
//void close (int fd);

    void
addr_check(void* addr)
{   
    if(!is_user_vaddr(addr))
        exit(-1);
}

    void
syscall_init (void) 
{
    intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

    static void
syscall_handler (struct intr_frame *f UNUSED) 
{
    //printf("1111\n");
    //printf("syscall: %d\n", *(int*)(f->esp));
    uint32_t* now = (uint32_t*)(f->esp);
    switch(*(int*)(f->esp)){//syscall number
        case SYS_HALT:
            shutdown_power_off();

            break;
        case SYS_EXIT:
            addr_check(f->esp+4);
            //printf("esp: %p\n", f->esp);
            //hex_dump(f->esp, f->esp, 500, 1);
            exit(*(uint32_t*)(f->esp+4));
            break;
        case SYS_EXEC:
            addr_check(f->esp+4);
            f->eax = exec((const char*)*(uint32_t*)(f->esp+4));
            break;
        case SYS_WAIT:
            addr_check(f->esp+4);
            f->eax = wait((pid_t)*(uint32_t*)(f->esp+4));
            break;
        case SYS_READ:
            addr_check(f->esp+64);
            addr_check(f->esp+68);
            addr_check(f->esp+72);
            //hex_dump(f->esp, f->esp, 1000, 1);
            f->eax = read((int)*(uint32_t*)(f->esp+64), (void*)*(uint32_t*)(f->esp+68), (unsigned)*((uint32_t*)(f->esp+72)));
            break;
        case SYS_WRITE:
            addr_check(f->esp+4);
            addr_check(f->esp+8);
            addr_check(f->esp+12);
            //printf("\n\nthats right!!!\n\n\n");
            //char temp_buf[1000];
            //temp_buf[0] = '\0';
            //strcat(temp_buf,
            //hex_dump(f->esp, f->esp, 1000, 1);
            //printf("\nesp: %p\n", f->esp);
            /*printf("%s\n", (char*)(f->ebx));
              printf("%s\n", (char*)(f->ecx));*/


            //printf("%c\n", (char)*(uint32_t*)(f->esp+0x152));
            //printf("%d\n", (unsigned)*(uint32_t*)(f->esp+12));
            f->eax = write((int)*(uint32_t*)(f->esp+4), (void*)*(uint32_t*)(f->esp+8), (unsigned)*(uint32_t*)(f->esp+12));
            break;
        case SYS_MAX:
            //printf("in SYS_MAX\n");
            //hex_dump(f->esp, f->esp, 400, 1);
            addr_check(f->esp+28);
            addr_check(f->esp+32);
            addr_check(f->esp+36);
            addr_check(f->esp+40);
            f->eax = max_of_four_int((int)*(uint32_t*)(f->esp+28), (int)*(uint32_t*)(f->esp+32), (int)*(uint32_t*)(f->esp+36), (int)*(uint32_t*)(f->esp+40));
            break;
        case SYS_FIBO:
            //printf("in SYS_FIBO\n");
            //hex_dump(f->esp, f->esp, 400, 1);
            addr_check(f->esp+12);
            f->eax = fibonacci((int)*(uint32_t*)(f->esp+12));
            break;
    }
    //printf ("system call!\n");
    //thread_exit ();
}

int
read (int fd, void* buffer, unsigned length)
{
    int i = -1;
    if(fd ==0){
        for(i = 0; i<length; i++){
            if(((char*)buffer)[i] == '\0')
                break;
        }
    }
    if(i == -1)
        return -1;
    else
        return i;
}

    int 
write (int fd, const void *buffer, unsigned length)
{
    //printf("buf: %s\n", (char*)buffer);
    //printf("buf: %s\n", (char*)buffer+strlen(buffer)+1);
    if(fd == 1){
        putbuf(buffer, length);
        return length;
    }
    return -1;
}

    void
exit (int status)
{
    //hex_dump(*esp, *esp, 100, 1);
    //printf("thread_name: %s\n", thread_name());
    int i = 0;
    char temp[100] = {};
    strlcpy(temp, thread_name(), strlen(thread_name())+1);
    /*for(int k = 0; k<10; k++){
      if(temp[k] == '\0')
      printf("NULL\n");
      else
      printf("%c\n", temp[k]);
      }*/
    //printf("len: %d\n", strlen(temp));
    for(i=0; i<strlen(temp)+1; i++){
        if(temp[i] == ' ' || temp[i] == '\0'){
            break;
        }
    }
    //printf("i = %d\n", i);
    temp[i] = '\0';
    printf("%s: exit(%d)\n", temp, status);
    //printf("%s: exit(%d)\n", thread_name(), status);
    //printf("%s: exit(%d)\n", "echo", status);
    thread_current()->exit_status = status;
    thread_exit ();
}

    pid_t
exec(const char* file){
    return process_execute(file);
}

    int
wait(pid_t pid){
    return process_wait(pid);
}

int
fibonacci(int n)
{
    int a=1, b=1, c=2;
    if(n < 0)
        return -1;
    else if(n == 0)
        return 0;
    else if(n == 1)
        return 1;
    else if(n == 2)
        return 1;
    else{
        for(int i=2; i<n; i++){
            c = a + b;
            a = b;
            b = c;
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
