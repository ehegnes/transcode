/*
 *  bitrate.c
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

static int cd[] = {650, 700, 1300, 1400};

int enc_bitrate(long frames, double fps, int abit, char *s, int cdsize)
{
    
    long  time;

    double audio, video, bitrate;

    int n;

    if(frames<0 || fps <0.0) return(-1);

    time = (long) (frames / fps);
    audio = (1.0 * abit * time)/(8*1024);    // 128/8

    printf("[%s] V: %ld frames, %ld sec @ %.3f fps\n", s, frames, time, fps);
    printf("[%s] A: %.2f MB @ %d kbps\n", s, audio, abit);

    // find the optimum bitrate for a 1/2 CD burn

    if(cdsize) {
      video = cdsize - audio;  
      bitrate = (video/time) * ((double) (1024 * 1024 * 8))/ 1000; 

      if(video>0) printf("USER CDSIZE: %4d MB | V: %6.1f MB @ %.1f kbps\n", cdsize, video, bitrate);
    } else {
    for(n=0; n<4; ++n) {

      video = cd[n] - audio;  
      bitrate = (video/time) * ((double) (1024 * 1024 * 8))/ 1000; 

      if(video>0) printf("[%s] CD: %4d MB | V: %6.1f MB @ %.1f kbps\n", s, cd[n], video, bitrate);

    }
    }

    return(0);
}

