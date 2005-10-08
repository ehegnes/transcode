/*
 *  vid_aux.c
 *
 *  Copyright (C) Thomas Östreich - January 2002
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

#include "vid_aux.h"

static int convert=0, convertY=0;
static int x_dim=0, y_dim=0;
static int x_dimY=0, y_dimY=0;
static uint8_t *frame_buffer=NULL, *planes[3];
static uint8_t *frame_bufferY=NULL, *rgb_outY;



int tc_yuv2rgb_init(int width, int height)
{
    if(convertY)
	tc_yuv2rgb_close();

    if ((frame_bufferY = malloc(width*height*3)) == NULL)
	return(-1);
    memset(frame_bufferY, 0, width*height*3);

    //init data
    x_dimY = width;
    y_dimY = height;
    rgb_outY = frame_bufferY;

    //activate
    convertY = 1;

    return(0);
}

int tc_yuv2rgb_core(uint8_t *buffer)
{
    uint8_t *planesY[3];

    if(!convertY)
	return(-1);

    //conversion
    planesY[0] = buffer;
    planesY[1] = planes[0] + x_dimY*y_dimY;
    planesY[2] = planes[1] + UV_PLANE_SIZE(IMG_YUV_DEFAULT, x_dimY, y_dimY);
    if (!ac_imgconvert(&rgb_outY, IMG_RGB_DEFAULT, planesY, IMG_YUV_DEFAULT,
		       x_dimY, y_dimY))
	return(-1);

    //put it back
    ac_memcpy(buffer, rgb_outY, x_dimY*y_dimY*3);

    return(0);
}

int tc_yuv2rgb_close()
{
    if(!convertY)
	return(0);

    free(frame_bufferY);
    frame_bufferY = NULL;
    convertY = 0;

    return(0);
}



int tc_rgb2yuv_init(int width, int height)
{
    if(convert)
	tc_rgb2yuv_close();

    if (!ac_imgconvert_init(tc_accel))
	return(-1);
    if ((frame_buffer = malloc(width*height*3))==NULL)
	return(-1);
    memset(frame_buffer, 0, width*height*3);

    //init data
    x_dim = width;
    y_dim = height;

    planes[0] = frame_buffer;
    planes[1] = planes[0] + x_dim*y_dim;
    planes[2] = planes[1] + UV_PLANE_SIZE(IMG_YUV_DEFAULT, x_dim, y_dim);

    //activate
    convert = 1;

    return(0);
}

int tc_rgb2yuv_core(uint8_t *buffer)
{
    if(!convert)
	return(-1);

    //conversion
    if (!ac_imgconvert(&buffer, IMG_RGB_DEFAULT, planes, IMG_YUV_DEFAULT,
		       x_dim, y_dim))
	return(-1);

    //put it back
    ac_memcpy(buffer, frame_buffer,
	      x_dim*y_dim + 2*UV_PLANE_SIZE(IMG_YUV_DEFAULT, x_dim, y_dim));

    return(0);
}

int tc_rgb2yuv_close()
{
    if(!convert)
	return(0);

    free(frame_buffer);
    frame_buffer = NULL;
    convert = 0;

    return(0);
}
