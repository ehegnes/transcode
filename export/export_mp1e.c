/******* NOTICE: this module is disabled *******/

/*
 *  export_mp1e.c
 *
 *  Copyright (C) Tilmann Bitterberg, December 2003
 *
 *  This module provides in interface to the mp1e programme.
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

#include <stdio.h>
#include <stdlib.h>

#include "transcode.h"
#include "libtcvideo/tcvideo.h"
#include "wavlib.h"

#define MOD_NAME    "export_mp1e.so"
#define MOD_VERSION "v0.0.2 (2006-03-16)"
#define MOD_CODEC   "(video) MPEG1 video | (audio) MPEG1-Layer2"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM|TC_CAP_YUV|TC_CAP_YUV422|TC_CAP_RGB;

#define MOD_PRE mp1e
#include "export_def.h"
static char export_cmd_buf [PATH_MAX];
static FILE *pFile = NULL;
static struct wave_header rtf;
const char *fifoname = "audio-mp1e.wav";
static int audio_open_done = 0;
static int do_audio = 0;

static int v_codec = 0;
static int width = 0;
static int height = 0;
static TCVHandle tcvhandle = 0;
static ImageFormat srcfmt, destfmt;
static WAV wav = NULL;


// mp1e cant handle audio from a pipe, so we write a temporary WAV file.
// The define WRITE_AUDIO_IN_ADVANCE configures how many audio chunks are
// to be written in advance, otherwise mp1e (or better: libaudiofile) believes
// the WAV file is to short. We lie about the length of the file in the
// WAV header.

static int audio_frames_written =0;
#define WRITE_AUDIO_IN_ADVANCE 30

/* ------------------------------------------------------------
 *
 * open codec
 *
 * ------------------------------------------------------------*/


MOD_open
{
tc_log_error(MOD_NAME, "****************** NOTICE ******************");
tc_log_error(MOD_NAME, "This module is disabled, probably because it");
tc_log_error(MOD_NAME, "is considered obsolete or redundant.  Try");
tc_log_error(MOD_NAME, "using a different module, such as ffmpeg.");
tc_log_error(MOD_NAME, "If you still need this module, please");
tc_log_error(MOD_NAME, "contact the transcode-users mailing list.");
return TC_IMPORT_ERROR;

    /* check for mp1e program */
    if (tc_test_program("mp1e") != 0) return (TC_EXPORT_ERROR);

    if (do_audio && !audio_open_done) {
        WAVError err;
        int rate;

        wav = wav_open(fifoname, WAV_WRITE, &err);
        if (wav == NULL) {
	        tc_log_error (MOD_NAME, "open audio file: %s",
                                    wav_strerror(err));
	        return (TC_EXPORT_ERROR);
    	}
        rate = (vob->mp3frequency != 0) ?vob->mp3frequency :vob->a_rate;
        wav_set_rate(wav, rate);
        wav_set_bitrate(wav, vob->dm_chan * rate * vob->dm_bits/8);
        wav_set_channels(wav, vob->dm_chan);
        wav_set_bits(wav, vob->dm_bits);

	    audio_open_done++;
    }

    if (param->flag == TC_VIDEO) {

	char *yuv_str;
	char *motion_str;
	const char *p1, *p2;
	char *mux_buf = "-X 2";

	int clock, period, is_vcd = 0;

	// frame ratio
	switch (vob->ex_frc) {
	    case 1: // 23.976
		clock  = 24000;
		period = 1001;
		break;
	    case 2: // 24.000
		clock  = 24000;
		period = 1000;
		break;
	    case 3: // 25.000
		clock  = 25000;
		period = 1000;
		break;
	    case 4: // 29.970
		clock  = 30000;
		period = 1001;
		break;
	    case 5: // 30.000
		clock  = 30000;
		period = 1000;
		break;
	    case 0: // notset
	    default:
		clock = (int)vob->ex_fps*1000;
		period = 1000;
		break;
	}

	// Quality
	switch (vob->divxquality) {
	    case 4: motion_str = "4,8"; break;
	    case 3: motion_str = "4,16"; break;
	    case 2: motion_str = "8,24"; break;
	    case 1: motion_str = "8,48"; break;
	    case 0: motion_str = "8,64"; break;
	    default:
	    case 5:
		motion_str = "0,0";
		break;
	}

	width = vob->ex_v_width;
	height = vob->ex_v_height;

	// Colorspace format
	v_codec = vob->im_v_codec;
	if (v_codec == CODEC_YUV) {
	    yuv_str = "yuv420";
	    srcfmt = IMG_YUV_DEFAULT;
	    destfmt = IMG_YUV420P;
	} else if (v_codec == CODEC_YUV422) {
	    yuv_str = "yuyv";
	    srcfmt = IMG_YUV422P;
	    destfmt = IMG_YUY2;
	} else if (v_codec == CODEC_RGB) {
	    yuv_str = "yuv420";
	    srcfmt = IMG_RGB_DEFAULT;
	    destfmt = IMG_YUV420P;
	} else {
	    tc_log_warn (MOD_NAME, "invalid codec for this export module");
	    return (TC_EXPORT_ERROR);
	}
	if (!(tcvhandle = tcv_init())) {
	    tc_log_warn (MOD_NAME, "failed to init image format conversion");
	    return (TC_EXPORT_ERROR);
	}


	// VCD?
	p1 = vob->ex_v_fcc;
	p2 = vob->ex_a_fcc;

	// plain mpeg1 is the default

	if (p1 && strlen(p1)>0) {
	    if (strlen(p1)>2 && strncmp(p1, "vcd", 3)==0) {
		mux_buf = "-X 4"; is_vcd = 1; }
	    else if (strncmp(p1, "4", 1)==0) {
		mux_buf = "-X 4"; is_vcd = 1; }
	    else if (strlen(p1)>3 && strncmp(p1, "null", 3)==0) mux_buf = "-X 0";
	    else if (strlen(p1)>3 && strncmp(p1, "nirv", 3)==0) mux_buf = "-X 0";
	    else if (strncmp(p1, "0", 1)==0) mux_buf = "-X 0";
	}
	if (!p2) p2 = "";

	// set audio and video bitrate
	if (is_vcd) {

		vob->divxbitrate = 1152;
		vob->mp3bitrate = 224;
	}

	// Build commandline
	if (do_audio) {

	    tc_snprintf(export_cmd_buf, PATH_MAX,
		"mp1e %s -m 3 -b %d -R %s -B %d -c raw:%s-%d-%d-%d-%d -o \"%s\" -p %s %s %s",
		mux_buf,
		vob->divxbitrate,
		motion_str,
		vob->mp3bitrate,
		yuv_str,
		vob->ex_v_width,
		vob->ex_v_height,
		clock,
		period,
		vob->video_out_file,
		fifoname,
		p2,
		vob->ex_v_string?vob->ex_v_string:"");

	} else { // no audio

	    tc_snprintf(export_cmd_buf, PATH_MAX,
		"mp1e -m 1 -b %d -R %s -c raw:%s-%d-%d-%d-%d -o \"%s\" %s %s" ,
		vob->divxbitrate,
		motion_str,
		yuv_str,
		vob->ex_v_width,
		vob->ex_v_height,
		(int)vob->ex_fps*1000,
		1000,
		vob->video_out_file,
		p2,
		vob->ex_v_string?vob->ex_v_string:"");
	}

	if (verbose > 0)
	    tc_log_info(MOD_NAME, "%s", export_cmd_buf);

    }
  return(0);
}

