/*
 *  import_mpeg3.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  This file is part of transcode, a linux video stream  processing tool
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "transcode.h"
#include "libmpeg3.h"

#define MOD_NAME    "import_mpeg3.so"
#define MOD_VERSION "v0.1 (2001-08-18)"
#define MOD_CODEC   "(video) MPEG2"

#define MOD_PRE mpeg3
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

mpeg3_t* file;

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_RGB|TC_CAP_YUV;

static int codec, stream_id;
static int height, width; 

static unsigned char framebuffer[PAL_W*PAL_H*3];
static unsigned char extrabuffer[PAL_W*3+4];
static unsigned char *rowptr[PAL_H];

static unsigned char *y_output, *u_output, *v_output;

/* ------------------------------------------------------------
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{

  int i;

  if(param->flag == TC_AUDIO) return(0);

  if(param->flag == TC_VIDEO) {

    if(!mpeg3_check_sig(vob->video_in_file)) return(TC_IMPORT_ERROR);

    //open stream

    if((file = mpeg3_open(vob->video_in_file))==NULL) {
      fprintf(stderr, "open file failed\n");
      return(TC_IMPORT_ERROR);
    }

    codec=vob->im_v_codec;

    stream_id = vob->v_track;

    width=vob->im_v_width;
    height=vob->im_v_height;

    switch(codec) {
    
    case CODEC_RGB:
      
    for (i=0; i<height; i++)
    {
	if(i==height-1)
	{
	    rowptr[i]=extrabuffer;
	}
	rowptr[i]=framebuffer+(width*(height-1)*3)-(width*3*i);
    }
    break;

    case CODEC_YUV:
      
      y_output=framebuffer;
      v_output=y_output+width*height;
      u_output=v_output+((width*height)>>2);
      break;
    }
    
    param->fd = NULL;
    
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
    
  int i, block;

  if(param->flag == TC_AUDIO) return(0);
  if(param->flag == TC_VIDEO) {

#ifdef HAVE_MMX
    mpeg3_set_mmx(file, 1);
#endif

    switch(codec) {
    
    case CODEC_RGB:
      
      if(mpeg3_read_frame(file, rowptr, 0, 0, width, height, width, height, MPEG3_BGR888, stream_id)) return(TC_IMPORT_ERROR);
      
      block = width * 3; 
      param->size = block * height;
    
      for(i=0; i<height; ++i) 
	memcpy(param->buffer+(i-1)*block,rowptr[height-i-1], block);

      break;

    case CODEC_YUV:

      if(mpeg3_read_yuvframe(file, y_output, u_output, v_output, 0, 0, width, height, stream_id)) return(TC_IMPORT_ERROR);

      param->size = (width * height * 3)>>1;
      memcpy(param->buffer, framebuffer,param->size);
      break;
    }
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
      mpeg3_close(file);
    }
    if(param->flag == TC_AUDIO) return(0);

    return(TC_IMPORT_ERROR);
}


