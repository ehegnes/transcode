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
int tc_get_mp3_header(unsigned char* hbuf, int* chans, int* srate, int *bitrate);
#define tc_decode_mp3_header(hbuf)  tc_get_mp3_header(hbuf, NULL, NULL, NULL)

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

static double frc_table[16] = {0,
			       NTSC_FILM, 24, 25, NTSC_VIDEO, 30, 50, 
			       (2*NTSC_VIDEO), 60,
			       1, 5, 10, 12, 15, 
			       0, 0};


void usage(int status)
{
  version(EXE);

  fprintf(stderr,"\nUsage: %s [options]\n", EXE);

  fprintf(stderr,"\t -i file           input file name [stdin]\n");
  fprintf(stderr,"\t -x codec          source codec\n");
  fprintf(stderr,"\t -e r[,b[,c]]      PCM audio stream parameter [%d,%d,%d]\n", RATE, BITS, CHANNELS);
  fprintf(stderr,"\t -f rate,frc       frame rate [%.3f][,frc]\n", PAL_FPS);
  fprintf(stderr,"\t -w num            estimate bitrate for num frames\n");
  fprintf(stderr,"\t -b bitrate        audio encoder bitrate kBits/s [%d]\n", ABITRATE);
  fprintf(stderr,"\t -c cdsize         user defined CD size in MB [0]\n");
  fprintf(stderr,"\t -d mode           verbosity mode\n");
  fprintf(stderr,"\t -v                print version\n");
  
  exit(status);
  
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

  int frc;

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
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      cdsize = atoi(optarg);
      
      break;

    case 'd': 
      
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      verbose = atoi(optarg);
      
      break;
      
    case 'e': 
      
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      
      if (3 != sscanf(optarg,"%d,%d,%d", &a_rate, &a_bits, &chan)) fprintf(stderr, "invalid pcm parameter set for option -e");
      
      if(a_rate > RATE || a_rate <= 0) {
	fprintf(stderr, "invalid pcm parameter 'rate' for option -e");
	usage(EXIT_FAILURE);
      }
      
      if(!(a_bits == 16 || a_bits == 8)) {
	fprintf(stderr, "invalid pcm parameter 'bits' for option -e");
	usage(EXIT_FAILURE);
      }
      
      if(!(chan == 0 || chan == 1 || chan == 2)) {
	fprintf(stderr, "invalid pcm parameter 'channels' for option -e");
	usage(EXIT_FAILURE);
      }
      
      break;
      
    case 'i': 
      
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      name = optarg;
      break;

    case 'x': 
      
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      codec = optarg;
      break;

    case 'f': 
      
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      n = sscanf(optarg,"%lf,%d", &fps, &frc);

      if (n == 2 && (frc > 0 && frc <= 0x10))
	  fps = frc_table[frc];

      if(fps<=0) {
	fprintf(stderr,"invalid frame rate for option -f\n");
	exit(1);
      }
      break;

    case 'w': 
      
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      bframes = atoi(optarg);      

      if(bframes <=0) {
	fprintf(stderr,"invalid parameter for option -w\n");
	exit(1);
      }
      break;

    case 'b': 
      
      if(optarg[0]=='-') usage(EXIT_FAILURE);
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
    usage(EXIT_FAILURE);
  }

  if(codec==NULL) codec="";

  // do not try to mess with the stream
  if(stream_stype!=TC_STYPE_STDIN) {
    
    if(file_check(name)) exit(1);
    
    if((ipipe.fd_in = open(name, O_RDONLY))<0) {
      perror("open file");
      exit(1);
    } 
    
    magic = fileinfo(ipipe.fd_in, 0);

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

  if(strcmp(codec,"mp3")==0 || magic == TC_MAGIC_MP3) {
    
      char header[4];
      int framesize = 0;
      int chunks = 0;
      int srate=0 , chans=0, bitrate=0;
      unsigned long bitrate_add = 0;
      off_t pos=0;
      double ms = 0;

      min = 500;
      max = 0;
      
      pos = lseek(ipipe.fd_in, 0, SEEK_CUR);
      // find mp3 header
      while ((total += read(ipipe.fd_in, header, 4))) {
	  if ( (framesize = tc_get_mp3_header (header, &chans, &srate, &bitrate)) > 0) {
	      break;
	  }
	  pos++;
	  lseek(ipipe.fd_in, pos, SEEK_SET);
      }
      printf("POS %lld\n", pos);

      // Example for _1_ mp3 chunk
      // 
      // fps       = 25
      // framesize = 480 bytes
      // bitrate   = 160 kbit/s == 20 kbytes/s == 20480 bytes/s == 819.20 bytes / frame
      //
      // 480 bytes = 480/20480 s/bytes = .0234 s = 23.4 ms
      //  
      //  ms = (framesize*1000*8)/(bitrate*1000);
      //                           why 1000 and not 1024?
      //  correct? yes! verified with "cat file.mp3|mpg123 -w /dev/null -v -" -- tibit

      while (on) {
	  if ( (bytes_read = read(ipipe.fd_in, buffer, framesize-4)) != framesize-4) { 
	      on = 0;
	  } else {
	      total += bytes_read;
	      while ((total += read(ipipe.fd_in, header, 4))) {
		  
		  //printf("%x %x %x %x\n", header[0]&0xff, header[1]&0xff, header[2]&0xff, header[3]&0xff);

		  if ( (framesize = tc_get_mp3_header (header, &chans, &srate, &bitrate)) < 0) {
		      fprintf(stderr, "[%s] corrupt mp3 file?\n", EXE);
		      on = 0;
		      break;
		  } else  {

		      /*
		      printf("Found new header (%d) (framesize = %d) chan(%d) srate(%d) bitrate(%d)\n", 
			  chunks, framesize, chans, srate, bitrate);
			  */

		      bitrate_add += bitrate;
		      check(bitrate);
		      ms += (framesize*8)/(bitrate);
		      ++chunks;
		      break;
		  }
	      }


	  }
      }
      printf("[%s] MPEG-1 layer-3 stream. Info: -e %d,%d,%d\n", 
	      EXE, srate, 16, chans);
      printf("[%s] Found %d MP3 chunks. Average bitrate is %3.2f kbps ", 
	      EXE, chunks, (double)bitrate_add/chunks);
      if (min != max) printf("(%d-%d)\n", min, max);
      else printf("(cbr)\n");

      printf("[%s] AVI overhead will be max. %d*(8+16) = %d bytes (%dk)\n", 
	      EXE, chunks, chunks*8+chunks*16, (chunks*8+chunks*16)/1024 );
      printf("[%s] Estimated time is %.0lf ms (%02d:%02d:%02d.%02d)\n", 
	      EXE, ms, 
	         (int)(ms/1000.0/60.0/60.0), 
	         (int)(ms/1000.0/60.0)%60, 
		 (int)(ms/1000)%60, 
		 (int)(ms)%(1000) );
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
  
  
// from mencoder
//----------------------- mp3 audio frame header parser -----------------------

static int tabsel_123[2][3][16] = {
   { {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0},
     {0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,0},
     {0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,0} },

   { {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,0},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0} }
};
static long freqs[9] = { 44100, 48000, 32000, 22050, 24000, 16000 , 11025 , 12000 , 8000 };

/*
 * return frame size or -1 (bad frame)
 */
int tc_get_mp3_header(unsigned char* hbuf, int* chans, int* srate, int *bitrate){
    int stereo, ssize, crc, lsf, mpeg25, framesize;
    int padding, bitrate_index, sampling_frequency;
    unsigned long newhead = 
      hbuf[0] << 24 |
      hbuf[1] << 16 |
      hbuf[2] <<  8 |
      hbuf[3];


#if 1
    // head_check:
    if( (newhead & 0xffe00000) != 0xffe00000 ||  
        (newhead & 0x0000fc00) == 0x0000fc00){
	//fprintf( stderr, "[%s] head_check failed\n", EXE);
	return -1;
    }
#endif

    if((4-((newhead>>17)&3))!=3){ 
      //fprintf( stderr, "[%s] not layer-3\n", EXE); 
      return -1;
    }

    if( newhead & ((long)1<<20) ) {
      lsf = (newhead & ((long)1<<19)) ? 0x0 : 0x1;
      mpeg25 = 0;
    } else {
      lsf = 1;
      mpeg25 = 1;
    }

    if(mpeg25)
      sampling_frequency = 6 + ((newhead>>10)&0x3);
    else
      sampling_frequency = ((newhead>>10)&0x3) + (lsf*3);

    if(sampling_frequency>8){
	fprintf( stderr, "[%s] invalid sampling_frequency\n", EXE);
	return -1;  // valid: 0..8
    }

    crc = ((newhead>>16)&0x1)^0x1;
    bitrate_index = ((newhead>>12)&0xf);
    padding   = ((newhead>>9)&0x1);
//    fr->extension = ((newhead>>8)&0x1);
//    fr->mode      = ((newhead>>6)&0x3);
//    fr->mode_ext  = ((newhead>>4)&0x3);
//    fr->copyright = ((newhead>>3)&0x1);
//    fr->original  = ((newhead>>2)&0x1);
//    fr->emphasis  = newhead & 0x3;

    stereo    = ( (((newhead>>6)&0x3)) == 3) ? 1 : 2;

    if(!bitrate_index){
      fprintf( stderr, "[%s] Free format not supported.\n", EXE);
      return -1;
    }

    if(lsf)
      ssize = (stereo == 1) ? 9 : 17;
    else
      ssize = (stereo == 1) ? 17 : 32;
    if(crc) ssize += 2;

    framesize = tabsel_123[lsf][2][bitrate_index] * 144000;
    if (bitrate) *bitrate = tabsel_123[lsf][2][bitrate_index];

    if(!framesize){
	fprintf( stderr, "[%s] invalid framesize/bitrate_index\n", EXE);
	return -1;  // valid: 1..14
    }

    framesize /= freqs[sampling_frequency]<<lsf;
    framesize += padding;

//    if(framesize<=0 || framesize>MAXFRAMESIZE) return FALSE;
    if(srate) *srate = freqs[sampling_frequency];
    if(chans) *chans = stereo;

    return framesize;
}

  
