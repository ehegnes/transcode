/*
 *  import_mpeg3.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  This file is part of transcode, a linux video stream  processing tool
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include "transcode.h"

#define MOD_NAME    "import_lve.so"
#define MOD_VERSION "v1.0 (12/15/02)"
#define MOD_CODEC   "(video/audio) MPEG/VOB/LVE"

#define MOD_PRE lve
#include "import_def.h"

#ifdef __cplusplus
}
#endif


#include "lve/lve_read.h"
//#include "lve/lve_read.c"

//-- main lve-reader context --
//-----------------------------
static T_LVE_READ_CTX *lve_ctx = NULL;


//-- import filter stuff --
//-------------------------
int verbose_flag    = TC_QUIET;
int capability_flag = TC_CAP_YUV|TC_CAP_PCM;


static void adjust_params(vob_t *vob, T_LVE_READ_CTX *ctx)
{
  //--------------------------
  //-- adjust import values --
  //--------------------------
    
  //-- video --
  vob->format_flag = 0;       // PAL video
  vob->fps         = PAL_FPS; // lve_ctx->frame_rate;
  vob->im_v_height = ctx->src_h;
  vob->im_v_width  = ctx->src_w;
  vob->im_v_size   = (ctx->pic_size_l*3)/2;
  vob->ex_asr      = ctx->aspect;
  vob->im_asr	   = ctx->aspect;
    
  //-- audio --
  vob->a_chan = 2;
  vob->a_bits = 16;
  if (lve_ctx->has_audio)
  {
    vob->a_rate = lve_ctx->sample_rate;
  }
  vob->im_a_size = (int)((vob->a_rate/PAL_FPS) * vob->a_chan * (vob->a_bits/8));
  vob->im_a_size = (vob->im_a_size>>2)<<2;
  
  
  //--------------------------
  //-- adjust export values --
  //--------------------------

  //-- video --
  vob->ex_v_height = vob->im_v_height;
  vob->ex_v_width  = vob->im_v_width;
  
  //-- (-j) pre-clipping --
  //-----------------------
  if (vob->im_clip_top  || vob->im_clip_bottom) 
    vob->ex_v_height -= (vob->im_clip_top + vob->im_clip_bottom);
  if (vob->im_clip_left || vob->im_clip_right) 
    vob->ex_v_width  -= (vob->im_clip_left + vob->im_clip_right);  
  
  //-- (-B) h/v resizer --
  //----------------------
  if (vob->hori_resize1)
    vob->ex_v_width -= vob->hori_resize1*32;
  if (vob->vert_resize1)
    vob->ex_v_height -= vob->vert_resize1*32;
  
  //-- (-Z) zoom --
  //---------------
  if (vob->zoom_width)
    vob->ex_v_width = vob->zoom_width;
  if (vob->zoom_height)
    vob->ex_v_height = vob->zoom_height;
    
  //-- (-Y) post clipping --
  //------------------------      
  if (vob->ex_clip_top || vob->ex_clip_bottom)  
    vob->ex_v_height -= (vob->ex_clip_top + vob->ex_clip_bottom);
  
  if (vob->ex_clip_left || vob->ex_clip_right)
    vob->ex_v_width -= (vob->ex_clip_left + vob->ex_clip_right);     

  //-- (-r) fast rescale-settings --
  //--------------------------------
  if (vob->reduce_h == 2 || vob->reduce_h == 4 || vob->reduce_h == 8)
    vob->ex_v_height /= vob->reduce_h;
  if (vob->reduce_w == 2 || vob->reduce_w == 4 || vob->reduce_w == 8)
    vob->ex_v_width /= vob->reduce_w;
  
  vob->ex_v_size = (vob->ex_v_width * vob->ex_v_height * 3)/2;

  //-- audio --
  vob->ex_a_size = vob->im_a_size;
   
  //-- must be initialized here ! --
  ctx->aframe_size = vob->im_a_size;
     
}


/* ------------------------------------------------------------
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{

  if(param->flag == TC_AUDIO) 
  {
    //-- we are reader ! --
    param->fd = NULL; 
    return(0);
  }
  
  if(param->flag == TC_VIDEO) 
  {
    if (lve_ctx) return (0);
    
    if (lr_file_chk(vob->video_in_file))
      lve_ctx = lr_init(vob->a_track);
    else
    {
      fprintf(stderr, "ERROR: (%s) is no lve edit-list\n", 
              vob->video_in_file);
      return(TC_IMPORT_ERROR);
    }
    
    //-- adjust parameters --
    //-----------------------
    adjust_params(vob, lve_ctx);
    
    //-- we are reader ! --
    param->fd = NULL;
    
    return(0);
  }
  
  return(TC_IMPORT_ERROR);
  
}

/* ------------------------------------------------------------ 
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/

MOD_decode
{
  if(param->flag == TC_AUDIO) 
  {
    if (!lr_get_samples(lve_ctx, param->buffer, param->size)) 
      return (TC_IMPORT_ERROR);
    return(0);
  }
  
  if(param->flag == TC_VIDEO) 
  {
    //-- read/decode video frame --
    lve_ctx->py = param->buffer;
    lve_ctx->pu = lve_ctx->py + lve_ctx->pic_size_l;
    lve_ctx->pv = lve_ctx->pu + lve_ctx->pic_size_c;
    if (!lr_get_frame(lve_ctx)) return (TC_IMPORT_ERROR);
    
    //-- force size --
    param->size = (lve_ctx->pic_size_l * 3)>>1;
    //memcpy(param->buffer, framebuffer, param->size);
    
    return(0);
  }
  return(TC_IMPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{  
    
    if(param->fd != NULL) pclose(param->fd);

    if(param->flag == TC_VIDEO) 
    {
      lr_cleanup();
      return (0);
    }

    if(param->flag == TC_AUDIO) 
    {
      return(0);
    }
    return(TC_IMPORT_ERROR);
}


