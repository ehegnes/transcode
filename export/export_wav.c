/*
 *  export_wav.c
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

#define MOD_NAME    "export_wav.so"
#define MOD_VERSION "v0.2.3 (2003-01-16)"
#define MOD_CODEC   "(audio) WAVE PCM"

#define MOD_PRE wav
#include "export_def.h"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_YUV|TC_CAP_VID;

static struct wave_header rtf;
static int fd;
static long total=0;

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
    rtf.riff.len = sizeof(struct riff_struct) + sizeof(struct
chunk_struct) + sizeof(struct common_struct);
    strncpy(rtf.riff.id, "RIFF", 4);
    strncpy(rtf.riff.wave_id, "WAVE",4);
    strncpy(rtf.format.id, "fmt ",4);
    
    rtf.format.len = sizeof(struct common_struct);
    rtf.common.wFormatTag=CODEC_PCM;
    
    rate=(vob->mp3frequency != 0) ? vob->mp3frequency : vob->a_rate;

    rtf.common.dwSamplesPerSec = rate; 
    rtf.common.dwAvgBytesPerSec = vob->dm_chan * rate * vob->dm_bits/8;
    rtf.common.dwAvgBytesPerSec = rate * vob->dm_bits/8;
    rtf.common.wChannels=vob->dm_chan;
    rtf.common.wBitsPerSample=vob->dm_bits;
    rtf.common.wBlockAlign=vob->dm_chan*vob->dm_bits/8;

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

  if(param->flag == TC_AUDIO) {
    
    if((fd = open(vob->audio_out_file, O_RDWR|O_CREAT|O_TRUNC,
		  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH))<0) {
      perror("open file");
      return(TC_EXPORT_ERROR);
    }     

    total=0;
    
    if(p_write(fd, (char *) &rtf, sizeof(rtf)) != sizeof(rtf)) {    
      perror("write wave header");
      return(TC_EXPORT_ERROR);
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

  int size = param->size;
    
  if(param->flag == TC_AUDIO) { 
    
    if(p_write(fd, param->buffer, size) != size) {    
      perror("write audio frame");
      return(TC_EXPORT_ERROR);
    }     

    total += size;

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

  off_t fsize;
  
  if(param->flag == TC_VIDEO) return(0);
  if(param->flag == TC_AUDIO) {

    /* If we can't seek then it is most probably a pipe and there is no
     * way to fix the header. At the same time it is not a problem and we
     * just return success.
    */
    if((fsize = lseek(fd, 0, SEEK_CUR))<0) {
      fprintf(stderr,"\nCan't seek to fix header, probably a pipe\n");
      return(0);
    }
    
    rtf.riff.len=fsize-8;
    rtf.data.len=total;

    //write wave header
    lseek(fd, 0, SEEK_SET);     

    if(p_write(fd, (char *) &rtf, sizeof(rtf)) != sizeof(rtf)) {    
      perror("write wave header");
      return(TC_EXPORT_ERROR);
    }     

    close(fd);

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

