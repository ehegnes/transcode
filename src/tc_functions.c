/*
 *  tc_functions.c 
 *
 *  Copyright (C) Thomas Östreich - August 2003
 *
 *  This file is part of transcode, a linux video stream processing tool
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

#include "tc_functions.h"

#if defined(HAVE_ALLOCA_H)
#include <alloca.h>
#endif

extern void version();
extern char *RED,*GREEN,*YELLOW,*BLUE,*WHITE,*GRAY;

void tc_error(char *fmt, ...)
{
  
  va_list ap;

  // munge format
  int size = strlen(fmt) + 2*strlen(RED) + 2*strlen(GRAY) +
             strlen(PACKAGE) + strlen("[] critical: \n") + 1;
  char *a = malloc (size);

  version();

  snprintf(a, size, "[%s%s%s] %scritical%s: %s\n",
                     RED, PACKAGE, GRAY, RED, GRAY, fmt);

  va_start(ap, fmt);
  vfprintf (stderr, a, ap);
  va_end(ap);
  free (a);
  //abort
  fflush(stdout);
  exit(1);
}

void tc_warn(char *fmt, ...)
{
  
  va_list ap;

  // munge format
  int size = strlen(fmt) + 2*strlen(BLUE) + 2*strlen(GRAY) +
             strlen(PACKAGE) + strlen("[]  warning: \n") + 1;
  char *a = malloc (size);

  version();

  snprintf(a, size, "[%s%s%s] %swarning%s : %s\n",
                     RED, PACKAGE, GRAY, YELLOW, GRAY, fmt);

  va_start(ap, fmt);
  vfprintf (stderr, a, ap);
  va_end(ap);
  free (a);
  fflush(stdout);
}

void tc_info(char *fmt, ...)
{
  
  va_list ap;

  // munge format
  int size = strlen(fmt) + strlen(BLUE) + strlen(GRAY) +
             strlen(PACKAGE) + strlen("[] \n") + 1;
  char *a = malloc (size);

  version();

  snprintf(a, size, "[%s%s%s] %s\n", BLUE, PACKAGE, GRAY, fmt);

  va_start(ap, fmt);
  vfprintf (stderr, a, ap);
  va_end(ap);
  free (a);
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
	int error = 0;

	if(!name)
	{
		fprintf(stderr, "[%s] ERROR: Searching for a NULL program!\n", PACKAGE);
		return(ENOENT);
	}

	if(!path)
	{
		fprintf(stderr, "[%s] ERROR: The '%s' program could not be found. \n"
		"[%s]        Because your PATH environment variable is not set.\n", PACKAGE, name, PACKAGE);
		return(ENOENT);
	}

	tmp_path	= local_alloc((strlen(path) + 1) * sizeof(char));
	strtokbuf	= local_alloc((strlen(path) + 1) * sizeof(char));

	strcpy(tmp_path, path);

	/* iterate through PATH tokens */

	for (done = 0, tok_path = strtok_r(tmp_path, ":", strtokbuf);
			!done && tok_path;
			tok_path = strtok_r((char *)0, ":", strtokbuf))
	{
		compl_path = local_alloc((strlen(tok_path) + strlen(name) + 2) * sizeof(char));
		snprintf(compl_path, strlen(tok_path) + strlen(name) + 2, "%s/%s", tok_path, name);
 
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
		fprintf(stderr, "[%s] ERROR: The '%s' program could not be found. \n"
				"[%s]        Please check your installation.\n", PACKAGE, name, PACKAGE);
		return(ENOENT); 
	}

	if(error != 0)
	{
		/* access returned an unhandled error */
		fprintf(stderr,
				"[%s] ERROR: The '%s' program was found, but is not accessible.\n"
				"[%s]        %s\n"
				"[%s]        Please check your installation.\n",
				PACKAGE, name,
				PACKAGE, strerror(error),
				PACKAGE);
		return(error);
	}
#endif

	return 0;
}

#undef local_alloc
#undef local_free


int tc_test_string(char *file, int line, int limit, int ret, int errnum)
{
    if (ret < 0) {
        fprintf(stderr, "[%s:%d] string error: %s\n",
                        file, line, strerror(errnum));
        return(1);
    }
    if (ret >= limit) {
        fprintf(stderr, "[%s:%d] truncated %d characters\n",
                        file, line, (ret - limit) + 1);
        return(1);
    }
    return(0);
}


#ifndef HAVE_STRLCPY
/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * #include <sys/types.h>
 * #include <string.h>
 */
/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t
strlcpy(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}
#endif

#ifndef HAVE_STRLCAT
/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * #include <sys/types.h>
 * #include <string.h>
 */
/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
size_t
strlcat(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (n-- != 0 && *d != '\0')
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return(dlen + strlen(s));
	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return(dlen + (s - src));	/* count does not include NUL */
}
#endif
