/*
 *  filter_null.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
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

#define MOD_NAME    "filter_null.so"
#define MOD_VERSION "v0.1 (2001-08-17)"
#define MOD_CAP     "demo filter plugin"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

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

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/


// demo filter, it does nothing!

int tc_filter(vframe_list_t *ptr, char *options)
{

  int pre=0, vid=0;

  vob_t *vob=NULL;

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
  
  
  if(verbose & TC_STATS) {
    
    printf("[%s] %s/%s %s %s\n", MOD_NAME, vob->mod_path, MOD_NAME, MOD_VERSION, MOD_CAP);
    
    // tag variable indicates, if we are called before
    // transcodes internal video/audo frame processing routines
    // or after and determines video/audio context
    
    if(ptr->tag & TC_PRE_PROCESS) pre=1;
    if(ptr->tag & TC_POST_PROCESS) pre=0;
    
    if(ptr->tag & TC_VIDEO) vid=1;
    if(ptr->tag & TC_AUDIO) vid=0;
    
    printf("[%s] frame [%06d] %s %16s call\n", MOD_NAME, ptr->id, (vid)?"(video)":"(audio)", (pre)?"pre-process filter":"post-process filter");
    
  }
  
  return(0);
}
