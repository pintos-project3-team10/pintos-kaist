/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/mmu.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void vm_anon_init(void)
{
	/* TODO: Set up the swap_disk. */
	swap_disk = NULL;
}

/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva)
{
	/* Set up the handler */
	page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;
	// anon은 초기화시 전부 0이라 할게 없다.

	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in(struct page *page, void *kva)
{
	struct anon_page *anon_page = &page->anon;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out(struct page *page)
{
	struct anon_page *anon_page = &page->anon;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy(struct page *page)
{
	struct anon_page *anon_page = &page->anon;
	// 연결된 프레임 처리
	struct frame *out_frame = page->frame;
	if (out_frame)
	{
		// // 1. 연결된 frame의 실제 공간 해제
		// palloc_free_page(out_frame->kva);

		// 2. frame_list에서 삭제
		list_remove(&out_frame->f_elem);
		// 3. frame 구조체 해제
		free(page->frame);
	}

	free(page->uninit.aux);
	// spt에서 제거
	// 어차피 hash_clear반복문 돌면서 제거함
	// spt_remove_page(&thread_current()->spt, page);
	// pml4에서 제거
	// 어차피 process clean up에서 해준다..? 근데 거기서 못찾게 spt에서 제거하는데?
	// pml4_clear_page(thread_current()->pml4, page->va);
}
