/*
 *  ac3scan.c
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
#include <unistd.h>
#include <inttypes.h>

#include "ac3.h"
#include "ac3scan.h"
#include "ioaux.h"

#define MAX_BUF 4096
static char sbuffer[MAX_BUF];

int ac3scan(FILE *fd, char *buffer, int size, int *ac_off, int *ac_bytes, int *pseudo_size, int *real_size, int verbose)
{
  int bitrate;
  
  float rbytes;
  
  int frame_size, pseudo_frame_size;
  
  if (fread(buffer, 5, 1, fd) !=1) 
    return(TC_IMPORT_ERROR);
  
  if((frame_size = 2*get_ac3_framesize(buffer+2)) < 1) {
    fprintf(stderr, "(%s) AC3 framesize=%d invalid\n", __FILE__, frame_size);
    return(TC_IMPORT_ERROR);    
  }
  
  // A single AC3 frame produces exactly 1536 samples 
  // and for 2 channels and 16bit 6kB bytes PCM audio 
  
  rbytes = (float) (size)/1024/6 * frame_size;
  pseudo_frame_size = (int) (rbytes+0.5); // XXX
  bitrate = get_ac3_bitrate(buffer+2);
  
  if(verbose) fprintf(stderr, "(%s) AC3 frame %d (%d) bytes | bitrate %d kBits/s | depsize %d | rbytes %f\n", __FILE__, frame_size, pseudo_frame_size, bitrate, size, rbytes);
    
  // return information
  
  *ac_off=5;
  *ac_bytes = pseudo_frame_size-(*ac_off);
  *pseudo_size = pseudo_frame_size;
  *real_size = frame_size;
  
  return(0);
}

static int verbose_flag=TC_QUIET;

int buf_probe_ac3(unsigned char *_buf, int len, pcm_t *pcm)
{

  int j=0, i=0, bitrate, fsize, nfchans;

  char *buffer;

  uint16_t sync_word = 0;

  // need to find syncframe:

  buffer=_buf;
  
  for(i=0; i<len-4; ++i) {
    
    sync_word = (sync_word << 8) + (uint8_t) buffer[i]; 
    
    if(sync_word == 0x0b77) break;
  }

  if(verbose_flag & TC_DEBUG) fprintf(stderr, "AC3 syncbyte @ %d\n", i);
  
  if(sync_word != 0x0b77) return(-1);

  j = get_ac3_samplerate(&buffer[i+1]);
  bitrate = get_ac3_bitrate(&buffer[i+1]);  
  fsize = 2*get_ac3_framesize(&buffer[i+1]);
  nfchans = get_ac3_nfchans(&buffer[i+1]);
  
  if(j<0 || bitrate <0) return(-1);

  pcm->samplerate = j;
  pcm->chan = (nfchans<2?2:nfchans);
  pcm->bits = 16;
  pcm->format = CODEC_AC3;
  pcm->bitrate = bitrate;
 
  if(verbose_flag & TC_DEBUG) 
      fprintf(stderr, "(%s) samplerate=%d Hz, bitrate=%d kbps, size=%d bytes\n", __FILE__, pcm->samplerate, bitrate, fsize);
  
  return(0);
}

void probe_ac3(info_t *ipipe)
{

    // need to find syncframe:
    
    if(p_read(ipipe->fd_in, sbuffer, MAX_BUF) != MAX_BUF) {
	ipipe->error=1;
	return;
    }

    verbose_flag = ipipe->verbose;

    //for single AC3 stream only
    if(buf_probe_ac3(sbuffer, MAX_BUF, &ipipe->probe_info->track[0])<0) {
	ipipe->error=1;
	return;
    }
    ipipe->probe_info->magic = TC_MAGIC_AC3;
    ++ipipe->probe_info->num_tracks;

    return;
}

