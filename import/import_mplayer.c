/*
 *  import_mplayer.c
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
#include <sys/types.h>
#include <sys/stat.h>

#include "transcode.h"


#define MOD_NAME    "import_mplayer.so"
#define MOD_VERSION "v0.0.5 (2003-03-10)"
#define MOD_CODEC   "(video) rendered by mplayer | (audio) rendered by mplayer"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_YUV|TC_CAP_RGB|TC_CAP_VID|TC_CAP_PCM;

#define MOD_PRE mplayer
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static char audiopipe[40] = "/tmp/mplayer2transcode-audio.XXXXXX";
static FILE *videopipefd = NULL;

/* ------------------------------------------------------------
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  switch (param->flag) {
    case TC_VIDEO:
    
      rgbswap = !rgbswap; // needed!
      
      if (mkfifo("stream.yuv", 00660) == -1) {
        perror("mkfifo(\"stream.yuv\") failed");
        return(TC_IMPORT_ERROR);
      }
      if (vob->im_v_string) {
		if (snprintf(import_cmd_buf, MAX_BUF, "mplayer -benchmark -noframedrop -nosound -vo yuv4mpeg %s \"%s\" -osdlevel 0 > /dev/null 2>&1", vob->im_v_string, vob->video_in_file ) < 0) {
		  perror("command buffer overflow");
		  exit(1);
		}
	  } else {
		if (snprintf(import_cmd_buf, MAX_BUF, "mplayer -benchmark -noframedrop -nosound -vo yuv4mpeg \"%s\" -osdlevel 0 > /dev/null 2>&1", vob->video_in_file ) < 0) {
		  perror("command buffer overflow");
		  exit(1);
		}
	  }
      if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

      if ((videopipefd = popen(import_cmd_buf, "w")) == NULL) {
        perror("mplayer could not be executed");
        exit(1);
      }
      
      if (vob->im_v_codec == CODEC_YUV) {
        if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -i stream.yuv -x yv12 -t yuv4mpeg")<0)) {
          perror("command buffer overflow");
          return(TC_IMPORT_ERROR);
        }
      } else {
        if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -i stream.yuv -x yv12 -t yuv4mpeg | tcdecode -x yv12 -g %dx%d",
          vob->im_v_width, vob->im_v_height)<0)) {
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
      
      if (snprintf(import_cmd_buf, MAX_BUF, "mplayer -hardframedrop -vo null -ao pcm -nowaveheader -aofile %s %s \"%s\" > /dev/null 2>&1",
          audiopipe, ((vob->im_a_string)?vob->im_a_string:""), vob->audio_in_file) < 0) {
        perror("command buffer overflow");
        exit(1);
      }
      if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

      if ((videopipefd = popen(import_cmd_buf, "w")) == NULL) {
        perror("mplayer could not be executed");
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
    unlink("stream.yuv");
  } else {
    if (param->fd != NULL)
      fclose(param->fd);
    if (audiopipe && *audiopipe)
      unlink(audiopipe);
  }
  return(0);
}
