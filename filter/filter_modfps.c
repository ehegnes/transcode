/*
 *  filter_modfps.c
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

// ----------------- Changes 
// 0.4 -> 0.5: marrq
//		initialize memory at runtime.
//		skip at PRE_S_ clone at POST_S_
//		fix counting/buffering bugs related to mode=1
// 0.3 -> 0.4: Tilmann Bitterberg
//             Fix a typo in the optstr_param printout and correct the filter
//             flags.
//             Fix a bug related to scanrange.

// ----------------- TODO
//	BUG
//	make this work with at least -c and hopefully filters cut and skip as well.
//	Currently, when using -c our counters are at 0, which means if we're
//	dropping frames, we choose to play catch up instead of dropping
//	and similar if we clone
//
//	feature:
//	Be able to do something fance when cloning.  I'm thinking of four things,
// 	1. interpolate field of frame which was cloned with frame that
//	   follows it.
//	2. evenly merge/average frames.
//	3. weighted average, so more intence luminance is stronger seen (think
//	   of phosphors on a TV
//	4. temporal weighted average

#define MOD_NAME    "filter_modfps.so"
#define MOD_VERSION "v0.5 (2003-08-01)"
#define MOD_CAP     "plugin to modify framerate"
#define MOD_AUTHOR  "Marrq"
//#define DEBUG 1

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
#include <math.h>

#include "transcode.h"
#include "framebuffer.h"
#include "optstr.h"

static int show_results=0;

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

static int mode=0;
static double infps  = 29.97;
static int infrc  = 0;
// default settings for NTSC 29.97 -> 23.976
static int numSample=5;
static int offset = 32;
static int runnow = 0;

static char **frames = NULL;
static int frbufsize;
static int frameIn = 0, frameOut = 0;
static int *framesOK, *framesScore;
static int scanrange = 0;

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
    printf ("  There are two modes of operation, buffered and unbuffered,\n");
    printf ("  unbuffered is quick, but buffered, especially when dropping frames\n");
    printf ("  should look better\n");
    printf ("  For most users, modfps will need either no options, or just mode=1\n");
    printf ("* Options\n");
    printf ("\tmode : (0=unbuffered, 1=buffered [%d]\n", mode);
    printf ("\tinfps : original fps (override what transcode supplies) [%lf]\n",infps);
    printf ("\tinfrc : original frc (overwrite infps) [%d]\n",infrc);
    printf ("\tbuffer : number of frames to buffer [%d]\n",numSample);
    printf ("\tsubsample : number of pixels to subsample when examining buffers [%d]\n",offset);
    printf ("\tverbose : 0 = not verbose, 1 is verbose [%d]\n",show_results);
}

static int memory_init(vframe_list_t * ptr){

  int i;
  frbufsize = numSample +1;
  if (ptr->v_codec == CODEC_YUV){
    // we only care about luminance
    scanrange = ptr->v_height*ptr->v_width;
  } else if (ptr->v_codec == CODEC_RGB){
    scanrange = ptr->v_height*ptr->v_width*3;
  } else if (ptr->v_codec == CODEC_YUY2){
    // we only care about luminance, but since this is packed
    // we'll look at everything.
    scanrange = ptr->v_height*ptr->v_width*2;
  }

  if (scanrange > ptr->video_size){
    // error, we'll overwalk boundaries later on
    fprintf(stderr, "[%s] Error, video_size doesn't look to be big enough.\n");
    return -1;
  }
  
  frames = (char**)malloc(sizeof (char*)*frbufsize);
  if (NULL == frames){
    fprintf(stderr, "[%s] Error allocating memory in init\n",MOD_NAME);
    return -1;
  } // else
  for (i=0;i<frbufsize; i++){
    frames[i] = (char*)malloc(sizeof(char)*ptr->video_size);
    if (NULL == frames[i]){
      fprintf(stderr, "[%s] Error allocating memory in init\n",MOD_NAME);
      return -1;
    }
  }
  framesOK = (int*)malloc(sizeof(int)*frbufsize);
  if (NULL == framesOK){
    fprintf(stderr, "[%s] Error allocating memory in init\n",MOD_NAME);
    return -1;
  }
  framesScore = (int*)malloc(sizeof(int)*frbufsize);
  if (NULL == framesScore){
    fprintf(stderr, "[%s] Error allocating memory in init\n",MOD_NAME);
    return -1;
  }
  if (mode == 1){
    return 0;
  }
  return -1;
}

int tc_filter(vframe_list_t * ptr, char *options)
{
    static vob_t *vob = NULL;
    static int frameCount = 0;
    static int init = 1;
    static int cloneq = 0; // queue'd clones ;)

    static double outfps = 0.0;

    //----------------------------------
    //
    // filter init
    //
    //----------------------------------


    if (ptr->tag & TC_FILTER_INIT) {

	if ((vob = tc_get_vob()) == NULL)
	    return (-1);

	// defaults
	outfps = vob->ex_fps;
	infps  = vob->fps;
	infrc  = vob->im_frc;

	// filter init ok.
	if (options != NULL) {
	  if (optstr_lookup (options, "help")) {
	    help_optstr();
	  }
	  optstr_get (options, "verbose", "%d", &show_results);
	  optstr_get (options, "mode", "%d", &mode);
	  optstr_get (options, "infps", "%lf", &infps);
	  optstr_get (options, "infrc", "%d", &infrc);
	  optstr_get (options, "buffer", "%d", &numSample);
	  optstr_get (options, "subsample", "%d", &offset);

	}

	if (infrc>0 && infrc < 16){
	  infps = frc_table[infrc];
	}

	if (verbose){
	  printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);
	  printf("[%s] converting from %2.4ffps to %2.4ffps\n",MOD_NAME,infps,outfps);
	}

	if (outfps > infps*2.0){
	  fprintf(stderr, "[%s] Error, desired output fps can not be greater\n",MOD_NAME);
	  fprintf(stderr, "[%s] than twice the input fps\n", MOD_NAME);
	  return -1;
	}

	if ( (outfps == infps) || (infrc && infrc == vob->ex_frc)) {
	  fprintf(stderr, "[%s] No framerate conversion requested, exiting\n",MOD_NAME);
	  return -1;
	}

	// clone in POST_S skip in PRE_S
	if (outfps > infps){
	  runnow = TC_POST_S_PROCESS;
	} else {
	  runnow = TC_PRE_S_PROCESS;
	}

	if ((mode >= 0) && (mode < 2)){
	  return 0;
	} // else

	fprintf (stderr, "[%s] Error, only two modes of operation.\n",MOD_NAME);
	return -1;
    }

    if (ptr->tag & TC_FILTER_GET_CONFIG){
      char buf[255];
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VYRE", "1");

      sprintf(buf, "%d",mode);
      optstr_param(options,"mode","mode of operation", "%d", buf, "0", "1");
      snprintf(buf, 128, "%lf", infps);
      optstr_param(options, "infps", "Original fps", "%f", buf, "MIN_FPS", "200.0");
      snprintf(buf, 128, "%d", infrc);
      optstr_param(options, "infrc", "Original frc", "%d", buf, "0", "16");
      sprintf(buf, "%d", numSample);
      optstr_param(options,"examine", "How many frames to buffer", "%d", buf, "2", "25");
      sprintf(buf, "%d", offset);
      optstr_param(options, "subsample", "How many pixels to subsample", "%d", buf, "1", "256");
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

    if ((ptr->tag & runnow) && (ptr->tag & TC_VIDEO)) {
      if (mode == 0){
        if (show_results){
          printf("[%s] mycount=%5d id=%5d calc=%06.3lf ",MOD_NAME,frameCount,ptr->id,(double)frameCount*infps/outfps);
	} 
        if (infps < outfps){
	  // Notes; since we currently only can clone frames (and just clone
	  // them once, we can at most double the input framerate.
	  if ((double)frameCount*infps/outfps < (double)ptr->id){
	    if (show_results){
	      printf("FRAME IS CLONED");
	    }
	    if ((ptr->attributes & TC_FRAME_IS_CLONED) || (ptr->attributes & TC_FRAME_WAS_CLONED)){
	      printf("Ack, this frame was cloned!\n");
	    }
	    ptr->attributes |= TC_FRAME_IS_CLONED;
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
      if (mode == 1){
        int i;
	if (init){
	  init = 0;
	  i = memory_init(ptr);
	  if(i!=0){
	    return i;
	  }
	}
	if (show_results){
          printf("[%s] frameIn=%d frameOut=%d mycount=%5d id=%5d calc=%06.3lf ",MOD_NAME,frameIn,frameOut,frameCount-numSample,ptr->id-numSample-cloneq,(double)(frameCount-numSample)*infps/outfps);
	}
	if (ptr->attributes & TC_FRAME_WAS_CLONED){
	  // don't do anything.  Since it's cloned, we don't
	  // want to put it our buffers, as it will just clog
	  // them up.  Later, we can try some merging/interpolation
	  // as the user requests and then leave, but for now, we'll
	  // just flee.
	  if (framesOK[(frameIn+0)%frbufsize]){
	    fprintf(stderr, "[%s] Oppps, this frame wasn't cloned but we thought it was\n",MOD_NAME);
	  }
	  ++frameCount;
	  --cloneq;
	  if (show_results){
	    printf("no slot needed for clones\n");
	  }
	  return 0;
	} // else 
	memcpy(frames[frameIn], ptr->video_buf, ptr->video_size);
	framesOK[frameIn] = 1;
#ifdef DEBUG
	printf("Inserted frame %d into slot %d \n",frameCount, frameIn);
#endif // DEBUG

	// Now let's look and see if we should compute a frame's
	// score.
	if (frameCount > 0){
	  char *t1, *t2;
	  int *score,t;
	  t=(frameIn+numSample)%frbufsize;
	  score = &framesScore[t];
	  t1 = frames[t];
	  t2 = frames[frameIn];
#ifdef DEBUG
	    printf("score: slot=%d, t1=%p t2=%p ",
	      t,t1,t2);
#endif // DEBUG
	  *score=0;
	  for(i=0; i<ptr->video_size; i+=offset){
	    *score += abs(t2[i] - t1[i]);
	  }
#ifdef DEBUG
	    printf("score = %d\n",*score);
#endif // DEBUG
	}

	// the first frbufsize-1 frames are not processed; only buffered
	// so that we might be able to effectively see the future frames
	// when deciding about a frame.
	if(frameCount < frbufsize-1){
	  ptr->attributes |= TC_FRAME_IS_SKIPPED;
	  frameIn = (frameIn+1) % frbufsize;
	  ++frameCount;
	  if (show_results){
	    printf("\n");
	  }
	  return 0;
	} // else

	// having filled the buffer, we will now check to see if we
	// are ready to clone/skip a frame.  If we are, we look for the frame to skip
	// in the buffer
	if (infps < outfps){
	  if ((double)(frameCount-numSample)*infps/outfps < (double)(ptr->id-numSample-cloneq)){
	    // we have to find a frame to clone
	    int diff=-1, mod=-1;
#ifdef DEBUG
	      printf("start=%d end=%d\n",(frameIn+1)%frbufsize,frameIn);
#endif // DEBUG
	    fflush(stdout);
	    for(i=((frameIn+1)%frbufsize); i!=frameIn; i=((i+1)%frbufsize)){
#ifdef DEBUG
	        printf("i=%d Ok=%d Score=%d\n",i,framesOK[i],framesScore[i]);
#endif // DEBUG
	      // make sure we haven't skipped/cloned this frame already
	      if(framesOK[i]){
	        // look for the frame with the most difference from it's next neighbor
	        if (framesScore[i] > diff){
		  diff = framesScore[i];
		  mod = i;
		}
	      }
	    }
	    fflush(stdout);
	    if (mod == -1){
	      fprintf(stderr,"[%s] Error calculating frame to clone\n",MOD_NAME);
	      return -1;
	    }
#ifdef DEBUG
	    printf("XXX cloning  %d\n",mod);
#endif // DEBUG
	    ++cloneq;
	    framesOK[mod] = 0;
	  }
	  memcpy(ptr->video_buf,frames[frameOut],ptr->video_size);
	  if (framesOK[frameOut]){
	    if (show_results){
	      printf("giving   slot %2d frame %6d\n",frameOut,ptr->id);
	    }
	  } else {
	    ptr->attributes |= TC_FRAME_IS_CLONED;
	    if (show_results){
	      printf("cloning  slot %2d frame %6d\n",frameOut,ptr->id);
	    }
	  }
	  frameOut = (frameOut+1) % frbufsize;
	} else {
	  // check to skip frames
	  if ((double)(frameCount-numSample)*infps/outfps > (double)(ptr->id-numSample)){
	    int diff=INT_MAX, mod=-1;

	    // since we're skipping, we look for the frame with the lowest
	    // difference between the frame which follows it.
	    for(i=((frameIn+1)%frbufsize); i!=frameIn; i=((i+1)%frbufsize)){
#ifdef DEBUG
	        printf("i=%d Ok=%d Score=%d\n",i,framesOK[i],framesScore[i]);
#endif // debug
	      // make sure we haven't skipped/cloned this frame already
	      if(framesOK[i]){
	        if (framesScore[i] < diff){
		  diff = framesScore[i];
		  mod = i;
		}
	      }
	    }
	    if (mod == -1){
	      fprintf(stderr,"[%s] Error calculating frame to skip\n",MOD_NAME);
	      return -1;
	    }
	    --frameCount;
	    framesOK[mod] = 0;
	  }
	  if (framesOK[frameOut]){
	    memcpy(ptr->video_buf,frames[frameOut],ptr->video_size);
	    if (show_results){
	      printf("giving   slot %2d frame %6d\n",frameOut,ptr->id);
	    }
	  } else {
	    ptr->attributes |= TC_FRAME_IS_SKIPPED;
	    if (show_results){
	      printf("skipping slot %2d frame %6d\n",frameOut,ptr->id);
	    }
	  }
	  frameOut = (frameOut+1) % frbufsize;
	}
	frameIn = (frameIn+1) % frbufsize;
	++frameCount;
	return 0;
      }
      fprintf(stderr, "[%s] Oppps, currently only 2 modes of operation\n",MOD_NAME);
      return(-1);
    }

    return (0);
}
