/*
 *  filter_nored
 *
 *  Copyright (C) Tilmann Bitterberg - June 2002
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

#define MOD_NAME    "filter_nored.so"
#define MOD_VERSION "v0.1.3 (2003-01-26)"
#define MOD_CAP     "nored the image"
#define MOD_AUTHOR  "Tilmann Bitterberg"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>

/* -------------------------------------------------
 *
 * mandatory include files
 *
 *-------------------------------------------------*/

#include "transcode.h"
#include "framebuffer.h"
#include "optstr.h"

// basic parameter

typedef struct MyFilterData {
	unsigned int start;
	unsigned int end;
	unsigned int step;
	int subst;
	int boolstep;
} MyFilterData;
	
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
   printf ("    nored an image\n");
   printf ("* Options\n");
   printf ("    'range' apply filter to [start-end]/step frames [0-oo/1]\n");
}

int tc_filter(vframe_list_t *ptr, char *options)
{

  static vob_t *vob=NULL;

  static int width, height;
  static int size;
  int h;
  
  if(ptr->tag & TC_FILTER_GET_CONFIG) {
      char buf[128];
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VYE", "1");

      snprintf(buf, 128, "%ux%u/%d", mfd->start, mfd->end, mfd->step);
      optstr_param (options, "range", "apply filter to [start-end]/step frames", 
	      "%u-%u/%d", buf, "0", "oo", "0", "oo", "1", "oo");

      snprintf(buf, 128, "%d", mfd->subst);
      optstr_param (options, "subst", "substract N red from Cr", 
	      "%d", buf, "-127", "127" );

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
    mfd->subst=2;

    if (options != NULL) {
    
	if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);

	optstr_get (options, "range",  "%u-%u/%d",    &mfd->start, &mfd->end, &mfd->step);
	optstr_get (options, "subst",  "%d",    &mfd->subst);
    }


    if (verbose > 1) {
	printf (" nored Image Settings:\n");
	printf ("             range = %u-%u\n", mfd->start, mfd->end);
	printf ("              step = %u\n", mfd->step);
    }

    if (options)
	if (optstr_lookup (options, "help")) {
	    help_optstr();
	}

    if (mfd->start % mfd->step == 0)
      mfd->boolstep = 0;
    else 
      mfd->boolstep = 1;

    width = vob->ex_v_width;
    height = vob->ex_v_height;

    if (vob->im_v_codec == CODEC_RGB) {
	fprintf(stderr, "[%s] This filter is only capable of YUV mode\n", MOD_NAME);
	return -1;
    } else 
      size = width*3/2;

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
  
  if((ptr->tag & TC_POST_PROCESS) && (ptr->tag & TC_VIDEO) && !(ptr->attributes & TC_FRAME_IS_SKIPPED))  {
    char *p;

    if (mfd->start <= ptr->id && ptr->id <= mfd->end && ptr->id%mfd->step == mfd->boolstep) {

      p = ptr->video_buf + ptr->v_height*ptr->v_width;

      for (h = 0; h < height*width/4; h++) {
	*p = (*p-mfd->subst)&0xff;
	p++;
      }
    }
  }
  
  return(0);
}

