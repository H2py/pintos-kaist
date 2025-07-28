#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef VM
#include "vm/vm.h"
#endif

static void process_cleanup (void);
static bool load (const char *file_name, struct intr_frame *if_);
static void initd (void *f_name);
static void __do_fork (void *);


/* General process initializer for initd and other process. */
static void
process_init (void) {
	struct thread *current = thread_current ();
	// 향후 추가될 수 있는 초기화 작업들:
    // 1. 프로세스별 데이터 구조 초기화
    // 2. 시그널 핸들러 설정
    // 3. 환경 변수 설정
    // 4. 파일 디스크립터 테이블 초기화
    // 5. 프로세스별 설정 로드
    // 6. 보안 컨텍스트 초기화
    // 7. 프로세스별 통계 초기화
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
 * The new thread may be scheduled (and may even exit)
 * before process_create_initd() returns. Returns the initd's
 * thread id, or TID_ERROR if the thread cannot be created.
 * Notice that THIS SHOULD BE CALLED ONCE. */
tid_t
process_create_initd (const char *file_name) {
	char *fn_copy;
	tid_t tid;
	char * file_token;	// FIX file_token 변수 : 스레드 생성 시 스레드명을 파일명으로 동일하게 넣어주기 위한 token 변수
	char * save_ptr;

	/* Make a copy of FILE_NAME.
	 * Otherwise there's a race between the caller and load(). */
	
	fn_copy = palloc_get_page (0);	// 실행 파일을 위한 메모리 생성 (memset)
	
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy (fn_copy, file_name, PGSIZE);	//page size - 1(=4KB) 만큼 문자 복사 후 dest 끝에 \n 문자 자동으로 붙임. flie_name -> fn_copy 

	file_token = strtok_r(file_name," ",&save_ptr);

	/* Create a new thread to execute FILE_NAME. */
	tid = thread_create (file_token, PRI_DEFAULT, initd, fn_copy);	
	if (tid == TID_ERROR)
		palloc_free_page (fn_copy);	
	return tid;
}

/* A thread function that launches first user process. */
static void
initd (void *f_name) {
#ifdef VM
	supplemental_page_table_init (&thread_current ()->spt);
#endif

	process_init ();	

	if (process_exec (f_name) < 0)	
		PANIC("Fail to launch initd\n");
	NOT_REACHED ();
}

/* Clones the current process as `name`. Returns the new process's thread id, or
 * TID_ERROR if the thread cannot be created. */
tid_t
process_fork (const char *name, struct intr_frame *if_) {
	tid_t child_tid;
	struct thread * child;
	struct fork_data *data = malloc(sizeof(struct fork_data));

	data->parent = thread_current();
	data->if_ptr = if_;

	/* Clone current thread to new thread.*/
	child_tid = thread_create (name,
			PRI_DEFAULT, __do_fork, data);
	
	if(child_tid == TID_ERROR){
		free(data);
		return TID_ERROR;
	}
	child = list_entry(list_back(&thread_current()->child_list),struct thread,c_elem);

	sema_down(&child->fork_sema);	//child으ㅣ fork sema로 변경
		
	return child_tid;
}

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 * pml4_for_each. This is only for the project 2. */
static bool
duplicate_pte (uint64_t *pte, void *va, void *aux) {
	struct thread *current = thread_current ();
	struct thread *parent = (struct thread *) aux;
	void *parent_page;
	void *newpage;
	bool writable;

	/* 1. TODO: If the parent_page is kernel page, then return immediately. */
	//if(is_kern_pte(pte)) return true;
	if(is_kernel_vaddr(va) || is_kern_pte(pte)) return true;

	/* 2. Resolve VA from the parent's page map level 4. */
	parent_page = pml4_get_page (parent->pml4, va); // parent의 pml4d에서 가상 메모리랑 대응되는 물리 주소를 가져온다
	if(parent_page == NULL) return false;

	/* 3. TODO: Allocate new PAL_USER page for the child and set result to NEWPAGE */
	newpage = palloc_get_page(PAL_USER);
	
	if(newpage == NULL) return false;
    
	memcpy(newpage, parent_page, PGSIZE);
	writable = is_writable(pte);

	/* 5. Add new page to child's page table at address VA with WRITABLE
	 *    permission. */
	if (!pml4_set_page (current->pml4, va, newpage, writable)) {
		/* 6. TODO: if fail to insert page, do error handling. */
		palloc_free_page(newpage);
		printf("Duplicate failed to at %p", va);
		return false;
	} 
	return true;
}
#endif

/* A thread function that copies parent's execution context.
 * Hint) parent->tf does not hold the userland context of the process.
 *       That is, you are required to pass second argument of process_fork to
 *       this function. */
static void
__do_fork (void *aux) {
	struct fork_data * fork_aux = (struct fork_data *) aux;

	struct intr_frame if_;
	struct thread *parent = fork_aux->parent;
	struct intr_frame *parent_if = fork_aux->if_ptr;
	free(aux);
	
	struct thread *current = thread_current ();
	/* somehow pass the parent_if. (i.e. process_fork()'s if_) */
	bool succ = true;

	/* 1. Read the cpu context to local stack. */
	memcpy (&if_, parent_if, sizeof (struct intr_frame));
	
	/* 2. Duplicate PT */
	current->pml4 = pml4_create();
	if (current->pml4 == NULL)
		goto error;

	process_activate (current);

#ifdef VM
	supplemental_page_table_init (&current->spt);
	if (!supplemental_page_table_copy (&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each (parent->pml4, duplicate_pte, parent))
		goto error;
#endif

	for(int i = 0; i < 63; i++){
		if(parent->fdt[i]){
			current->fdt[i] = file_duplicate(parent->fdt[i]);
		}
	}

	process_init ();
	
	if_.R.rax = 0;

	/* Finally, switch to the newly created process. */
	if (succ){
		sema_up(&thread_current()->fork_sema);
		do_iret (&if_);
	}
error:
	thread_exit ();
}

int
process_exec (void *f_name) {
    // 1. 파일명 설정
    char *file_name = f_name;
    bool success;

    // 2. 새로운 인터럽트 프레임 준비
    struct intr_frame _if;
    _if.ds = _if.es = _if.ss = SEL_UDSEG;  // 사용자 데이터 세그먼트
    _if.cs = SEL_UCSEG;                    // 사용자 코드 세그먼트
    _if.eflags = FLAG_IF | FLAG_MBS;       // 인터럽트 활성화 + 기본 플래그

    // 3. 현재 컨텍스트 정리
    process_cleanup ();

    // 4. 새로운 바이너리 로드
    success = load (file_name, &_if);

    // 5. 실패 시 처리
    palloc_free_page (file_name);
    if (!success)
        return -1;

    // 6. 새로운 프로세스 시작
    do_iret (&_if);
    NOT_REACHED ();
}
/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */
int
process_wait (tid_t child_tid) {
	struct list_elem *e;
	struct thread * child;
	struct thread * parent = thread_current();
	int child_exit_status = -1;

	for (e = list_begin(&parent->child_list); e != list_end(&parent->child_list); e = list_next(e))
	{
		child = list_entry(e, struct thread, c_elem);
		if (child_tid == child->tid) {
			if(child->is_waited){
				return child_exit_status;
			} else {
				child->is_waited = true;

				sema_down(&child->wait_sema);
				child_exit_status = child->exit_status;
				list_remove(e);
				return child_exit_status;
			}
		}
	}
	return child_exit_status;
}

/* Exit the process. This function is called by thread_exit (). */
void
process_exit (void) {
	struct thread *curr = thread_current ();
	/* Close all open file descriptors. */

	if(curr->pml4 != NULL){
		printf("%s: exit(%d)\n", curr->name, curr->exit_status);
	}

	if(curr->running_file) {
		file_allow_write(curr->running_file);
		file_close(curr->running_file);
		curr->running_file = NULL;
	}

	for(int fd = 3; fd < 64; fd++)
	{
		if(curr->fdt[fd] != NULL) {
			file_allow_write(curr->fdt[fd]);
			file_close(curr->fdt[fd]);
			curr->fdt[fd] = NULL;
		}
	}

	curr->next_fd = 3;

	sema_up(&curr->wait_sema);

	process_cleanup ();
}

/* Free the current process's resources. */
static void
process_cleanup (void) {	//현재 스레드를 위해 할당한 페이지를 해제
	struct thread *curr = thread_current ();

#ifdef VM
	supplemental_page_table_kill (&curr->spt);
#endif

	uint64_t *pml4;
	/* Destroy the current process's page directory and switch back
	 * to the kernel-only page directory. */
	pml4 = curr->pml4;	
	if (pml4 != NULL) {
		/* Correct ordering here is crucial.  We must set
		 * cur->pagedir to NULL before switching page directories,
		 * so that a timer interrupt can't switch back to the
		 * process page directory.  We must activate the base page
		 * directory before destroying the process's page
		 * directory, or our active page directory will be one
		 * that's been freed (and cleared). */
		curr->pml4 = NULL;
		pml4_activate (NULL);
		pml4_destroy (pml4);
	}
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
void
process_activate (struct thread *next) {	//왜 next일까? -> // 현재 스레드에서 다음 스레드로 전환 process_activate(next);  "다음" 스레드를 활성화
	/* Activate thread's page tables. */
	pml4_activate (next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. */
	tss_update (next);
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
struct ELF64_hdr {
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* Abbreviations */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes,
		bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *RIP
 * and its initial stack pointer into *RSP.
 * Returns true if successful, false otherwise. */
static bool
load (const char *file_name, struct intr_frame *if_) {
	// FIX 고정 크기 매개변수 배열 생성 
	static char *argv_addrs[LOADER_ARGS_LEN / 2 + 1];
	static char *argv[LOADER_ARGS_LEN / 2 + 1];

	struct thread *t = thread_current ();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

	// FIX 파일명 / strtok_r 함수용 포인터 변수 2개 생성
	char * target_file;
	char * save_ptr = NULL;
	char * token = NULL;
	long int argc = 0;
	int length;	//argument 문자열의 길이

	/* Allocate and activate page directory. */
	t->pml4 = pml4_create ();	//새로운 페이지 할당
	if (t->pml4 == NULL)
		goto done;
	process_activate (thread_current ());	//새로운 스레드의 페이지 테이블 활성화

	token = strtok_r(file_name," ",&save_ptr);

	while(token != NULL){
		argv[argc++] = token;
		token = strtok_r(NULL," ",&save_ptr);
	}
	
	target_file = argv[0];	// 

	/* Open executable file. */
	file = filesys_open (target_file);	//디스크에서 실행 파일을 열어서 읽기 준비
	if (file == NULL) {
		printf ("load: %s: open failed\n", target_file);
		goto done;
	}

	/* Read and verify executable header. */
	//읽고, ELF 헤더 검증
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
			|| memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7)	//ELF 매직 넘버 확인
			|| ehdr.e_type != 2								//실행 파일 타입
			|| ehdr.e_machine != 0x3E // amd64 아키텍처인지
			|| ehdr.e_version != 1		// ELF 버전 확인
			|| ehdr.e_phentsize != sizeof (struct Phdr)	//프로그램 헤더 크기
			|| ehdr.e_phnum > 1024) {	//프로그램 헤더 개수 제한
		printf ("load: %s: error loading executable\n", file_name);
		goto done;
	}

	/* Read program headers. */
	file_ofs = ehdr.e_phoff;	//프로그램 헤더 오프셋
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);	//프로그램 헤더 읽기 위해 오프셋 만큼 파일 포인터 이동

		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)	// 프로그램 헤더 읽기 
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type) {
			case PT_NULL:
			case PT_NOTE:
			case PT_PHDR:
			case PT_STACK:
			default:
				/* Ignore this segment. */
				break;
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_SHLIB:
				goto done;
			case PT_LOAD:
				if (validate_segment (&phdr, file)) {
					bool writable = (phdr.p_flags & PF_W) != 0;
					uint64_t file_page = phdr.p_offset & ~PGMASK;
					uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint64_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					if (phdr.p_filesz > 0) {
						/* Normal segment.
						 * Read initial part from disk and zero the rest. */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
								- read_bytes);
					} else {
						/* Entirely zero.
						 * Don't read anything from disk. */
						read_bytes = 0;
						zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
					}
					if (!load_segment (file, file_page, (void *) mem_page,
								read_bytes, zero_bytes, writable))
						goto done;
				}
				else
					goto done;
				break;
		}
	}

	/* Set up stack. */
	if (!setup_stack (if_))
		goto done;

	/* Start address. */
	if_->rip = ehdr.e_entry;

	enum intr_level old_level = intr_disable();
	//argc는 마지막 null 개수까지 포함하고 있다.
	for(i = argc - 1; i >= 0; i--){	//스택에 파싱한 인자 값 저장
		length = strlen(argv[i]) + 1; 	

		if_->rsp -= length;
		argv_addrs[i] = if_->rsp;
		memcpy(if_->rsp, argv[i],(sizeof(char) * length));
	}

	if_->rsp -= if_->rsp % 8;	//padding

	if_->rsp -= sizeof(char*);	//null 삽입
	
	uintptr_t argv_p;
	for(i = argc -1 ; i>=0; i--){	//argv 인자들 메모리 주소 넣기
		if_->rsp -= sizeof(argv_addrs[i]);	
		memcpy(if_->rsp,&argv_addrs[i],sizeof(char *));	
		if(i == 0) argv_p = if_->rsp;
	}

	if_->rsp -= sizeof(char*);
	memcpy(if_->rsp,&argv_p,sizeof(char*));	//argv 주소 넣기

	if_->rsp -= sizeof(long int);
	memcpy(if_->rsp, &argc, sizeof(long int));	//argc 넣기

	if_->rsp -= sizeof(char *); 	//argv 주소 넣기

	if_->R.rsi = argv_p;
	if_->R.rdi = argc;

	file_deny_write(file);
	t->running_file = file;
	intr_set_level(old_level);

	success = true;

