/*
 *  export_mov.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *  Copyright (C) (2002) Christian Vogelgsang 
 *  <Vogelgsang@informatik.uni-erlangen.de> (extension for all codecs supported
 *  by quicktime4linux
 *  Copyright (C) ken@hero.com 2001 (initial module author)
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

#define MOD_NAME    "export_mov.so"
#define MOD_VERSION "v0.1.0 (2002-01-31)"
#define MOD_CODEC   "(video) * | (audio) *"
#define MOD_PRE mov
#include "export_def.h"

#include <quicktime.h>

/* verbose flag */
static int verbose_flag=TC_QUIET;

/* set encoder capabilities */
static int capability_flag=
 /* audio formats */
 TC_CAP_PCM| /* pcm audio */
 /* video formats */
 TC_CAP_RGB|  /* rgb frames */
 TC_CAP_YUV|  /* chunky YUV 4:2:0 */
 TC_CAP_YUY2; /* chunky YUV 4:2:2 */

/* exported quicktime file */
static quicktime_t *qtfile = NULL;

/* row pointer for source frames */
static unsigned char** row_ptr = NULL;

/* toggle for raw frame export */
static int rawVideo = 0;
static int rawAudio = 0;

/* store frame dimension */
static int w = 0,h = 0;

/* audio channels */
static int channels = 0;

/* sample size */
static int bits = 0;

/* encode audio buffers */
static int16_t* audbuf0 = NULL;
static int16_t* audbuf1 = NULL;

/* ------------------------------------------------------------ 
 *
 * open outputfile
 *
 * ------------------------------------------------------------*/

