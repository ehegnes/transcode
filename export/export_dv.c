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
#include "vid_aux.h"

#define MOD_NAME    "export_dv.so"
#define MOD_VERSION "v0.5 (2003-07-24)"
#define MOD_CODEC   "(video) Digital Video | (audio) MPEG/AC3/PCM"

#define MOD_PRE dv
#include "export_def.h"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_YUV|TC_CAP_AC3;

static unsigned char *target; //[TC_FRAME_DV_PAL];

static avi_t *avifile=NULL;

static int frame_size=0, format=0;

static int dv_yuy2_mode=0;

#ifdef LIBDV_095
static dv_encoder_t *encoder = NULL;
static unsigned char *pixels[3], *tmp_buf;
#endif

static unsigned char *bufalloc(size_t size)
{

#ifdef HAVE_GETPAGESIZE
   long buffer_align=getpagesize();
#else
   long buffer_align=0;
#endif

   char *buf = malloc(size + buffer_align);

   long adjust;

   if (buf == NULL) {
       fprintf(stderr, "(%s) out of memory", __FILE__);
   }
   
   adjust = buffer_align - ((long) buf) % buffer_align;

   if (adjust == buffer_align)
      adjust = 0;

   return (unsigned char *) (buf + adjust);
}


/* ------------------------------------------------------------ 
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{
    
    if(param->flag == TC_VIDEO) {

      target = bufalloc(TC_FRAME_DV_PAL);

      if(vob->dv_yuy2_mode) {
	tmp_buf = bufalloc(PAL_W*PAL_H*2); //max frame
	dv_yuy2_mode=1;
      }

#ifdef LIBDV_095
      encoder = dv_encoder_new(FALSE, FALSE, FALSE);
#else
      dvenc_init();
#endif
      
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
  
  // open out file
  if(vob->avifile_out==NULL) 
    if(NULL == (vob->avifile_out = AVI_open_output_file(vob->video_out_file))) {
      AVI_print_error("avi open error");
      exit(TC_EXPORT_ERROR);
    }

  /* save locally */
  avifile = vob->avifile_out;

  if(param->flag == TC_VIDEO) {

    AVI_set_video(vob->avifile_out, vob->ex_v_width, vob->ex_v_height, vob->ex_fps, "DVSD");

    if (vob->avi_comment_fd>0)
	AVI_set_comment_fd(vob->avifile_out, vob->avi_comment_fd);
    
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
    
#ifdef LIBDV_095
    encoder->isPAL = (vob->ex_v_height==PAL_H);
    encoder->is16x9 = FALSE;
    encoder->vlc_encode_passes = 3;
    encoder->static_qno = 0;
    encoder->force_dct = DV_DCT_AUTO;
#else
    dvenc_set_parameter(format, vob->ex_v_height, vob->a_rate);
#endif      
    

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

    time_t now = time(NULL);

    
#ifdef LIBDV_095

      pixels[0] = (char *) param->buffer;
	
      if(encoder->isPAL) {
	pixels[2]=(char *) param->buffer + PAL_W*PAL_H;
	pixels[1]=(char *) param->buffer + (PAL_W*PAL_H*5)/4;
      } else {
	pixels[2]=(char *) param->buffer + NTSC_W*NTSC_H;
	pixels[1]=(char *) param->buffer + (NTSC_W*NTSC_H*5)/4;
      }
      
      if(dv_yuy2_mode) {
	yv12toyuy2(pixels[0], pixels[1], pixels[2], tmp_buf, PAL_W, (encoder->isPAL)? PAL_H : NTSC_H);
	pixels[0]=tmp_buf;
      }
      
    dv_encode_full_frame(encoder, pixels, (format)?e_dv_color_yuv:e_dv_color_rgb, target);

    dv_encode_metadata(target, encoder->isPAL, encoder->is16x9, &now, 0);
    dv_encode_timecode(target, encoder->isPAL, 0);
#else    
    dvenc_frame(param->buffer, NULL, 0, target);
#endif
    
    // write video
    // only keyframes 
    key = 1;
    
    //0.6.2: switch outfile on "r/R" and -J pv
    //0.6.2: enforce auto-split at 2G (or user value) for normal AVI files
    if((uint32_t)(AVI_bytes_written(avifile)+frame_size+16+8)>>20 >= tc_avi_limit) tc_outstream_rotate_request();
    
    if(key) tc_outstream_rotate();

    
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
    
#ifdef LIBDV_095
    dv_encoder_free(encoder);  
#else    
    dvenc_close();
#endif
    
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

