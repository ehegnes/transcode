/*
 *  import_rawlist.c
 *
 *  Copyright (C) Thomas Östreich - July 2002
 *
 *  This file is part of transcode, a video stream processing tool
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

#define MOD_NAME    "import_rawlist.so"
#define MOD_VERSION "v0.1.2 (2003-10-14)"
#define MOD_CODEC   "(video) YUV/RGB raw frames"

#include "transcode.h"
#include "aclib/imgconvert.h"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_RGB | TC_CAP_YUV | TC_CAP_AUD | TC_CAP_YUV422;

#define MOD_PRE rawlist
#include "import_def.h"

#include <time.h>
#include <sys/types.h>


#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static FILE *fd;
static char buffer[PATH_MAX+2];

static int bytes=0;
static int out_bytes=0;
static uint8_t *video_buffer=NULL;

/* ------------------------------------------------------------
 *
 * open stream
 *
 * ------------------------------------------------------------*/

static ImageFormat srcfmt = IMG_RGB24, destfmt = IMG_RGB24;

MOD_open
{
    if (param->flag == TC_AUDIO) {
	return(0);
    }
    if (param->flag != TC_VIDEO) {
	return(TC_IMPORT_ERROR);
    }

    param->fd = NULL;

    if (vob->im_v_string) {
	if        (!strcasecmp(vob->im_v_string, "RGB")) {
	    srcfmt = IMG_RGB24;
	    destfmt = IMG_RGB_DEFAULT;
	    bytes = vob->im_v_width * vob->im_v_height * 3;
	} else if (!strcasecmp(vob->im_v_string, "yuv420p") ||
		   !strcasecmp(vob->im_v_string, "i420")) {
	    srcfmt = IMG_YUV420P;
	    destfmt = IMG_YUV_DEFAULT;
	    bytes = vob->im_v_width * vob->im_v_height * 3 / 2;
	} else if (!strcasecmp(vob->im_v_string, "yv12")) {
	    srcfmt = IMG_YV12;
	    destfmt = IMG_YUV_DEFAULT;
	    bytes = vob->im_v_width * vob->im_v_height * 3 / 2;
	} else if (!strcasecmp(vob->im_v_string, "gray") ||
		   !strcasecmp(vob->im_v_string, "grey")) {
	    srcfmt = IMG_GRAY8;
	    if (vob->im_v_codec == CODEC_RGB)
		destfmt = IMG_RGB_DEFAULT;
	    else
		destfmt = IMG_YUV_DEFAULT;
	    bytes = vob->im_v_width * vob->im_v_height;
	} else if (!strcasecmp(vob->im_v_string, "yuy2")) {
	    srcfmt = IMG_YUY2;
	    if (vob->im_v_codec == CODEC_YUV422) {
		destfmt = IMG_YUV422P;
	    } else {
		destfmt = IMG_YUV_DEFAULT;
	    }
	    bytes = vob->im_v_width * vob->im_v_height * 2;
	} else if (!strcasecmp(vob->im_v_string, "uyvy")) {
	    srcfmt = IMG_UYVY;
	    if (vob->im_v_codec == CODEC_YUV422) {
		destfmt = IMG_YUV422P;
	    } else {
		destfmt = IMG_YUV_DEFAULT;
	    }
	    bytes = vob->im_v_width * vob->im_v_height * 2;
	} else if (!strcasecmp(vob->im_v_string, "argb")) {
	    srcfmt = IMG_ARGB32;
	    destfmt = IMG_RGB24;
	    bytes = vob->im_v_width * vob->im_v_height * 4;
	} else if (!strcasecmp(vob->im_v_string, "ayuv")) {
	    tc_log_error(MOD_NAME, "ayuv not supported");
	    return(TC_IMPORT_ERROR);
	} else {
	    tc_log_error(MOD_NAME, "Unknown format {rgb, gray, argb, ayuv, yv12, i420, yuy2, uyvy}");
	    return(TC_IMPORT_ERROR);
	}
    }

    if((fd = fopen(vob->video_in_file, "r"))==NULL) {
	tc_log_error(MOD_NAME, "You need to specify a filelist as input");
	return(TC_IMPORT_ERROR);
    }

    switch(vob->im_v_codec) {
      case CODEC_RGB:
	out_bytes = vob->im_v_width * vob->im_v_height * 3;
	break;
      case CODEC_YUV:
	out_bytes = vob->im_v_width * vob->im_v_height
                  + 2 * ((vob->im_v_width/2) * (vob->im_v_height/2));
	break;
      case CODEC_YUV422:
	out_bytes = (vob->im_v_width * vob->im_v_height * 2);
	break;
    }

    if((video_buffer = (uint8_t *)calloc(1, out_bytes)) == NULL) {
	fprintf(stderr, "(%s) out of memory", __FILE__);
	return(TC_IMPORT_ERROR);
    }

    return(0);
}


/* ------------------------------------------------------------
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/

MOD_decode {

  char
    *filename = NULL;

  int
    fd_in=0, n;

  if(param->flag == TC_AUDIO) return(0);

retry:

  // read a filename from the list
  if(fgets (buffer, PATH_MAX, fd)==NULL) return(TC_IMPORT_ERROR);

  filename = buffer;

  n=strlen(filename);
  if(n<2) return(TC_IMPORT_ERROR);
  filename[n-1]='\0';

  //read the raw frame

  if((fd_in = open(filename, O_RDONLY))<0) {
    tc_log_warn(MOD_NAME, "Opening file \"%s\" failed!", filename);
    perror("open file");
    goto retry;
  }

  if(tc_pread(fd_in, param->buffer, bytes) != bytes) {
    perror("image parameter mismatch");
    close(fd_in);
    goto retry;
  }

  if(srcfmt != destfmt) {
    uint8_t *src[3], *dest[3];
    YUV_INIT_PLANES(src, param->buffer, srcfmt, vob->im_v_width,
		    vob->im_v_height);
    YUV_INIT_PLANES(dest, video_buffer, srcfmt, vob->im_v_width,
		    vob->im_v_height);
    ac_imgconvert(src, srcfmt, dest, destfmt,
		  vob->im_v_width, vob->im_v_height);
    ac_memcpy(param->buffer, video_buffer, out_bytes);
  }

  param->size=out_bytes;
  param->attributes |= TC_FRAME_IS_KEYFRAME;

  close(fd_in);

  return(0);
}

/* ------------------------------------------------------------
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{

  if(param->flag == TC_VIDEO) {

    if(fd != NULL) fclose(fd);
    if (param->fd != NULL) pclose(param->fd);

    return(0);
  }

  if(param->flag == TC_AUDIO) return(0);

  return(TC_IMPORT_ERROR);
}


