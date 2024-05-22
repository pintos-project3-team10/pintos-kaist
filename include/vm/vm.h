#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"
#include <hash.h>

enum vm_type
{
	/* page not initialized */
	VM_UNINIT = 0,
	/* page not related to the file, aka anonymous page */
	VM_ANON = 1,
	/* page that realated to the file */
	VM_FILE = 2,
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3,

	/* Bit flags to store state */

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
	VM_MARKER_0 = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
struct page
{
	const struct page_operations *operations;
	void *va;			 /* Address in terms of user space */
	struct frame *frame; /* Back reference for frame */

	/* TODO
	현재는 spt를 hash table로 구현하고 추가적인(supplemental) 정보는 넣지 않은 상태
	앞으로 구현하면서 필요한 경우가 생길 때마다 추가적으로 구현할 예정
	 */
	// SPT에 넣기 위한 hash_elem
	struct hash_elem p_hash_elem;
	// vm_do_claim_page에서 writable 찾는 과정을 줄이기 위해 주가
	int writable;
	int dirty;

	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	union
	{
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;

#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};

/* The representation of "frame" */
struct frame
{
	void *kva;
	struct page *page;
	// TODO : 프레임 관리 인터페이스를 구현하는 과정에서 멤버 추가
	// Frame_Table을 해쉬 테이블이 아닌 연결 리스트로 선언할 예정이기 때문에 Table -> List
	// Frame_List에 들어갈 element
	struct list_elem f_elem;
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
// file.c의 전역변수 file_ops를 본다면, 이 구조체가 인터페이스로 사용된다는 설명을 이해하기 쉽다.
struct page_operations
{
	bool (*swap_in)(struct page *, void *);
	bool (*swap_out)(struct page *);
	void (*destroy)(struct page *);
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in((page), v)
#define swap_out(page) (page)->operations->swap_out(page)
#define destroy(page)                \
	if ((page)->operations->destroy) \
	(page)->operations->destroy(page)

/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. */
// spt 설계
// 해쉬 테이블로 만들고 싶은데
// 거기 들어가는 구조체는 { 페이지 + valid(페이지 폴트용? 필요한가?) + (해쉬 테이블로 들어가야 하니까) hash_elem }
// va가 입력으로 들어오면, 원하는 page가 바로 딱 나오는 hash table
struct supplemental_page_table
{
	struct hash spt_hash_table;
};

#include "threads/thread.h"
void supplemental_page_table_init(struct supplemental_page_table *spt);
bool supplemental_page_table_copy(struct supplemental_page_table *dst,
								  struct supplemental_page_table *src);
void supplemental_page_table_kill(struct supplemental_page_table *spt);
struct page *spt_find_page(struct supplemental_page_table *spt,
						   void *va);
bool spt_insert_page(struct supplemental_page_table *spt, struct page *page);
void spt_remove_page(struct supplemental_page_table *spt, struct page *page);

void vm_init(void);
bool vm_try_handle_fault(struct intr_frame *f, void *addr, bool user,
						 bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer((type), (upage), (writable), NULL, NULL)

bool vm_alloc_page_with_initializer(enum vm_type type, void *upage,
									bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page(struct page *page);
bool vm_claim_page(void *va);
enum vm_type page_get_type(struct page *page);

// 추가
unsigned page_hash(const struct hash_elem *p_elem, void *aux UNUSED);
bool page_less(const struct hash_elem *p_elem_a,
			   const struct hash_elem *p_elem_b, void *aux UNUSED);
void page_kill(struct hash_elem *e, void *aux);

bool is_in_stack_segment(void *addr, uintptr_t user_rsp);
static void vm_stack_growth(void *addr UNUSED);
int get_number_of_needed_page(void *addr);
#endif /* VM_VM_H */
