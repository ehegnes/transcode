/*
 *  export_pcm.c
 *
 *  Copyright (C) Thomas Östreich - May 2002
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
#include "ioaux.h"

#define MOD_NAME    "export_pcm.so"
#define MOD_VERSION "v0.0.4 (2003-09-30)"
#define MOD_CODEC   "(audio) PCM (non-interleaved)"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_YUV|TC_CAP_VID;

#define MOD_PRE wav
#include "export_def.h"

static struct wave_header rtf;
static int fd_r, fd_l, fd_c, fd_ls, fd_rs, fd_lfe;

#if 0  /* gte this from ioaux.c */
static int p_write (int fd, char *buf, size_t len)
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
#endif

 
/* ------------------------------------------------------------ 
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{
  
  int rate;

  if(param->flag == TC_VIDEO) return(0);
  if(param->flag == TC_AUDIO) {

    memset((char *) &rtf, 0, sizeof(rtf));
    
    strncpy(rtf.riff.id, "RIFF", 4);
    strncpy(rtf.riff.wave_id, "WAVE",4);
    strncpy(rtf.format.id, "fmt ",4);
    
    rtf.format.len = sizeof(struct common_struct);
    rtf.common.wFormatTag=CODEC_PCM;
    
    rate=(vob->mp3frequency != 0) ? vob->mp3frequency : vob->a_rate;

    rtf.common.dwSamplesPerSec=rate;
    rtf.common.dwAvgBytesPerSec = vob->dm_chan * rate * vob->dm_bits/8;
    rtf.common.wChannels=vob->dm_chan;
    rtf.common.wBitsPerSample=vob->dm_bits;
    rtf.common.wBlockAlign=vob->dm_chan*vob->dm_bits/8;

    if (!vob->fixme_a_codec ||
        !rtf.common.wChannels || 
	!rtf.common.dwSamplesPerSec ||
	!rtf.common.wBitsPerSample ||
	!rtf.common.wBlockAlign) {
	    tc_warn("Cannot export PCM, invalid format (no audio track at all?)");
	    return TC_EXPORT_ERROR;
    }

    rtf.riff.len=0x7FFFFFFF;
    rtf.data.len=0x7FFFFFFF;

    strncpy(rtf.data.id, "data",4);

    return(0);
  }
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

  char fname[256];

  if(param->flag == TC_AUDIO) {
      
      switch(rtf.common.wChannels) {

	  
      case 6:
	  
	  tc_snprintf(fname, sizeof(fname), "%s_ls.pcm", vob->audio_out_file);
	  
	  if((fd_ls = open(fname, O_RDWR|O_CREAT|O_TRUNC,
			   S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH))
	     <0) {
	      perror("open file");
	      return(TC_EXPORT_ERROR);
	  }     
	  
	  
	  tc_snprintf(fname, sizeof(fname), "%s_rs.pcm", vob->audio_out_file);
	  
	  if((fd_rs = open(fname, O_RDWR|O_CREAT|O_TRUNC,
			   S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH))
	     <0) {
	      perror("open file");
	      return(TC_EXPORT_ERROR);
	  }     
	  
	  
	  tc_snprintf(fname, sizeof(fname), "%s_lfe.pcm", vob->audio_out_file);
	  
	  if((fd_lfe = open(fname, O_RDWR|O_CREAT|O_TRUNC,
			    S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH))
	     <0) {
	      perror("open file");
	      return(TC_EXPORT_ERROR);
	  }     
	  
	  
      case 2:
	  
	  tc_snprintf(fname, sizeof(fname), "%s_l.pcm", vob->audio_out_file);
	  
	  if((fd_l = open(fname, O_RDWR|O_CREAT|O_TRUNC,
			  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH))
	     <0) {
	      perror("open file");
	      return(TC_EXPORT_ERROR);
	  }     
	  
	  tc_snprintf(fname, sizeof(fname), "%s_r.pcm", vob->audio_out_file);
	  
	  if((fd_r = open(fname, O_RDWR|O_CREAT|O_TRUNC,
			  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH))
	     <0) {
	      perror("open file");
	      return(TC_EXPORT_ERROR);
	  }     
	  
      case 1:
	  
	  
	  tc_snprintf(fname, sizeof(fname), "%s_c.pcm", vob->audio_out_file);
	  
	  if((fd_c = open(fname, O_RDWR|O_CREAT|O_TRUNC,
			  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH))
	     <0) {
	      perror("open file");
	      return(TC_EXPORT_ERROR);
	  }     
      }      
      
      return(0);
  }
  
  
  if(param->flag == TC_VIDEO) return(0);
  
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

  if(param->flag == TC_AUDIO) { 
    int size = (int) (param->size/rtf.common.wChannels);
    
    switch(rtf.common.wChannels) {
      
    case 6:
      
      if(p_write(fd_rs, param->buffer + 5*size, size) != size) {
	perror("write audio frame");
	return(TC_EXPORT_ERROR);
      }     
      
      if(p_write(fd_ls, param->buffer + 4*size, size) != size) {    
	perror("write audio frame");
	return(TC_EXPORT_ERROR);
      }
      
      if(p_write(fd_r, param->buffer + 3*size, size) != size) {    
	perror("write audio frame");
	return(TC_EXPORT_ERROR);
      }     
      
      if(p_write(fd_c, param->buffer + 2*size, size) != size) {    
	perror("write audio frame");
	return(TC_EXPORT_ERROR);
      }

      if(p_write(fd_l, param->buffer + size, size) != size) {    
	perror("write audio frame");
	return(TC_EXPORT_ERROR);
      }
      
      if(p_write(fd_lfe, param->buffer, size) != size) {    
	perror("write audio frame");
	return(TC_EXPORT_ERROR);
      }
      
      break;
      
      
    case 2:
      
      if(p_write(fd_r, param->buffer + size, size) != size) {    
	perror("write audio frame");
	return(TC_EXPORT_ERROR);
      }
      
      if(p_write(fd_l, param->buffer, size) != size) {    
	perror("write audio frame");
	return(TC_EXPORT_ERROR);
      }
      
      break;
      
    case 1:
      
      if(p_write(fd_c, param->buffer, size) != size) {    
	perror("write audio frame");
	return(TC_EXPORT_ERROR);
      }
      
      break;
    }
    
    return(0);
  }
  
  if(param->flag == TC_VIDEO) return(0);
  
  // invalid flag
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * close outputfiles
 *
 * ------------------------------------------------------------*/

MOD_close
{  
  
  if(param->flag == TC_VIDEO) return(0);
  if(param->flag == TC_AUDIO) {
      
      close(fd_l);
      close(fd_c);
      close(fd_r);
      close(fd_ls);
      close(fd_rs);
      close(fd_lfe);

    return(0);
  }
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
  if(param->flag == TC_AUDIO) return(0);
  
  return(TC_EXPORT_ERROR);
}

