/*
 *  export_dvraw.c
 *
 *  Copyright (C) Thomas �streich - June 2001
 *
 *  This file is part of transcode, a video stream processing tool
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
#include <libdv/dv.h>
#include "transcode.h"
#include "vid_aux.h"
#include "optstr.h"
#include "ioaux.h"

#define MOD_NAME    "export_dvraw.so"
#define MOD_VERSION "v0.4 (2003-10-14)"
#define MOD_CODEC   "(video) Digital Video | (audio) PCM"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_YUV|TC_CAP_VID|TC_CAP_YUV422;

#define MOD_PRE dvraw
#include "export_def.h"

static int fd;

static int16_t *audio_bufs[4];

static uint8_t *target, *vbuf;

static dv_encoder_t *encoder = NULL;
static uint8_t *pixels[3], *tmp_buf;

static int frame_size=0, format=0;
static int pass_through=0;

static int chans, rate;
static int dv_yuy2_mode=0;
static int dv_uyvy_mode=0;

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

#if 0  /* get this from ioaux.c */
static int p_write (int fd, char *buf, size_t len)
{
   size_t n = 0;
   size_t r = 0;

   while (r < len) {
      n = write (fd, buf + r, len - r);
      if (n < 0)
         return n;
      
      r += n;
   }
   return r;
}
#endif

#if 0
static void pcm_swap(char *buffer, int len)
{
  char *in, *out;

  int n;

  char tt;

  in  = buffer;
  out = buffer;

  for(n=0; n<len; n=n+2) {

    tt = *(in+1);
    *(out+1) = *in;
    *out = tt;
    
    in = in+2;
    out = out+2;
  }
}
#endif

/* ------------------------------------------------------------ 
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{
  
  int i;
  
  if(param->flag == TC_VIDEO) {
    
    target = bufalloc(TC_FRAME_DV_PAL);
    vbuf = bufalloc(PAL_W*PAL_H*3);

    if(vob->dv_yuy2_mode) {
      tmp_buf = bufalloc(PAL_W*PAL_H*2); //max frame
      dv_yuy2_mode=1;
    }

    if (vob->im_v_codec == CODEC_YUV422) {
      tmp_buf = bufalloc(PAL_W*PAL_H*2); //max frame
      dv_uyvy_mode=1;
    }
    
    encoder = dv_encoder_new(FALSE, FALSE, FALSE);
    
    return(0);
  }
  
  if(param->flag == TC_AUDIO) {
    
    // tmp audio buffer
    for(i=0; i < 4; i++) {
      if(!(audio_bufs[i] = malloc(DV_AUDIO_MAX_SAMPLES * sizeof(int16_t)))) {
	fprintf(stderr, "(%s) out of memory\n", __FILE__);
	return(TC_EXPORT_ERROR); 
      }  
    }	  
    
    return(0);
  }
  
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
  int bytealignment;
  int bytespersecond;
  int bytesperframe;
  
  if(param->flag == TC_VIDEO) {
    
    // video
    if((fd = open(vob->video_out_file, O_RDWR|O_CREAT|O_TRUNC,
		  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH))<0) {
      perror("open file");
      
      return(TC_EXPORT_ERROR);
    }     
    
    switch(vob->im_v_codec) {
      
    case CODEC_RGB:
      format=0;

      if(verbose & TC_DEBUG) fprintf(stderr, "[%s] raw format is RGB\n", MOD_NAME);
      break;
      
    case CODEC_YUV:
      format=1;
      
      if(verbose & TC_DEBUG) fprintf(stderr, "[%s] raw format is YV12\n", MOD_NAME);
      break;

    case CODEC_YUV422:
      format=1;
      
      if(verbose & TC_DEBUG) fprintf(stderr, "[%s] raw format is UYVY\n", MOD_NAME);
      break;
      
      
    case CODEC_RAW:
    case CODEC_RAW_YUV:
      format=1;
      pass_through=1;
      break;
      
    default:
      
      fprintf(stderr, "[%s] codec not supported\n", MOD_NAME);
      return(TC_EXPORT_ERROR); 
      
      break;
    }
    
    // for reading
    frame_size = (vob->ex_v_height==PAL_H) ? TC_FRAME_DV_PAL:TC_FRAME_DV_NTSC;

    if(verbose & TC_DEBUG) fprintf(stderr, "[%s] encoding to %s DV\n", MOD_NAME, (vob->ex_v_height==PAL_H) ? "PAL":"NTSC");

    // Store aspect ratio - ex_asr uses the value 3 for 16x9
    encoder->is16x9 = ((vob->ex_asr<0) ? vob->im_asr:vob->ex_asr) == 3;
    encoder->isPAL = (vob->ex_v_height==PAL_H);
    encoder->vlc_encode_passes = 3;
    encoder->static_qno = 0;
    if (vob->ex_v_string != NULL)
      if (optstr_get (vob->ex_v_string, "qno", "%d", &encoder->static_qno) == 1)
        printf("[%s] using quantisation: %d\n", MOD_NAME, encoder->static_qno);
    encoder->force_dct = DV_DCT_AUTO;

    return(0);
  }
  
  
  if(param->flag == TC_AUDIO) {
    
    if (!encoder) {
      tc_warn("[export_dvraw] -y XXX,dvraw is not possible without the video");
      tc_warn("[export_dvraw] export module also being dvraw");
      return (TC_EXPORT_ERROR);
    }
    chans = vob->dm_chan;
    //re-sampling only with -J resample possible
    rate = vob->a_rate;

    bytealignment = (chans==2) ? 4:2;
    bytespersecond = rate * bytealignment;
    bytesperframe = bytespersecond/(encoder->isPAL ? 25 : 30);

    if(verbose & TC_DEBUG) fprintf(stderr, "[%s] audio: CH=%d, f=%d, balign=%d, bps=%d, bpf=%d\n", MOD_NAME, chans, rate, bytealignment, bytespersecond, bytesperframe);

    return(0);
  }
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
  int i;

  if(param->flag == TC_VIDEO) { 
    
    if(pass_through) {
      tc_memcpy(target, param->buffer, frame_size);
    } else { 
      tc_memcpy(vbuf, param->buffer, param->size);
    }
    
    if(verbose & TC_STATS) fprintf(stderr, "[%s] ---V---\n", MOD_NAME);

    return(0);
  }
  
  if(param->flag == TC_AUDIO) {
    
    time_t now = time(NULL);

    if(verbose & TC_STATS) fprintf(stderr, "[%s] ---A---\n", MOD_NAME);
    
    if(!pass_through) {

      pixels[0] = (char *) vbuf;
      
      if(encoder->isPAL) {
	pixels[2]=(char *) vbuf + PAL_W*PAL_H;
	pixels[1]=(char *) vbuf + (PAL_W*PAL_H*5)/4;
      } else {
	pixels[2]=(char *) vbuf + NTSC_W*NTSC_H;
	pixels[1]=(char *) vbuf + (NTSC_W*NTSC_H*5)/4;
      }
      
      if(dv_yuy2_mode && !dv_uyvy_mode) {	
	yv12toyuy2(pixels[0], pixels[1], pixels[2], tmp_buf, PAL_W, (encoder->isPAL)? PAL_H : NTSC_H);
	pixels[0]=tmp_buf;
      }

      if (dv_uyvy_mode) {
	  uyvytoyuy2(pixels[0], tmp_buf, PAL_W, (encoder->isPAL)? PAL_H : NTSC_H);
	  pixels[0]=tmp_buf;
      }
      
      dv_encode_full_frame(encoder, pixels, (format)?e_dv_color_yuv:e_dv_color_rgb, target);
      
    }//no pass-through
#ifdef LIBDV_099
        encoder->samples_this_frame = param->size / (sizeof(int16_t) * 2);
        /* 
         * real sample number is 
         * amount of data / (sample size * channel number)
         */
