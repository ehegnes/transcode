/*
 *  vid_aux.h
 *
 *  Copyright (C) Thomas Östreich - January 2002
 *
 *  This file is part of transcode, a linux video stream processing tool
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

#ifndef _VID_AUX_H
#define _VID_AUX_H

#include "config.h"
#include "transcode.h"
#include "libvo/rgb2yuv.h"
#include "libvo/yuv2rgb.h"

int tc_rgb2yuv_init(int width, int height);
int tc_rgb2yuv_core(char *buffer);
int tc_rgb2yuv_core_flip(char *buffer);
int tc_rgb2yuv_close();

int tc_yuv2rgb_init(int width, int height);
int tc_yuv2rgb_core(char *buffer);
int tc_yuv2rgb_close();

void yv12toyuy2(char *_y, char *_u, char *_v, char *output, int width, int height); 
void uyvytoyuy2(char *input, char *output, int width, int height);

#endif
