/*
 *  filter_decimate.c
 *
 *  Copyright (C) Marrq - July 2003
 *
 *  This file is part of transcode, a linux video stream processing tool
 *  Based on the excellent work of Donald Graft in Decomb and of
 *  Thanassis Tsiodras of transcode's decimate filter and Tilmann Bitterberg
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

#define MOD_NAME    "filter_modfps.so"
#define MOD_VERSION "v0.2 (2003-07-19)"
#define MOD_CAP     "plugin to modify framerate"
#define MOD_AUTHOR  "Marrq"

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
#include <unistd.h>
#include <inttypes.h>

#include "transcode.h"
#include "framebuffer.h"
#include "optstr.h"

static int show_results=0;

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

static int simple=1;
static double infps  = 29.97;
static int infrc  = 0;
// default settings for NTSC 29.97 -> 23.976
static int numSample=5;
static int numOut=4;

static double frc_table[16] = {0,
			       NTSC_FILM, 24, 25, NTSC_VIDEO, 30, 50, 
			       (2*NTSC_VIDEO), 60,
			       1, 5, 10, 12, 15, 
			       0, 0};
static void help_optstr()
{
    printf("[%s] (%s) help\n", MOD_NAME, MOD_CAP);
    printf ("* Overview\n");
    printf ("  This filter aims to allow transcode to alter the fps\n");
    printf ("  of video.  While one can reduce the fps to any amount,\n");
    printf ("  one can only increase the fps to at most twice the\n");
    printf ("  original fps\n");
    printf ("  There are two modes of operation, simple dropping and cloning,\n");
    printf ("  and one can decimate when lowering the framerate (I.E. allow\n");
    printf ("  3 out of every 5 frames will pick the three frames least like the\n");
    printf ("  frame following it.  This should yield best results, but won't\n");
    printf ("  work for all framerates\n");
    printf ("  Temp: decimate not implemented\n");
    printf ("* Options\n");
    printf ("\tsimple : whether simple drop/clone or decimation (0=decimation, other=simple) [%d]\n", simple);
    printf ("\tinfps : original fps (needed for simple operation) [%lf]\n",infps);
    printf ("\tinfrc : original frc (overwrite fps) [%d]\n",infrc);
    printf ("\texamine : number of frames to examine for decimation [%d]\n",numSample);
    printf ("\tallow : number of frames to allow per examined frames for decimation [%d]\n",numOut);
    printf ("\tverbose : 0 = not verbose, 1 is verbose [%d]\n",show_results);
}

int tc_filter(vframe_list_t * ptr, char *options)
{
    static vob_t *vob = NULL;
    static int frameCount = 0;

    static double outfps = 0.0;

    //----------------------------------
    //
    // filter init
    //
    //----------------------------------


    if (ptr->tag & TC_FILTER_INIT) {

	if ((vob = tc_get_vob()) == NULL)
	    return (-1);

	// filter init ok.
	if (options != NULL) {
	  if (optstr_lookup (options, "help")) {
	    help_optstr();
	  }
	  optstr_get (options, "verbose", "%d", &show_results);
	  optstr_get (options, "simple", "%d", &simple);
	  optstr_get (options, "infps", "%lf", &infps);
	  optstr_get (options, "infrc", "%d", &infrc);
	  optstr_get (options, "examine", "%d", &numSample);
	  optstr_get (options, "allow", "%d", &numOut);

	}

	if (verbose)
	    printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);

	if (simple){
	  outfps = vob->fps;
	  if (outfps > infps*2.0){
	    fprintf(stderr, "[%s] Error, desired output fps can not be greater\n",MOD_NAME);
	    fprintf(stderr, "[%s] than twice the input fps\n", MOD_NAME);
	    return -1;
	  }
	  if (infrc>0 && infrc < 16)
	      infps = frc_table[infrc];

	  return 0;
	} // else
	fprintf (stderr, "[%s] Error, general decimation not yet supported, sorry\n",MOD_NAME);
	return -1;
    }

    if (ptr->tag & TC_FILTER_GET_CONFIG){
      char buf[255];
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "", "1");

      sprintf(buf, "%d",simple);
      optstr_param(options,"simple","Simple drop/clone", "%d", buf, "0", "1");
      snprintf(buf, 128, "%lf", infps);
      optstr_param(options, "infps", "Original fps", "%f", buf, "MIN_FPS", "200.0");
      snprintf(buf, 128, "%lf", infrc);
      optstr_param(options, "infrc", "Original frc", "%d", buf, "0", "16");
      sprintf(buf, "%d", numSample);
      optstr_param(options,"examine", "How many frames to examine for decimation", "%d", buf, "1", "25");
      sprintf(buf, "%d", numOut);
      optstr_param(options, "allow", "How many frames to accept per examined", "%d", buf, "1", "25");
      sprintf(buf, "%d", verbose);
      optstr_param(options, "verbose", "run in verbose mode", "%d", buf, "0", "1");
      return 0;
    }

    //----------------------------------
    //
    // filter close
    //
    //----------------------------------


    if (ptr->tag & TC_FILTER_CLOSE) {
	
	return (0);
    }
    //----------------------------------
    //
    // filter frame routine
    //
    //----------------------------------


    // tag variable indicates, if we are called before
    // transcodes internal video/audio frame processing routines
    // or after and determines video/audio context

    if ((ptr->tag & TC_POST_PROCESS) && (ptr->tag & TC_VIDEO)) {
      if (simple){
        if (show_results){
          printf("infps=%02.4lf outfps=%02.4lf mycount=%5d id=%5d calc=%06.3lf\t",infps,outfps,frameCount,ptr->id,(double)frameCount*infps/outfps);
	} 
        if (infps < outfps){
	  // Notes; since we currently only can clone frames (and just clone
	  // them once, we can at most double the input framerate.
	  if ((double)frameCount*infps/outfps < (double)ptr->id){
	    if (show_results){
	      printf("FRAME IS CLONED");
	    }
	    ptr->attributes |= TC_FRAME_IS_CLONED;
	    ++frameCount;
	  }
	} else {
	  if ((double)frameCount*infps/outfps > (double)ptr->id){
	    if (show_results){
	      printf("FRAME IS SKIPPED");
	    }
	    ptr->attributes |= TC_FRAME_IS_SKIPPED;
	    --frameCount;
	  }
	}
	if (show_results){
	  printf("\n");
	}
	++frameCount;
	return(0);
      } // else

      fprintf(stderr, "[%s] Oppps, decimation not yet supported\n",MOD_NAME);
      return(-1);
    }

    return (0);
}
