/*
 *  filter_mask.c
 *
 *  Copyright (C) Thomas Östreich, Chad Page - February, March 2002
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

#define MOD_NAME    "filter_mask.so"
#define MOD_VERSION "v0.2.0 (2002-04-21)"
#define MOD_CAP     "masking plugin"

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

void ymask_yuv(unsigned char *buf, vob_t *vob, int top, int bottom)
{
	int i;
	unsigned char *bufcr, *bufcb;

	// printf("%d %d\n", top, bottom);

	bufcr = &buf[vob->im_v_width * vob->im_v_height];
	bufcb = &bufcr[(vob->im_v_width * vob->im_v_height) / 4];

	for (i = top; i <= bottom; i+=2) {
		memset(&buf[i * vob->im_v_width], 0, vob->im_v_width); 
		memset(&buf[(i + 1) * vob->im_v_width], 0, vob->im_v_width); 
	//	memset(&bufcr[(i / 2) * (vob->im_v_width / 2)], 128, vob->im_v_width / 2); 
	//	memset(&bufcb[(i / 2) * (vob->im_v_width / 2)], 128, vob->im_v_width / 2); 
	}
}

void ymask_rgb(unsigned char *buf, vob_t *vob, int top, int bottom)
{
	int i;

	for (i = top; i <= bottom; i++) {
		memset(&buf[i * vob->im_v_width * 3], 0, vob->im_v_width * 3); 
	}
}

void xmask_yuv(unsigned char *buf, vob_t *vob, int left, int right)
{
	int i;
	unsigned char *bufcr, *bufcb;
	unsigned char *ptr, *ptrmax;

	// printf("%d %d\n", top, bottom);

	bufcr = &buf[vob->im_v_width * vob->im_v_height];
	bufcb = &bufcr[(vob->im_v_width * vob->im_v_height) / 4];

	/* Y */

	for (i = left; i < right; i++) {
		ptr = &buf[i]; 
		ptrmax = &buf[i + (vob->im_v_height * vob->im_v_width)]; 
		while (ptr < ptrmax) {
			*ptr = 0;
			ptr += vob->im_v_width;
		}
	}	
}

void xmask_rgb(unsigned char *buf, vob_t *vob, int left, int right)
{
	int x, y;
	unsigned char *ptr, *ptrmax;

	for (y = 0; y < vob->im_v_height; y++) { 
		ptr = &buf[(y * vob->im_v_width * 3) + (left * 3)]; 
		memset(ptr, 0, (right - left) * 3); 
	}
}

int tc_filter(vframe_list_t *ptr, char *options)
{

  static vob_t *vob=NULL;
  static int lc, rc, tc, bc; 

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
    
    if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);
    
    if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);

    buffer = malloc(SIZE_RGB_FRAME);

    lc = 0; 
    tc = 0;

    if(options != NULL) sscanf(options, "%d:%d:%d:%d", &lc, &_rc, &tc, &_bc);

    rc = vob->im_v_width - _rc;
    bc = vob->im_v_height - _bc;

    return(0);
  }
  
  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  
  if(ptr->tag & TC_FILTER_CLOSE) {
    
    free(buffer);
    
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
  
  if(ptr->tag & TC_PRE_M_PROCESS && ptr->tag & TC_VIDEO) {
  //    memcpy(buffer, ptr->video_buf, SIZE_RGB_FRAME);
  //    memcpy(ptr->video_buf, buffer, SIZE_RGB_FRAME);

      if (vob->im_v_codec==CODEC_YUV) {
	  if (tc > 2) ymask_yuv(ptr->video_buf, vob, 0, tc - 1); 
	  if ((vob->im_v_height - bc) > 1) ymask_yuv(ptr->video_buf, vob, bc, vob->im_v_height - 1); 
	  if (lc > 2) xmask_yuv(ptr->video_buf, vob, 0, lc - 1); 
	  if ((vob->im_v_width - rc) > 1) xmask_yuv(ptr->video_buf, vob, rc, vob->im_v_width - 1); 
      }
      if (vob->im_v_codec==CODEC_RGB) {
	  if (tc > 2) ymask_rgb(ptr->video_buf, vob, 0, tc - 1); 
	  if ((vob->im_v_height - bc) > 1) ymask_rgb(ptr->video_buf, vob, bc, vob->im_v_height - 1); 
	  if (lc > 2) xmask_rgb(ptr->video_buf, vob, 0, lc - 1); 
	  if ((vob->im_v_width - rc) > 1) xmask_rgb(ptr->video_buf, vob, rc, vob->im_v_width - 1); 
      }
  } 
  
  return(0);
}
