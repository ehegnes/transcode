/*
 * video_out_yuv.c
 * Copyright (C) 1999-2001 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "yuv2rgb.h"

#include "video_out.h"
#include "video_out_internal.h"

typedef struct yuv_instance_s {
    vo_instance_t vo;
    int prediction_index;
    vo_frame_t * frame_ptr[3];
    vo_frame_t frame[3];
    int width;
    int height;
    int rgbstride;
    int bpp;
    int pipe;
    uint8_t *rgbdata;
    int framenum;
    void (* outstream) (char *buff, int bytes);
    char header[1024];
    char filename[128];
} yuv_instance_t;


static void internal_draw_frame (yuv_instance_t * instance,
				 vo_frame_t * frame)
{
    instance->outstream(frame->base[0], instance->width*instance->height);
    instance->outstream(frame->base[2], instance->width/2*instance->height/2);
    instance->outstream(frame->base[1], instance->width/2*instance->height/2);
}

static int internal_setup (vo_instance_t * _instance, int width, int height,
			   void (* draw_frame) (vo_frame_t *))
{
    yuv_instance_t * instance;

    instance = (yuv_instance_t *) _instance;

    instance->vo.close = libvo_common_free_frames;
    instance->vo.get_frame = libvo_common_get_frame;
    instance->width = width;
    instance->height = height;
    
    instance->rgbstride = width * instance->bpp / 8;
    instance->rgbdata = malloc (instance->rgbstride * height);

    return libvo_common_alloc_frames ((vo_instance_t *) instance,
				      width, height, sizeof (vo_frame_t),
				      NULL, NULL, draw_frame);
}

static void yuv_draw_frame (vo_frame_t * frame)
{
    yuv_instance_t * instance;

    instance = (yuv_instance_t *) frame->instance;
    if (++(instance->framenum) < 0)
	return;
    
    internal_draw_frame (instance, frame);
}

static int yuv_setup (vo_instance_t * instance, int width, int height)
{
    return internal_setup (instance, width, height, yuv_draw_frame);
}

vo_instance_t * vo_yuv_open (void (*callback))
{
    yuv_instance_t * instance;

    instance = malloc (sizeof (yuv_instance_t));
    if (instance == NULL)
        return NULL;

    instance->bpp = 24;
    instance->pipe = 0;
    instance->outstream=callback;

    instance->vo.setup = yuv_setup;
    instance->framenum = -2;
    return (vo_instance_t *) instance;
}

static void yuvpipe_draw_frame (vo_frame_t *frame)
{
    yuv_instance_t * instance;

    instance = (yuv_instance_t *)frame->instance;
    if (++(instance->framenum) >= 0)
	internal_draw_frame (instance, frame);
}

static int yuvpipe_setup (vo_instance_t * instance, int width, int height)
{
  return internal_setup (instance, width, height, yuvpipe_draw_frame);
}

vo_instance_t *vo_yuvpipe_open (void (*callback))
{
    yuv_instance_t * instance;

    instance = malloc (sizeof (yuv_instance_t));
    if (instance == NULL)
        return NULL;

    instance->bpp = 24;
    instance->pipe = 1;
    instance->outstream=callback;

    yuv2rgb_init (instance->bpp, MODE_BGR);

    instance->vo.setup = yuvpipe_setup;
    instance->framenum = -2;
    return (vo_instance_t *) instance;
}

