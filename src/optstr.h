/*
 *  optstr.h
 *
 *  Copyright (C) Tilmann Bitterberg 2002
 *
 *  Description: A general purpose option string parser
 * 
 *  Usage:
 *  int main (void)
 *  {
 *      int sum, top, bottom, quant;
 *      int n;
 *      char s[100];
 *      char options[] = "ranges=5-10:range=8,12,100:percent=16%:help";
 *  
 *      if (optstr_get (options, "help", "") >= 0)
 *              usage();
 *  
 *      optstr_get (options, "range", "%d,%d,%d", &bottom, &top, &sum);
 *      optstr_get (options, "ranges", "%d-%d", &bottom, &top);
 *      optstr_get (options, "range", "%d,%d", &bottom, &top);
 *      optstr_get (options, "string", "%[^:]", s);
 *      n = optstr_get (options, "percent", "%d%%", &quant);
 *      printf("found %d argumens\n", n);
 *  
 *      return 0;
 *  }
 *  
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

#ifndef __OPTSTR_H
#define __OPTSTR_H

#define ARG_MAXIMUM (16)
#define ARG_SEP ':'

/*
 * Finds the _exact_ needle in haystack
 * Return values:
 * A pointer   into haystack when needle is found
 * NULL        if needle is not found in haystack
 */
char * optstr_lookup(char *haystack, char *needle);

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

int optstr_get(char *options, char *name, char *fmt, ...);

#endif /* __OPTSTR_H */
