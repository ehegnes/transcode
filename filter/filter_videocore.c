/*
 *  filter_videocore
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

#define MOD_NAME    "filter_videocore.so"
#define MOD_VERSION "v0.0.4 (2003-02-01)"
#define MOD_CAP     "Core video transformations"
#define MOD_AUTHOR  "Thomas, Tilmann"

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
#include "filter.h"
#include "../src/video_trans.h"

// basic parameter
unsigned char gamma_table[256];
static int gamma_table_flag;

typedef struct MyFilterData {
	int deinterlace;
	int flip;
	int mirror;
	int rgbswap;
	int decolor;
	float dgamma;
	int antialias;
	double aa_weight;
	double aa_bias;
} MyFilterData;
	
static MyFilterData *mfd = NULL;

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

static void help_optstr(void) 
{
   printf ("[%s] (%s) help\n", MOD_NAME, MOD_CAP);
}

int tc_filter(vframe_list_t *ptr, char *options)
{

  static vob_t *vob=NULL;

  //----------------------------------
  //
  // filter init
  //
  //----------------------------------


  if (ptr->tag & TC_AUDIO) {
      return 0;
  }

  if(ptr->tag & TC_FILTER_INIT) {

    if((vob = tc_get_vob())==NULL) return(-1);

    if((mfd = (MyFilterData *)malloc (sizeof(MyFilterData))) == NULL) return (-1);
    memset (mfd, 0, sizeof(MyFilterData));

    mfd->deinterlace = 0;
    mfd->flip        = 0;
    mfd->mirror      = 0;
    mfd->rgbswap     = 0;
    mfd->decolor     = 0;
    mfd->dgamma      = 0.0;
    mfd->antialias   = 0;
    mfd->aa_weight   = TC_DEFAULT_AAWEIGHT;
    mfd->aa_bias     = TC_DEFAULT_AABIAS;


    if (options != NULL) {

	optstr_get (options, "deinterlace",  "%d",     &mfd->deinterlace);
	if (optstr_get (options, "flip",    "") >= 0)  mfd->flip = !mfd->flip;
	if (optstr_get (options, "mirror",  "") >= 0)  mfd->mirror = !mfd->mirror;
	if (optstr_get (options, "rgbswap", "") >= 0)  mfd->rgbswap = !mfd->rgbswap;
	if (optstr_get (options, "decolor", "") >= 0)  mfd->decolor = !mfd->decolor;
	optstr_get (options, "dgamma",  "%f",          &mfd->dgamma);
	optstr_get (options, "antialias",  "%d/%f/%f",   
		&mfd->antialias, &mfd->aa_weight, &mfd->aa_bias);

	if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);
    }

    // filter init ok.
    if (verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);

    
    return(0);
  }

  //----------------------------------
  //
  // filter get config
  //
  //----------------------------------

  if(ptr->tag & TC_FILTER_GET_CONFIG && options) {

      char buf[255];
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRYE", "1");

      sprintf (buf, "%d", mfd->deinterlace);
      optstr_param (options, "deinterlace", "same as -I", "%d", buf, "0", "5");

      sprintf (buf, "%d", mfd->flip);
      optstr_param (options, "flip", "same as -z", "", buf);

      sprintf (buf, "%d", mfd->mirror);
      optstr_param (options, "mirror", "same as -l", "", buf);

      sprintf (buf, "%d", mfd->rgbswap);
      optstr_param (options, "rgbswap", "same as -k", "", buf);

      sprintf (buf, "%d", mfd->decolor);
      optstr_param (options, "decolor", "same as -K", "", buf);

      sprintf (buf, "%f", mfd->dgamma);
      optstr_param (options, "dgamma", "same as -G", "%f", buf, "0.0", "3.0");

      sprintf (buf, "%d/%.2f/%.2f", mfd->antialias, mfd->aa_weight, mfd->aa_bias);
      optstr_param (options, "antialias", "same as -C/weight/bias", "%d/%f/%f", buf, 
	      "0", "3", "0.0", "1.0", "0.0", "1.0");

      return 0;
  }

  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  
  if(ptr->tag & TC_FILTER_CLOSE) {

    if (mfd)
	free(mfd);
    mfd = NULL;

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
  
  if((ptr->tag & TC_PRE_PROCESS) && (vob->im_v_codec == CODEC_YUV) && !(ptr->attributes & TC_FRAME_IS_SKIPPED)) {
   if (mfd->deinterlace) {

      switch (mfd->deinterlace) {

	  case 1:
	      yuv_deinterlace_linear(ptr->video_buf, ptr->v_width, ptr->v_height);
	      break;

	  case 2:
	      //handled by encoder
	      break;

	  case 3:
	      deinterlace_yuv_zoom(ptr->video_buf, ptr->v_width, ptr->v_height);
	      break;

	  case 4:
	      deinterlace_yuv_nozoom(ptr->video_buf, ptr->v_width, ptr->v_height);
	      break;

	  case 5:
	      yuv_deinterlace_linear_blend(ptr->video_buf, 
		      ptr->video_buf_Y[ptr->free], ptr->v_width, ptr->v_height);
	      break;
      }

   }
   if (mfd->flip) {
       yuv_flip(ptr->video_buf, ptr->v_width, ptr->v_height);   
   }
   if (mfd->mirror) {
       yuv_mirror(ptr->video_buf, ptr->v_width, ptr->v_height); 
   }
   if (mfd->rgbswap) {
       yuv_swap(ptr->video_buf, ptr->v_width, ptr->v_height);
   }
   if (mfd->dgamma > 0.0) {

      if(!gamma_table_flag) {
	  init_gamma_table(gamma_table, mfd->dgamma);
	  gamma_table_flag = 1;
      }

      yuv_gamma(ptr->video_buf, ptr->v_width * ptr->v_height);
   }
   if (mfd->decolor) {
       yuv_decolor(ptr->video_buf, ptr->v_width * ptr->v_height);
   }
   if (mfd->antialias) {
       init_aa_table(vob->aa_weight, vob->aa_bias);

       //UV components unchanged
       memcpy(ptr->video_buf_Y[ptr->free]+ptr->v_width*ptr->v_height, 
	      ptr->video_buf + ptr->v_width*ptr->v_height, 
	      ptr->v_width*ptr->v_height/2);
    
       yuv_antialias(ptr->video_buf, ptr->video_buf_Y[ptr->free], 
	      ptr->v_width, ptr->v_height, mfd->antialias);
    
       // adjust pointer, zoomed frame in tmp buffer
       ptr->video_buf = ptr->video_buf_Y[ptr->free];
       ptr->free = (ptr->free) ? 0:1;
    
       // no update for frame_list_t *ptr required
   }



  } else if((ptr->tag & TC_PRE_PROCESS) && (vob->im_v_codec == CODEC_RGB)) {
   if (mfd->deinterlace) {

      switch (mfd->deinterlace) {

	  case 1:
	      rgb_deinterlace_linear(ptr->video_buf, ptr->v_width, ptr->v_height);
	      break;

	  case 2:
	      //handled by encoder
	      break;

	  case 3:
	      deinterlace_rgb_zoom(ptr->video_buf, ptr->v_width, ptr->v_height);
	      break;

	  case 4:
	      deinterlace_rgb_nozoom(ptr->video_buf, ptr->v_width, ptr->v_height);
	      break;

	  case 5:
	      rgb_deinterlace_linear_blend(ptr->video_buf, 
		      ptr->video_buf_RGB[ptr->free], ptr->v_width, ptr->v_height);
	      break;
      }

   }
   if (mfd->flip) {
       rgb_flip(ptr->video_buf, ptr->v_width, ptr->v_height);   
   }
   if (mfd->mirror) {
       rgb_mirror(ptr->video_buf, ptr->v_width, ptr->v_height); 
   }
   if (mfd->rgbswap) {
       rgb_swap(ptr->video_buf, ptr->v_width * ptr->v_height);
   }
   if (mfd->dgamma > 0.0) {

      if(!gamma_table_flag) {
	  init_gamma_table(gamma_table, mfd->dgamma);
	  gamma_table_flag = 1;
      }

      rgb_gamma(ptr->video_buf, ptr->v_width * ptr->v_height * ptr->v_bpp>>3);
   }
   if (mfd->decolor) {
      rgb_decolor(ptr->video_buf, ptr->v_width * ptr->v_height * ptr->v_bpp>>3);
   }
   if (mfd->antialias) {

       init_aa_table(vob->aa_weight, vob->aa_bias);
    
       rgb_antialias(ptr->video_buf, ptr->video_buf_RGB[ptr->free], 
	       ptr->v_width, ptr->v_height, vob->antialias);

       // adjust pointer, zoomed frame in tmp buffer
       ptr->video_buf = ptr->video_buf_RGB[ptr->free];
       ptr->free = (ptr->free) ? 0:1;

   }

  }
  
  return(0);
}

