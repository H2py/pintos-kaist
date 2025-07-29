#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/thread.h"

void syscall_init (void);
void halt(void);
void exit(int status);
tid_t exec(const char *cmd_line);
int wait(tid_t pid);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
tid_t fork (const char *thread_name);
unsigned tell (int fd);
bool create(const char *file, unsigned initial_size);
int open(const char *file);
int filesize(int fd);
void close(int fd);
bool remove(const char *file);
struct file *get_file_by_fd(int fd);
extern struct lock global_lock;
#endif /* userprog/syscall.h */
