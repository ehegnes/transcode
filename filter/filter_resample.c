/*
 *  filter_resample.c
 *
 *  Copyright (C) Thomas Östreich - February 2002
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

#define MOD_NAME    "filter_resample.so"
#define MOD_VERSION "v0.1.4 (2003-08-22)"
#define MOD_CAP     "audio resampling filter plugin"
#define MOD_AUTHOR  "Thomas Oestreich, Stefan Scheffler"

#include "transcode.h"
#include "filter.h"
#include "libtc/libtc.h"
#include "libtc/optstr.h"

#include <ffmpeg/avcodec.h>

static char * resample_buffer = NULL;
static int bytes_per_sample;
static int error;
static ReSampleContext *resamplecontext = NULL;
static int resample_buffer_size;

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/


int tc_filter(frame_list_t *ptr_, char *options)
{
  aframe_list_t *ptr = (aframe_list_t *)ptr_;
  vob_t *vob=NULL;


  //----------------------------------
  //
  // filter init
  //
  //----------------------------------

  if(ptr->tag & TC_FILTER_GET_CONFIG) {
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, "Thomas Oestreich", "AE", "1");
      return 0;
  }

  if(ptr->tag & TC_FILTER_INIT) {
    double samples_per_frame, ratio;
    if((vob = tc_get_vob())==NULL) return(-1);

    // filter init ok.

    if(verbose) tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);

    if(verbose) tc_log_info(MOD_NAME, "options=%s", options);

    bytes_per_sample = vob->a_chan * vob->a_bits/8;
    samples_per_frame = vob->a_rate/vob->ex_fps;
    ratio = (float)vob->mp3frequency/(float)vob->a_rate;

    resample_buffer_size = (int)(samples_per_frame * ratio) * bytes_per_sample + 16                            // frame + 16 bytes
                                + ((vob->a_leap_bytes > 0)?(int)(vob->a_leap_bytes * ratio) :0); // leap bytes .. kinda

                                resample_buffer = tc_malloc(resample_buffer_size * sizeof(char));
    if (!resample_buffer) {
        tc_log_error(MOD_NAME, "Buffer allocation failed");
        return 1;
    }

    if (verbose & TC_DEBUG) {
	tc_log_info(MOD_NAME,
		    "bufsize : %i, bytes : %i, bytesfreq/fps: %i, rest %i",
		    resample_buffer_size, bytes_per_sample,
		    vob->mp3frequency * bytes_per_sample / (int)vob->fps,
		    (vob->a_leap_bytes > 0 )?(int)(vob->a_leap_bytes * ratio):0);
    }

    if((int) (bytes_per_sample * vob->mp3frequency / vob->fps) > resample_buffer_size) return(1);

    if (!vob->a_rate || !vob->mp3frequency) {
	tc_log_error(MOD_NAME, "Invalid settings");
	error = 1;
	return -1;
    }
    if (vob->a_rate == vob->mp3frequency) {
	tc_log_error(MOD_NAME, "Frequencies are too similar, filter skipped");
	error=1;
	return -1;
    }
    resamplecontext = audio_resample_init(vob->a_chan, vob->a_chan, vob->mp3frequency, vob->a_rate);

    if (resamplecontext == NULL) return -1;
    //this will force this resample filter to do the job, not
    //the export module.

    vob->a_rate=vob->mp3frequency;
    vob->mp3frequency=0;
    vob->ex_a_size=resample_buffer_size;

    return(0);
  }

  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  if(ptr->tag & TC_FILTER_CLOSE) {

    if (!error) {
	  audio_resample_close(resamplecontext);
          free(resample_buffer);
    }
    return(0);
  }

  //----------------------------------
  //
  // filter frame routine
  //
  //----------------------------------

  // tag variable indicates, if we are called before
  // transcodes internal video/audo frame processing routines
  // or after and determines video/audio context

  if(ptr->tag & TC_PRE_S_PROCESS && ptr->tag & TC_AUDIO) {

    if (resample_buffer_size != 0) {

    if (verbose & TC_STATS) tc_log_info(MOD_NAME, "inbuf:%i, bufsize: %i", ptr->audio_size, resample_buffer_size);

    ptr->audio_size = bytes_per_sample * audio_resample(resamplecontext, (short *)resample_buffer, (short *)ptr->audio_buf, ptr->audio_size/bytes_per_sample);

    if (verbose & TC_STATS) tc_log_info(MOD_NAME, "outbuf: %i", ptr->audio_size);

    if(ptr->audio_size<0) ptr->audio_size=0;

    ac_memcpy(ptr->audio_buf, resample_buffer, ptr->audio_size);
    }
  }

  return(0);
}
