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

#include "transcode.h"
#include "../ffmpeg/libavcodec/avcodec.h"
#include "yuv2rgb.h"
#include "avilib.h"
#include "magic.h"

#define MOD_NAME    "import_ffmpeg.so"
#define MOD_VERSION "v0.1.10 (2003-12-30)"
#define MOD_CODEC   "(video)  " LIBAVCODEC_IDENT \
                    ": MS MPEG4v1-3/MPEG4/MJPEG"
#define MOD_PRE ffmpeg
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

// libavcodec is not thread-safe. We must protect concurrent access to it.
// this is visible (without the mutex of course) with 
// transcode .. -x ffmpeg -y ffmpeg -F mpeg4

extern pthread_mutex_t init_avcodec_lock;

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_YUV|TC_CAP_RGB|TC_CAP_VID;
static int done_seek=0;

struct ffmpeg_codec {
  int   id;
  unsigned int tc_id;
  char *name;
  char  fourCCs[10][5];
};

// fourCC to ID mapping taken from MPlayer's codecs.conf
static struct ffmpeg_codec ffmpeg_codecs[] = {
  {CODEC_ID_MSMPEG4V1, TC_CODEC_ERROR, "mp41",
    {"MP41", "DIV1", ""}},
  {CODEC_ID_MSMPEG4V2, TC_CODEC_MP42, "mp42",
    {"MP42", "DIV2", ""}},
  {CODEC_ID_MSMPEG4V3, TC_CODEC_DIVX3, "msmpeg4",
    {"DIV3", "DIV5", "AP41", "MPG3", "MP43", ""}},
  {CODEC_ID_MPEG4, TC_CODEC_DIVX4, "mpeg4",
    {"DIVX", "XVID", "MP4S", "M4S2", "MP4V", "UMP4", "DX50", ""}},
  {CODEC_ID_MJPEG, TC_CODEC_MJPG, "mjpeg",
    {"MJPG", "AVRN", "AVDJ", "JPEG", "MJPA", "JFIF", ""}},
  {CODEC_ID_MPEG1VIDEO, TC_CODEC_MPG1, "mpeg1video",
    {"MPG1", ""}},
  {CODEC_ID_DVVIDEO, TC_CODEC_DV, "dvvideo",
    {"DVSD", ""}},
  {CODEC_ID_WMV1, TC_CODEC_WMV1, "wmv1",
    {"WMV1", ""}},
  {CODEC_ID_WMV2, TC_CODEC_WMV2, "wmv2",
    {"WMV2", ""}},
  {CODEC_ID_HUFFYUV, TC_CODEC_HFYU, "hfyu",
    {"HFYU", ""}},
  {CODEC_ID_H263I, TC_CODEC_H263I, "h263i",
    {"I263", ""}},
  {CODEC_ID_H263P, TC_CODEC_H263P, "h263p",
    {"H263", "U263", "VIV1", ""}},
  {CODEC_ID_RV10, TC_CODEC_RV10, "rv10",
    {"RV10", "RV13", ""}},
  {CODEC_ID_SVQ1, TC_CODEC_SVQ1, "svq1",
    {"SVQ1", ""}},
  {CODEC_ID_SVQ3, TC_CODEC_SVQ3, "svq3",
    {"SVQ3", ""}},
  {CODEC_ID_MPEG2VIDEO, TC_CODEC_MPEG2, "mpeg2video",
    {"MPG2", ""}},
  {CODEC_ID_MPEG2VIDEO, TC_CODEC_MPEG, "mpeg2video",
    {"MPG2", ""}},
  {CODEC_ID_ASV1, TC_CODEC_ASV1, "asv1",
    {"ASV1", ""}},
  {CODEC_ID_ASV2, TC_CODEC_ASV2, "asv2",
    {"ASV2", ""}},
  {CODEC_ID_FFV1, TC_CODEC_FFV1, "ffv1",
    {"FFV1", ""}},
  {0, TC_CODEC_UNKNOWN, NULL, {""}}};

