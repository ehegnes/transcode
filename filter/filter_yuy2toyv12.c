/*
 *  filter_yuy2tov12.c
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

#define MOD_NAME    "filter_yuy2tov12.so"
#define MOD_VERSION "v0.0.1 (2001-10-18)"
#define MOD_CAP     "YUY2 to YV12 converter plugin"

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
#include <unistd.h>
#include <inttypes.h>

#include "transcode.h"
#include "framebuffer.h"


void yuy2toyv12(char *dest, char *input, int width, int height) 
{

    int i,j,w2;
    char *y, *u, *v;

    w2 = width/2;

    //I420
    y = dest;
    v = dest+width*height;
    u = dest+width*height*5/4;
    
    for (i=0; i<height; i+=2) {
      for (j=0; j<w2; j++) {
	
	/* packed YUV 422 is: Y[i] U[i] Y[i+1] V[i] */
	*(y++) = *(input++);
	*(u++) = *(input++);
	*(y++) = *(input++);
	*(v++) = *(input++);
      }
      
      //down sampling
      
      for (j=0; j<w2; j++) {
	/* skip every second line for U and V */
	*(y++) = *(input++);
	input++;
	*(y++) = *(input++);
	input++;
      }
    }
}

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

static  char *video_buffer=NULL;

int tc_filter(vframe_list_t *ptr, char *options)
{
  
  static vob_t *vob=NULL;
  int bytes;

  //----------------------------------
  //
  // filter init
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_INIT) {
    
    if((vob = tc_get_vob())==NULL) return(-1);
    
    if((video_buffer = (char *)calloc(1, SIZE_RGB_FRAME))==NULL) {
      fprintf(stderr, "(%s) out of memory", __FILE__);
      return(-1);
    }
    
    // filter init ok.
    
    if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);
    
    return(0);
  }

  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  
  if(ptr->tag & TC_FILTER_CLOSE) {
    free(video_buffer);
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
  
  if((ptr->tag & TC_PRE_PROCESS) && (ptr->tag & TC_VIDEO) && vob->im_v_codec==CODEC_YUV &&
  	(!ptr->attributes & TC_FRAME_IS_SKIPPED))  {
    
    bytes = ptr->v_width * ptr->v_height * 3/2;
    
    yuy2toyv12(video_buffer, ptr->video_buf, ptr->v_width, ptr->v_height); 
    
    memcpy(ptr->video_buf, video_buffer, bytes);
    
  }
  
  return(0);
}

