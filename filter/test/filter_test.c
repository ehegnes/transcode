/*
 *  filter_test.c
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

#define MOD_NAME    "filter_test.so"
#define MOD_VERSION "v0.0.1 (2002-11-04)"
#define MOD_CAP     "test filter plugin"

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "aclib/ac.h"

static char *buffer;

static int ac=0, loop=0;

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

static unsigned char *bufalloc(size_t size)
{

#ifdef HAVE_GETPAGESIZE
   int buffer_align=getpagesize();
#else
   int buffer_align=0;
#endif

   char *buf = malloc(size + buffer_align);

   int adjust;

   if (buf == NULL) {
       fprintf(stderr, "(%s) out of memory", __FILE__);
   }
   
   adjust = buffer_align - ((int) buf) % buffer_align;

   if (adjust == buffer_align)
      adjust = 0;

   return (unsigned char *) (buf + adjust);
}


/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/


int tc_filter(vframe_list_t *ptr, char *options)
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

    buffer = bufalloc(SIZE_RGB_FRAME);

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

    for(n=0; n<loop; ++n) {
      
      switch (ac) {

#ifdef HAVE_MMX	
      case 1:
	ac_memcpy_mmx(buffer, ptr->video_buf, ptr->video_size);
	memset(ptr->video_buf, 0, ptr->video_size);
	ac_memcpy_mmx(ptr->video_buf, buffer, ptr->video_size);
	break;
#endif

#ifdef HAVE_SSE	
      case 2:
	ac_memcpy_sse(buffer, ptr->video_buf, ptr->video_size);
	memset(ptr->video_buf, 0, ptr->video_size);
	ac_memcpy_sse(ptr->video_buf, buffer, ptr->video_size);
	break;
#endif

      case 0:
      default:
	memcpy(buffer, ptr->video_buf, ptr->video_size);
	memset(ptr->video_buf, 0, ptr->video_size);
	memcpy(ptr->video_buf, buffer, ptr->video_size);
	break;
      } //ac
    } //loop


  } //slot 
  
  return(0);
}
