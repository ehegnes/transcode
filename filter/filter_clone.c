/*
 *  filter_clone.c
 *
 *  Copyright (C) Thomas Östreich - August 2002
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

#define MOD_NAME    "filter_clone.so"
#define MOD_VERSION "v0.1 (2002-08-13)"
#define MOD_CAP     "frame rate conversion filter"
#define MOD_AUTHOR  "Thomas Oestreich"

#include "transcode.h"
#include "filter.h"

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/


// demo filter, it does nothing!

int tc_filter(frame_list_t *ptr_, char *options)
{
  vframe_list_t *ptr = (vframe_list_t *)ptr_;
  static int mycount=0;
  static int ofps=25;
  vob_t *vob=NULL;

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
    if (options)
      ofps=atoi(options);

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

  if(ptr->tag & TC_POST_S_PROCESS && ptr->tag & TC_VIDEO) {

    //printf("mycount (%d) ptr->id (%d) (%f)\n", mycount, ptr->id, (double)mycount*15.0/25.0);
    if (((double)mycount*15.0)/25.0 < (double)ptr->id) {
      ptr->attributes |= TC_FRAME_IS_CLONED;
    }

    // converts from 15 fps to 25 fps

    ++mycount;

  }

  return(0);
}
