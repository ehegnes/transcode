/*
 *  export_dvraw.c
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

#define MOD_NAME    "export_dvraw.so"
#define MOD_VERSION "v0.1.3 (2002-12-19)"
#define MOD_CODEC   "(video) Digital Video | (audio) PCM"

#define MOD_PRE dvraw
#include "export_def.h"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_YUV|TC_CAP_VID;

static int fd;

static int16_t *audio_bufs[4];

unsigned char *target;
unsigned char vbuf[SIZE_RGB_FRAME];

#ifdef LIBDV_095
static dv_encoder_t *encoder = NULL;
static unsigned char *pixels[3];
#endif

static int frame_size=0, format=0;
static int pass_through=0;

static int chans, rate;

dv_enc_audio_info_t audio;

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
    
#ifdef LIBDV_095
    encoder = dv_encoder_new(FALSE, FALSE, FALSE);
#else
    dvenc_init();
#endif
    
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
  int format;
  
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
      break;
      
    case CODEC_YUV:
      format=1;
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
  
  
  if(param->flag == TC_AUDIO) {
    
    chans = (vob->dm_chan != vob->a_chan) ? vob->dm_chan : vob->a_chan;
    //re-sampling only with -J resample possible
    rate = vob->a_rate;

    audio.channels = chans;
    audio.frequency = rate;
    audio.bitspersample = 16;
    audio.bytealignment = (chans==2) ? 4:2;
    audio.bytespersecond = rate * audio.bytealignment;
    audio.bytesperframe = audio.bytespersecond/(encoder->isPAL ? 25 : 30);

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

  if(param->flag == TC_VIDEO) { 
    
    if(pass_through) {
      memcpy(target, param->buffer, frame_size);
    } else { 
      memcpy(vbuf, param->buffer, param->size);
    }
    return(0);
  }
  
  if(param->flag == TC_AUDIO) {
    
    time_t now = time(NULL);
    
    if(!pass_through) {

#ifdef LIBDV_095
      pixels[0] = (char *) vbuf;
      
      if(encoder->isPAL) {
	pixels[2]=(char *) vbuf + PAL_W*PAL_H;
	pixels[1]=(char *) vbuf + (PAL_W*PAL_H*5)/4;
      } else {
	pixels[2]=(char *) vbuf + NTSC_W*NTSC_H;
	pixels[1]=(char *) vbuf + (NTSC_W*NTSC_H*5)/4;
      }

      dv_encode_full_frame(encoder, pixels, (format)?e_dv_color_yuv:e_dv_color_rgb, target);
      dv_encode_metadata(target, encoder->isPAL, encoder->is16x9, &now, 0);
      dv_encode_timecode(target, encoder->isPAL, 0);
      
      dv_encode_full_audio(encoder, audio_bufs, chans, rate, target);
      
      memcpy(audio.data, param->buffer, param->size);
      dvenc_insert_audio(target, &audio, encoder->isPAL);
#else    
      //merge audio     
      dvenc_frame(vbuf, param->buffer, param->size, target);
#endif
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
  
  //  int i;
  
  if(param->flag == TC_VIDEO) {
    
#ifdef LIBDV_095
    dv_encoder_free(encoder);  
#else    
    dvenc_close();
#endif
    
    return(0);
  }
  
  if(param->flag == TC_AUDIO) {
    //    for(i=0; i < 4; i++) free(audio_bufs[i]);
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

