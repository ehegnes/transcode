/*
 *  import_dv.c
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

#include "transcode.h"

#define MOD_NAME    "import_dv.so"
#define MOD_VERSION "v0.2.6 (2001-11-08)"
#define MOD_CODEC   "(video) DV | (audio) PCM"

#define MOD_PRE dv
#include "import_def.h"


#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_RGB|TC_CAP_YUV|TC_CAP_DV|TC_CAP_PCM;

static int frame_size=0;
static FILE *fd=NULL;

int scan(char *name) 
{
  struct stat fbuf;
  
  if(stat(name, &fbuf)) {
    fprintf(stderr, "(%s) invalid file \"%s\"\n", __FILE__, name);
    exit(1);
  }
  
  // file or directory?
  
  if(S_ISDIR(fbuf.st_mode)) return(1);
  return(0);
}

/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{

  char cat_buf[1024];

  if(param->flag == TC_VIDEO) {

    //directory mode?
    (scan(vob->video_in_file)) ? sprintf(cat_buf, "tccat") : sprintf(cat_buf, "tcextract -x dv");
    
    param->fd = NULL;

    switch(vob->im_v_codec) {
      
    case CODEC_RGB:
      
      if((snprintf(import_cmd_buf, MAX_BUF, "%s -i \"%s\" -d %d | tcdecode -x dv -y rgb -d %d -Q %d", cat_buf, vob->video_in_file, vob->verbose, vob->verbose, vob->quality)<0)) {
	perror("command buffer overflow");
	return(TC_IMPORT_ERROR);
      }
      
      // popen
      if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
	return(TC_IMPORT_ERROR);
      }
      
      break;
      
    case CODEC_YUV:
      
      if((snprintf(import_cmd_buf, MAX_BUF, "%s -i \"%s\" -d %d | tcdecode -x dv -y yv12 -d %d -Q %d", cat_buf, vob->video_in_file, vob->verbose, vob->verbose, vob->quality)<0)) {
	perror("command buffer overflow");
	return(TC_IMPORT_ERROR);
      }

      // for reading
      frame_size = (vob->im_v_width * vob->im_v_height * 3)/2;

      param->fd = NULL;
      
      // popen
      if((fd = popen(import_cmd_buf, "r"))== NULL) {
	return(TC_IMPORT_ERROR);
      }
      
      break;


    case CODEC_RAW:
      
      if((snprintf(import_cmd_buf, MAX_BUF, "%s -i \"%s\" -d %d", cat_buf, vob->video_in_file, vob->verbose)<0)) {
	perror("command buffer overflow");
	return(TC_IMPORT_ERROR);
      }

      // for reading
      frame_size = (vob->im_v_height==PAL_H) ? TC_FRAME_DV_PAL:TC_FRAME_DV_NTSC;

      param->fd = NULL;
      
      // popen
      if((fd = popen(import_cmd_buf, "r"))== NULL) {
	return(TC_IMPORT_ERROR);
      }
      
      break;

      
    default: 
      fprintf(stderr, "invalid import codec request 0x%x\n", vob->im_v_codec);
      return(TC_IMPORT_ERROR);
      
    }

    // print out
    if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);
    
    return(0);
  }
  
  if(param->flag == TC_AUDIO) {

    //directory mode?
    (scan(vob->audio_in_file)) ? sprintf(cat_buf, "tccat") : sprintf(cat_buf, "tcextract -x dv");
    
    if((snprintf(import_cmd_buf, MAX_BUF, "%s -i \"%s\" -d %d | tcdecode -x dv -y pcm -d %d", cat_buf, vob->audio_in_file, vob->verbose, vob->verbose)<0)) {
      perror("command buffer overflow");
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

MOD_decode
{

    if(param->flag == TC_AUDIO) return(0);
    
    // video and YUV only
    if(param->flag == TC_VIDEO && frame_size==0) return(TC_IMPORT_ERROR);
    
    // return true yuv frame size as physical size of video data
    param->size = frame_size; 
    
    if (fread(param->buffer, frame_size, 1, fd) !=1) 
	return(TC_IMPORT_ERROR);
    
    return(0);
}

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


