/*
 *  audio_trans.c
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

#include "transcode.h"
#include "framebuffer.h"
#include "audio_trans.h"

/* ------------------------------------------------------------ 
 *
 * audio frame transformation auxiliary routines
 *
 * ------------------------------------------------------------*/

static short aclip(int v, int *cclip)
{
  if (v > SHRT_MAX) {
    ++ *cclip;
    return SHRT_MAX;
  } else if (v < -SHRT_MAX) {
    ++ *cclip;
    return -SHRT_MAX;
  }
  
  // ok
  return ((short) v);
}

void pcm_swap(char *buffer, int len)
{
  char *in, *out;

  int n;

  char tt;

  in  = buffer;
  out = buffer;

  for(n=0; n<len; n=n+2) {

    tt = *(in+1);
    *(out+1) = *in;
    *out = tt;
    
    in = in+2;
    out = out+2;
  }
}

/* ------------------------------------------------------------ 
 *
 * audio frame transformation
 *
 * audio buffer: ptr->audio_buf
 *
 * ------------------------------------------------------------*/


int process_aud_frame(vob_t *vob, aframe_list_t *ptr)
    
{
  short *s, *d, uu, uu1, uu2;
  int n;

  char *b;

  int trans=TC_FALSE;

  // check for pass-through mode
  
  if(vob->pass_flag & TC_AUDIO) return(0);
  
  // check if a frame transformation is requested:
  
  if (vob->volume > 0.0 || pcmswap) trans = TC_TRUE;

  if(vob->im_a_codec != CODEC_PCM) {

    if(trans) 
      tc_error("Oops, this version of transcode only supports PCM data for audio transformation"); 
    else 
      return(0);
  }
  
  // update frame
  ptr->a_codec = CODEC_PCM;
  
  //-----------------------------------------------------------------
  //
  // transformation: stretch audio given by vob->sync_ms
  //
  // flag: vob->sync_ms != 0

  if(vob->sync_ms!=0) {

    int bytes=0;

    //convert ms into PCM sample
    bytes = (vob->a_chan*vob->a_bits/16)*(int)(vob->a_rate/1000*vob->sync_ms);

    //bytes > 0 --> discard PCM data

    if(bytes>0) memcpy(ptr->audio_buf, ptr->audio_buf+bytes, ptr->audio_size-bytes);

    //bytes < 0 --> padd PCM data

    if(bytes<0) {
      memmove(ptr->audio_buf-bytes, ptr->audio_buf, ptr->audio_size);
      memset(ptr->audio_buf, 0, -bytes);
    }
    
    ptr->audio_size -=bytes;
    if(verbose & TC_DEBUG) printf("(%s) adjusted %d PCM samples (%d ms)\n", __FILE__, bytes/(vob->a_chan*vob->a_bits/16), vob->sync_ms);
    vob->sync_ms=0;
  }

  //-----------------------------------------------------------------
  //  
  // transformation: swap audio bytes
  //
  // flag: pcmswap
  
  if(pcmswap) pcm_swap(ptr->audio_buf, ptr->audio_size);

		       
  //-----------------------------------------------------------------
  //
  // transformation: rescale audio amplitude 
  //
  // flag: vob->volume>0

  if(vob->volume > 0.0) {
    
    s=(short *) ptr->audio_buf;
    
    for(n=0; n<ptr->audio_size>>1; ++n) {
      uu = aclip((int) (vob->volume * *s), &vob->clip_count);
      *s++ = uu;
    }
  }
  
  //-----------------------------------------------------------------
  //
  // transformation: convert 16 bit / 2 channel interleaved stereo to mono
  //
  // flag: vob->dm_chan = 1

  if(vob->dm_chan == 1 && vob->a_chan == 2 && vob->a_bits == 16) {
     
    s=(short *) ptr->audio_buf;
    d=s;
    
    for(n=0; n<ptr->audio_size>>2; ++n) {
      
      uu1 = (*s++>>1);
      uu2 = (*s++>>1);
      
      *d++ = uu1+uu2;
      
    }
    
    ptr->audio_size = ptr->audio_size>>1;
    
  }
  
  
  //-----------------------------------------------------------------
  //
  // transformation: convert 16 bit to 8 bit samples
  //
  // only 8 bit unsigned supported!
  //
  // flag: vob->dm_bits = 8
  
  if(vob->dm_bits == 8 && vob->a_bits == 16) {
    
    s = (short *) ptr->audio_buf;
    b = (char *) s;
    
    for(n=0; n<ptr->audio_size>>1; ++n) (*b++) = (char) ((*s++)/256+0x80);
    ptr->audio_size = ptr->audio_size>>1;
    
  }
  

  return(0);
}


