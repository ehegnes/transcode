/*
 *  filter_fps.c
 *
 *  Copyright 2003 Christopher Cramer
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
 *  along with transcode; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */

#define MOD_NAME    "filter_fps.so"
#define MOD_VERSION "v0.1 (2003-07-15)"
#define MOD_CAP     "convert video frame rate"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "transcode.h"
#include "framebuffer.h"

static int
parse_options(char *options, double *infps, double *outfps)
{
	char	*p, *q, *r;
	size_t	len;
	vob_t	*vob;

	/* defaults from -f and --export_fps */
	vob = tc_get_vob();
	if (!vob) return -1;
	*infps = vob->fps;
	*outfps = vob->ex_fps;

	if (!options || !*options) return 0;
	if (!strcmp(options, "help")) {
		printf("[%s] help\n", MOD_NAME);
		printf("This filter converts the video frame rate,"
			" by repeating or dropping frames.\n");
		printf("options: <input fps>:<output fps>\n");
		printf("example: -J fps=25:29.97 will convert"
			" from PAL to NTSC\n");
		return -1;
	}
	len = strlen(options);
	p = alloca(len + 1);
	memcpy(p, options, len);
	q = memchr(p, ':', len);
	if (!q) return -1;
	*q++ = '\0';
	*infps = strtod(p, &r);
	if (r == p) return -1;
	*outfps = strtod(q, &r);
	if (r == q) return -1;
}

int
tc_filter(vframe_list_t *ptr, char *options)
{
	static double		infps, outfps;
	static unsigned long	framesin = 0, framesout = 0;

	if(ptr->tag & TC_FILTER_INIT) {
		if (verbose) printf("[%s] %s %s\n",
			MOD_NAME, MOD_VERSION, MOD_CAP);
		if (parse_options(&options, &infps, &outfps) == -1) return -1;
		if (verbose && options) printf(
			"[%s] options=%s, converting from %g fps to %g fps\n",
			MOD_NAME, options, infps, outfps);
		if (verbose && !options) printf(
			"[%s] no options, converting from %g fps to %g fps\n",
			MOD_NAME, infps, outfps);
		return 0;
	}

	if (infps > outfps && ptr->tag & TC_PRE_S_PROCESS
			&& ptr->tag & TC_VIDEO) {
		if ((double)++framesin / infps > (double)framesout / outfps)
			framesout++;
		else ptr->attributes |= TC_FRAME_IS_SKIPPED;
		return 0;
	}

	if (infps < outfps && ptr->tag & TC_POST_S_PROCESS
			&& ptr->tag & TC_VIDEO) {
		if (!(ptr->attributes & TC_FRAME_WAS_CLONED)) framesin++;
		if ((double)framesin / infps > (double)++framesout / outfps)
			ptr->attributes |= TC_FRAME_IS_CLONED;
	}

	return 0;
}
