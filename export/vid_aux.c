/*
 *  vid_aux.c
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

#include "vid_aux.h"

static int convert=0;
static int x_dim=0, y_dim=0;
static char *frame_buffer=NULL;
#define BUFFER_SIZE SIZE_RGB_FRAME
static char *y_out, *u_out, *v_out;

int tc_rgb2yuv_init(int width, int height)
{
    
    if(convert) tc_rgb2yuv_close();
    
    init_rgb2yuv();
    
    if ((frame_buffer = malloc(BUFFER_SIZE))==NULL) return(-1);
    
    memset(frame_buffer, 0, BUFFER_SIZE);  

    //init data

    x_dim = width;
    y_dim = height;
    
    y_out=frame_buffer;
    u_out=frame_buffer + y_dim*x_dim;
    v_out=frame_buffer + (y_dim*x_dim*5)/4;
    
    //activate
    convert = 1;
    
    return(0);
}

int tc_rgb2yuv_core(char *buffer)
{	  
    int cc=0, flip=0;
    
    if(!convert) return(0);
    
    //conversion
    
    cc=RGB2YUV(x_dim, y_dim, buffer, y_out,
		  u_out, v_out, x_dim, flip);
    
    if(cc!=0) return(-1);
    
    //put it back
    memcpy(buffer, frame_buffer, (y_dim*x_dim*3)/2);
    
    return(0);
    
}

int tc_rgb2yuv_close()
{
    if(!convert) return(0);
    
    if(frame_buffer!=NULL) free(frame_buffer);
    
    frame_buffer=NULL;
    convert = 0;

    return(0);
}
