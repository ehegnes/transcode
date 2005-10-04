/*
 *  export_pvn.c
 *
 *  Copyright (C) Jacob (Jack) Gryn - July 2004
 *
 *  Based on export_ppm module by Thomas Östreich - June 2001
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

#include <stdio.h>
#include <stdlib.h>

#include "transcode.h"
#include "avilib.h"
#include "aud_aux.h"
#include "aclib/colorspace.h"

#define MOD_NAME    "export_pvn.so"
#define MOD_VERSION "v0.1 (2004-07-12)"
#define MOD_CODEC   "(video) PVN | (audio) MPEG/AC3/PCM"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_YUV|TC_CAP_RGB|TC_CAP_PCM|TC_CAP_AC3|TC_CAP_AUD;

#define MOD_PRE pvn
#include "export_def.h"

static char buf[512];

static FILE *fd;

static uint8_t tmp_buffer[SIZE_RGB_FRAME];

static int codec, width, height, row_bytes;

static char *type;
static int interval=1;
static unsigned int int_counter=0;

/* ------------------------------------------------------------ 
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{
    
    /* set the 'spit-out-frame' interval */
    interval = vob->frame_interval;

    if(param->flag == TC_VIDEO) {

      if(vob->im_v_codec == CODEC_YUV) {
	colorspace_init (tc_accel);

	width = vob->ex_v_width;
	height = vob->ex_v_height;
	
	row_bytes = vob->v_bpp/8 * vob->ex_v_width;

	codec =  CODEC_YUV;
      }

      return(0);
    }

    if(param->flag == TC_AUDIO) return(audio_init(vob, verbose_flag));

    // invalid flag
    return(TC_EXPORT_ERROR); 
}

/* ------------------------------------------------------------ 
 *
 * open outputfile
 *
 * ------------------------------------------------------------*/

MOD_open
{
  
    if(param->flag == TC_VIDEO) {
      
      // video
      
	switch(vob->im_v_codec) {

	case CODEC_YUV:
	case CODEC_RGB:
	  
	  type = (vob->decolor) ? "PV5a":"PV6a"; 
	  
	  if((fd = fopen(vob->video_out_file, "w"))<0) 
	  {
	    perror("fopen file");
	    return(TC_EXPORT_ERROR);
	  }     
    
          /* replace 0 with actual # of frames; but 0 should work for now */
	  snprintf(buf, sizeof(buf), "%s\n#(%s-v%s) \n%d %d %d\n8.0000 %d\n", type, PACKAGE, VERSION, vob->ex_v_width, vob->ex_v_height, 0, (unsigned int)vob->ex_fps);

	  if(fwrite(buf, strlen(buf), 1, fd) != 1) 
	  {    
	    perror("write header");
	    return(TC_EXPORT_ERROR);
	  }     

	  break;
	  
	default:
	  
	  fprintf(stderr, "[%s] codec not supported\n", MOD_NAME);
	  return(TC_EXPORT_ERROR); 
	  
	  break;
	}
	
	return(0);
    }
    
    
    if(param->flag == TC_AUDIO) return(audio_open(vob, NULL));
    
    // invalid flag
    return(TC_EXPORT_ERROR); 
}   

/* ------------------------------------------------------------ 
 *
 * encode and export
 *
 * ------------------------------------------------------------*/

MOD_encode
{
  char *out_buffer = param->buffer;
  int n, out_size = param->size;
  
  if ((++int_counter-1) % interval != 0)
      return (0);

  if(param->flag == TC_VIDEO) { 
    
    
    if(codec==CODEC_YUV) {
      yuv2rgb (tmp_buffer, 
	       param->buffer, param->buffer+width*height, 
	       param->buffer+5*width*height/4, 
	       width, height, row_bytes, width, width/2);
      
      out_buffer = tmp_buffer;
      out_size = height * 3 *width;
    }
    
    if(strncmp(type, "PV5a", 4)==0) 
    {   
	out_size /= 3;
	for (n=0; n<out_size; ++n) out_buffer[n] = out_buffer[3*n];
    }
    
    if(fwrite(out_buffer, out_size, 1, fd) != 1) {    
	perror("write frame");
	return(TC_EXPORT_ERROR);
    }  
    return(0);
  }
  
  if(param->flag == TC_AUDIO) return(audio_encode(param->buffer, param->size, NULL));
  
  // invalid flag
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * stop encoder
 *
 * ------------------------------------------------------------*/

MOD_stop 
{
  
  if(param->flag == TC_VIDEO) return(0);
  if(param->flag == TC_AUDIO) return(audio_stop());
  
  fclose(fd);

  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * close outputfiles
 *
 * ------------------------------------------------------------*/

MOD_close
{  
    if(fd != NULL)    
      fclose(fd);

    if(param->flag == TC_AUDIO) return(audio_close());
    if(param->flag == TC_VIDEO) return(0);

    return(TC_EXPORT_ERROR);      
}