MOD_open
{
  if(param->flag == TC_VIDEO) 
    return(0);
  
  if(param->flag == TC_AUDIO) 
    return(0);
  
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{
  /* video setup -------------------------------------------------- */
  if(param->flag == TC_VIDEO) {
    char *qt_codec;
    
    /* fetch frame size */
    w = vob->ex_v_width;
    h = vob->ex_v_height;

    /* check frame format */
    if( ((w%16)!=0) || ((h%16)!=0) ) {
      fprintf(stderr,"[%s] width/height must be multiples of 16\n",
	      MOD_NAME);
      fprintf(stderr,"[%s] Try the -j option\n",
	      MOD_NAME);
      return(TC_EXPORT_ERROR);
    }

    /* open target file for writing */
    if(NULL == (qtfile = quicktime_open(vob->video_out_file, 0, 1)) ){
      fprintf(stderr,"[%s] error opening qt file '%s'\n",
	      MOD_NAME,vob->video_out_file);
      return(TC_EXPORT_ERROR);
    }

    /* fetch qt codec from -F switch */
    qt_codec = vob->ex_v_fcc;
    if(qt_codec == NULL || strlen(qt_codec)==0) {
      //default
      qt_codec = "mjpa";
      fprintf(stderr,"[%s] empty qt codec name - switching to %s\n", MOD_NAME, qt_codec);
    }

    /* set proposed video codec */
    quicktime_set_video(qtfile, 1, w, h, vob->fps,qt_codec); 
     
    /* check given qt video compressor */
    switch(vob->im_v_codec) {
    case CODEC_RGB:
      /* check if we can encode with this codec */
      if (!quicktime_supported_video(qtfile, 0)){
	fprintf(stderr,"[%s] qt video codec '%s' unsupported\n",
		MOD_NAME,qt_codec);
	return(TC_EXPORT_ERROR);
      }

      /* quicktime quality is 1-100, so 20,40,60,80,100 */
      quicktime_set_jpeg(qtfile, 20 * vob->divxquality, 0);

      /* alloc row pointers for frame encoding */
      row_ptr = malloc (sizeof(char *) * h);
    
      /* we need to encode frames */
      rawVideo = 0;
      break;

    case CODEC_YUV: /* 4:2:0 */
      /* only one codec fits :) */
      if(strcmp(qt_codec,QUICKTIME_YUV420)!=0) {
	fprintf(stderr,"[%s] qt codec '%s' given, but not suitable for YUV\n",
		MOD_NAME,qt_codec);
	fprintf(stderr,"[%s] Try RGB mode or use '%s'\n",
		MOD_NAME,QUICKTIME_YUV420);
	return(TC_EXPORT_ERROR);
      }
      /* raw write frames */
      rawVideo = 1;
      break;

    case CODEC_YUY2: /* 4:2:2 */
      /* only one codec fits :) */
      if(strcmp(qt_codec,QUICKTIME_YUV2)!=0) {
	fprintf(stderr,"[%s] qt codec '%s' given, but not suitable for YUY2\n",
		MOD_NAME,qt_codec);
	fprintf(stderr,"[%s] Try RGB mode or use '%s'\n",
		MOD_NAME,QUICKTIME_YUV2);
	return(TC_EXPORT_ERROR);
      }
      /* raw write frames */
      rawVideo = 1;
      break;

    default:
      /* unsupported internal format */
      fprintf(stderr,"[%s] unsupported internal video format %x\n",
	      MOD_NAME,vob->ex_v_codec);
      return(TC_EXPORT_ERROR);
      break;
    }

    /* verbose */
    fprintf(stderr,"[%s] video codec='%s' w=%d h=%d fps=%g\n",
	    MOD_NAME,qt_codec,w,h,vob->fps);

    return(0);
  }

  /* audio setup -------------------------------------------------- */
  if(param->flag == TC_AUDIO){
    char *qt_codec;
    
    /* check given audio format */
    if((vob->a_chan!=1)&&(vob->a_chan!=2)) {
      fprintf(stderr,"[%s] Only mono or stereo audio supported\n",
	      MOD_NAME);
      return(TC_EXPORT_ERROR);
    }
    channels = vob->a_chan;
    bits = vob->a_bits;

    /* fetch qt codec from -F switch */
    qt_codec = vob->ex_a_fcc;
    if(qt_codec == NULL || strlen(qt_codec)==0) {
      qt_codec = "ima4";
      fprintf(stderr,"[%s] empty qt codec name - switching to %s\n",MOD_NAME, qt_codec);
    }

    /* setup audio parameters */
    quicktime_set_audio(qtfile,channels,vob->a_rate,bits,qt_codec);

    /* check encoder mode */
    switch(vob->im_a_codec) {
    case CODEC_PCM:
      /* check if we can encode with this codec */
      if (!quicktime_supported_audio(qtfile, 0)){
	fprintf(stderr,"[%s] qt audio codec '%s' unsupported\n",
		MOD_NAME,qt_codec);
	return(TC_EXPORT_ERROR);
      }

      /* allocate sample buffers */
      audbuf0 = (int16_t*)malloc(vob->ex_a_size);
      audbuf1 = (int16_t*)malloc(vob->ex_a_size);

      /* need to encode audio */
      rawAudio = 0;
      break;

    default:
      /* unsupported internal format */
      fprintf(stderr,"[%s] unsupported internal audio format %x\n",
	      MOD_NAME,vob->ex_v_codec);
      return(TC_EXPORT_ERROR);
      break;
    }

    /* verbose */
    fprintf(stderr,"[%s] audio codec='%s' bits=%d chan=%d rate=%d\n",
	    MOD_NAME,qt_codec,bits,channels,vob->a_rate);

    return(0);
  }
  
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * encode and export frame
 *
 * ------------------------------------------------------------*/


MOD_encode
{
  /* video -------------------------------------------------------- */
  if(param->flag == TC_VIDEO) {
    /* raw mode is easy */
    if(rawVideo) {
      if(quicktime_write_frame(qtfile,param->buffer,param->size,0)<0) {
	fprintf(stderr, "[%s] error writing raw video frame\n",
		MOD_NAME);
	return(TC_EXPORT_ERROR);
      }
    }
    /* encode frame */
    else {
      /* setup row pointers for RGB: inverse! */
      int iy,sl = w*3;
      char *ptr = param->buffer;
      for(iy=0;iy<h;iy++){
	row_ptr[iy] = ptr;
	ptr += sl;
      }

      /* encode frame */
      if(quicktime_encode_video(qtfile, row_ptr, 0)<0) {
	fprintf(stderr, "[%s] error encoding video frame\n",MOD_NAME);
	return(TC_EXPORT_ERROR);
      }
    }
    return(0);
  }
  
  /* audio -------------------------------------------------------- */
  if(param->flag == TC_AUDIO){
    /* raw mode is easy */
    if(rawAudio) {
      if(quicktime_write_frame(qtfile,
			       param->buffer,param->size,0)<0) {
	fprintf(stderr, "[%s] error writing raw audio frame\n",
		MOD_NAME);
	return(TC_EXPORT_ERROR);
      }
    }
    /* encode audio */
    else {
      int s,t;
      int16_t *aptr[2] = { audbuf0, audbuf1 };

      /* calc number of samples */
      int samples = param->size;
      if(bits==16)
	samples >>= 1;
      if(channels==2)
	samples >>= 1;

      /* fill audio buffer */
      if(bits==8) {
	/* UNTESTED: please verify :) */
	if(channels==1) {
	  for(s=0;s<samples;s++) 
	    audbuf0[s] = ((((int16_t)param->buffer[s]) << 8)-0x8000);
	} else /* stereo */ {
	  for(s=0,t=0;s<samples;s++,t+=2) {
	    audbuf0[s] = ((((int16_t)param->buffer[t]) << 8)-0x8000);
	    audbuf1[s] = ((((int16_t)param->buffer[t+1]) << 8)-0x8000);
	  }
	}
      }
      else /* 16 bit */ {
	if(channels==1) {
	  aptr[0] = (int16_t *)param->buffer;
	} else /* stereo */ {
	  /* decouple channels */
	  for(s=0,t=0;s<samples;s++,t+=2) {
	    audbuf0[s] = ((int16_t *)param->buffer)[t];
	    audbuf1[s] = ((int16_t *)param->buffer)[t+1];
	  }
	}
      }

      /* encode audio samples */
      if ((quicktime_encode_audio(qtfile, aptr, NULL, samples)<0)){
	fprintf(stderr,"[%s] error encoding audio frame\n",MOD_NAME);
	return(TC_EXPORT_ERROR);
      }
    }
    return(0);
  }
  
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
    /* free row pointers */
    if(row_ptr!=NULL)
      free(row_ptr);
    return(0);
  }
  
  if(param->flag == TC_AUDIO) {
    /* free audio buffers */
    if(audbuf0!=NULL)
      free(audbuf0);
    if(audbuf1!=NULL)
      free(audbuf1);
    return(0);
  }
  
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * close outputfile
 *
 * ------------------------------------------------------------*/

MOD_close
{  
  if(param->flag == TC_VIDEO){
    /* close quicktime file */
    quicktime_close(qtfile);
    qtfile=NULL;
    return(0);
  }
  
  if(param->flag == TC_AUDIO) {
    return(0);
  }
  
  return(TC_EXPORT_ERROR);
  
}




