/*
 *  filter_smooth.c
 *
 *  Copyright (C) Chad Page - October 2002
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

#define MOD_NAME    "filter_smooth.so"
#define MOD_VERSION "v0.1.0 (2002-10-13)"
#define MOD_CAP     "(single-frame) smoothing plugin"

#include <stdio.h>
#include <stdlib.h>

static char *buffer;

static int loop=1;

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

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

#define MAX_X	1024
#define MAX_Y	768

static unsigned char *tbuf;

void smooth_yuv(unsigned char *buf, vob_t *vob, int maxdiff, int maxldiff, int maxdist, float level)
{
	int i, x, y, pl, pu, cpu, cdiff;
	int xa, ya, oval, ldiff;
	unsigned char *bufcr, *bufcb;
	unsigned char *tbufcr, *tbufcb;
	float dist, ratio, nval;

	memcpy(tbuf, buf, (vob->im_v_width * vob->im_v_height) * 3 / 2);
	
	bufcr = &buf[vob->im_v_width * vob->im_v_height];
	bufcb = &bufcr[(vob->im_v_width * vob->im_v_height) / 4];
	
	tbufcr = &tbuf[vob->im_v_width * vob->im_v_height];
	tbufcb = &tbufcr[(vob->im_v_width * vob->im_v_height) / 4];

	/* First pass - horizontal */

	for (y = 0; y < (vob->im_v_height); y++) {
		for (x = 0; x < vob->im_v_width; x++) {
			pu = ((y * vob->im_v_width) / 2) + (x / 2); 
			nval = ((float)buf[x + (y * vob->im_v_width)]);
			oval = buf[x + (y * vob->im_v_width)];
			for (xa = x - maxdist; (xa <= (x + maxdist)) && (xa < vob->im_v_width); xa++) {
				if (xa < 0) xa = 0;
				if (xa == x) xa++;
				cpu = ((y * vob->im_v_width) / 2) + (xa / 2); 
				cdiff = abs(tbufcr[pu] - tbufcr[cpu]); 
				cdiff += abs(tbufcb[pu] - tbufcb[cpu]); 

				/* If color difference not too great, average the pixel according to distance */
				ldiff = abs(tbuf[xa + (y * vob->im_v_width)] - oval); 
				if ((cdiff < maxdiff) && (ldiff < maxldiff)) {
					dist = abs(xa - x);	
					ratio = level / dist;
					nval = nval * (1 - ratio);
					nval += ((float)tbuf[xa + (y * vob->im_v_width)]) * ratio;
				}
			}
			buf[x + (y * vob->im_v_width)] = (unsigned char)(nval + 0.5);
		}
	}
	
	/* Second pass - vertical lines */
	
	memcpy(tbuf, buf, (vob->im_v_width * vob->im_v_height) * 3 / 2);
	
	for (y = 0; y < (vob->im_v_height); y++) {
		for (x = 0; x < vob->im_v_width; x++) {
			pu = ((y * vob->im_v_width) / 2) + (x / 2); 
			nval = ((float)buf[x + (y * vob->im_v_width)]);
			oval = buf[x + (y * vob->im_v_width)];
			for (ya = y - maxdist; (ya <= (y + maxdist)) && (ya < vob->im_v_height); ya++) {
				if (ya < 0) ya = 0;
				if (ya == y) ya++;
				cpu = ((ya * vob->im_v_width) / 2) + (x / 2); 
				cdiff = abs(tbufcr[pu] - tbufcr[cpu]); 
				cdiff += abs(tbufcb[pu] - tbufcb[cpu]); 

				/* If color difference not too great, average the pixel according to distance */
				ldiff = abs(tbuf[x + (ya * vob->im_v_width)] - oval); 
				if ((cdiff < maxdiff) && (ldiff < maxldiff)) {
					dist = abs(ya - y);	
					ratio = level / dist;
					nval = nval * (1 - ratio);
					nval += ((float)tbuf[x + (ya * vob->im_v_width)]) * ratio;
//						printf("%d %d %d %d %f %f\n", xa, ya, tbuf[x * (y * vob->im_v_width)], buf[x + (y * vob->im_v_width)], nval, ratio);
				}
			}
			buf[x + (y * vob->im_v_width)] = (unsigned char)(nval + 0.5);
		}
	}
}

int tc_filter(vframe_list_t *ptr, char *options)
{

  static vob_t *vob=NULL;
  static int cdiff, ldiff, range;
  static float strength;

  int x, y, _rc, _bc;

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
  // filter init
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_INIT) {
    
    if((vob = tc_get_vob())==NULL) return(-1);
    
    // filter init ok.
   
    // set defaults

    strength = 0.25;	/* Blending factor.  Do not exceed 2 ever */
    cdiff = 6;		/* Max difference in UV values */
    ldiff = 8;		/* Max difference in chroma value */
    range = 4;		/* Search range */

    if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);
    
    if (options != NULL) {
    	if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);
	
	optstr_get (options, "strength",  "%f", &strength);
	optstr_get (options, "cdiff",  "%d", &cdiff);
	optstr_get (options, "ldiff",  "%d", &ldiff);
	optstr_get (options, "range",  "%d", &range);
    }

    tbuf = (unsigned char *) malloc(SIZE_RGB_FRAME);
    if (strength > 0.9) strength = 0.9;

    return(0);
  }
  
  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  
  if(ptr->tag & TC_FILTER_CLOSE) {
    free(tbuf);

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
  
  if(ptr->tag & TC_PRE_PROCESS && ptr->tag & TC_VIDEO) {

	if (vob->im_v_codec == CODEC_YUV) smooth_yuv(ptr->video_buf, vob, cdiff, ldiff, range, strength);

  } 
  
  return(0);
}
