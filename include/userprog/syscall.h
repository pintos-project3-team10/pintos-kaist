#include <stdbool.h>
#include "threads/interrupt.h"
#include "threads/synch.h"


#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

// syscall function
void halt(void);
void exit(int);
bool create(const char*, unsigned);
bool remove(const char*);
int open(const char*);
int filesize(int);
int read(int , void *, unsigned);
int write(int , const void *, unsigned);
void seek(int, unsigned);
unsigned tell(int);
void close(int);
int fork(const char*);
int exec(const char*);
int wait(int );

#endif /* userprog/syscall.h */
