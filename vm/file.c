/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

struct lazy_aux
{
	struct file *file;
	off_t ofs;
	uint8_t *upage;
	uint32_t page_read_bytes;
	uint32_t page_zero_bytes;
	bool writable;
};


static bool
lazy_load_segment(struct page *page, void *aux)
{
	struct lazy_aux *la = aux;
	file_seek(la->file, la->ofs);
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	// VA가 존재한다는 뜻 -> page에 할당된 frame이 존재한다는 뜻 -> page->frame_kva가 존재한다는 뜻
	//
	// ASSERT((la->page_read_bytes + la->page_zero_bytes) % PGSIZE == 0);
	// ASSERT(pg_ofs(la->upage) == 0);
	// ASSERT(la->ofs % PGSIZE == 0);

	size_t page_read_bytes = la->page_read_bytes;
	size_t page_zero_bytes = la->page_zero_bytes;

	// cpu는 오직 va만 사용하고, 변환은 mmu에 전부 맡겨야 하기 때문에 vtop 매크로는 사용하지 않는다.
	// vtop, ptov는 mmu.c 관련 코드 수정에서만 사용하면 될 것 같다.
	uint64_t kpage = page->frame->kva;
	if (kpage == NULL)
		return false;

	/* Load this page. */
	if (file_read(la->file, kpage, page_read_bytes) != (int)page_read_bytes)
		return false;
	memset(kpage + page_read_bytes, 0, page_zero_bytes);

	return true;
}


/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {

	// ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
	// ASSERT(pg_ofs(upage) == 0);
	// ASSERT(ofs % PGSIZE == 0);

	//

	uint32_t read_bytes = length;
	uint32_t zero_bytes = PGSIZE - length % PGSIZE;
	file_reopen(file);
	// core of program loader
	while (read_bytes > 0 || zero_bytes > 0)
	{
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		/*
		여기서 lazy_load_segment함수에서 필요한 aux를 전달해야 한다. 필요한 정보를 한번에 전달하기 위해
		적절한 aux구조체를 만들어야 한다.
		lazy_load_segment는 VM이 활성화되기 전 load_segment함수면 될 것 같다.
		그 함수에서 필요한 aux를 기반으로 aux구조체를 만들자.

		lazy_aux는 할당 받아야 하는가? 받는다면 언제 해제해야 하는가? - 하기 전까지
		*/
		struct lazy_aux *la = malloc(sizeof(struct lazy_aux));
		la->file = file;
		la->ofs = offset;
		la->upage = addr;
		la->page_read_bytes = page_read_bytes;
		la->page_zero_bytes = page_zero_bytes;
		la->writable = writable;

		if (!vm_alloc_page_with_initializer(VM_FILE, addr,
											writable, lazy_load_segment, la))
			return false;

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;
		offset += page_read_bytes;
	}
	return true;

}

/* Do the munmap */
void
do_munmap (void *addr) {
}
