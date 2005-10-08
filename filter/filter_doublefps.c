/*
 *  filter_slowmo.c
 *
 *  Copyright (C) Tilmann Bitterberg - June 2003
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

#define MOD_NAME    "filter_doublefps.so"
#define MOD_VERSION "v0.2 (2003-06-23)"
#define MOD_CAP     "double frame rate by creating frames from fields"
#define MOD_AUTHOR  "Tilmann Bitterberg"

#include "transcode.h"
#include "filter.h"
#include "optstr.h"


/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

int tc_filter(frame_list_t *ptr_, char *options)
{
  vframe_list_t *ptr = (vframe_list_t *)ptr_;
  static vob_t *vob=NULL;
  static char *lines = NULL;
  static int width, height, codec;
  int h;
  static int evenfirst=0;

  //----------------------------------
  //
  // filter init
  //
  //----------------------------------

  if(ptr->tag & TC_AUDIO)
      return 0;

  if(ptr->tag & TC_FILTER_INIT) {
    
    if((vob = tc_get_vob())==NULL) return(-1);
    
    width  = vob->ex_v_width;
    height = vob->ex_v_height;
    codec  = vob->im_v_codec;
    
    // dirty, dirty, dirty.
    //vob->ex_v_height /= 2;
    //height_mod = vob->ex_v_height;

    if (!lines) 
	lines = (char *) malloc (width*height*3);
    
    if (!lines) {
	fprintf(stderr, "[%s] No lines buffer available\n", MOD_NAME);
	return -1;
    }

    if(options) {
	if (verbose & TC_INFO) printf("[%s] options=%s\n", MOD_NAME, options);

	optstr_get (options, "shiftEven",      "%d",  &evenfirst        );
    }

    // filter init ok.
    if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);
    
    return(0);
  }

  //----------------------------------
  //
  // filter configure
  //
  //----------------------------------

  if(ptr->tag & TC_FILTER_GET_CONFIG) {
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRYO", "1");
      optstr_param (options, "shiftEven",    "Assume even field dominance",  "%d", "0", "0", "1");
  }

  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  if(ptr->tag & TC_FILTER_CLOSE) {
    if (lines) free(lines); lines=NULL;
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

  // this must be POST_S_PROCESS otherwise we won't see a cloned frame again.
  
  if(ptr->tag & TC_POST_S_PROCESS) {
      int stride = ptr->v_width*3;
      if (codec==CODEC_YUV)
	  stride = ptr->v_width;

      if (!(ptr->attributes & TC_FRAME_WAS_CLONED)) {

	  char *p = ptr->video_buf;
	  char *s = lines+((evenfirst)?stride:0);
	  //printf("Is cloned\n");

	  ptr->attributes |= TC_FRAME_IS_CLONED;

	  ac_memcpy (lines, ptr->video_buf, ptr->video_size);

	  for (h = 0; h < height/2; h++) {
	      ac_memcpy (p, s, stride);
	      s += 2*stride;
	      p +=   stride;
	  }
	  if (codec==CODEC_YUV) {
	      for (h = 0; h < height/4; h++) {
		  ac_memcpy (p, s, stride/2);
		  s += 2*stride/2;
		  p +=   stride/2;
	      }
	      for (h = 0; h < height/4; h++) {
		  ac_memcpy (p, s, stride/2);
		  s += 2*stride/2;
		  p +=   stride/2;
	      }
	  }

	  //memset (p, 128, ptr->video_size/2);

      } else {

	  char *p = ptr->video_buf;
	  char *s = lines+((evenfirst)?0:stride);
	 // printf("WAS cloned\n");

	  for (h = 0; h < height/2; h++) {
	      ac_memcpy (p, s, stride);
	      s += 2*stride;
	      p +=   stride;
	  }

	  if (codec==CODEC_YUV) {
	      for (h = 0; h < height/4; h++) {
		  ac_memcpy (p, s, stride/2);
		  s += 2*stride/2;
		  p +=   stride/2;
	      }
	      for (h = 0; h < height/4; h++) {
		  ac_memcpy (p, s, stride/2);
		  s += 2*stride/2;
		  p +=   stride/2;
	      }
	  }
	  //memset (p, 128, ptr->video_size/2);

      }
     //ptr->v_height = height_mod;
  }
  
  return(0);
}
