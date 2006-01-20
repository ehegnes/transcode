/*
 *  probe_v4l.c
 *
 *  Copyright (C) Tilmann Bitterberg - January 2004
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
#include "tcinfo.h"
#include "ioaux.h"
#include "tc.h"

#ifdef HAVE_V4L

#include <sys/ioctl.h>

#include "v4l/videodev.h"

#if defined(HAVE_LINUX_VIDEODEV2_H) && defined(HAVE_STRUCT_V4L2_BUFFER)
#define _LINUX_TIME_H
#include <linux/videodev2.h>
#else
#include "v4l/videodev2.h"
#endif


void probe_v4l(info_t *ipipe)
{

  int is_v4l2 = 0, is_pal = 0;
  struct v4l2_capability caps; // v4l2
  struct video_capability  capability; // v4l1

  close(ipipe->fd_in);
  ipipe->fd_in = open(ipipe->name, O_RDWR, 0);
  if (ipipe->fd_in < 0) {
      fprintf(stderr, "[probe_v4l] cannot (reopen) device in RW mode\n");
      perror("[probe_v4l] open video4linux device");
      goto error;
  }

  // try a v4l2 ioctl
  if (ipipe->verbose & TC_DEBUG) fprintf(stderr, "Checking if v4l2 ioctls are supported .. ");
  if(ioctl(ipipe->fd_in, VIDIOC_QUERYCAP, &caps) < 0) {
      is_v4l2 = 0;
      if (ipipe->verbose & TC_DEBUG) fprintf(stderr, "no\n");
  } else {
      is_v4l2 = 1;
      ipipe->probe_info->magic=TC_MAGIC_V4L2_VIDEO;
      if (ipipe->verbose & TC_DEBUG) fprintf(stderr, "yes\n");
  }

  // try v4l1
  if (!is_v4l2) {
      if (ipipe->verbose & TC_DEBUG) fprintf(stderr, "Checking if v4l1 ioctls are supported .. ");
      if (-1 == ioctl(ipipe->fd_in,VIDIOCGCAP,&capability)) {
	  if (ipipe->verbose & TC_DEBUG) fprintf(stderr, "no\n");
	  goto error;
      } else {
	  ipipe->probe_info->magic=TC_MAGIC_V4L_VIDEO;
	  if (ipipe->verbose & TC_DEBUG) fprintf(stderr, "yes\n");
      }
  }

  // I don't care much for v4l1
  ipipe->probe_info->width  = 720;
  ipipe->probe_info->height = 576;
  ipipe->probe_info->fps = 25;
  ipipe->probe_info->frc = 3;

  if (is_v4l2) {

       v4l2_std_id std;

      if(ioctl(ipipe->fd_in, VIDIOC_G_STD, &std) < 0) {
	  is_pal = 1;
      } else {

	  if (std & V4L2_STD_525_60) {

	      ipipe->probe_info->fps = (30000/1001);
	      ipipe->probe_info->frc = 4;
	      ipipe->probe_info->width  = 640;
	      ipipe->probe_info->height = 480;
	      is_pal = 0;

	  } else if(std & V4L2_STD_625_50) {

	      ipipe->probe_info->fps = 25;
	      ipipe->probe_info->frc = 3;
	      ipipe->probe_info->width  = 720;
	      ipipe->probe_info->height = 576;
	      is_pal = 1;
	  }

      }
  }

  // FIXME: Check if these settings are actually supported by /dev/dsp
  ipipe->probe_info->track[0].samplerate = 44100;
  ipipe->probe_info->track[0].chan = 2;
  ipipe->probe_info->track[0].bits = 16;
  ipipe->probe_info->track[0].format = 0x1;

  if(ipipe->probe_info->track[0].chan>0) ipipe->probe_info->num_tracks=1;

  return;

error:
  ipipe->error = 1;
  ipipe->probe_info->codec=TC_CODEC_UNKNOWN;
  ipipe->probe_info->magic=TC_MAGIC_UNKNOWN;

  return;

}

#else // HAVE_V4l

void probe_v4l(info_t *ipipe)
{
    fprintf(stderr, "No support for video4linux compiled in\n");
    ipipe->probe_info->codec=TC_CODEC_UNKNOWN;
    ipipe->probe_info->magic=TC_MAGIC_UNKNOWN;
}

#endif
