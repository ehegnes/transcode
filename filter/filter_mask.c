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
#define MOD_VERSION "v0.2.3 (2003-10-12)"
#define MOD_CAP     "Filter through a rectangular Mask"
#define MOD_AUTHOR  "Thomas Östreich, Chad Page"

#include "transcode.h"
#include "filter.h"
#include "optstr.h"


static char *buffer;

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

void ymask_yuv(unsigned char *buf, vob_t *vob, int top, int bottom)
{
	int i;
	unsigned char *bufcr, *bufcb;
	int w2 = vob->im_v_width / 2;

	// printf("%d %d\n", top, bottom);

	bufcr = buf + vob->im_v_width * vob->im_v_height;
	bufcb = buf + vob->im_v_width * vob->im_v_height * 5/ 4;

	for (i = top; i <= bottom; i+=2) {
		memset(&buf[i * vob->im_v_width], 0x10, vob->im_v_width); 
		memset(&buf[(i + 1) * vob->im_v_width], 0x10, vob->im_v_width); 
	    	memset(&bufcr[(i / 2) * w2], 128, w2); 
	    	memset(&bufcb[(i / 2) * w2], 128, w2); 
	}
}

void ymask_yuv422(unsigned char *buf, vob_t *vob, int top, int bottom)
{
	int i,j;
	unsigned char *t;

	for (i = top; i <= bottom; i++) {
	    t = buf + i * vob->im_v_width * 2;
	    for (j=0; j<vob->im_v_width*2; j++)
		*t++ = (j&1)?0x10:0x80;
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

	// printf("%d %d\n", left, right);

	bufcr = buf + vob->im_v_width * vob->im_v_height;
	bufcb = buf + vob->im_v_width * vob->im_v_height * 5/ 4;

	/* Y */
	for (i = left; i < right; i++) {
		ptr = &buf[i]; 
		ptrmax = &buf[i + (vob->im_v_height * vob->im_v_width)]; 
		while (ptr < ptrmax) {
			*ptr = 0x10;
			ptr += vob->im_v_width;
		}
	}	

	/* Cr */
	for (i = left; i < right; i++) {
		ptr = &bufcr[i/2]; 
		ptrmax = &bufcr[i/2 + (vob->im_v_height/2 * vob->im_v_width/2)]; 
		while (ptr < ptrmax) {
			*ptr = 128;
			ptr += vob->im_v_width/2;
		}
	}	

	/* Cb */
	for (i = left; i < right; i++) {
		ptr = &bufcb[i/2]; 
		ptrmax = &bufcb[i/2 + (vob->im_v_height/2 * vob->im_v_width/2)]; 
		while (ptr < ptrmax) {
			*ptr = 128;
			ptr += vob->im_v_width/2;
		}
	}	
}

void xmask_yuv422(unsigned char *buf, vob_t *vob, int left, int right)
{
	int y, j;
	unsigned short *t;
	unsigned short mask = ((0x80 << 8)&0xff00) | ((0x10)&0xff);

	for (y = 0; y < vob->im_v_height; y++) { 
	    t = (unsigned short *)(buf + y * vob->im_v_width * 2 + left*2);
	    for (j=0; j<(right-left); j++)
		*t++ = mask;
	}
}

void xmask_rgb(unsigned char *buf, vob_t *vob, int left, int right)
{
	int y;
	unsigned char *ptr;

	for (y = 0; y < vob->im_v_height; y++) { 
		ptr = &buf[(y * vob->im_v_width * 3) + (left * 3)]; 
		memset(ptr, 0, (right - left) * 3); 
	}
}


// old or new syntax?
static int is_optstr (char *buf) {
    if (strchr(buf, '='))
	return 1;
    if (strchr(buf, 't'))
	return 1;
    if (strchr(buf, 'h'))
	return 1;
    return 0;
}

static void help_optstr()
{
   printf ("[%s] (%s) help\n", MOD_NAME, MOD_CAP);
   printf ("* Overview\n");
   printf ("    This filter applies an rectangular mask to the video.\n");
   printf ("    Everything outside the mask is set to black.\n");
   printf ("* Options\n");
   printf ("    lefttop : Upper left corner of the box\n");
   printf ("   rightbot : Lower right corner of the box\n");

}

int tc_filter(vframe_list_t *ptr, char *options)
{

  static vob_t *vob=NULL;
  static int lc, rc, tc, bc; 

  int _rc, _bc;

  if(ptr->tag & TC_FILTER_GET_CONFIG) {
      char buf[32];

      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRY4E", "1");
      
      snprintf(buf, 32, "%dx%d", lc, tc);
      optstr_param (options, "lefttop", "Upper left corner of the box", "%dx%d", buf, "0", "width", "0", "height"); 

      snprintf(buf, 32, "%dx%d", rc, bc);
      optstr_param (options, "rightbot", "Lower right corner of the box", "%dx%d", buf, "0", "width", "0", "height"); 

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
    
    if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);

    if (!buffer)
	buffer = malloc(SIZE_RGB_FRAME);

    lc = 0; 
    tc = 0;
    _rc = 0;
    _bc = 0;
    rc = vob->im_v_width;
    bc = vob->im_v_height;

    if(options != NULL) { 
	if (!is_optstr(options)) { // old syntax
	    sscanf(options, "%d:%d:%d:%d", &lc, &_rc, &tc, &_bc);
	    rc = vob->im_v_width - _rc;
	    bc = vob->im_v_height - _bc;
	} else {
           optstr_get (options, "lefttop", "%dx%d", &lc, &tc);
           optstr_get (options, "rightbot", "%dx%d", &rc, &bc);
	   if (optstr_lookup(options, "help"))
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
    
      if (buffer)
	  free(buffer);
      buffer = NULL;
    
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
  
  if(ptr->tag & TC_PRE_M_PROCESS && ptr->tag & TC_VIDEO && !(ptr->attributes & TC_FRAME_IS_SKIPPED)) {
  //    tc_memcpy(buffer, ptr->video_buf, SIZE_RGB_FRAME);
  //    tc_memcpy(ptr->video_buf, buffer, SIZE_RGB_FRAME);

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
      if (vob->im_v_codec==CODEC_YUV422) {
	  if (tc > 2) ymask_yuv422(ptr->video_buf, vob, 0, tc - 1); 
	  if ((vob->im_v_height - bc) > 1) ymask_yuv422(ptr->video_buf, vob, bc, vob->im_v_height - 1); 
	  if (lc > 2) xmask_yuv422(ptr->video_buf, vob, 0, lc - 1); 
	  if ((vob->im_v_width - rc) > 1) xmask_yuv422(ptr->video_buf, vob, rc, vob->im_v_width - 1); 
      }
  } 
  
  return(0);
}