#define BUFFER_SIZE SIZE_RGB_FRAME

static avi_t              *avifile = NULL;
static int                 pass_through = 0;
static char               *buffer =  NULL;
static char               *yuv2rgb_buffer = NULL;
static AVCodec            *lavc_dec_codec = NULL;
static AVCodecContext     *lavc_dec_context;
static int                 x_dim = 0, y_dim = 0;
static int                 pix_fmt, frame_size = 0, bpp;
static char                *frame = NULL;
static unsigned long       format_flag;
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

static struct ffmpeg_codec *find_ffmpeg_codec_id(unsigned int transcode_id) {
  struct ffmpeg_codec *cdc;
  
  cdc = &ffmpeg_codecs[0];
  while (cdc->name != NULL) {
      if (cdc->tc_id == transcode_id)
	  return cdc;
    cdc++;
  }
  
  return NULL;
}

int scan(char *name) 
{
  struct stat fbuf;
  
  if(stat(name, &fbuf)) {
    fprintf(stderr, "[%s] invalid file \"%s\"\n", MOD_NAME, name);
    exit(1);
  }
  
  // file or directory?
  
  if(S_ISDIR(fbuf.st_mode)) return(1);
  return(0);
}


inline static int stream_read_char(char *d)
{
    return (*d & 0xff);
}

inline static unsigned int stream_read_dword(char *s)
{
    unsigned int y;
    y=stream_read_char(s);
    y=(y<<8)|stream_read_char(s+1);
    y=(y<<8)|stream_read_char(s+2);
    y=(y<<8)|stream_read_char(s+3);
    return y;
}

// Determine of the compressed frame is a keyframe for direct copy
int mpeg4_is_key(unsigned char *data, long size)
{
        int result = 0;
        int i;

        for(i = 0; i < size - 5; i++)
        {
                if( data[i]     == 0x00 && 
                        data[i + 1] == 0x00 &&
                        data[i + 2] == 0x01 &&
                        data[i + 3] == 0xb6)
                {
                        if((data[i + 4] & 0xc0) == 0x0) 
                                return 1;
                        else
                                return 0;
                }
        }
        
        return result;
}

int divx3_is_key(char *d)
{
    int32_t c=0;
    
    c=stream_read_dword(d);
    if(c&0x40000000) return(0);
    
    return(1);
}


