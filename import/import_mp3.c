/*
 *  import_mp3.c
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
#include <string.h>

#include "transcode.h"

#define MOD_NAME    "import_mp3.so"
#define MOD_VERSION "v0.1.2 (2003-03-27)"
#define MOD_CODEC   "(audio) MPEG"

#define MOD_PRE mp3
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static FILE *fd;

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM;

static int codec;

static int count=TC_PAD_AUD_FRAMES;


/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{

    // audio only
    if(param->flag != TC_AUDIO) return(TC_IMPORT_ERROR);
    
    codec = vob->im_a_codec;
    count = 0;
    
    switch(codec) {
	
    case CODEC_PCM:
	
	if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -a %d -i \"%s\" -x mp3 -d %d | tcdecode -x mp3 -d %d", vob->a_track, vob->audio_in_file, vob->verbose, vob->verbose)<0)) {
	    perror("command buffer overflow");
	    return(TC_IMPORT_ERROR);
	}
	
	if(verbose_flag) printf("[%s] MP3->PCM\n", MOD_NAME);
	
	break;
	
    default: 
	fprintf(stderr, "invalid import codec request 0x%x\n", codec);
	return(TC_IMPORT_ERROR);
	
    }
    
    // print out
    if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);
    
    // set to NULL if we handle read
    param->fd = NULL;
    
    // popen
    if((fd = popen(import_cmd_buf, "r"))== NULL) {
	perror("popen pcm stream");
	return(TC_IMPORT_ERROR);
    }
    
    return(0);
}

/* ------------------------------------------------------------ 
 *
 * decode stream
 *
 * ------------------------------------------------------------*/

MOD_decode
{
    
  int ac_bytes=0, ac_off=0; 

  // audio only
  if(param->flag != TC_AUDIO) return(TC_IMPORT_ERROR);
  
  switch(codec) {
      
  case CODEC_PCM:
    
    //default:
    ac_off   = 0;
    ac_bytes = param->size;
    break;
    
    
  default: 
    fprintf(stderr, "invalid import codec request 0x%x\n",codec);
      return(TC_IMPORT_ERROR);
      
  }
  
  memset(param->buffer+ac_off, 0, ac_bytes);

  if (fread(param->buffer+ac_off, ac_bytes, 1, fd) !=1) {
    // not sure what this hack is for, probably can be removed
    //--count; if(count<=0) return(TC_IMPORT_ERROR);
    return(TC_IMPORT_ERROR);
  }
  
  return(0);
}

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{  
  
  if(param->flag != TC_AUDIO) return(TC_IMPORT_ERROR);

  
  if(fd != NULL) pclose(fd);
  if(param->fd != NULL) pclose(param->fd);

  fd        = NULL;
  param->fd = NULL;
 
  
  return(0);
}

