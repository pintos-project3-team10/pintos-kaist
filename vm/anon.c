/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/mmu.h"
#include "bitmap.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static struct bitmap *swap_table;
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
	// swap_disk 영역 연결
	swap_disk = disk_get(1, 1);
	// 디스크를 slot(8 * sector)으로 설정하고, swap_table로 관리
	// 초기 비트들은 flase
	swap_table = bitmap_create((disk_size(swap_disk) + 7) / 8);
	// disk_lock 초기화
}

/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva)
{
	/* Set up the handler */
	page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;
	anon_page->slot_no = -1;

	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in(struct page *page, void *kva)
{
	struct anon_page *anon_page = &page->anon;
	if (anon_page->slot_no == -1)
		return true;
	// dist의 slot에 쓰여진 context 다시 읽어오기
	for (int i = 0; i < 8; i++)
		disk_read(swap_disk, anon_page->slot_no * 8 + i, kva + DISK_SECTOR_SIZE * i);

	// swap 영역에서 올렸기 때문에 다시 돌려놓는다.
	bitmap_set(swap_table, anon_page->slot_no, true);

	// bitmap_flip(swap_table, anon_page->slot_no);
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out(struct page *page)
{
	struct anon_page *anon_page = &page->anon;
	// swap_map에서 빈 공간 찾기
	// bitmap_scan_and_flip 함수 설명
	// 비트밉에서 2번째 인자부터 시작해서, 3번째 인자만큼 연속으로,
	// 4번째 value를 가진 그룹을 찾아서, 시작 idx를 반환bitmap_scan_and_flip(swap_table, 0, 1, false)

	uint64_t start_bitmap_no = bitmap_scan_and_flip(swap_table, 0, 1, false);
	if (start_bitmap_no == BITMAP_ERROR)
		return false;
	anon_page->slot_no = start_bitmap_no;
	// 찾은 index로 disk에서 위치 선택
	for (int i = 0; i < 8; i++)
		disk_write(swap_disk, start_bitmap_no * 8 + i, page->frame->kva + DISK_SECTOR_SIZE * i);

	return true;
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
		// // process_cleanup에서 destroy를 해서 없어도 된다.
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
	// 어차피 따로 없애는 코드가 없기 때문에, process_cleanup에서 destroy를 해서 없어도 된다.
	// pml4_clear_page(thread_current()->pml4, page->va);
}
