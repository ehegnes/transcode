/*
 *  filter_cshift.c
 *
 *  Copyright (C) Thomas Östreich, Chad Page - February/March 2002
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

#define MOD_NAME    "filter_cshift.so"
#define MOD_VERSION "v0.2.1 (2003-01-21)"
#define MOD_CAP     "chroma-lag shifter"
#define MOD_AUTHOR  "Thomas Östreich, Chad Page"

#include "transcode.h"
#include "filter.h"
#include "optstr.h"
#include "aclib/imgconvert.h"

static int shift = 1;
static uint8_t *buffer;

static void cshift_yuv(uint8_t *buffer, vob_t *vob, int shift,
		       ImageFormat format)
{
    int x, y, w, h;
    uint8_t *cbaddr, *craddr;

    if (format == IMG_YUV420P) {
	w = vob->im_v_width/2;
	h = vob->im_v_height/2;
    } else if (format == IMG_YUV422P) {
	w = vob->im_v_width/2;
	h = vob->im_v_height;
    } else if (format == IMG_YUV444P) {
	w = vob->im_v_width;
	h = vob->im_v_height;
	shift *= 2;
    } else {
	tc_log_error(MOD_NAME, "unsupported image format %d in cshift_yuv()", format);
	return;
    }
    cbaddr = buffer + w*h;
    craddr = buffer + 2*w*h;

    for (y = 0; y < h; y++) {
	for (x = 0; x < w-shift; x++) {
	    cbaddr[y*w+x] = cbaddr[y*w+x+shift];
	    craddr[y*w+x] = cbaddr[y*w+x+shift];
	}
    }
}

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

// old or new syntax?
static int is_optstr (char *buf) {
    if (strchr(buf, '='))
	return 1;
    if (strchr(buf, 's'))
	return 1;
    if (strchr(buf, 'h'))
	return 1;
    return 0;
}

int tc_filter(frame_list_t *ptr_, char *options)
{
  vframe_list_t *ptr = (vframe_list_t *)ptr_;
  static vob_t *vob=NULL;

  if(ptr->tag & TC_FILTER_GET_CONFIG) {
      char buf[32];
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRYE", "1");

      tc_snprintf(buf, 32, "%d", shift);
      optstr_param (options, "shift", "Shift chroma(color) to the left", "%d", buf, "0", "width");
      return 0;
  }

  //----------------------------------
  //
  // filter init
  //
  //----------------------------------

  if(ptr->tag & TC_FILTER_INIT) {

    if((vob = tc_get_vob()) == NULL) return(-1);

    // filter init ok.

    if(verbose) tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);

    if(verbose) tc_log_info(MOD_NAME, "options=%s", options);

    if (!buffer)
	    buffer = tc_malloc(SIZE_RGB_FRAME);

        if(options != NULL) {
	    if (!is_optstr(options)) { // old syntax
	        shift = atoi(options);
    	} else {
	        optstr_get (options, "shift", "%d", &shift);
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
  // transcodes internal video/audio frame processing routines
  // or after and determines video/audio context

  if(ptr->tag & TC_PRE_M_PROCESS && ptr->tag & TC_VIDEO && !(ptr->attributes & TC_FRAME_IS_SKIPPED)) {

      if (vob->im_v_codec == CODEC_YUV) {
	  cshift_yuv(ptr->video_buf, vob, shift, IMG_YUV_DEFAULT);
      } else if (vob->im_v_codec == CODEC_RGB) {
	  uint8_t *planes[3];
	  planes[0] = buffer;
	  planes[1] = buffer + ptr->v_width*ptr->v_height;
	  planes[2] = buffer + 2*ptr->v_width*ptr->v_height;
	  ac_imgconvert(&ptr->video_buf, IMG_RGB_DEFAULT, planes, IMG_YUV444P,
			ptr->v_width, ptr->v_height);
	  cshift_yuv(buffer, vob, shift, IMG_YUV444P);
	  ac_imgconvert(planes, IMG_YUV444P, &ptr->video_buf, IMG_RGB_DEFAULT,
			ptr->v_width, ptr->v_height);
      }
  }

  return(0);
}
