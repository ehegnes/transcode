/*
 *  import_yuv4mpeg.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
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

#define MOD_NAME    "import_yuv4mpeg.so"
#define MOD_VERSION "v0.2.4 (2002-01-20)"
#define MOD_CODEC   "(video) YUV4MPEGx | (audio) WAVE"

#include "transcode.h"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_RGB | TC_CAP_YUV | TC_CAP_PCM;

#define MOD_PRE yuv4mpeg
#include "import_def.h"


#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
	if(param->flag == TC_VIDEO)
	{
		switch(vob->im_v_codec)
		{
			case CODEC_RGB:
			{
				if(tc_snprintf(import_cmd_buf, MAX_BUF, "tccat -i \"%s\" | tcextract -x yuv420p -t yuv4mpeg | tcdecode -x yuv420p -g %dx%d", vob->video_in_file, vob->im_v_width, vob->im_v_height) < 0)
				{
					perror("cmd buffer overflow");
					return(TC_IMPORT_ERROR);
      			}

      			break;
			}
  
			case CODEC_YUV:
			{
				if(tc_snprintf(import_cmd_buf, MAX_BUF, "tccat -i \"%s\" | tcextract -x yuv420p -t yuv4mpeg", vob->video_in_file) < 0)
				{
					perror("cmd buffer overflow");
					return(TC_IMPORT_ERROR);
				}
	
      			break;
			}
		}
    
    // print out
    if(verbose_flag) tc_tag_info(MOD_NAME, "%s", import_cmd_buf);
    
    param->fd = NULL;
    
    // popen
    if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
      perror("popen RGB stream");
      return(TC_IMPORT_ERROR);
    }

    return(0);
  }
  
  if(param->flag == TC_AUDIO)
  {
    // need to check if audio and video file are identical, which is
    // not desired
      
      if(strcmp(vob->audio_in_file, vob->video_in_file) == 0) {
	  
      // user error, print warnig and exit
      tc_tag_warn(MOD_NAME, "audio/video files are identical");
      tc_tag_warn(MOD_NAME, "unable to read pcm data from yuv stream");
      tc_tag_warn(MOD_NAME, "use \"-x yuv4mpeg,null\" for dummy audio input");
      
      return(TC_IMPORT_ERROR);
    }
    
      
      if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcextract -x pcm -t wav -i \"%s\"", vob->audio_in_file) < 0) {
      perror("cmd buffer overflow");
      return(TC_IMPORT_ERROR);
    }
    
    // print out
    if(verbose_flag) tc_tag_info(MOD_NAME, "%s", import_cmd_buf);
    
    param->fd = NULL;
    
    // popen
    if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
      perror("popen PCM stream");
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


