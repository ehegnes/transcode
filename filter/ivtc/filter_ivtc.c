/*
 *  filter_ivtc.c
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

#define MOD_NAME    "filter_ivtc.so"
#define MOD_VERSION "v0.4 (2003-04-22)"
#define MOD_CAP     "NTSC inverse telecine plugin"

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

#define FRBUFSIZ 3

int tc_filter(vframe_list_t * ptr, char *options)
{

    static vob_t *vob = NULL;
    static char *lastFrames[FRBUFSIZ];
    static int frameIn = 0;
    static int frameCount = 0;

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

    if ((ptr->tag & TC_PRE_PROCESS) && (ptr->tag & TC_VIDEO)) {

	memcpy(	lastFrames[frameIn], 
		ptr->video_buf, 
		ptr->v_width*ptr->v_height*3);
	if (show_results) 
	    fprintf(stderr, "Inserted frame %d into slot %d\n", 
		    frameCount, frameIn);
	frameIn = (frameIn+1) % FRBUFSIZ;
	frameCount++;

	// The first 2 frames are not output - they are only buffered
	if (frameCount <= 2) {
	    ptr->attributes |= TC_FRAME_IS_SKIPPED;
	} else {
	    // We have the last 3 frames in the buffer... 
	    //
	    //		Previous Current Next
	    // 
	    // OK, time to work...
	   
	    unsigned char *curr, 
		*pprev, *pnext, *cprev, *cnext, *nprev, *nnext, *dstp;
	    int idxp, idxc, idxn;
	    int p, c, n, lowest, chosen;
	    int C, x, y;
	    int comb;

	    idxn = frameIn-1; while(idxn<0) idxn+=FRBUFSIZ;
	    idxc = frameIn-2; while(idxc<0) idxc+=FRBUFSIZ;
	    idxp = frameIn-3; while(idxp<0) idxp+=FRBUFSIZ;

	    // bottom field of current
	    curr =  &lastFrames[idxc][ptr->v_width];  	
	    // top field of previous
	    pprev = &lastFrames[idxp][0];		
	    // top field of previous - 2nd scanline
	    pnext = &lastFrames[idxp][2*ptr->v_width];	
	    // top field of current
	    cprev = &lastFrames[idxc][0];		
	    // top field of current - 2nd scanline
	    cnext = &lastFrames[idxc][2*ptr->v_width];	
	    // top field of next
	    nprev = &lastFrames[idxn][0];		
	    // top field of next - 2nd scanline
	    nnext = &lastFrames[idxn][2*ptr->v_width]; 	

	    // Blatant copy begins...

	    p = c = n = 0;
	    /* Try to match the top field of the current frame to the
	       bottom fields of the previous, current, and next frames.
	       Output the assembled frame that matches up best. For
	       matching, subsample the frames in the x dimension
	       for speed. */
	    for (y = 0; y < ptr->v_height-2; y+=4)
	    {
		for (x = 0; x < ptr->v_width;)
		{
		    C = curr[x];
#define T 100
		    /* This combing metric is based on 
		       an original idea of Gunnar Thalin. */
		    comb = ((long)pprev[x] - C) * ((long)pnext[x] - C);
		    if (comb > T) p++;

		    comb = ((long)cprev[x] - C) * ((long)cnext[x] - C);
		    if (comb > T) c++;

		    comb = ((long)nprev[x] - C) * ((long)nnext[x] - C);
		    if (comb > T) n++;

		    if (!(++x&3)) x += 12;
		}
		curr  += ptr->v_width*4;
		pprev += ptr->v_width*4;
		pnext += ptr->v_width*4;
		cprev += ptr->v_width*4;
		cnext += ptr->v_width*4;
		nprev += ptr->v_width*4;
		nnext += ptr->v_width*4;
	    }

	    lowest = c;
	    chosen = 1;
	    if (p < lowest)
	    {
		    lowest = p;
		    chosen = 0;
	    }
	    if (n < lowest)
	    {
		    lowest = n;
		    chosen = 2;
	    }
		
	    // Blatant copy ends... :)

	    if (show_results) 
		fprintf(stderr, 
		    "Telecide => frame %d: p=%u  c=%u  n=%u [using %d]\n", 
		    frameCount, p, c, n, chosen);

	    // Set up the pointers in preparation to output final frame. 

	    // First, the Y plane
	    if (chosen == 0) 
		curr = lastFrames[idxp];
	    else if (chosen == 1) 
		curr = lastFrames[idxc];
	    else 
		curr = lastFrames[idxn];

	    dstp = ptr->video_buf;
	    
	    // First output the top field selected 
	    // from the set of three stored frames.
	    for (y = 0; y < (ptr->v_height+1)/2; y++)
	    {
		    memcpy(dstp, curr, ptr->v_width);
		    curr += ptr->v_width*2;
		    dstp += ptr->v_width*2;
	    }

	    // The bottom field of the current frame unchanged 
	    dstp = ptr->video_buf   + ptr->v_width;
	    curr = lastFrames[idxc] + ptr->v_width;
	    
	    for (y = 0; y < (ptr->v_height+1)/2; y++)
	    {
		    memcpy(dstp, curr, ptr->v_width);
		    curr += ptr->v_width*2;
		    dstp += ptr->v_width*2;
	    }

	    // And now, the color planes
	    if (chosen == 0) 
		curr = lastFrames[idxp];
	    else if (chosen == 1) 
		curr = lastFrames[idxc];
	    else 
		curr = lastFrames[idxn];
	    curr += ptr->v_width * ptr->v_height;

	    dstp = ptr->video_buf;
	    dstp += ptr->v_width * ptr->v_height;
	    
	    // First output the top field selected 
	    // from the set of three stored frames. 
	    for (y = 0; y < (ptr->v_height+1)/2; y++)
	    {
		    memcpy(dstp, curr, ptr->v_width/2);
		    curr += ptr->v_width;  // jump two color scanlines
		    dstp += ptr->v_width;  // jump two color scanlines
	    }

	    /* The bottom field of the current frame unchanged */
	    dstp =  ptr->video_buf + 
		    ptr->v_width * ptr->v_height + 
		    ptr->v_width/2;
	    curr =  lastFrames[idxc] + 
		    ptr->v_width * ptr->v_height + 
		    ptr->v_width/2;
	    
	    for (y = 0; y < (ptr->v_height+1)/2; y++)
	    {
		    memcpy(dstp, curr, ptr->v_width/2);
		    curr += ptr->v_width;  // jump two color scanlines
		    dstp += ptr->v_width;  // jump two color scanlines
	    }

	}
    }

    return (0);
}
