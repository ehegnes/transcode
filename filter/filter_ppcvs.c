/*
 *  filter_pp.c
 *
 *  Copyright (C) Gerhard Monzel - Januar 2002
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

#define MOD_NAME    "filter_pp.so"
#define MOD_VERSION "v1.0 (2002-01-03)"
#define MOD_CAP     "postprocess filters"

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
#include "postprocess.h"

static int pp_init_done = 0;

//-- dummy stuff --
//-----------------
int divx_quality;

extern int readNPPOpt(void *conf, char *arg);

void odivx_postprocess(unsigned char *src[], int src_stride,
                       unsigned char *dst[], int dst_stride,
                       int w, int h,
                       QP_STORE_T *QP_store, int QP_Stride, int mode)
{
  return;
}  
//-----------------                      

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/


int tc_filter(vframe_list_t *ptr, char *options)
{

  static vob_t *vob=NULL;
  
  //----------------------------------
  //
  // filter init
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_INIT) 
  {
    int verbose_sav, i;
       
    if((vob = tc_get_vob())==NULL) return(-1);
         
    if (vob->im_v_codec == CODEC_RGB)
    {
      fprintf(stderr, "[%s] error: filter is not capable for RGB-Mode !\n", MOD_NAME);
      return(-1);
    }
    
    if (!options || !strlen(options))
    {
      fprintf(stderr, "[%s] error: this filter needs options !\n", MOD_NAME);
      return(-1);
    }

    for (i=0; i<strlen(options); i++)
	    if (options[i] == ' ')
		    options[i] = ',';
    
    verbose_sav = verbose;
    if (verbose < 2) verbose = 0;
  
    if (readNPPOpt("", options) == -1) 
    {
      fprintf(stderr, "[%s] error in parsing filter options (%s)!\n", MOD_NAME, options);
      verbose = verbose_sav;
      return(-1);
    
    }
    verbose = verbose_sav;    
    
    // filter init ok.
    pp_init_done = 1;
    if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);
    return(0);
  }

  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  
  if(ptr->tag & TC_FILTER_CLOSE) 
  {
    pp_init_done = 0;
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
  
  if( (pp_init_done) && (ptr->tag & TC_POST_PROCESS) && (ptr->tag & TC_VIDEO))  
  {
    unsigned char *pp_page[3];
    
    if (vob->im_v_codec != CODEC_RGB) 
    {
      pp_page[0] = ptr->video_buf;
      pp_page[1] = pp_page[0] + (vob->ex_v_width * vob->ex_v_height);
      pp_page[2] = pp_page[1] + (vob->ex_v_width * vob->ex_v_height)/4;
       
      postprocess(pp_page, vob->ex_v_width, 
                  pp_page, vob->ex_v_width,
                  vob->ex_v_width, vob->ex_v_height,
                  NULL, 0,
                  GET_PP_QUALITY_MAX);    
    }
  }
  
  return(0);
}

