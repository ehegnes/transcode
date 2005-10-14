/*
 *  filter_32detect.c
 *
 *  Copyright (C) Thomas �streich - June 2001
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

#define MOD_NAME    "filter_32detect.so"
#define MOD_VERSION "v0.2.4 (2003-07-22)"
#define MOD_CAP     "3:2 pulldown / interlace detection plugin"
#define MOD_AUTHOR  "Thomas Oestreich"

#include "transcode.h"
#include "filter.h"
#include "optstr.h"

#include <inttypes.h>

// basic parameter

#define COLOR_EQUAL  10
#define COLOR_DIFF   30
#define THRESHOLD     9 

static int color_diff_threshold1[MAX_FILTER];  //=COLOR_EQUAL;
static int color_diff_threshold2[MAX_FILTER];  //=COLOR_DIFF;
static int chroma_diff_threshold1[MAX_FILTER]; //=COLOR_EQUAL/2;
static int chroma_diff_threshold2[MAX_FILTER]; //=COLOR_DIFF/2;

static int force_mode=0;
static int threshold[MAX_FILTER];              //=THRESHOLD; 
static int chroma_threshold[MAX_FILTER];       //=THRESHOLD/2; 
static int show_results[MAX_FILTER];           //=0;

static int pre[MAX_FILTER];	// = 0;

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

static inline int sq( int d ) { return d * d; }

static void help_optstr(void) 
{
   printf ("[%s] (%s) help\n", MOD_NAME, MOD_CAP);
   printf ("* Overview\n");
   printf("    This filter checks for interlaced video frames.\n");
   printf("    Subsequent de-interlacing with transcode can be enforced with 'force_mode' option\n");

   printf ("* Options\n");
   printf ("   'threshold' interlace detection threshold [%d]\n", THRESHOLD);
   printf ("   'chromathres' interlace detection chroma threshold [%d]\n", THRESHOLD/2);
   printf ("   'equal' threshold for equal colors [%d]\n", COLOR_EQUAL);
   printf ("   'chromaeq' threshold for equal chroma [%d]\n", COLOR_EQUAL/2);
   printf ("   'diff' threshold for different colors [%d]\n", COLOR_DIFF);
   printf ("   'chromadi' threshold for different colors [%d]\n", COLOR_DIFF/2);
   printf ("   'force_mode' set internal force de-interlace flag with mode -I N [0]\n");
   printf ("   'pre' run as pre filter [1]\n");
   printf ("   'verbose' show results [off]\n");
}

static int interlace_test(char *video_buf, int width, int height, int id, int instance, int thres, int eq, int diff)
{

    int j, n, off, block, cc_1, cc_2, cc, flag;

    uint16_t s1, s2, s3, s4;
    
    cc_1 = 0;
    cc_2 = 0;
    
    block = width;
	
    flag = 0;

    for(j=0; j<block; ++j) {

	off=0;
	
	for(n=0; n<(height-4); n=n+2) {

	    
	    s1 = (video_buf[off+j        ] & 0xff);
	    s2 = (video_buf[off+j+  block] & 0xff);
	    s3 = (video_buf[off+j+2*block] & 0xff);
	    s4 = (video_buf[off+j+3*block] & 0xff);
	    
	    if((abs(s1 - s3) < eq) &&
	       (abs(s1 - s2) > diff)) ++cc_1;
	    
	    if((abs(s2 - s4) < eq) &&
	       (abs(s2 - s3) > diff)) ++cc_2;
	    
	    off +=2*block;
	}
    }
    
    // compare results
    
    cc = (int)((cc_1 + cc_2)*1000.0/(width*height));
	       
    flag = (cc > thres) ? 1:0;
    

    if(show_results[instance]) fprintf(stderr, "(%d) frame [%06d]: (1) = %5d | (2) = %5d | (3) = %3d | interlaced = %s\n", instance, id, cc_1, cc_2, cc, ((flag)?"yes":"no")); 
    
    return(flag);
}

int tc_filter(frame_list_t *ptr_, char *options)
{
  vframe_list_t *ptr = (vframe_list_t *)ptr_;
  static vob_t *vob=NULL;

  int is_interlaced = 0;
  int instance = ptr->filter_id;
  
  //----------------------------------
  //
  // filter init
  //
  //----------------------------------
  
  
  if(ptr->tag & TC_FILTER_GET_CONFIG) {
      char buf[255];
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, "Thomas", "VRYMEO", "1");

      tc_snprintf(buf, sizeof(buf), "%d", THRESHOLD);
      optstr_param (options, "threshold", "Interlace detection threshold", "%d", buf, "0", "255");

      tc_snprintf(buf, sizeof(buf), "%d", THRESHOLD/2);
      optstr_param (options, "chromathres", "Interlace detection chroma threshold", "%d", buf, "0", "255");

      tc_snprintf(buf, sizeof(buf), "%d", COLOR_EQUAL);
      optstr_param (options, "equal", "threshold for equal colors", "%d", buf, "0", "255");

      tc_snprintf(buf, sizeof(buf), "%d", COLOR_EQUAL/2);
      optstr_param (options, "chromaeq", "threshold for equal chroma", "%d", buf, "0", "255");

      tc_snprintf(buf, sizeof(buf), "%d", COLOR_DIFF);
      optstr_param (options, "diff", "threshold for different colors", "%d", buf, "0", "255");

      tc_snprintf(buf, sizeof(buf), "%d", COLOR_DIFF/2);
      optstr_param (options, "chromadi", "threshold for different chroma", "%d", buf, "0", "255");

      optstr_param (options, "force_mode", "set internal force de-interlace flag with mode -I N", 
	      "%d", "0", "0", "5");

      optstr_param (options, "pre", "run as pre filter", "%d", "1", "0", "1");
      optstr_param (options, "verbose", "show results", "", "0");
      return (0);
  }

  if(ptr->tag & TC_FILTER_INIT) {
      
      if((vob = tc_get_vob())==NULL) return(-1);

      color_diff_threshold1[instance]  = COLOR_EQUAL;
      chroma_diff_threshold1[instance] = COLOR_EQUAL/2;
      color_diff_threshold2[instance]  = COLOR_DIFF;
      chroma_diff_threshold2[instance] = COLOR_DIFF/2;
      threshold[instance]              = THRESHOLD;
      chroma_threshold[instance]       = THRESHOLD/2;
      show_results[instance]           = 0;
      pre[instance]                    = 1;
      
      // filter init ok.
      
      if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);
      
      // process filter options:
      
      if (options != NULL) {
	  
	  if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);
	  
	  optstr_get (options, "threshold", "%d",  &threshold[instance]);
	  optstr_get (options, "chromathres", "%d",  &chroma_threshold[instance]);
	  optstr_get (options, "force_mode", "%d",  &force_mode);
	  optstr_get (options, "equal", "%d",  &color_diff_threshold1[instance]);
	  optstr_get (options, "chromaeq", "%d",  &chroma_diff_threshold1[instance]);
	  optstr_get (options, "diff", "%d",  &color_diff_threshold2[instance]);
	  optstr_get (options, "chromadi", "%d",  &chroma_diff_threshold2[instance]);
	  optstr_get (options, "pre", "%d",  &pre[instance]);

	  if (optstr_get (options, "verbose", "") >= 0) {
	      show_results[instance]=1;
	  }

	  if (optstr_get (options, "help", "") >= 0) {
	      help_optstr();
	  }
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
  // transcodes internal video/audo frame processing routines
  // or after and determines video/audio context
  
  if (!(ptr->tag & TC_VIDEO))
	  return (0);

  //if((ptr->tag & TC_PRE_M_PROCESS) && (ptr->tag & TC_VIDEO))  {
  if(((ptr->tag & TC_PRE_M_PROCESS  && pre[instance]) || 
	  (ptr->tag & TC_POST_M_PROCESS && !pre[instance])) && !(ptr->attributes & TC_FRAME_IS_SKIPPED)) {

    //if (ptr->tag & TC_PRE_M_PROCESS) fprintf(stderr, "32#%d pre (%d)\n", instance, ptr->id);
    //if (ptr->tag & TC_POST_M_PROCESS) fprintf(stderr, "32#%d post (%d)\n", instance, ptr->id);
    
    if(vob->im_v_codec==CODEC_RGB) {
	is_interlaced = interlace_test(ptr->video_buf, 3*ptr->v_width, ptr->v_height, ptr->id, instance, 
		threshold[instance], color_diff_threshold1[instance], color_diff_threshold2[instance]); 
    } else {
	is_interlaced += interlace_test(ptr->video_buf, ptr->v_width, ptr->v_height, ptr->id, instance,
		threshold[instance], color_diff_threshold1[instance], color_diff_threshold2[instance]); 
	is_interlaced += interlace_test(ptr->video_buf+ptr->v_width*ptr->v_height, ptr->v_width/2, ptr->v_height/2, ptr->id, instance,
		chroma_threshold[instance], chroma_diff_threshold1[instance], chroma_diff_threshold2[instance]); 
	is_interlaced += interlace_test(ptr->video_buf+ptr->v_width*ptr->v_height*5/4, ptr->v_width/2, ptr->v_height/2, ptr->id, instance,
		chroma_threshold[instance], chroma_diff_threshold1[instance], chroma_diff_threshold2[instance]); 
    }

    //force de-interlacing?
    if(force_mode && is_interlaced) {
	ptr->attributes  |= TC_FRAME_IS_INTERLACED;
	ptr->deinter_flag = force_mode;
    }

    //reset
    is_interlaced=0;

  }
  
  return(0);
}

