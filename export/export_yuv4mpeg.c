/*
 *  export_yuv4mpeg.c
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
#include "aud_aux.h"
#include "vid_aux.h"

#define MOD_NAME    "export_yuv4mpeg.so"
#define MOD_VERSION "v0.1.3 (2002-03-11)"
#define MOD_CODEC   "(video) YUV4MPEG2 | (audio) MPEG/AC3/PCM"

#define MOD_PRE yuv4mpeg
#include "export_def.h"

#include "mjpegtools/yuv4mpeg.h"
#include "mjpegtools/mpegconsts.h"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_YUV|TC_CAP_PCM|TC_CAP_AC3|TC_CAP_AUD|TC_CAP_RGB;


static int fd, size;

float framerates[] = { 0, 23.976, 24.0, 25.0, 29.970, 30.0, 50.0, 59.940, 60.0 };

static y4m_stream_info_t y4mstream;

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

	//ThOe added RGB2YUV cap
	if(vob->im_v_codec == CODEC_RGB) {
	    if(tc_rgb2yuv_init(vob->ex_v_width, vob->ex_v_height)<0) {
		fprintf(stderr, "[%s] rgb2yuv init failed\n", MOD_NAME);
		return(TC_EXPORT_ERROR); 
	    }
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

  int asr, mask;
  char dar_tag[20];
  y4m_ratio_t framerate;  

  if(param->flag == TC_VIDEO) {
    
    // video

    //note: this is the real framerate of the raw stream
    framerate = (vob->im_frc==0) ? mpeg_conform_framerate(vob->fps):mpeg_framerate(vob->im_frc);

    asr = (vob->ex_asr<0) ? vob->im_asr:vob->ex_asr;

    y4m_init_stream_info(&y4mstream);
    y4m_si_set_framerate(&y4mstream,framerate);
    y4m_si_set_interlace(&y4mstream,Y4M_UNKNOWN );
    y4m_si_set_sampleaspect(&y4mstream,y4m_sar_UNKNOWN);
    snprintf( dar_tag, 19, "XM2AR%03d", asr );
    y4m_xtag_add( y4m_si_xtags(&y4mstream), dar_tag );
    y4mstream.height = vob->ex_v_height;
    y4mstream.width = vob->ex_v_width;
    
    size = vob->ex_v_width * vob->ex_v_height * 3/2;
    
    mask = umask (0);
    umask (mask);
    
    if((fd = open(vob->video_out_file, O_RDWR|O_CREAT|O_TRUNC, (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) &~ mask))<0) {
	  perror("open file");
	  return(TC_EXPORT_ERROR);
    }     
    
    if( y4m_write_stream_header( fd, &y4mstream ) != Y4M_OK ){
      perror("write stream header");
      return(TC_EXPORT_ERROR);
    }     
    
    /* WAS: sprintf(buf, "FRAME\n"); */
    
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
    y4m_frame_info_t info;
    
    if(param->flag == TC_VIDEO) { 
	
	//ThOe 
	if(tc_rgb2yuv_core(param->buffer)<0) {
	    fprintf(stderr, "[%s] rgb2yuv conversion failed\n", MOD_NAME);
	    return(TC_EXPORT_ERROR);
	}
	
	
	y4m_init_frame_info(&info);
	
	if(y4m_write_frame_header( fd, &info ) != Y4M_OK )
	{
	    perror("write frame header");
	    return(TC_EXPORT_ERROR);
	}     
	
	//do not trust para->size
	if(p_write(fd, param->buffer, size) != size) {    
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
  
  if(param->flag == TC_VIDEO) {
      
      //ThOe
      tc_rgb2yuv_close();
      
      return(0);
  }
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
  if(param->flag == TC_VIDEO) {
    close(fd);
    return(0);
  }
  return(TC_EXPORT_ERROR);  
  
}

