/*
 *  decode_mpeg2.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *  Copyright (C) 1999-2001 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>

#include "ioaux.h"
#include "video_out.h"
#include "mpeg2.h"
#include "mm_accel.h"

extern vo_open_t vo_ppmpipe_open;
extern vo_open_t vo_yuvpipe_open;
extern vo_open_t vo_yuv_open;

// extern vo_instance_t * vo_open(vo_open_t, int (*)(char *, int));

extern void mpeg2_mc_info(uint32_t accel);
extern void mpeg2_idct_info(uint32_t accel);

#define BUFFER_SIZE 262144
static uint8_t buffer[BUFFER_SIZE];

static FILE *in_file;
static FILE *out_file;

static mpeg2dec_t mpeg2dec;

static int verbose;

static void es_loop(void)
{
  uint8_t * end;
  
  do {
    end = buffer + fread (buffer, 1, BUFFER_SIZE, in_file);
    
    mpeg2_decode_data (&mpeg2dec, buffer, end);
    
  } while (end == buffer + BUFFER_SIZE);
}


static int outstream(char *framebuffer, int bytes)
{
    if (fwrite(framebuffer, bytes, 1, out_file)!= 1) {
	fprintf(stderr, "(%s) video write failed.\n", __FILE__);
	import_exit(0);
  }
  return(1);
}

/* ------------------------------------------------------------ 
 *
 * decoder thread
 *
 * ------------------------------------------------------------*/

void decode_mpeg2(decode_t *decode)
{
  
  vo_instance_t *output=NULL;
  uint32_t accel;

  verbose = decode->verbose;

  accel = mm_accel () | MM_ACCEL_MLIB;
  vo_accel(accel);


  if(decode->format == TC_CODEC_YV12) output = vo_open(vo_yuvpipe_open, outstream);
  if(decode->format == TC_CODEC_RGB) output = vo_open(vo_ppmpipe_open, outstream);

  in_file = fdopen(decode->fd_in, "r");
  out_file = fdopen(decode->fd_out, "w");
   
  mpeg2_init(&mpeg2dec, accel, output);

  if(verbose & TC_DEBUG) {
      mpeg2_mc_info(accel);
      mpeg2_idct_info(accel);
  }

  es_loop();
  
  mpeg2_close(&mpeg2dec);
  
  vo_close(output);
  
  import_exit(0);
}
