/*
 *  export_ppm.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
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

#include <stdio.h>
#include <stdlib.h>

#include "transcode.h"
#include "avilib.h"
#include "aud_aux.h"
#include "yuv2rgb.h"

#define MOD_NAME    "export_ppm.so"
#define MOD_VERSION "v0.1.1 (2002-02-14)"
#define MOD_CODEC   "(video) PPM/PGM | (audio) MPEG/AC3/PCM"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_YUV|TC_CAP_RGB|TC_CAP_PCM|TC_CAP_AC3|TC_CAP_AUD;

#define MOD_PRE ppm
#include "export_def.h"

static char buf[256];
static char buf2[64];

static uint8_t *tmp_buffer; //[SIZE_RGB_FRAME];

static int codec, width, height, row_bytes;

static int counter=0;
static char *prefix="frame";
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
	yuv2rgb_init (vob->v_bpp, MODE_BGR); 

	width = vob->ex_v_width;
	height = vob->ex_v_height;
	
	row_bytes = vob->v_bpp/8 * vob->ex_v_width;

	codec =  CODEC_YUV;

	if (!tmp_buffer) tmp_buffer = malloc (vob->ex_v_width*vob->ex_v_height*3);
	if (!tmp_buffer) return 1;
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
	  
	  if(vob->video_out_file!=NULL && strcmp(vob->video_out_file,"/dev/null")!=0) prefix=vob->video_out_file;
	  
	  type = (vob->decolor) ? "P5":"P6"; 

	  sprintf(buf, "%s\n#(%s-v%s) \n%d %d 255\n", type, PACKAGE, VERSION, vob->ex_v_width, vob->ex_v_height);
	  
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
  
  FILE *fd;
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
    
    if(strncmp(type, "P5", 2)==0) {   
	out_size /= 3;
	for (n=0; n<out_size; ++n) out_buffer[n] = out_buffer[3*n];
	sprintf(buf2, "%s%06d.pgm", prefix, counter++);
    } else 
	sprintf(buf2, "%s%06d.ppm", prefix, counter++);
    
    if((fd = fopen(buf2, "w"))==NULL) {
      perror("fopen file");
      return(TC_EXPORT_ERROR);
    }     
    
    if(fwrite(buf, strlen(buf), 1, fd) != 1) {    
      perror("write header");
      return(TC_EXPORT_ERROR);
    }     

    if(fwrite(out_buffer, out_size, 1, fd) != 1) {    
	perror("write frame");
	return(TC_EXPORT_ERROR);
    }  
    fclose(fd);
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

  if (tmp_buffer) free(tmp_buffer);
  tmp_buffer = NULL;
  
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * close outputfiles
 *
 * ------------------------------------------------------------*/

MOD_close
{  

    if(param->flag == TC_AUDIO) return(audio_close());
    if(param->flag == TC_VIDEO) return(0);
    
    return(TC_EXPORT_ERROR);  
    
}

