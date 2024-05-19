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

void syscall_entry(void);
void syscall_handler(struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081			/* Segment selector msr */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

struct lock filesys_lock;

void syscall_init(void)
{
	lock_init(&filesys_lock);

	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

void check_address(void *addr)
{
	// if (is_kernel_vaddr(addr) || pml4_get_page(thread_current()->pml4, addr) == NULL)
	if (is_kernel_vaddr(addr) || addr == NULL)
		exit(-1);
}

void check_fd(int fd, struct thread *cur_thread)
{
	if (fd < 0 || fd >= MAX_FILE_SIZE)
	{
		exit(-1);
	}
	if (cur_thread->fd_table[fd] == NULL)
	{
		exit(-1);
	}
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f)
{
	// TODO: Your implementation goes here.

	check_address(f->rsp);

	uint64_t sysnum = f->R.rax;
	memcpy(&thread_current()->user_tf, f, sizeof(struct intr_frame));

	switch (sysnum)
	{
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		exit(f->R.rdi);
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
		f->R.rax = wait(f->R.rdi);
		break;
	default:
		thread_exit();
	}
}

void halt()
{
	power_off();
}

void exit(int status)
{
	struct thread *cur_thread = thread_current();
	cur_thread->exit_status = status;
	printf("%s: exit(%d)\n", cur_thread->name, status);
	thread_exit();
}

bool create(const char *file, unsigned initial_size)
{
	check_address(file);
	return filesys_create(file, initial_size);
}

bool remove(const char *file)
{
	check_address(file);
	return filesys_remove(file);
}

int open(const char *file)
{
	check_address(file);

	struct thread *cur_thread = thread_current();
	if (cur_thread->max_fd >= MAX_FILE_SIZE)
	{
		return -1;
	}

	lock_acquire(&filesys_lock);
	struct file *file_obj = filesys_open(file);
	if (file_obj == NULL)
	{
		lock_release(&filesys_lock);
		return -1;
	}

	cur_thread->fd_table[cur_thread->max_fd++] = file_obj;
	lock_release(&filesys_lock);
	return cur_thread->max_fd - 1;
}

void close(int fd)
{
	struct thread *cur_thread = thread_current();
	check_fd(fd, cur_thread);

	file_close(cur_thread->fd_table[fd]);
	cur_thread->fd_table[fd] = NULL;
}

int filesize(int fd)
{
	struct thread *cur_thread = thread_current();

	check_fd(fd, cur_thread);

	int size = file_length(cur_thread->fd_table[fd]);
	return size;
}

int read(int fd, void *buffer, unsigned size)
{

	if (fd == 0)
	{
		char *buf = buffer;
		int count = 0;
		for (int i = 0; i < size; i++)
		{
			*buf = input_getc();
			count++;
			if (*buf == '\0')
			{
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

int write(int fd, const void *buffer, unsigned size)
{

	if (fd == 1)
	{
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

void seek(int fd, unsigned position)
{
	struct thread *cur_thread = thread_current();
	check_fd(fd, cur_thread);

	file_seek(cur_thread->fd_table[fd], position);
}

unsigned tell(int fd)
{
	struct thread *cur_thread = thread_current();
	check_fd(fd, cur_thread);
	return file_tell(cur_thread->fd_table[fd]);
}

int fork(const char *thread_name)
{
	return process_fork(thread_name, &thread_current()->user_tf);
}

int wait(int pid)
{
	return process_wait(pid);
}

int exec(const char *cmd_line)
{
	check_address(cmd_line);
	char *filename = palloc_get_page(PAL_USER);
	memcpy(filename, cmd_line, strlen(cmd_line) + 1);

	int result = process_exec(filename);
	if (result == -1)
	{
		exit(-1);
	}
	return result;
}