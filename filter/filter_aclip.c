/*
 *  filter_aclip.c
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

#define MOD_NAME    "filter_aclip.so"
#define MOD_VERSION "v0.1.0 (02/26/02)"
#define MOD_CAP     "generate audio clips from source"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>

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

static uint64_t total=0;

static int level=10, range=25, range_ctr=0, skip_mode=0;

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/


int tc_filter(aframe_list_t *ptr, char *options)
{

  int n;
  double sum;

  short *s;

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

    if(options!=NULL) n=sscanf(options,"%d:%d", &level, &range);

    range_ctr=range;
    
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

  if(verbose & TC_STATS) printf("[%s] %s/%s %s %s\n", MOD_NAME, vob->mod_path, MOD_NAME, MOD_VERSION, MOD_CAP);
  
  // tag variable indicates, if we are called before
  // transcodes internal video/audo frame processing routines
  // or after and determines video/audio context
  
  if(ptr->tag & TC_PRE_S_PROCESS && ptr->tag & TC_AUDIO && (!ptr->attributes & TC_FRAME_IS_SKIPPED)) {
    
    total += (uint64_t) ptr->audio_size;
    
    s=(short *) ptr->audio_buf;
    
    sum=0;

    for(n=0; n<ptr->audio_size>>1; ++n) {
      sum+=(double) ((int)(*s) * (int)(*s));
      s++;
    }

    if(ptr->audio_size>0) sum = sqrt(sum)/(ptr->audio_size>>1);

    sum *= 1000;

    if(verbose & TC_DEBUG) printf("frame=%d sum=%f\n", ptr->id, sum);
    
    if(sum<level) {

      if(range_ctr == range) {
	
	ptr->attributes |= TC_FRAME_IS_SKIPPED;
	skip_mode=1;
      } else ++range_ctr;
      
    } else {
      
      if(skip_mode) ptr->attributes |= TC_FRAME_IS_KEYFRAME;
      skip_mode = 0;
      range_ctr = 0;
    }
  }
  return(0);
}
