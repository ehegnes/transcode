/*
 *  import_lzo.c
 *
 *  Copyright (C) Thomas Östreich - October 2002
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
#include <unistd.h>
#include <string.h>
#include "avilib.h"
#include "transcode.h"

#include <lzo1x.h>
#if (LZO_VERSION > 0x1070)
#  include <lzoutil.h>
#endif

#define MOD_NAME    "import_lzo.so"
#define MOD_VERSION "v0.0.3 (2002-11-26)"
#define MOD_CODEC   "(video) LZO"

#define MOD_PRE lzo
#include "import_def.h"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM|TC_CAP_YUV|TC_CAP_RGB|TC_CAP_AUD|TC_CAP_VID;

static avi_t *avifile1=NULL;
static avi_t *avifile2=NULL;

static int audio_codec;
static int aframe_count=0, vframe_count=0;

#define BUFFER_SIZE SIZE_RGB_FRAME<<1

static int r;
static lzo_byte *out;
static lzo_byte *wrkmem;
static lzo_uint out_len;


/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  int width=0, height=0;
  double fps=0;
  char *codec=NULL;
  
  param->fd = NULL;
  
  if(param->flag == TC_AUDIO) return(TC_IMPORT_ERROR);
  
  if(param->flag == TC_VIDEO) {
    
    param->fd = NULL;
    
    if(avifile2==NULL) 
      if(NULL == (avifile2 = AVI_open_input_file(vob->video_in_file,1))){
	AVI_print_error("avi open error");
	return(TC_IMPORT_ERROR); 
      }
    
    //read all video parameter from input file
    width  =  AVI_video_width(avifile2);
    height =  AVI_video_height(avifile2);
    
    fps    =  AVI_frame_rate(avifile2);
    codec  =  AVI_video_compressor(avifile2);
    
    
    fprintf(stderr, "[%s] codec=%s, fps=%6.3f, width=%d, height=%d\n", 
	    MOD_NAME, codec, fps, width, height);


    /*
     * Step 1: initialize the LZO library
     */
    
    if (lzo_init() != LZO_E_OK) {
      printf("[%s] lzo_init() failed\n", MOD_NAME);
      return(TC_IMPORT_ERROR); 
    }

    wrkmem = (lzo_bytep) lzo_malloc(LZO1X_1_MEM_COMPRESS);
    out = (lzo_bytep) lzo_malloc(BUFFER_SIZE);
    
    if (wrkmem == NULL || out == NULL) {
      printf("[%s] out of memory\n", MOD_NAME);
      return(TC_IMPORT_ERROR); 
    }
    
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
  
  int key;

  long bytes_read=0;
  
  if(param->flag == TC_VIDEO) {
    // If we are using tccat, then do nothing here
    if (param->fd != NULL) {
      return(0);
    }

    out_len = AVI_read_frame(avifile2, out, &key);
    
    if(verbose & TC_STATS && key) printf("keyframe %d\n", vframe_count); 
    
    if(out_len<=0) {
      if(verbose & TC_DEBUG) AVI_print_error("AVI read video frame");
      return(TC_IMPORT_ERROR);
    }
    
    r = lzo1x_decompress(out, out_len, param->buffer, &param->size, wrkmem);
    
    if (r == LZO_E_OK) {
      if(verbose & TC_DEBUG) printf("decompressed %lu bytes into %lu bytes\n",
				    (long) out_len, (long) param->size);
    } else {
      
      /* this should NEVER happen */
      printf("[%s] internal error - decompression failed: %d\n", MOD_NAME, r);
      return(TC_IMPORT_ERROR); 
    }
   
    //transcode v.0.5.0-pre8 addition
    if(key) param->attributes |= TC_FRAME_IS_KEYFRAME;

    ++vframe_count;
    
    return(0);
  }
  
  if(param->flag == TC_AUDIO) {

    switch(audio_codec) {
      
    case CODEC_RAW:
    
      bytes_read = AVI_audio_size(avifile1, aframe_count);

      if(bytes_read<=0) {
	if(verbose & TC_DEBUG) AVI_print_error("AVI audio read frame");
	return(TC_IMPORT_ERROR);
      }
      
      if(AVI_read_audio(avifile1, param->buffer, bytes_read) < 0) {
	AVI_print_error("AVI audio read frame");
	return(TC_IMPORT_ERROR);
      }

      param->size = bytes_read;
      ++aframe_count;
      
      break;
      
    default:
      
      bytes_read = AVI_read_audio(avifile1, param->buffer, param->size);
      
      if(bytes_read<0) {
	if(verbose & TC_DEBUG) AVI_print_error("AVI audio read frame");
	return(TC_IMPORT_ERROR);
      }
      
      if(bytes_read < param->size) param->size=bytes_read;
    }
    
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
  
  if(param->flag == TC_AUDIO) return(TC_IMPORT_ERROR);
  
  if(param->flag == TC_VIDEO) {
    
    lzo_free(wrkmem);
    lzo_free(out);
    
    if(avifile2!=NULL) {
      AVI_close(avifile2);
      avifile2=NULL;
    }
    return(0);
  }
  
  return(TC_IMPORT_ERROR);
}


