/*
 *  import_mov.c
 *
 *  Copyright (C) Thomas Östreich - January 2002
 *  updated by Christian Vogelgsang <Vogelgsang@informatik.uni-erlangen.de> 
 *
 *  This file is part of transcode, a linux video stream  processing tool
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
#include <quicktime.h>
#include "transcode.h"

#define MOD_NAME    "import_mov.so"
#define MOD_VERSION "v0.1.2 (2002-05-16)"
#define MOD_CODEC   "(video) * | (audio) *"

#define MOD_PRE mov
#include "import_def.h"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_YUV;

/* movie handles */
static quicktime_t *qt_audio=NULL;
static quicktime_t *qt_video=NULL;

/* row pointer for decode frame */
static unsigned char **ptr = 0;
/* raw or decode frame */
static int rawVideoMode = 0;
/* raw or decode audio */
static int rawAudioMode = 0;
/* frame size */
static int width=0, height=0;
/* number of audio channels */
static int chan=0;
/* number of audio bits */
static int bits=0;

static int frames=0;

/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  /* audio */
  if(param->flag == TC_AUDIO) {
    int numTrk;
    long rate;
    char *codec;

    param->fd = NULL;
    
    /* open movie for audio extraction */
    if(qt_audio==NULL) {
      if(NULL == (qt_audio = quicktime_open(vob->audio_in_file,1,0))){
	fprintf(stderr,"error: can't open quicktime!\n");
	return(TC_IMPORT_ERROR); 
      } 
    }   

    /* check for audio track */
    numTrk = quicktime_audio_tracks(qt_audio);
    if(numTrk==0) {
      fprintf(stderr,"error: no audio track in quicktime found!\n");
      return(TC_IMPORT_ERROR); 
    }
    
    /* extract audio parameters */
    rate   = quicktime_sample_rate(qt_audio, 0);
    chan   = quicktime_track_channels(qt_audio, 0);
    bits   = quicktime_audio_bits(qt_audio, 0);
    codec  = quicktime_audio_compressor(qt_audio, 0);

    /* verbose info */
    fprintf(stderr, "[%s] codec=%s, rate=%ld Hz, bits=%d, channels=%d\n", 
	    MOD_NAME, codec, rate, bits, chan);

    /* check bits */
    if((bits!=8)&&(bits!=16)) {
      fprintf(stderr,"error: unsupported sample bits: %d\n",bits);
      return(TC_IMPORT_ERROR);
    }

    /* check channels */
    if(chan>2) {
      fprintf(stderr,"error: too many audio channels: %d\n",chan);
      return(TC_IMPORT_ERROR);
    }
    
    /* check codec string */
    if(strlen(codec)==0) {
      fprintf(stderr, "error: empty codec in quicktime?\n");
      return(TC_IMPORT_ERROR);
    }

    /* check if audio compressor is supported */
    if(quicktime_supported_audio(qt_audio, 0)!=0) {
      rawAudioMode = 0;
    } 
    /* RAW PCM is directly supported */
    else if(strcasecmp(codec,QUICKTIME_RAW)==0) {
      rawAudioMode = 1;
      fprintf(stderr,"[%s] using RAW audio mode!\n",MOD_NAME);
    }
    /* unsupported codec */
    else {
      fprintf(stderr, "error: quicktime audio codec '%s' not supported!\n",
	      codec);
      return(TC_IMPORT_ERROR);
    }
    return(0);
  }

  /* video */
  if(param->flag == TC_VIDEO) {
    double fps;
    char *codec;
    int numTrk;
  
    param->fd = NULL;

    /* open movie for video extraction */
    if(qt_video==NULL) 
      if(NULL == (qt_video = quicktime_open(vob->video_in_file,1,0))){
	fprintf(stderr,"error: can't open quicktime!\n");
	return(TC_IMPORT_ERROR); 
      }
    
    /* check for audio track */
    numTrk = quicktime_video_tracks(qt_video);
    if(numTrk==0) {
      fprintf(stderr,"error: no video track in quicktime found!\n");
      return(TC_IMPORT_ERROR); 
    }

    /* read all video parameter from input file */
    width  =  quicktime_video_width(qt_video, 0);
    height =  quicktime_video_height(qt_video, 0);    
    fps    =  quicktime_frame_rate(qt_video, 0);
    codec  =  quicktime_video_compressor(qt_video, 0);
    
    /* verbose info */
    fprintf(stderr, "[%s] codec=%s, fps=%6.3f, width=%d, height=%d\n", 
	    MOD_NAME, codec, fps, width, height);

    //ThOe total frames
    frames=quicktime_video_length(qt_video, 0);

    /* check codec string */
    if(strlen(codec)==0) {
      fprintf(stderr, "error: empty codec in quicktime?\n");
      return(TC_IMPORT_ERROR);
    }

    /* RGB import */
    if(vob->im_v_codec == CODEC_RGB) {

      /* check if a suitable compressor is available */
      if(quicktime_supported_video(qt_video,0)==0) {
	fprintf(stderr, "error: quicktime codec '%s' not supported for RGB!\n",
		codec);
	return(TC_IMPORT_ERROR);
      }

      rawVideoMode = 0;

      /* allocate buffer for row pointers */
      ptr = malloc(height*sizeof(char *));
      if(ptr==0) {
	fprintf(stderr,"error: can't alloc row pointers\n");
	return(TC_IMPORT_ERROR);
      }
    }
    /* YUV import */
    else if(vob->im_v_codec == CODEC_YUV) {

      /* accept only 4:2:0 YUV */
      if((strcasecmp(codec,QUICKTIME_YUV4)!=0)&&
	 (strcasecmp(codec,QUICKTIME_YUV420)!=0)) {
	fprintf(stderr, "error: quicktime codec '%s' not suitable for YUV!\n",
		codec);
	return(TC_IMPORT_ERROR);
      }

      rawVideoMode = 1;
    }
    /* unknown import mode */
    else {
      fprintf(stderr,"error: unknown codec mode!\n");
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
  /* video */
  if(param->flag == TC_VIDEO) {
    if(rawVideoMode) {
      /* read frame raw */
      param->size = quicktime_read_frame(qt_video, param->buffer, 0);
      if(param->size<=0) {
	if(verbose & TC_DEBUG) 
	  fprintf(stderr,"quicktime read video frame");
	return(TC_IMPORT_ERROR);
      }
    } else {
      /* decode frame */
      int y;
      char *mem = param->buffer;
      
      /* set row pointers for buffer */
      for(y=0;y<height;y++) {
	ptr[y] = mem;
	mem += width * 3;
      }
      /* decode the next frame */
      if(quicktime_decode_video(qt_video,ptr,0)<0) {
	if(verbose & TC_DEBUG)
	  fprintf(stderr,"can't decode frame");
	return(TC_IMPORT_ERROR);
      }
      param->size = (mem-param->buffer);
    }

    //ThOe trust file header and terminate after all frames have been processed.
    if(frames--==0) return(TC_IMPORT_ERROR);
    return(0);
  }

  /* audio */
  if(param->flag == TC_AUDIO) {
    int bytes_read;

    /* raw read mode */
    if(rawAudioMode) {
      bytes_read = quicktime_read_audio(qt_audio, 
					param->buffer, param->size, 0);
    } 
    /* decode audio mode */
    else {
      long pos = quicktime_audio_position(qt_audio,0);
      long samples = param->size;
      if(bits==16)
	samples >>= 1;

      /* mono */
      if(chan==1) {
	/* direct copy */
	bytes_read = quicktime_decode_audio(qt_audio,
					    (int16_t *)param->buffer,NULL,
					    samples,0);

	/* check result */
	if(bytes_read<0) {
	  if(verbose & TC_DEBUG) 
	    fprintf(stderr,"error: reading quicktime audio frame!\n");
	  return(TC_IMPORT_ERROR);
	}
      }
      /* stereo */
      else {
	int16_t *tgt;
	int16_t *tmp;
	int s,t;

	samples >>= 1;
	tgt = (int16_t *)param->buffer;
	tmp = (int16_t *)malloc(samples*sizeof(int16_t));

	/* read first channel into target buffer */
	bytes_read = quicktime_decode_audio(qt_audio,tgt,NULL,samples,0);
	if(bytes_read<0) {
	  if(verbose & TC_DEBUG) 
	    fprintf(stderr,"error: reading quicktime audio frame!\n");
	  return(TC_IMPORT_ERROR);
	}

	/* read second channel in temp buffer */
	quicktime_set_audio_position(qt_audio,pos,0);
	bytes_read = quicktime_decode_audio(qt_audio,tmp,NULL,samples,1);
	if(bytes_read<0) {
	  if(verbose & TC_DEBUG) 
	    fprintf(stderr,"error: reading quicktime audio frame!\n");
	  return(TC_IMPORT_ERROR);
	}

	/* spread first channel */
	for(s=samples-1;s>=0;s--)
	  tgt[s<<1] = tgt[s];

	/* fill in second channel from temp buffer */
	t = 1;
	for(s=0;s<samples;s++) {
	  tgt[t] = tmp[s];
	  t += 2;
	}

	free(tmp);
      }
      quicktime_set_audio_position(qt_audio,pos+samples,0);
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
  /* free up audio */
  if(param->flag == TC_AUDIO) {
    if(qt_audio!=NULL) {
      quicktime_close(qt_audio);
      qt_audio=NULL;
    }
    return(0);
  }
  
  /* free up video */
  if(param->flag == TC_VIDEO) {
    if(qt_video!=NULL) {
      quicktime_close(qt_video);
      qt_video=NULL;
    }
    /* free row pointer */
    if(ptr!=0)
      free(ptr);
    
    return(0);
  }
    
  return(TC_IMPORT_ERROR);
}


