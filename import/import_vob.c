/*
 *  import_vob.c
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

#include "transcode.h"
#include "ac3scan.h"
#include "demuxer.h"
#include "clone.h"

#define MOD_NAME    "import_vob.so"
#define MOD_VERSION "v0.5.8 (2003-06-11)"
#define MOD_CODEC   "(video) MPEG-2 | (audio) MPEG/AC3/PCM | (subtitle)"

#define MOD_PRE vob
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_RGB|TC_CAP_YUV|TC_CAP_PCM|TC_CAP_AC3;

static int codec, syncf=0;
static int pseudo_frame_size=0, real_frame_size=0, effective_frame_size=0;
static int ac3_bytes_to_go=0;
static FILE *fd;

/* ------------------------------------------------------------
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  
  char *logfile="sync.log";
  
  char seq_buf[256];
  char dem_buf[256];

  int off=0x80;
  
  (vob->ps_seq1 != 0 || vob->ps_seq2 != INT_MAX) ? snprintf(seq_buf, 256, "-S %d,%d-%d", vob->ps_unit, vob->ps_seq1, vob->ps_seq2) : sprintf(seq_buf, "-S %d", vob->ps_unit);
  
  if(param->flag == TC_AUDIO) {

    sprintf(dem_buf, "-M %d", vob->demuxer);

    codec = vob->im_a_codec;
    syncf = vob->sync;
    
    switch(codec) {
      
    case CODEC_AC3:
      
      if((snprintf(import_cmd_buf, MAX_BUF, "tccat -i \"%s\" -t vob -d %d -S %d | tcdemux -a %d -x ac3 %s %s -d %d | tcextract -t vob -a %d -x ac3 -d %d | tcextract -t raw -x ac3 -d %d", vob->audio_in_file, vob->verbose, vob->vob_offset, vob->a_track, seq_buf, dem_buf, vob->verbose, vob->a_track, vob->verbose, vob->verbose)<0)) {
	perror("command buffer overflow");
	return(TC_IMPORT_ERROR);
      }
      
      if(verbose_flag & TC_DEBUG) printf("[%s] AC3->AC3\n", MOD_NAME);
      
      break;
      
    case CODEC_PCM:
      
      if(vob->fixme_a_codec==CODEC_AC3) {
	
	if((snprintf(import_cmd_buf, MAX_BUF, "tccat -i \"%s\" -t vob -d %d -S %d | tcdemux -a %d -x ac3 %s %s -d %d | tcextract -t vob -a %d -x ac3 -d %d | tcdecode -x ac3 -d %d -s %f,%f,%f -A %d", vob->audio_in_file, vob->verbose, vob->vob_offset, vob->a_track, seq_buf, dem_buf, vob->verbose, vob->a_track, vob->verbose, vob->verbose, vob->ac3_gain[0], vob->ac3_gain[1], vob->ac3_gain[2], vob->a52_mode)<0)) {
	  perror("command buffer overflow");
	  return(TC_IMPORT_ERROR);
	}
	
	if(verbose_flag & TC_DEBUG) printf("[%s] AC3->PCM\n", MOD_NAME);
      } 
      
      
      if(vob->fixme_a_codec==CODEC_A52) {
	
	if((snprintf(import_cmd_buf, MAX_BUF, "tccat -i \"%s\" -t vob -d %d -S %d | tcdemux -a %d -x ac3 %s %s -d %d | tcextract -t vob -a %d -x ac3 -d %d | tcdecode -x a52 -d %d -A %d", vob->audio_in_file, vob->verbose, vob->vob_offset, vob->a_track, seq_buf, dem_buf, vob->verbose, vob->a_track, vob->verbose, vob->verbose, vob->a52_mode)<0)) {
	  perror("command buffer overflow");
	  return(TC_IMPORT_ERROR);
	}
	
	if(verbose_flag & TC_DEBUG) printf("[%s] A52->PCM\n", MOD_NAME);
      } 
      
      if(vob->fixme_a_codec==CODEC_MP3) {
	
	if((snprintf(import_cmd_buf, MAX_BUF, "tccat -i \"%s\" -t vob -d %d -S %d | tcdemux -a %d -x mp3 %s %s -d %d | tcextract -t vob -a %d -x mp3 -d %d | tcdecode -x mp3 -d %d", vob->audio_in_file, vob->verbose, vob->vob_offset, vob->a_track, seq_buf, dem_buf, vob->verbose, vob->a_track, vob->verbose, vob->verbose)<0)) {
	  perror("command buffer overflow");
	  return(TC_IMPORT_ERROR);
	}
	
	if(verbose_flag & TC_DEBUG) printf("[%s] MP3->PCM\n", MOD_NAME);
      }

      if(vob->fixme_a_codec==CODEC_MP2) {
	
	if((snprintf(import_cmd_buf, MAX_BUF, "tccat -i \"%s\" -t vob -d %d -S %d | tcdemux -a %d -x mp3 %s %s -d %d | tcextract -t vob -a %d -x mp2 -d %d | tcdecode -x mp2 -d %d", vob->audio_in_file, vob->verbose, vob->vob_offset, vob->a_track, seq_buf, dem_buf, vob->verbose, vob->a_track, vob->verbose, vob->verbose)<0)) {
	  perror("command buffer overflow");
	  return(TC_IMPORT_ERROR);
	}
	
	if(verbose_flag & TC_DEBUG) printf("[%s] MP2->PCM\n", MOD_NAME);
      }
      
      if(vob->fixme_a_codec==CODEC_PCM || vob->fixme_a_codec==CODEC_LPCM) {
	
	if((snprintf(import_cmd_buf, MAX_BUF, "tccat -i \"%s\" -t vob -d %d -S %d | tcdemux -a %d -x pcm %s %s -d %d | tcextract -t vob -a %d -x pcm -d %d", vob->audio_in_file, vob->verbose, vob->vob_offset, vob->a_track, seq_buf, dem_buf, vob->verbose, vob->a_track, vob->verbose)<0)) {
	  perror("command buffer overflow");
	  return(TC_IMPORT_ERROR);
	}
	
	if(verbose_flag & TC_DEBUG) printf("[%s] LPCM->PCM\n", MOD_NAME);
      }
      
      break;
      
    default: 
      fprintf(stderr, "invalid import codec request 0x%x\n", codec);
      return(TC_IMPORT_ERROR);
      
    }
    
    // print out
    if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);
    
    // set to NULL if we handle read
    param->fd = NULL;
    
    // popen
    if((fd = popen(import_cmd_buf, "r"))== NULL) {
      perror("popen PCM stream");
      return(TC_IMPORT_ERROR);
    }
    
    return(0);
  }
  
  if(param->flag == TC_SUBEX) {  
    
    sprintf(dem_buf, "-M %d", vob->demuxer);
    
    codec = vob->im_a_codec;
    syncf = vob->sync;
    
    if((snprintf(import_cmd_buf, MAX_BUF, "tccat -i \"%s\" -t vob -d %d -S %d | tcdemux -a %d -x ps1 %s %s -d %d | tcextract -t vob -a 0x%x -x ps1 -d %d", vob->audio_in_file, vob->verbose, vob->vob_offset, vob->s_track, seq_buf, dem_buf, vob->verbose, (vob->s_track+0x20), vob->verbose)<0)) {
      perror("command buffer overflow");
	  return(TC_IMPORT_ERROR);
    }
    
    if(verbose_flag & TC_DEBUG) printf("[%s] subtitle extraction\n", MOD_NAME);
    
    // print out
    if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);
    
    // popen
    if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
      perror("popen subtitle stream");
      return(TC_IMPORT_ERROR);
    }
    
    return(0);
  }
  
  if(param->flag == TC_VIDEO) {


      if (vob->demuxer==TC_DEMUX_SEQ_FSYNC || vob->demuxer==TC_DEMUX_SEQ_FSYNC2) {
	
	if((logfile=clone_fifo())==NULL) {
	  printf("[%s] failed to create a temporary pipe\n", MOD_NAME);
	  return(TC_IMPORT_ERROR);
	} 
	sprintf(dem_buf, "-M %d -f %f -P %s %s", vob->demuxer, vob->fps, logfile, ((vob->vob_chunk==0)? "": "-O"));
      } else sprintf(dem_buf, "-M %d", vob->demuxer);
      
      //determine subtream id for sync adjustment
      //default is off=0x80

      off=0x80;
      
      if(vob->fixme_a_codec==CODEC_PCM || vob->fixme_a_codec==CODEC_LPCM) 
	off=0xA0;
      if(vob->fixme_a_codec==CODEC_MP3 || vob->fixme_a_codec==CODEC_MP2) 
	off=0xC0;
      
      switch(vob->im_v_codec) {
	
      case CODEC_RGB:
	
	if((snprintf(import_cmd_buf, MAX_BUF, "tccat -i \"%s\" -t vob -d %d -S %d | tcdemux -s 0x%x -x mpeg2 %s %s -d %d | tcextract -t vob -a %d -x mpeg2 -d %d | tcdecode -x mpeg2 -d %d", vob->video_in_file, vob->verbose, vob->vob_offset, (vob->a_track+off), seq_buf, dem_buf, vob->verbose, vob->v_track, vob->verbose, vob->verbose)<0)) {
	  perror("command buffer overflow");
	  return(TC_IMPORT_ERROR);
	}
	
	break;
	
      case CODEC_YUV:
	
	if((snprintf(import_cmd_buf, MAX_BUF, "tccat -i \"%s\" -t vob -d %d -S %d | tcdemux -s 0x%x -x mpeg2 %s %s -d %d | tcextract -t vob -a %d -x mpeg2 -d %d | tcdecode -x mpeg2 -d %d -y yv12", vob->video_in_file, vob->verbose, vob->vob_offset, (vob->a_track+off), seq_buf, dem_buf, vob->verbose, vob->v_track, vob->verbose, vob->verbose)<0)) {
	  perror("command buffer overflow");
	  return(TC_IMPORT_ERROR);
	}
	
	break;
      }
      
      // print out
      if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);
      
      param->fd = NULL;
      
      // popen
      if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
	perror("popen RGB stream");
	return(TC_IMPORT_ERROR);
      }

      if (vob->demuxer==TC_DEMUX_SEQ_FSYNC || vob->demuxer==TC_DEMUX_SEQ_FSYNC2) {
	
	if(clone_init(param->fd)<0) {
	  if(verbose_flag) printf("[%s] failed to init stream sync mode\n", MOD_NAME);
	  return(TC_IMPORT_ERROR);
	} else param->fd = NULL;
      }
      
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
  
  int ac_bytes=0, ac_off=0; 
  int num_frames;

  if(param->flag == TC_VIDEO) {
    
    if (vob->demuxer==TC_DEMUX_SEQ_FSYNC || vob->demuxer==TC_DEMUX_SEQ_FSYNC2) {
      
      if(clone_frame(param->buffer, param->size)<0) {
	if(verbose_flag & TC_DEBUG) printf("[%s] end of stream - failed to sync video frame\n", MOD_NAME);
	return(TC_IMPORT_ERROR);
      } 
    }
    
    return(0);
  }
  
  if(param->flag == TC_SUBEX) return(0);
  
  if(param->flag == TC_AUDIO) {
      
    switch(codec) {
      
    case CODEC_AC3:
      
      // determine frame size at the very beginning of the stream
      
      if(pseudo_frame_size==0) {
	  
	if(ac3scan(fd, param->buffer, param->size, &ac_off, &ac_bytes, &pseudo_frame_size, &real_frame_size, verbose)!=0) return(TC_IMPORT_ERROR);
	
      } else { 
	ac_off = 0;
	ac_bytes = pseudo_frame_size;
      }


      // switch to entire frames:
      // bytes_to_go is the difference between requested bytes and 
      // delivered bytes
      //
      // pseudo_frame_size = average bytes per audio frame
      // real_frame_size = real AC3 frame size in bytes

      num_frames = (ac_bytes + ac3_bytes_to_go) / real_frame_size;

      effective_frame_size = num_frames * real_frame_size;
      ac3_bytes_to_go = ac_bytes + ac3_bytes_to_go - effective_frame_size;
      
      // return effective_frame_size as physical size of audio data
      param->size = effective_frame_size; 

      if(verbose_flag & TC_STATS) fprintf(stderr,"[%s] pseudo=%d, real=%d, frames=%d, effective=%d\n", MOD_NAME, ac_bytes, real_frame_size, num_frames, effective_frame_size);

      // adjust
      ac_bytes=effective_frame_size;

      if(syncf>0) {
	//dump an ac3 frame, instead of a pcm frame 
	ac_bytes = real_frame_size-ac_off;
	param->size = real_frame_size; 
	--syncf;
      }
      
      break;
      
    case CODEC_PCM:
      
      //default:
      ac_off   = 0;
      ac_bytes = param->size;
      break;
      
    default: 
      fprintf(stderr, "invalid import codec request 0x%x\n",codec);
      return(TC_IMPORT_ERROR);
      
    }
    
    if (fread(param->buffer+ac_off, ac_bytes, 1, fd) !=1) 
      return(TC_IMPORT_ERROR);
    
    
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
    
    if(param->fd) { 
	pclose(param->fd);
    }
    param->fd = NULL;

    if(param->flag == TC_VIDEO) {

	//safe
      clone_close();
	
      return(0);
    }
    
    if(param->flag == TC_SUBEX) return(0);
    
    if(param->flag == TC_AUDIO) {
      
      if(fd) pclose(fd);
      fd=NULL;
      
      return(0);
      
    }
    return(TC_IMPORT_ERROR);
}


