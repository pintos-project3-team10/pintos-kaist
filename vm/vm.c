/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include <hash.h>
#include "threads/vaddr.h"
#include "threads/mmu.h" //for pml4
#include <string.h>		 //for memcpy
#include "userprog/process.h"

// Frame_Table을 해쉬 테이블이 아닌 연결 리스트로 선언할 예정이기 때문에 Table -> List
struct list frame_list;
struct lock frame_lock;
/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void vm_init(void)
{
	list_init(&frame_list);
	lock_init(&frame_lock);
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
bool vm_alloc_page_with_initializer(enum vm_type type, void *va, bool writable,
									vm_initializer *init, void *aux)
{
	ASSERT(VM_TYPE(type) != VM_UNINIT)
	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, va) == NULL)
	{
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		/* TODO:
			1. page 구조체를 할당
			2. 전달된 vm_ 유형에 따라 적절한 initializer를 선택, 이를 사용해 uninit_new를 호출
				- VM_TYPE 매크로 사용해보자
			3. spt에 만든 page구조체 추가
		*/

		// 1. 구조체 할당
		struct page *new_page = malloc(sizeof(struct page));

		// 2. 타입별로 init을 적절한 initializer로 fetch
		// + uninit_new를 호출
		switch (VM_TYPE(type))
		{
		case VM_ANON:
			uninit_new(new_page, va, init, type, aux, anon_initializer);
			break;
		case VM_FILE:
			uninit_new(new_page, va, init, type, aux, file_backed_initializer);
			break;
		}

		new_page->writable = writable;
		// 3. spt에 추가
		spt_insert_page(spt, new_page);

		return true;
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
	elem_found = hash_find(&spt->spt_hash_table, &(temp_page.p_hash_elem));
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
		result = hash_insert(&spt->spt_hash_table, &(page->p_hash_elem));
	return result != NULL ? true : false;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	// vm_dealloc_page(page);
	if (hash_delete(&spt->spt_hash_table, &page->p_hash_elem))
		return false;
	return true;
}

/* Get the struct frame, that will be  evicted. */
// 희생자 프레임 찾기
static struct frame *
vm_get_victim(void)
{
	// 일단 가장 앞의
	lock_acquire(&frame_lock);
	struct frame *victim = list_entry(list_pop_front(&frame_list), struct frame, f_elem);
	lock_release(&frame_lock);
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
// 찾은 프레임 비우고(swap out) 빈 프레임  반환하기
static struct frame *
vm_evict_frame(void)
{
	// printf("-------\n");
	struct frame *victim = vm_get_victim();
	// 삭제될 페이지와 연결된 페이지 swap out;
	swap_out(victim->page);
	// swap out한 페이지 pml4에 반영
	pml4_clear_page(thread_current()->pml4, victim->page->va);
	return victim;
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
	frame = malloc(sizeof(struct frame));
	// frame을 user pool에서 할당받는다.
	void *kva = palloc_get_page(PAL_USER | PAL_ZERO);

	//	이부분을 지나면 반드시 frame은 할당되어야 한다.
	if (!frame || !kva)
	{
		frame = vm_evict_frame();
		memset(frame->kva, 0, PGSIZE);
		frame->page->frame = NULL;
		frame->page = NULL;
	}
	else
	{
		// frame 초기화
		frame->kva = kva;
		frame->page = NULL;
	}
	// 할당한 frame 을 frame_list에 추가
	lock_acquire(&frame_lock);
	list_push_back(&frame_list, &frame->f_elem);
	lock_release(&frame_lock);

	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);

	return frame;
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
	if (!not_present)
		return false;

	// kernel에서 page fault가 일어나는 경우는 syscall_handler에서 확인하기 때문에 여기서는 확인하지 않는다.
	// 유저의 rsp
	if (user)
		thread_current()->user_rsp = f->rsp;
	uintptr_t f_rsp = f->rsp;

	// 입력된 addr이 스택영역이라면
	if (is_in_stack_segment(addr, f_rsp))
	{
		vm_stack_growth(addr);
		return true;
	}

	/* TODO: Your code goes here */
	if (page = spt_find_page(spt, addr))
		return vm_do_claim_page(page);
	return false;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
	// unint_page를 만들고
	vm_alloc_page(VM_ANON, pg_round_down(addr), true);
	// 프레임 할당
	vm_claim_page(pg_round_down(addr));
}
bool is_in_stack_segment(void *addr, uintptr_t f_rsp)
{
	// 유효한 영역 내인지 확인
	// +8의 이유 : rsp의 아래 8byte
	if (addr > USER_STACK || addr < f_rsp - 8)
		return false;

	// 유저인지 커널인지 확인
	if (f_rsp != thread_current()->user_rsp)
		return false;

	// 최대 크기 제한 1MB 확인
	if (get_number_of_needed_page(addr) * PGSIZE > (1 << 20))
		return false;

	return true;
}