static unsigned char *bufalloc(size_t size) {
#ifdef HAVE_GETPAGESIZE
  long buffer_align = getpagesize();
#else
  long buffer_align = 0;
#endif
  char *buf = malloc(size + buffer_align);
  long adjust;

  if (buf == NULL)
    fprintf(stderr, "(%s) out of memory", __FILE__);

  adjust = buffer_align - ((long) buf) % buffer_align;

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
  int extra_data_size = 0;

  if (param->flag == TC_VIDEO) {
    
    format_flag = vob->format_flag;

    if (scan(vob->video_in_file)) goto do_dv;

    if (format_flag == TC_MAGIC_AVI) {
      goto do_avi;
    } else if ( format_flag==TC_MAGIC_DV_PAL || format_flag==TC_MAGIC_DV_NTSC) {
      fprintf(stderr, "Format 0x%lX DV!!\n", format_flag);
      goto do_dv;
    } else {
      fprintf(stderr, "[%s] Format 0x%lX not supported\n", MOD_NAME, format_flag);
      return(TC_IMPORT_ERROR); 
    }
    fprintf(stderr, "[%s] Format 0x%lX\n", MOD_NAME, format_flag);

do_avi:
    if(avifile==NULL) {
      if(vob->nav_seek_file) {
	if(NULL == (avifile = AVI_open_input_indexfile(vob->video_in_file,0,vob->nav_seek_file))){
	  AVI_print_error("avi open error");
	  return(TC_IMPORT_ERROR); 
	}
      } else {
	if(NULL == (avifile = AVI_open_input_file(vob->video_in_file,1))){
	  AVI_print_error("avi open error");
	  return(TC_IMPORT_ERROR); 
	} 
      }
    }
    
    // vob->offset contains the last keyframe
    if (!done_seek && vob->vob_offset>0) {
	AVI_set_video_position(avifile, vob->vob_offset);
	done_seek=1;
    }

    //important parameter
    x_dim = vob->im_v_width;
    y_dim = vob->im_v_height;
    fps   = vob->fps;

    fourCC = AVI_video_compressor(avifile);

    if (strlen(fourCC) == 0) {
      fprintf(stderr, "[%s] FOURCC has zero length!? Broken source?\n",
              MOD_NAME);
      
      return TC_IMPORT_ERROR;
    }

    //-- initialization of ffmpeg stuff:          --
    //----------------------------------------------
    pthread_mutex_lock(&init_avcodec_lock);
    avcodec_init();
    avcodec_register_all();
    pthread_mutex_unlock(&init_avcodec_lock);

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

    if (vob->decolor) lavc_dec_context->flags |= CODEC_FLAG_GRAY;
    lavc_dec_context->error_resilience = 2;
    lavc_dec_context->error_concealment = 3;
    lavc_dec_context->workaround_bugs = FF_BUG_AUTODETECT;
    lavc_dec_context->stream_codec_tag= (fourCC[0]<<24)|(fourCC[1]<<16)|(fourCC[2]<<8)|fourCC[0];

    // XXX: some codecs need extra data
    switch (codec->id)
    {
      case CODEC_ID_MJPEG: extra_data_size  = 28; break;
      case CODEC_ID_LJPEG: extra_data_size  = 28; break;
      case CODEC_ID_HUFFYUV: extra_data_size = 1000; break;
      case CODEC_ID_ASV1: extra_data_size = 8; break;
      case CODEC_ID_ASV2: extra_data_size = 8; break;
      case CODEC_ID_WMV1: extra_data_size = 4; break;
      case CODEC_ID_WMV2: extra_data_size = 4; break;
      default: extra_data_size = 0; break;
    }

    if (extra_data_size) {
      lavc_dec_context->extradata = malloc(extra_data_size);
      memset (lavc_dec_context->extradata, 0, extra_data_size);
      lavc_dec_context->extradata_size = extra_data_size;
    }
    

    if (avcodec_open(lavc_dec_context, lavc_dec_codec) < 0) {
      fprintf(stderr, "[%s] Could not initialize the '%s' codec.\n", MOD_NAME,
              codec->name);
      return TC_IMPORT_ERROR;
    }
    
    pix_fmt = vob->im_v_codec;
    
    frame_size = x_dim * y_dim * 3;
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
      case CODEC_RAW_YUV:
      case CODEC_RAW_RGB:
        pass_through = 1;
        break;
    }
    
    if (!frame) {
        frame = calloc(frame_size, 1);
        if (!frame) {
            perror("out of memory");
            return TC_IMPORT_ERROR;
        }
    }
    
    //----------------------------------------
    //
    // setup decoder
    //
    //----------------------------------------
    
    if(buffer == NULL) buffer=bufalloc(frame_size);
    
    if(buffer == NULL) {
      perror("out of memory");
      return TC_IMPORT_ERROR;
    }

    memset(buffer, 0, frame_size);  
    
    param->fd = NULL;

    return 0;
do_dv: 
    x_dim = vob->im_v_width;
    y_dim = vob->im_v_height;

    {
      char yuv_buf[255];
      //char ext_buf[255];
      struct ffmpeg_codec *codec;
      
      switch (vob->im_v_codec) {
	case CODEC_RGB:
	  sprintf(yuv_buf, "rgb");
	  break;
	case CODEC_YUV:
	  sprintf(yuv_buf, "yv12");
	  break;
      }

      codec = find_ffmpeg_codec_id (vob->codec_flag);
      if (codec == NULL) {
	fprintf(stderr, "[%s] No codec is known the TAG '%lx'.\n", MOD_NAME,
	    vob->codec_flag);
	return TC_IMPORT_ERROR;
      }

      //printf ("FFMPEG: codec->name = %s ->id = 0x%x\n", codec->name, codec->id);

      snprintf (import_cmd_buf, MAX_BUF, 
	  "tccat -i \"%s\" -d %d"
	  " | tcextract -x dv -d %d"
	  " | tcdecode -x %s -t lavc -y %s -g %dx%d -Q %d -d %d"
	  ,
	  vob->video_in_file, vob->verbose,
	  vob->verbose,
	  codec->name, yuv_buf, x_dim, y_dim, vob->quality, vob->verbose);
    }
      
    // print out
    if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

    // set to NULL if we handle read
    param->fd = NULL;

    // popen
    if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
      perror("popen LAVC stream");
      return(TC_IMPORT_ERROR);
    }

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

  /*
   * When using directory mode or dvraw etc, we don't enter here 
   * (transcode-core does the reading) so there is no need to protect this
   * stuff by and if() or something.
   */

  int        key,len, i, edge_width, j;
  long       bytes_read = 0;
  int        got_picture, UVls, src, dst, row, col;
  char      *Ybuf, *Ubuf, *Vbuf;
  int        retry;
  AVFrame  picture;

  if (param->flag == TC_VIDEO) {
    bytes_read = AVI_read_frame(avifile, buffer, &key);

    if (bytes_read < 0) {
      return TC_IMPORT_ERROR;
    }
    
    if (key) {
      param->attributes |= TC_FRAME_IS_KEYFRAME;
    }

    // PASS_THROUGH MODE

    if (pass_through) {
      int bkey = 0;

      // check for keyframes
      if (codec->id == CODEC_ID_MSMPEG4V3) {
	if (divx3_is_key(buffer)) bkey = 1;
      }
      else if (codec->id == CODEC_ID_MPEG4) {
	if (mpeg4_is_key(buffer, bytes_read)) bkey = 1;
      }
      else if (codec->id == CODEC_ID_MJPEG) {
	bkey = 1;
      }

      if (bkey) {
	param->attributes |= TC_FRAME_IS_KEYFRAME;
      }

      if (verbose & TC_DEBUG) 
	if (key || bkey) printf("[%s] Keyframe info (AVI | Bitstream) (%d|%d)\n", MOD_NAME, key, bkey);

      param->size = (int) bytes_read;
      memcpy(param->buffer, buffer, bytes_read); 

      return TC_IMPORT_OK;
    }

    if (bytes_read == 0) {
        // repeat last frame
        memcpy(param->buffer, frame, frame_size);
        param->size = frame_size;
        return 0;
    }

    // ------------      
    // decode frame
    // ------------

    // safety counter
    retry = 0;
    do {
      pthread_mutex_lock(&init_avcodec_lock);
      len = avcodec_decode_video(lavc_dec_context, &picture, 
			         &got_picture, buffer, bytes_read);
      pthread_mutex_unlock(&init_avcodec_lock);

      if (len < 0) {
	tc_warn ("[%s] frame decoding failed", MOD_NAME);
        return TC_IMPORT_ERROR;
      }
    } while (!got_picture && ++retry<1000);

    if (retry >= 1000) {
      tc_warn("[%s] tried a 1000 times but could not decode frame", MOD_NAME);
      return TC_IMPORT_ERROR;
    }

    Ybuf = frame;
    Ubuf = Ybuf + lavc_dec_context->width * lavc_dec_context->height;
    Vbuf = Ubuf + lavc_dec_context->width * lavc_dec_context->height / 4;
    UVls = picture.linesize[1];
    
    switch (lavc_dec_context->pix_fmt) {
      case PIX_FMT_YUV420P:
        // Result is in YUV 4:2:0 (YV12) format, but each line ends with
        // an edge which we must skip
        if (pix_fmt == CODEC_YUV) {
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
          yuv2rgb(frame, yuv2rgb_buffer,
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
      case PIX_FMT_YUV422P:
          // Result is in YUV 4:2:2 format (subsample UV vertically for YV12):
          memcpy(Ybuf, picture.data[0], picture.linesize[0] * lavc_dec_context->height);
          src = 0;
          dst = 0;
          for (row=0; row<lavc_dec_context->height; row+=2) {
            memcpy(Ubuf + dst, picture.data[1] + src, UVls);
            memcpy(Vbuf + dst, picture.data[2] + src, UVls);
            dst += UVls;
            src = dst << 1;
          }
	  break;
      case PIX_FMT_YUV444P:
          // Result is in YUV 4:4:4 format (subsample UV h/v for YV12):
	  memcpy(Ybuf, picture.data[0], picture.linesize[0] * lavc_dec_context->height);
          src = 0;
          dst = 0;
          for (row=0; row<lavc_dec_context->height; row+=2) {
            for (col=0; col<lavc_dec_context->width; col+=2) {
              Ubuf[dst] = picture.data[1][src];
              Vbuf[dst] = picture.data[2][src];
              dst++;
              src += 2;
            }
            src += UVls;
          }
          break;
      case PIX_FMT_YUV411P:
        if (pix_fmt == CODEC_YUV) {
	  // Planar YUV 4:1:1 (1 Cr & Cb sample per 4x1 Y samples)
	  // 4:1:1 -> 4:2:0

          for (i = 0; i < lavc_dec_context->height; i++) {
            memcpy(Ybuf + i * lavc_dec_context->width,
                   picture.data[0] + i * picture.linesize[0], 
                   lavc_dec_context->width);
          }
          for (i = 0; i < lavc_dec_context->height; i++) {
	      for (j=0; j < lavc_dec_context->width / 2; j++) {
		  Vbuf[i/2 * lavc_dec_context->width/2 + j] = 
		    *(picture.data[1] + i * picture.linesize[1] + j/2);
		  Ubuf[i/2 * lavc_dec_context->width/2 + j] = 
		    *(picture.data[2] + i * picture.linesize[2] + j/2);
	      }
          }
        } else { // RGB
          Ybuf = yuv2rgb_buffer;
          Ubuf = Ybuf + lavc_dec_context->width * lavc_dec_context->height;
          Vbuf = Ubuf + lavc_dec_context->width * lavc_dec_context->height / 4;
          for (i = 0; i < lavc_dec_context->height; i++) {
            memcpy(Ybuf + i * lavc_dec_context->width,
                   picture.data[0] + i * picture.linesize[0], 
                   lavc_dec_context->width);
          }
          for (i = 0; i < lavc_dec_context->height; i++) {
	      for (j=0; j < lavc_dec_context->width / 2; j++) {
		  Vbuf[i/2 * lavc_dec_context->width/2 + j] = *(picture.data[1] + i * picture.linesize[1] + j/2);
		  Ubuf[i/2 * lavc_dec_context->width/2 + j] = *(picture.data[2] + i * picture.linesize[2] + j/2);
	      }
          }
          yuv2rgb(frame, yuv2rgb_buffer,
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
	tc_warn("[%s] Unsupported decoded frame format: %d", MOD_NAME, lavc_dec_context->pix_fmt);
	return TC_IMPORT_ERROR;
    }

    memcpy(param->buffer, frame, frame_size);
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
      avcodec_flush_buffers(lavc_dec_context);
      
      avcodec_close(lavc_dec_context);
      free(lavc_dec_context);

      lavc_dec_context = NULL;
      done_seek=0;

    }
    
    if (param->fd) pclose(param->fd);
    param->fd = NULL;

    // do not free buffer and yuv2rgb_buffer!!
    
    if(avifile!=NULL) {
      AVI_close(avifile);
      avifile=NULL;
    }
    
    return(0);
  } 
  
  return TC_IMPORT_ERROR;
}

// vim: sw=2
