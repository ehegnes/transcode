/*
 *  filter_resample.c
 *
 *  Copyright (C) Thomas Östreich - February 2002
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

#define MOD_NAME    "filter_resample.so"
#define MOD_VERSION "v0.1.2 (2002-02-21)"
#define MOD_CAP     "audio resampling filter plugin"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "resample.h"

#define RESAMPLE_BUFFER_SIZE 8192

static char resample_buffer[RESAMPLE_BUFFER_SIZE];
static int bytes_per_sample;
static int error;

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


int tc_filter(aframe_list_t *ptr, char *options)
{

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

    bytes_per_sample = vob->a_chan * vob->a_bits/8;
    
    if((int) (bytes_per_sample * vob->mp3frequency / vob->fps) > RESAMPLE_BUFFER_SIZE) return(1);
    
    if (!vob->a_rate || !vob->mp3frequency) {
	fprintf(stderr, "[%s] Invalid settings\n", MOD_NAME);
	error = 1;
	return -1;
    }
    filter_resample_init(vob->a_rate, vob->mp3frequency);
    
    //this will force this resample filter to do the job, not
    //the export module.
    
    vob->a_rate=vob->mp3frequency;
    vob->mp3frequency=0;
    
    return(0);
  }
  
  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  
  if(ptr->tag & TC_FILTER_CLOSE) {
    
      if (!error)
	  filter_resample_stop(resample_buffer);
    
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
  
  if(ptr->tag & TC_PRE_S_PROCESS && ptr->tag & TC_AUDIO) {

    ptr->audio_size = bytes_per_sample * filter_resample_flow(ptr->audio_buf, ptr->audio_size/bytes_per_sample, resample_buffer);
    
    if(ptr->audio_size<0) ptr->audio_size=0;
    
    memcpy(ptr->audio_buf, resample_buffer, ptr->audio_size);
    
  } 
  
  return(0);
}
