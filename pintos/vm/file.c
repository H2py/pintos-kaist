/* file.c: Implementation of memory backed file object (mmaped object). */
/* file.c: 메모리 백업 파일 객체의 구현 (메모리 매핑된 객체). */
/* 진성이의 주석 : file-backed-page 인듯 */

#include "vm/vm.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* 이 구조체는 수정하지 마세요 */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* 파일 가상 메모리의 초기화 함수 */
void
vm_file_init (void) {
}

/* 파일 백업 페이지를 초기화합니다 */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;

	return true; // TODO: 수정 필요
}

/* 파일에서 내용을 읽어와 페이지를 스왑 인합니다. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* 내용을 파일에 다시 쓰고 페이지를 스왑 아웃합니다. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* 파일 백업 페이지를 파괴합니다. PAGE는 호출자에 의해 해제됩니다. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* mmap을 수행합니다 */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
}

/* munmap을 수행합니다 */
void
do_munmap (void *addr) {
}
