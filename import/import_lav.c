/*
 *  import_lav.c
 *
 *  Copyright (C) German Gomez Garcia <german@piraos.com> - December 2001
 *
 *  This file is part of transcode, a video stream processing tool
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

#define MOD_NAME    "import_lav.so"
#define MOD_VERSION "v0.0.2 (2002-01-18)"
#define MOD_CODEC   "(video) LAV | (audio) WAVE"

#include <errno.h>

#include "transcode.h"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_RGB | TC_CAP_YUV | TC_CAP_PCM;

#define MOD_PRE lav
#include "import_def.h"


static char import_cmd_buf[TC_BUF_MAX];


/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  int i;
  long sret;

  i = strlen(vob->video_in_file);
  if(vob->video_in_file[i-1] == '/')
      i = 1;
  else
      i = 0;

  if(param->flag == TC_VIDEO) {

    /* check for lav2yuv */
    if (tc_test_program("lav2yuv") != 0) return (TC_EXPORT_ERROR);

    switch(vob->im_v_codec) {

    case CODEC_RGB:

      sret = snprintf(import_cmd_buf, TC_BUF_MAX,
                      "lav2yuv \"%s\"%s |"
                      " tcextract -x yv12 -t yuv4mpeg |"
                      " tcdecode -x yv12 -g %dx%d",
                      vob->video_in_file, i ? "*" : "",
                      vob->im_v_width, vob->im_v_height);
      if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
	return(TC_IMPORT_ERROR);

      break;

    case CODEC_YUV:

      sret = snprintf(import_cmd_buf, TC_BUF_MAX,
                      "lav2yuv \"%s\"%s |"
                      " tcextract -x yv12 -t yuv4mpeg",
                      vob->video_in_file, i ? "*" : "");
      if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
	return(TC_IMPORT_ERROR);

      break;

    default:
      break;
    }

    // print out
    if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

    param->fd = NULL;

    // popen
    if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
      perror("popen RGB stream");
      return(TC_IMPORT_ERROR);
    }

    return(TC_IMPORT_OK);
  }

  if(param->flag == TC_AUDIO) {

    /* check for lav2wav */
    if (tc_test_program("lav2wav") != 0) return (TC_EXPORT_ERROR);

    sret = snprintf(import_cmd_buf, TC_BUF_MAX,
                    "lav2wav \"%s\"%s | tcextract -x pcm -t wav ",
                    vob->audio_in_file, i ? "*" : "");
    if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
      return(TC_IMPORT_ERROR);

    // print out
    if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

    param->fd = NULL;

    // popen
    if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
      perror("popen PCM stream");
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

MOD_decode{ return(TC_IMPORT_OK); }

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{  

  if(param->fd != NULL) pclose(param->fd);

  return(TC_IMPORT_OK);
}