// 필요한 페이지 사이즈 찾는 함수
int get_number_of_needed_page(void *addr)
{
	uint64_t page_aligned_addr = USER_STACK - (uint64_t)addr;
	return (page_aligned_addr + PGSIZE - 1) / PGSIZE;
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
	page = spt_find_page(&thread_current()->spt, va);
	if (page)
		return vm_do_claim_page(page);
	return false;
}

/* Claim the PAGE and set up the mmu. */
/* 인자로 주어진 page에 물리 메모리 프레임을 할당
 */
static bool vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();
	struct thread *current = thread_current();
	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	/*
	page table관련 함수를 사용해서 입력된 page의 시작주소를 frame의 시작주소와 맵핑시킨다.
	1. pte를 찾는다.
	2. pte의 PFN값을 vm_get_frame()함수로 할당한 frame의 kva값을 이용해서 설정한다.
	*/

	// set page 함수 사용 방법
	// 설정이 실패하면 false를 반환
	if (!pml4_set_page(current->pml4, page->va, frame->kva, page->writable))
	{
		palloc_free_page(frame);
		return false;
	}

	// not yet 아직 구현 안함
	return swap_in(page, frame->kva);

	// walk 함수 사용 방법
	// uint64_t *pte_found = pml4e_walk(thread_current()->pml4, page->va, 1);
	// // & ~0xFFF 넣을지 고민
	// if (!pte_found)

	// 	return false;
	// *pte_found = vtop(frame->kva) & ~0xFFF | PTE_P;
	// return swap_in(page, frame->kva);
}

/*  spt 초기화 함수
 initd, __do_fork에서 사용, 즉 프로세스가 새로 생성될 때 사용하기 때문에
 테이블은 유저 영역에 할당한다.
*/
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
	hash_init(&spt->spt_hash_table, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED)
{
	/*
	TODO: src spt를 순회(해쉬 순회)하면서 모든 page들을 dst에서 새로 할당하고 claim해준다.
	그리고 dst에 넣어준다.
	*/
	struct hash_iterator i;

	// 순회
	hash_first(&i, &src->spt_hash_table);
	while (hash_next(&i))
	{
		// 복사할 page
		struct page *src_page = hash_entry(hash_cur(&i), struct page, p_hash_elem);

		// uninit
		if (src_page->operations->type == VM_TYPE(VM_UNINIT))
		{

			struct lazy_aux *copy_aux = calloc(1, sizeof(struct lazy_aux));
			memcpy(copy_aux, src_page->uninit.aux, sizeof(struct lazy_aux));

			if (!vm_alloc_page_with_initializer(page_get_type(src_page), src_page->va,
												src_page->writable, src_page->uninit.init, copy_aux))
				return false;

			// if (!vm_alloc_page_with_initializer(page_get_type(src_page), src_page->va,
			// 									src_page->writable, src_page->uninit.init, src_page->uninit.aux))
			// 	return false;
		}
		// 초기화 후 file
		else if (src_page->operations->type == VM_TYPE(VM_FILE))
		{
			struct lazy_aux *copy_aux = calloc(1, sizeof(struct lazy_aux));
			memcpy(copy_aux, src_page->file.aux, sizeof(struct lazy_aux));
			if (!vm_alloc_page_with_initializer(page_get_type(src_page), src_page->va,
												src_page->writable, lazy_load_segment, copy_aux))
				return false;
			struct page *dst_page = spt_find_page(dst, src_page->va);
			if (!vm_claim_page(dst_page->va))
				return false;
		}
		// 초기화 후 anony
		else
		{
			if (!vm_alloc_page(page_get_type(src_page), src_page->va, src_page->writable))
				return false;

			struct page *dst_page = spt_find_page(dst, src_page->va);
			if (!vm_claim_page(dst_page->va))
				return false;

			memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
		}
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	/*
	spt를 순회하면서 하나씩 dirty비트를 확인한다.
	만약 dirty하다면 디스크에 쓴다.
	확인이 끝나면 메모리 관련된 메모리를 해제한다.
		1. page->operations->destroy(page)호출
			- uninit_destroy와 anon_destroy를 구현해야 한다.
		2. page 메모리 해제
	 */
	hash_clear(spt, page_kill);
}

void page_kill(struct hash_elem *e, void *aux)
{
	// 페이지 찾음
	struct page *src_page = hash_entry(e, struct page, p_hash_elem);
	// 없다면 종료
	if (!src_page)
		return;

	// destroy 호출
	destroy(src_page);

	// page 메모리 해제
	free(src_page);
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