/*
 *  probe_lav.c
 *
 *  Copyright (C) Alex Stewart - July 2002
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

#include "ioaux.h"
#include "tc.h"

#define LAV_AUDIO_FORMAT TC_CODEC_AC3
#define LAV_VIDEO_FORMAT TC_CODEC_LAV

#define MAX_BUF 1024

static char cmd_buf[MAX_BUF];
static char read_buf[MAX_BUF];
static char default_value[] = "0";

void probe_lav(info_t *ipipe)
{
    FILE *f;
    char *param, *value, *ptr;

    if (ipipe->seek_allowed>0) {
      // We're working on a file, so the filename should be in ipipe->name.
      // Do this the easy way.
      if((tc_snprintf(cmd_buf, MAX_BUF, "lavinfo \"%s\"", ipipe->name)<0)) {
        perror("Unable to create lavinfo command string");
	return;
      }

      if (ipipe->verbose & TC_DEBUG) printf("(%s) %s\n", __FILE__, cmd_buf);
           
      if ((f = popen(cmd_buf, "r")) == NULL) {
        perror("Unable to execute lavinfo");
        return;
      }
    } else {
      fprintf(stderr, "(%s) LAV parsing from a stream not yet supported\n", __FILE__);
      return;
    }

    while (!feof(f)) {
      fgets(read_buf, MAX_BUF, f);
      param = read_buf;
      value = default_value;
      for (ptr=read_buf; ptr<read_buf+MAX_BUF && *ptr; ptr++) {
	if (*ptr == '=') {
	  *ptr = 0;
	  value = ptr+1;
	}
      }
      *(--ptr) = 0; // For safety (and gets rid of trailing \n)

      if (strcmp(param, "video_frames") == 0) {
	ipipe->probe_info->frames = atoi(value);
	continue;
      }
      if (strcmp(param, "video_width") == 0) {
	ipipe->probe_info->width = atoi(value);
	continue;
      }
      if (strcmp(param, "video_height") == 0) {
	ipipe->probe_info->height = atoi(value);
	continue;
      }
      if (strcmp(param, "video_fps") == 0) {
	ipipe->probe_info->fps = atof(value);
	continue;
      }
      if (strcmp(param, "has_audio") == 0) {
	ipipe->probe_info->num_tracks = atoi(value);
	continue;
      }
      if (strcmp(param, "audio_chans") == 0) {
	ipipe->probe_info->track[0].chan = atoi(value);
	continue;
      }
      if (strcmp(param, "audio_bits") == 0) {
	ipipe->probe_info->track[0].bits = atoi(value);
	continue;
      }
      if (strcmp(param, "audio_rate") == 0) {
	ipipe->probe_info->track[0].samplerate = atoi(value);
	continue;
      }
    }

    if (ipipe->probe_info->num_tracks) {
      ipipe->probe_info->track[0].format = LAV_AUDIO_FORMAT;
    }
    ipipe->probe_info->magic=TC_MAGIC_LAV;
    ipipe->probe_info->codec=LAV_VIDEO_FORMAT;
    ipipe->probe_info->frc=fps2frc(ipipe->probe_info->fps);

    pclose(f);
}
