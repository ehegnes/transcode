/*
 *  tcdecode.c
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
#include <limits.h>

#include "transcode.h"
#include "ioaux.h"
#include "tc.h"

#define EXE "tcdecode"

extern long fileinfo(int fd, int skip);

static int verbose=TC_QUIET;

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

  fprintf(stderr,"\nUsage: %s [options]\n", EXE);

  fprintf(stderr,"\t -i file           input file [stdin]\n");
  fprintf(stderr,"\t -x codec          source codec (required)\n");
  fprintf(stderr,"\t -g wxh            stream frame size [autodetect]\n");
  fprintf(stderr,"\t -y codec          output raw stream codec [rgb]\n");
  fprintf(stderr,"\t -Q mode           decoding quality (0=fastest-5=best) [%d]\n", VQUALITY);
  fprintf(stderr,"\t -d mode           verbosity mode\n");
  fprintf(stderr,"\t -s c,f,r          audio gain for ac3 downmixing [1,1,1]\n");
  fprintf(stderr,"\t -A n              A52 decoder flag [0]\n");
  fprintf(stderr,"\t -C s,e            decode only from start to end ((V) frames/(A) bytes) [all]\n");
  fprintf(stderr,"\t -v                print version\n");

  exit(status);
  
}


/* ------------------------------------------------------------ 
 *
 * universal decode thread frontend 
 *
 * ------------------------------------------------------------*/

int main(int argc, char *argv[])
{

    info_t ipipe;

    long 
	stream_stype = TC_STYPE_UNKNOWN, 
	stream_magic = TC_MAGIC_UNKNOWN, 
	stream_codec = TC_CODEC_UNKNOWN;

    int width=0, height=0;

    int ch, done=0, quality=VQUALITY, a52_mode=0;
    char *codec=NULL, *name=NULL, *format="rgb";

    //proper initialization
    memset(&ipipe, 0, sizeof(info_t));
    // default ac3_gain
    ipipe.ac3_gain[0] = ipipe.ac3_gain[1] = ipipe.ac3_gain[2] = 1.0;
    ipipe.frame_limit[0]=0; 
    ipipe.frame_limit[1]=LONG_MAX; 

    while ((ch = getopt(argc, argv, "Q:d:x:i:a:g:vy:s:C:A:?h")) != -1) {
	
	switch (ch) {
	    
	case 'i': 
	    
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  name = optarg;
	  break;


	case 'd': 
	    
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  verbose = atoi(optarg);
	  break;

	case 'Q': 
	    
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  quality = atoi(optarg);
	  break;

	case 'A': 
	    
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  a52_mode = atoi(optarg);
	  break;
		  
	case 'x': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  codec = optarg;
	  break;

	case 'y': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  format = optarg;
	  break;
	  
	case 'g': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  if (2 != sscanf(optarg,"%dx%d", &width, &height)) usage(EXIT_FAILURE);
	  break;
	  
	case 'v': 
	  version(EXE);
	  exit(0);
	  break;

	case 's': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  if (3 != sscanf(optarg,"%lf,%lf,%lf", &ipipe.ac3_gain[0], &ipipe.ac3_gain[1], &ipipe.ac3_gain[2])) usage(EXIT_FAILURE);
	  break;
	  
	case 'C': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  if (2 != sscanf(optarg,"%ld,%ld", &ipipe.frame_limit[0], &ipipe.frame_limit[1])) usage(EXIT_FAILURE);
 	  if (ipipe.frame_limit[0] >= ipipe.frame_limit[1])
	  {
  		fprintf(stderr,"Invalid -C options\n");
		usage(EXIT_FAILURE);
	  }
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
    
    // no autodetection yet
    if(codec==NULL) {
	fprintf(stderr, "error: invalid codec %s\n", codec);
	usage(EXIT_FAILURE);
    }
    
    // do not try to mess with the stream
    if(stream_stype!=TC_STYPE_STDIN) {
	
	if(file_check(name)) exit(1);
	
	if((ipipe.fd_in = open(name, O_RDONLY))<0) {
	    perror("open file");
	    exit(1);
	} 
	
	// try to find out the filetype
	
	stream_magic=fileinfo(ipipe.fd_in, 0);

	if(verbose) fprintf(stderr, "[%s] (pid=%d) %s\n", EXE, getpid(), filetype(stream_magic));
	
    } else ipipe.fd_in = STDIN_FILENO;
    
    // fill out defaults for info structure
    ipipe.fd_out = STDOUT_FILENO;
    
    ipipe.magic = stream_magic;
    ipipe.stype = stream_stype;
    ipipe.codec = stream_codec;

    ipipe.verbose = verbose;
    ipipe.quality = quality;

    ipipe.name=name;

    ipipe.width  = (width > 0)  ? width  : 0;
    ipipe.height = (height > 0) ? height : 0;

    ipipe.a52_mode = a52_mode;

    /* ------------------------------------------------------------ 
     *
     * output raw stream format
     *
     * ------------------------------------------------------------*/

    if(strcmp(format,"rgb")==0)  ipipe.format = TC_CODEC_RGB;

    if(strcmp(format,"yv12")==0) ipipe.format = TC_CODEC_YV12;

    if(strcmp(format,"yuy2")==0) ipipe.format = TC_CODEC_YUY2;
    
    if(strcmp(format,"pcm")==0)  ipipe.format = TC_CODEC_PCM;

    /* ------------------------------------------------------------ 
     *
     * codec specific section
     *
     * note: user provided values overwrite autodetection!
     *
     * ------------------------------------------------------------*/

    
    // MPEG2
    if(strcmp(codec,"mpeg2")==0) { 

	ipipe.codec = TC_CODEC_MPEG2;

	decode_mpeg2(&ipipe);
	done = 1;
    }

    
    
    // AC3
    if(strcmp(codec,"ac3")==0) {
	
	ipipe.codec = TC_CODEC_AC3;

	decode_ac3(&ipipe);
	done = 1;
    }

    // A52
    if(strcmp(codec,"a52")==0) {
	
	ipipe.codec = TC_CODEC_A52;

	decode_a52(&ipipe);
	done = 1;
    }

    // MP3
    if(strcmp(codec,"mp3")==0) {
	
	ipipe.codec = TC_CODEC_MP3;

	decode_mp3(&ipipe);
	done = 1;
    }


    // DV
    if(strcmp(codec,"dv")==0) {
	
	ipipe.codec = TC_CODEC_DV;

	decode_dv(&ipipe);
	done = 1;
    }


    // YV12
    if(strcmp(codec,"yv12")==0) { 
	
	ipipe.codec = TC_CODEC_YV12;

	decode_yuv(&ipipe);
	done = 1;
    }

    // AF6 audio
    if(strcmp(codec,"af6audio")==0) {
      
      ipipe.select = TC_AUDIO;

      decode_af6(&ipipe);
      done = 1;
    }
    
    // AF6 video
    if(strcmp(codec,"af6video")==0) {
      
      ipipe.select = TC_VIDEO;
      
      decode_af6(&ipipe);
      done = 1;
    }
    
    
    if(!done) {
	fprintf(stderr, "[%s] (pid=%d) unable to handle codec %s\n", EXE, getpid(), codec);
	exit(1);
    }
    
    if(ipipe.fd_in != STDIN_FILENO) close(ipipe.fd_in);
    
    return(0);
}

