#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "userprog/syscall.h"
#include <console.h>
#include "userprog/process.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

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


void
syscall_init (void) {
    // lock_init(&filesys_lock);
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
syscall_handler (struct intr_frame *f UNUSED) {

	uint64_t sys_numer = f->R.rax;
	switch(sys_numer){
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit(f->R.rdi);			// status 숫자를 뭘 넣어줘야 하는거지?
			break;
		case SYS_FORK:
			f->R.rax = fork(f->R.rdi);
			break;	
		case SYS_EXEC:
			f->R.rax = exec(f->R.rdi);
			break;	
		case SYS_WAIT:
			f->R.rax = wait(f->R.rdi);
			break;	
		case SYS_CREATE:
			printf("dd");
			break;	
		case SYS_REMOVE:
			printf("dd");
			break;	
		case SYS_OPEN:
			printf("dd");
			break;	
		case SYS_FILESIZE:
			printf("dd");
			break;	
		case SYS_READ:
			f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
			break;	
		case SYS_WRITE:
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;	
		case SYS_SEEK:
			printf("dd");
			break;	
		case SYS_TELL:
			printf("dd");
			break;	
		case SYS_CLOSE:
			close(f->R.rdi);
			break;	
			//projec 3
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

void exit(int status)
{
	struct thread* cur = thread_current();
	cur->exit_status = status;	//exit_status 와 thread의 Status는 다르다.
	thread_exit();
}

tid_t fork (const char *thread_name)
{
    return process_fork(thread_name, &thread_current()->tf);
}


/* Create child process and execute program correspond to cmd_line on it*/
tid_t exec(const char *cmd_line)
{
	tid_t pid = fork(cmd_line);

	if (pid < 0) {
		printf("fork failed");
		return -1;
	}
	else if (pid == 0) {
		// Child process
		process_exec(cmd_line);
	}
}

/* 자식 프로세스가 종료되기를 기다리고, 자식의 종료 상태를 반환*/
int wait(tid_t pid)
{
	if(pid < 0) {
		printf("유효한 pid가 아님\n");
		return -1;	//유효한 PID가 아님.
	}

	struct thread *curr = thread_current();	//부모 프로세스
	struct list_elem *e;
	struct thread *target;

	for (e = list_begin(&curr->child_list); e != list_end(&curr->child_list); e = list_next(e))
	{
		target = list_entry(e, struct thread, c_elem);
		if (pid == target->tid) {
			if(target->is_waited){
				printf("이미 기다리고 있는 자식 프로세스임.\n");
				return -1;
			}
			if(target->status == THREAD_DYING){
				return target->exit_status;
			}
			else{
				target->is_waited = true;
				return process_wait(pid);
			}
		}
	}

	printf("해당 pid를 갖는 자식 프로세스를 찾지 못함.\n");
	return -1;
	
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
        // lock_acquire(&filesys_lock);
		int bytes_read = file_read(file, buffer, size);
        // lock_release(&filesys_lock);
	
		if(bytes_read < 0)
			return -1; 
		else if(bytes_read == 0)
			return 0; 
		return bytes_read;
	}
} 

int write(int fd, const void *buffer, unsigned size)
{

	if(fd < 0 || fd > 63){
		printf("%d is not right fd value\n",fd);
		return -1;
	}

	struct thread *cur = thread_current();
	struct file *file = cur->fdt[fd];

	// if(file == NULL)
	// 	return -1; 
	
    if(fd == 1) {
		putbuf(buffer, size);
		return size;
	}
    else {
        // lock_acquire(&filesys_lock);
        int byte_write = file_write(file, buffer, size); // If error occurs use the int32_t
        // lock_release(&filesys_lock);

        if(byte_write < 0) return -1;
        else if (byte_write == 0) return 0;
        return byte_write;		
    }
    
    printf("not yet file descriptors \n");
    return -1;
}

/* Fix */
int close(int fd)
{
	if(fd < 0 || fd > 63)
		return -1;

	struct thread *cur = thread_current();
	if(cur->fdt[fd] == NULL)
		return -1; 
    
    // lock_acquire(&filesys_lock);
	file_close(cur->fdt[fd]);
    // lock_release(&filesys_lock);
	cur->fdt[fd] = NULL;

	if(fd == cur->next_fd - 1)
		cur->next_fd--;

	return 0; 
}


static bool is_valid_pointer(void * ptr){
	if(ptr == NULL) return false;
	if(!is_user_vaddr(ptr)) return false;
	// mapping 된 메모리 영역을 가리키는지 확인하는 함수가 필요함
	return true;
}