/*
 *  import_raw.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  This file is part of transcode, a video stream  processing tool
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

#define MOD_NAME    "import_raw.so"
#define MOD_VERSION "v0.3.2 (2002-11-10)"
#define MOD_CODEC   "(video) RGB/YUV | (audio) PCM"

#include "transcode.h"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_RGB | TC_CAP_YUV | TC_CAP_PCM | TC_CAP_YUV422;

#define MOD_PRE raw
#include "import_def.h"

#include "ioaux.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];
static int codec;

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
      (scan(vob->audio_in_file)) ? tc_snprintf(cat_buf, sizeof(cat_buf), "tccat -a") : ((vob->im_a_string) ? tc_snprintf(cat_buf, sizeof(cat_buf), "tcextract -x pcm %s", vob->im_a_string) : tc_snprintf(cat_buf, sizeof(cat_buf), "tcextract -x pcm"));
      
      if(tc_snprintf(import_cmd_buf, MAX_BUF, "%s -i \"%s\" -d %d | tcextract -a %d -x pcm -d %d -t raw", cat_buf, vob->audio_in_file, vob->verbose, vob->a_track, vob->verbose) < 0) {
	perror("cmd buffer overflow");
	return(TC_IMPORT_ERROR);
      }
      
	// print out
	if(verbose_flag) tc_log_info(MOD_NAME, "%s", import_cmd_buf);
      
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
	tc_snprintf(cat_buf, sizeof(cat_buf), "tccat");
	co=""; 
      } else {
	
	(vob->im_v_string) ? tc_snprintf(cat_buf, sizeof(cat_buf), "tcextract %s", vob->im_v_string) : tc_snprintf(cat_buf, sizeof(cat_buf), "tcextract");
	
	switch (codec) {
	    case CODEC_RGB: co = "-x rgb"; break;
	    case CODEC_YUV422: co = "-x yuv422p"; break;
	    case CODEC_YUV: 
	    default: co = "-x yuv420p"; break;
	}
      }
      
      
      switch(codec) {
	
      case CODEC_RGB:
	
	if(tc_snprintf(import_cmd_buf, MAX_BUF, "%s -i \"%s\" -d %d %s | tcextract -a %d -x rgb -d %d", cat_buf, vob->video_in_file, vob->verbose, co, vob->v_track, vob->verbose) < 0) {
	  perror("cmd buffer overflow");
	  return(TC_IMPORT_ERROR);
	}
	
	break;
	
      case CODEC_YUV422:
	
	if(tc_snprintf(import_cmd_buf, MAX_BUF, "%s -i \"%s\" -d %d %s | tcextract -a %d -x yuv422p -d %d", cat_buf, vob->video_in_file, vob->verbose, co, vob->v_track, vob->verbose) < 0) {
	  perror("cmd buffer overflow");
	  return(TC_IMPORT_ERROR);
	}
	
	break;

      case CODEC_YUV:
      default:
	
	if(tc_snprintf(import_cmd_buf, MAX_BUF, "%s -i \"%s\" -d %d %s | tcextract -a %d -x yuv420p -d %d", cat_buf, vob->video_in_file, vob->verbose, co, vob->v_track, vob->verbose) < 0) {
	  perror("cmd buffer overflow");
	  return(TC_IMPORT_ERROR);
	}

        break;
	
      }
      
      // print out
      if(verbose_flag) tc_log_info(MOD_NAME, "%s", import_cmd_buf);
      
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



