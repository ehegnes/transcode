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
// 0.3 -> 0.4: Tilmann Bitterberg
//             Fix a typo in the optstr_param printout and correct the filter
//             flags.
//             Fix a bug related to scanrange.

#define MOD_NAME    "filter_modfps.so"
#define MOD_VERSION "v0.4 (2003-07-31)"
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
    printf ("* Options\n");
    printf ("\tmode : (0=unbuffered, 1=buffered [%d]\n", mode);
    printf ("\tinfps : original fps (needed) [%lf]\n",infps);
    printf ("\tinfrc : original frc (overwrite infps) [%d]\n",infrc);
    printf ("\tbuffer : number of frames to buffer [%d]\n",numSample);
    printf ("\tsubsample : number of pixels to subsample when examining buffers [%d]\n",offset);
    printf ("\tverbose : 0 = not verbose, 1 is verbose [%d]\n",show_results);
}

int tc_filter(vframe_list_t * ptr, char *options)
{
    static vob_t *vob = NULL;
    static int frameCount = 0;
    static char **frames = NULL;
    static int frbufsize;
    static int frameIn = 0, frameOut = 0;
    static int *framesOK, *framesScore;
    static int scanrange = 0;

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
	if (infrc>0 && infrc < 16)
	    infps = frc_table[infrc];

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

	if (verbose)
	    printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);

	  if (outfps > infps*2.0){
	    fprintf(stderr, "[%s] Error, desired output fps can not be greater\n",MOD_NAME);
	    fprintf(stderr, "[%s] than twice the input fps\n", MOD_NAME);
	    return -1;
	  }

	  if ( (outfps == infps) || (infrc && infrc == vob->ex_frc)) {
	    fprintf(stderr, "[%s] No framerate conversion requested, exiting\n",MOD_NAME);
	    return -1;
	  }


	if (mode == 0){
	  return 0;
	} // else

	// can't do that here, its not valid. ptr->video_size will contain the
	// maximum of the frame the filter may get. 
	//scanrange = ptr->video_size;
	{ // allocate buffers
	  int i;
	  frbufsize = numSample +1;
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
	}
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

    if ((ptr->tag & TC_PRE_S_PROCESS) && (ptr->tag & TC_VIDEO)) {
      if (mode == 0){
        if (show_results){
          printf("[%s] infps=%02.4lf outfps=%02.4lf mycount=%5d id=%5d calc=%06.3lf\t",MOD_NAME,infps,outfps,frameCount,ptr->id,(double)frameCount*infps/outfps);
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
      if (mode == 1){
        int i;
	if (show_results){
          printf("[%s] infps=%02.4lf outfps=%02.4lf mycount=%5d id=%5d calc=%06.3lf ",MOD_NAME,infps,outfps,frameCount-numSample,ptr->id-numSample,(double)(frameCount-numSample)*infps/outfps);
	}
	//printf("frames=%p frameIn=%p next=%p ptr=%p size=%d\n",
	//  frames,frames[frameIn],frames[(frameIn+1)%frbufsize],ptr->video_buf,ptr->video_size);
        memcpy(frames[frameIn], ptr->video_buf, ptr->video_size);
#ifdef DEBUG
	  fprintf(stderr, "Inserted frame %d into slot %d \n",frameCount, frameIn);
#endif // DEBUG
	framesOK[frameIn] = 1; 
	fflush(stdout);
	if (frameCount > 0){
	  // we have a frame before this one, so we'll compute its score
	  char *t1, *t2;
	  int *score,t;
	  t=(frameIn+numSample)%frbufsize;
	  score = &framesScore[t];
	  t1 = frames[t];
	  t2 = frames[frameIn];
#ifdef DEBUG
	    printf("score: offset=%d, offset-1=%d, score=%p t1=%p t2=%p ",
	      frameIn,t,score,t1,t2);
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
	// are ready to skip a frame.  If we are, we look for the frame to skip
	// in the buffer
	if (infps < outfps){
	  if ((double)(frameCount-numSample)*infps/outfps < (double)(ptr->id-numSample)){
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
	      printf("XXX cloning %d\n",mod);
#endif // DEBUG
	    ++frameCount;
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
