/*
 *  import_rawlist.c
 *
 *  Copyright (C) Thomas Östreich - July 2002
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "ioaux.h"

#include "transcode.h"

#define MOD_NAME    "import_rawlist.so"
#define MOD_VERSION "v0.1.1 (2002-11-18)"
#define MOD_CODEC   "(video) YUV/RGB raw frames"

#define MOD_PRE rawlist
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_RGB|TC_CAP_YUV|TC_CAP_AUD;

static FILE *fd; 
static char buffer[PATH_MAX+2];

static int bytes=0;
static int out_bytes=0;
static char *video_buffer=NULL;
static int alloc_buffer;
  
/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/
static void dummyconvert(char *dest, char *input, int width, int height) { }

static void (*convfkt)(char *, char *, int, int) = dummyconvert;

static void uyvy2toyv12(char *dest, char *input, int width, int height) 
{

    int i,j,w2;
    char *y, *u, *v;

    w2 = width/2;

    //I420
    y = dest;
    v = dest+width*height;
    u = dest+width*height*5/4;
    
    for (i=0; i<height; i+=2) {
      for (j=0; j<w2; j++) {
	
	/* UYVY.  The byte order is CbY'CrY' */
	*u++ = *input++;
	*y++ = *input++;
	*v++ = *input++;
	*y++ = *input++;
      }

      //down sampling
      u -= w2;
      v -= w2;
      
      /* average every second line for U and V */
      for (j=0; j<w2; j++) {
	  int un = *u & 0xff;
	  int vn = *v & 0xff; 

	  un += *input++ & 0xff;
	  *u++ = un>>1;

	  *y++ = *input++;

	  vn += *input++ & 0xff;
	  *v++ = vn>>1;

	  *y++ = *input++;
      }
    }
}
static void yuy2toyv12(char *dest, char *input, int width, int height) 
{

    int i,j,w2;
    char *y, *u, *v;

    w2 = width/2;

    //I420
    y = dest;
    v = dest+width*height;
    u = dest+width*height*5/4;
    
    for (i=0; i<height; i+=2) {
      for (j=0; j<w2; j++) {
	
	/* packed YUV 422 is: Y[i] U[i] Y[i+1] V[i] */
	*(y++) = *(input++);
	*(u++) = *(input++);
	*(y++) = *(input++);
	*(v++) = *(input++);
      }
      
      //down sampling
      
      for (j=0; j<w2; j++) {
	/* skip every second line for U and V */
	*(y++) = *(input++);
	input++;
	*(y++) = *(input++);
	input++;
      }
    }
}
static void gray2rgb(char *dest, char *input, int width, int height) 
{

    int i;
    
    for (i=0; i<width*height; i++) {
	*dest++ = *input;
	*dest++ = *input;
	*dest++ = *input++;
    }
}

static void gray2yuv(char *dest, char *input, int width, int height) 
{
    memcpy (dest, input, height*width);
    memset (dest+height*width, 128, height*width/2);
}

static void argb2rgb(char *dest, char *input, int width, int height) 
{
	int run;
	int size = width*height;

	for (run = 0; run < size; run++) {

	        input++; // skip alpha
		*dest++ = *input++;
		*dest++ = *input++;
		*dest++ = *input++;
	}
}
static void ayuvtoyv12(char *dest, char *input, int width, int height) 
{

    int i,j,w2;
    char *y, *u, *v, *n = input;

    w2 = width/2;

    //I420
    y = dest;
    v = dest+width*height;
    u = dest+width*height*5/4;
    
    for (i=0; i<height*width/4; i++) {
#if 0
	*v++ = *input++;
	*u++ = *input++;
	*y++ = *input++;
	input++; // a
#endif

	for (j=0; j<4; j++) {
	    input++;
	    input++;
	    *y++ = *input++;
	    input++;
	}
	*u++= 128;
	*v++= 128;

    }
#if 0
    printf("y-dest (%d) v-orig (%d) u-orig (%d) input-i (%d)\n", (int)(y-dest),
	    (int)(v-(dest+width*height)), (int)(u-(dest+width*height*5/4)), (int)(input-n));

    for (i=0; i<height; i+=2) {
      for (j=0; j<w2; j++) {
	
	/* packed YUV 444 is: V[i] U[i] Y[i] a[i] */
	*(v++) = *(input++);
	*(u++) = *(input++);
	*(y++) = *(input++);
	input++;
      }
      
      //down sampling
      
      for (j=0; j<w2; j++) {
	/* skip every second line for U and V */
	input++;
	input++;
	*(y++) = *(input++);
	input++;
      }
    }
#endif
}



