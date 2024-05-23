/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

struct lazy_aux
{
	struct file *file;
	off_t ofs;
	uint8_t *upage;
	uint32_t page_read_bytes;
	uint32_t page_zero_bytes;
	bool writable;
};

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void vm_file_init(void)
{
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva)
{
	/* Set up the handler */
	page->operations = &file_ops;
	struct file_page *file_page = &page->file;

	// struct lazy_aux *la = file_page->la;
	// file_seek(la->file, la->ofs);

	// size_t page_read_bytes = la->page_read_bytes;
	// size_t page_zero_bytes = la->page_zero_bytes;

	// uint64_t kpage = kva;
	// if (kpage == NULL)
	// 	return false;

	// /* Load this page. */
	// if (file_read(la->file, kpage, page_read_bytes) != (int)page_read_bytes)
	// 	return false;
	// memset(kpage + page_read_bytes, 0, page_zero_bytes);

	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page *page, void *kva)
{
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;
	// free(file_page->la);
	// TODO: dirty 확인하고 맞다면 디스크에 쓰기
	if (page->dirty)
	{
	}
}

/* Do the mmap */
void *
do_mmap(void *addr, size_t length, int writable, struct file *file, off_t offset)
{
	void *temp = addr;
	// 읽어야 하는 바이트
	size_t read_bytes;
	if (file_length(file) < length)
		read_bytes = file_length(file);
	else
		read_bytes = length;

	// 0 처리해야하는 바이트
	size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;

	ASSERT(offset % PGSIZE == 0); // 모름

	// 읽을 바이트들이 다 떨어질 때까지 반복
	while (read_bytes > 0 || zero_bytes > 0)
	{

		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct lazy_aux *la = malloc(sizeof(struct lazy_aux));
		la->file = file;
		la->ofs = offset;
		la->upage = temp;
		la->page_read_bytes = page_read_bytes;
		la->page_zero_bytes = page_zero_bytes;
		la->writable = writable;
		bool lazy_load_segment(struct page * page, void *aux);
		// 이거 메모리 해제 언제 하지..?
		if (!vm_alloc_page_with_initializer(VM_FILE, temp, writable, lazy_load_segment, la))
			return NULL;

		// // page 구조체 생성
		// if (!vm_alloc_page(VM_FILE, temp, writable))
		// 	return NULL;

		// page의 file_page 구조체에 정보 저장
		// struct page *p = spt_find_page(&thread_current()->spt, temp);
		// p->file.la = la;

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		temp += PGSIZE;
		offset += page_read_bytes;
	}
	return addr;
}

/* Do the munmap */
void do_munmap(void *addr)
{
}
