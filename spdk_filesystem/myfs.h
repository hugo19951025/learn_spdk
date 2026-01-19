#ifndef __MYFS_H__
#define __MYFS_H__

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <syscall.h>

int open(const char* pathname, int flags, ...);
int creat(const char* pathname, mode_t mode);

ssize_t read(int fd, void* buf, size_t count);

ssize_t write(int fd, const void* buf, size_t count);

off_t lseek(int fd, off_t offset, int whence);

int close(int fd);


#endif