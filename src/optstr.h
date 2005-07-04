/*
 *  optstr.h
 *
 *  Copyright (C) Tilmann Bitterberg 2003
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

#ifndef __OPTSTR_H
#define __OPTSTR_H

#define ARG_MAXIMUM (16)
#define ARG_SEP ':'
#define ARG_CONFIG_LEN 8192

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

/*
 * Purpose:
 *   Generate a Description of a filter
 *   The output in buf will be a row in CSV format. Example:
 *   "filter_foo", "comment", "0.1", "no@one", "VRY", "1"\n
 *
 * Input:
 *   buf:     A write buffer, will contain the result of the function.
 *            buf must be at least ARG_CONFIG_LEN characters large.
 *   filter_(name|comment|version|author):
 *            obvious, various filter meta data
 *   capabilities:
 *            String of filter capabilities. 
 *               "V":  Can do Video
 *               "A":  Can do Audio
 *               "R":  Can do RGB
 *               "Y":  Can do YUV
 *               "4":  Can do YUV422
 *               "M":  Can do Multiple Instances
 *               "E":  Is a PRE filter
 *               "O":  Is a POST filter
 *            Valid examples:
 *               "VR"  : Video and RGB
 *               "VRY" : Video and YUV and RGB
 *            
 *   frames_needed:
 *            A string of how many frames the filter needs to take effect.
 *            Usually this is "1".
 *
 * Return values:
 *    1       Not enough space in buf
 *    0       Successfull
 */
int optstr_filter_desc (char *buf,
		char *filter_name,
                char *filter_comment,
		char *filter_version,
		char *filter_author,
		char *capabilities,
		char *frames_needed
		);

/*
 * Purpose:
 *   Extract the how many frames the filter needs from an CSV row.
 *
 * Input:
 *   filter_desc:
 *            the CSV row
 *   needed_frames:
 *            The result will be stored in needed_frames
 *
 * Return values:
 *    1       An Error happend
 *    0       Successfull
 */
int optstr_frames_needed (char *filter_desc, int *needed_frames);

/*
 * Purpose:
 *   Generate a description of one filter parameter. The output will be in CSV
 *   format. Example:
 *   "radius", "Search radius", "%d", "8", "8", "24"\n
 *
 * Input:
 *   buf:     A write buffer, will contain the result of the function.
 *            buf must be at least ARG_CONFIG_LEN characters large.
 *   name:    The name of the parameter (eg "radius")
 *   comment: A short description (eg "Search radius")
 *   fmt:     A printf style parse string (eg "%d")
 *   val:     Current value (eg "8")
 *   (...):   Always pairs: Legal values for the parameter
 *            (eg "8", "24" -- meaning, the radius parameter is valid 
 *            from 8 to 24)
 *          
 * Return values:
 *    1       An Error happend
 *    0       Successfull
 *
 * More examples:
 *   "pos", "Position (0-width x 0-height)", "%dx%d", "0x0", "0", "width", "0", "height"
 *    "%dx%d" is interesting, because this parameter takes two values in this format
 *            so we must supply two ranges (one for each parameter), when this
 *            param is valid ("0", "width", "0", "height")
 *    
 *   "flip", "Mirror image", "", "0"
 *     This is a boolean, defaults to false. A boolean has no argument, eg "filter_foo=flip"
 *   
 */
int optstr_param  (char *buf, 
		   char *name, 
		   char *comment, 
		   char *fmt, 
		   char *val, 
		   ... ); /* char *valid_from1, char *valid_to1 */ 

/* internal */
int optstr_is_string_arg(char *fmt);

#endif /* __OPTSTR_H */
