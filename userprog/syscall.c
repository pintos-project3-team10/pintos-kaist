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
#include "include/threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "include/userprog/process.h"
#include "threads/palloc.h"
#include <stdlib.h>

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

struct lock filesys_lock;


void
syscall_init (void) {
	lock_init(&filesys_lock);

	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}


void check_address(void *addr) {
	if (is_kernel_vaddr(addr) || pml4_get_page(thread_current()->pml4, addr) == NULL) 
		exit(-1);
}

void check_fd(int fd, struct thread* cur_thread) {
	if (fd < 0 || fd >= 128) {
		exit(-1);
	}
	if (cur_thread->fd_table[fd] == NULL) {
		exit(-1);
	}

}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f) {
	// TODO: Your implementation goes here.

	// rax에서 syscall 번호
	// rsp 주소와 인자가 가리키는 주소가 유저 영역인지 확인

	check_address(f->rsp);

	uint64_t sysnum = f->R.rax;
	// thread_current()->user_tf = *f;
	memcpy(&thread_current()->user_tf, f, sizeof(struct intr_frame));

	switch(sysnum) {
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			// printf("exit %d\n", thread_current()->tid);
			exit(f->R.rdi);
			// thread_exit ();
			break;
		case SYS_CREATE:
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;
		case SYS_REMOVE:
			f->R.rax = remove(f->R.rdi);
			break;
		case SYS_OPEN:
			f->R.rax = open(f->R.rdi);
			break;
		case SYS_CLOSE:
			close(f->R.rdi);
			break;
		case SYS_FILESIZE:
			f->R.rax = filesize(f->R.rdi);
			break;
		case SYS_READ:
			f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_WRITE:
			// printf("%s", f->R.rsi);
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_SEEK:
			seek(f->R.rdi, f->R.rsi);
			break;
		case SYS_TELL:
			f->R.rax = tell(f->R.rdi);
			break;
		case SYS_FORK:
			f->R.rax = fork(f->R.rdi);
			break;
		case SYS_EXEC:
			f->R.rax = exec(f->R.rdi);
			break;
		case SYS_WAIT:
			// printf("wait %d\n", f->R.rdi);
			f->R.rax = wait(f->R.rdi);
			break;
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
	cur_thread->exit_status = status;
	printf("%s: exit(%d)\n", cur_thread->name, status);
	thread_exit();
}

bool create(const char *file, unsigned initial_size) {
	// file을 이름으로 하고, 크기가 initial_size인 새로운 파일 생성
	// 성공적으로 파일이 생성되었다면 Ture, 실패했다면 false 반환
	check_address(file);
	return filesys_create(file, initial_size);
}

bool remove(const char *file) {
	// file이라는 이름을 가진 파일을 삭제한다.
	// 성공적으로 삭제했다면 True, 실패했다면 False
	check_address(file);
	return filesys_remove(file);
}

int open(const char *file) {
	// file이라는 이름을 가진 파일을 연다.
	// 성공적으로 열렸다면, 파일 식별자로 불리는 비음수 정수를 반환. 실패했다면 -1 반환.
	// 0번, 1번 파일식별자는 이미 역할이 정해져 있다. 0: 표준 입력(STDIN_FILENO), 1: 표준 출력(STDOUT_FILENO)
	check_address(file);

	struct thread *cur_thread = thread_current();
	lock_acquire(&filesys_lock);
	struct file *file_obj = filesys_open(file);
	if (file_obj == NULL) {
		lock_release(&filesys_lock);
		return -1;
	}

	cur_thread->fd_table[cur_thread->max_fd++] = file_obj;
	lock_release(&filesys_lock);
	return cur_thread->max_fd-1;
}

void close(int fd) {
	// fd에 해당하는 file 찾기
	// 메모리 해제..
	struct thread *cur_thread = thread_current();
	check_fd(fd, cur_thread);

	file_close(cur_thread->fd_table[fd]);
	cur_thread->fd_table[fd] = NULL;
}

int filesize(int fd) {
	struct thread *cur_thread = thread_current();

	check_fd(fd, cur_thread);

	int size = file_length(cur_thread->fd_table[fd]);
	return size;
}

int read(int fd, void *buffer, unsigned size) {
	// fd에 해당하는 파일에서 읽어서 buffer에 size 만큼 저장
	// 실제 읽어낸 바이트 수 반환
	// 파일 끝에서 시도하면 0
	// 파일이 읽어질 수 없다면 -1 (파일 끝이라서가 아닌 다른 조건 때문에 못 읽은 경우)

	// 성공 시 읽은 바이트 수를 반환, 실패 시 0을 반환
	// buffer: 읽은 데이터를 저장할 버퍼의 주소 값, size: 읽을 데이터 크기
	// fd 값이 0일 때 키보드의 데이터를 읽어 버퍼에 저장. (input_getc())

	if (fd == 0) {
		char *buf = buffer;
		int count = 0;
		for(int i=0;i<size;i++) {
			*buf = input_getc();
			count++;
			if (*buf == '\0') {
				break;
			}
			buf++;
		}

		return count;
	}

	struct thread *cur_thread = thread_current();

	check_fd(fd, cur_thread);
	check_address(buffer);


	lock_acquire(&filesys_lock);
	int read_size = file_read(cur_thread->fd_table[fd], buffer, size);
	lock_release(&filesys_lock);

	return read_size;
}

int write(int fd, const void *buffer, unsigned size) {
	// 열린 파일의 데이터를 기록 시스템 콜
	// 성공 시 기록한 데이터의 바이트 수를 반환, 실패시 -1 반환
	// buffer 기록 할 데이터를 저장한 버퍼의 주소값, size: 기록할 데이터 크기
	// fd 값이 1일 때, 버퍼에 저장된 데이터를 화면에 출력. (putbuf())

	if(fd == 1) {
		putbuf(buffer, size);
		return size;
	}

	struct thread *cur_thread = thread_current();

	check_fd(fd, cur_thread);
	check_address(buffer);

	lock_acquire(&filesys_lock);
	int write_size = file_write(cur_thread->fd_table[fd], buffer, size);
	lock_release(&filesys_lock);

	return write_size;
}

void seek(int fd, unsigned position) {
	struct thread *cur_thread = thread_current();
	check_fd(fd, cur_thread);

	// lock_acquire(&filesys_lock);
	file_seek(cur_thread->fd_table[fd], position);
	// lock_release(&filesys_lock);
}

unsigned tell(int fd) {
	struct thread *cur_thread = thread_current();
	check_fd(fd, cur_thread);
	return file_tell(cur_thread->fd_table[fd]);
}

int fork(const char *thread_name) {
	return process_fork(thread_name, &thread_current()->user_tf); 
}

int wait(int pid) {
	return process_wait(pid);
}

int exec(const char* cmd_line) {
	check_address(cmd_line);
	char * filename = palloc_get_page(PAL_USER);
	memcpy(filename, cmd_line, strlen(cmd_line)+1);

	int result = process_exec(filename);
	// sema_down(&thread_current()->exec_sema);
	if (result == -1) {
		exit(-1);
	}
	return result;
}