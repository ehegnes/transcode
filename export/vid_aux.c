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

static int convert=0, convertY=0;
static int x_dim=0, y_dim=0;
static int x_dimY=0, y_dimY=0;
static char *frame_buffer=NULL;
static char *frame_bufferY=NULL;
#define BUFFER_SIZE SIZE_RGB_FRAME
static char *y_out, *u_out, *v_out;
static char *rgb_outY;

int tc_yuv2rgb_init(int width, int height)
{
    
    if(convert) tc_yuv2rgb_close();
    
    // XXX: 24
    yuv2rgb_init(24, MODE_BGR);
    
    if ((frame_bufferY = malloc(BUFFER_SIZE))==NULL) return(-1);
    
    memset(frame_bufferY, 0, BUFFER_SIZE);  

    //init data

    x_dimY = width;
    y_dimY = height;
    
    rgb_outY=frame_bufferY;
    
    //activate
    convertY = 1;
    
    return(0);
}

int tc_yuv2rgb_core(char *buffer)
{	  
    if(!convertY) return(0);
    
    //conversion
    
    yuv2rgb((void *)rgb_outY, 
	    buffer, buffer + x_dimY*y_dimY, buffer + (x_dimY*y_dimY*5)/4,
	    x_dimY /*h_size*/, y_dimY /*v_size*/, 
	    x_dimY*3 /*rgb_stride*/, x_dimY, x_dimY/2);
    
    //put it back
    memcpy(buffer, rgb_outY, y_dim*x_dim*3);
    
    return(0);
    
}


int tc_yuv2rgb_close()
{
    if(!convertY) return(0);
    
    if(frame_bufferY!=NULL) free(frame_bufferY);
    
    frame_bufferY=NULL;
    convertY = 0;

    return(0);
}

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

int tc_rgb2yuv_core_flip(char *buffer)
{	  
    int cc=0, flip=1;
    
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
