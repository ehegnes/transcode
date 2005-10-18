/*
 *  filter_detectsilence.c
 *
 *  Copyright (C) Tilmann Bitterberg - July 2003
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

#define MOD_NAME    "filter_detectsilence.so"
#define MOD_VERSION "v0.0.1 (2003-07-26)"
#define MOD_CAP     "audio silence detection with tcmp3cut commandline generation"
#define MOD_AUTHOR  "Tilmann Bitterberg"

#include "transcode.h"
#include "filter.h"
#include "optstr.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

static int a_rate, a_bits, chan; 


/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

#define SILENCE_FRAMES  4
#define MAX_SONGS      50

int tc_filter(frame_list_t *ptr_, char *options)
{
  aframe_list_t *ptr = (aframe_list_t *)ptr_;
  int n;
  short *s;
  int sum;
  double p;
  static int zero=0;
  static int next=0;
  static int songs[MAX_SONGS];
  char cmd[1024];

  vob_t *vob=NULL;

  if(ptr->tag & TC_FILTER_GET_CONFIG) {
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, "Tilmann Bitterberg", "AE", "1");
  }

  //----------------------------------
  //
  // filter init
  //
  //----------------------------------

  if(ptr->tag & TC_FILTER_INIT) {
    int i;
    
    if((vob = tc_get_vob())==NULL) return(-1);
    
    // filter init ok.
    
    if(verbose) tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);

    a_bits=vob->a_bits;
    a_rate=vob->a_rate;
    chan = vob->a_chan;

    for (i=0; i<MAX_SONGS; i++){
      songs[i]=-1;
    }
    return(0);
  }

  //----------------------------------
  //
  // filter close
  //
  //----------------------------------
  
  if(ptr->tag & TC_FILTER_CLOSE) {
    int i, res, len=0;
    if (next<1) return 0;

    if((vob = tc_get_vob())==NULL) return(-1);

    //len += sprintf(cmd, "tcmp3cut -i %s -o %s ", vob->audio_in_file, vob->audio_out_file?vob->audio_out_file:vob->audio_in_file);
    res = tc_snprintf(cmd, sizeof(cmd), "tcmp3cut -i in.mp3 -o base ");
    if (res < 0) {
      tc_log_error(MOD_NAME, "cmd buffer overflow");
      return(-1);
    }
    len += res;
    tc_log_info(MOD_NAME, "********** Songs ***********");
    if (next>0) {
      printf("%d", songs[0]);
      res = tc_snprintf(cmd+len, sizeof(cmd) - len, "-t %d", songs[0]);
      if (res < 0) {
        tc_log_error(MOD_NAME, "cmd buffer overflow");
        return(-1);
      }
      len += res;
    }
    for (i=1; i<next; i++) {
      printf(",%d", songs[i]);
      res = tc_snprintf(cmd+len, sizeof(cmd) - len, ",%d", songs[i]);
      if (res < 0) {
        tc_log_error(MOD_NAME, "cmd buffer overflow");
        return(-1);
      }
      len += res;
    }
    printf("\n");
    tc_log_info(MOD_NAME, "Execute: %s", cmd);
    
    return(0);
  }
  
  //----------------------------------
  //
  // filter frame routine
  //
  //----------------------------------

  // tag variable indicates, if we are called before
  // transcodes internal video/audo frame processing routines
  // or after and determines video/audio context
  
  if(ptr->tag & TC_PRE_S_PROCESS && ptr->tag & TC_AUDIO) {
    
    s=(short *) ptr->audio_buf;
    p=0.0;
    
    for(n=0; n<ptr->audio_size>>1; ++n) {
      double d=(double)(*s++)/((double)SHRT_MAX*1.0);
      p += (d>0.0?d:-d);
    }
    
   sum = (int)p; 

   // Is this frame silence?
   if (sum == 0) zero++;

   // if we have found SILENCE_FRAMES in a row, there must be a song change.
 
   if (zero>=SILENCE_FRAMES && sum) {
     
     // somwhere in the middle of silence, the +3 is just a number
     int tot = (ptr->id - zero)*ptr->audio_size;
     tot *= 8;
     tot /= (a_rate*chan*a_bits/1000);
     
     songs[next++] = tot;

     if (next > MAX_SONGS) {
       tc_log_error(MOD_NAME, "Cannot save more songs");
       return (-1);
     }

     //printf("\nCut at time %d frame %d\n", tot, ptr->id - (zero+2)/2);
     zero=0;
   }

   //printf("%5d: sum (%07.3f)\n", ptr->id, p);
  }
  
  return(0);
}

// vim: sw=2
