/*
 *  export_dv.c
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

#include <stdio.h>
#include <stdlib.h>
#include "../libdvenc/dvenc.h"
#include "transcode.h"
#include "avilib.h"
#include "aud_aux.h"

#define MOD_NAME    "export_dv.so"
#define MOD_VERSION "v0.1.0 (2001-12-04)"
#define MOD_CODEC   "(video) Digital Video | (audio) MPEG/AC3/PCM"

#define MOD_PRE dv
#include "export_def.h"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_YUV|TC_CAP_AC3;

unsigned char target[TC_FRAME_DV_PAL];

static avi_t *avifile=NULL;

static int frame_size=0;

/* ------------------------------------------------------------ 
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{
    
    if(param->flag == TC_VIDEO) {

      dvenc_init();

      return(0);
    }

    if(param->flag == TC_AUDIO) return(audio_init(vob, verbose_flag));

    // invalid flag
    return(TC_EXPORT_ERROR); 
}

/* ------------------------------------------------------------ 
 *
 * open outputfile
 *
 * ------------------------------------------------------------*/

MOD_open
{
  
  int format;

    
  // open out file
  if(vob->avifile_out==NULL) 
    if(NULL == (vob->avifile_out = AVI_open_output_file(vob->video_out_file))) {
      AVI_print_error("avi open error");
      exit(TC_EXPORT_ERROR);
    }

  /* save locally */
  avifile = vob->avifile_out;

  if(param->flag == TC_VIDEO) {

    AVI_set_video(vob->avifile_out, vob->ex_v_width, vob->ex_v_height, vob->fps, "DVSD");
    
    switch(vob->im_v_codec) {
      
    case CODEC_RGB:
      format=0;
      break;
      
    case CODEC_YUV:
      format=1;
      break;
      
    default:
      
      fprintf(stderr, "[%s] codec not supported\n", MOD_NAME);
      return(TC_EXPORT_ERROR); 
      
      break;
    }

     // for reading
    frame_size = (vob->ex_v_height==PAL_H) ? TC_FRAME_DV_PAL:TC_FRAME_DV_NTSC;

    dvenc_set_parameter(format, vob->ex_v_height, vob->a_rate);
    
    return(0);
  }
  
  
  if(param->flag == TC_AUDIO)  return(audio_open(vob, vob->avifile_out));
  
  // invalid flag
  return(TC_EXPORT_ERROR); 
}   

/* ------------------------------------------------------------ 
 *
 * encode and export
 *
 * ------------------------------------------------------------*/

MOD_encode
{

  int key;

  if(param->flag == TC_VIDEO) { 
    
    dvenc_frame(param->buffer, NULL, 0, target);
    // write video
    
    key = 1;
    
    if(AVI_write_frame(avifile, target, frame_size, key)<0) {
      AVI_print_error("avi video write error");
      
      return(TC_EXPORT_ERROR); 
    }
    return(0);
  }
  
  if(param->flag == TC_AUDIO) return(audio_encode(param->buffer, param->size, avifile));
  
  // invalid flag
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * stop encoder
 *
 * ------------------------------------------------------------*/

MOD_stop 
{
  
  if(param->flag == TC_VIDEO) {
    
    dvenc_close();
    return(0);
  }
  
  if(param->flag == TC_AUDIO) return(audio_stop());
  
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * close outputfiles
 *
 * ------------------------------------------------------------*/

MOD_close
{  

  vob_t *vob = tc_get_vob();
  if(param->flag == TC_AUDIO) return(audio_close());
  
  //outputfile
  if(vob->avifile_out!=NULL) {
    AVI_close(vob->avifile_out);
    vob->avifile_out=NULL;
  }
  
  if(param->flag == TC_VIDEO) return(0);
  
  return(TC_EXPORT_ERROR);  

}

