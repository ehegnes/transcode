/*
 *  filter_slowmo.c
 *
 *  Copyright (C) Thomas Östreich - August 2002
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

#define MOD_NAME    "filter_slowmo.so"
#define MOD_VERSION "v0.3 (2003-29-11)"
#define MOD_CAP     "very cheap slow-motion effect"
#define MOD_AUTHOR  "Tilmann Bitterberg"

/* -------------------------------------------------
 *
 * mandatory include files
 *
 *-------------------------------------------------*/

#include "transcode.h"
#include "framebuffer.h"
#include "optstr.h"

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

static void help_optstr(void) 
{
   printf ("[%s] (%s) help\n", MOD_NAME, MOD_CAP);
   printf ("* Overview\n");
   printf ("   This filter produces a simple slow-motion effect by\n");
   printf ("   duplicating certain frames. I have seen this effect\n");
   printf ("   on TV and despite its the simple algorithm it works\n");
   printf ("   quite well. The filter has no options.\n");
}

static int do_clone (int id)
{
    static int last = 0;
    if ((id) % 3 == 0) {
	last = 0;
	return 1;
    }
    if (last>0) {
	last--;
	return 0;
    }

    if (last == 0) {
	last = -1;
	return 1;
    }


    return 0;
}

int tc_filter(vframe_list_t *ptr, char *options)
{

  vob_t *vob=NULL;

  // API explanation:
  // ================
  //
  // (1) need more infos, than get pointer to transcode global 
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
  // (6) filter is last time with TC_FILTER_CLOSE flag set


  //----------------------------------
  //
  // filter init
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_INIT) {
    
    if((vob = tc_get_vob())==NULL) return(-1);
    
    // filter init ok.
    
    if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);
    
    if (options) {
	if (verbose) printf("[%s] options=%s\n", MOD_NAME, options);
	if (optstr_get(options, "help", "")>=0) help_optstr();
    }
    
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
  // filter read configure
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_GET_CONFIG) {

      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRYE", "1");
  }
  
  //----------------------------------
  //
  // filter frame routine
  //
  //----------------------------------

  // tag variable indicates, if we are called before
  // transcodes internal video/audo frame processing routines
  // or after and determines video/audio context
  //  1 <-
  //  2 <-
  //  3 = 2
  //  4 <-
  //  5 = 4
  //  6 <-
  //  7 <-
  //  8 = 7
  //  9 <-
  // 10 = 9
  // 11 <-
  // 12 <-
  // 13 = 12
  // 14 <-
  // 15 = 14

  if(ptr->tag & TC_PRE_S_PROCESS && ptr->tag & TC_VIDEO) {
    
    if(!(ptr->tag & TC_FRAME_WAS_CLONED) && do_clone(ptr->id))  {
	//fprintf(stderr, "cloning frame %d\n", ptr->id);
	ptr->attributes |= TC_FRAME_IS_CLONED;
    }
    
  }
  
  return(0);
}