done:
	/* We arrive here whether the load is successful or not. */
	file_close (file);
	return success;
}


/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Phdr *phdr, struct file *file) {
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (uint64_t) file_length (file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range. */
	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */

/* load() helpers. */
static bool install_page (void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Get a page of memory. */
		uint8_t *kpage = palloc_get_page (PAL_USER);
		if (kpage == NULL)
			return false;

		/* Load this page. */
		if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
			palloc_free_page (kpage);
			return false;
		}
		memset (kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. */
		if (!install_page (upage, kpage, writable)) {
			printf("fail\n");
			palloc_free_page (kpage);
			return false;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
static bool
setup_stack (struct intr_frame *if_) {
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page (PAL_USER | PAL_ZERO);	//물리 메모리에서 4KB 페이지 할당 0으로 초기화
	if (kpage != NULL) {
		success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);	//USER_STACK 주소에서 - 페이지 크기 => 스택의 맨 아래 페이지 주소(스택은 밑으로 성장하니까)
		if (success)			//kpage에 가상 주소 USER_SATCK - PGSIZE <-> 물리주소 매핑
			if_->rsp = USER_STACK;	//페이지 할당되면, rsp(스택 포인터)가 USER_STACK 주소를 가리킴
		else
			palloc_free_page (kpage);
	}
	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

static bool
lazy_load_segment (struct page *page, void *aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		void *aux = NULL;
		if (!vm_alloc_page_with_initializer (VM_ANON, upage,
					writable, lazy_load_segment, aux))
			return false;

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
static bool
setup_stack (struct intr_frame *if_) {
	bool success = false;
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);

	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* TODO: Your code goes here */

	return success;
}
#endif /* VM */