/*
 *  decode_lavc.c
 *
 *  Copyright (C) Tilmann Bitterberg - March 2003
 *  Copyright (C) Thomas Östreich - June 2001
 *  Copyright (C) 1999-2001 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 *  This file is part of transcode, a video stream  processing tool
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

#include "transcode.h"
#include "tcinfo.h"

#include "aclib/imgconvert.h"
#include "ioaux.h"
#include "tc.h"

// FIXME
#ifdef EMULATE_FAST_INT
#undef EMULATE_FAST_INT
#endif
#include <ffmpeg/avcodec.h>

#define READ_BUFFER_SIZE (10*1024*1024)
#define MOD_NAME "decode_ffmpeg"

#define MAX_BUF 1024

static int verbose_flag=TC_QUIET;

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
  {CODEC_ID_HUFFYUV, TC_CODEC_HUFFYUV, "hfyu",
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
  {0, TC_CODEC_UNKNOWN, NULL, {""}}};


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


/* ------------------------------------------------------------
 *
 * decoder thread
 *
 * ------------------------------------------------------------*/

void decode_lavc(decode_t *decode)
{
  uint8_t           *out_buffer = NULL;
  int                 pass_through = 0;
  char               *buffer =  NULL;
  char               *yuv2rgb_buffer = NULL;
  AVCodec            *lavc_dec_codec = NULL;
  AVCodecContext     *lavc_dec_context;
  int                 x_dim = 0, y_dim = 0;
  int                 pix_fmt, frame_size = 0, bpp = 8;
  struct ffmpeg_codec *codec;

  char   *fourCC = NULL;
  char *mp4_ptr=NULL;
  int flush = 0;
  int mp4_size=0;
  int buf_len=0;
  int run=0;

  // decoder
  int        len = 0;
  long       bytes_read = 0;
  uint8_t   *planes[3];

  verbose_flag = decode->verbose;

  x_dim = decode->width;
  y_dim = decode->height;

  fourCC="DIVX";

  //----------------------------------------
  //
  // setup decoder
  //
  //----------------------------------------

  avcodec_init();
  avcodec_register_all();

  codec = find_ffmpeg_codec_id(decode->codec);
  if (codec == NULL) {
      fprintf(stderr, "[%s] No codec is known the TAG '%lx'.\n", MOD_NAME,
	      decode->codec);
      goto decoder_error;
  }
  if (decode->verbose & TC_DEBUG) {
      fprintf(stderr, "[%s] Using Codec %s id 0x%x\n", MOD_NAME , codec->name, codec->tc_id);
  }

  lavc_dec_codec = avcodec_find_decoder(codec->id);
  if (!lavc_dec_codec) {
      fprintf(stderr, "[%s] No codec found for the FOURCC '%s'.\n", MOD_NAME,
	      fourCC);
      goto decoder_error;
  }

  // Set these to the expected values so that ffmpeg's decoder can
  // properly detect interlaced input.
  lavc_dec_context = avcodec_alloc_context();
  if (lavc_dec_context == NULL) {
      fprintf(stderr, "[%s] Could not allocate enough memory.\n", MOD_NAME);
      goto decoder_error;
  }
  lavc_dec_context->width  = x_dim;
  lavc_dec_context->height = y_dim;

  lavc_dec_context->error_resilience = 2;
  lavc_dec_context->error_concealment = 3;
  lavc_dec_context->workaround_bugs = FF_BUG_AUTODETECT;

  if (avcodec_open(lavc_dec_context, lavc_dec_codec) < 0) {
      fprintf(stderr, "[%s] Could not initialize the '%s' codec.\n", MOD_NAME,
	      codec->name);
      goto decoder_error;
  }

  pix_fmt = decode->format;

  frame_size = (x_dim * y_dim * 3)/2;
  switch (pix_fmt)
  {
      case TC_CODEC_YUV420P:
        frame_size = (x_dim * y_dim * 3)/2;
        break;

      case TC_CODEC_RGB:
        frame_size = x_dim * y_dim * 3;
        bpp = 24;

        if (yuv2rgb_buffer == NULL)
	    yuv2rgb_buffer = tc_bufalloc(frame_size);

        if (yuv2rgb_buffer == NULL)
	{
            perror("out of memory");
            goto decoder_error;
        }
	else
	    memset(yuv2rgb_buffer, 0, frame_size);
        break;

      case TC_CODEC_RAW:
	pass_through = 1;
	break;
  }

  if(buffer == NULL) buffer=tc_bufalloc(READ_BUFFER_SIZE);
  if(buffer == NULL) {
      perror("out of memory");
      goto decoder_error;
  }

  if(out_buffer == NULL) out_buffer=tc_bufalloc(frame_size);
  if(out_buffer == NULL) {
      perror("out of memory");
      goto decoder_error;
  }

  memset(buffer, 0, READ_BUFFER_SIZE);
  memset(out_buffer, 0, frame_size);

  // DECODE MAIN LOOP

  bytes_read = tc_pread(decode->fd_in, (uint8_t*) buffer, READ_BUFFER_SIZE);

  if (bytes_read < 0) {
      fprintf(stderr, "[%s] EOF?\n", MOD_NAME);
      goto decoder_error;
  }
  mp4_ptr = buffer;
  mp4_size = bytes_read;
  buf_len = 0;

  do {
      AVFrame  picture;
      int got_picture = 0;

      if (buf_len >= mp4_size) {
	  if (verbose_flag & TC_DEBUG) fprintf(stderr, "[%s] EOF?\n", MOD_NAME);
	  break;
      }

      //fprintf(stderr, "SIZE: (%d) MP4(%d) blen(%d) BUF(%d) read(%ld)\n", len, mp4_size, buf_len, READ_BUFFER_SIZE, bytes_read);
      do {
	  len = avcodec_decode_video(lavc_dec_context, &picture,
		  &got_picture, buffer+buf_len, mp4_size-buf_len);

	  if (len < 0) {
	      fprintf(stderr, "[%s] frame decoding failed\n", MOD_NAME);
	      goto decoder_error;
	  }
	  if (verbose_flag & TC_DEBUG) fprintf (stderr, "here frame pic %d run %d len %d\n", got_picture, run, len);
	  if (run++>10000) { fprintf(stderr, "[%s] Fatal decoder error\n", MOD_NAME); goto decoder_error; }
      } while (!got_picture);
      run = 0;

      buf_len += len;

      YUV_INIT_PLANES(planes, out_buffer, IMG_YUV420P,
		      lavc_dec_context->width, lavc_dec_context->height);

      // Convert avcodec image to the requested YUV or RGB format
      switch (lavc_dec_context->pix_fmt) {
	case PIX_FMT_YUVJ420P:
	case PIX_FMT_YUV420P:
	    // Remove "dead space" at right edge of planes, if any
	    if (picture.linesize[0] != lavc_dec_context->width) {
		int y;
		for (y = 0; y < lavc_dec_context->height; y++) {
		    ac_memcpy(picture.data[0] + y*lavc_dec_context->width,
			      picture.data[0] + y*picture.linesize[0],
			      lavc_dec_context->width);
		    if (y%2 == 0) {
			ac_memcpy(picture.data[1] + y*(lavc_dec_context->width/2),
				  picture.data[1] + y*picture.linesize[1],
				  lavc_dec_context->width/2);
			ac_memcpy(picture.data[2] + y*(lavc_dec_context->width/2),
				  picture.data[2] + y*picture.linesize[2],
				  lavc_dec_context->width/2);
		    }
		}
	    }
	    ac_imgconvert(picture.data, IMG_YUV420P, planes,
			  pix_fmt==TC_CODEC_YUV420P ? IMG_YUV420P : IMG_RGB_DEFAULT,
			  lavc_dec_context->width, lavc_dec_context->height);
	    break;
	case PIX_FMT_YUV411P:
	    if (picture.linesize[0] != lavc_dec_context->width) {
		int y;
		for (y = 0; y < lavc_dec_context->height; y++) {
		    ac_memcpy(picture.data[0] + y*lavc_dec_context->width,
			      picture.data[0] + y*picture.linesize[0],
			      lavc_dec_context->width);
		    ac_memcpy(picture.data[1] + y*(lavc_dec_context->width/4),
			      picture.data[1] + y*picture.linesize[1],
			      lavc_dec_context->width/4);
		    ac_memcpy(picture.data[2] + y*(lavc_dec_context->width/4),
			      picture.data[2] + y*picture.linesize[2],
			      lavc_dec_context->width/4);
		}
	    }
	    ac_imgconvert(picture.data, IMG_YUV411P, planes,
			  pix_fmt==TC_CODEC_YUV420P ? IMG_YUV420P : IMG_RGB_DEFAULT,
			  lavc_dec_context->width, lavc_dec_context->height);
	    break;
	case PIX_FMT_YUVJ422P:
	case PIX_FMT_YUV422P:
	    if (picture.linesize[0] != lavc_dec_context->width) {
		int y;
		for (y = 0; y < lavc_dec_context->height; y++) {
		    ac_memcpy(picture.data[0] + y*lavc_dec_context->width,
			      picture.data[0] + y*picture.linesize[0],
			      lavc_dec_context->width);
		    ac_memcpy(picture.data[1] + y*(lavc_dec_context->width/2),
			      picture.data[1] + y*picture.linesize[1],
			      lavc_dec_context->width/2);
		    ac_memcpy(picture.data[2] + y*(lavc_dec_context->width/2),
			      picture.data[2] + y*picture.linesize[2],
			      lavc_dec_context->width/2);
		}
	    }
	    ac_imgconvert(picture.data, IMG_YUV422P, planes,
			  pix_fmt==TC_CODEC_YUV420P ? IMG_YUV420P : IMG_RGB_DEFAULT,
			  lavc_dec_context->width, lavc_dec_context->height);
	    break;
	case PIX_FMT_YUVJ444P:
	case PIX_FMT_YUV444P:
	    if (picture.linesize[0] != lavc_dec_context->width) {
		int y;
		for (y = 0; y < lavc_dec_context->height; y++) {
		    ac_memcpy(picture.data[0] + y*lavc_dec_context->width,
			      picture.data[0] + y*picture.linesize[0],
			      lavc_dec_context->width);
		    ac_memcpy(picture.data[1] + y*lavc_dec_context->width,
			      picture.data[1] + y*picture.linesize[1],
			      lavc_dec_context->width);
		    ac_memcpy(picture.data[2] + y*lavc_dec_context->width,
			      picture.data[2] + y*picture.linesize[2],
			      lavc_dec_context->width);
		}
	    }
	    ac_imgconvert(picture.data, IMG_YUV444P, planes,
			  pix_fmt==TC_CODEC_YUV420P ? IMG_YUV420P : IMG_RGB_DEFAULT,
			  lavc_dec_context->width, lavc_dec_context->height);
	    break;
	default:
	    fprintf(stderr, "[%s] Unsupported decoded frame format", MOD_NAME);
	    goto decoder_error;
      }

      /* buffer more than half empty -> Fill it */
      if (!flush && buf_len > mp4_size/2+1) {
	  int rest = mp4_size - buf_len;
	  if (verbose_flag & TC_DEBUG) fprintf(stderr, "FILL rest %d\n", rest);

	  /* Move data if needed */
	  if (rest)
	      memmove(buffer, buffer+buf_len, READ_BUFFER_SIZE-buf_len);

	  /* read new data */
	  if ( (bytes_read = tc_pread(decode->fd_in, (uint8_t*) (buffer+(READ_BUFFER_SIZE-buf_len)), buf_len) )  != buf_len) {
	      if (verbose_flag & TC_DEBUG) fprintf(stderr, "read failed read (%ld) should (%d)\n", bytes_read, buf_len);
	      flush = 1;
	      mp4_size -= buf_len;
	      mp4_size += bytes_read;
	  }
	  buf_len = 0;
      }
      //fprintf(stderr, "SIZE: (%d) MP4(%d) blen(%d) BUF(%d) read(%ld)\n", len, mp4_size, buf_len, READ_BUFFER_SIZE, bytes_read);
      if (mp4_size<=0) {

	  if (verbose_flag & TC_DEBUG) fprintf(stderr, "no more bytes\n");
	  break;
      }

      if (tc_pwrite(decode->fd_out, out_buffer, frame_size) != frame_size) {
	  goto decoder_error;
      }

  } while (1);

  import_exit(0);
  return;

decoder_error:
  import_exit(1);
}

