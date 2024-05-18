/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include <hash.h>
#include <mmu.h> // for pml4_walk

// Frame_Table을 해쉬 테이블이 아닌 연결 리스트로 선언할 예정이기 때문에 Table -> List
struct list frame_list;

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
									vm_initializer *init, void *aux)
{

	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
// 인자로 spt와 가상주소가 들어온다.
// 가상주소를 이용해서 spt내의 특정 page를 찾아내서 반환한다.
struct page *
spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED)
{
	struct page temp_page;
	struct hash_elem *elem_found;

	// 사용자가 요청하는 va는 반드시 page의 시작점이라는 보장이 없기 때문에
	// pg_round_down함수로 페이지 시작점을 찾는다.
	va = pg_round_down(va);

	temp_page.va = va;
	elem_found = hash_find(spt->spt_hash_table, &(temp_page.p_hash_elem));
	return elem_found != NULL ? hash_entry(elem_found, struct page, p_hash_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
// spt에 인자로 들어온 page를 삽입한다.
// 삽입 전에 spt에 va가 이미 존재하는지 확인해야 한다.
bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
					 struct page *page UNUSED)
{
	struct hash_elem *result = NULL;
	if (!spt_find_page(spt, page->va))
		result = hash_insert(spt->spt_hash_table, &(page->p_hash_elem));
	return result != NULL ? true : false;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	vm_dealloc_page(page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim(void)
{
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim UNUSED = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
/* palloc을 kernel pool에서 받게 된다면, 예상치 못하게 테스트 케이스에서 실패할 수 있다.
 반드시 user pool에서 할당받아야 한다.
 이 함수로 모든 유저 공간(user pool) page들을 할당한다.
*/
static struct frame *
vm_get_frame(void)
{
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	// user pool에서 할당받는다.
	frame = palloc_get_page(PAL_USER);

	// 지금은 일단 swap out 구현 전이기 때문에 todo로 표시
	if (!frame)
		PANIC("todo");

	// frame 초기화
	frame->kva = ptov(frame);
	frame->page = NULL;

	// 할당한 frame 을 frame_list에 추가
	list_push_back(&frame_list, &frame->f_elem);

	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
						 bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	return vm_do_claim_page(page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
bool vm_claim_page(void *va UNUSED)
{
	struct page *page = NULL;
	/* TODO: Fill this function */
	page = palloc_get_page(PAL_USER);
	page->va = va;
	// 만들어진 페이지는 spt에 추가
	hash_insert(thread_current()->spt.spt_hash_table, &(page->p_hash_elem));

	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
/* 인자로 주어진 page에 물리 메모리 프레임을 할당
 */
static bool
vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	/*
	page table관련 함수를 사용해서 입력된 page의 시작주소를 frame의 시작주소와 맵핑시킨다.
	1. pte를 찾는다.
	2. pte의 PFN값을 vm_get_frame()함수로 할당한 frame의 kva값을 이용해서 설정한다.
	*/
	uint64_t *pte_found = pml4e_walk(thread_current()->pml4, page->va, 1);
	if (pte_found && (*pte_found & PTE_P))
	{
		pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable);
		// & ~0xFFF 넣을지 고민
		*pte_found = vtop(frame->kva);
		return true;
	}
	return false;
	// if (!pte_found)
	// 	return false;

	// // & ~0xFFF 넣을지 고민
	// *pte_found = vtop(frame->kva) & ~0xFFF;

	// return true;
	/* not yet 아직 구현 안함
	return swap_in(page, frame->kva);
	*/
}

/*  spt 초기화 함수
 initd, __do_fork에서 사용, 즉 프로세스가 새로 생성될 때 사용하기 때문에
 테이블은 유저 영역에 할당한다.
*/
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
	spt->spt_hash_table = palloc_get_page(PAL_USER);
	hash_init(spt->spt_hash_table, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED)
{
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}

/* hash table로 만들어진 spt에서 사용할 hash 함수 */
unsigned
page_hash(const struct hash_elem *p_elem, void *aux UNUSED)
{
	const struct page *p = hash_entry(p_elem, struct page, p_hash_elem);
	return hash_bytes(&p->va, sizeof p->va);
}

/* hash table로 만들어진 spt에서 사용할 hash_comarison 함수 */
bool page_less(const struct hash_elem *p_elem_a,
			   const struct hash_elem *p_elem_b, void *aux UNUSED)
{
	const struct page *a = hash_entry(p_elem_a, struct page, p_hash_elem);
	const struct page *b = hash_entry(p_elem_b, struct page, p_hash_elem);

	return a->va < b->va;
}