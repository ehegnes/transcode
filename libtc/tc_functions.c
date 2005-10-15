/*
 *  tc_functions.c 
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

#include "libtc.h"
#include "tc_func_excl.h"

#if defined(HAVE_ALLOCA_H)
#include <alloca.h>
#endif

/* local colors macro; get COL(x) macro from tc_func_excl.h */
#define COL_RED 		COL(31)
#define COL_GREEN		COL(32)
#define COL_YELLOW		COL(33)
#define COL_BLUE		COL(34)
#define COL_WHITE		COL(37)
#define COL_GRAY		"\033[0m"

#define TC_MSG_BUF_SIZE		(128)

const char *tc_log_preambles[] = {
  	"["COL_RED"%s"COL_GRAY"]"COL_RED" critical"COL_GRAY": %s\n",
	"["COL_RED"%s"COL_GRAY"]"COL_YELLOW" warning"COL_GRAY": %s\n", 
  	"["COL_BLUE"%s"COL_GRAY"] %s\n", 
	"[%s] %s\n",
};

void tc_log(int level, const char *tag, const char *fmt, ...)
{
  char buf[TC_MSG_BUF_SIZE];
  char *a = buf;
  int dynbuf = 0; // flag
  va_list ap;
  size_t size = 0;

  // sanity check;
  level = (level < TC_LOG_ERR) ?TC_LOG_ERR :level;
  level = (level > TC_LOG_MSG) ?TC_LOG_MSG :level;

  size = strlen(tc_log_preambles[level] + 1);			 
 
  if(size > TC_MSG_BUF_SIZE) {
	dynbuf = 1;
	a = malloc(size);
	if(!a) {
		fprintf(stderr, "(%s) CRITICAL: can't get memory in "
				"tc_tag_error(); tag='%s'\n", __FILE__, tag);
		return;
	}
  } else {
	size = TC_MSG_BUF_SIZE - 1;
  }

  snprintf(a, size, tc_log_preambles[level], tag, fmt);
  
  va_start(ap, fmt);
  vfprintf(stderr, a, ap);
  va_end(ap);
  if(dynbuf) {
	  free(a);
  }
  fflush(stdout);
}  

#if defined(HAVE_ALLOCA)
#define local_alloc(s) alloca(s)
#define local_free(s) do { (void)0; } while(0)
#else
#define local_alloc(s) malloc(s)
#define local_free(s) free(s)
#endif

int tc_test_program(char *name)
{
#ifndef NON_POSIX_PATH
	const char * path = getenv("PATH");
	char *tok_path = NULL;
	char *compl_path = NULL;
	char *tmp_path;
	char **strtokbuf;
	char done;
	size_t pathlen;
	long sret;
	int error = 0;

	if(!name)
	{
		tc_warn("ERROR: Searching for a NULL program!\n");
		return(ENOENT);
	}

	if(!path)
	{
		tc_warn("The '%s' program could not be found. \n", name);
		tc_warn("Because your PATH environment variable is not set.\n");
		return(ENOENT);
	}

	pathlen		= strlen(path) + 1;
	tmp_path	= local_alloc(pathlen * sizeof(char));
	strtokbuf	= local_alloc(pathlen * sizeof(char));

	sret = strlcpy(tmp_path, path, pathlen);
	tc_test_string(__FILE__, __LINE__, pathlen, sret, errno);

	/* iterate through PATH tokens */

	for (done = 0, tok_path = strtok_r(tmp_path, ":", strtokbuf);
			!done && tok_path;
			tok_path = strtok_r((char *)0, ":", strtokbuf))
	{
		pathlen = strlen(tok_path) + strlen(name) + 2;
		compl_path = local_alloc(pathlen * sizeof(char));
		sret = snprintf(compl_path, pathlen, "%s/%s", tok_path, name);
 		tc_test_string(__FILE__, __LINE__, pathlen, sret, errno);

		if(access(compl_path, X_OK) == 0)
		{
			error	= 0;
			done	= 1;
		}
		else
		{
			if(errno != ENOENT)
			{
				done	= 1;
				error	= errno;
			}
		}

		local_free(compl_path);
	}

	local_free(tmp_path);
	local_free(strtokbuf); 

	if(!done)
	{
		tc_warn("The '%s' program could not be found. \n", name);
		tc_warn("Please check your installation.\n");
		return(ENOENT); 
	}

	if(error != 0)
	{
		/* access returned an unhandled error */
		tc_warn("The '%s' program was found, but is not accessible.\n", name);
		tc_warn("%s\n", strerror(errno));
		tc_warn("Please check your installation.\n");
		return(error);
	}
#endif

	return 0;
}

#undef local_alloc
#undef local_free


#define delta 0.05
int tc_guess_frc(double fps)
{
  if (fps-delta < 00.010 && 00.010 < fps+delta) return 0;
  if (fps-delta < 23.976 && 23.976 < fps+delta) return 1;
  if (fps-delta < 24.000 && 24.000 < fps+delta) return 2;
  if (fps-delta < 25.000 && 25.000 < fps+delta) return 3;
  if (fps-delta < 29.970 && 29.970 < fps+delta) return 4;
  if (fps-delta < 30.000 && 30.000 < fps+delta) return 5;
  if (fps-delta < 50.000 && 50.000 < fps+delta) return 6;
  if (fps-delta < 59.940 && 59.940 < fps+delta) return 7;
  if (fps-delta < 60.000 && 60.000 < fps+delta) return 8;
  if (fps-delta <  1.000 &&  1.000 < fps+delta) return 9;
  if (fps-delta <  5.000 &&  5.000 < fps+delta) return 10;
  if (fps-delta < 10.000 && 10.000 < fps+delta) return 11;
  if (fps-delta < 12.000 && 12.000 < fps+delta) return 12;
  if (fps-delta < 15.000 && 15.000 < fps+delta) return 13;
  return -1;
}
#undef delta


int tc_test_string(const char *file, int line, int limit, long ret, int errnum)
{
    if (ret < 0) {
        fprintf(stderr, "[%s:%d] string error: %s\n",
                        file, line, strerror(errnum));
        return(1);
    }
    if (ret >= limit) {
        fprintf(stderr, "[%s:%d] truncated %ld characters\n",
                        file, line, (ret - limit) + 1);
        return(1);
    }
    return(0);
}


/*
 * These versions of [v]snprintf() return -1 if the string was truncated,
 * printing a message to stderr in case of truncation (or other error).
 */

int _tc_vsnprintf(const char *file, int line, char *buf, size_t limit,
		  const char *format, va_list args)
{
    int res = vsnprintf(buf, limit, format, args);
    return tc_test_string(file, line, limit, res, errno) ? -1 : res;
}


int _tc_snprintf(const char *file, int line, char *buf, size_t limit,
		 const char *format, ...)
{
    va_list args;
    int res;

    va_start(args, format);
    res = _tc_vsnprintf(file, line, buf, limit, format, args);
    va_end(args);
    return res;
}
