/*
 *  decode_yuv.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  This file is part of transcode, a linux video stream  processing tool
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

#include "ioaux.h"
#include "tc.h"

#include "video_out.h"
#include "mpeg2.h"

#include "mm_accel.h"

extern vo_open_t vo_ppmpipe_open;

static FILE *in_file;
static FILE *out_file;

static vo_open_t *output_open = vo_ppmpipe_open;
extern void directdraw_rgb(vo_instance_t *output, char *yuv[3]);
extern vo_open_t vo_ppmpipe_open;


static int yuv_read_frame (int fd, char *yuv[3], int width, int height)
{

   int v, h, i;

   int bytes;


   h = width;
   v = height;

   /* Read luminance scanlines */

   for (i = 0; i < v; i++)
       if ((bytes=p_read (fd, yuv[0] + i * h, h)) != h) {
	   if(bytes<0)  fprintf(stderr,"(%s) read failed", __FILE__);
	   return 0;
       }

   v /= 2;
   h /= 2;

   /* Read chrominance scanlines */

   for (i = 0; i < v; i++)
       if ((bytes=p_read (fd, yuv[1] + i * h, h)) != h) {
	  if(bytes<0)  fprintf(stderr,"(%s) read failed", __FILE__);
	   return 0;
       }

   for (i = 0; i < v; i++)
       if ((bytes=p_read (fd, yuv[2] + i * h, h)) != h) {
	   if(bytes<0)  fprintf(stderr,"(%s) read failed", __FILE__);
	   return 0;
       }
   
   return 1;
}

static int outstream(char *framebuffer, int bytes)
{

  if (fwrite(framebuffer, bytes, 1, out_file)!= 1) {
      fprintf(stderr,"(%s) video write failed.\n", __FILE__);
    import_exit(0);
  }

  return(0);
}

/* ------------------------------------------------------------ 
 *
 * decoder thread
 *
 * ------------------------------------------------------------*/

void decode_yuv(info_t *ipipe)
{
  
  vo_instance_t *output;

  uint32_t accel;

  char *yuv[3];

  int width, height;

  accel = mm_accel () | MM_ACCEL_MLIB;
  
  vo_accel(accel);
  output = vo_open(output_open, outstream);

  in_file = fdopen(ipipe->fd_in, "r");
  out_file = fdopen(ipipe->fd_out, "w");

  // allocate space

  yuv[0] = (char *)calloc(1, PAL_W*PAL_H);  
  yuv[1] = (char *)calloc(1, PAL_W/2 * PAL_H/2);  
  yuv[2] = (char *)calloc(1, PAL_W/2 * PAL_H/2);  

  // frame/stream parameter
  width = ipipe->width;
  height = ipipe->height;

  if(width==0 || height ==0) fprintf(stderr,"(%s) invalid frame parameter %dx%d\n", __FILE__, width, height);
  
  // decoder init
  vo_setup(output, width, height);
  
  // read frame by frame - decode into BRG - pipe to stdout
  
  while(yuv_read_frame(ipipe->fd_in, yuv, width, height)) 
    directdraw_rgb(output, yuv);
  
  // ends
  vo_close(output);
  
  import_exit(0);
}
