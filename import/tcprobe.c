/*
 *  tcprobe.c
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
#include <string.h>
#include <sys/errno.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>

#include "config.h"
#include "transcode.h"
#include "ioaux.h"
#include "tc.h"
#include "demuxer.h"
#include "dvd_reader.h"

#define EXE "tcprobe"

#define MAX_BUF 1024

static int verbose=TC_INFO;

int bitrate=ABITRATE;
int binary_dump=0;

void import_exit(int code) 
{
  if(verbose & TC_DEBUG) import_info(code, EXE);
  exit(code);
}


/* ------------------------------------------------------------ 
 *
 * print a usage/version message
 *
 * ------------------------------------------------------------*/



void usage(int status)
{
  version(EXE);

  fprintf(stderr,"\nUsage: %s [options] [-]\n", EXE);
  fprintf(stderr,"\t -i name        input file/directory/device/host name [stdin]\n");
  fprintf(stderr,"\t -B             binary output to stdout (used by transcode) [off]\n");
  fprintf(stderr,"\t -H n           probe n MB of stream [1]\n");
  fprintf(stderr,"\t -T title       probe for DVD title [off]\n");
  fprintf(stderr,"\t -b bitrate     audio encoder bitrate kBits/s [%d]\n", ABITRATE);
  fprintf(stderr,"\t -d verbosity   verbosity mode [1]\n");
  fprintf(stderr,"\t -v             print version\n");

  exit(status);
}


/* ------------------------------------------------------------ 
 *
 * universal extract thread frontend 
 *
 * ------------------------------------------------------------*/

