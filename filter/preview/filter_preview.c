/*
 *  filter_preview.c
 *
 *  Copyright (C) Thomas �streich - December 2001
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

#define MOD_NAME    "filter_preview.so"
#define MOD_VERSION "v0.1.4 (2002-10-08)"
#define MOD_CAP     "xv/sdl/gtk preview plugin"
#define MOD_AUTHOR  "Thomas Oestreich"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "display.h"
#include "filter_preview.h"

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

static char buffer[128];
static int size=0;
static int use_secondary_buffer=0;
static char *undo_buffer = NULL;

static int preview_delay=0;

vob_t *vob=NULL;

/* global variables */

static dv_player_t *dv_player = NULL;

dv_player_t *dv_player_new(void) 
{
    dv_player_t *result;
  
    if(!(result = calloc(1,sizeof(dv_player_t)))) goto no_mem;
    if(!(result->display = dv_display_new())) goto no_display;
    
    return(result);

 no_display:
    free(result);
    result = NULL;
 no_mem:
    return(result);
} // dv_player_new


/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

int tc_filter(vframe_list_t *ptr, char *options)
{

  int pre=0, vid=0;

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
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRYO", "1");
  }

  //----------------------------------
  //
  // filter init
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_INIT) {

    int w, h;
    
    if((vob = tc_get_vob())==NULL) return(-1);
    
    // filter init ok.
    
    if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);
    
    if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);

    snprintf(buffer, sizeof(buffer), "%s-%s", PACKAGE, VERSION);
    
    if(dv_player != NULL) return(-1);
    if(!(dv_player = dv_player_new())) return(-1);
    
    //init filter

    dv_player->display->arg_display=0;

    if(options!=NULL) {
      if(strcasecmp(options,"help")==0) return -1;
      if(strcasecmp(options,"gtk")==0) dv_player->display->arg_display=1;
      if(strcasecmp(options,"sdl")==0) dv_player->display->arg_display=3;
      if(strcasecmp(options,"xv")==0)  dv_player->display->arg_display=2;
    }

    w = tc_x_preview;
    h = tc_y_preview;
    
    if(verbose) printf("[%s] preview window %dx%d\n", MOD_NAME, w, h);
    
    switch(vob->im_v_codec) {
      
    case CODEC_RGB:
      
      if(!dv_display_init(dv_player->display, 0, NULL, 
			  w, h, e_dv_color_rgb,
			  buffer, buffer)) return(-1);
      
      size = w * h * 3;
      break;
      
    case CODEC_YUV:
      
      if(!dv_display_init(dv_player->display, 0, NULL, 
			  w, h, e_dv_sample_420, 
			  buffer, buffer)) return(-1);
      
      size = w*h* 3/2;
      break;
    
    case CODEC_RAW_YUV:
    
      if(!dv_display_init(dv_player->display, 0, NULL, 
			  w, h, e_dv_sample_420, 
			  buffer, buffer)) return(-1);
      size = w*h* 3/2;

      use_secondary_buffer=1;

      break;

    default:
      fprintf(stderr, "[%s] codec not supported for preview\n", MOD_NAME);
      return(-1);
    }

    if ((undo_buffer = (char *) malloc (size)) == NULL) {
      fprintf(stderr, "[%s] codec not supported for preview\n", MOD_NAME);
      return (-1);
    }

    return(0);
  }
  
  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  
  if(ptr->tag & TC_FILTER_CLOSE) {

    if(size) dv_display_exit(dv_player->display);

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
  
  if(verbose & TC_STATS) printf("[%s] %s/%s %s %s\n", MOD_NAME, vob->mod_path, MOD_NAME, MOD_VERSION, MOD_CAP);
  
  // tag variable indicates, if we are called before
  // transcodes internal video/audo frame processing routines
  // or after and determines video/audio context
  
  pre = (ptr->tag & TC_POST_S_PROCESS)? 1:0;
  vid = (ptr->tag & TC_VIDEO)? 1:0;
  
  if(pre && vid) {
    
    //0.6.2 (secondaray buffer for pass-through mode)
    (use_secondary_buffer) ? tc_memcpy(dv_player->display->pixels[0], (char*) ptr->video_buf2, size) : tc_memcpy(dv_player->display->pixels[0], (char*) ptr->video_buf, size); 
    
    //display video frame
    dv_display_show(dv_player->display);
    
    //0.6.2
    usleep(preview_delay);
  }
  
  return(0);
}
