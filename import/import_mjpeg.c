/*
 *  import_mjpeg.c
 *
 *  Copyright (C) Thomas Östreich - January 2002
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
#include <dlfcn.h>

#include "transcode.h"
#include "../ffmpeg/libavcodec/avcodec.h"
#include "avilib.h"

#define MOD_NAME    "import_mjpeg.so"
#define MOD_VERSION "v0.1.0 (2002-03-25)"
#define MOD_CODEC   "(video) MJPEG"
#define MOD_PRE mjpeg
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_YUV;
static int codec, frame_size=0;

static avi_t *avifile=NULL;

static int pass_through=0;

//temporary video buffer
static char *buffer;
#define BUFFER_SIZE SIZE_RGB_FRAME

static AVCodec        *mpa_codec = NULL;
static AVCodecContext  mpa_ctx;
static AVPicture       picture;
static int             x_dim=0, y_dim=0;

static unsigned char *bufalloc(size_t size)
{

#ifdef HAVE_GETPAGESIZE
   int buffer_align=getpagesize();
#else
   int buffer_align=0;
#endif

   char *buf = malloc(size + buffer_align);

   int adjust;

   if (buf == NULL) {
       fprintf(stderr, "(%s) out of memory", __FILE__);
   }
   
   adjust = buffer_align - ((int) buf) % buffer_align;

   if (adjust == buffer_align)
      adjust = 0;

   return (unsigned char *) (buf + adjust);
}

/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  
  char *codec_str=NULL;
  double fps=0;

  if(param->flag == TC_VIDEO) {
    
    if(avifile==NULL) 
      if(NULL == (avifile = AVI_open_input_file(vob->video_in_file,1))){
	AVI_print_error("avi open error");
	return(TC_IMPORT_ERROR); 
      }
    
    //important parameter

    x_dim = AVI_video_width(avifile);
    y_dim = AVI_video_height(avifile);

    fps = AVI_frame_rate(avifile);
    codec_str = AVI_video_compressor(avifile);

    fprintf(stderr, "[%s] codec=%s, fps=%6.3f, width=%d, height=%d\n", 
	    MOD_NAME, codec_str, fps, x_dim, y_dim);

    if(strlen(codec_str)==0 || strcasecmp(codec_str,"MJPG")!=0) {
      printf("[%s] detected no MJPEG codec FOURCC MJPG\n", MOD_NAME);
      return(TC_IMPORT_ERROR); 
    }

    //-- initialization of ffmpeg stuff:          --
    //-- only mjpeg video decoder needed --
    //----------------------------------------------
    avcodec_init();
    register_avcodec(&mjpeg_decoder);
    
    

    //-- get it --
    mpa_codec = avcodec_find_decoder(CODEC_ID_MJPEG);

    if (!mpa_codec) {
      fprintf(stderr, "[%s] mjpeg codec not found - internal error\n", MOD_NAME);
      return(TC_IMPORT_ERROR); 
    }


    // Set these to the expected values so that ffmpeg's mjpeg decoder can
    // properly detect interlaced input.
    mpa_ctx.width = x_dim;
    mpa_ctx.height = y_dim;

    //-- open codec --
    //----------------
    if (avcodec_open(&mpa_ctx, mpa_codec) < 0) {
        fprintf(stderr, "[%s] could not open MJPEG codec\n", MOD_NAME);
        return(TC_IMPORT_ERROR); 
    }
    
    codec=vob->im_v_codec;
    
    switch(codec) {
      
    case CODEC_YUV:
      
      frame_size = (x_dim * y_dim * 3)/2;
      break;
      
    case CODEC_RAW:
      pass_through=1;
      break;

      //    default: 
      //fprintf(stderr, "invalid import codec request 0x%x\n", vob->im_v_codec);
      //return(TC_IMPORT_ERROR);
    }
    
    
    //----------------------------------------
    //
    // setup decoder
    //
    //----------------------------------------
    
  
    if ((buffer = bufalloc(BUFFER_SIZE))==NULL) {
      perror("out of memory");
      return(TC_IMPORT_ERROR); 
    } else
      memset(buffer, 0, BUFFER_SIZE);  
    
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

MOD_decode {

    int key,len;
    
    long bytes_read=0;
    
    int ptr;

    char *Ybuf, *Ubuf, *Vbuf;
    int  UVls, row, col, src, dst;

    if(param->flag == TC_VIDEO) {
	
      bytes_read = AVI_read_frame(avifile, buffer, &key);
      
      if(bytes_read < 0) return(TC_IMPORT_ERROR); 

      param->attributes |= TC_FRAME_IS_KEYFRAME;
      
      // PASS_THROUGH MODE

      if(pass_through) {
	
	param->size = (int) bytes_read;
	memcpy(param->buffer, buffer, bytes_read); 

	return(0);
      }
      
      // ------------      
      // decode frame
      // ------------
      
      len=avcodec_decode_video(&mpa_ctx, &picture, 
			       &ptr, buffer, bytes_read);
      
      if(len<0) {
	printf("[%s] frame decoding failed", MOD_NAME);
	return(TC_IMPORT_ERROR);
      }
      
      Ybuf = param->buffer;
      Ubuf = Ybuf + y_dim * x_dim;
      Vbuf = Ubuf + (y_dim * x_dim >> 2);
      UVls = picture.linesize[1];

      switch (mpa_ctx.pix_fmt) {
        case PIX_FMT_YUV420P:
          // Result is in YUV 4:2:0 (YV12) format (copy it straight across):
          memcpy(Ybuf, picture.data[0], picture.linesize[0] * mpa_ctx.height);
          memcpy(Ubuf, picture.data[1], picture.linesize[1] * mpa_ctx.height);
          memcpy(Vbuf, picture.data[2], picture.linesize[2] * mpa_ctx.height);
          break;
        case PIX_FMT_YUV422P:
          // Result is in YUV 4:2:2 format (subsample UV vertically for YV12):
          memcpy(Ybuf, picture.data[0], picture.linesize[0] * mpa_ctx.height);
          src = 0;
          dst = 0;
          for (row=0; row<mpa_ctx.height; row+=2) {
            memcpy(Ubuf + dst, picture.data[1] + src, UVls);
            memcpy(Vbuf + dst, picture.data[2] + src, UVls);
            dst += UVls;
            src = dst << 1;
          }
          break;
        case PIX_FMT_YUV444P:
          // Result is in YUV 4:4:4 format (subsample UV h/v for YV12):
          memcpy(Ybuf, picture.data[0], picture.linesize[0] * mpa_ctx.height);
          src = 0;
          dst = 0;
          for (row=0; row<mpa_ctx.height; row+=2) {
            for (col=0; col<mpa_ctx.width; col+=2) {
              Ubuf[dst] = picture.data[1][src];
              Vbuf[dst] = picture.data[2][src];
              dst++;
              src += 2;
            }
            src += UVls;
          }
          break;
        default:
	  printf("[%s] unsupported decoded frame format", MOD_NAME);
	  return(TC_IMPORT_ERROR);
      }

      //set size
      param->size = frame_size;

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
  
  if(param->flag == TC_VIDEO) {
    
    avcodec_close(&mpa_ctx);
    
    return(0);
  }
  
  
  return(TC_IMPORT_ERROR);
}