MOD_open
{
  if(param->flag == TC_AUDIO) {
      return(0);
  }
  
  if(param->flag == TC_VIDEO) {

    param->fd = NULL;

    if (vob->im_v_string) {
	if        (!strcasecmp(vob->im_v_string, "RGB")) {
	    convfkt = dummyconvert;
	    bytes=vob->im_v_width * vob->im_v_height * 3;
	} else if (!strcasecmp(vob->im_v_string, "yv12") || 
		   !strcasecmp(vob->im_v_string, "i420")) {
	    convfkt = dummyconvert;
	    bytes = vob->im_v_width * vob->im_v_height * 3 / 2;
	} else if (!strcasecmp(vob->im_v_string, "gray") || 
		   !strcasecmp(vob->im_v_string, "grey")) {
	    bytes = vob->im_v_width * vob->im_v_height;
	    if (vob->im_v_codec == CODEC_RGB)
		convfkt = gray2rgb;
	    else 
		convfkt = gray2yuv;
	    alloc_buffer = 1;
	} else if (!strcasecmp(vob->im_v_string, "yuy2")) {
	    convfkt = yuy2toyv12;
	    bytes = vob->im_v_width * vob->im_v_height * 2;
	    alloc_buffer = 1;
	} else if (!strcasecmp(vob->im_v_string, "uyvy")) {
	    convfkt = uyvy2toyv12;
	    bytes = vob->im_v_width * vob->im_v_height * 2;
	    alloc_buffer = 1;
	} else if (!strcasecmp(vob->im_v_string, "argb")) {
	    convfkt = argb2rgb;
	    bytes = vob->im_v_width * vob->im_v_height * 4;
	    alloc_buffer = 1;
	} else if (!strcasecmp(vob->im_v_string, "ayuv")) {
	    convfkt = ayuvtoyv12;
	    bytes = vob->im_v_width * vob->im_v_height * 4;
	    alloc_buffer = 1;
	} else {
	    tc_error("Unknown format {rgb, gray, argb, ayuv, yv12, i420, yuy2, uyvy}");
	}
    }

    if((fd = fopen(vob->video_in_file, "r"))==NULL) {
	    tc_error("You need to specify a filelist as input");
	    return(TC_IMPORT_ERROR);
    }

    switch(vob->im_v_codec) {
      
    case CODEC_RGB:
      if (!bytes)
	  bytes=vob->im_v_width * vob->im_v_height * 3;
      out_bytes=vob->im_v_width * vob->im_v_height * 3;
      break;
      
    case CODEC_YUV:
      if (!bytes)
	  bytes=(vob->im_v_width * vob->im_v_height * 3)/2;
      out_bytes=(vob->im_v_width * vob->im_v_height * 3)/2;
      break;
    }

    if (alloc_buffer)
      if((video_buffer = (char *)calloc(1, SIZE_RGB_FRAME))==NULL) {
	fprintf(stderr, "(%s) out of memory", __FILE__);
	return(TC_IMPORT_ERROR);
      }
    
    return(0);
  }
 
  return(TC_IMPORT_ERROR);
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
    fprintf(stderr, "[%s] Opening file \"%s\" failed!\n", MOD_NAME, filename);
    perror("open file");
    goto retry;
  } 
  
  if (alloc_buffer) {
    if(p_read(fd_in, param->buffer, bytes) != bytes) { 
      perror("image parameter mismatch");
      close(fd_in);
      goto retry;
    }

    convfkt(video_buffer, param->buffer, vob->im_v_width, vob->im_v_height);
    memcpy(param->buffer, video_buffer, out_bytes);
  
  } else  {
    if(p_read(fd_in, param->buffer, bytes) != bytes) { 
      perror("image parameter mismatch");
      close(fd_in);
      goto retry;
    }
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


