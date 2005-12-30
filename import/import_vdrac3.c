/******* NOTICE: this module is disabled *******/

/*
 *  import_vdrac3.c
 *
 *  Copyright (C) Thomas Östreich - January 2002
 *
 *  special module request by Dieter Bloms <dbloms@suse.de>
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

#define MOD_NAME    "import_vdrac3.so"
#define MOD_VERSION "v0.0.2 (2002-01-13)"
#define MOD_CODEC   "(audio) VDR-AC3"

#include "transcode.h"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_PCM | TC_CAP_AC3;

#define MOD_PRE vdrac3
#include "import_def.h"

#include "ac3scan.h"


#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static FILE *fd;

static int codec, pseudo_frame_size=0, frame_size=0, syncf=0;


/* ------------------------------------------------------------
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{

tc_log_error(MOD_NAME, "****************** NOTICE ******************");
tc_log_error(MOD_NAME, "This module is disabled, probably because it");
tc_log_error(MOD_NAME, "is considered obsolete or redundant.");
tc_log_error(MOD_NAME, "Try using a different module, such as ffmpeg or mplayer.");
tc_log_error(MOD_NAME, "If you still need this module, please");
tc_log_error(MOD_NAME, "contact the transcode-users mailing list.");
return TC_IMPORT_ERROR;

    // audio only
    if(param->flag != TC_AUDIO) return(TC_IMPORT_ERROR);

    codec = vob->im_a_codec;
    syncf = vob->sync;

    switch(codec) {

    case CODEC_AC3:

	// produce a clean sequence of AC3 frames
	if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcextract -t vdr -i \"%s\" -x ps1 -d %d | tcextract -t raw -x ac3 -d %d", vob->audio_in_file, vob->verbose, vob->verbose) < 0) {
	    perror("command buffer overflow");
	    return(TC_IMPORT_ERROR);
	}

	if(verbose_flag) tc_log_info(MOD_NAME, "AC3->AC3");

	break;

    case CODEC_PCM:

	if(vob->a_codec_flag==CODEC_AC3) {

	    if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcextract -t vdr -i \"%s\" -x ps1 -d %d | tcdecode -x ac3 -d %d -s %f,%f,%f -A %d", vob->audio_in_file, vob->verbose, vob->verbose, vob->ac3_gain[0], vob->ac3_gain[1], vob->ac3_gain[2], vob->a52_mode) < 0) {
		perror("command buffer overflow");
		return(TC_IMPORT_ERROR);
	    }

	    if(verbose_flag) tc_log_info(MOD_NAME, "AC3->PCM");
	}


	if(vob->a_codec_flag==CODEC_A52) {

	    if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcextract -t vdr -i \"%s\" -x ps1 -d %d | tcdecode -x a52 -d %d -A %d", vob->audio_in_file, vob->verbose, vob->verbose, vob->a52_mode) < 0) {
		perror("command buffer overflow");
		return(TC_IMPORT_ERROR);
	    }

	    if(verbose_flag) tc_log_info(MOD_NAME, "A52->PCM");
	}

	break;

    default:
	tc_log_warn(MOD_NAME, "invalid import codec request 0x%x");
	return(TC_IMPORT_ERROR);

    }

    // print out
    if(verbose_flag) tc_log_info(MOD_NAME, "%s", import_cmd_buf);

    // set to NULL if we handle read
    param->fd = NULL;

    // popen
    if((fd = popen(import_cmd_buf, "r"))== NULL) {
	perror("popen pcm stream");
	return(TC_IMPORT_ERROR);
    }

    return(0);
}

/* ------------------------------------------------------------
 *
 * decode stream
 *
 * ------------------------------------------------------------*/

MOD_decode
{

  int ac_bytes=0, ac_off=0;

  // audio only
  if(param->flag != TC_AUDIO) return(TC_IMPORT_ERROR);

  switch(codec) {

  case CODEC_AC3:

      // determine frame size at the very beginning of the stream

      if(pseudo_frame_size==0) {

	  if(ac3scan(fd, param->buffer, param->size, &ac_off, &ac_bytes, &pseudo_frame_size, &frame_size, verbose)!=0) return(TC_IMPORT_ERROR);

      } else {
	  ac_off = 0;
	  ac_bytes = pseudo_frame_size;
      }

      // return true pseudo_frame_size as physical size of audio data
      param->size = pseudo_frame_size;

      if(syncf>0) {
	  //dump an ac3 frame, instead of a pcm frame
	  ac_bytes = frame_size-ac_off;
	  param->size = frame_size;
	  --syncf;
      }

      break;

  case CODEC_PCM:

    //default:
    ac_off   = 0;
    ac_bytes = param->size;
    break;


  default:
      tc_log_warn(MOD_NAME, "invalid import codec request 0x%x", codec);
      return(TC_IMPORT_ERROR);

  }

  if (fread(param->buffer+ac_off, ac_bytes, 1, fd) !=1)
      return(TC_IMPORT_ERROR);


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

