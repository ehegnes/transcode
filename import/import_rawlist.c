/*
 *  import_rawlist.c
 *
 *  Copyright (C) Thomas Östreich - July 2002
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
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "ioaux.h"

#include "transcode.h"

#define MOD_NAME    "import_rawlist.so"
#define MOD_VERSION "v0.0.1 (2002-07-30)"
#define MOD_CODEC   "(video) YUV/RGB raw frames"

#define MOD_PRE rawlist
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_RGB|TC_CAP_YUV|TC_CAP_AUD;

static FILE *fd; 
static char buffer[PATH_MAX+2];

static int bytes=0;
  
/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  if(param->flag == TC_AUDIO) {
      return(0);
  }
  
  if(param->flag == TC_VIDEO) {

    param->fd = NULL;

    if((fd = fopen(vob->video_in_file, "r"))==NULL) return(TC_IMPORT_ERROR);

    switch(vob->im_v_codec) {
      
    case CODEC_RGB:
      bytes=vob->im_v_width * vob->im_v_height * 3;
      break;
      
    case CODEC_YUV:
      bytes=(vob->im_v_width * vob->im_v_height * 3)/2;
      break;
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

MOD_decode {
  
  char
    *filename = NULL;
  
  int
    fd_in, n;
  
  if(param->flag == TC_AUDIO) return(0);
  
  // read a filename from the list
  if(fgets (buffer, PATH_MAX, fd)==NULL) return(TC_IMPORT_ERROR);    
  
  filename = buffer; 
  
  n=strlen(filename);
  if(n<2) return(TC_IMPORT_ERROR);  
  filename[n-1]='\0';
  
  //read the raw frame
  
  if((fd_in = open(filename, O_RDONLY))<0) {
    perror("open file");
    return(TC_IMPORT_ERROR);
  } 
  
  if(p_read(fd_in, param->buffer, bytes) != bytes) { 
    perror("image parameter mismatch");
    return(TC_IMPORT_ERROR);
  }
  
  close(fd_in);
  
  param->size=bytes;
  param->attributes |= TC_FRAME_IS_KEYFRAME;
  
  return(0);
}

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{  
  
  if(param->flag == TC_VIDEO) {
    
    if(fd != NULL) fclose(fd);
    if (param->fd != NULL) pclose(param->fd);
    
    return(0);
  }
  
  if(param->flag == TC_AUDIO) return(0);
  
  return(TC_IMPORT_ERROR);
}


