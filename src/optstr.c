/*
 *  optstr.c
 *
 *  Copyright (C) Tilmann Bitterberg 2002
 *
 *  Description: A general purpose option string parser
 * 
 *  Usage: see optstr.h
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

#include "config.h"

#ifdef HAVE_VSSCANF
#  define __USE_ISOC99 /* for vsscanf */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "optstr.h"

/*
 * Finds the _exact_ needle in haystack
 * Return values:
 * A pointer   into haystack when needle is found
 * NULL        if needle is not found in haystack
 */

char * optstr_lookup(char *haystack, char *needle)
{
	char *ch = haystack;
	int found = 0;
	int len = strlen (needle);

	while (!found) {
		ch = strstr(ch, needle);

		/* not in string */
		if (!ch)
			break;

		/* do we want this hit? ie is it exact? */
		if (ch[len] == '\0' || ch[len] == '=' || ch[len] == ARG_SEP) {
			found = 1;
		} else {
			/* go a little further */
			ch++; 
		}
	} 

	return (ch);

	
}

/*
 * Purpose:
 *   Extract values from option string 
 *
 * Input:
 *   options: A null terminated string of options to parse, syntax is
 *            "opt1=val1:opt_bool:opt2=val1-val2"
 *             where ':' is the seperator.
 *   name:    The name to look for in options; eg
 *            "opt2"
 *   fmt:     The format to scan values (printf format); eg
 *            "%d-%d"
 *   (...):   Variables to assign; eg
 *            &lower, &upper
 *
 * Return values:
 *   -2       internal error
 *   -1       `name' is not in `options'
 *    0       `name' is in `options'
 *   positiv  number of arguments assigned
 */

int optstr_get(char *options, char *name, char *fmt, ...)
{
	va_list ap;     /* points to each unnamed arg in turn */
	int numargs= 0; 
	int n      = 0;
	int pos;
	char *ch = NULL;

#ifndef HAVE_VSSCANF
	void *temp[ARG_MAXIMUM];
#endif
	
	ch = optstr_lookup(options, name);
	if (!ch) return (-1);
	
	/* name IS in options */

	/* Find how many arguments we expect */
	for (pos=0; pos < strlen(fmt); pos++) {
		if (fmt[pos] == '%') {
			++numargs;
			/* is this one quoted  with '%%' */
			if (pos+1 < strlen(fmt) && fmt[pos+1] == '%') {
				--numargs;
				++pos;
			}
		}
	}

#ifndef HAVE_VSSCANF
	if (numargs > ARG_MAXIMUM) {
		fprintf (stderr, 
			"(%s:%d) Internal Overflow; redefine ARG_MAXIMUM (%d) to something higher\n", 
			__FILE__, __LINE__, ARG_MAXIMUM);
		return (-2);
	}
#endif

	n = numargs;
	/* Bool argument */
	if (numargs <= 0) {
		return (0);
	}

	/* skip the `=' */
	ch += ( strlen(name) + 1);

	va_start (ap, fmt);

#ifndef HAVE_VSSCANF
	while (--n >= 0) {
		temp[numargs-n-1] = va_arg(ap, void *);
	}

	n = sscanf (ch, fmt, 
			temp[0],  temp[1],  temp[2],  temp[3], temp[4],
			temp[5],  temp[6],  temp[7],  temp[8], temp[9], 
			temp[10], temp[11], temp[12], temp[13], temp[14], 
			temp[15]);

#else
	/* this would be very nice instead of the above, 
	 * but it does not seem portable 
	 */
	 n = vsscanf (ch, fmt, ap); 
#endif
	
	va_end(ap);

	return n;
}
