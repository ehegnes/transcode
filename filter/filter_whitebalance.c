/*
    filter_whitebalance.c
    
    This file is part of transcode, a video stream processing tool
    
    White Balance Filter - correct images with a broken white balance
    (typically, images from a dv camcorder with an unset white balance or
    wrongly forced to indoor or outdoor)
     
    Copyright (C) 2003 Guillaume Cottenceau <gc at mandrakesoft.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#define MOD_NAME    "filter_whitebalance.so"
#define MOD_VERSION "v0.1 (2003-10-01)"
#define MOD_CAP     "White Balance Filter - correct images with a broken white balance"
#define MOD_AUTHOR  "Guillaume Cottenceau"

#include "transcode.h"
#include "filter.h"
#include "optstr.h"

#include "export/vid_aux.h"

#include <math.h>
#include <ctype.h>

static unsigned char * buffer = NULL;
static int level = 40;
static char limit[PATH_MAX];

static double factor;
static int state = 1;
static int next_switchoff = -1;
static int next_switchon = -1;

static void update_switches(void)
{
	static char * ptr = limit;
	int next = 0;
	if (!ptr)
		return;

	ptr = strchr(ptr, state ? '-' : '+');
	if (ptr) {
		ptr++;
		while (*ptr && isdigit(*ptr)) {
			next = (next * 10) + (*ptr - '0');
			ptr++;
		}
	} else {
		next = -1;
		ptr = NULL;
		return;
	}
	if (state)
		next_switchoff = next;
	else
		next_switchon = next;
}

int tc_filter(frame_list_t *ptr_, char *options)
{
	vframe_list_t *ptr = (vframe_list_t *)ptr_;
	static vob_t *vob = NULL;
	static unsigned char red_filter[256];
	static unsigned char blue_filter[256];

	if (ptr->tag & TC_FILTER_GET_CONFIG) {
		char buf[32];
		optstr_filter_desc(options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRYE", "1");
		
		snprintf(buf, 32, "%d", level);
		optstr_param(options, "level", "Level of blue-to-yellow white balance shifting (can be negative)", "%d", buf, "-1000", "+1000");
		optstr_param(options, "limit", "Limit to specified ranges (+fnumber toggles on, -fnumber toggles off)", "%s", "");
		return 0;
	}

	if (ptr->tag & TC_FILTER_INIT) {
		unsigned int width, height;
		int i;

		if (verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);
    
		if (!(vob = tc_get_vob())) {
			fprintf(stderr, "[%s] ERROR: Could not get vob\n", MOD_NAME);
			return -1;
		}

		width  = vob->im_v_width;
		height = vob->im_v_height;

		if (options != NULL) {
			if (verbose) printf("[%s] options=%s\n", MOD_NAME, options);

			optstr_get(options, "level", "%d", &level);
			memset(limit, 0, PATH_MAX);
			optstr_get(options, "limit", "%[^:]", &limit);
		}
		if (verbose) printf("[%s] options set to: level=%d limit=%s\n", MOD_NAME, level, limit);
		factor = 1 + ((double)abs(level))/100;
		if (level < 0)
			factor = 1/factor;
		/* preprocess filters for performance */
		for (i=0; i<=255; i++) {
			red_filter[i]  = pow(((double) i)/255, 1/factor) * 255;
			blue_filter[i] = pow(((double) i)/255, factor)   * 255;
		}
		update_switches();

		if (vob->im_v_codec == CODEC_YUV) {
			if (verbose) printf("[%s] will need to convert YUV to RGB before filtering\n", MOD_NAME);
			tc_rgb2yuv_init(width, height);
			tc_yuv2rgb_init(width, height);
		}

		if (!buffer)
			buffer = malloc(SIZE_RGB_FRAME);
		if (!buffer) {
			fprintf(stderr, "[%s] ERROR: Could not malloc %d bytes\n", MOD_NAME, SIZE_RGB_FRAME);
			return -1;
		}

		return 0;
	}
	
	if (ptr->tag & TC_FILTER_CLOSE) {
		if (buffer)
			free(buffer);
		buffer = NULL;

		if (vob->im_v_codec == CODEC_YUV) {
			tc_rgb2yuv_close();
			tc_yuv2rgb_close();
		}
		return 0;
	}

	if (ptr->tag & TC_PRE_M_PROCESS && ptr->tag & TC_VIDEO && !(ptr->attributes & TC_FRAME_IS_SKIPPED)) {
		int x, y;

		if (!state && ptr->id == next_switchon) {
			state = 1;
			update_switches();
		} else if (state && ptr->id == next_switchoff) {
			state = 0;
			update_switches();
		}

		if (state) {
			if (vob->im_v_codec == CODEC_YUV)
				tc_yuv2rgb_core(ptr->video_buf);
			tc_memcpy(buffer, ptr->video_buf, ptr->v_width*ptr->v_height*3);
			
			
			for (y = 0; y < vob->im_v_height; y++) {
				unsigned char * line = &buffer[y * (vob->im_v_width * 3)];
				for (x = 0; x < vob->im_v_width*3; x += 3) {
					/* we modify red and blue curves to enhance/reduce mostly mediums */
					line[x]   = red_filter[line[x]];
					line[x+2] = blue_filter[line[x+2]];
				}
			}
			
			
			tc_memcpy(ptr->video_buf, buffer, ptr->v_width*ptr->v_height*3);
			if (vob->im_v_codec == CODEC_YUV)
				tc_rgb2yuv_core(ptr->video_buf);
		}
	} 

	return 0;
}

