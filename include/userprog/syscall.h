#include <stdbool.h>

#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

// syscall function
void halt(void);
void exit(int);
bool create(const char*, unsigned);
bool remove(const char*);
int open(const char*);

#endif /* userprog/syscall.h */
