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
 *  This file made for transcode, a video stream  processing tool
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

#define MOD_NAME    "import_ffbin.so"
#define MOD_VERSION "v0.0.2 (2004-05-11)"
#define MOD_CODEC   "(video) rendered by ffmpeg binary | (audio) rendered by ffmpeg binary"

#include "transcode.h"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_YUV | TC_CAP_RGB | TC_CAP_VID | TC_CAP_PCM;

#define MOD_PRE ffbin
#include "import_def.h"


extern int errno;
char import_cmd_buf[TC_BUF_MAX];

static char audiopipe[40] = "/tmp/ffbin2transcode-audio.XXXXXX";
static char videopipe[40] = "/tmp/ffbin2transcode-audio.XXXXXX";
static FILE *videopipefd = NULL;
static FILE *audiopipefd = NULL;

/* ------------------------------------------------------------
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  long sret;

  /* check for ffmpeg */
  if (tc_test_program("ffmpeg") != 0) return (TC_EXPORT_ERROR);

  switch (param->flag) {
    case TC_VIDEO:

      rgbswap = !rgbswap; // needed!

      if (!(mktemp(videopipe) && videopipe)) {
        perror("mktemp videopipe failed");
        return(TC_IMPORT_ERROR);
      }
      if (mkfifo(videopipe, 00660) == -1) {
        perror("mkfifo videopipe failed");
        return(TC_IMPORT_ERROR);
      }

      sret = snprintf(import_cmd_buf, TC_BUF_MAX,
                 "ffmpeg %s -i \"%s\" -f yuv4mpegpipe -y %s >/dev/null 2>&1",
                 ((vob->im_v_string) ? vob->im_v_string : ""),
                 vob->video_in_file, videopipe);
      if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
        return(TC_IMPORT_ERROR);

      if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

      if ((videopipefd = popen(import_cmd_buf, "w")) == NULL) {
        perror("popen videopipe");
        return(TC_IMPORT_ERROR);
      }

      if (vob->im_v_codec == CODEC_YUV) {
        sret = snprintf(import_cmd_buf, TC_BUF_MAX,
               "tcextract -i %s -x yv12 -t yuv4mpeg", videopipe);
        if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
          return(TC_IMPORT_ERROR);
      } else {
        sret = snprintf(import_cmd_buf, TC_BUF_MAX,
               "tcextract -i %s -x yv12 -t yuv4mpeg |"
               " tcdecode -x yv12 -g %dx%d",
               videopipe, vob->im_v_width, vob->im_v_height);
        if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
          return(TC_IMPORT_ERROR);
      }

      // print out
      if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

      param->fd = NULL;

      // popen
      if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
        perror("popen YUV stream");
        return(TC_IMPORT_ERROR);
      }

      return(TC_IMPORT_OK);

    case TC_AUDIO:

      if (!(mktemp(audiopipe) && audiopipe)) {
        perror("mktemp audiopipe failed");
        return(TC_IMPORT_ERROR);
      }
      if (mkfifo(audiopipe, 00660) == -1) {
        perror("mkfifo audiopipe failed");
        return(TC_IMPORT_ERROR);
      }

      sret = snprintf(import_cmd_buf, TC_BUF_MAX,
                      "ffmpeg %s -i \"%s\" -f s16le -y %s >/dev/null 2>&1",
                      ((vob->im_a_string) ? vob->im_a_string : ""),
                      vob->audio_in_file, audiopipe);
      if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
        return(TC_IMPORT_ERROR);

      if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

      if ((audiopipefd = popen(import_cmd_buf, "w")) == NULL) {
        perror("popen audiopipe failed");
        return(TC_IMPORT_ERROR);
      }      

      if ((param->fd = fopen(audiopipe, "r")) == NULL) {
        perror("fopen audio stream");
        return(TC_IMPORT_ERROR);
      }

      return(TC_IMPORT_OK);
  }
  return(TC_IMPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/

MOD_decode { return(TC_IMPORT_OK); }

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{
  switch (param->flag) {
  case TC_VIDEO:
    if (param->fd != NULL)
      pclose(param->fd);
    if (videopipefd != NULL)
      pclose(videopipefd);
    if (videopipe && *videopipe)
      unlink(videopipe);
    break;
  case TC_AUDIO:
    if (param->fd != NULL)
      fclose(param->fd);
    if (audiopipefd != NULL)
      pclose(audiopipefd);
    if (audiopipe && *audiopipe)
      unlink(audiopipe);
    break;
  default:
    fprintf(stderr, "[%s] unsupported request (close ?)\n", MOD_NAME);
    return(TC_IMPORT_ERROR);
    break;
  }
  return(TC_IMPORT_OK);
}
