#include "userprog/syscall.h"

#include <console.h>
#include <stdio.h>
#include <syscall-nr.h>

#include "include/filesys/file.h"
#include "include/filesys/filesys.h"
#include "intrinsic.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/loader.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/gdt.h"
#include "userprog/process.h"

struct lock global_lock;
void syscall_entry(void);
void syscall_handler(struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

static void is_valid_pointer(void *);

void syscall_init(void)
{
	lock_init(&global_lock);
    write_msr(MSR_STAR, ((uint64_t) SEL_UCSEG - 0x10) << 48 |
                            ((uint64_t) SEL_KCSEG) << 32);
    write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

    /* The interrupt service rountine should not serve any interrupts
     * until the syscall_entry swaps the userland stack to the kernel
     * mode stack. Therefore, we masked the FLAG_FL. */
    write_msr(MSR_SYSCALL_MASK, FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);	
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f)
{
    uint64_t sys_numer = f->R.rax;
    switch (sys_numer)
    {
        case SYS_HALT:
            halt();
            break;
        case SYS_EXIT:
            int exit_status = f->R.rdi;
            exit(exit_status);  //status 숫자를 뭘 넣어줘야 하는거지?
            break;
        case SYS_FORK:
            const char* fork_file_name = f->R.rdi;
            struct intr_frame *if_ = f;
            is_valid_pointer(fork_file_name);
            f->R.rax = fork(f->R.rdi, if_);
            break;
        case SYS_EXEC:
            const char* exec_file_name = f->R.rdi;
            is_valid_pointer(exec_file_name);
            f->R.rax = exec(exec_file_name);
            break;
        case SYS_WAIT:
            int tid = f->R.rdi;
            f->R.rax = wait(tid);
            break;
        case SYS_CREATE:
            const char* create_file_name = f->R.rdi;
            unsigned initial_size = f->R.rsi;
            is_valid_pointer(create_file_name);
            f->R.rax = create(create_file_name, initial_size);
            break;
        case SYS_REMOVE:
            const char* remove_file_name = f->R.rdi;
            is_valid_pointer(remove_file_name);
            f->R.rax = remove(remove_file_name);
            break;
        case SYS_OPEN:
            const char* open_file_name = f->R.rdi;
            is_valid_pointer(open_file_name);
            f->R.rax = open(open_file_name);
            break;
        case SYS_FILESIZE:
            int filesize_fd = f->R.rdi;
            f->R.rax = filesize(filesize_fd);
            break;
        case SYS_READ:
            int read_fd = f->R.rdi;
            void* read_buffer = f->R.rsi;
            unsigned size = f->R.rdx;
            is_valid_pointer(read_buffer);
            f->R.rax = read(read_fd, read_buffer, size);
            break;
        case SYS_WRITE:
            int write_fd = f->R.rdi;
            void* write_buffer = f->R.rsi;
            unsigned length = f->R.rdx;
            is_valid_pointer(write_buffer);
            f->R.rax = write(write_fd, write_buffer, length);
            break;
        case SYS_SEEK:
            int seek_fd = f->R.rdi;
            unsigned position = f->R.rsi;
            seek(seek_fd, position);
            break;
        case SYS_TELL:
            int tell_fd = f->R.rdi;
            f->R.rax = tell(tell_fd);
            break;
        case SYS_CLOSE:
            int close_fd = f->R.rdi;
            close(close_fd);
            break;
            // projec 3
        case SYS_MMAP:
            	printf("dd");
            	break;
        case SYS_MUNMAP:
            	printf("dd");
            	break;
    }
    // thread_exit ();
}

void halt(void)
{
    power_off();
}

bool create(const char *file, unsigned initial_size)
{
    return filesys_create(file, initial_size);
}

bool remove(const char *file)
{
    return filesys_remove(file);
}

int open(const char *file)
{
    struct thread *cur = thread_current();

    lock_acquire(&global_lock);
    struct file *f = filesys_open(file);
    lock_release(&global_lock);

    if (f == NULL) return -1;

    for (int fd = 2; fd < 20; fd++) {
        if (cur->fdt[fd] == NULL)
        {
            cur->fdt[fd] = f;
            return fd;
        }
    }

    file_close(f);
    return -1;
}

int filesize(int fd)
{
    return file_length(get_file_by_fd(fd));
}

void exit(int status)
{
    thread_current()->exit_status = status; 
    thread_exit();
}

tid_t fork(const char *thread_name, struct intr_frame *if_)
{
    // struct intr_frame *if_ = pg_round_up(&thread_name) - sizeof(struct intr_frame);
    return process_fork(thread_name, if_);
}

/* Create child process and execute program correspond to cmd_line on it*/
tid_t exec(const char *cmd_line)
{
    tid_t result;
    char *file_copy = palloc_get_page(PAL_ZERO);
    if(file_copy == NULL) return -1;
    
    strlcpy(file_copy, cmd_line, PGSIZE);
    
    result = process_exec(file_copy);

    return result;
}

/* 자식 프로세스가 종료되기를 기다리고, 자식의 종료 상태를 반환*/
int wait(tid_t pid)
{
    if (pid < 0) exit(-1);

    return process_wait(pid);
}

int read(int fd, void *buffer, unsigned size)
{
    if (fd == 0)
    { 
        for (int i = 0; i < size; i++) ((uint8_t *) buffer)[i] = input_getc();
        return size;
    }

    struct file *file = get_file_by_fd(fd);
    if (file == NULL) return -1;
    	
	lock_acquire(&global_lock);
    int bytes_read = file_read(file, buffer, size);
	lock_release(&global_lock);

    return bytes_read;
}

int write(int fd, const void *buffer, unsigned size)
{
    if (fd == 1)
    {
        putbuf(buffer, size);
        return size;
    }
    
    struct file *file = get_file_by_fd(fd);
    if (file == NULL) return -1;

    lock_acquire(&global_lock);
    int byte_write = file_write(file, buffer, size);
    lock_release(&global_lock);

    return byte_write;
}

void close(int fd)
{
    struct thread *cur = thread_current();
    struct file *target;

    if ((target = get_file_by_fd(fd)) == NULL) return;

    file_close(target);
    cur->fdt[fd] = NULL;
}

unsigned tell(int fd)
{
    return file_tell(get_file_by_fd(fd));
}

void seek(int fd, unsigned position)
{
    file_seek(get_file_by_fd(fd), position);
}

static void is_valid_pointer(void *ptr)
{
    if (ptr == NULL || is_kernel_vaddr(ptr) || !pml4_get_page(thread_current()->pml4, ptr)) exit(-1);
}

struct file *get_file_by_fd(int fd)
{
    if (fd < 0 || fd >= 20 || (thread_current()->fdt[fd] == NULL)) return NULL;
    return thread_current()->fdt[fd];
}