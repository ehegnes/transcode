/*
 *  tcscan.c
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

int enc_bitrate(long frames, double fps, int abit, char *s, int cdsize);

#undef TCF_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <errno.h>
#include <unistd.h>

#include "transcode.h"
#include "ioaux.h"
#include "tc.h"
#include "ac3.h"
#include "avilib.h"

#define EXE "tcscan"

static int verbose=TC_QUIET;

void import_exit(int code) 
{
  if(verbose & TC_DEBUG) import_info(code, EXE);
  exit(code);
}

#define CHUNK_SIZE 4096

static int min=0, max=0;

static void check (int v)
{
  
  if (v > max) {
    max = v;
  } else if (v < min) {
    min = v;
  }
  
  return;
}

/* ------------------------------------------------------------ 
 *
 * print a usage/version message
 *
 * ------------------------------------------------------------*/



void usage()
{
  version(EXE);

  fprintf(stderr,"\nUsage: %s [options]\n", EXE);

  fprintf(stderr,"\t -i file           input file name [stdin]\n");
  fprintf(stderr,"\t -x codec          source codec\n");
  fprintf(stderr,"\t -e r[,b[,c]]      PCM audio stream parameter [%d,%d,%d]\n", RATE, BITS, CHANNELS);
  fprintf(stderr,"\t -f rate           frame rate [%.3f]\n", PAL_FPS);
  fprintf(stderr,"\t -w num            estimate bitrate for num frames\n");
  fprintf(stderr,"\t -b bitrate        audio encoder bitrate kBits/s [%d]\n", ABITRATE);
  fprintf(stderr,"\t -c cdsize         user defined CD size in MB [0]\n");
  fprintf(stderr,"\t -d mode           verbosity mode\n");
  fprintf(stderr,"\t -v                print version\n");
  
  exit(0);
  
}


/* ------------------------------------------------------------ 
 *
 * scan stream
 *
 * ------------------------------------------------------------*/

