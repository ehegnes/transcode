/*
 *  vid_aux.h
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

#ifndef _VID_AUX_H
#define _VID_AUX_H

#include "transcode.h"

#include "aclib/ac.h"
#include "aclib/imgconvert.h"

int tc_rgb2yuv_init(int width, int height);
int tc_rgb2yuv_core(uint8_t *buffer);
int tc_rgb2yuv_close(void);

int tc_yuv2rgb_init(int width, int height);
int tc_yuv2rgb_core(uint8_t *buffer);
int tc_yuv2rgb_close(void);

#endif
