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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

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
/* we can can embed color macros? moving it from tc_functions.c?
 * This break anything? -- fromani 20051015 */
extern void version(void);
extern char *RED;
extern char *GREEN;
extern char *YELLOW;
extern char *BLUE;
extern char *WHITE;
extern char *GRAY;

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
 * tc_mallocz: like tc_malloc, but zeroes all acquired memory before
 *             returning to the caller (this is quite common in 
 *             transcode codebase)
 * tc_free: the companion memory releasing wrapper.
 */

#define tc_malloc(size) \
    _tc_malloc(__FILE__, __LINE__, size)
#define tc_mallocz(size) \
    _tc_mallocz(__FILE__, __LINE__, size)
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
 * _tc_nallocz: do the real work behind tc_mallocz macro
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
void *_tc_mallocz(const char *file, int line, size_t size);

/* Allocate a buffer aligned to the machine's page size, if known.  The
 * buffer must be freed with buffree() (not free()). */

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
 * Precondtiions: ptr is acquired via tc_bufalloc(). Really BAD things
 *                will happen if a buffer acquired via tc_bufalloc()
 *                is released using anything but tc_buffree(), or
 *                vice versa.
 * Postconditions: none
 */
void tc_buffree(void *ptr);

#endif  /* _LIBTC_H */
