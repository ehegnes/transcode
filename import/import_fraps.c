/*
 *  import_fraps.c
 *
 *  Copyright (C) Tilmann Bitterberg - Nov 2003
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
 *
 * Fraps has codec FPS1 and the image data is actually a packed form of YUV420
 *
 * Binary layout is as follows:
 * Each frame has an 8 byte header:
 *    The first DWORD (little endian) contains the following:
        Bit 31  - Skipped frame when set, no data.
        Bit 30  - DWORD pad to align to 8 bytes (always set)
        Bit 0-7 - Version number (currently 0. Unrecognised format if higher)
 * Then comes data which is organized into 24 bytes blocks:
 * 
 *   8bytes luma | 8bytes = next line luma | 4 bytes Cr | 4 bytes Cb
 *
 * So one 24 bytes block makes defines 8x2 pixels
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "transcode.h"

#define MOD_NAME    "import_fraps.so"
#define MOD_VERSION "v0.0.2 (2003-11-12)"
#define MOD_CODEC   "(video) * "

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_YUV;

#define MOD_PRE fraps
#include "import_def.h"

static avi_t *avifile2=NULL;

static int vframe_count=0;
static int width=0, height=0;


/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

static char *buffer = NULL;
static char *save_buffer = NULL;

MOD_open
{
  double fps=0;
  char *codec=NULL;
  size_t size;

  param->fd = NULL;

  if(param->flag == TC_VIDEO) {
    
    
    param->fd = NULL;
    
    if(avifile2==NULL) {
      if(vob->nav_seek_file) {
	if(NULL == (avifile2 = AVI_open_input_indexfile(vob->video_in_file,0,vob->nav_seek_file))){
	  AVI_print_error("avi open error");
	  return(TC_IMPORT_ERROR); 
	} 
      } else {
	if(NULL == (avifile2 = AVI_open_input_file(vob->video_in_file,1))){
	  AVI_print_error("avi open error");
	  return(TC_IMPORT_ERROR); 
	} 
      }
    }

    size = AVI_video_width(avifile2)*AVI_video_height(avifile2)*3;
    if (!buffer) buffer = malloc(size);
    if (!save_buffer) save_buffer = malloc(size);

    if (vob->vob_offset>0)
	AVI_set_video_position(avifile2, vob->vob_offset);
    
    //read all video parameter from input file
    width  =  AVI_video_width(avifile2);
    height =  AVI_video_height(avifile2);
    
    fps    =  AVI_frame_rate(avifile2);
    codec  =  AVI_video_compressor(avifile2);
    
    
    fprintf(stderr, "[%s] codec=%s, fps=%6.3f, width=%d, height=%d\n", 
	    MOD_NAME, codec, fps, width, height);
    
    if((strlen(codec)!=0 && strcmp("FPS1", codec) != 0) || vob->im_v_codec == CODEC_RGB) {
      fprintf(stderr, "error: invalid AVI file codec '%s' for YUV processing\n", codec);
      return(TC_IMPORT_ERROR);
    }
    
    return(0);
  }
  
  return(TC_IMPORT_ERROR);
}


/* ------------------------------------------------------------ 
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/


MOD_decode 
{
  
  int key;

  
  if(param->flag == TC_VIDEO) {
    int x, y;
    int size = 0;
    char *c, *d, *e, *u, *v;
    unsigned char version;

    // If we are using tccat, then do nothing here
    if (param->fd != NULL) {
      return(0);
    }

    size = AVI_read_frame(avifile2, buffer, &key);

    if(size<=0) {
	if(verbose & TC_DEBUG) AVI_print_error("AVI read video frame");
	return(TC_IMPORT_ERROR);
    }

    if (size<width*height) {
	tc_memcpy (buffer, save_buffer, width*height*3/2+8);
    } else {
	tc_memcpy (save_buffer, buffer, width*height*3/2+8);
    }


    // right?
    version = buffer[0] & 0xff;
    if (version != 0) {
	tc_warn ("unsupported protocol version for FRAPS");
	return (TC_IMPORT_ERROR);
    }
    c = buffer+8; // skip header
    d = param->buffer;
    param->size = width*height*3/2;
    

    v = param->buffer+width*height;
    u = param->buffer+(width*height)*5/4;

    for (y=0; y<height; y+=2) {
	d = param->buffer+y*width;
	e = param->buffer+(y+1)*width;

	for (x=0; x<width; x+=8) {
	    tc_memcpy(d, c, 8);
	    tc_memcpy(e, c+8, 8);
	    tc_memcpy (u, c+16, 4);
	    tc_memcpy (v, c+20, 4);
	    c+=24; 
	    d+=8;
	    e+=8;
	    u+=4;
	    v+=4;
	}
    }

    param->attributes |= TC_FRAME_IS_KEYFRAME;

    ++vframe_count;

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
    
    if(param->fd != NULL) pclose(param->fd);
    
    if(param->flag == TC_VIDEO) {
	
	if(avifile2!=NULL) {
	    AVI_close(avifile2);
	    avifile2=NULL;
	}
	return(0);
    }
    
    return(TC_IMPORT_ERROR);
}


/* -- parse as R:3 G:6 B:3
	val =  ((c[2]<<16)|(c[1]<<8)|c[0]);
	*d++ = (( val & 0xE00000 >> 21) << 4) & 0xff;
	*d++ = (( val & 0x1F8000 >> 15) << 1)& 0xff;
	*d++ = (( val & 0x7000 >>   12) << 4)& 0xff;
	*d++ = (( val & 0xE00 >>   9) << 4) & 0xff;
	*d++ = (( val & 0x1F8 >>   3) << 1)& 0xff;
	*d++ = (( val & 0x7 >>   0) << 4)& 0xff;
	c+=3;
 */