int main(int argc, char *argv[])
{

  info_t ipipe;

  long stream_stype = TC_STYPE_UNKNOWN;

  long magic = TC_MAGIC_UNKNOWN;

  FILE *in_file;
  
  int n=0, ch;
  char *codec=NULL, *name=NULL;

  int bytes_per_sec, bytes_read, bframes=0;

  uint64_t total=0;

  int a_rate=RATE, a_bits=BITS, chan=CHANNELS;
 
  int on=1;
  short *s;

  char buffer[CHUNK_SIZE];
  
  double fps=PAL_FPS, frames, fmin, fmax, vol=0.0;

  int pseudo_frame_size=0;

  int ac_bytes=0, frame_size, bitrate=ABITRATE;
  
  float rbytes;
  
  uint32_t i=0, j=0;
  uint16_t sync_word = 0;
  int cdsize = 0;

  //proper initialization
  memset(&ipipe, 0, sizeof(info_t));
  
  while ((ch = getopt(argc, argv, "c:b:e:i:vx:f:d:w:?h")) != -1) {
    
    switch (ch) {
    case 'c':
      if(optarg[0]=='-') usage();
      cdsize = atoi(optarg);
      
      break;

    case 'd': 
      
      if(optarg[0]=='-') usage();
      verbose = atoi(optarg);
      
      break;
      
    case 'e': 
      
      if(optarg[0]=='-') usage();
      
      if (3 != sscanf(optarg,"%d,%d,%d", &a_rate, &a_bits, &chan)) fprintf(stderr, "invalid pcm parameter set for option -e");
      
      if(a_rate > RATE || a_rate <= 0) {
	fprintf(stderr, "invalid pcm parameter 'rate' for option -e");
	usage();
      }
      
      if(!(a_bits == 16 || a_bits == 8)) {
	fprintf(stderr, "invalid pcm parameter 'bits' for option -e");
	usage();
      }
      
      if(!(chan == 0 || chan == 1 || chan == 2)) {
	fprintf(stderr, "invalid pcm parameter 'channels' for option -e");
	usage();
      }
      
      break;
      
    case 'i': 
      
      if(optarg[0]=='-') usage();
      name = optarg;
      break;

    case 'x': 
      
      if(optarg[0]=='-') usage();
      codec = optarg;
      break;

    case 'f': 
      
      if(optarg[0]=='-') usage();
      fps = atof(optarg);      

      if(fps<=0) {
	fprintf(stderr,"invalid frame rate for option -f");
	exit(1);
      }
      break;

    case 'w': 
      
      if(optarg[0]=='-') usage();
      bframes = atoi(optarg);      

      if(bframes <=0) {
	fprintf(stderr,"invalid parameter for option -w");
	exit(1);
      }
      break;

    case 'b': 
      
      if(optarg[0]=='-') usage();
      bitrate = atoi(optarg);      

      if(bitrate < 0) {
	fprintf(stderr,"invalid bitrate for option -b");
	exit(1);
      }
      break;

      
    case 'v': 
      version(EXE);
      exit(0);
      break;
      
    case '?':
    case 'h':
    default:
      usage();
    }
  }
  
  /* ------------------------------------------------------------ 
   *
   * fill out defaults for info structure
   *
   * ------------------------------------------------------------*/

  // simple bitrate calculator
  if(bframes) {
    enc_bitrate(bframes, fps, bitrate, EXE, cdsize);
    exit(0);
  }
  
  // assume defaults
  if(name==NULL) stream_stype=TC_STYPE_STDIN;

  // no autodetection yet
  if(codec==NULL && name == NULL) {
    fprintf(stderr, "error: invalid codec %s\n", codec);
    usage();
    exit(1);
  }

  if(codec==NULL) codec="";

  // do not try to mess with the stream
  if(stream_stype!=TC_STYPE_STDIN) {
    
    if(file_check(name)) exit(1);
    
    if((ipipe.fd_in = open(name, O_RDONLY))<0) {
      perror("open file");
      exit(1);
    } 
    
    magic = fileinfo(ipipe.fd_in);

  } else ipipe.fd_in = STDIN_FILENO;
  
  
  /* ------------------------------------------------------------ 
   *
   * AC3 stream
   *
   * ------------------------------------------------------------*/

  if(strcmp(codec,"ac3")==0 || magic==TC_MAGIC_AC3) {

 
    for(;;) {
      
      for(;;) {
	
	if (p_read(ipipe.fd_in, buffer, 1) !=1) {
	  perror("ac3 sync frame scan failed");
	  goto ac3_summary;
	}
	
	sync_word = (sync_word << 8) + (uint8_t) buffer[0]; 
	
	++i;
	
	if(sync_word == 0x0b77) break;
      }

      i=i-2;
      
      if (p_read(ipipe.fd_in, buffer, 3) !=3) {
	perror("ac3 header read failed");
	goto ac3_summary;
      }
      
      if((frame_size = 2*get_ac3_framesize(buffer)) < 1) {
	printf("ac3 framesize %d invalid - frame broken?\n", frame_size);
	goto more;
      }
      
      //FIXME: I assume that a single AC3 frame contains 6kB PCM bytes
      
      rbytes = (float) SIZE_PCM_FRAME/1024/6 * frame_size;
      pseudo_frame_size = (int) rbytes;
      bitrate = get_ac3_bitrate(buffer);
      
      printf("[%05d] offset %06d (%06d) %04d bytes, bitrate %03d kBits/s\n", n++, i, j, frame_size, bitrate);
      
      // read the rest
      
      ac_bytes = frame_size-5;
      
      if(ac_bytes>CHUNK_SIZE) {
	fprintf(stderr, "Oops, no buffer space framesize %d\n", ac_bytes); 
	exit(1);
      }
      
      if ((bytes_read=p_read(ipipe.fd_in, buffer, ac_bytes)) != ac_bytes) {
	fprintf(stderr, "error reading ac3 frame (%d/%d)\n", bytes_read, ac_bytes);
	break;
      }
      
    more:
      
      i+=frame_size;
      j=i;
    }

  ac3_summary:
    
    vol = (double) (n * 1024 * 6)/4/RATE;
        
    fprintf(stderr, "[%s] valid AC3 frames=%d, estimated clip length=%.2f seconds\n", EXE, n, vol);
    
    return(0);
  }
  
  /* ------------------------------------------------------------ 
   *
   * PCM stream
   *
   * ------------------------------------------------------------*/

  if(strcmp(codec,"pcm")==0) {
    
      while(on) {
	  
	  if( (bytes_read = p_read(ipipe.fd_in, buffer, CHUNK_SIZE)) != CHUNK_SIZE) on = 0;
	  
	  total += (uint64_t) bytes_read;
	  
	  s=(short *) buffer;
	  
	  for(n=0; n<bytes_read>>1; ++n) {
	      check((int) (*s));
	      s++;
	  }
      }
      
      bytes_per_sec = a_rate * (a_bits/8) * chan; 
      
      frames = (fps*((double)total)/bytes_per_sec);
      
      fmin = -((double) min)/SHRT_MAX;
      fmax =  ((double) max)/SHRT_MAX;
  
      if(min==0 || max == 0) exit(0);
  
      vol = (fmin<fmax) ? 1./fmax : 1./fmin;
  
      printf("[%s] audio frames=%.2f, estimated clip length=%.2f seconds\n", EXE, frames, frames/fps);
      printf("[%s] (min/max) amplitude=(%.3f/%.3f), suggested volume rescale=%.3f\n", EXE, -fmin, fmax, vol);
      
      enc_bitrate((long) frames, fps, bitrate, EXE, cdsize);
      
      return(0);
  }
  
  /* ------------------------------------------------------------ 
   *
   * MPEG program stream
   *
   * ------------------------------------------------------------*/

  if(strcmp(codec, "mpeg2")==0 || strcmp(codec, "mpeg")==0 || strcmp(codec, "vob")==0 || magic==TC_MAGIC_VOB || magic==TC_MAGIC_M2V) {
    
    in_file = fdopen(ipipe.fd_in, "r");
    
    scan_pes(verbose, in_file);
    
    return(0);
  }

  /* ------------------------------------------------------------ 
   *
   * AVI
   *
   * ------------------------------------------------------------*/

  if(magic==TC_MAGIC_AVI || TC_MAGIC_WAV) {
      
      if(name!=NULL) AVI_scan(name);
      
      return(0);
  }
  
  
  fprintf(stderr, "[%s] unable to handle codec/filetype %s\n", EXE, codec);
  
  exit(1);
  
}
  
  
  
