/*
 *  import_ogg.c
 *
 *  Copyright (C) Thomas �streich - July 2002
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

#define MOD_NAME    "import_ogg.so"
#define MOD_VERSION "v0.0.2 (2003-08-21)"
#define MOD_CODEC   "(video) all | (audio) Ogg Vorbis"

#include "transcode.h"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_RGB | TC_CAP_YUV | TC_CAP_AUD | TC_CAP_PCM | TC_CAP_VID;

#define MOD_PRE ogg
#include "import_def.h"

#include "magic.h"


#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static FILE *fd;

/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{

    param->fd = NULL;
    
    if(param->flag == TC_VIDEO) {

	char *codec;
	char *color;
	char *magic;

	switch (vob->im_v_codec) {

	    case CODEC_RGB:
		color = "rgb";
		break;

	    case CODEC_YUV:
		color = "yuv420p";
		break;

	    default:
		color = "";
		break;

	}

	// add more codecs: dv, mjpeg, ..
	//fprintf(stderr, "CODEC_FLAG = |%lx|\n", vob->codec_flag);
	switch (vob->codec_flag) {

	    case TC_CODEC_DIVX5:
	    case TC_CODEC_DIVX4:
	    case TC_CODEC_DIVX3:
	    case TC_CODEC_XVID:
		codec = "divx4";
		magic = "-t lavc";
		break;

	    case TC_CODEC_DV:
		codec = "dv";
		magic = "";
		break;

	    case TC_CODEC_RGB:
	    case TC_CODEC_YUV420P:
	    default:
		codec = "raw";
		magic = "";
		break;

	}

	if(tc_snprintf(import_cmd_buf, MAX_BUF, 
			"tcextract -i \"%s\" -x raw -d %d | "
			"tcdecode %s -g %dx%d -x %s -y %s -d %d",
			vob->video_in_file, vob->verbose, 
			magic, vob->im_v_width, vob->im_v_height, codec, color, vob->verbose) < 0) {
	    perror("command buffer overflow");
	    return(TC_IMPORT_ERROR);
	}
	// print out
	if(verbose_flag) tc_tag_info(MOD_NAME, "%s", import_cmd_buf);

	if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
	    perror("popen video stream");
	    return(TC_IMPORT_ERROR);
	}


	return(0);
    }
    if(param->flag == TC_AUDIO) {

	char *codec="";

	switch (vob->fixme_a_codec) {

	    case CODEC_MP3:
	    case CODEC_MP2:
		codec = "mp3";
		break;

	    case CODEC_VORBIS:
		codec = "ogg";
		break;

	    case CODEC_PCM:
		codec = "pcm";
		break;

	    default:
		tc_tag_warn(MOD_NAME, "Unkown codec");
		break;
	}
    
	if(tc_snprintf(import_cmd_buf, MAX_BUF, 
			"tcextract -i \"%s\" -x %s -a %d -d %d | tcdecode -x %s -d %d",
			vob->audio_in_file, codec, vob->a_track, vob->verbose, 
			codec, vob->verbose) < 0) {
	    perror("command buffer overflow");
	    return(TC_IMPORT_ERROR);
	}

	if (vob->fixme_a_codec == CODEC_PCM) {
	    if(tc_snprintf(import_cmd_buf, MAX_BUF, 
			"tcextract -i \"%s\" -x %s -a %d -d %d",
			vob->audio_in_file, codec, vob->a_track,
			vob->verbose) < 0) {
		perror("command buffer overflow");
		return(TC_IMPORT_ERROR);
	    }
	}

	// print out
	if(verbose_flag) tc_tag_info(MOD_NAME, "%s", import_cmd_buf);

	// popen
	if((fd = popen(import_cmd_buf, "r"))== NULL) {
	    perror("popen pcm stream");
	    return(TC_IMPORT_ERROR);
	}

	//caller handles read
	param->fd = fd;

	return(0);
    }
    return(TC_IMPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * decode stream
 *
 * ------------------------------------------------------------*/

MOD_decode
{
  //nothing to do
  return(0);
}

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

