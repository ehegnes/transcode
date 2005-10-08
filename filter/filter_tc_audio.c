/*
 *  filter_tc_audio.c
 *
 *  Copyright (C) Tilmann Bitterberg - August 2002
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

#define MOD_NAME    "filter_tc_audio.so"
#define MOD_VERSION "v0.1 (2002-08-13)"
#define MOD_CAP     "audio 23.9 -> 29.9 telecide filter"
#define MOD_AUTHOR  "Tilmann Bitterberg"

#include "transcode.h"
#include "filter.h"
#include "optstr.h"

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/


// telecine filter

int tc_filter(frame_list_t *ptr_, char *options)
{
  aframe_list_t *ptr = (aframe_list_t *)ptr_;
  static vob_t *vob=NULL;
  static char *audio_buf[2] = {NULL, NULL};
  double fch;
  int leap_bytes1, leap_bytes2;


  //----------------------------------
  //
  // filter init
  //
  //----------------------------------

  if (ptr->tag & TC_VIDEO)
	  return (0);

  if(ptr->tag & TC_FILTER_INIT) {
    
    if((vob = tc_get_vob())==NULL) return(-1);
    
    // filter init ok.
    
    if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);
    
    if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);

    /* thank god there is no write protection for variables */
    
    // if keep_ifps is supplied, do NOT change import_audio size 

    if (options && optstr_lookup (options, "keep_ifps")) {
      ;
    } else {

	// copied verbatim from transcode.c
	fch = vob->a_rate/NTSC_FILM;

	// bytes per audio frame
	vob->im_a_size = (int)(fch * (vob->dm_bits/8) * vob->dm_chan);
	vob->im_a_size =  (vob->im_a_size>>2)<<2;

	// rest:
	fch *= (vob->dm_bits/8) * vob->dm_chan;

	leap_bytes1 = TC_LEAP_FRAME * (fch - vob->im_a_size);
	leap_bytes2 = - leap_bytes1 + TC_LEAP_FRAME * (vob->dm_bits/8) * vob->dm_chan;
	leap_bytes1 = (leap_bytes1 >>2)<<2;
	leap_bytes2 = (leap_bytes2 >>2)<<2;

	if(leap_bytes1<leap_bytes2) {
	  vob->a_leap_bytes = leap_bytes1;
	} else {
	  vob->a_leap_bytes = -leap_bytes2;
	  vob->im_a_size += (vob->dm_bits/8) * vob->dm_chan;
	}
    }

    if (!audio_buf[0] && !audio_buf[1]) {
	audio_buf[0] = malloc (SIZE_PCM_FRAME);
	audio_buf[1] = malloc (SIZE_PCM_FRAME);
	if (!audio_buf[0] || !audio_buf[1]) {
	    fprintf(stderr, "[%s] [%s:%d] malloc failed\n", MOD_NAME, __FILE__, __LINE__);
	    return (-1);
	}
    }
    if (verbose & TC_DEBUG)
      printf("[%s] changing audio bufsize (%d) -> (%d)\n", MOD_NAME, vob->im_a_size, vob->ex_a_size);
    
    return(0);
  }

  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  if(ptr->tag & TC_FILTER_CLOSE) {
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

  /* pass-through */
  if (ptr->id == 0)
      return 0;

  /* in:      T1 B1 | T2 B2 | T3 B3 | T4 B4 */
  /* out: T1 B1 | T2 B2 | T2 B3 | T3 B4 | T4 B4 */

  // rearrange the audio buffers, do not add or delete data
  // 4*8008 -> 5*6408

  /* XXX: Is this really working??  appearantly -- tibit*/
  if(ptr->tag & TC_POST_S_PROCESS && ptr->tag & TC_AUDIO) {
    int mod = ptr->id % 4;
    int ex = vob->ex_a_size; // 6408
    //    int diff = im - ex;
    int diff = ex/4;

    switch (mod) {
      case 1:
	  ac_memcpy (audio_buf[0], ptr->audio_buf+ex, 1*diff);
	  ptr->audio_size=ex;
	break;
      case 2:
	  ac_memcpy (audio_buf[0]+1*diff , ptr->audio_buf           , ex-1*diff);
	  ac_memcpy (audio_buf[1]        , ptr->audio_buf+ex-1*diff , 2*diff);
	  ac_memcpy (ptr->audio_buf      , audio_buf[0]             , ex);
	  ptr->audio_size=ex;
	break;
      case 3:
	  ac_memcpy (audio_buf[1]+2*diff , ptr->audio_buf          , ex-2*diff);
	  ac_memcpy (audio_buf[0]        , ptr->audio_buf+ex-2*diff, 3*diff);
	  ac_memcpy (ptr->audio_buf      , audio_buf[1]            , ex);
	  ptr->audio_size=ex;
	break;
      case 0:
	if (!(ptr->attributes & TC_FRAME_WAS_CLONED)) {
	    ptr->attributes |= TC_FRAME_IS_CLONED;

	    if (verbose & TC_DEBUG)
	      printf("[A] frame cloned (%d)\n", ptr->id);

	    ac_memcpy (audio_buf[0]+3*diff , ptr->audio_buf          , ex-3*diff);
	    ac_memcpy (audio_buf[1]        , ptr->audio_buf+ex-3*diff, 4*diff);
	    ac_memcpy (ptr->audio_buf      , audio_buf[0]            , ex);
	    ptr->audio_size=ex;
	} else {
	  //	    ac_memcpy (audio_buf[1]+4*diff , ptr->audio_buf          , ex-4*diff);
	    ac_memcpy (ptr->audio_buf      , audio_buf[1]            , ex);
	    ptr->audio_size=ex;
	  
	}
	break;
    }
    
  }
  
  return(0);
}
