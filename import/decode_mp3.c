/*
 *  decode_mp3.c
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

#include "ioaux.h"
#include "tc.h"

#ifdef HAVE_LAME
#include "mpg123.h"
#endif

#define MP3_PCM_SIZE 1152
short buffer[MP3_PCM_SIZE<<2];
short ch1[MP3_PCM_SIZE], ch2[MP3_PCM_SIZE];


/* why there are decode_mp3 and decode_mp2 which are very similar?
 * It is possible that lame_decode_initfile() when looking for an MP3 syncbyte
 * finds an invalid one (esp. in broken mp3 streams). Thats why we use the
 * format argument to decide which syncword detection is to be done. The
 * syncword detection for mp2 also finds mp3 sync bytes but NOT the other way round.
 */

/* ------------------------------------------------------------ 
 *
 * decoder thread
 *
 * ------------------------------------------------------------*/

void decode_mp3(decode_t *decode)
{

#ifdef HAVE_LAME

  int samples=0, j, bytes, channels=0, i, padding=0, c;
  int verbose;

  mp3data_struct *mp3data;
  
  FILE *in_file;

  verbose = decode->verbose;

  // init decoder
  
  if((mp3data = malloc(sizeof(mp3data_struct)))==NULL) {
    fprintf(stderr, "(%s) out of memory", __FILE__);
    exit(1);
  }

  memset(mp3data, 0, sizeof(mp3data_struct));
  
  if(lame_decode_init()<0) {
    fprintf(stderr, "(%s) failed to init decoder", __FILE__);
    exit(1);
  }
  
  in_file = fdopen(decode->fd_in, "r");

  while (!(c = fgetc(in_file))) padding++;
  if (c != EOF) ungetc(c, in_file);

  samples=lame_decode_initfile(in_file, mp3data, 0x55);

  if (verbose)
    fprintf(stderr, "(%s) channels=%d, samplerate=%d Hz, bitrate=%d kbps, (%d)\n", __FILE__, mp3data->stereo, mp3data->samplerate, mp3data->bitrate, mp3data->framesize);

  if (decode->padrate > 0) {
    padding = (int)((double)padding / (double)decode->padrate * mp3data->samplerate)
      * mp3data->stereo * 2;
    memset(buffer, 0, sizeof(buffer));
    while (padding >= sizeof(buffer)) {
      if (tc_pwrite(decode->fd_out, (uint8_t *)buffer, sizeof(buffer)) < 0)
        import_exit(1);
      padding -= sizeof(buffer);
    }
    if (padding && tc_pwrite(decode->fd_out, (uint8_t *)buffer, padding) < 0)
      import_exit(0);
  }
  
  // decoder loop

  channels=mp3data->stereo;

  while((samples=lame_decode_fromfile(in_file, ch1, ch2, mp3data))>0) {
    
    //interleave data

    j=0;
    switch (channels) {
    case 1: // mono
      ac_memcpy (buffer, ch1, samples*sizeof(short));
      break;
    case 2: // stereo
    for(i=0; i < samples; i++) {
	  *(buffer+j+0) = ch1[i];
	  *(buffer+j+1) = ch2[i];
	  j+=2;
      } 
      break;
    }
    
    bytes = samples * channels * sizeof(short);

    if (tc_pwrite(decode->fd_out, (uint8_t *) buffer, bytes) < 0)
      break; /* broken pipe */
  }

  import_exit(0);

#else  // HAVE_LAME
  fprintf(stderr, "(%s) no lame support available\n", __FILE__);
  import_exit(1);
#endif
  
}

void decode_mp2(decode_t *decode)
{
  
#ifdef HAVE_LAME

  int samples=0, j, bytes, channels=0, i;
  int verbose;
  
  mp3data_struct *mp3data;
  
  FILE *in_file;

  verbose = decode->verbose;

  // init decoder
  
  if((mp3data = malloc(sizeof(mp3data_struct)))==NULL) {
    fprintf(stderr, "(%s) out of memory", __FILE__);
    exit(1);
  }

  memset(mp3data, 0, sizeof(mp3data_struct));
  
  if(lame_decode_init()<0) {
    fprintf(stderr, "(%s) failed to init decoder", __FILE__);
    exit(1);
  }
  
  in_file = fdopen(decode->fd_in, "r");

  samples=lame_decode_initfile(in_file, mp3data, 0x50);

  if (verbose)
    fprintf(stderr, "(%s) channels=%d, samplerate=%d Hz, bitrate=%d kbps, (%d)\n", __FILE__, mp3data->stereo, mp3data->samplerate, mp3data->bitrate, mp3data->framesize);
  
  // decoder loop

  channels=mp3data->stereo;

  while((samples=lame_decode_fromfile(in_file, ch1, ch2, mp3data))>0) {
    
    //interleave data

    j=0;
    switch (channels) {
    case 1: // mono
      ac_memcpy (buffer, ch1, samples*sizeof(short));
      break;
    case 2: // stereo
    for(i=0; i < samples; i++) {
	  *(buffer+j+0) = ch1[i];
	  *(buffer+j+1) = ch2[i];
	  j+=2;
      } 
      break;
    }
    
    bytes = samples * channels * sizeof(short);

    if (tc_pwrite(decode->fd_out, (uint8_t *) buffer, bytes) < 0)
      break; /* broken pipe */
  }

  import_exit(0);

#else  // HAVE_LAME
  fprintf(stderr, "(%s) no lame support available\n", __FILE__);
  import_exit(1);
#endif

}
  

