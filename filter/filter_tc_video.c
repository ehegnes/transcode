/******* NOTICE: this module is disabled *******/

/*
 *  filter_tc_video.c
 *
 *  Copyright (C) Tilmann Bitterberg - August 2002
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

#define MOD_NAME    "filter_tc_video.so"
#define MOD_VERSION "v0.2 (2003-06-10)"
#define MOD_CAP     "video 23.9 -> 29.9 telecide filter"
#define MOD_AUTHOR  "Tilmann Bitterberg"

#include "transcode.h"
#include "filter.h"
#include "optstr.h"

// this variable is for external control from transcode. Not enabled right now
// extern int tc_do_telecide;

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/


// telecine filter

int tc_filter(frame_list_t *ptr_, char *options)
{
  vframe_list_t *ptr = (vframe_list_t *)ptr_;
  static vob_t *vob=NULL;
  static char *video_buf[2] = {NULL, NULL};

tc_log_error(MOD_NAME, "****************** NOTICE ******************");
tc_log_error(MOD_NAME, "This module is disabled, probably because it");
tc_log_error(MOD_NAME, "is considered obsolete or redundant.");
tc_log_error(MOD_NAME, "If you still need this module, please");
tc_log_error(MOD_NAME, "contact the transcode-users mailing list.");
return -1;

  if (ptr->tag & TC_FILTER_GET_CONFIG) {
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VYE", "1");
      return 0;
  }

  if (ptr->tag & TC_AUDIO)
	  return (0);


  //----------------------------------
  //
  // filter init
  //
  //----------------------------------

  if(ptr->tag & TC_FILTER_INIT) {

    if((vob = tc_get_vob())==NULL) return(-1);

    // filter init ok.

    if(verbose) tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);

    if(verbose) tc_log_info(MOD_NAME, "options=%s", options);

    if (!video_buf[0] && !video_buf[1]) {
	video_buf[0] = tc_malloc (SIZE_RGB_FRAME);
	video_buf[1] = tc_malloc (SIZE_RGB_FRAME);
	if (!video_buf[0] || !video_buf[1]) {
	    fprintf(stderr, "[%s] can't allocate buffers", MOD_NAME);
	    return (-1);
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

  /* pass-through */
  if (ptr->id == 0)
      return 0;
  /* in:      T1 B1 | T2 B2 | T3 B3 | T4 B4 */
  /* out: T1 B1 | T2 B2 | T2 B3 | T3 B4 | T4 B4 */
  if(ptr->tag & TC_PRE_PROCESS && ptr->tag & TC_VIDEO) {
  if (vob->im_v_codec == CODEC_YUV) {
    int mod = ptr->id % 4;
    int height = vob->ex_v_height;
    int width  = vob->ex_v_width;
    int width2  = width/2;
    int wh      = width*height;
    int y;
    char *Y1, *Cr1;
    char *Y2, *Cr2;

    /*
    if (!tc_do_telecide) {
	    ac_memcpy (video_buf[0], ptr->video_buf, height*width*3/2);
	    ac_memcpy (video_buf[1], ptr->video_buf, height*width*3/2);
	    return 0;
    }
    */

    //fprintf(stderr, "Doing operations on frame %d\n", ptr->id);
    switch (mod) {
      case 1:
	/* nothing, pass frame through */
	break;
      case 2:
	Y1 = video_buf[0];
	Cr1 = video_buf[0]+wh;

	Y2 = ptr->video_buf;
	Cr2 = ptr->video_buf+wh;

	/* save top2 lines */
	for (y=0; y<(height+1)/2; y++) {
	    ac_memcpy (Y1, Y2, width);
	    Y1 += width*2;
	    Y2 += width*2;
	}
	/* color */
	for (y=0; y<(height+1)/2; y++) {
	    ac_memcpy (Cr1, Cr2, width2);
	    Cr1 += width;
	    Cr2 += width;
	}

	break;
      case 3:
	Y1 = video_buf[1];
	Cr1 = video_buf[1]+wh;

	Y2 = ptr->video_buf;
	Cr2 = ptr->video_buf+wh;

	/* save top3 lines */
	for (y=0; y<(height+1)/2; y++) {
	    ac_memcpy (Y1, Y2, width);
	    Y1 += width*2;
	    Y2 += width*2;
	}
	/* color */
	for (y=0; y<(height+1)/2; y++) {
	    ac_memcpy (Cr1, Cr2, width2);
	    Cr1 += width;
	    Cr2 += width;
	}

	Y1 = ptr->video_buf;
	Cr1 = ptr->video_buf+wh;

	Y2 = video_buf[0];
	Cr2 = video_buf[0]+wh;

	/* merge bot3 with top2 */
	for (y=0; y<(height+1)/2; y++) {
	    ac_memcpy (Y1, Y2, width);
	    Y1 += width*2;
	    Y2 += width*2;
	}
	/* color */
	for (y=0; y<(height+1)/2; y++) {
	    ac_memcpy (Cr1, Cr2, width2);
	    Cr1 += width;
	    Cr2 += width;
	}
	break;
      case 0:
	if (!(ptr->attributes & TC_FRAME_WAS_CLONED)) {
	    ptr->attributes |= TC_FRAME_IS_CLONED;

	    /* save complete frame */
	    ac_memcpy (video_buf[0], ptr->video_buf, height*width*3/2);

	    /* merge bot4 with top3 */
	    Y1 = ptr->video_buf;
	    Cr1 = ptr->video_buf+wh;

	    Y2 = video_buf[1];
	    Cr2 = video_buf[1]+wh;

	    for (y=0; y<(height+1)/2; y++) {
		ac_memcpy (Y1, Y2, width);
		Y1 += width*2;
		Y2 += width*2;
	    }
	    /* color */
	    for (y=0; y<(height+1)/2; y++) {
		ac_memcpy (Cr1, Cr2, width2);
		Cr1 += width;
		Cr2 += width;
	    }
	} else {
	    /* restore frame4 = frame 5 */
	    // this is the cloned frame
	    ac_memcpy (ptr->video_buf, video_buf[0], height*width*3/2);
	}
	break;
    } // switch mod
  }
  else if (vob->im_v_codec == CODEC_RGB)
  {
      // This is wrong
    int mod = ptr->id % 4;
    int height = vob->ex_v_height;
    int width  = vob->ex_v_width;
    int width3  = width*3;
    int y;

    switch (mod) {
      case 1:
	/* nothing, pass frame through */
	break;
      case 2:
	/* save top2 lines */
	for (y=0; y<height-1; y+=2)
	  ac_memcpy (video_buf[0]+y*width3, ptr->video_buf+y*width3, width3);
	break;
      case 3:
	/* save top3 lines */
	for (y=0; y<height-1; y+=2)
	  ac_memcpy (video_buf[1]+y*width3, ptr->video_buf+y*width3, width3);

	/* merge bot3 with top2 */
	for (y=0; y<height-1; y+=2)
	  ac_memcpy (ptr->video_buf+y*width3, video_buf[0]+y*width3, width3);
	break;
      case 0:
	if (!(ptr->attributes & TC_FRAME_WAS_CLONED)) {
	    ptr->attributes |= TC_FRAME_IS_CLONED;

	    /* save complete frame */
	    ac_memcpy (video_buf[0], ptr->video_buf, height*width3);
	    /* merge bot4 with top3 */
	    for (y=0; y<height-1; y+=2)
		ac_memcpy (ptr->video_buf+y*width3, video_buf[1]+y*width3, width3);
	} else {
	    /* restore frame4 = frame 5 */
	    // this is the cloned frame
	    ac_memcpy (ptr->video_buf, video_buf[0], height*width3);
	}
	break;
    } // switch mod

  } // CODEC_RGB
  }


  return(0);
}
