/*
 *  import_ffmpeg.c
 *
 *  Copyright (C) Moritz Bunkus - October 2002
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
#include "yuv2rgb.h"
#include "avilib.h"

#define MOD_NAME    "import_ffmpeg.so"
#define MOD_VERSION "v0.1.1 (2002-11-29)"
#define MOD_CODEC   "(video) FFMPEG API (build " LIBAVCODEC_BUILD_STR \
                    "): MS MPEG4v1-3/MPEG4/MJPEG"
#define MOD_PRE ffmpeg
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_YUV|TC_CAP_RGB|TC_CAP_VID;

struct ffmpeg_codec {
  int   id;
  char *name;
  char  fourCCs[10][5];
};

// fourCC to ID mapping taken from MPlayer's codecs.conf
static struct ffmpeg_codec ffmpeg_codecs[] = {
  {CODEC_ID_MSMPEG4V1, "msmpeg4v1",
    {"MP41", "DIV1", ""}},
  {CODEC_ID_MSMPEG4V2, "msmpeg4v2",
    {"MP42", "DIV2", ""}},
  {CODEC_ID_MSMPEG4V3, "msmpeg4",
    {"DIV3", "DIV5", "AP41", "MPG3", "MP43", ""}},
  {CODEC_ID_MPEG4, "mpeg4",
    {"DIVX", "XVID", "MP4S", "M4S2", "MP4V", "UMP4", "DX50", ""}},
  {CODEC_ID_MJPEG, "mjpeg",
    {"MJPG", "AVRN", "AVDJ", "JPEG", "MJPA", "JFIF", ""}},
  {CODEC_ID_MPEG1VIDEO, "mpeg1video",
    {"MPG1", ""}},
  {CODEC_ID_DVVIDEO, "dvvideo",
    {"DVSD", ""}},
  {CODEC_ID_WMV1, "wmv1",
    {"WMV1", ""}},
  {CODEC_ID_WMV2, "wmv2",
    {"WMV2", ""}},
  {CODEC_ID_H263I, "h263",
    {"I263", ""}},
  {CODEC_ID_H263P, "h263p",
    {"H263", "U263", "VIV1", ""}},
  {CODEC_ID_RV10, "rv10",
    {"RV10", "RV13", ""}},
  {0, NULL, {""}}};

#define BUFFER_SIZE SIZE_RGB_FRAME

static avi_t              *avifile = NULL;
static int                 pass_through = 0;
static char               *buffer =  NULL;
static char               *yuv2rgb_buffer = NULL;
static AVCodec            *lavc_dec_codec = NULL;
static AVCodecContext     *lavc_dec_context;
static int                 x_dim = 0, y_dim = 0;
static int                 pix_fmt, frame_size = 0, bpp;
static struct ffmpeg_codec *codec;

static struct ffmpeg_codec *find_ffmpeg_codec(char *fourCC) {
  int i;
  struct ffmpeg_codec *cdc;
  
  cdc = &ffmpeg_codecs[0];
  while (cdc->name != NULL) {
    i = 0;
    while (cdc->fourCCs[i][0] != 0) {
      if (!strcasecmp(cdc->fourCCs[i], fourCC))
        return cdc;
      i++;
    }
    cdc++;
  }
  
  return NULL;
}

static unsigned char *bufalloc(size_t size) {
#ifdef HAVE_GETPAGESIZE
  int buffer_align = getpagesize();
#else
  int buffer_align = 0;
#endif
  char *buf = malloc(size + buffer_align);
  int adjust;

  if (buf == NULL)
    fprintf(stderr, "(%s) out of memory", __FILE__);

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

MOD_open {
  char   *fourCC = NULL;
  double  fps = 0;

  if (param->flag == TC_VIDEO) {
    
    if (avifile == NULL) 
      if ((avifile = AVI_open_input_file(vob->video_in_file, 1)) == NULL) {
	AVI_print_error("avi open error");

	return TC_IMPORT_ERROR;
      }
    
    //important parameter

    x_dim = AVI_video_width(avifile);
    y_dim = AVI_video_height(avifile);

    fps = AVI_frame_rate(avifile);
    fourCC = AVI_video_compressor(avifile);

    if (strlen(fourCC) == 0) {
      fprintf(stderr, "[%s] FOURCC has zero length!? Broken source?\n",
              MOD_NAME);
      
      return TC_IMPORT_ERROR;
    }

    //-- initialization of ffmpeg stuff:          --
    //----------------------------------------------
    avcodec_init();
    avcodec_register_all();

    codec = find_ffmpeg_codec(fourCC);
    if (codec == NULL) {
      fprintf(stderr, "[%s] No codec is known the FOURCC '%s'.\n", MOD_NAME,
              fourCC);
      return TC_IMPORT_ERROR;
    }

    lavc_dec_codec = avcodec_find_decoder(codec->id);
    if (!lavc_dec_codec) {
      fprintf(stderr, "[%s] No codec found for the FOURCC '%s'.\n", MOD_NAME,
              fourCC);
      return TC_IMPORT_ERROR;
    }

    // Set these to the expected values so that ffmpeg's decoder can
    // properly detect interlaced input.
    lavc_dec_context = avcodec_alloc_context();
    if (lavc_dec_context == NULL) {
      fprintf(stderr, "[%s] Could not allocate enough memory.\n", MOD_NAME);
      return TC_IMPORT_ERROR;
    }
    lavc_dec_context->width  = x_dim;
    lavc_dec_context->height = y_dim;

    lavc_dec_context->flags |= CODEC_FLAG_NOT_TRUNCATED;

    if (vob->decolor) lavc_dec_context->flags |= CODEC_FLAG_GRAY;
    lavc_dec_context->error_resilience = 2;
    lavc_dec_context->error_concealment = 3;
    lavc_dec_context->workaround_bugs = FF_BUG_AUTODETECT;

    if (avcodec_open(lavc_dec_context, lavc_dec_codec) < 0) {
      fprintf(stderr, "[%s] Could not initialize the '%s' codec.\n", MOD_NAME,
              codec->name);
      return TC_IMPORT_ERROR;
    }
    
    pix_fmt = vob->im_v_codec;
    
    switch (pix_fmt) {
      case CODEC_YUV:
        frame_size = (x_dim * y_dim * 3)/2;
        break;
      case CODEC_RGB:
        frame_size = x_dim * y_dim * 3;
        yuv2rgb_init(vob->v_bpp, MODE_RGB);
        bpp = vob->v_bpp;

        if (yuv2rgb_buffer == NULL) yuv2rgb_buffer = bufalloc(BUFFER_SIZE);
	
        if (yuv2rgb_buffer == NULL) {
          perror("out of memory");
          return TC_IMPORT_ERROR;
        } else
          memset(yuv2rgb_buffer, 0, BUFFER_SIZE);  
        break;
      case CODEC_RAW:
        pass_through = 1;
        break;
    }
    
    
    //----------------------------------------
    //
    // setup decoder
    //
    //----------------------------------------
    
    if(buffer == NULL) buffer=bufalloc(BUFFER_SIZE);
    
    if(buffer == NULL) {
      perror("out of memory");
      return TC_IMPORT_ERROR;
    } else
      memset(buffer, 0, BUFFER_SIZE);  
    
    param->fd = NULL;

    return 0;
  }
  
  return TC_IMPORT_ERROR;
}


/* ------------------------------------------------------------ 
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/

MOD_decode {
  int        key,len, i, edge_width;
  long       bytes_read = 0;
  int        got_picture;
  char      *Ybuf, *Ubuf, *Vbuf;
  AVPicture  picture;

  if (param->flag == TC_VIDEO) {
    bytes_read = AVI_read_frame(avifile, buffer, &key);

    if (bytes_read < 0) {
      return TC_IMPORT_ERROR;
    }
    
    if (key)
      param->attributes |= TC_FRAME_IS_KEYFRAME;

    // PASS_THROUGH MODE

    if (pass_through) {
      param->size = (int) bytes_read;
      memcpy(param->buffer, buffer, bytes_read); 

      return 0;
    }

    // ------------      
    // decode frame
    // ------------

    do {
      len = avcodec_decode_video(lavc_dec_context, &picture, 
			         &got_picture, buffer, bytes_read);

      if (len < 0) {
        fprintf(stderr, "[%s] frame decoding failed", MOD_NAME);
        return TC_IMPORT_ERROR;
      }
    } while (!got_picture);

    switch (lavc_dec_context->pix_fmt) {
      case PIX_FMT_YUV420P:
        // Result is in YUV 4:2:0 (YV12) format, but each line ends with
        // an edge which we must skip
        if (pix_fmt == CODEC_YUV) {
          Ybuf = param->buffer;
          Ubuf = Ybuf + lavc_dec_context->width * lavc_dec_context->height;
          Vbuf = Ubuf + lavc_dec_context->width * lavc_dec_context->height / 4;
          edge_width = (picture.linesize[0] - lavc_dec_context->width) / 2;
          for (i = 0; i < lavc_dec_context->height; i++) {
            memcpy(Ybuf + i * lavc_dec_context->width,
                   picture.data[0] + i * picture.linesize[0], //+ edge_width,
                   lavc_dec_context->width);
          }
          for (i = 0; i < lavc_dec_context->height / 2; i++) {
            memcpy(Vbuf + i * lavc_dec_context->width / 2,
                   picture.data[1] + i * picture.linesize[1],// + edge_width / 2,
                   lavc_dec_context->width / 2);
            memcpy(Ubuf + i * lavc_dec_context->width / 2,
                   picture.data[2] + i * picture.linesize[2],// + edge_width / 2,
                   lavc_dec_context->width / 2);
          }
        } else {
          Ybuf = yuv2rgb_buffer;
          Ubuf = Ybuf + lavc_dec_context->width * lavc_dec_context->height;
          Vbuf = Ubuf + lavc_dec_context->width * lavc_dec_context->height / 4;
          edge_width = (picture.linesize[0] - lavc_dec_context->width) / 2;
          for (i = 0; i < lavc_dec_context->height; i++) {
            memcpy(Ybuf + (lavc_dec_context->height - i - 1) *
                     lavc_dec_context->width,
                   picture.data[0] + i * picture.linesize[0], //+ edge_width,
                   lavc_dec_context->width);
          }
          for (i = 0; i < lavc_dec_context->height / 2; i++) {
            memcpy(Vbuf + (lavc_dec_context->height / 2 - i - 1) *
                     lavc_dec_context->width / 2,
                   picture.data[1] + i * picture.linesize[1],// + edge_width / 2,
                   lavc_dec_context->width / 2);
            memcpy(Ubuf + (lavc_dec_context->height / 2 - i - 1) *
                     lavc_dec_context->width / 2,
                   picture.data[2] + i * picture.linesize[2],// + edge_width / 2,
                   lavc_dec_context->width / 2);
          }
          yuv2rgb(param->buffer, yuv2rgb_buffer,
                  yuv2rgb_buffer +
                    lavc_dec_context->width * lavc_dec_context->height, 
                  yuv2rgb_buffer +
                    5 * lavc_dec_context->width * lavc_dec_context->height / 4, 
                  lavc_dec_context->width,
                  lavc_dec_context->height, 
                  lavc_dec_context->width * bpp / 8,
                  lavc_dec_context->width,
                  lavc_dec_context->width / 2);
        }
        break;
      default:
	fprintf(stderr, "[%s] Unsupported decoded frame format", MOD_NAME);
	return TC_IMPORT_ERROR;
    }

    //set size
    param->size = frame_size;

    return 0;
  }

  return TC_IMPORT_ERROR;
}

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close {  

  if (param->flag == TC_VIDEO) {

    if(lavc_dec_context) {
      
      avcodec_close(lavc_dec_context);
      free(lavc_dec_context);

      lavc_dec_context = NULL;

    }
    
    // do not free buffer and yuv2rgb_buffer!!
    
    if(avifile!=NULL) {
      AVI_close(avifile);
      avifile=NULL;
    }
    
    return(0);
  } 
  
  return TC_IMPORT_ERROR;
}
