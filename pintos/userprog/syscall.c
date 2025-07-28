#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "include/filesys/filesys.h"
#include <console.h>
#include "userprog/process.h"
#include "threads/palloc.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
static struct lock filesys_lock;

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

static bool is_valid_pointer(void *);
static struct lock filesys_lock;

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f) {

	uint64_t sys_numer = f->R.rax;
	switch(sys_numer){
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit(f->R.rdi);			// status 숫자를 뭘 넣어줘야 하는거지?
			break;
		case SYS_FORK:
			if(is_valid_pointer(f->R.rdi)){
				f->R.rax = fork(f->R.rdi);
			}
			else{
				exit(-1);
			}
			break;	
		case SYS_EXEC:
			f->R.rax = exec(f->R.rdi);
			break;	
		case SYS_WAIT:
			f->R.rax = wait(f->R.rdi);
			break;	
		case SYS_CREATE:
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;	
		case SYS_REMOVE:
			f->R.rax = remove(f->R.rdi);
			break;	
		case SYS_OPEN:
			f->R.rax = open(f->R.rdi);
			break;	
		case SYS_FILESIZE:
			f->R.rax = filesize(f->R.rdi);
			break;	
		case SYS_READ:
			f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
			break;	
		case SYS_WRITE:
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;	
		case SYS_SEEK:
			seek(f->R.rdi, f->R.rsi);
			break;	
		case SYS_TELL:
			f->R.rax = tell(f->R.rdi);
			break;	
		case SYS_CLOSE:
			close(f->R.rdi);
			break;	
			//projec 3
		// case SYS_MMAP:
		// 	printf("dd");
		// 	break;	
		// case SYS_MUNMAP:
		// 	printf("dd");
		// 	break;	
	}
	// thread_exit ();
}

void halt(void)
{
	power_off();
}

bool create(const char *file, unsigned initial_size)
{
    if(file == NULL) exit(-1);

    return filesys_create(file, initial_size);
}

bool remove(const char *file)
{
    if (file == NULL) exit(-1);

    return filesys_remove(file);
}

int open(const char *file)
{
	int ret = -1;
    struct thread *cur = thread_current();

    if(!is_valid_pointer(file)){
		exit(-1);
		return ret;
	}

    struct file *f = filesys_open(file);
    if(f == NULL){
		cur->exit_status = -1;
		return ret;
	}
    else {
        for(int fd = 3; fd < 63; fd++) {
            if(get_file_by_fd(fd) == NULL) {
                cur->next_fd = fd;
				ret = fd;
				cur->fdt[cur->next_fd] = f;
                break;
            }
        }
    }
    return ret;
}

int filesize(int fd)
{
	if(get_file_by_fd(fd) == NULL){
		
	}
    return file_length(get_file_by_fd(fd));
}

void exit(int status)
{
	struct thread* cur = thread_current();
	cur->exit_status = status;	//exit_status 와 thread의 Status는 다르다.
	thread_exit();
}

tid_t fork (const char *thread_name)
{
	tid_t pid;
	
	struct intr_frame *if_ = pg_round_up(&thread_name) - sizeof(struct intr_frame);

	pid = process_fork(thread_name, if_);
    return pid;
}


/* Create child process and execute program correspond to cmd_line on it*/
tid_t exec(const char *cmd_line)
{
	int result;
	if(!is_valid_pointer(cmd_line)){
		exit(-1);
	}

	char *file_copy = palloc_get_page(PAL_ZERO);
	if(file_copy) {
		strlcpy(file_copy, cmd_line, PGSIZE);
		result = process_exec(file_copy);
		palloc_free_page(file_copy);
		return result;
	}


	// tid_t pid;
	// if((pid = fork(cmd_line)) > 0){
	// 	exit_status = process_wait(pid);
	// 	printf("\n%d\n",exit_status);
	// }
	// else if(pid < 0){
	// 	return -1;
	// }
	// else{
	// 	if(process_exec(cmd_line) < 0){
	// 		return -1;
	// 	}
	// }
}

/* 자식 프로세스가 종료되기를 기다리고, 자식의 종료 상태를 반환*/
int wait(tid_t pid)
{
	if(pid < 0)
		exit(-1);

	return process_wait(pid);
}

int read(int fd, void *buffer, unsigned size)
{
    
	if(fd < 0 || fd > 63)
		return -1;
	
	struct thread *cur = thread_current();
	struct file *file = cur->fdt[fd];		/* 읽어 올 file 가져오기 */
	if(file == NULL)
		return -1;
	// TODO : 읽기 권한이 있는지 체크
	// TODO : Buffer의 크기와 size 간에 관계에 대한 조건을 체크해야 되는지 확인하기

	if(fd == 0) {							/* stdin에서 읽어옴 */
		for (int i=0; i < size; i++)
			((uint8_t *)buffer)[i] = input_getc(); 
		return size;
	} 
	else {
		int bytes_read = file_read(file, buffer, size);
	
		if(bytes_read < 0)
			return -1; 
		else if(bytes_read == 0)
			return 0;
		return bytes_read;
	}
}

int write(int fd, const void *buffer, unsigned size)
{

	struct thread *cur = thread_current();
	struct file *file = cur->fdt[fd];


	if(fd < 0 || fd > 63){
		cur->exit_status = -1;
		return -1;
	}

    if(fd == 1) {
		putbuf(buffer, size);
		return size;
	}
	else if(file == NULL){
		cur->exit_status = -1;
		return -1;
	} 
    else {
        int byte_write = file_write(file, buffer, size); // If error occurs use the int32_t

        if(byte_write < 0){
			cur->exit_status = -1;
			return -1;
		} 
        else if (byte_write == 0) return 0;
        return byte_write;		
    }
    
    return -1;
}

/* Fix */
void close(int fd)
{    
    struct thread *cur = thread_current();
	struct file * target;
	
	if((target = get_file_by_fd(fd)) == NULL){
		cur->exit_status = -1;
		return ;
	}
	file_close(target);
	cur->fdt[fd] = NULL;

	if(fd == cur->next_fd - 1)
		cur->next_fd--;
}

unsigned tell (int fd)
{
    return file_tell(get_file_by_fd(fd));
}

void seek(int fd, unsigned position)
{
    file_seek(get_file_by_fd(fd), position);
}

static bool is_valid_pointer(void * ptr){
	struct thread *cur = thread_current();
	if(ptr == NULL) return false;
	if(is_kernel_vaddr(ptr)) return false;
	if(!pml4_get_page(cur->pml4, ptr)) return false; // 현재 프로세스의 페이지 테이블에서 ptr이 가리키는 페이지가 존재하는지 확인
    
	return true;
}

struct file *get_file_by_fd(int fd)
{
    if(fd < 0 || fd > 63)
        return NULL;
    struct thread *cur = thread_current();
    
    if(cur->fdt[fd] == NULL)
		return NULL; 

    return cur->fdt[fd];
}