/*
 *  import_mpeg2.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
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

#include "transcode.h"


#define MOD_NAME    "import_mpeg2.so"
#define MOD_VERSION "v0.3.0 (2002-12-05)"
#define MOD_CODEC   "(video) MPEG2"

#define MOD_PRE mpeg2
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_RGB|TC_CAP_YUV;

/* ------------------------------------------------------------
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  
  if(param->flag != TC_VIDEO) return(TC_IMPORT_ERROR);
  
  if(vob->ts_pid1==0) {
    
    switch(vob->im_v_codec) {
      
    case CODEC_RGB:
      
      if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -x mpeg2 -i \"%s\" -d %d | tcdecode -x mpeg2 -d %d", vob->video_in_file, vob->verbose, vob->verbose)<0)) {
	perror("command buffer overflow");
	return(TC_IMPORT_ERROR);
      }
      break;
      
    case CODEC_YUV:
      
      if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -x mpeg2 -i \"%s\" -d %d | tcdecode -x mpeg2 -d %d -y yv12", vob->video_in_file, vob->verbose, vob->verbose)<0)) {
	perror("command buffer overflow");
	return(TC_IMPORT_ERROR);
      }
      break;
    }
  
  } else {
    
    switch(vob->im_v_codec) {
      
    case CODEC_RGB:
      
      if((snprintf(import_cmd_buf, MAX_BUF, "tccat -i \"%s\" -d %d -n 0x%x | tcextract -x mpeg2 -t m2v -d %d | tcdecode -x mpeg2 -d %d", vob->video_in_file, vob->verbose, vob->ts_pid1, vob->verbose, vob->verbose)<0)) {
	perror("command buffer overflow");
	return(TC_IMPORT_ERROR);
      }
      break;
      
    case CODEC_YUV:
      
      if((snprintf(import_cmd_buf, MAX_BUF, "tccat -i \"%s\" -d %d -n 0x%x | tcextract -x mpeg2 -t m2v -d %d | tcdecode -x mpeg2 -d %d -y yv12", vob->video_in_file, vob->verbose,vob->ts_pid1, vob->verbose, vob->verbose)<0)) {
	perror("command buffer overflow");
	return(TC_IMPORT_ERROR);
      }
      break;
    }
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


