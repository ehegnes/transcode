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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

void tc_error(char *fmt, ...);
void tc_info(char *fmt, ...);
void tc_warn(char *fmt, ...);

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

int tc_test_program(char *name);

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

int tc_test_string(char *file, int line, int limit, long ret, int errnum);

/*
 * Allocate a buffer aligned to the machine's page size, if known.  The
 * buffer must be freed with buffree() (not free()).
 */

#define tc_bufalloc(size) \
            _tc_bufalloc(__FILE__, __LINE__, size)

/*
 * _tc_bufalloc:
 *     do the real work behind _tc_bufalloc macro
 *
 * Parameters:
 *     file: name of the file on which call occurs
 *     line: line of above file on which call occurs
 *           (above two parameters are intended to be, and usually
 *           are, filled by tc_malloc macro)
 *     size: size of desired chunk of memory
 * Return Value:
 *     a pointer of acquired, aligned, memory, or NULL if acquisition fails
 * Side effects:
 *     a message is printed on stderr (20051017)
 * Preconditions:
 *     file param not null
 */
void *_tc_bufalloc(const char *file, int line, size_t size);

/*
 * tc_buffree:
 *     release a memory buffer acquired using tc_bufalloc
 *
 * Parameters:
 *     ptr: pointer obtained as return value of a succesfull
 *          tc_bufalloc() call
 * Return Value:
 *     none
 * Preconditions:
 *     ptr is acquired via tc_bufalloc(). Really BAD things will happen
 *     if a buffer acquired via tc_bufalloc() is released using anything
 *     but tc_buffree(), or vice versa.
 */
void tc_buffree(void *ptr);

#ifdef __cplusplus
}
#endif

#endif  /* _LIBTC_H */
