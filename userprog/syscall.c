#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "include/threads/init.h"
#include <filesys/filesys.h>

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

static bool
check_address(void *addr) {
	if (is_kernel_vaddr(addr) || pml4_get_page(thread_current()->pml4, addr) == NULL) 
		return false;
	return true;
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.

	// rax에서 syscall 번호
	// rsp 주소와 인자가 가리키는 주소가 유저 영역인지 확인

	// if (!is_user_vaddr((*f).rsp)) {
	// 	thread_exit ();
	// }  
	if (!check_address(f->rsp)) {
		thread_exit();
	}

	// printf("-- %d\n", (*f).R.rdi);
	// printf("-- %d\n", f->R.rdi);
	// printf("-- %d\n", f->R.rdi);
	// printf("-- %s\n", f->R.rsi);
	// char *argv = f->R.rsi;
	// uint8_t count = f->R.rdi;
	// for(int i=0;i<count;i++) {
	// 	printf("%s\n", argv[i]);
	// }

	uint64_t sysnum = f->R.rax;

	switch(sysnum) {
		case SYS_HALT:
			// printf("halt!!\n");
			halt();
			break;
		case SYS_EXIT:
			// printf("exit!!\n");
			exit(f->R.rdi);
			thread_exit ();
			break;
		case SYS_CREATE:
			// printf("create!!\n");
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;
		case SYS_REMOVE:
			// printf("remove!!\n");
			f->R.rax = remove(f->R.rdi);
			break;
		case SYS_WRITE: /* Write to a file. */
			/* code */
			printf("%s", f->R.rsi);
			break;
		// case SYS_WRITE:
		// 	printf("write!!\n");
		// 	break;
		default:
			thread_exit ();
	}

	// 유저
	// printf("%d\n", (*f).R.rax);

	// printf ("system call!\n");
}

void halt() {
	power_off();
}

void exit(int status) {
	// 현재 동작중인 유저 프로그램 종료.
	// 커널에 상태를 리턴하면서 종료.
	// 만약 부모 프로세스가 현재 유저 프로그램의 종료를 기다리던 중이라면, 
	// 그 말은 종료되면서 리턴될 그 상태를 기다린다는 것.
	// 상태 = 0 성공. 0이 아닌 값은 에러

	// 현재 프로세스를 종료시키는 시스템 콜
	// 종료 시 '프로세스 이름: exit(status)' 출력. (Process Termination maeesage)
	// 정상적으로 종료시 status 0
	// status 프로그램이 정상적으로 종료됐는지 확인

	// 실행 중인 스레드 구조체를 가져옴
	// 프로세스 종료 메시지 출력
	// 스레드 종료

	struct thread *cur_thread = thread_current();
	printf("%s: exit(%d)\n", cur_thread->name, status);
	thread_exit();
}

bool create(const char *file, unsigned initial_size) {
	// file을 이름으로 하고, 크기가 initial_size인 새로운 파일 생성
	// 성공적으로 파일이 생성되었다면 Ture, 실패했다면 false 반환
	if (!check_address(file)) {
		exit(-1);
	}
	return filesys_create(file, initial_size);
}

bool remove(const char *file) {
	// file이라는 이름을 가진 파일을 삭제한다.
	// 성공적으로 삭제했다면 True, 실패했다면 False
	if (!check_address(file)) {
		exit(-1);
	}

	return filesys_remove(file);
}

int open(const char *file) {
	// file이라는 이름을 가진 파일을 연다.
	// 성공적으로 열렸다면, 파일 식별자로 불리는 비음수 정수를 반환. 실패했다면 -1 반환.
	// 0번, 1번 파일식별자는 이미 역할이 정해져 있다. 0: 표준 입력(STDIN_FILENO), 1: 표준 출력(STDOUT_FILENO)


}