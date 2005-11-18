/*
 *  libtc.h
 *
 *  Copyright (C) Thomas Östreich - August 2003
 *
 *  This file is part of transcode, a video stream processing tool
 *
 *  transcode is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  transcode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _LIBTC_H
#define _LIBTC_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifndef SYS_BSD
# ifdef HAVE_MALLOC_H
# include <malloc.h>
# endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* colors macros */
#define COL(x)              "\033[" #x ";1m"
#define COL_RED             COL(31)
#define COL_GREEN           COL(32)
#define COL_YELLOW          COL(33)
#define COL_BLUE            COL(34)
#define COL_WHITE           COL(37)
#define COL_GRAY            "\033[0m"


#define TC_LOG_ERR		0 // critical error condition
#define TC_LOG_WARN		1 // non-critical error condition
#define TC_LOG_INFO		2 // informative highlighted message
#define TC_LOG_MSG		3 // regular message

void tc_log(int level, const char *tag, const char *fmt, ...);

/* compatibility macros */
#define tc_error(format, args...) \
    do { tc_log(TC_LOG_ERR, PACKAGE, format , ## args); exit(1); } while(0)
#define tc_info(format, args...) \
    tc_log(TC_LOG_INFO, PACKAGE, format , ## args)
#define tc_warn(format, args...) \
    tc_log(TC_LOG_WARN, PACKAGE, format , ## args)

/* macro goodies */
#define tc_log_error(tag, format, args...) \
    tc_log(TC_LOG_ERR, tag, format , ## args)
#define tc_log_info(tag, format, args...) \
    tc_log(TC_LOG_INFO, tag, format , ## args)
#define tc_log_warn(tag, format, args...) \
    tc_log(TC_LOG_WARN, tag, format , ## args)

/* Provided by caller */
extern void version(void);

/*
 * Find program <name> in $PATH
 * returns 0 if found, ENOENT if not and the value of errno of the first
 * occurance if found but not accessible.
 */
int tc_test_program(const char *name);

/* guess the frame rate code from the frames per second */
int tc_guess_frc(double fps);

/*
 * Safer string functions from OpenBSD, since these are not in all
 * libc implementations.
 */

#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t size);
#endif

#ifndef HAVE_STRLCAT
size_t strlcat(char *dst, const char *src, size_t size);
#endif

/*
 * Check the return value of snprintf, strlcpy, and strlcat.
 *   return value < 0 is an internal error.
 *   return value >= limit means characters were truncated.
 * Returns 0 if not problems, 1 if error.
 * If error, prints reason.
 */
int tc_test_string(const char *file, int line, int limit, long ret, int errnum);


/*
 * These versions of [v]snprintf() return -1 if the string was truncated,
 * printing a message to stderr in case of truncation (or other error).
 */
#define tc_vsnprintf(buf,limit,format,args...) \
    _tc_vsnprintf(__FILE__, __LINE__, buf, limit, format , ## args)
#define tc_snprintf(buf,limit,format,args...) \
    _tc_snprintf(__FILE__, __LINE__, buf, limit, format , ## args)

int _tc_vsnprintf(const char *file, int line, char *buf, size_t limit,
		  const char *format, va_list args);
int _tc_snprintf(const char *file, int line, char *buf, size_t limit,
		 const char *format, ...);

/*
 * tc_malloc: just a simple wrapper on libc's malloc(), with emits
 *            an additionalwarning, specifying calling context,
 *            if allocation fails
 * tc_zalloc: like tc_malloc, but zeroes all acquired memory before
 *             returning to the caller (this is quite common in
 *             transcode codebase)
 * tc_free: the companion memory releasing wrapper.
 */
#define tc_malloc(size) \
    _tc_malloc(__FILE__, __LINE__, size)
#define tc_zalloc(size) \
    _tc_zalloc(__FILE__, __LINE__, size)
#define tc_free(ptr) \
    free(ptr);

/*
 * _tc_malloc: do the real work behind tc_malloc macro
 *
 * Parameters: file: name of the file on which call occurs
 *             line: line of above file on which call occurs
 *             (above two parameters are intended to be, and usually
 *             are, filled by tc_malloc macro)
 *             size: size of desired chunk of memory
 * Return Value: a pointer of acquired memory, or NULL if acquisition fails
 * Side effects: a message is printed on stderr (20051017)
 * Preconditions: file param not null
 * Postconditions: none
 */
void *_tc_malloc(const char *file, int line, size_t size);

/*
 * _tc_zalloc: do the real work behind tc_zalloc macro
 *
 * Parameters: file: name of the file on which call occurs
 *             line: line of above file on which call occurs
 *             (above two parameters are intended to be, and usually
 *             are, filled by tc_malloc macro)
 *             size: size of desired chunk of memory
 * Return Value: a pointer of acquired memory, or NULL if acquisition fails
 * Side effects: a message is printed on stderr (20051017)
 * Preconditions: file param not null
 * Postconditions: if call succeed, acquired memory contains all '0'
 */
void *_tc_zalloc(const char *file, int line, size_t size);

/*
 * Allocate a buffer aligned to the machine's page size, if known.  The
 * buffer must be freed with buffree() (not free()).
 */

#define tc_bufalloc(size) \
    _tc_bufalloc(__FILE__, __LINE__, size)

void *_tc_bufalloc(const char *file, int line, size_t size);

/*
 * tc_buffree: release a memory buffer acquired using tc_bufalloc
 *
 * Parameters: ptr: pointer obtained as return value of a succesfull
 *                  tc_bufalloc() call
 * Return Value: none
 * Side effects: none
 * Preconditions: ptr is acquired via tc_bufalloc(). Really BAD things
 *                will happen if a buffer acquired via tc_bufalloc()
 *                is released using anything but tc_buffree(), or
 *                vice versa.
 * Postconditions: none
 */
void tc_buffree(void *ptr);

/*
 * tc_strdup: a macro wrapper on top of _tc_strndup, like tc_malloc, above
 * tc_strndup: like tc_strdup, but copies only N byte of given string
 *
 * This function does the same thing of libc's standard functions
 * strdup(3) an strndup(3), but using the libtc's tc_malloc features.
 */
#define tc_strdup(s) \
    _tc_strndup(__FILE__, __LINE__, s, strlen(s))
#define tc_strndup(s, n) \
    _tc_strndup(__FILE__, __LINE__, s, n)

/*
 * _tc_strndup: do the real work behind tc_strdup/tc_strndup macro.
 *              this function adds automatically and implicitely a
 *              '\0' terminator at end of copied string.
 *
 * Parameters: file: name of the file on which call occurs
 *             line: line of above file on which call occurs
 *             (above two parameters are intended to be, and usually
 *             are, filled by tc_malloc macro)
 *             s: null-terminated string to copy
 *             n: copy at most 'n' characters of original string.
 * Return Value: a pointer to a copy of given string.
 *               this pointer must be freed using tc_free() to avoid
 *               memory leaks
 * Side effects: a message is printed on stderr (20051017)
 * Preconditions: file param not null
 * Postconditions: none
 */
char *_tc_strndup(const char *file, int line, const char *s, size_t n);

/*
 * tc_file_check: verify the type of a given file (path)
 *                this function will be deprecated very soon,
 *                replaced by a powered tc_probe_path().
 *
 * Parameters: file: the file (really: path) to verify.
 * Return Value: -1 if an internal error occur
 *               0 if given path is really a file
 *               1 if given path is a directory
 * Side effects: none
 * Preconditions: none
 * Postconditions: none
 */
int tc_file_check(const char *file);

/*
 * tc_pread: read an entire buffer from a file descriptor, restarting
 *           automatically if interrupted.
 *           This function is basically a wrapper around posix read(2);
 *           read(2) can be interrupted by a signal, so doesn't guarantee
 *           that all requested bytes are effectively readed when read(2)
 *           returns; this function ensures so, except for critical errors.
 * Parameters: fd: read data from this file descriptor
 *             buf: pointer to a buffer which will hold readed data
 *             len: how much data function must read from fd
 * Return Value: size of effectively readed data
 * Side effects: errno is readed internally
 * Preconditions: none
 * Postconditions: read exactly the requested bytes, if no *critical*
 *                 (tipically I/O related) error occurs.
 */
ssize_t tc_pread(int fd, uint8_t *buf, size_t len);

/*
 * tc_pwrite: write an entire buffer from a file descriptor, restarting
 *            automatically if interrupted.
 *            This function is basically a wrapper around posix write(2);
 *            write(2) can be interrupted by a signal, so doesn't guarantee
 *            that all requested bytes are effectively writed when write(2)
 *            returns; this function ensures so, except for critical errors.
 * Parameters: fd: write data on this file descriptor
 *             buf: pointer to a buffer which hold data to be written
 *             len: how much data function must write in fd
 * Return Value: size of effectively written data
 * Side effects: errno is readed internally
 * Preconditions: none
 * Postconditions: write exactly the requested bytes, if no *critical*
 *                 (tipically I/O related) error occurs.
 */
ssize_t tc_pwrite(int fd, uint8_t *buf, size_t len);

/*
 * tc_preadwrite: read all data avalaible from a file descriptor, putting
 *                it on the other one.
 * Parameters: in: read data from this file descriptor
 *             out: write readed data on this file descriptor
 * Return Value: -1 if a read error happens
 *               0 if no error happens
 * Side effects: none
 * Preconditions: none
 * Postconditions: move the entire content of 'in' into 'out',
 *                 if no *critical* (tipically I/O related) error occurs.
 */
int tc_preadwrite(int in, int out);

#define TC_PROBE_PATH_INVALID	0
#define TC_PROBE_PATH_ABSPATH	1
#define TC_PROBE_PATH_RELDIR	2
#define TC_PROBE_PATH_FILE	3
#define TC_PROBE_PATH_NET	4
#define TC_PROBE_PATH_BKTR	5
#define TC_PROBE_PATH_SUNAU	6
#define TC_PROBE_PATH_V4L_VIDEO	7
#define TC_PROBE_PATH_V4L_AUDIO	8
#define TC_PROBE_PATH_OSS	9

/*
 * tc_probe_path: verify the type of a given path.
 *
 * Parameters: path: the path to probe.
 * Return Value: the probed type of path. Can be
 *               TC_PROBE_PATH_INVALID if given path
 *               doesn't exists or an internal error occur.
 * Side effects: if function fails, one or more debug message
 *               can be issued using tc_log*().
 *               A name resolve request can be issued to system.
 * Preconditions: none
 * Postconditions: none
 */
int tc_probe_path(const char *name);


#ifdef __cplusplus
}
#endif

#endif  /* _LIBTC_H */
