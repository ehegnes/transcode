/*
 *  import_lav.c
 *
 *  Copyright (C) German Gomez Garcia <german@piraos.com> - December 2001
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

#define MOD_NAME    "import_lav.so"
#define MOD_VERSION "v0.0.2 (2002-01-18)"
#define MOD_CODEC   "(video) LAV | (audio) WAVE"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_RGB|TC_CAP_YUV|TC_CAP_PCM;

#define MOD_PRE lav
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  int i;
  i = strlen(vob->video_in_file);
  if(vob->video_in_file[i-1] == '/')
      i = 1;
  else
      i = 0;
  
  if(param->flag == TC_VIDEO) {
    
    switch(vob->im_v_codec) {
      
    case CODEC_RGB:
      
      if((snprintf(import_cmd_buf, MAX_BUF, "lav2yuv \"%s\"%s | tcextract -x yv12 -t yuv4mpeg | tcdecode -x yv12 -g %dx%d", vob->video_in_file, i ? "*" : "", vob->im_v_width, vob->im_v_height)<0)) {
	perror("cmd buffer overflow");
	return(TC_IMPORT_ERROR);
      }
      
      break;
  
    case CODEC_YUV:
      
      if((snprintf(import_cmd_buf, MAX_BUF, "lav2yuv \"%s\"%s | tcextract -x yv12 -t yuv4mpeg", vob->video_in_file, i ? "*" : "")<0)) {
	perror("cmd buffer overflow");
	return(TC_IMPORT_ERROR);
      }
      
      break;
    }
    
    // print out
    if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);
    
    param->fd = NULL;
    
    // popen
    if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
      perror("popen RGB stream");
      return(TC_IMPORT_ERROR);
    }

    return(0);
  }
  
  if(param->flag == TC_AUDIO) {
    if((snprintf(import_cmd_buf, MAX_BUF, "lav2wav \"%s\"%s | tcextract -x pcm -t wav ", vob->audio_in_file, i ? "*" : "")<0)) {
      perror("cmd buffer overflow");
      return(TC_IMPORT_ERROR);
    }
    
    // print out
    if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);
    
    param->fd = NULL;
    
    // popen
    if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
      perror("popen PCM stream");
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

MOD_decode{return(0);}

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{  

  if(param->fd != NULL) pclose(param->fd);

  return(0);
}


