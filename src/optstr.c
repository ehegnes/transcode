/*
 *  optstr.c
 *
 *  Copyright (C) Tilmann Bitterberg 2003
 *
 *  Description: A general purpose option string parser
 * 
 *  Usage: see optstr.h, please
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
# include "config.h"
#endif

/* for vsscanf */
#ifdef HAVE_VSSCANF
#  define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "optstr.h"

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

int optstr_is_string_arg(char *fmt)
{
    if (!fmt)
	return 0;

    if (!strlen(fmt))
	return 0;

    if (strchr (fmt, 's'))
	return 1;

    if (strchr (fmt, '[') && strchr (fmt, ']'))
	return 1;

    return 0;
}


int optstr_filter_desc (char *buf,
		char *filter_name,
                char *filter_comment,
		char *filter_version,
		char *filter_author,
		char *capabilities,
		char *frames_needed
		)
{
    int len = strlen(buf);
    if (snprintf(buf+len, ARG_CONFIG_LEN-len, "\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\"\n", 
				    filter_name,filter_comment,filter_version,
				    filter_author, capabilities, frames_needed) <= 0)
	return 1;
    return 0;
}

int optstr_frames_needed (char *filter_desc, int *needed_frames)
{
    char *s;

    if ((s = strrchr (filter_desc, ',')) == NULL)
	return 1;
    if ((s = strchr (s, '\"')) == NULL)
	return 1;

    *needed_frames = strtol (s+1, (char **)NULL, 0);
    return 0;
}

int optstr_param  (char *buf, 
		   char *name, 
		   char *comment, 
		   char *fmt, 
		   char *val, 
		   ... ) /* char *valid_from1, char *valid_to1, ... */ 
{
    va_list ap; 
    int n = 0, pos, numargs=0;
    int len = strlen(buf);

    if ((n += snprintf(buf+len, ARG_CONFIG_LEN-len, "\"%s\", \"%s\", \"%s\", \"%s\"", 
				name,comment,fmt,val)) <= 0)
	return 1;


    /* count format strings */
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
    numargs *= 2;

    if (numargs && optstr_is_string_arg(fmt))
	numargs = 0;

    va_start (ap, val);
      while (numargs--) {
	  if ((n += snprintf (buf+len+n, ARG_CONFIG_LEN-len-n, ", \"%s\"", va_arg(ap, char *))) <= 0)
	      return 1;
      }
    va_end (ap);

    if ((n += snprintf (buf+len+n, ARG_CONFIG_LEN-len-n, "\n")) <= 0 )
	return 1;


    return 0;
}

