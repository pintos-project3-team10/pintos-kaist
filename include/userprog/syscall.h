#include <stdbool.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "filesys/off_t.h"
#include <stddef.h>

#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init(void);

// syscall function
void halt(void);
void exit(int);
bool create(const char *, unsigned);
bool remove(const char *);
int open(const char *);
int filesize(int);
int read(int, void *, unsigned);
int write(int, const void *, unsigned);
void seek(int, unsigned);
unsigned tell(int);
void close(int);
int fork(const char *);
int exec(const char *);
int wait(int);
void *mmap(void *, size_t, int, int, off_t);

#endif /* userprog/syscall.h */
