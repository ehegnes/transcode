/******* NOTICE: this module is disabled *******/

/*
 *  filter_test.c
 *
 *  Copyright (C) Thomas Östreich - February 2002
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

#define MOD_NAME    "filter_test.so"
#define MOD_VERSION "v0.0.1 (2002-11-04)"
#define MOD_CAP     "test filter plugin"
#define MOD_AUTHOR  "Thomas Oestreich"

#include "transcode.h"
#include "filter.h"

static char *buffer;

static int ac=0, loop=0;


/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/


int tc_filter(frame_list_t *ptr_, char *options)
{
  vframe_list_t *ptr = (vframe_list_t *)ptr_;
  vob_t *vob=NULL;


tc_log_error(MOD_NAME, "****************** NOTICE ******************");
tc_log_error(MOD_NAME, "This module is disabled, probably because it");
tc_log_error(MOD_NAME, "is considered obsolete or redundant.");
tc_log_error(MOD_NAME, "If you still need this module, please");
tc_log_error(MOD_NAME, "contact the transcode-users mailing list.");
return -1;

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

    buffer = tc_bufalloc(SIZE_RGB_FRAME);

    if(options != NULL) sscanf(options,"%d:%d", &loop, &ac);

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

  if((ptr->tag & TC_PRE_M_PROCESS) && (ptr->tag & TC_VIDEO)) {

    int n;

    for(n=0; n<loop; ++n)
	{
	    ac_memcpy(buffer, ptr->video_buf, ptr->video_size);
	    memset(ptr->video_buf, 0, ptr->video_size);
	    ac_memcpy(ptr->video_buf, buffer, ptr->video_size);
    } //loop

  } //slot

  return(0);
}
