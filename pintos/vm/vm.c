/* vm.c: 가상 메모리 객체를 위한 일반적인 인터페이스입니다. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

/* 각 하위 시스템의 초기화 코드를 호출하여 가상 메모리 하위 시스템을 초기화합니다. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* 프로젝트 4를 위한 코드드 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* 위의 라인들은 수정하지 마세요. */
	/* TODO: 여러분의 코드가 여기에 들어갑니다. */
}

/* 페이지의 타입을 가져옵니다. 이 함수는 페이지가 초기화된 후의 타입을 알고 싶을 때 유용합니다.
 * 이 함수는 현재 완전히 구현되어 있습니다. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* 초기화 함수와 함께 대기 중인 페이지 객체를 생성합니다. 페이지를 생성하려면 직접 생성하지 말고
 * 이 함수나 `vm_alloc_page`를 통해 생성하세요. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* upage가 이미 점유되어 있는지 확인합니다. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: 페이지를 생성하고, VM 타입에 따라 초기화 함수를 가져와서
		 * TODO: uninit_new를 호출하여 "uninit" 페이지 구조체를 생성하세요. uninit_new를
		 * TODO: 호출한 후 필드를 수정해야 합니다. */

		/* TODO: 페이지를 spt에 삽입하세요. */
	}
err:
	return false;
}

/* spt에서 VA를 찾아 페이지를 반환합니다. 오류 시 NULL을 반환합니다. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	/* TODO: Fill this function. */
	/* spt에서 va(가상주소)에 대응하는 페이지를 찾는다. */
	/* 실패시 NULL */
	/* TODO: 이 함수를 구현하세요요. */
	struct page *page = NULL;
	struct hash_elem *e;

	page->va = va;
	e = hash_find(&spt->spt_table, &page->h_elem);

	return e != NULL ? hash_entry(e, struct page, h_elem) : NULL;
}

/* 검증과 함께 PAGE를 spt에 삽입합니다. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	/* TODO: Fill this function. */
	/* 해당 페이지를 SPT에 삽입 */
	/* 만약 해당 주소가 존재할 경우 삽입하지 않음 */
	/* TODO: 이 함수를 구현하세요. */
	int succ = true;
	if(!hash_insert(&spt->spt_table, &page->h_elem))
		return succ;

	return !succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* 제거될 구조체 프레임을 가져옵니다. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
    /* TODO: The policy for eviction is up to you. */
	/* TODO: 제거 정책( 진성이의 주석: 페이지 교체 정책인듯 )은 여러분에게 달려있습니다. */

	return victim;
}

/* 한 페이지를 제거하고 해당하는 프레임을 반환합니다.
 * 오류 시 NULL을 반환합니다.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: victim을 스왑 아웃하고 제거된 프레임을 반환하세요. */

	return NULL;
}

/* palloc()을 호출하고 프레임을 가져옵니다. 사용 가능한 페이지가 없으면 페이지를 제거하고
 * 반환합니다. 이 함수는 항상 유효한 주소를 반환합니다. 즉, 사용자 풀 메모리가 가득 차면
 * 이 함수는 사용 가능한 메모리 공간을 얻기 위해 프레임을 제거합니다.*/
static struct frame *
vm_get_frame (void) {
    struct frame *frame = NULL;
    /* TODO: 이 함수를 구현하세요. */

    ASSERT (frame != NULL);
    ASSERT (frame->page == NULL);
    return frame;
}

/* 스택을 확장합니다. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
/* 쓰기 보호된 페이지의 폴트를 처리합니다 */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* 성공 시 true를 반환합니다 */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
    /* TODO: Validate the fault */
    /* TODO: Your code goes here */
	/* TODO: 폴트를 검증하세요 */
	/* TODO: 여러분의 코드가 여기에 들어갑니다 */

	return vm_do_claim_page (page);
}

/* 페이지를 해제합니다.
 * 이 함수는 수정하지 마세요. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* VA에 할당된 페이지를 요청합니다. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* 해당 가상주소에 대한 페이지를 할당 */
	/* 먼저 페이지를 찾고 vm_do_claim_page()를 호출 */
	
	/* TODO: 이 함수를 구현하세요. */
	return vm_do_claim_page (page);
}

/* PAGE를 요청하고 MMU를 설정합니다. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* 주어진 페이지에 물리 프레임을 항당 */
	/* vm_get_frame으로 프레임을 얻고 MMU세팅을 수행 */
	/* 가상주소와 물리 주소간 매핑 테이블에 추가 */
	/* 성공 여부를 true / false로 반환 */
    /* TODO: Insert page table entry to map page's VA to frame's PA. */
	/* TODO: 페이지의 VA를 프레임의 PA에 매핑하기 위해 페이지 테이블 항목을 삽입하세요. */

	return swap_in (page, frame->kva);
}

/* 새로운 보조 페이지 테이블(supplemental_page_table)을 초기화합니다 */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->spt_table, hash_func, hash_less, NULL);
}

/* 보조 페이지 테이블(supplemental_page_table)을 src에서 dst로 복사합니다 */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}


/* Free the resource hold by the supplemental page table */
/* 보조 페이지 테이블(supplemental_page_table)이 보유한 리소스를 해제합니다 */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
    /* TODO: Destroy all the supplemental_page_table hold by thread and
     * TODO: writeback all the modified contents to the storage. */
	/* TODO: 스레드가 보유한 모든 보조 페이지 테이블(supplemental_page_table)을 파괴하고
	 * TODO: 수정된 모든 내용을 저장소에 다시 쓰세요. */
}
