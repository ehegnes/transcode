/*
 *  aud_scan.c
 *
 *  Scans the audio track
 *
 *  Copyright (C) Tilmann Bitterberg - June 2003
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

// MP3

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
	fprintf( stderr, "[%s] head_check failed\n", __FILE__);
	return -1;
    }
#endif

    if((4-((newhead>>17)&3))!=3){ 
      fprintf( stderr, "[%s] not layer-3\n", __FILE__); 
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
	fprintf( stderr, "[%s] invalid sampling_frequency\n", __FILE__);
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
      fprintf( stderr, "[%s] Free format not supported.\n", __FILE__);
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
	fprintf( stderr, "[%s] invalid framesize/bitrate_index\n", __FILE__);
	return -1;  // valid: 1..14
    }

    framesize /= freqs[sampling_frequency]<<lsf;
    framesize += padding;

//    if(framesize<=0 || framesize>MAXFRAMESIZE) return FALSE;
    if(srate) *srate = freqs[sampling_frequency];
    if(chans) *chans = stereo;

    return framesize;
}
