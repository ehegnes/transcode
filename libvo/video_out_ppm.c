/*
 * video_out_ppm.c
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

typedef struct ppm_instance_s {
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
} ppm_instance_t;


static void internal_draw_frame (ppm_instance_t * instance, FILE * file,
				 vo_frame_t * frame)
{
    uint8_t *frame_in;
    
    unsigned int y; 
    
    yuv2rgb (instance->rgbdata, 
	     frame->base[0], frame->base[1], frame->base[2], 
	     instance->width, instance->height, instance->rgbstride, 
	     instance->width, instance->width >> 1);
    
    if(!instance->pipe) {
      fwrite (instance->rgbdata, instance->width*3, instance->height, file);
    } else {
    
      // frame is written upside down
      frame_in   = instance->rgbdata + instance->rgbstride*(instance->height-1);
      
      for (y = instance->height; y > 0; y--) {
	
	instance->outstream(frame_in, instance->rgbstride);
	
	frame_in  -= instance->rgbstride;
      }
    }
}


void directdraw_rgb(vo_instance_t *_instance, char *yuv[3])
{
    ppm_instance_t *instance;
    instance = (ppm_instance_t *) _instance;

    yuv2rgb (instance->rgbdata, yuv[0],  yuv[1],  yuv[2], 
	     instance->width, instance->height, instance->rgbstride, 
	     instance->width, instance->width >> 1);
    
    instance->outstream(instance->rgbdata, instance->rgbstride*instance->height);
}


static int internal_setup (vo_instance_t * _instance, int width, int height,
			   void (* draw_frame) (vo_frame_t *))
{
    ppm_instance_t * instance;

    instance = (ppm_instance_t *) _instance;

    instance->vo.close = libvo_common_free_frames;
    instance->vo.get_frame = libvo_common_get_frame;
    instance->width = width;
    instance->height = height;

    sprintf (instance->header, "P6\n#ThOe \n%d %d 255\n", width, height);

    instance->rgbstride = width * instance->bpp / 8;
    instance->rgbdata = malloc (instance->rgbstride * height);

    return libvo_common_alloc_frames ((vo_instance_t *) instance,
				      width, height, sizeof (vo_frame_t),
				      NULL, NULL, draw_frame);
}

static void ppm_draw_frame (vo_frame_t * frame)
{
    ppm_instance_t * instance;
    FILE * file;

    instance = (ppm_instance_t *) frame->instance;
    if (++(instance->framenum) < 0)
	return;
    sprintf (instance->filename, "%06d.ppm", instance->framenum);
    file = fopen (instance->filename, "wb");
    if (!file)
	return;

    fwrite (instance->header, strlen (instance->header), 1, file);

    internal_draw_frame (instance, file, frame);
    fclose (file);
}

static int ppm_setup (vo_instance_t * instance, int width, int height)
{
    return internal_setup (instance, width, height, ppm_draw_frame);
}

vo_instance_t * vo_ppm_open (void (*callback))
{
    ppm_instance_t * instance;

    instance = malloc (sizeof (ppm_instance_t));
    if (instance == NULL)
        return NULL;

    instance->bpp = 24;
    instance->pipe = 0;
    instance->outstream=callback;

    yuv2rgb_init (instance->bpp, MODE_RGB);

    instance->vo.setup = ppm_setup;
    instance->framenum = -2;
    return (vo_instance_t *) instance;
}

static void ppmpipe_draw_frame (vo_frame_t *frame)
{
    ppm_instance_t * instance;

    instance = (ppm_instance_t *)frame->instance;
    if (++(instance->framenum) >= 0)
	internal_draw_frame (instance, stdout, frame);
}

static int ppmpipe_setup (vo_instance_t * instance, int width, int height)
{
  return internal_setup (instance, width, height, ppmpipe_draw_frame);
}

vo_instance_t *vo_ppmpipe_open (void (*callback))
{
    ppm_instance_t * instance;

    instance = malloc (sizeof (ppm_instance_t));
    if (instance == NULL)
        return NULL;

    instance->bpp = 24;
    instance->pipe = 1;
    instance->outstream=callback;

    yuv2rgb_init (instance->bpp, MODE_BGR);

    instance->vo.setup = ppmpipe_setup;
    instance->framenum = -2;
    return (vo_instance_t *) instance;
}