#endif
      dv_encode_metadata(target, encoder->isPAL, encoder->is16x9, &now, 0);
      dv_encode_timecode(target, encoder->isPAL, 0);

#ifdef WORDS_BIGENDIAN
      for (i=0; i<param->size; i+=2) {
	  char tmp = param->buffer[i];
	  param->buffer[i] = param->buffer[i+1];
	  param->buffer[i+1] = tmp;
      }
#endif

      // Although dv_encode_full_audio supports 4 channels, the internal
      // PCM data (param->buffer) is only carrying 2 channels so only deal
      // with 1 or 2 channel audio.
      // Work around apparent bug in dv_encode_full_audio when chans == 1
      // by putting silence in 2nd channel and calling with chans = 2
      if (chans == 1) {
          audio_bufs[0] = (int16_t *)param->buffer;
          memset(audio_bufs[1], 0, DV_AUDIO_MAX_SAMPLES * 2);
          dv_encode_full_audio(encoder, audio_bufs, 2, rate, target);
      }
      else {
          // assume 2 channel, demultiplex for libdv API
          for(i=0; i < param->size/4; i++) {
              audio_bufs[0][i] = ((int16_t *)param->buffer)[i*2];
              audio_bufs[1][i] = ((int16_t *)param->buffer)[i*2+1];
          }
          dv_encode_full_audio(encoder, audio_bufs, chans, rate, target);
      }


    //write raw DV frame
    
    if(p_write(fd, target, frame_size) != frame_size) {    
      perror("write frame");
      return(TC_EXPORT_ERROR);
    }     
    
    return(0);
  }
  
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
  
  int i;
  
  if(param->flag == TC_VIDEO) {
    
    dv_encoder_free(encoder);  
    
    return(0);
  }
  
  if(param->flag == TC_AUDIO) {
    for(i=0; i < 4; i++) free(audio_bufs[i]);
    return(0);
  }  
  
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * close outputfiles
 *
 * ------------------------------------------------------------*/

MOD_close
{  

  if(param->flag == TC_VIDEO) {
    close(fd);
    return(0);
  }

  if(param->flag == TC_AUDIO) return(0);
  
  return(TC_EXPORT_ERROR);  

}

