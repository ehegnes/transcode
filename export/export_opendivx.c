/*
 *  export_opendivx.c
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
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "../encore2/encore2.h"
#include "transcode.h"
#include "avilib.h"
#include "aud_aux.h"

#define MOD_NAME    "export_opendivx.so"
#define MOD_VERSION "v0.2.8 (2002-01-15)"
#define MOD_CODEC   "(video) OpenDivX | (audio) MPEG/AC3/PCM"

#define MOD_PRE opendivx
#include "export_def.h"

static avi_t *avifile=NULL;
  
//temporary audio/video buffer
static char *buffer;
#define BUFFER_SIZE SIZE_RGB_FRAME<<1

ENC_PARAM   *divx;
ENC_FRAME  encode;
ENC_RESULT    key;

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_AC3|TC_CAP_AUD|TC_CAP_YUV;

/* ------------------------------------------------------------ 
 *
 * open outputfile
 *
 * ------------------------------------------------------------*/

MOD_open
{
    
  // open file
  if(vob->avifile_out==NULL) 
    if(NULL == (vob->avifile_out = AVI_open_output_file(vob->video_out_file))) {
      AVI_print_error("avi open error");
      return(TC_EXPORT_ERROR); 
    }
     
  /* save locally */
  avifile = vob->avifile_out;

  if(param->flag == TC_VIDEO) {
    
    // video
    AVI_set_video(vob->avifile_out, vob->ex_v_width, vob->ex_v_height, 
		  vob->fps, "DIVX");
    return(0);
  }

  if(param->flag == TC_AUDIO) return(audio_open(vob, vob->avifile_out)); 
  
  // invalid flag
  return(TC_EXPORT_ERROR); 
}


/* ------------------------------------------------------------ 
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{

  int ch;
  
  if(param->flag == TC_VIDEO) {

    //check for odd frame parameter:
    
    if((ch = vob->ex_v_width - ((vob->ex_v_width>>3)<<3)) != 0) {
      printf("[%s] frame width %d (no multiple of 8)\n", MOD_NAME, vob->ex_v_width);
      printf("[%s] encoder may not work correctly or crash\n", MOD_NAME);
      
      if(ch & 1) {
	printf("[%s] invalid frame width\n", MOD_NAME); 
	return(TC_EXPORT_ERROR); 
      }
    }
    
    if((ch = vob->ex_v_height - ((vob->ex_v_height>>3)<<3)) != 0) {
      printf("[%s] invalid frame height %d (no multiple of 8)\n", MOD_NAME, vob->ex_v_height);
      printf("[%s] encoder may not work correctly or crash\n", MOD_NAME);
      return(TC_EXPORT_ERROR); 
    }
    
    if ((buffer = malloc(BUFFER_SIZE))==NULL) {
      perror("out of memory");
      return(TC_EXPORT_ERROR); 
    } else
      memset(buffer, 0, BUFFER_SIZE);  
    
    if ((divx = malloc(sizeof(ENC_PARAM)))==NULL) {
      perror("out of memory");
      return(TC_EXPORT_ERROR); 
    }
    
    divx->x_dim = vob->ex_v_width;
    divx->y_dim = vob->ex_v_height;
    divx->framerate = vob->fps;
    divx->bitrate = vob->divxbitrate*1000;

    //recommended
    divx->rc_period = 2000;
    divx->rc_reaction_period = 10;
    divx->rc_reaction_ratio  = 20;

    divx->max_key_interval=vob->divxkeyframes;
    divx->quality=vob->divxquality;  
    divx->handle=NULL;
    
    if(encore(NULL, ENC_OPT_INIT, divx, NULL) < 0) {
      printf("opendivx open error");
      return(TC_EXPORT_ERROR); 
    }
    
    encode.bitstream  = buffer;
    encode.colorspace = (vob->im_v_codec==CODEC_RGB) ? ENC_CSP_RGB24:ENC_CSP_YV12;

    if(verbose_flag & TC_DEBUG) 
    {
	fprintf(stderr, "[%s]                quality: %d\n", MOD_NAME, divx->quality);
	fprintf(stderr, "[%s]      bitrate [kBits/s]: %d\n", MOD_NAME, divx->bitrate/1000);
	fprintf(stderr, "[%s]              crispness: %d\n", MOD_NAME, vob->divxcrispness);
	fprintf(stderr, "[%s]  max keyframe interval: %d\n", MOD_NAME, divx->max_key_interval);
	fprintf(stderr, "[%s]             frame rate: %.2f\n", MOD_NAME, vob->fps);
	fprintf(stderr, "[%s]            color space: %s\n", MOD_NAME, (vob->im_v_codec==CODEC_RGB) ? "RGB24":"YV12");
    }

    return(0);
  }

  if(param->flag == TC_AUDIO) return(audio_init(vob, verbose_flag));  
  
  // invalid flag
  return(TC_EXPORT_ERROR); 
}

/* ------------------------------------------------------------ 
 *
 * encode and export frame
 *
 * ------------------------------------------------------------*/


MOD_encode
{
  
  if(param->flag == TC_VIDEO) { 
    
    // encode video
	
    encode.image = param->buffer;
    
    if(encore(divx->handle, ENC_OPT_ENCODE, &encode, &key) < 0) {
      printf("divx encode error");
      return(TC_EXPORT_ERROR); 
    }
    
    // write video
    
    if(AVI_write_frame(avifile, buffer, encode.length, key.is_key_frame)<0) {
      printf("avi video write error");
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
    if(encore(divx->handle, ENC_OPT_RELEASE, NULL, NULL) < 0) {
      printf("opendivx close error");
    }

    if(buffer!=NULL) {
      free(buffer);
      buffer=NULL;
    }
    
    return(0);
  }
  
  if(param->flag == TC_AUDIO) return(audio_stop());  
  
  return(TC_EXPORT_ERROR);     
}

/* ------------------------------------------------------------ 
 *
 * close codec
 *
 * ------------------------------------------------------------*/

MOD_close
{  

  vob_t *vob = tc_get_vob();
  if(param->flag == TC_AUDIO) return(audio_close()); 
  
  if(vob->avifile_out!=NULL) {
    AVI_close(vob->avifile_out);
    vob->avifile_out=NULL;
  }
  
  if(param->flag == TC_VIDEO) return(0);
  
  return(TC_EXPORT_ERROR); 
}

