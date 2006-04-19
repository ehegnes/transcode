/*
 *  import_v4l.c
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

#include "transcode.h"
#include "vcr.h"

#define MOD_NAME    "import_v4l.so"
#define MOD_VERSION "v0.0.5 (2003-06-11)"
#define MOD_CODEC   "(video) v4l | (audio) PCM"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_RGB|TC_CAP_YUV|TC_CAP_PCM;

#define MOD_PRE v4l
#include "import_def.h"

#define MAX_DISPLAY_PTS 25

//static char *default_audio="/dev/dsp";
//static char *default_video="/dev/video";

static uint32_t aframe_cnt=0;
static uint32_t vframe_cnt=0;

static double aframe_pts0=0, aframe_pts=0;
static double vframe_pts0=0, vframe_pts=0;

static int audio_drop_frames=25;
static int video_drop_frames=0;
static int do_audio = 1;

static int do_resync = 1;

/* ------------------------------------------------------------
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
    int fmt = VIDEO_PALETTE_YUV420P;


  if(param->flag == TC_VIDEO) {

    // print out
    if(verbose_flag) tc_log_info(MOD_NAME, "video4linux video grabbing");

    param->fd = NULL;

    //set channel_id with vob->v_track
    //set channel with vob->station or vob->station_id
    //set device with vob->video_in_file

    //tc_log_msg(MOD_NAME, "vob->amod_probed (%s)", vob->amod_probed);

    /* This check is bogus since amod->probed does not contain what the user
     * specified with -x

    if ((vob->amod_probed && strlen(vob->amod_probed)>=4 && !strncmp(vob->amod_probed, "null", 4))
	    || !vob->amod_probed) {
	tc_log_msg(MOD_NAME, "NO AUDIO");
	do_audio = 0;
    }
    */

    if ((vob->video_in_file && strlen(vob->video_in_file)>=11 && strncmp(vob->video_in_file, "/dev/video1", 11))) do_resync=0; //no resync stuff for webcams

    switch(vob->im_v_codec) {

    case CODEC_RGB:

      if(video_grab_init(vob->video_in_file, vob->chanid, vob->station_id, vob->im_v_width, vob->im_v_height, VIDEO_PALETTE_RGB24, verbose_flag, do_audio)<0) {
	tc_log_error(MOD_NAME, "error grab init");
	return(TC_IMPORT_ERROR);
      }

      break;

    case CODEC_YUV:

      if (vob->im_v_string && strlen (vob->im_v_string)>0) {
	  if ( (strcmp (vob->im_v_string, "yuv422")) == 0)
	      fmt = VIDEO_PALETTE_YUV422;
      }

      if(video_grab_init(vob->video_in_file, vob->chanid, vob->station_id, vob->im_v_width, vob->im_v_height, fmt, verbose_flag, do_audio)<0) {
	tc_log_error(MOD_NAME, "error grab init");
	return(TC_IMPORT_ERROR);
      }

      break;
    }

    vframe_pts0 =  vframe_pts = v4l_counter_init();
    if (do_audio)
	video_drop_frames = audio_drop_frames - (int) ((vframe_pts0-aframe_pts0)*vob->fps);
    if(verbose_flag) tc_log_info(MOD_NAME, "dropping %d video frames for AV sync ", video_drop_frames);

    return(0);
  }

  if(param->flag == TC_AUDIO) {

    // print out
    if(verbose_flag) tc_log_info(MOD_NAME, "video4linux audio grabbing");

    //set device with vob->audio_in_file
    if(audio_grab_init(vob->audio_in_file, vob->a_rate, vob->a_bits, vob->a_chan, verbose_flag)<0) return(TC_IMPORT_ERROR);

    aframe_pts0 = aframe_pts = v4l_counter_init();

    param->fd = NULL;

    return(0);
  }

  return(TC_IMPORT_ERROR);
}

/* ------------------------------------------------------------
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/

MOD_decode{

  if(param->flag == TC_VIDEO) {

    if(!do_resync) video_drop_frames=1;

    do {
      video_grab_frame(param->buffer);
      if((verbose & TC_STATS) && vframe_cnt<MAX_DISPLAY_PTS) v4l_counter_print("VIDEO", vframe_cnt, vframe_pts0, &vframe_pts);

      ++vframe_cnt;
      --video_drop_frames;

    } while(video_drop_frames>0);

    video_drop_frames=1;

    return(0);
  }

  if(param->flag == TC_AUDIO) {

    if(!do_resync) audio_drop_frames=1;

    do {

      audio_grab_frame(param->buffer, param->size);
      if((verbose & TC_STATS) && aframe_cnt<MAX_DISPLAY_PTS) v4l_counter_print("AUDIO", aframe_cnt, aframe_pts0, &aframe_pts);

      ++aframe_cnt;
      --audio_drop_frames;

    } while(audio_drop_frames>0);

    audio_drop_frames=1;

    return(0);
 }

 return(TC_IMPORT_ERROR);

}

/* ------------------------------------------------------------
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{

  if(param->flag == TC_VIDEO) {

    // stop grabbing
    video_grab_close(do_audio);

    return(0);
  }

  if(param->flag == TC_AUDIO) {

    // stop grabbing
    audio_grab_close(do_audio);

    return(0);
  }

  return(TC_IMPORT_ERROR);
}


