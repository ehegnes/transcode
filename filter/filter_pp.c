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
#define MOD_VERSION "v1.1 (2002-11-14)"
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
#include <postproc/postprocess.h>
#include "../aclib/ac.h"

static int pp_init_done = 0;

static pp_mode_t *mode=NULL;
static pp_context_t *context=NULL;

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/


int tc_filter(vframe_list_t *ptr, char *options)
{

  static vob_t *vob=NULL;

  int ppStride[3];
  
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

    mode = pp_get_mode_by_name_and_quality(options, PP_QUALITY_MAX);

    if(mode==NULL) {
      fprintf(stderr, "[%s] internal error (pp_get_mode_by_name_and_quality)\n", MOD_NAME);
      return(-1);
    }

    if(tc_accel & MM_MMX) context = pp_get_context(vob->ex_v_width, vob->ex_v_height, PP_CPU_CAPS_MMX);
    
    if(tc_accel & MM_3DNOW) context = pp_get_context(vob->ex_v_width, vob->ex_v_height, PP_CPU_CAPS_3DNOW);

    if(context==NULL) {
      fprintf(stderr, "[%s] internal error (pp_get_context)\n", MOD_NAME);
      return(-1);
    }
    
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

    pp_free_mode(mode);
    pp_free_context(context);

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
  
  if( (pp_init_done) && (ptr->tag & TC_POST_M_PROCESS) && (ptr->tag & TC_VIDEO))  
  {
    unsigned char *pp_page[3];
    
    if (vob->im_v_codec != CODEC_RGB) 
    {
      pp_page[0] = ptr->video_buf;
      pp_page[1] = pp_page[0] + (vob->ex_v_width * vob->ex_v_height);
      pp_page[2] = pp_page[1] + (vob->ex_v_width * vob->ex_v_height)/4;

      ppStride[0] = vob->ex_v_width;
      ppStride[1] = ppStride[2] = vob->ex_v_width>>1;
       
      pp_postprocess(pp_page, ppStride, 
		     pp_page, ppStride,
		     vob->ex_v_width, vob->ex_v_height,
		     NULL, 0, mode, context, 0);
    }
  }
  
  return(0);
}

