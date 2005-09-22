/*
 *  decode_yuv.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  This file is part of transcode, a video stream  processing tool
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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "ioaux.h"

#include "aclib/yuv2rgb.h"
#include "aclib/ac.h"

#include "transcode.h"
#include "tc.h"

/*
 * About this code:
 *
 * based on video_out.h, video_out.c, video_out_ppm.c
 * 
 * Copyright (C) 1999-2001 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 * Stripped and rearranged for transcode by
 * Francesco Romani <fromani@gmail.com> - July 2005
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 *
 * What?
 * Basically, this code does only a colorspace conversion from ingress frames
 * (YV12) to egress frames (RGB). It uses basically the same routines of libvo
 * and old decode_yuv.c
 * 
 * Why?
 * decode_yuv was the one and the only transcode module which uses the main
 * libvo routines, not the colorspace conversion ones. It not make sense to me
 * to have an extra library just for one module, so I stripped down to minimum
 * the libvo code used by decode_yuv.c and I moved it here.
 * This code has only few things recalling it's ancestor, but I'm still remark
 * it's origin.
 */

typedef struct vo_s {
    // frame size
    unsigned int width;
    unsigned int height;
    
    // needed by yuv2rgb routines
    int rgbstride;
    int bpp;
    
    // internal frame buffers
    uint8_t *rgb;
    uint8_t *yuv[3];
} vo_t;

#define vo_convert(instp) \
	yuv2rgb((instp)->rgb, (instp)->yuv[0], (instp)->yuv[1], (instp)->yuv[2], \
	     (instp)->width, (instp)->height, (instp)->rgbstride, \
	     (instp)->width, (instp)->width >> 1);

/* 
 * legacy (and working :) ) code: 
 * read one YV12 plane at time from file descriptor (pipe, usually)
 * and store it in internal buffer
 */
int vo_read_yuv (vo_t *vo, int fd)
{
   unsigned int v = vo->height, h = vo->width;
   int i, bytes;

   /* Read luminance scanlines */

   for (i = 0; i < v; i++)
       if ((bytes = p_read (fd, vo->yuv[0] + i * h, h)) != h) {
	   if (bytes < 0)
	      fprintf(stderr,"(%s) read failed", __FILE__);
	   return 0;
       }

   v /= 2;
   h /= 2;

   /* Read chrominance scanlines */

   for (i = 0; i < v; i++)
       if ((bytes = p_read (fd, vo->yuv[1] + i * h, h)) != h) {
	  if (bytes < 0) 
	     fprintf(stderr,"(%s) read failed", __FILE__);
	  return 0;
       }

   for (i = 0; i < v; i++)
       if ((bytes = p_read (fd, vo->yuv[2] + i * h, h)) != h) {
	   if (bytes < 0)
	      fprintf(stderr,"(%s) read failed", __FILE__);
	   return 0;
       }
   
   return 1;
}

/*
 * simpler than above:
 * write the whole RGB buffer in to file descriptor (pipe, usually).
 * WARNING: caller must ensure that RGB buffer holds valid data
 * invoking vo_convert *before* to invoke this function
 */
int vo_write_rgb (vo_t *vo, int fd)
{
   int framesize = vo->rgbstride * vo->height, bytes = 0;
   bytes = p_write (fd, vo->rgb, framesize);
   if (bytes != framesize) {
      if (bytes < 0) 
         fprintf(stderr,"(%s) read failed", __FILE__);
      return 0;
   }
   return 1;
}

/*
 * finalize a vo structure, free()ing it's internal buffers.
 * WARNING: DO NOT cause a buffer flush, you must do it manually.
 */
void vo_clean (vo_t *vo)
{
    free (vo->yuv[0]);
    free (vo->yuv[1]);
    free (vo->yuv[2]);
    free (vo->rgb);
}

/*
 * initialize a vo structure, allocate internal buffers
 * and so on
 */
int vo_alloc (vo_t *vo, int width, int height)
{
    if (width <= 0 || height <= 0) {
        return -1;
    }

    vo->width = (unsigned int)width;
    vo->height = (unsigned int)height;
    vo->bpp = BPP;

    vo->rgbstride = width * vo->bpp / 8;

    vo->yuv[0] = calloc (1, PAL_W * PAL_H);
    if (!vo->yuv[0]) {
        fprintf (stderr, "(%s) out of memory\n", __FILE__);
	return -1;
    }
    vo->yuv[1] = calloc (1, PAL_W/2 * PAL_H/2);  
    if (!vo->yuv[1]) {
        fprintf (stderr, "(%s) out of memory\n", __FILE__);
	free (vo->yuv[0]);
	return -1;
    }
    vo->yuv[2] = calloc (1, PAL_W/2 * PAL_H/2);  
    if(!vo->yuv[2]) {
        fprintf (stderr, "(%s) out of memory\n", __FILE__);
	free (vo->yuv[0]);
	free (vo->yuv[1]);
	return -1;
    }
    
    vo->rgb = calloc (1, vo->rgbstride * height);
    if(!vo->rgb) {
        fprintf (stderr, "(%s) out of memory\n", __FILE__);
	free (vo->yuv[0]);
	free (vo->yuv[1]);
	free (vo->yuv[2]);
        return -1;
    }
    
#if 0
    yuv2rgb_init (vo->bpp, MODE_BGR);
// EMS: this fixes RGB output, probably breaks ppm output...
#else
    yuv2rgb_init (ac_mmflag(), vo->bpp, MODE_RGB);
#endif
    
    return 0;
}


/* ------------------------------------------------------------ 
 *
 * decoder thread
 *
 * ------------------------------------------------------------*/

void decode_yuv(decode_t *decode)
{
  vo_t vo;
  
  if(decode->width <= 0 || decode->height <= 0) {
     fprintf(stderr,"(%s) invalid frame parameter %dx%d\n", 
		    __FILE__, decode->width, decode->height);
     import_exit(1);
  }
  
  vo_alloc(&vo, decode->width, decode->height);
  
  // read frame by frame - decode into BRG - pipe to stdout
  
  while(vo_read_yuv(&vo, decode->fd_in)) {
    vo_convert(&vo);
    vo_write_rgb(&vo, decode->fd_out);
  }
  
  // ends
  vo_clean(&vo);
  
  import_exit(0);
}