int main(int argc, char *argv[])
{

    info_t ipipe;

    long 
	stream_stype = TC_STYPE_UNKNOWN, 
	stream_magic = TC_MAGIC_UNKNOWN, 
	stream_codec = TC_CODEC_UNKNOWN;

    int ch, i, n, cc=0, probe_factor=1;


    int dvd_title=1;
    char *name=NULL;

    char *c_ptr=NULL, *c_new="(*)", *c_old="";

    long frame_time=0;

    //proper initialization
    memset(&ipipe, 0, sizeof(info_t));

    while ((ch = getopt(argc, argv, "i:vBd:T:b:H:?h")) != -1) {
      
	switch (ch) {

	case 'b': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  bitrate = atoi(optarg);      
	  
	  if(bitrate < 0) {
	    fprintf(stderr,"invalid bitrate for option -b");
	    exit(1);
	  }
	  break;
	  
	  
	case 'i': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  name = optarg;
	  
	  break;
	  
	case 'd': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  verbose = atoi(optarg);
	  
	  break;

	case 'H': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  probe_factor = atoi(optarg);
	    
	  if(probe_factor < 0) {
	    fprintf(stderr,"invalid parameter for option -H");
	    exit(1);
	  }
	  break;
	    
	case 'B':
	  
	  binary_dump = 1;
	    
	  break;

	case 'T':
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  dvd_title = atoi(optarg);

	  break;
	  
	case 'v': 
	  
	  version(EXE);
	  exit(0);
	  break;
	  
	case 'h':
	  usage(EXIT_SUCCESS);
	default:
	    usage(EXIT_FAILURE);
	}
    }
    
    /* ------------------------------------------------------------ 
     *
     * fill out defaults for info structure
     *
     * ------------------------------------------------------------*/
    
    // assume defaults
    if(name==NULL) stream_stype=TC_STYPE_STDIN;

    if(optind < argc) {
	if(strcmp(argv[optind],"-")!=0) usage(EXIT_FAILURE);
	stream_stype=TC_STYPE_STDIN;
    }

    // need at least a file name
    if(argc==1) usage(EXIT_FAILURE);

    ipipe.verbose = verbose;

    //--------------------------------------
    //
    //  scan for valid source:
    //
    //  (I)   DVD device/image
    //  (II)  directory 
    //  (III) regular file
    //  (IV)  network host
    // 
    //--------------------------------------

    // do not try to mess with the stream
    if(stream_stype!=TC_STYPE_STDIN) {
      
      cc=probe_path(name);

      switch(cc) {
	
      case -1: //non-existent source
	exit(1);
	
      case 0:  //regular file
	
	if((ipipe.fd_in = open(name, O_RDONLY))<0) {
	  perror("file open");
	  return(-1);
	}

	stream_magic = fileinfo(ipipe.fd_in);
	ipipe.seek_allowed = 1;

	break;
	
	
      case 1:  //relative path to directory
	
	if(fileinfo_dir(name, &ipipe.fd_in, &stream_magic)<0) exit(1);
	ipipe.seek_allowed = 0;

	break;
	
	
      case 2: //absolute path

	ipipe.seek_allowed = 0;

	if(dvd_verify(name)<0) {

	  //normal directory - no DVD copy
	  if(fileinfo_dir(name, &ipipe.fd_in, &stream_magic)<0) exit(1);
	  
	} else {
	  
	  stream_magic = TC_MAGIC_DVD;
	} // DVD
	
	break;

      case 3: //network host

	ipipe.seek_allowed = 0;
	stream_magic = TC_MAGIC_SOCKET;
	break;

      default:
	exit(1);
	
      } // probe_path
      
    } else {
      ipipe.fd_in = STDIN_FILENO;
      ipipe.seek_allowed = 0;
      
      stream_magic = streaminfo(ipipe.fd_in);
    }

    if(strncmp(name, "/dev/video", 10)==0) stream_magic = TC_MAGIC_V4L_VIDEO;
    if(strncmp(name, "/dev/dsp",7)==0) stream_magic = TC_MAGIC_V4L_AUDIO;
    
    // fill out defaults for info structure
    ipipe.fd_out = STDOUT_FILENO;
    
    ipipe.magic = stream_magic;
    ipipe.stype = stream_stype;
    ipipe.codec = stream_codec;
    ipipe.name = name;
    ipipe.dvd_title = dvd_title;
    ipipe.factor = probe_factor;

    /* ------------------------------------------------------------ 
     *
     * codec specific section
     *
     * note: user provided values overwrite autodetection!
     *
     * ------------------------------------------------------------*/
    
    
    if(verbose) fprintf(stderr, "[%s] %s\n", EXE, filetype(stream_magic));
    
    tcprobe_thread(&ipipe);
    
    switch(ipipe.error) {
      
    case 1:
      if(verbose) fprintf(stderr, "[%s] failed to probe source\n", EXE);
      return(1);
      break;
      
    case 2:
      if(verbose) fprintf(stderr, "[%s] filetype/codec not yet supported by '%s'\n", EXE, PACKAGE);
      return(1);
      break;
    }
    
    //-------------------------------------
    //
    // transcode only support mode: 
    //
    // write source info structure to stdout
    //
    //-------------------------------------
    
    if(binary_dump) {
      p_write(ipipe.fd_out, (char*) ipipe.probe_info, sizeof(probe_info_t));
      exit(0);
    }
    
    //-------------------------------------------
    //
    // user mode:
    //
    // recommended transcode command line options:
    //
    //-------------------------------------------
    
    printf("[%s] summary for %s, (*) = not default, 0 = not detected\n", EXE, ((stream_magic==TC_STYPE_STDIN)?"-" : ipipe.name));
    
    // frame size
    
    c_ptr=c_old;
    if(ipipe.probe_info->width != PAL_W || ipipe.probe_info->height != PAL_H) c_ptr=c_new;
    
    if(ipipe.probe_info->width==0 || ipipe.probe_info->height==0) goto audio; 
    
    printf("%18s %s %dx%d [%dx%d] %s\n", "import frame size:", "-g", ipipe.probe_info->width, ipipe.probe_info->height, PAL_W, PAL_H, c_ptr);
    
    // aspect ratio
    
    c_ptr=c_old;
    if(ipipe.probe_info->asr != 1) c_ptr=c_new;
    
    switch(ipipe.probe_info->asr) {
      
    case 1:
      printf("%18s %s %s\n", "aspect ratio:", "1:1", c_ptr);
      break;
      
    case  2:
    case  8:
    case 12:
      printf("%18s %s %s\n", "aspect ratio:", "4:3", c_ptr);
      break;
      
    case 3:
      printf("%18s %s %s\n", "aspect ratio:", "16:9", c_ptr);
      break;
      
    case 4:
      printf("%18s %s %s\n", "aspect ratio:", "2.21:1", c_ptr);
      break;
    }
    
    // frame rate
    
    c_ptr=c_old;
    if(ipipe.probe_info->fps != PAL_FPS) c_ptr=c_new;

    frame_time = (ipipe.probe_info->fps!=0)? (long) (1./ipipe.probe_info->fps*1000):0;
    
    printf("%18s %s %.3f [%.3f] frc=%d %s\n", "frame rate:", "-f", ipipe.probe_info->fps, PAL_FPS, ipipe.probe_info->frc, c_ptr);

    if(ipipe.probe_info->pts_start && ipipe.probe_info->bitrate) printf("%18s PTS=%.4f, frame_time=%ld ms, bitrate=%ld kbps\n", "", ipipe.probe_info->pts_start, frame_time, ipipe.probe_info->bitrate);
    else if (ipipe.probe_info->pts_start) printf("%18s PTS=%.4f, frame_time=%ld ms\n", "", ipipe.probe_info->pts_start, frame_time);


 audio:
    
    // audio parameter
    
    for(n=0; n<TC_MAX_AUD_TRACKS; ++n) {
      
      if(ipipe.probe_info->track[n].format !=0 && ipipe.probe_info->track[n].chan>0) {
	
	c_ptr=c_old;
	if(ipipe.probe_info->track[n].samplerate != RATE || ipipe.probe_info->track[n].chan != CHANNELS  || ipipe.probe_info->track[n].bits != BITS || ipipe.probe_info->track[n].format != CODEC_AC3) c_ptr=c_new;
	
	printf("%18s -a %d [0] -e %d,%d,%d [%d,%d,%d] -n 0x%x [0x%x] %s\n", "audio track:", ipipe.probe_info->track[n].tid, ipipe.probe_info->track[n].samplerate, ipipe.probe_info->track[n].bits,  ipipe.probe_info->track[n].chan, RATE, BITS, CHANNELS, ipipe.probe_info->track[n].format, CODEC_AC3, c_ptr);
	
	if(ipipe.probe_info->track[n].pts_start 
	   && ipipe.probe_info->track[n].bitrate)
	  
	  printf("%18s PTS=%.4f, bitrate=%d kbps\n", " ", ipipe.probe_info->track[n].pts_start, ipipe.probe_info->track[n].bitrate);
	
	if(ipipe.probe_info->track[n].pts_start 
	   && ipipe.probe_info->track[n].bitrate==0)
	  printf("%18s PTS=%.4f\n", " ", ipipe.probe_info->track[n].pts_start);
	
	if(ipipe.probe_info->track[n].pts_start==0 
	   && ipipe.probe_info->track[n].bitrate)
	  printf("%18s bitrate=%d kbps\n", " ", ipipe.probe_info->track[n].bitrate);
	
      }
    }
    
    // subtitles
    
    i=0;
    for(n=0; n<TC_MAX_AUD_TRACKS; ++n) {
      if(ipipe.probe_info->track[n].attribute & PACKAGE_SUBTITLE) ++i; 
    }
    
    if(i) printf("detected (%d) subtitle(s)\n", i);

    // P-units

    if(ipipe.probe_info->unit_cnt)  
      printf("detected (%d) presentation unit(s) (SCR reset)\n", ipipe.probe_info->unit_cnt+1);

    // no audio

    if(ipipe.probe_info->num_tracks==0) 
      printf("%18s %s\n", "no audio track:", "use \"null\" import module for audio");
    
    
    //encoder bitrate infos (DVD only)
    
    if(stream_magic == TC_MAGIC_DVD_PAL || stream_magic == TC_MAGIC_DVD_NTSC ||stream_magic == TC_MAGIC_DVD) {
      enc_bitrate((long)ceil(ipipe.probe_info->fps*ipipe.probe_info->time), ipipe.probe_info->fps, bitrate, EXE, 0);
    } else {
      
      if(ipipe.probe_info->frames > 0)
	printf("%18s %ld frames, frame_time=%ld msec\n", "length:", ipipe.probe_info->frames, frame_time);
    }
    
    if(ipipe.fd_in != STDIN_FILENO) close(ipipe.fd_in);
    
    return(0);
}

