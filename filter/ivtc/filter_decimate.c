/*
 *  filter_decimate.c
 *
 *  Copyright (C) Thanassis Tsiodras - August 2002
 *
 *  This file is part of transcode, a linux video stream processing tool
 *  Based on the excellent work of Donald Graft in Decomb.
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

#define MOD_NAME    "filter_decimate.so"
#define MOD_VERSION "v0.2 (2002-08-12)"
#define MOD_CAP     "NTSC decimation plugin"

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

// basic parameter
// static int color_diff_threshold1 = 50;
// static int color_diff_threshold2 = 100;
// static double critical_threshold = 0.00005;
static int show_results=0;

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

#define FRBUFSIZ 15

int tc_filter(vframe_list_t * ptr, char *options)
{

    static vob_t *vob = NULL;
    static char *lastFrames[FRBUFSIZ];
    static int frameIn = 0, frameOut = 0;
    static int frameCount = -1, lastFramesOK[FRBUFSIZ];

    //----------------------------------
    //
    // filter init
    //
    //----------------------------------


    if (ptr->tag & TC_FILTER_INIT) {

	int i;

	if ((vob = tc_get_vob()) == NULL)
	    return (-1);

	if (vob->im_v_codec != CODEC_YUV) {
		printf("[%s] Sorry, only YUV input allowed for now\n", MOD_NAME);
		return (-1);
	}

	// filter init ok.
	if (options != NULL) {

	    if (optstr_get (options, "verbose", "") >= 0) {
		show_results=1;
	    }

	}

	if (verbose)
	    printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);

	for(i=0; i<FRBUFSIZ; i++) {
	    lastFrames[i] = malloc(SIZE_RGB_FRAME);
	    lastFramesOK[i] = 1;
    	}

	return (0);
    }
    //----------------------------------
    //
    // filter close
    //
    //----------------------------------


    if (ptr->tag & TC_FILTER_CLOSE) {
	int i;
	
	for(i=0; i<FRBUFSIZ; i++) 
	    free(lastFrames[i]);
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

	// After frame processing, the frames must be deinterlaced 
	// To correctly IVTC, you must use filter_ivtc before this filter,
	// i.e. -J ivtc,decimate
	memcpy(lastFrames[frameIn], ptr->video_buf, SIZE_RGB_FRAME);
	if (show_results) fprintf(stderr, "Inserted frame %d into slot %d ", frameCount, frameIn);
	lastFramesOK[frameIn] = 1;
	frameIn = (frameIn+1) % FRBUFSIZ;
	frameCount++;

	// The first 4 frames are not output - they are only placed in the buffer
	if (frameCount <= 4) {
	    ptr->attributes |= TC_FRAME_IS_SKIPPED;
	} else {
	    // We have the last FRBUFSIZ frames in the buffer
	    // We will now output one from the FRBUFSIZ buffered.
	    // From now on, we will drop 1 frame out of 5 (29.97->23.976)
	    // We will drop the one that looks exactly like its previous one
	    
	    if ((frameCount % 5) == 0) {
	
		// First, find which one from the last 4 looks almost exactly like
		// its previous one.
		int diff1,diff2,diff3,diff4,diff5, i;
		diff1 = diff2 = diff3 = diff4 = diff5 = 0;

		// Skip 16 pixels for speed.
		// Don't put all 'diff' calculations in the same loop - cache prefers data locality
		for(i=0; i<ptr->v_height*ptr->v_width; i+=16)
		    diff1 += abs(lastFrames[(frameOut+1)%FRBUFSIZ][i] - lastFrames[(frameOut+0)%FRBUFSIZ][i]);
		for(i=0; i<ptr->v_height*ptr->v_width; i+=16)
		    diff2 += abs(lastFrames[(frameOut+2)%FRBUFSIZ][i] - lastFrames[(frameOut+1)%FRBUFSIZ][i]);
		for(i=0; i<ptr->v_height*ptr->v_width; i+=16)
		    diff3 += abs(lastFrames[(frameOut+3)%FRBUFSIZ][i] - lastFrames[(frameOut+2)%FRBUFSIZ][i]);
		for(i=0; i<ptr->v_height*ptr->v_width; i+=16)
		    diff4 += abs(lastFrames[(frameOut+4)%FRBUFSIZ][i] - lastFrames[(frameOut+3)%FRBUFSIZ][i]);
		for(i=0; i<ptr->v_height*ptr->v_width; i+=16)
		    diff5 += abs(lastFrames[(frameOut+5)%FRBUFSIZ][i] - lastFrames[(frameOut+4)%FRBUFSIZ][i]);

		if (diff1<diff2 && diff1<diff3 && diff1<diff4 && diff1<diff5) {		// 0 and 1 are almost the same
		    lastFramesOK[(frameOut+0)%FRBUFSIZ] = 0;
		}
		else if (diff2<diff1 && diff2<diff3 && diff2<diff4 && diff2<diff5) {	// 1 and 2 are almost the same
		    lastFramesOK[(frameOut+1)%FRBUFSIZ] = 0;
		}
		else if (diff3<diff1 && diff3<diff2 && diff3<diff4 && diff3<diff5) {	// 2 and 3 are almost the same
		    lastFramesOK[(frameOut+2)%FRBUFSIZ] = 0;
		}
		else if (diff4<diff1 && diff4<diff2 && diff4<diff3 && diff4<diff5) {	// 3 and 4 are almost the same
		    lastFramesOK[(frameOut+3)%FRBUFSIZ] = 0;
		}
		else if (diff5<diff1 && diff5<diff2 && diff5<diff3 && diff5<diff4) {	// 4 and 5 are almost the same
		    lastFramesOK[(frameOut+4)%FRBUFSIZ] = 0;
		}
		else {									// all are the same...
		    lastFramesOK[(frameOut+0)%FRBUFSIZ] = 0;
		}
	    }

	    if (lastFramesOK[frameOut]) {
		memcpy(ptr->video_buf, lastFrames[frameOut], SIZE_RGB_FRAME);
		if (show_results) fprintf(stderr, "giving slot %d\n", frameOut);
	    }
	    else {
		ptr->attributes |= TC_FRAME_IS_SKIPPED;
		if (show_results) fprintf(stderr, "droping slot %d\n", frameOut);
	    }
	    frameOut = (frameOut+1) % FRBUFSIZ;
	}
    }

    return (0);
}
