/**
 *  @file filter_null.c Demo filter
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

/*
 * ChangeLog:
 * v0.2 (2003-09-04)
 *
 * v0.3 (2005-01-02) Thomas Wehrspann
 *    -Documentation added
 *    -New help function
 *    -optstr_filter_desc now returns
*      the right capability flags
 */

/// Name of the filter
#define MOD_NAME    "filter_null.so"

/// Version of the filter
#define MOD_VERSION "v0.3 (2005-01-02)"

/// A short description
#define MOD_CAP     "demo filter plugin; does nothing"

/// Author of the filter plugin
#define MOD_AUTHOR  "Thomas Östreich, Thomas Wehrspann"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/* -------------------------------------------------
 *
 * mandatory include files
 *
 *-------------------------------------------------*/

#include "transcode.h"
#include "framebuffer.h"
#include "filter.h"
#include "optstr.h"


/**
 * Help text.
 * This function prints out a small description of this filter and
 * the command-line options when the "help" parameter is given
 *********************************************************/
static void help_optstr(void)
{
  printf ("[%s] help : * Overview                                                          \n", MOD_NAME);
  printf ("[%s] help :     This exists for demonstration purposes only. It does NOTHING!   \n", MOD_NAME);
  printf ("[%s] help :                                                                     \n", MOD_NAME);
  printf ("[%s] help : * Options                                                           \n", MOD_NAME);
  printf ("[%s] help :         'help' Prints out this help text                            \n", MOD_NAME);
  printf ("[%s] help :                                                                     \n", MOD_NAME);
}


/**
 * Main function of a filter.
 * This is the single function interface to transcode. This is the only function needed for a filter plugin.
 * @param ptr     frame accounting structure
 * @param options command-line options of the filter
 *
 * @return 0, if everything went OK.
 *********************************************************/
int tc_filter(vframe_list_t *ptr, char *options)
{
  int pre=0, vid=0;

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
  // filter get config
  //
  //----------------------------------
  if(ptr->tag & TC_FILTER_GET_CONFIG) 
{
    // Valid flags for the string of filter capabilities:
    //  "V" :  Can do Video
    //  "A" :  Can do Audio
    //  "R" :  Can do RGB
    //  "Y" :  Can do YUV
    //  "4" :  Can do YUV422
    //  "M" :  Can do Multiple Instances
    //  "E" :  Is a PRE filter
    //  "O" :  Is a POST filter
    optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VARY4EO", "1");

    optstr_param (options, "help", "Prints out a short help", "", "0");
    return 0;
  }

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
    
    // Parameter parsing
    if (options)
      if (optstr_lookup (options, "help")) {
        help_optstr();
        return(0);
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
  // filter frame routine
  //
  //----------------------------------
  // tag variable indicates, if we are called before
  // transcodes internal video/audio frame processing routines
  // or after and determines video/audio context
  if(verbose & TC_STATS) {
    
    printf("[%s] %s/%s %s %s\n", MOD_NAME, vob->mod_path, MOD_NAME, MOD_VERSION, MOD_CAP);
    
    // tag variable indicates, if we are called before
    // transcodes internal video/audo frame processing routines
    // or after and determines video/audio context
    
    if(ptr->tag & TC_PRE_PROCESS) pre=1;
    if(ptr->tag & TC_POST_PROCESS) pre=0;
    
    if(ptr->tag & TC_VIDEO) vid=1;
    if(ptr->tag & TC_AUDIO) vid=0;
    
    printf("[%s] frame [%06d] %s %16s call\n", MOD_NAME, ptr->id, (vid)?"(video)":"(audio)", (pre)?"pre-process filter":"post-process filter");
    
  }
  
  return(0);
}
