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
#define MOD_VERSION "v0.1.0 (2003-11-01)"
#define MOD_CAP     "detect clipping parameters (-j or -Y)"
#define MOD_AUTHOR  "Tilmann Bitterberg, A'rpi"

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
    /* configurable */
	unsigned int start;
	unsigned int end;
	unsigned int step;
	int post;
	int limit;
	int x1, y1, x2, y2;

    /* internal */
	int stride, bpp;
	int fno;
	int boolstep;
} MyFilterData;
	
static MyFilterData *mfd[16];

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
   printf ("    Detect black regions on top, bottom, left and right of an image\n");
   printf ("    It is suggested that the filter is run for around 100 frames.\n");
   printf ("    It will print its detected parameters every frame. If you\n");
   printf ("    don't notice any change in the printout for a while, the filter\n");
   printf ("    probably won't find any other values.\n");
   printf ("    The filter converges, meaning it will learn.\n");
   printf ("* Options\n");
   printf ("    'range' apply filter to [start-end]/step frames [0-oo/1]\n");
   printf ("    'limit' the sum of a line must be below this limit to be considered black\n");
   printf ("     'post' run as a POST filter (calc -Y instead of the default -j)\n");
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

  if (ptr->tag & TC_AUDIO)
    return 0;

  if(ptr->tag & TC_FILTER_GET_CONFIG) {
      char buf[128];
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRYEOM", "1");

      snprintf(buf, 128, "%u-%u/%d", mfd[ptr->filter_id]->start, mfd[ptr->filter_id]->end, mfd[ptr->filter_id]->step);
      optstr_param (options, "range", "apply filter to [start-end]/step frames", 
	      "%u-%u/%d", buf, "0", "oo", "0", "oo", "1", "oo");
      optstr_param (options, "limit", "the sum of a line must be below this limit to be considered as black", "%d", "24", "0", "255");
      optstr_param (options, "post", "run as a POST filter (calc -Y instead of the default -j)", "", "0");

      return 0;
  }
  
  //----------------------------------
  //
  // filter init
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_INIT) {

    if((vob = tc_get_vob())==NULL) return(-1);

    if((mfd[ptr->filter_id] = (MyFilterData *)malloc (sizeof(MyFilterData))) == NULL) return (-1);


    mfd[ptr->filter_id]->start=0;
    mfd[ptr->filter_id]->end=(unsigned int)-1;
    mfd[ptr->filter_id]->step=1;
    mfd[ptr->filter_id]->limit=24;
    mfd[ptr->filter_id]->post = 0;

    if (options != NULL) {
    
	if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);

	optstr_get (options, "range",  "%u-%u/%d",    &mfd[ptr->filter_id]->start, &mfd[ptr->filter_id]->end, &mfd[ptr->filter_id]->step);
	optstr_get (options, "limit",  "%d",    &mfd[ptr->filter_id]->limit);
	if (optstr_get (options, "post",  "")>=0) mfd[ptr->filter_id]->post = 1;
    }


    if (verbose > 1) {
	printf (" detectclipping#%d Settings:\n", ptr->filter_id);
	printf ("              range = %u-%u\n", mfd[ptr->filter_id]->start, mfd[ptr->filter_id]->end);
	printf ("               step = %u\n", mfd[ptr->filter_id]->step);
	printf ("              limit = %u\n", mfd[ptr->filter_id]->limit);
	printf ("    run POST filter = %s\n", mfd[ptr->filter_id]->post?"yes":"no");
    }

    if (options)
	if (optstr_lookup (options, "help")) {
	    help_optstr();
	}

    if (mfd[ptr->filter_id]->start % mfd[ptr->filter_id]->step == 0)
      mfd[ptr->filter_id]->boolstep = 0;
    else 
      mfd[ptr->filter_id]->boolstep = 1;

    if (!mfd[ptr->filter_id]->post) {
	mfd[ptr->filter_id]->x1 = vob->im_v_width;
	mfd[ptr->filter_id]->y1 = vob->im_v_height;
    } else {
	mfd[ptr->filter_id]->x1 = vob->ex_v_width;
	mfd[ptr->filter_id]->y1 = vob->ex_v_height;
    }
    mfd[ptr->filter_id]->x2 = 0;
    mfd[ptr->filter_id]->y2 = 0;
    mfd[ptr->filter_id]->fno = 0;

    if (vob->im_v_codec == CODEC_YUV) {
	mfd[ptr->filter_id]->stride = mfd[ptr->filter_id]->post?vob->ex_v_width:vob->im_v_width;
	mfd[ptr->filter_id]->bpp = 1;
    } else if (vob->im_v_codec == CODEC_RGB) {
	mfd[ptr->filter_id]->stride = mfd[ptr->filter_id]->post?(vob->ex_v_width*3):(vob->im_v_width*3);
	mfd[ptr->filter_id]->bpp = 3;
    } else {
	fprintf (stderr, "[%s] unsupported colorspace\n", MOD_NAME);
	return -1;
    }




    // filter init ok.
    if (verbose) printf("[%s] %s %s #%d\n", MOD_NAME, MOD_VERSION, MOD_CAP, ptr->filter_id);

    
    return(0);
  }

  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  
  if(ptr->tag & TC_FILTER_CLOSE) {

    if (mfd[ptr->filter_id]) { 
	free(mfd[ptr->filter_id]);
    }
    mfd[ptr->filter_id]=NULL;

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
  
  if(((ptr->tag & TC_PRE_M_PROCESS && !mfd[ptr->filter_id]->post) || 
      (ptr->tag & TC_POST_M_PROCESS && mfd[ptr->filter_id]->post)) && 
     !(ptr->attributes & TC_FRAME_IS_SKIPPED))  {

    int y;
    char *p = ptr->video_buf;
    int l,r,t,b;

    if (mfd[ptr->filter_id]->fno++ < 3)
	return 0;

    if (mfd[ptr->filter_id]->start <= ptr->id && ptr->id <= mfd[ptr->filter_id]->end && ptr->id%mfd[ptr->filter_id]->step == mfd[ptr->filter_id]->boolstep) {

    for (y = 0; y < mfd[ptr->filter_id]->y1; y++) {
	if(checkline(p+mfd[ptr->filter_id]->stride*y, mfd[ptr->filter_id]->bpp, ptr->v_width, mfd[ptr->filter_id]->bpp) > mfd[ptr->filter_id]->limit) {
	    mfd[ptr->filter_id]->y1 = y;
	    break;
	}
    }

    for (y=ptr->v_height-1; y>mfd[ptr->filter_id]->y2; y--) {
	if (checkline(p+mfd[ptr->filter_id]->stride*y, mfd[ptr->filter_id]->bpp, ptr->v_width, mfd[ptr->filter_id]->bpp) > mfd[ptr->filter_id]->limit) {
	    mfd[ptr->filter_id]->y2 = y;
	    break;
	}
    }
    
    for (y = 0; y < mfd[ptr->filter_id]->x1; y++) {
	if(checkline(p+mfd[ptr->filter_id]->bpp*y, mfd[ptr->filter_id]->stride, ptr->v_height, mfd[ptr->filter_id]->bpp) > mfd[ptr->filter_id]->limit) {
	    mfd[ptr->filter_id]->x1 = y;
	    break;
	}
    }

    for (y = ptr->v_width-1; y > mfd[ptr->filter_id]->x2; y--) {
	if(checkline(p+mfd[ptr->filter_id]->bpp*y, mfd[ptr->filter_id]->stride, ptr->v_height, mfd[ptr->filter_id]->bpp) > mfd[ptr->filter_id]->limit) {
	    mfd[ptr->filter_id]->x2 = y;
	    break;
	}
    }


    t = (mfd[ptr->filter_id]->y1+1)&(~1);
    l = (mfd[ptr->filter_id]->x1+1)&(~1);
    b = ptr->v_height - (mfd[ptr->filter_id]->y2+1)&(~1);
    r = ptr->v_width - (mfd[ptr->filter_id]->x2+1)&(~1);

    printf("[detectclipping#%d] valid area: X: %d..%d Y: %d..%d  -> %s %d,%d,%d,%d\n",
	ptr->filter_id,
	mfd[ptr->filter_id]->x1,mfd[ptr->filter_id]->x2,
	mfd[ptr->filter_id]->y1,mfd[ptr->filter_id]->y2,
	mfd[ptr->filter_id]->post?"-Y":"-j", 
	t, l, b, r
	  );
    
    }

  }
  
  return(0);
}

