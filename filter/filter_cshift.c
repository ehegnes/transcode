/*
 *  filter_cshift.c
 *
 *  Copyright (C) Thomas Östreich, Chad Page - February/March 2002
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

#define MOD_NAME    "filter_cshift.so"
#define MOD_VERSION "v0.2.1 (2003-01-21)"
#define MOD_CAP     "chroma-lag shifter"
#define MOD_AUTHOR  "Thomas Östreich, Chad Page"

#include <stdio.h>
#include <stdlib.h>

static char *buffer;

static int loop=1;

/* -------------------------------------------------
 *
 * mandatory include files
 *
 *-------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "transcode.h"
#include "framebuffer.h"
#include "optstr.h"

void crshift_yuv(char * buffer, vob_t *vob, int shift)
{
	int x, y;
	char * craddr;
	char * cbaddr;
	char * addr;

	craddr = &buffer[(vob->im_v_width * vob->im_v_height)];
	cbaddr = &buffer[(vob->im_v_width * vob->im_v_height) * 5 / 4];

	for (y = 0; y < (vob->im_v_height / 2); y++) {
		for (x = 0; x < ((vob->im_v_width / 2) - shift); x++) {
			addr = &craddr[(y * (vob->im_v_width / 2)) + x]; 
			*addr = addr[shift];
			addr = &cbaddr[(y * (vob->im_v_width / 2)) + x]; 
			*addr = addr[shift];
		}	
	}
}

void rgb2yuv(unsigned char *out, unsigned char *in, int width)
{
	int i, r, g, b;

	/* i = Y, i+1 = Cr, i+2 = Cb */

	for (i = 0; i < (width * 3); i+=3) {
		r = in[i]; g = in[i+1]; b = in[i+2];
		out[i] = (r * 299 / 1000) + (g * 587 / 1000) + (b * 115 / 1000);	
		out[i+1] = (-(r * 169 / 1000) - (g * 331 / 1000) + (b / 2)) + 128;
		out[i+2] = ((r / 2) - (g * 418 / 1000) - (b * 816 / 10000)) + 128;
	}
}

void yuv2rgb(unsigned char *out, unsigned char *in, int width)
{
	int i, r, g, b;

	/* i = Y, i+1 = Cr, i+2 = Cb */
	for (i = 0; i < (width * 3); i+=3) {
		b = in[i] + ((in[i+1] - 128) * 14022 / 10000);
		g = in[i] - ((in[i+2] - 128) * 3456 / 10000) - ((in[i+1] - 128.0) * 7145 / 10000);
		r = in[i] + ((in[i+2] - 128) * 1771 / 1000);
		if (r < 0) r = 0;
		if (g < 0) g = 0;
		if (b < 0) b = 0;
		if (r > 255) r = 255;
		if (g > 255) g = 255;
		if (b > 255) b = 255;
		out[i] = r;
		out[i+1] = g;
		out[i+2] = b;
	}
}

void crshift_rgb(unsigned char * buffer, vob_t *vob, int shift)
{
	unsigned char buffer_yuv[4096]; 
	int y, x;

	for (y = 0; y < vob->im_v_height; y++) {
		rgb2yuv(buffer_yuv, &buffer[y * (vob->im_v_width * 3)], vob->im_v_width);
		for (x = 0; x < ((vob->im_v_width - shift) * 3); x+=3) {
			buffer_yuv[x + 1] = buffer_yuv[x + ((shift * 3) + 1)]; 
			buffer_yuv[x + 2] = buffer_yuv[x + ((shift * 3) + 2)]; 
		}
		yuv2rgb(&buffer[y * (vob->im_v_width * 3)], buffer_yuv, vob->im_v_width);
	}

}

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

// old or new syntax?
int is_optstr (char *buf) {
    if (strchr(buf, '='))
	return 1;
    if (strchr(buf, 's'))
	return 1;
    if (strchr(buf, 'h'))
	return 1;
    return 0;
}

int tc_filter(vframe_list_t *ptr, char *options)
{

  static vob_t *vob=NULL;

  // API explanation:
  // ================
  //
  // (1) need more infos, than get pointer to transcode global 
  //     information structure vob_t as defined in transcode.h.
  //
  // (2) 'tc_get_vob' and 'verbose' are exported by transcode.
  //
  // (3) filter is called first time with TC_FILTER_INIT flag set.
  //
  // (4) make sure to exit immediately if context (video/audio) or 
  //     placement of call (pre/post) is not compatible with the filters 
  //     intended purpose, since the filter is called 4 times per frame.
  //
  // (5) see framebuffer.h for a complete list of frame_list_t variables.
  //
  // (6) filter is last time with TC_FILTER_CLOSE flag set


  if(ptr->tag & TC_FILTER_GET_CONFIG) {
      char buf[32];
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRYE", "1");

      snprintf(buf, 32, "%d", loop);
      optstr_param (options, "shift", "Shift chroma(color) to the left", "%d", buf, "0", "width");
      return 0;
  }
  //----------------------------------
  //
  // filter init
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_INIT) {
    
    if((vob = tc_get_vob())==NULL) return(-1);
    
    // filter init ok.
    
    if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);
    
    if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);

    if (!buffer)
	buffer = malloc(SIZE_RGB_FRAME);

    if(options != NULL) {
	if (!is_optstr(options)) { // old syntax
	    loop=atoi(options);
	} else {
	    optstr_get (options, "shift", "%d", &loop);
	}
    }
      
    return(0);
  }
  
  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  
  if(ptr->tag & TC_FILTER_CLOSE) {
    
    if (buffer)
	free(buffer);
    buffer = NULL;
    
    return(0);
  }
  
  //----------------------------------
  //
  // filter frame routine
  //
  //----------------------------------
  
  
  // tag variable indicates, if we are called before
  // transcodes internal video/audo frame processing routines
  // or after and determines video/audio context
  
  if(ptr->tag & TC_PRE_M_PROCESS && ptr->tag & TC_VIDEO) {

      memcpy(buffer, ptr->video_buf, ptr->v_width*ptr->v_height*3);

      if (vob->im_v_codec == CODEC_YUV) crshift_yuv(buffer, vob, loop);
      if (vob->im_v_codec == CODEC_RGB) crshift_rgb(buffer, vob, loop);
      
      memcpy(ptr->video_buf, buffer, ptr->v_width*ptr->v_height*3);
  } 
  
  return(0);
}
