/*
 *  filter_detectclipping
 *
 *  Copyright (C) Tilmann Bitterberg - June 2002
 *    Based on Code from mplayers cropdetect by A'rpi
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

#define MOD_NAME    "filter_detectclipping.so"
#define MOD_VERSION "v0.1.4 (2003-10-12)"
#define MOD_CAP     "detectclipping the image"
#define MOD_AUTHOR  "Tilmann Bitterberg"

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
#include "optstr.h"

// basic parameter

typedef struct MyFilterData {
	unsigned int start;
	unsigned int end;
	unsigned int step;
	int boolstep;
	int limit;
} MyFilterData;
	
typedef struct {
    int tl, tr, bl, br;
} crop;

static MyFilterData *mfd = NULL;

/* should probably honor the other flags too */ 

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

static void help_optstr(void) 
{
   printf ("[%s] (%s) help\n", MOD_NAME, MOD_CAP);
   printf ("* Overview\n");
   printf ("    detect black regions on top and bottom of an image\n");
   printf ("* Options\n");
   printf ("    'range' apply filter to [start-end]/step frames [0-oo/1]\n");
}

static int checkline(unsigned char* src,int stride,int len,int bpp){
    int total=0;
    int div=len;
    switch(bpp){
    case 1:
	while(--len>=0){
	    total+=src[0]; src+=stride;
	}
	break;
    case 3:
    case 4:
	while(--len>=0){
	    total+=src[0]+src[1]+src[2]; src+=stride;
	}
	div*=3;
	break;
    }
    total/=div;
    return total;
}

int tc_filter(vframe_list_t *ptr, char *options)
{

  static vob_t *vob=NULL;

  static int width, height;
  static int size;
  int w, h;
  
  if(ptr->tag & TC_FILTER_GET_CONFIG) {
      char buf[128];
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRY4O", "1");

      snprintf(buf, 128, "%u-%u/%d", mfd->start, mfd->end, mfd->step);
      optstr_param (options, "range", "apply filter to [start-end]/step frames", 
	      "%u-%u/%d", buf, "0", "oo", "0", "oo", "1", "oo");

      return 0;
  }
  
  //----------------------------------
  //
  // filter init
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_INIT) {

    if((vob = tc_get_vob())==NULL) return(-1);

    if((mfd = (MyFilterData *)malloc (sizeof(MyFilterData))) == NULL) return (-1);


    mfd->start=0;
    mfd->end=(unsigned int)-1;
    mfd->step=1;
    mfd->limit=24;

    if (options != NULL) {
    
	if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);

	optstr_get (options, "range",  "%u-%u/%d",    &mfd->start, &mfd->end, &mfd->step);
	optstr_get (options, "limit",  "%d",    &mfd->limit);
    }


    if (verbose > 1) {
	printf (" detectclipping Settings:\n");
	printf ("             range = %u-%u\n", mfd->start, mfd->end);
	printf ("              step = %u\n", mfd->step);
	printf ("             limit = %u\n", mfd->limit);
    }

    if (options)
	if (optstr_lookup (options, "help")) {
	    help_optstr();
	}

    if (mfd->start % mfd->step == 0)
      mfd->boolstep = 0;
    else 
      mfd->boolstep = 1;

    // filter init ok.
    if (verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);

    
    return(0);
  }

  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  
  if(ptr->tag & TC_FILTER_CLOSE) {

    if (mfd) { 
	free(mfd);
    }
    mfd=NULL;

    return(0);

  } /* filter close */
  
  //----------------------------------
  //
  // filter frame routine
  //
  //----------------------------------

    
  // tag variable indicates, if we are called before
  // transcodes internal video/audo frame processing routines
  // or after and determines video/audio context
  
  if((ptr->tag & TC_POST_PROCESS) && (ptr->tag & TC_VIDEO) && 
     !(ptr->attributes & TC_FRAME_IS_SKIPPED))  {

    int y, x;
    char *p = ptr->video_buf;

  }
  
  return(0);
}

