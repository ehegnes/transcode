/*
 *  export_debugppm.c
 *
 *  Copyright (C) Thomas Östreich - June 2003
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

#define MOD_NAME    "export_debugppm.so"
#define MOD_VERSION "v0.0.1 (2003-06-19)"
#define MOD_CODEC   "(video) debugPPM/PGM | (audio) MPEG/AC3/PCM"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_YUV|TC_CAP_RGB|TC_CAP_PCM|TC_CAP_AC3|TC_CAP_AUD;

#define MOD_PRE debugppm
#include "export_def.h"

static char buf[256];
static char buf2[64];

static int codec, width, height;

static int counter=0;
static char *prefix="frame";
static char *type;
static int interval=1;
static unsigned int int_counter=0;

/* ------------------------------------------------------------ 
 * if we have YUV codec, we'll write a pgm file which is the
 * original's width, but 1.5 times the original's height.  In
 * the extra half of the height, we'll put the images of the
 * Cb and Cr, so that we have a graymap of the original's luminance
 * with two images (each 1/4 the size of the original) under that
 * of each chrominance.  The one in the bottom left is the first
 * chrominance.
 *
 * If we're using rgb, we'll make three files, _r.pgm _g.pgm and
 * (you guessed it!) _b.pgm so we'll have three graymaps for
 * each color.
 *
 * sometimes I want to know if one particular color is scewed,
 * or if the Y or Cb/Cr is messed up, and this way we can look
 * at the individual components which make up the color relatively
 * conveniently.  
 *
 * ------------------------------------------------------------*/

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

      width = vob->ex_v_width;
      height = vob->ex_v_height;

      if(vob->im_v_codec == CODEC_YUV) {
	codec =  CODEC_YUV;
        return(0);
      }
      if(vob->im_v_codec == CODEC_RGB) {
	codec =  CODEC_RGB;
	return(0);
      }
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
	  if(vob->video_out_file!=NULL && strcmp(vob->video_out_file,"/dev/null")!=0){
	    prefix=vob->video_out_file;
	  }
	  type = "P5";
	  snprintf(buf, sizeof(buf), "%s\n%d %d 255\n", type, vob->ex_v_width, vob->ex_v_height*3/2);
	  break;

	case CODEC_RGB:
	  
	  if(vob->video_out_file!=NULL && strcmp(vob->video_out_file,"/dev/null")!=0){
	    prefix=vob->video_out_file;
	  }
	  type = "P5"; 
	  snprintf(buf, sizeof(buf), "%s\n%d %d 255\n", type, vob->ex_v_width, vob->ex_v_height);
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
  int n, offset, out_size = param->size;
  
  if ((++int_counter-1) % interval != 0)
      return (0);

  if(param->flag == TC_VIDEO) { 
    
    if(codec==CODEC_RGB) {
      FILE *fdr,*fdg,*fdb;
      char *c_buffer;
      out_size /= 3;
      snprintf(buf2, sizeof(buf2), "%s%06d_r.pgm", prefix, counter);
      if((fdr = fopen(buf2, "w"))<0) {
        perror("fopen file");
	return(TC_EXPORT_ERROR);
      }   
      snprintf(buf2, sizeof(buf2), "%s%06d_g.pgm", prefix, counter);
      if((fdg = fopen(buf2, "w"))<0) {
        perror("fopen file");
	return(TC_EXPORT_ERROR);
      }   
      snprintf(buf2, sizeof(buf2), "%s%06d_b.pgm", prefix, counter++);
      if((fdb = fopen(buf2, "w"))<0) {
        perror("fopen file");
	return(TC_EXPORT_ERROR);
      }   
      c_buffer = (char *)malloc( sizeof(char)*width*height);
      if (NULL == c_buffer){
        perror("allocate memory");
	return(TC_EXPORT_ERROR);
      }
      for (n=0; n<out_size; ++n) c_buffer[n] = out_buffer[3*n];
      if(fwrite(buf, strlen(buf), 1, fdr) != 1) {
        perror("write header");
	return(TC_EXPORT_ERROR);
      }
      if(fwrite(c_buffer,out_size,1,fdr) != 1){
        perror("write frame");
	return(TC_EXPORT_ERROR);
      }
      fclose(fdr);
      for (n=0; n<out_size; ++n) c_buffer[n] = out_buffer[3*n+1];
      if(fwrite(buf, strlen(buf), 1, fdg) != 1) {
        perror("write header");
	return(TC_EXPORT_ERROR);
      }
      if(fwrite(c_buffer,out_size,1,fdg) != 1){
        perror("write frame");
	return(TC_EXPORT_ERROR);
      }
      fclose(fdg);
      for (n=0; n<out_size; ++n) c_buffer[n] = out_buffer[3*n+2];
      if(fwrite(buf, strlen(buf), 1, fdb) != 1) {
        perror("write header");
	return(TC_EXPORT_ERROR);
      }
      if(fwrite(c_buffer,out_size,1,fdb) != 1){
        perror("write frame");
	return(TC_EXPORT_ERROR);
      }
      fclose(fdb);
      free(c_buffer);
      return(0);
    } // else YUV
    snprintf(buf2, sizeof(buf2), "%s%06d.pgm", prefix, counter++);
    
    if((fd = fopen(buf2, "w"))<0) {
      perror("fopen file");
      return(TC_EXPORT_ERROR);
    }     
    
    if(fwrite(buf, strlen(buf), 1, fd) != 1) {    
      perror("write header");
      return(TC_EXPORT_ERROR);
    }     

    if(fwrite(out_buffer,width*height , 1, fd) != 1) {    
	perror("write frame");
	return(TC_EXPORT_ERROR);
    }  
    out_buffer += width*height;
    offset = (width*height)>>2;
    for(n=0 ; n<height/2 ; n++){
      if(fwrite(out_buffer,width/2,1,fd) != 1) {
        perror("write frame");
	return(TC_EXPORT_ERROR);
      }
      if(fwrite(out_buffer+offset,width/2,1,fd) != 1) {
        perror("write frame");
	return(TC_EXPORT_ERROR);
      }
      out_buffer += width/2;
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

