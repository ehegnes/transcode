/*
 *  filter_testframe.c
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

#define MOD_NAME    "filter_testframe.so"
#define MOD_VERSION "v0.1.3 (2003-09-04)"
#define MOD_CAP     "generate stream of testframes"

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
#include "optstr.h"

static int mode=0;
static vob_t *vob=NULL;

void generate_rgb_frame(char *buffer, int width, int height)
{
  int n, j, row_bytes;
  
  row_bytes = width*3; 

  memset(buffer, 0, width*height*3);

  switch(mode) {
  
  case 0:
      
      for(n=0; n<height; ++n) {
	  
	  if(n & 1) {
	      for(j=0; j<row_bytes; ++j) buffer[n*row_bytes+j] = 255 & 0xff;
	  } else { 
	      for(j=0; j<row_bytes; ++j) buffer[n*row_bytes+j] = 0;
	  }
      }
      
      break;
      
  case 1:
      
      for(n=0; n<height*width; n=n+2) {
	  buffer[n*3]   = 255 & 0xff;
	  buffer[n*3+1] = 255 & 0xff;
	  buffer[n*3+2] = 255 & 0xff;
      } 
      
      break;

  case 2:  //red picture

    for(n=0; n<height*width; ++n) {
      buffer[n*3]   = 255 & 0xff;
      buffer[n*3+1] = 255 & 0x00;
      buffer[n*3+2] = 255 & 0x00;
    } 
    break;

  case 3:  //green picture

    for(n=0; n<height*width; ++n) {
      buffer[n*3]   = 255 & 0x00;
      buffer[n*3+1] = 255 & 0xff;
      buffer[n*3+2] = 255 & 0x00;
    } 
    break;
  case 4:  //blue

    for(n=0; n<height*width; ++n) {
      buffer[n*3]   = 255 & 0x00;
      buffer[n*3+1] = 255 & 0x00;
      buffer[n*3+2] = 255 & 0xff;
    } 
    break;
  }
}

void generate_yuv_frame(char *buffer, int width, int height)
{
  int n, j, row_bytes;
  
  row_bytes = width; 

  memset(buffer, 0x80, width*height*3/2);

  switch(mode) {
      
  case 0:
      
      for(n=0; n<height; ++n) {
	  
	  if(n & 1) {
	      for(j=0; j<row_bytes; ++j) buffer[n*row_bytes+j]   = 255 & 0xff;
	  } else { 
	      for(j=0; j<row_bytes; ++j) buffer[n*row_bytes+j]   = 0;
	  }
      }
      
      break;
      
  case 1:
      
      for(n=0; n<height*width; ++n) buffer[n]=(n&1)?255 & 0xff:0;
      
      break;

  case 5: // from libavformat
      {
	  static int indx = 0;
	  int x, y;
	  unsigned char 
	      *Y = buffer, 
	      *U=buffer+width*height, 
	      *V=buffer+width*height*5/4;

	  for(y=0;y<height;y++) {
	      for(x=0;x<width;x++) {
		  Y[y * width + x] = x + y + indx * 3;
	      }
	  }
    
	  /* Cb and Cr */
	  for(y=0;y<height/2;y++) {
	      for(x=0;x<width/2;x++) {
		  U[y * width/2 + x] = 128 + y + indx * 2;
		  V[y * height + x] = 64 + x + indx * 5;
	      }
	  }
	  indx++;
      }
      break;
  }
}


/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/


static int is_optstr(char *options)
{
    if (strchr(options, 'm')) return 1;
    if (strchr(options, 'h')) return 1;
    if (strchr(options, '=')) return 1;
    return 0;
}

int tc_filter(vframe_list_t *ptr, char *options)
{

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


  if(ptr->tag & TC_FILTER_GET_CONFIG) {

      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, "Thomas Oestreich", "VRYE", "1");
      optstr_param (options, "mode",   "Choose the test pattern (0-4 interlaced, 5 colorfull)", "%d", "0", "0", "5");
  }

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

    if (options) {
	if (is_optstr(options)) {
	    optstr_get(options, "mode", "%d", &mode);
	} else 
	    sscanf(options, "%d", &mode);
    }

    if(mode <0) { fprintf(stderr, "[%s] Invalid mode\n", MOD_NAME); return(-1); }
    
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
  
  if(ptr->tag & TC_PRE_PROCESS && ptr->tag & TC_VIDEO && !(ptr->attributes & TC_FRAME_IS_SKIPPED)) {
    
    if(vob->im_v_codec==CODEC_RGB) {
      generate_rgb_frame(ptr->video_buf, ptr->v_width, ptr->v_height);
    } else {
      generate_yuv_frame(ptr->video_buf, ptr->v_width, ptr->v_height);
    }
  }
  return(0);
}
