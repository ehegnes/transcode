/*
 *  filter_astat.c
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

#define MOD_NAME    "filter_astat.so"
#define MOD_VERSION "v0.1.3 (2003-09-04)"
#define MOD_CAP     "audio statistics filter plugin"
#define MOD_AUTHOR  "Thomas Oestreich"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

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
#include "optstr.h"

static int min=0, max=0, bytes_per_sec;
static long total=0;
static int a_rate, a_bits, chan; 
static double fps, fmin, fmax, vol;
static char *file;

static void check (int v)
{
  
  if (v > max) {
    max = v;
  } else if (v < min) {
    min = v;
  }
  
  return;
}

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

static int is_optstr(char *a) {
    if (strlen(a)>4) if (strncmp(a,"help",4)==0) return 1;
    if (strchr(a, '=')) return 1;
    return 0;
}

int tc_filter(aframe_list_t *ptr, char *options)
{

  int n;

  short *s;

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
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, "Thomas Oestreich", "AE", "1");
      optstr_param (options, "file", "File to save the calculated volume rescale number to", "%s", ""); 
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

    /* extract file name */
    file = NULL;
    if(options!=NULL) {
      if (!is_optstr(options)) {
	file = strdup(options);
      } else  {
	file = malloc(1024);
	optstr_get(options, "file", "%[^:]", file);
      }
      if(verbose) 
	printf("[%s] saving audio scale value to '%s'\n", MOD_NAME, file);
    }

    fps=vob->fps;
    a_bits=vob->a_bits;
    a_rate=vob->a_rate;
    chan = vob->a_chan;
    return(0);
  }

  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  
  if(ptr->tag & TC_FILTER_CLOSE) {
    bytes_per_sec = a_rate * (a_bits/8) * chan; 
    
    //frames = (fps*((double)total)/bytes_per_sec);
    
    fmin = -((double) min)/SHRT_MAX;
    fmax =  ((double) max)/SHRT_MAX;
    
    if(min==0 || max == 0) exit(0);
    
    vol = (fmin<fmax) ? 1./fmax : 1./fmin;
    
    //    printf("[%s] audio frames=%.2f, estimated clip length=%.2f seconds\n", MOD_NAME, frames, frames/fps);
    printf("\n[%s] (min=%.3f/max=%.3f), normalize volume with \"-s %.3f\"\n", MOD_NAME, -fmin, fmax, vol);

    /* write scale value to file */
    if(file!=NULL) {
      FILE *fh;

      fh = fopen(file,"w");
      fprintf(fh,"%.3f\n",vol);
      fclose(fh);
      if(verbose)
	printf("[%s] wrote audio scale value to '%s'\n",MOD_NAME,file);
      free(file);
    }
    
    return(0);
  }
  
  //----------------------------------
  //
  // filter frame routine
  //
  //----------------------------------

  if(verbose & TC_STATS) printf("[%s] %s/%s %s %s\n", MOD_NAME, vob->mod_path, MOD_NAME, MOD_VERSION, MOD_CAP);
  
  // tag variable indicates, if we are called before
  // transcodes internal video/audo frame processing routines
  // or after and determines video/audio context
  
  if(ptr->tag & TC_PRE_S_PROCESS && ptr->tag & TC_AUDIO && !(ptr->attributes & TC_FRAME_IS_SKIPPED)) {
    
    total += (uint64_t) ptr->audio_size;
	  
    s=(short *) ptr->audio_buf;
    
    for(n=0; n<ptr->audio_size>>1; ++n) {
      check((int) (*s));
      s++;
    }
  }
  
  return(0);
}
