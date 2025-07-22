#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include <syscall.h>
#include <sys/types.h>
#include <file.h>
#include <console.h>

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
// /*
// 	SYS_HALT,                   /* Halt the operating system. */
// 	SYS_EXIT,                   /* Terminate this process. */
// 	SYS_FORK,                   /* Clone current process. */
// 	SYS_EXEC,                   /* Switch current process. */
// 	SYS_WAIT,                   /* Wait for a child process to die. */
// 	SYS_CREATE,                 /* Create a file. */
// 	SYS_REMOVE,                 /* Delete a file. */
// 	SYS_OPEN,                   /* Open a file. */
// 	SYS_FILESIZE,               /* Obtain a file's size. */
// 	SYS_READ,                   /* Read from a file. */
// 	SYS_WRITE,                  /* Write to a file. */
// 	SYS_SEEK,                   /* Change position in a file. */
// 	SYS_TELL,                   /* Report current position in a file. */
// 	SYS_CLOSE,                  /* Close a file. */

// 	/* Project 3 and optionally project 4. */
// 	SYS_MMAP,                   /* Map a file into memory. */
// 	SYS_MUNMAP,                 /* Remove a memory mapping. */

// 	/* Project 4 only. */
// 	SYS_CHDIR,                  /* Change the current directory. */
// 	SYS_MKDIR,                  /* Create a directory. */
// 	SYS_READDIR,                /* Reads a directory entry. */
// 	SYS_ISDIR,                  /* Tests if a fd represents a directory. */
// 	SYS_INUMBER,                /* Returns the inode number for a fd. */
// 	SYS_SYMLINK,                /* Returns the inode number for a fd. */

// 	/* Extra for Project 2 */
// 	SYS_DUP2,                   /* Duplicate the file descriptor */

// 	SYS_MOUNT,
// 	SYS_UMOUNT,

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	uint64_t sys_no = f->R.rax;

	switch(sys_no){
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			int status = f->R.rdi;
			exit(status);			// status 숫자를 뭘 넣어줘야 하는거지?
			break;
		case SYS_FORK:
			printf("dd");
			break;	
		case SYS_EXEC:
			const char* cmd = f->R.rdi;
			f->R.rax = exec(cmd);
			break;	
		case SYS_WAIT:
			printf("dd");
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
			int fd = f->R.rdi;
			void *buffer = f->R.rsi;
			int size = f->R.rdx;
			f->R.rax = read(fd, buffer, size);
			break;	
		case SYS_WRITE:
			int fd = f->R.rdi;
			void *buffer = f->R.rsi;
			int size = f->R.rdx;
			f->R.rax = write(fd, buffer, size);
			break;	
		case SYS_SEEK:
			printf("dd");
			break;	
		case SYS_TELL:
			printf("dd");
			break;	
		case SYS_CLOSE:
			printf("dd");
			break;	
			//projec 3
		case SYS_MMAP:
			printf("dd");
			break;	
		case SYS_MUNMAP:
			printf("dd");
			break;	
	}
	printf ("system call!\n");
	thread_exit ();
}

void halt(void)
{
	power_off();
}

void exit(int status)
{
	struct thread* cur = thread_current();
	printf("Process %s : exit(%d).", cur->name, status);
	thread_exit();
}

/* Create child process and execute program correspond to cmd_line on it*/
pid_t exec(const char *cmd_line)
{
	pid_t pid = fork();

	if (pid < 0) {
		perror("fork failed");
		return -1;
	}
	else if (pid == 0) {
		// Child process
		process_exec(cmd_line);
	}
}

/* 자식 프로세스가 종료되기를 기다리고, 자식의 종료 상태를 반환*/
int wait(pid_t pid)
{
	struct thread *curr = thread_current();
	struct list_elem *e;
	struct thread *target;
	for (e = list_begin(&curr->child_list); e != list_end(&curr->child_list); e = list_next(e))
	{
		target = list_entry(e, struct thread, c_elem);
		if ((pid == target->tid) && target->status == THREAD_RUNNING) {
			while(target->status != THREAD_RUNNING) {
				
			}
		}
	}
	

}


int read(int fd, void *buffer, unsigned size)
{
	if(fd < 0 || fd > 63)
		return -1;
	
	struct thread *cur = thread_current();
	struct file *file = cur->fdt[fd];		/* 읽어 올 file 가져오기 */
	
	if(file == NULL)
		return -1; 

	if(fd == 0) {							/* stdin에서 읽어옴 */
		for (int i=0; i < size; i++)
			((uint8_t *)buffer)[i] = input_getc(); 
		return size;
	} else {
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
	if(fd < 0 || fd > 63)
		return -1;

	struct thread *cur = thread_current();
	struct file *file = cur->fdt[fd];
	if(file == NULL)
		return -1; 
	
	if(fd == 1) {
		putbuf(buffer, size);
		return size;
	} else {
		int byte_write = file_write(file, buffer, size); // If error occurs use the int32_t

		if(byte_write < 0)
			return -1;
		else if (byte_write == 0)
			return 0;
		return byte_write;		
	}
}

/* Fix */
int close(int fd)
{
	if(fd < 0 || fd > 63)
		return -1;

	struct thread *cur = thread_current();
	if(cur->fdt[fd] == NULL)
		return -1; 
 
	file_close(cur->fdt[fd]);
	cur->fdt[fd] = NULL;

	if(fd == cur->next_fd - 1)
		cur->next_fd--;

	return 0; 
}

