/*
 *  export_dvraw.c
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
#include "../libdvenc/dvenc.h"
#include "transcode.h"

#define MOD_NAME    "export_dvraw.so"
#define MOD_VERSION "v0.1.0 (2001-12-04)"
#define MOD_CODEC   "(video) Digital Video | (audio) PCM"

#define MOD_PRE dvraw
#include "export_def.h"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_YUV;

static int fd;

unsigned char target[TC_FRAME_DV_PAL];
unsigned char vbuf[SIZE_RGB_FRAME];

static int frame_size=0;

int p_write (int fd, char *buf, size_t len)
{
   size_t n = 0;
   size_t r = 0;

   while (r < len) {
      n = write (fd, buf + r, len - r);
      if (n < 0)
         return n;
      
      r += n;
   }
   return r;
}

/* ------------------------------------------------------------ 
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{
    
    if(param->flag == TC_VIDEO) {

      dvenc_init();

      return(0);
    }

    if(param->flag == TC_AUDIO) return(0);

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
  int mask, format;
  
  if(param->flag == TC_VIDEO) {
    
    // video
    mask = umask (0);
    umask (mask);
    
    if((fd = open(vob->video_out_file, O_RDWR|O_CREAT|O_TRUNC, (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) &~ mask))<0) {
      perror("open file");
      
      return(TC_EXPORT_ERROR);
    }     

  switch(vob->im_v_codec) {
      
    case CODEC_RGB:
      format=0;
      break;
      
    case CODEC_YUV:
      format=1;
      break;
      
    default:
      
      fprintf(stderr, "[%s] codec not supported\n", MOD_NAME);
      return(TC_EXPORT_ERROR); 
      
      break;
    }

  // for reading
  frame_size = (vob->ex_v_height==PAL_H) ? TC_FRAME_DV_PAL:TC_FRAME_DV_NTSC;
  
  dvenc_set_parameter(format, vob->ex_v_height, vob->a_rate);
  
  return(0);
  }
  
  
  if(param->flag == TC_AUDIO) return(0);
  
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

  if(param->flag == TC_VIDEO) { 
    
    memcpy(vbuf, param->buffer, param->size);
    return(0);
  }
  
  if(param->flag == TC_AUDIO) {
    
    dvenc_frame(vbuf, param->buffer, param->size, target);
    
    //merge audio and write dv frame
    if(p_write(fd, target, frame_size) != frame_size) {    
      perror("write frame");
      return(TC_EXPORT_ERROR);
    }     
    
    return(0);
  }

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
  
  if(param->flag == TC_VIDEO) {
    
    dvenc_close();
    return(0);
  }

  if(param->flag == TC_AUDIO) return(0);
  
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * close outputfiles
 *
 * ------------------------------------------------------------*/

MOD_close
{  

  if(param->flag == TC_VIDEO) {
    close(fd);
    return(0);
  }

  if(param->flag == TC_AUDIO) return(0);
  
  return(TC_EXPORT_ERROR);  

}

