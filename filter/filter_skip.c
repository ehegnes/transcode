/*
 *  filter_skip.c
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

#define MOD_NAME    "filter_skip.so"
#define MOD_VERSION "v0.0.1 (2001-11-27)"
#define MOD_CAP     "skip all listed frames"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_SKIP 32

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

char *get_next_range(char *name, char *_string)
{
  char *res, *string;

  int off=0;

  if(_string[0]=='\0') return(NULL);

  while(_string[off]==' ') ++off;

  string = &_string[off];

  if((res=strchr(string, ' '))==NULL) {
    strcpy(name, string);
    //return pointer to '\0'
    return(string+strlen(string));
  }
  
  memcpy(name, string, (int)(res-string));
  
  return(res+1);
}

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

static int ia[MAX_SKIP];
static int ib[MAX_SKIP];
static int cut=0, status=0;

int tc_filter(vframe_list_t *ptr, char *options)
{

  int pre=0;
  int i,n;

  char buf[64];

  char *offset;

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

  if(ptr->tag & TC_FILTER_GET_CONFIG) {
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, "Thomas Oestreich", "VAE", "1");
      optstr_param (options, "fstart1-fend1 [ fstart2-fend2 [ .. ] ]", "apply filter [start-end] frames", "%s", "");
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

    if(options == NULL) return(0);

    offset=options;
    if(verbose) printf("[%s] skipping frames ", MOD_NAME);
    for (n=0; n<MAX_SKIP; ++n) {

      memset(buf, 0, 64);

      if((offset=get_next_range(buf, offset))==NULL) break;
      
      i=sscanf(buf, "%d-%d", &ia[n], &ib[n]);
      
      if(i==2) {
	printf("%d-%d ", ia[n], ib[n]); 
	++cut;    
      } else {
	if(i<0) break;
      }
    }
    printf("\n"); 
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
  
  if(ptr->tag & TC_PRE_S_PROCESS) pre=1;
  
  if(pre==0) return(0);

  status=0;
  
  for(n=0; n<cut; ++n) {
      if(ptr->id >= ia[n] && ptr->id < ib[n]) {
	  status = 1;
	  goto cont;
      }
  }

  cont:
  
  if(status==1) ptr->attributes |= TC_FRAME_IS_SKIPPED;
  
  return(0);
}
