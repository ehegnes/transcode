/*
 *  import_raw.c
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

#define MOD_NAME    "import_raw.so"
#define MOD_VERSION "v0.3.1 (2001-11-09)"
#define MOD_CODEC   "(video) RGB/YUV | (audio) PCM"

#define MOD_PRE raw
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_RGB|TC_CAP_YUV|TC_CAP_PCM;
static int codec;

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
    char *co;

    if(param->flag == TC_AUDIO) {
    
	//directory mode?
	(scan(vob->audio_in_file)) ? sprintf(cat_buf, "tccat -a") : sprintf(cat_buf, "tcextract -x pcm");
	
	if((snprintf(import_cmd_buf, MAX_BUF, "%s -i \"%s\" -d %d | tcextract -a %d -x pcm -d %d", cat_buf, vob->audio_in_file, vob->verbose, vob->a_track, vob->verbose)<0)) {
	    perror("cmd buffer overflow");
	    return(TC_IMPORT_ERROR);
	}
      
	// print out
	if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);
      
      param->fd = NULL;
      
      // popen
      if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
	perror("popen audio stream");
	return(TC_IMPORT_ERROR);
      }
      
      return(0);
    }
    
    if(param->flag == TC_VIDEO) {

	codec=vob->im_v_codec;

	//directory mode?
	if(scan(vob->video_in_file)) {
	    sprintf(cat_buf, "tccat");
	    co=""; 
	} else {
	    sprintf(cat_buf, "tcextract");
	    
	    co=(codec==CODEC_RGB)? "-x rgb":"-x yv12";
	}
	
	
      switch(codec) {
    
      case CODEC_RGB:
	  
	  if((snprintf(import_cmd_buf, MAX_BUF, "%s -i \"%s\" -d %d %s | tcextract -a %d -x rgb -d %d", cat_buf, vob->video_in_file, vob->verbose, co, vob->v_track, vob->verbose)<0)) {
	      perror("cmd buffer overflow");
	      return(TC_IMPORT_ERROR);
	  }
	
	break;
      
      case CODEC_YUV:
	
	if((snprintf(import_cmd_buf, MAX_BUF, "%s -i \"%s\" -d %d %s | tcextract -a %d -x yv12 -d %d", cat_buf, vob->video_in_file, vob->verbose, co, vob->v_track, vob->verbose)<0)) {
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
	perror("popen video stream");
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

MOD_decode {return(0);}


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


