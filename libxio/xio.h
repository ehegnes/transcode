#ifndef __iolib_h
#define __iolib_h

#include <sys/types.h>
#include <sys/stat.h>

int xio_open(const char *pathname, int flags, ...);
ssize_t xio_read(int fd, void *buf, size_t count);
ssize_t xio_write(int fd, const void *buf, size_t count);
int xio_ftruncate(int fd, off_t length);
off_t xio_lseek(int fd, off_t offset, int whence);
int xio_close(int fd);
int xio_fstat(int fd, struct stat *buf);
int xio_lstat(const char *filename, struct stat *buf);
int xio_stat(const char *filename, struct stat *buf);
int xio_rename(const char *oldpath, const char *newpath);

#endif
