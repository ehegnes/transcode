/*
 *  filter_cpaudio.c
 *
 *  Copyright (C) William H Wittig - May 2003
 *  Still GPL, of course
 *
 *  This filter takes the audio signal on one channel and dupes it on
 *  the other channel.
 *  Only supports 16 bit stereo (for now)
 *
 * based on filter_null.c from transcode - orignal copyright below
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

#define MOD_NAME    "filter_cpaudio.so"
#define MOD_VERSION "v0.1 (2003-04-30)"
#define MOD_CAP     "copy audio filter plugin"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/* -------------------------------------------------
 *
 * mandatory include files
 *
 *-------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "transcode.h"
#include "framebuffer.h"
#include "optstr.h"

/*-------------------------------------------------
 * local utility functions
 *-------------------------------------------------*/

static void help_optstr(void)
{
   printf ("[%s] (%s) help\n", MOD_NAME, MOD_CAP);
   printf ("* Overview\n");
   printf ("    Copies audio from one channel to another\n");
   printf ("* Options\n");
   printf ("     'source=['l<eft>' or 'r<ight>']\n");
}

/*-------------------------------------------------
 * single function interface
 *-------------------------------------------------*/

int tc_filter(aframe_list_t *ptr, char *options)
{
  vob_t *vob=NULL;
  static int sourceChannel = 0;    // Init to left. '1' = right
   
  // API explanation:
  // ================
  //
  // (1) need more info, then get pointer to transcode global
  //     information structure vob_t as defined in transcode.h.
  //
  // (2) 'tc_get_vob' and 'verbose' are exported by transcode.
  //
  // (3) filter is called first time with TC_FILTER_INIT flag set.
  //
  // (4) make sure to exit immediately if context (video/audio) or
  //     placement of call (pre/post) is not compatible with the filters
  //     intended purpose, since the filter is called 4 times per frame.
  //
  // (5) see framebuffer.h for a complete list of frame_list_t variables.
  //
  // (6) filter is called last time with TC_FILTER_CLOSE flag set

  //----------------------------------
  // filter init
  //----------------------------------

  if (ptr->tag & TC_FILTER_INIT)
  {
    if ((vob = tc_get_vob()) == NULL)
        return (-1);

    if (vob->a_bits != 16)
    {
      fprintf (stderr, "This filter only works for 16 bit samples\n");
      return (-1);
    }
   
    if (options != NULL)
    {
      char srcChannel;
       
      optstr_get(options, "source", "%c", &srcChannel);

      if (srcChannel == 'l')
         sourceChannel = 0;
      else
         sourceChannel = 1;
    }

    if (options)
      if (optstr_lookup (options, "help"))
      {
        help_optstr();
      }

    // filter init ok.
   
    if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);
    if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);
   
    return(0);
  }

  //----------------------------------
  // filter close
  //----------------------------------

 
  if(ptr->tag & TC_FILTER_CLOSE)
  {
    return(0);
  }
 
  //----------------------------------
  // filter frame routine
  //----------------------------------

  // tag variable indicates, if we are called before
  // transcodes internal video/audo frame processing routines
  // or after and determines video/audio context
 
  if(ptr->tag & TC_POST_PROCESS && ptr->tag & TC_AUDIO)
  {
    int16_t* data = (int16_t *)ptr->audio_buf;
    int len = ptr->audio_size / 2; // 16 bits samples
    int i;
 
    // if(verbose) printf("[%s] Length: %d, Source: %d\n", MOD_NAME, len, sourceChannel);

    for (i = 0; i < len; i += 2) // Implicitly assumes even number of samples (e.g. l,r pairs)
    {
        if (sourceChannel == 0)
            data[i+1] = data[i];
        else
            data[i] = data[i+1];
    }
  }
  return(0);
}
