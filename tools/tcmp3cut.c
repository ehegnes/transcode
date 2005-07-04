/*
 *  tcmp3cut.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
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

#include "transcode.h"

#include <sys/errno.h>

#include "framebuffer.h"

int tc_get_mp3_header(unsigned char* hbuf, int* chans, int* srate, int *bitrate);
#define tc_decode_mp3_header(hbuf)  tc_get_mp3_header(hbuf, NULL, NULL, NULL)

#define EXE "tcmp3cut"

int verbose=TC_QUIET;

#define CHUNK_SIZE 4096

static int min=0, max=0;



/* ------------------------------------------------------------ 
 *
 * print a usage/version message
 *
 * ------------------------------------------------------------*/

void version()
{
    printf("%s (%s v%s) (C) 2003 Tilmann Bitterberg\n", EXE, PACKAGE, VERSION);
}

void usage(int status)
{
  version();

  fprintf(stderr,"\nUsage: %s [options]\n", EXE);

  fprintf(stderr,"\t -i file           input file name\n");
  fprintf(stderr,"\t -o base           output file name base\n");
  fprintf(stderr,"\t -e r[,b[,c]]      MP3 audio stream parameter [%d,%d,%d]\n", RATE, BITS, CHANNELS);
  fprintf(stderr,"\t -t c1[,c2[,.]]    cut points in milliseconds\n");
  fprintf(stderr,"\t -d mode           verbosity mode\n");
  fprintf(stderr,"\t -v                print version\n");
  
  exit(status);
  
}


/* ------------------------------------------------------------ 
 *
 * scan stream
 *
 * ------------------------------------------------------------*/

#define MAX_SONGS 50

int main(int argc, char *argv[])
{


  int fd=-1;
  FILE *out=NULL;
  
  int n=0, ch;
  char *name=NULL, *offset=NULL, *base=NULL;
  char outfile[1024];
  int cursong=0;

  int bytes_read;

  uint64_t total=0;

  int a_rate=RATE, a_bits=BITS, chan=CHANNELS;
  int songs[MAX_SONGS];
  int numsongs=0;
 
  int on=1;

  char buffer[CHUNK_SIZE];
  
  uint32_t i=0;

  if (argc<2)
      usage(EXIT_SUCCESS);

  while ((ch = getopt(argc, argv, "o:e:i:t:d:v?h")) != -1) {
    
    switch (ch) {
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

    case 't':
      if(optarg[0]=='-') usage(EXIT_FAILURE);

      offset = optarg;
      i=0;
      songs[i]=atoi(offset);
      while ((offset = strchr(offset,','))) {
	  offset++;
	  i++;
	  songs[i]=atoi(offset);
      }
      numsongs=i+1;
      break;

    case 'o':
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      base = optarg;
      break;
    case 'v': 
      version();
      exit(0);
      break;
      
    case 'h':
      usage(EXIT_SUCCESS);
    default:
      usage(EXIT_FAILURE);
    }
  }

  printf("Got %d songs:\n", numsongs);
  for (n=0; n<numsongs; n++)
      printf("%d : %d\n", n, songs[n]);
  

  if (!name) {
      fprintf(stderr, "No filename given\n");
      exit (EXIT_FAILURE);
  }

  if ( (fd = open(name, O_RDONLY)) < 0) {
      perror("open()");
      return -1;
  }

  if ( (fd = open(name, O_RDONLY)) < 0) {
      perror("open()");
      return -1;
  }

  snprintf(outfile, sizeof(outfile), "%s-%04d.mp3", base, cursong);
  if ( (out = fopen(outfile, "w")) == NULL) {
      perror ("fopen() output");
      return -1;
  }
  
  if(1) {
    
      char header[4];
      int framesize = 0;
      int chunks = 0;
      int srate=0 , chans=0, bitrate=0;
      off_t pos=0;
      double ms = 0;

      min = 500;
      max = 0;
      
      pos = lseek(fd, 0, SEEK_CUR);
      // find mp3 header
      while ((total += read(fd, header, 4))) {
	  if ( (framesize = tc_get_mp3_header (header, &chans, &srate, &bitrate)) > 0) {
	      fwrite (header, 4, 1,out);
	      ms += (framesize*8)/(bitrate);
	      break;
	  }
	  pos++;
	  lseek(fd, pos, SEEK_SET);
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
	  if ( (bytes_read = read(fd, buffer, framesize-4)) != framesize-4) { 
	      on = 0;
	  } else {
	      total += bytes_read;
	      fwrite (buffer, bytes_read, 1,out);
	      while ((total += read(fd, header, 4))) {
		  
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

		      ms += (framesize*8)/(bitrate);
		      // close/open

		      if (ms>=songs[cursong]) {
			  fclose(out);
			  cursong++;
			  if (cursong>numsongs)
			      break;
			  snprintf(outfile, sizeof(outfile), "%s-%04d.mp3", base, cursong);
			  if ( (out = fopen(outfile, "w")) == NULL) {
			      perror ("fopen() output");
			      return -1;
			  }
		      }
		      fwrite (header, 4, 1,out);

		      ++chunks;
		      break;
		  }
	      }


	  }
      }
      fclose(out);
      close(fd);
      return(0);
  }
  
  
  fprintf(stderr, "[%s] unable to handle codec/filetype\n", EXE);
  
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
