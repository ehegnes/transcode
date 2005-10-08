/*
 *  import_nuv.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *  NUV-code by Andreas Påhlsson
 *  bugfix by Christian Vogelgsang <Vogelgsang@informatik.uni-erlangen.de>
 *  more fixes by Tilmann Bitterberg <tilmann@bitterberg.de>  
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

#include "transcode.h"

#include "rtjpeg_vid_plugin.h"
#include "rtjpeg_aud_plugin.h"

#define MOD_NAME    "import_nuv.so"
#define MOD_VERSION "v0.1.2 (2002-08-01)"
#define MOD_CODEC   "(video) YUV | (audio) PCM"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_YUV|TC_CAP_PCM;

#define MOD_PRE nuv
#include "import_def.h"


#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static int yuv_size=0;
static int y_offset=0;
static int u_offset=0;
static int v_offset=0;
static int y_size=0;
static int u_size=0;
static int v_size=0;

static void* videobuf1 = NULL;
static void* videobuf2 = NULL;
static unsigned char* audiobuf1 = NULL;
static unsigned char* audiobuf2 = NULL;
static int audiolen1 = 0;
static int audiolen2 = 0;
static int timecode = 0;
static int audioframe = 0;
static int videoframe = 0;

/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  //  fprintf(stderr, "nuv: open\n");
  if(param->flag == TC_VIDEO) {
    //    fprintf(stderr, "nuv: video\n");
    if(rtjpeg_vid_file == 0) {
      rtjpeg_vid_open(vob->video_in_file);
      param->fd = NULL;
    }
    yuv_size = (rtjpeg_vid_video_width * rtjpeg_vid_video_height * 3) / 2;
    y_offset = 0;
    u_offset = rtjpeg_vid_video_width * rtjpeg_vid_video_height;
    v_offset = (rtjpeg_vid_video_width * rtjpeg_vid_video_height * 5) /4;
    u_size = v_size = (rtjpeg_vid_video_width * rtjpeg_vid_video_height) / 4;
    y_size = rtjpeg_vid_video_width * rtjpeg_vid_video_height;
    videoframe = 0;
    return 0;
  }
  
  if(param->flag == TC_AUDIO) {
    //    fprintf(stderr, "nuv: audio\n");
    if(rtjpeg_aud_file == 0) {
      rtjpeg_aud_open(vob->audio_in_file);
      param->fd = NULL;
    }
    audioframe = 0;
    rtjpeg_aud_resample = 1;
    return 0;
  }
  
  return(TC_IMPORT_ERROR);
  
}

/* ------------------------------------------------------------ 
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/

MOD_decode
{
  if(param->flag == TC_VIDEO) {
    //    fprintf(stderr, "nuv: video\n");

    if(rtjpeg_vid_end_of_video()) return(TC_IMPORT_ERROR);

    //fprintf(stderr,"vid: get frame %d\n",videoframe);
    videobuf1 = rtjpeg_vid_get_frame(videoframe, &timecode, 1, 
				    &audiobuf1, &audiolen1);

    if(videobuf1 == NULL) {
      // fprintf(stderr, "nuv: video buffer empty\n");
      return(TC_IMPORT_ERROR);
    }
    
    param->size = yuv_size; 
  

    // Do the shuffle... yuv => yvu

    ac_memcpy(param->buffer, videobuf1, y_size);
    ac_memcpy(param->buffer + v_offset, videobuf1 + u_offset, u_size);
    ac_memcpy(param->buffer + u_offset, videobuf1 + v_offset, v_size);

    videoframe++;

    return 0;
  }

  if(param->flag == TC_AUDIO) {
    //    fprintf(stderr, "nuv: audio\n");

    if(rtjpeg_aud_end_of_video()) return(TC_IMPORT_ERROR);
    
    //fprintf(stderr,"aud: get frame %d\n",audioframe);
    videobuf2 = rtjpeg_aud_get_frame(audioframe, &timecode, 0, 
				     &audiobuf2, &audiolen2);

    if(audiobuf2 == NULL) {
      // fprintf(stderr, "nuv: buffer buffer empty\n");
      return(TC_IMPORT_ERROR);
    }

    param->size = audiolen2; 
    ac_memcpy(param->buffer, audiobuf2, audiolen2);

    audioframe++;
    
    return 0;

  }

  param->size = 0;
  return TC_IMPORT_ERROR;
}

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{  

  if(param->flag == TC_AUDIO) {
    rtjpeg_aud_close();
    rtjpeg_aud_file=0;
    return(0);
  }
  
  if(param->flag == TC_VIDEO) {
    rtjpeg_vid_close();
    rtjpeg_vid_file=0;
    return(0);
  }
  
  return TC_IMPORT_ERROR;
}


