/*  import_ffbin2.c v 0.0.1
 *
 *  Copyright (C) Michael Ramendik - May 2004
 *  Used in Christian service (this is just developer information, not licensing)
 *  Based on import_mplayer_c, (C) Thomas Östreich - June 2001
 *
 *  VERSION HISTORY
 *  0.0.1    May 11 2004   First try, based on 0.0.2 of import_ffbin,failed
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


#define MOD_NAME    "import_ffbin2.so"
#define MOD_VERSION "v0.0.1 (2004-05-11)"
#define MOD_CODEC   "(video) | (audio) rendered by ffmpeg binary together, use only for both!"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_YUV|TC_CAP_RGB|TC_CAP_VID|TC_CAP_PCM;

#define MOD_PRE mplayer
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static int ffbin_video_codec_loaded=0;
static int ffbin_audio_codec_loaded=0;
static int ffbin_app_started=0;

static char audiopipe[40] = "/tmp/ffbin2transcode-audio.XXXXXX";
static char videopipe[40] = "/tmp/ffbin2qinternal-video.XXXXXX";
static char videopipe_out[40] = "/tmp/ffbin2transcode-video.XXXXXX";
static FILE *videopipefd = NULL;
static FILE *videopipe_outfd = NULL;
static FILE *video_paramfd = NULL;
static FILE *audio_paramfd = NULL;



/* ------------------------------------------------------------
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  
  /* internal function - start the ffmpeg app
   * note: generates all pipes
   */
  int startapp() {
      if (!(mktemp(audiopipe) && audiopipe)) {
        perror("mktemp could not create a unique file name for the audio pipe");
        return(TC_IMPORT_ERROR);
      }
      if (mkfifo(audiopipe, 00660) == -1) {
        perror("mkfifo failed for audiopipe");
        return(TC_IMPORT_ERROR);
      }
      if (!(mktemp(videopipe) && videopipe)) {
        perror("mktemp could not create a unique file name for the intenal video pipe");
        return(TC_IMPORT_ERROR);
      }
      if (mkfifo(videopipe, 00660) == -1) {
        perror("mkfifo failed for videopipe");
        return(TC_IMPORT_ERROR);
      }

      if (snprintf(import_cmd_buf, MAX_BUF, "ffmpeg -i \"%s\" -f yuv4mpegpipe -y %s -f s16le -y %s >/dev/null 2>&1", 
        vob->video_in_file, videopipe, audiopipe ) < 0) {
	  perror("command buffer overflow");
	  exit(1);
      }

      // print out
      if(verbose_flag) fprintf(stderr,"[%s] %s\n", MOD_NAME, import_cmd_buf);

      if ((videopipefd = popen(import_cmd_buf, "w")) == NULL) {
        perror("ffmpeg binary could not be executed");
        exit(1);
      };
      
      //start tcextract/tcdecode

      //init "external" (final) video pipe
      if (!(mktemp(videopipe_out) && videopipe_out)) {
        perror("mktemp could not create a unique file name for the external video pipe");
        return(TC_IMPORT_ERROR);
      }
      if (mkfifo(videopipe_out, 00660) == -1) {
        perror("mkfifo failed");
        return(TC_IMPORT_ERROR);
      }
      
      //and feed it from the app, you know
      if (vob->im_v_codec == CODEC_YUV) {
        if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -i %s -x yv12 -t yuv4mpeg >%s", 
          videopipe, videopipe_out)<0)) {
          perror("command buffer overflow");
          return(TC_IMPORT_ERROR);
        }
      } else {
        if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -i %s -x yv12 -t yuv4mpeg | tcdecode -x yv12 -g %dx%d >%s",
          videopipe, vob->im_v_width, vob->im_v_height, videopipe_out)<0)) {
          perror("command buffer overflow");
          return(TC_IMPORT_ERROR);
        }
      }
      
      // print out
      if(verbose_flag) fprintf(stderr,"[%s] %s\n", MOD_NAME, import_cmd_buf);
      
      // popen
      if((videopipe_outfd = popen(import_cmd_buf, "w"))== NULL) {
        perror("popen for video stream failed");
        return(TC_IMPORT_ERROR);
      }

      // now open the external video pipe 
      if ((video_paramfd = fopen(videopipe_out, "r")) == NULL) {
        perror("fopen external video stream");
        exit(1);
      }

      // and now open the audio pipe as well
      if ((audio_paramfd = fopen(audiopipe, "r")) == NULL) {
        perror("fopen audio stream");
        exit(1);
      }

      ffbin_app_started=1;
      return 0;
  }

  switch (param->flag) {
    case TC_VIDEO:    
      rgbswap = !rgbswap; // needed!

      // start the app, unless started before
      if (!ffbin_app_started) {
        int rr=startapp();
        if (rr){
          perror("[ffbin2] error while starting import applications");
          exit(1);
        };
      };
      
      // if the audio part was not already loaded, show a warning

      if (!ffbin_audio_codec_loaded)    
        fprintf(stderr,"[ffbin2] WARNING. You must use ffbin2 for BOTH video and audio. Else transcode will hang");
      ffbin_video_codec_loaded=1;
      
      //assign the video pipe and return
      param->fd = video_paramfd;      
      return 0;
      
    case TC_AUDIO:

      // start the app, unless started before
      if (!ffbin_app_started) {
        int rr=startapp();
        if (rr){
          perror("[ffbin2] error while starting import applications");
          exit(1);
        };
      };

      if (!ffbin_video_codec_loaded)    
        fprintf(stderr,"[ffbin2] WARNING. You must use ffbin2 for BOTH video and audio. Else transcode will hang");
      ffbin_audio_codec_loaded=1;

      //assign the audio pipe and return
      param->fd = audio_paramfd;      

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
    if (videopipe_outfd != NULL)
      pclose(videopipe_outfd);
    if (videopipe && *videopipe)
      unlink(videopipe);
    if (videopipe_out && *videopipe_out)
      unlink(videopipe_out);
  } else {
    if (param->fd != NULL)
      fclose(param->fd);
    if (audiopipe && *audiopipe)
      unlink(audiopipe);
  }
  return(0);
}
