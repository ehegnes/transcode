/*
 *  filter_29to23.c
 *
 *  Copyright (C) Tilmann Bitterberg - September 2002
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

#define MOD_NAME    "filter_29to23.so"
#define MOD_VERSION "v0.2 (2003-02-01)"
#define MOD_CAP     "frame rate conversion filter"

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


static unsigned char *f1 = NULL;
static unsigned char *f2 = NULL;

int tc_filter(vframe_list_t *ptr, char *options)
{

  static vob_t *vob=NULL;

  if (ptr->tag & TC_AUDIO) return 0;

  //----------------------------------
  //
  // filter init
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_INIT) {
    
    if((vob = tc_get_vob())==NULL) return(-1);
    
    // filter init ok.
    
    f1 = malloc (SIZE_RGB_FRAME);
    f2 = malloc (SIZE_RGB_FRAME);
    
    if (!f1 || !f2) {
	    printf("[%s]: Malloc failed in %d\n", MOD_NAME, __LINE__);
	    return -1;
    }

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

      if (f1) free (f1);
      if (f2) free (f2);
      return(0);
  }
  
  //----------------------------------
  //
  // filter frame routine
  //
  //----------------------------------

  if(ptr->tag & TC_POST_PROCESS && vob->im_v_codec == CODEC_RGB) {

      switch ((ptr->id+1)%5) {
	  case 1:
	      break;
	  case 2:
	      memcpy (f1, ptr->video_buf, ptr->v_width*ptr->v_height*3);
	      ptr->attributes |= TC_FRAME_IS_SKIPPED;
	      break;
	  case 3:
	      memcpy (f2, ptr->video_buf, ptr->v_width*ptr->v_height*3);
	      {
		  int i;
		  int u, v, w;
		  for (i = 0; i<ptr->video_size; i++) {
		      v = (int)*(f1+i);
		      w = (int)*(f2+i);

		      u = (v + w)/2;

		      if (u > 255) u = 255;
		      if (u < 1) u = 0;

		      u &= 0xff;

		      *(ptr->video_buf+i) = u;
		  }
	      }

	      break;
	  case 4:
	      memcpy (f1, ptr->video_buf, ptr->v_width*ptr->v_height*3);
	      {
		  int i;
		  int u, v, w;
		  for (i = 0; i<ptr->video_size; i++) {
		      v = (int)*(f1+i);
		      w = (int)*(f2+i);

		      u = (v + w)/2;
		      if (u > 255) u = 255;
		      if (u < 1) u = 0;
		      u &= 0xff;


		      *(ptr->video_buf+i) = u;
		  }
	      }

	      break;
	  case 0:
	      break;
      }
  }
  
  return(0);
}
// vim: sw=4
