/*
 *  import_avi.c
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
#include <unistd.h>
#include <string.h>
#include "avilib.h"
#include "transcode.h"
#include <xio.h>

#define MOD_NAME    "import_avi.so"
#define MOD_VERSION "v0.4.2 (2002-05-24)"
#define MOD_CODEC   "(video) * | (audio) *"

#define MOD_PRE avi
#include "import_def.h"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_AUD|TC_CAP_VID|TC_CAP_YUV422;

static avi_t *avifile1=NULL;
static avi_t *avifile2=NULL;

static int audio_codec;
static int aframe_count=0, vframe_count=0;
static int width=0, height=0;


/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  double fps=0;
  char *codec=NULL;
  long rate=0, bitrate=0;
  int format=0, chan=0, bits=0;
  struct stat fbuf;
  char import_cmd_buf[1024];

  param->fd = NULL;

  if(param->flag == TC_AUDIO) {
    
    param->fd = NULL;
    
    // Is the input file actually a directory - if so use
    // tccat to dump out the audio. N.B. This isn't going
    // to work if a particular track is needed
    if((xio_stat(vob->audio_in_file, &fbuf))==0 && S_ISDIR(fbuf.st_mode)) {
      sprintf(import_cmd_buf, "tccat -a -i \"%s\" -d %d", vob->video_in_file, vob->verbose);
      if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);
      if ((param->fd = popen(import_cmd_buf, "r"))== NULL) {
        return(TC_IMPORT_ERROR);
      }
      return(0);
    }

    // Otherwise proceed to open the file directly and decode here
    if(avifile1==NULL) {
      if(vob->nav_seek_file) {
	if(NULL == (avifile1 = AVI_open_input_indexfile(vob->audio_in_file,0,vob->nav_seek_file))){
	  AVI_print_error("avi open error");
	  return(TC_IMPORT_ERROR); 
	} 
      } else {
	if(NULL == (avifile1 = AVI_open_input_file(vob->audio_in_file,1))){
	  AVI_print_error("avi open error");
	  return(TC_IMPORT_ERROR); 
	} 
      }
    }   

    //set selected for multi-audio AVI-files
    AVI_set_audio_track(avifile1, vob->a_track);
    
    rate   =  AVI_audio_rate(avifile1);
    chan   =  AVI_audio_channels(avifile1);
    
    if(!chan) {
      fprintf(stderr, "error: no audio track found\n");
      return(TC_IMPORT_ERROR); 
    }
    
    bits   =  AVI_audio_bits(avifile1);
    bits   =  (!bits)?16:bits;
    
    format =  AVI_audio_format(avifile1);
    bitrate=  AVI_audio_mp3rate(avifile1);
    
    if (verbose_flag)
        fprintf(stderr, "[%s] format=0x%x, rate=%ld Hz, bits=%d, channels=%d, bitrate=%ld\n", MOD_NAME, format, rate, bits, chan, bitrate);
    
    if(vob->im_a_codec == CODEC_PCM && format != CODEC_PCM) {
      fprintf(stderr, "error: invalid AVI audio format '0x%x' for PCM processing\n", format);
      return(TC_IMPORT_ERROR);
    }
    // go to a specific byte for seeking
    AVI_set_audio_position(avifile1, vob->vob_offset*vob->im_a_size);

    audio_codec=vob->im_a_codec;
    return(0);
  }
  
  if(param->flag == TC_VIDEO) {
    
    
    param->fd = NULL;
    
    if(avifile2==NULL) {
      if(vob->nav_seek_file) {
	if(NULL == (avifile2 = AVI_open_input_indexfile(vob->video_in_file,0,vob->nav_seek_file))){
	  AVI_print_error("avi open error");
	  return(TC_IMPORT_ERROR); 
	} 
      } else {
	if(NULL == (avifile2 = AVI_open_input_file(vob->video_in_file,1))){
	  AVI_print_error("avi open error");
	  return(TC_IMPORT_ERROR); 
	} 
      }
    }

    if (vob->vob_offset>0)
	AVI_set_video_position(avifile2, vob->vob_offset);
    
    //read all video parameter from input file
    width  =  AVI_video_width(avifile2);
    height =  AVI_video_height(avifile2);
    
    fps    =  AVI_frame_rate(avifile2);
    codec  =  AVI_video_compressor(avifile2);
    
    
    fprintf(stderr, "[%s] codec=%s, fps=%6.3f, width=%d, height=%d\n", 
	    MOD_NAME, codec, fps, width, height);
    
    if(strlen(codec)!=0 && vob->im_v_codec == CODEC_RGB) {
      fprintf(stderr, "error: invalid AVI file codec '%s' for RGB processing\n", codec);
      return(TC_IMPORT_ERROR);
    }
    
    if(AVI_max_video_chunk(avifile2) > SIZE_RGB_FRAME){
      fprintf(stderr, "error: invalid AVI video frame chunk size detected\n");
      return(TC_IMPORT_ERROR);
    }
    
    if(strlen(codec)!=0 && vob->im_v_codec == CODEC_YUV && strcmp(codec, "YV12") != 0) {
      fprintf(stderr, "error: invalid AVI file codec '%s' for YV12 processing\n", codec);
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
    int i, mod=width%4;

    // If we are using tccat, then do nothing here
    if (param->fd != NULL) {
      return(0);
    }

    param->size = AVI_read_frame(avifile2, param->buffer, &key);

    // Fixup: For uncompressed AVIs, it must be aligned at
    // a 4-byte boundary
    if (mod && vob->im_v_codec == CODEC_RGB) {
	for (i = 0; i<height; i++) {
	    memmove (param->buffer+(i*width*3),
		     param->buffer+(i*width*3) + (mod)*i,
		     width*3);
	}
    }

    if(verbose & TC_STATS && key) printf("keyframe %d\n", vframe_count); 
    
    if(param->size<0) {
      if(verbose & TC_DEBUG) AVI_print_error("AVI read video frame");
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
      fprintf(stderr, "  XXX bytes_read = %ld|\n", bytes_read);

      if(bytes_read<0) {
	if(verbose & TC_DEBUG) AVI_print_error("AVI audio size frame");
	return(TC_IMPORT_ERROR);
      }
      
      if(AVI_read_audio(avifile1, param->buffer, bytes_read) < 0) {
	AVI_print_error("[import_avi] AVI audio read frame");
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
    
    if(param->flag == TC_AUDIO) {
	
	if(avifile1!=NULL) {
	    AVI_close(avifile1);
	    avifile1=NULL;
	}
	return(0);
    }
    
    if(param->flag == TC_VIDEO) {
	
	if(avifile2!=NULL) {
	    AVI_close(avifile2);
	    avifile2=NULL;
	}
	return(0);
    }
    
    return(TC_IMPORT_ERROR);
}