/* ------------------------------------------------------------
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{
  if(param->flag == TC_AUDIO) {
        do_audio = 1;
  }

  return(0);
}


/* ------------------------------------------------------------
 *
 * encode and export
 *
 * ------------------------------------------------------------*/

MOD_encode
{
    if (do_audio
	    && audio_frames_written<WRITE_AUDIO_IN_ADVANCE
	    && param->flag == TC_VIDEO)  {
	param->attributes |= TC_FRAME_IS_CLONED;
	return 0;
    }
    if (param->flag == TC_VIDEO)  {
	vob_t *vob = tc_get_vob();

	//
	// If we open the pipe at _open time, it does not work
	// reliable because mp1e may start reading the audio from
	// empty wav file and will bail out.
	//

	if (!pFile) {
	    // finally open the pipe
	    pFile = popen(export_cmd_buf, "w");
	    if (!pFile) {
		perror ("popen mp1e command");
		return (TC_EXPORT_ERROR);
	    }
	}

	if (!tcv_convert(tcvhandle, param->buffer, vob->ex_v_width,
			 vob->ex_v_height, srcfmt, destfmt)) {
	    tc_log_warn(MOD_NAME, "image format conversion failed");
	    return(TC_EXPORT_ERROR);
	}

	if (v_codec == CODEC_YUV422) {
	    fwrite(param->buffer, width*height*2, 1, pFile);
	} else {
	    fwrite(param->buffer, width*height*3/2, 1, pFile);
	}
    }
    if (param->flag == TC_AUDIO)  {
        wav_write_data(wav, param->buffer, param->size);
        audio_frames_written++;
    }
  return(0);
}


/* ------------------------------------------------------------
 *
 * stop codec
 *
 * ------------------------------------------------------------*/

MOD_stop
{
  return(0);
}


/* ------------------------------------------------------------
 *
 * close codec
 *
 * ------------------------------------------------------------*/

MOD_close
{
  if (pFile) pclose(pFile); pFile=NULL;

  if (wav) {
      wav_close(wav);
      unlink(fifoname);
      wav=NULL;
  }
  audio_open_done = 0;
  do_audio = 0;

  tcv_free(tcvhandle);
  tcvhandle = 0;

  return(0);
}


