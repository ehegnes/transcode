/*
 *  filter_cut.c
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

#define MOD_NAME    "filter_cut.so"
#define MOD_VERSION "v0.1.0 (2003-05-03)"
#define MOD_CAP     "encode only listed frames"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "optstr.h"

// do the mod/step XXX

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

#include "../libioaux/framecode.h"

extern int max_frame_buffer;


/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/


static void help_optstr(void) 
{
    printf ("[%s] (%s) help\n", MOD_NAME, MOD_CAP);
    printf ("* Overview\n");
    printf ("    extract frame regions\n");
    printf ("* Options\n");
    printf ("    'HH:MM:SS.f-HH:MM:SS.f/step apply filter [start-end] frames [0-oo/1]\n");
}

int tc_filter(vframe_list_t *ptr, char *options)
{
  static struct fc_time *list;
  static double avoffset=1.0;
  char separator[] = " ";

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
    
    if(verbose & TC_DEBUG) printf("[%s] options=%s\n", MOD_NAME, options);

    if(options == NULL) return(0);
    else if (optstr_lookup (options, "help")) {
	help_optstr();
	return (0);
    } else {
	if( parse_fc_time_string( options, vob->fps, separator, verbose, &list ) == -1 ) {
	    help_optstr();
	    return (-1);
	}
    }
    avoffset = vob->fps/vob->ex_fps;

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
  // transcodes internal video/audio frame processing routines
  // or after and determines video/audio context
  
  if((ptr->tag & TC_PRE_S_PROCESS) && (ptr->tag & TC_VIDEO)) {

      // fc_frame_in_time returns the step frequency
      int ret = fc_frame_in_time(list, ptr->id);


      if (!(ret && !(ptr->id%ret)))
	  ptr->attributes |= TC_FRAME_IS_SKIPPED;

      // last cut region finished?
      if (tail_fc_time(list)->etf+max_frame_buffer < ptr->id)
	  tc_import_stop();
  } else if ((ptr->tag & TC_PRE_S_PROCESS) && (ptr->tag & TC_AUDIO)){
    int ret;
    int tmp_id;

    tmp_id = (int)((double)ptr->id*avoffset);
    ret = fc_frame_in_time(list, tmp_id);
    if (!(ret && !(tmp_id%ret))){
      ptr->attributes |= TC_FRAME_IS_SKIPPED;
    }
  }
  

  return(0);
}
