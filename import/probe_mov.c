/*
 *  probe_mov.c
 *
 *  Copyright (C) Thomas Östreich - Januray 2002
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

#include "ioaux.h"
#include "tc.h"

#ifdef HAVE_QT

#include <quicktime.h>

void probe_mov(info_t *ipipe)
{
  
  quicktime_t *qt_file=NULL;
  char *codec=NULL;

  int j, tracks;

  /* open movie for video probe */
  if(qt_file==NULL) 
    if(NULL == (qt_file = quicktime_open(ipipe->name,1,0))){
      fprintf(stderr,"error: can't open quicktime!\n");
      ipipe->error=1;
      return; 
    }
  
  // extract audio parameters
  tracks=quicktime_audio_tracks(qt_file);
  
  if(tracks>TC_MAX_AUD_TRACKS) {
    fprintf(stderr, "(%s) only %d of %d audio tracks scanned\n", __FILE__, TC_MAX_AUD_TRACKS, tracks);
    tracks=TC_MAX_AUD_TRACKS;
  }
  
  for(j=0; j<tracks; ++j) {
    
    ipipe->probe_info->track[j].samplerate = quicktime_sample_rate(qt_file, j);
    ipipe->probe_info->track[j].chan = quicktime_track_channels(qt_file, j);
    ipipe->probe_info->track[j].bits = quicktime_audio_bits(qt_file, j);

    codec  = quicktime_audio_compressor(qt_file, j);

    if(strcasecmp(codec,QUICKTIME_RAW)==0 || strcasecmp(codec,QUICKTIME_TWOS)==0) ipipe->probe_info->track[j].format = CODEC_PCM;

    if(strcasecmp(codec,QUICKTIME_IMA4)==0)
      ipipe->probe_info->track[j].format = CODEC_IMA4;
    
    fprintf(stderr, "[%s] codec=%s\n", __FILE__, codec);
    
    if(ipipe->probe_info->track[j].chan>0) ++ipipe->probe_info->num_tracks;
  }
  
  
  // read all video parameter from input file
  ipipe->probe_info->width  =  quicktime_video_width(qt_file, 0);
  ipipe->probe_info->height =  quicktime_video_height(qt_file, 0);    
  ipipe->probe_info->fps = quicktime_frame_rate(qt_file, 0);
  
  ipipe->probe_info->frames = quicktime_video_length(qt_file, 0);

  codec  =  quicktime_video_compressor(qt_file, 0);

  //check for supported codecs
  
  if(codec!=NULL) {
    
    if(strlen(codec)==0) {
      ipipe->probe_info->codec=TC_CODEC_RGB;
    } else {
      
      if(strcasecmp(codec,"dvsd")==0)
	ipipe->probe_info->codec=TC_CODEC_DV;
      
      if(strcasecmp(codec,"yv12")==0)
	ipipe->probe_info->codec=TC_CODEC_YV12;
      
      if(strcasecmp(codec,"DIV3")==0)
	ipipe->probe_info->codec=TC_CODEC_DIVX3;
      
      if(strcasecmp(codec,"DIVX")==0)
	ipipe->probe_info->codec=TC_CODEC_DIVX4;
      
      if(strcasecmp(codec,"MJPG")==0)
	ipipe->probe_info->codec=TC_CODEC_MJPG;

      if(strcasecmp(codec,"YUV2")==0)
	ipipe->probe_info->codec=TC_CODEC_YUV2;
      
    }
  } else
    ipipe->probe_info->codec=TC_CODEC_UNKNOWN;
  
  ipipe->probe_info->magic=TC_MAGIC_MOV;
  ipipe->probe_info->frc=fps2frc(ipipe->probe_info->fps);
  
  return;
}
#else

void probe_mov(info_t *ipipe)
{
	fprintf(stderr, "(%s) no support for Quicktime compiled - exit.\n", __FILE__);
	ipipe->error=1;
	return;
}
#endif

