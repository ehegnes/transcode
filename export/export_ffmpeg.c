/*
 *  export_ffmpeg.c
 *
 *  Copyright (C) Thomas Östreich - March 2002
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

#include "transcode.h"
#include "avilib.h"
#include "aud_aux.h"
#include "../ffmpeg/libavcodec/avcodec.h"

#define MOD_NAME    "export_ffmpeg.so"
#define MOD_VERSION "v0.1.0 (2002-05-29)"
#define MOD_CODEC   "(video) ffmpeg API | (audio) MPEG/AC3/PCM"

#define MOD_PRE ffmpeg
#include "export_def.h"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_YUV|TC_CAP_RGB|TC_CAP_PCM|TC_CAP_AC3|TC_CAP_AUD;

static uint8_t tmp_buffer[SIZE_RGB_FRAME];

//static int codec, width, height;

static AVCodec        *mpa_codec = NULL;
static AVCodecContext mpa_ctx;
static AVPicture      mpa_picture;

static avi_t *avifile=NULL;

//== little helper ==
//===================
static void adjust_ch(char *line, char ch)
{
  char *src = &line[strlen(line)];
  char *dst = line;

  //-- remove blanks from right and left side --
  do { src--; } while ( (src != line) && (*src == ch) );
  *(src+1) = '\0';
  src = line;
  while (*src == ch) src++; 

  if (src == line) return;

  //-- copy rest --
  while (*src)
  {
    *dst = *src;
    src++;
    dst++;
  }
  *dst = '\0';
}

//default
static int codec_id=CODEC_ID_MSMPEG4V3;
static char *FCC="DIV3";

/* ------------------------------------------------------------ 
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{
    char *p1 = "MJPEG";
    char *p2 = NULL;
    char *p3 = NULL;
    
    if(param->flag == TC_VIDEO) {
	
	avcodec_init();

	if(vob->ex_v_fcc != NULL && strlen(vob->ex_v_fcc) != 0) {
	    p1 = vob->ex_v_fcc;
	    adjust_ch(p1, ' ');//-- parameter 1 (base profile) --
	}

	//p2,p3 not used yet

	if(vob->ex_a_fcc != NULL && strlen(vob->ex_a_fcc) != 0) {
	    p2 = vob->ex_a_fcc;
	    adjust_ch(p2, ' ');//-- parameter 2 (resizer-mode) --
	}
	
	if(vob->ex_profile_name != NULL && strlen(vob->ex_profile_name) != 0) {
	    p3 = vob->ex_profile_name;
	    adjust_ch(p3, ' ');//-- parameter 3 (user profile-name) --
	}
	
	if(verbose_flag & TC_INFO) printf("[%s] ffmpeg codec: %s\n", MOD_NAME, p1);
	
	//select video codec
	if(p1 != NULL && strcasecmp(p1,"MJPEG")==0) {
	  codec_id=CODEC_ID_MJPEG;
	  register_avcodec(&mjpeg_encoder);
	  FCC="MJPG";
	}
	
	if(p1 != NULL && strcasecmp(p1,"MSMPEG4V3")==0) {
	  codec_id=CODEC_ID_MSMPEG4V3;
	  register_avcodec(&msmpeg4v3_encoder);
	  FCC="DIV3";
	}
	
	//-- get it --
	mpa_codec = avcodec_find_encoder(codec_id);
	if (!mpa_codec) {
	  fprintf(stderr, "[%s] codec not found !\n", MOD_NAME);
	  return(TC_EXPORT_ERROR); 
	}
	
	//-- set parameters 
	//--------------------------------------------------------
	memset(&mpa_ctx, 0, sizeof(mpa_ctx));       // default all


	mpa_ctx.frame_rate         = vob->fps * FRAME_RATE_BASE;
	mpa_ctx.bit_rate           = vob->divxbitrate*1000;
	mpa_ctx.bit_rate_tolerance = 1024 * 8 * 1000;
	mpa_ctx.qmin               = vob->min_quantizer;
	mpa_ctx.qmax               = vob->max_quantizer;
	mpa_ctx.max_qdiff          = 3;
	mpa_ctx.qcompress          = 0.5;
	mpa_ctx.qblur              = 0.5;
	mpa_ctx.max_b_frames       = 0;
	mpa_ctx.b_quant_factor     = 2.0;
	mpa_ctx.rc_strategy        = 2;
	mpa_ctx.b_frame_strategy   = 0;
	mpa_ctx.gop_size           = vob->divxkeyframes;
	mpa_ctx.flags              = CODEC_FLAG_HQ;
	mpa_ctx.me_method          = 5;
	
	if(vob->im_v_codec == CODEC_YUV) {
	    mpa_ctx.pix_fmt = PIX_FMT_YUV420P;
	    
	    mpa_picture.linesize[0]=vob->ex_v_width;     
	    mpa_picture.linesize[1]=vob->ex_v_width/2;     
	    mpa_picture.linesize[2]=vob->ex_v_width/2;     
	    
	}
	
	if(vob->im_v_codec == CODEC_RGB) {
	    mpa_ctx.pix_fmt = PIX_FMT_RGB24;
	    
	    mpa_picture.linesize[0]=vob->ex_v_width*3;     
	    mpa_picture.linesize[1]=0;     
	    mpa_picture.linesize[2]=0;
	}
	
	mpa_ctx.width = vob->ex_v_width;
	mpa_ctx.height = vob->ex_v_height;
	mpa_ctx.frame_rate = vob->fps;
	
	//-- open codec --
	//----------------
	if (avcodec_open(&mpa_ctx, mpa_codec) < 0) {
	    fprintf(stderr, "[%s] could not open codec\n", MOD_NAME);
	    return(TC_EXPORT_ERROR); 
	}
	
	
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

  // open output file
  
  if(vob->avifile_out==NULL) { 
    if(NULL == (vob->avifile_out = AVI_open_output_file(vob->video_out_file))) {
      AVI_print_error("avi open error");
      exit(TC_EXPORT_ERROR);
    }
  }
  
  /* save locally */
  avifile = vob->avifile_out;

  if(param->flag == TC_VIDEO) {
    
      // video
      AVI_set_video(vob->avifile_out, vob->ex_v_width, vob->ex_v_height, vob->fps, FCC);
    return(0);
  }
  
  
  if(param->flag == TC_AUDIO) return(audio_open(vob, vob->avifile_out));
  
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
  
  int out_size;
  
  if(param->flag == TC_VIDEO) { 
    
    mpa_picture.data[0]=param->buffer;
    mpa_picture.data[2]=param->buffer + mpa_ctx.width * mpa_ctx.height;
    mpa_picture.data[1]=param->buffer + (mpa_ctx.width * mpa_ctx.height*5)/4;
  
    out_size = avcodec_encode_video(&mpa_ctx, (unsigned char *) tmp_buffer, 
				    SIZE_RGB_FRAME, &mpa_picture);
    
    if(AVI_write_frame(avifile, tmp_buffer, out_size, 1)<0) {
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

    //-- release encoder --
    if (mpa_codec) avcodec_close(&mpa_ctx);

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

  if(vob->avifile_out!=NULL) {
    AVI_close(vob->avifile_out);
    vob->avifile_out=NULL;
  }
  
  if(param->flag == TC_AUDIO) return(audio_close());
  if(param->flag == TC_VIDEO) return(0);
  
  return(TC_EXPORT_ERROR);  
  
}

