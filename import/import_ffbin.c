/*
 *  import_ffbin.c v 0.0.2
 *
 *  Copyright (C) Michael Ramendik - May 2004
 *  Used in Lutheran service (this is just developer information, not licensing)
 *  Based on import_mplayer_c, (C) Thomas Östreich - June 2001
 *
 *  VERSION HISTORY
 *  0.0.1    May 10 2004   First try, and it worked nearly at once! 
 *                         Thanks go to Thomas for such nice code for ripping <g>
 *  0.0.2    May 11 2004   Gave the video pipe a regular random name, like the audio pipe
 *
 *  This file made for transcode, a linux video stream  processing tool
 *      
 *  this file is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  this file is distributed in the hope that it will be useful,
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
#include <sys/types.h>
#include <sys/stat.h>

#include "transcode.h"


#define MOD_NAME    "import_ffbin.so"
#define MOD_VERSION "v0.0.2 (2004-05-11)"
#define MOD_CODEC   "(video) rendered by ffmpeg binary | (audio) rendered by ffmpeg binary"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_YUV|TC_CAP_RGB|TC_CAP_VID|TC_CAP_PCM;

#define MOD_PRE ffbin
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static char audiopipe[40] = "/tmp/ffbin2transcode-audio.XXXXXX";
static char videopipe[40] = "/tmp/ffbin2transcode-audio.XXXXXX";
static FILE *videopipefd = NULL;

/* ------------------------------------------------------------
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{

  /* check for ffmpeg */
  if (tc_test_program("ffmpeg") != 0) return (TC_EXPORT_ERROR);
        
  switch (param->flag) {
    case TC_VIDEO:
    
      rgbswap = !rgbswap; // needed!
      
      if (!(mktemp(videopipe) && videopipe)) {
        perror("mktemp could not create a unique file name for the intenal video pipe");
        return(TC_IMPORT_ERROR);
      }


      if (mkfifo(videopipe, 00660) == -1) {
        perror("mkfifo failed");
        return(TC_IMPORT_ERROR);
      }
      if (vob->im_v_string) {
		if (snprintf(import_cmd_buf, MAX_BUF, "ffmpeg -i %s \"%s\" -f yuv4mpegpipe -y %s >/dev/null 2>&1", vob->im_v_string, vob->video_in_file, videopipe ) < 0) {
		  perror("command buffer overflow");
		  exit(1);
		}
	  } else {
		if (snprintf(import_cmd_buf, MAX_BUF, "ffmpeg -i \"%s\" -f yuv4mpegpipe -y %s >/dev/null 2>&1", vob->video_in_file, videopipe ) < 0) {
		  perror("command buffer overflow");
		  exit(1);
		}
	  }
      if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

      if ((videopipefd = popen(import_cmd_buf, "w")) == NULL) {
        perror("ffmpeg binary could not be executed");
        exit(1);
      }
      
      if (vob->im_v_codec == CODEC_YUV) {
        if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -i %s -x yv12 -t yuv4mpeg", videopipe)<0)) {
          perror("command buffer overflow");
          return(TC_IMPORT_ERROR);
        }
      } else {
        if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -i %s -x yv12 -t yuv4mpeg | tcdecode -x yv12 -g %dx%d",
          videopipe, vob->im_v_width, vob->im_v_height)<0)) {
          perror("command buffer overflow");
          return(TC_IMPORT_ERROR);
        }
      }
      
      // print out
      if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);
      
      param->fd = NULL;

      // popen
      if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
        perror("popen YUV stream");
        return(TC_IMPORT_ERROR);
      }
      
      return 0;
      
    case TC_AUDIO:
      if (!(mktemp(audiopipe) && audiopipe)) {
        perror("mktemp could not create a unique file name for the audio pipe");
        return(TC_IMPORT_ERROR);
      }
      if (mkfifo(audiopipe, 00660) == -1) {
        perror("mkfifo failed");
        return(TC_IMPORT_ERROR);
      }
      
      if (snprintf(import_cmd_buf, MAX_BUF, "ffmpeg -i %s \"%s\" -f s16le -y %s >/dev/null 2>&1",
          ((vob->im_a_string)?vob->im_a_string:""), vob->audio_in_file, audiopipe) < 0) {
        perror("command buffer overflow");
        exit(1);
      }
      if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

      if ((videopipefd = popen(import_cmd_buf, "w")) == NULL) {
        perror("ffmpeg binary could not be executed");
        exit(1);
      }      
      
      if ((param->fd = fopen(audiopipe, "r")) == NULL) {
        perror("fopen audio stream");
        exit(1);
      }
    
      return 0;
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
  if (param->flag == TC_VIDEO) {
    if (param->fd != NULL)
      pclose(param->fd);
    if (videopipefd != NULL)
      pclose(videopipefd);
    if (videopipe && *videopipe)
      unlink(videopipe);
  } else {
    if (param->fd != NULL)
      fclose(param->fd);
    if (audiopipe && *audiopipe)
      unlink(audiopipe);
  }
  return(0);
}
