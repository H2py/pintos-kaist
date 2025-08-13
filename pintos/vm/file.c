/* file.c: Implementation of memory backed file object (mmaped object). */
/* file.c: 메모리 백업 파일 객체의 구현 (메모리 매핑된 객체). */
/* 진성이의 주석 : file-backed-page 인듯 */

#include <string.h>

#include "include/threads/vaddr.h"
#include "include/userprog/process.h"
#include "vm/vm.h"
#include "include/threads/mmu.h"

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

/* 이 구조체는 수정하지 마세요 */
static const struct page_operations file_ops = {
    .swap_in = file_backed_swap_in,
    .swap_out = file_backed_swap_out,
    .destroy = file_backed_destroy,
    .type = VM_FILE,
};

/* 파일 가상 메모리의 초기화 함수 */
void vm_file_init(void)
{
}

/* 파일 백업 페이지를 초기화합니다 */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva)
{
    /* Set up the handler */
    page->operations = &file_ops;

    struct file_page *file_page = &page->file;
	struct lazy_load_data *data = (struct lazy_load_data *)page->uninit.aux;

	file_page->file = data->file;
	file_page->offset = data->ofs;
	file_page->read_bytes = data->page_read_bytes;
	file_page->zero_bytes = data->page_zero_bytes;

    return true; 
}

/* 파일에서 내용을 읽어와 페이지를 스왑 인합니다. */
static bool file_backed_swap_in(struct page *page, void *kva)
{
    struct file_page *file_page UNUSED = &page->file;
}

/* 내용을 파일에 다시 쓰고 페이지를 스왑 아웃합니다. */
static bool file_backed_swap_out(struct page *page)
{
    struct file_page *file_page UNUSED = &page->file;
}

/* 파일 백업 페이지를 파괴합니다. PAGE는 호출자에 의해 해제됩니다. */
static void file_backed_destroy(struct page *page)
{
    struct file_page *file_page UNUSED = &page->file;
	struct thread *cur = thread_current();
	if(pml4_is_dirty(&cur->pml4, page->va))
	{
		file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->offset);
		pml4_set_dirty(&cur->pml4, page->va, true);
	}
	pml4_clear_page(&cur->pml4, page->va);
}

/* mmap을 수행합니다 */
void *do_mmap(void *addr, size_t length, int writable, struct file *file,
              off_t offset)
{
	struct file *f = file_reopen(file);
    struct thread *cur = thread_current();
    struct page *page;

    int read_bytes = file_length(f) < length ? file_length(f) : length;
    int zero_bytes = PGSIZE - read_bytes % PGSIZE; 
	int mmap_page_cnt = (length + PGSIZE - 1) / PGSIZE;

    void *result_addr = addr;

    ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT(pg_ofs(addr) == 0);
    ASSERT(offset % PGSIZE == 0);

    while (read_bytes > 0 || zero_bytes > 0)
    {
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        struct lazy_load_data *data =
            (struct lazy_load_data *) malloc(sizeof(struct lazy_load_data));
        if (data == NULL) return false;

        data->file = f;
        data->ofs = offset;
        data->page_read_bytes = page_read_bytes;
        data->page_zero_bytes = page_zero_bytes;

        if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable,
                                            lazy_load_segment, data))
        {
            free(data);
            return NULL;
        }

        page = spt_find_page(&cur->spt, addr);
        if (!page) return NULL;
		page->mmap_page_cnt = mmap_page_cnt;

        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        offset += page_read_bytes;
        result_addr += PGSIZE;
    }
    return result_addr;
}

/* munmap을 수행합니다 */
void do_munmap(void *addr)
{
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *page = spt_find_page(spt, addr);

	int cnt = page->mmap_page_cnt;
    for (int i = 0; i < cnt; i++) {
		if(page)
			destroy(page);
		addr += PGSIZE;
		page = spt_find_page(spt, addr);
	}
}
