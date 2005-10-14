/*
 *  libtc.h
 *
 *  Copyright (C) Thomas �streich - August 2003
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

#define tc_error(format,args...) \
    tc_tag_error(__FILE__, format , ## args)
#define tc_info(format,args...) \
    tc_tag_info(__FILE__, format , ## args)
#define tc_warn(format,args...) \
    tc_tag_warn(__FILE__, format , ## args)
void tc_tag_error(const char *tag, char *fmt, ...);
void tc_tag_info(const char *tag, char *fmt, ...);
void tc_tag_warn(const char *tag, char *fmt, ...);

/* Provided by caller */
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

#endif  /* _LIBTC_H */
