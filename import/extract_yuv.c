/*
 *  extract_yuv.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
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

#include "ioaux.h"
#include "avilib.h"

#define MAX_BUF 4096

static int yuv_readwrite_frame (int in_fd, int out_fd, char *buffer, int bytes)
{
    
    unsigned char magic[7];

    int padding, blocks, n;
    
    for(;;) {
      if (p_read (in_fd, magic, 6) != 6)
	return 0;
      
      // Check for extra header in case input was tccat'ed
      if (strncmp (magic, "YUV4MP", 6) == 0) {
	do {
	  if (p_read (in_fd, magic, 1) < 1) return 0;
	} while(magic[0] != '\n');
      } else break;
    }

    if (strncmp (magic, "FRAME\n", 6)) {
	magic[6] = '\0';
	fprintf (stderr, "\nStart of frame is not \"FRAME\\n\"\n");
	return 0;
    }

    padding = bytes % MAX_BUF;
    blocks  = bytes / MAX_BUF;
  
    for (n=0; n<blocks; ++n) {
	
	if (p_read(in_fd, buffer, MAX_BUF) != MAX_BUF)
	    return 0;
	if (p_write(out_fd, buffer, MAX_BUF) != MAX_BUF)
	    return 0;
    }
    
    if (p_read(in_fd, buffer, padding) != padding)
	return 0;
    if (p_write(out_fd, buffer, padding) != padding)
	return 0;
    
    return 1;
}

#define PARAM_LINE_MAX 256

static int yuv_read_header (int fd_in, int *horizontal_size, int *vertical_size, int *frame_rate_code)
{
    int n, nerr, offset;
    char param_line[PARAM_LINE_MAX];
    char *scanfmt;
    
    for (n = 0; n < PARAM_LINE_MAX; n++) {
	if ((nerr = read (fd_in, param_line + n, 1)) < 1) {
	    fprintf (stderr, "Error reading header from stdin\n");
	    /* set errno if nerr == 0 ? */
	    return -1;
	}
	if (param_line[n] == '\n')
	    break;
    }
    if (n == PARAM_LINE_MAX) {
	fprintf (stderr,
		 "Didn't get linefeed in first %d characters of data\n",
		 PARAM_LINE_MAX);
	/* set errno to EBADMSG? */
	return -1;
    }
    param_line[n] = 0;           /* Replace linefeed by end of string */
    
    if (strncmp (param_line, "YUV4MPEG ", 9) && 
	strncmp (param_line, "YUV4MPEG2 ", 10)) {
	fprintf (stderr, "Input does not start with \"YUV4MPEG \"\n");
	fprintf (stderr, "This is not a valid input for me\n");
	/* set errno to EBADMSG? */
	return -1;
    }

    scanfmt = (param_line[8] == '2')? "W%d H%d F%d": "%d %d %d";
    offset  = (param_line[8] == '2')? 10:9;

    sscanf (param_line + offset, scanfmt, horizontal_size, vertical_size,
	    frame_rate_code);
    
    nerr = 0;
    if (*horizontal_size <= 0) {
	fprintf (stderr, "Horizontal size illegal\n");
	nerr++;
    }
    if (*vertical_size <= 0) {
	fprintf (stderr, "Vertical size illegal\n");
	nerr++;
    }
    
    return nerr ? -1 : 0;
}

/* ------------------------------------------------------------ 
 *
 * yuv extract thread
 *
 * magic: TC_MAGIC_YUV4MPEG
 *        TC_MAGIC_RAW      <-- default
 *
 *
 * ------------------------------------------------------------*/

void extract_yuv(info_t *ipipe)
{

  avi_t *avifile=NULL;
  char *video;
  
  int key, error=0;
  
  char *buffer;
  
  int width, height, frc;    

  long frames, bytes, n;
  
  switch(ipipe->magic) {
    
  case TC_MAGIC_YUV4MPEG:
    
    // read mjpeg tools header and dump it
    if(yuv_read_header(ipipe->fd_in, &width, &height, &frc)<0) {
      error=1;
      break;
    }
    
    // allocate space
    if((buffer = (char *)calloc(1, MAX_BUF))==NULL) {
      perror("out of memory");
      error=1;
      break;
    }
    
    // read frame by frame - decapitate - and pipe to stdout
    bytes = width*height + 2 * (width/2) * (height/2);
    
    do {
      ;;
    } while (yuv_readwrite_frame(ipipe->fd_in, ipipe->fd_out, buffer, bytes)); 
    
    free(buffer);
    
    break;
    
  case TC_MAGIC_AVI:
    
    // scan file
    if (ipipe->nav_seek_file) {
      if(NULL == (avifile = AVI_open_indexfd(ipipe->fd_in,0,ipipe->nav_seek_file))) {
	AVI_print_error("AVI open");
	import_exit(1);
      }
    } else {
      if(NULL == (avifile = AVI_open_fd(ipipe->fd_in,1))) {
	AVI_print_error("AVI open");
	import_exit(1);
      }
    }
    
    // read video info;
    
    frames =  AVI_video_frames(avifile);
    if (ipipe->frame_limit[1] < frames)
      {
	frames=ipipe->frame_limit[1];
      }
    
    
    if(ipipe->verbose & TC_STATS) fprintf(stderr, "(%s) %ld video frames\n", __FILE__, frames);
    
    // allocate space, assume max buffer size
    if((video = (char *)calloc(1, SIZE_RGB_FRAME))==NULL) {
      fprintf(stderr, "(%s) out of memory", __FILE__);
      error=1;
      break;
    }
    
    (int)AVI_set_video_position(avifile,ipipe->frame_limit[0]);
    for (n=ipipe->frame_limit[0]; n<=frames; ++n) {
      
      // video
      if((bytes = AVI_read_frame(avifile, video, &key))<0) {
	error=1;
	break;
      }
      if(p_write(ipipe->fd_out, video, bytes)!=bytes) {
	error=1;
	break;
      }
    }
    
    free(video);
    
    break;
    
    
  case TC_MAGIC_RAW:
  default:
    
    if(ipipe->magic == TC_MAGIC_UNKNOWN)
      fprintf(stderr, "(%s) no file type specified, assuming (%s)\n", 
	      __FILE__, filetype(TC_MAGIC_RAW));
    
    
    error=p_readwrite(ipipe->fd_in, ipipe->fd_out);
    
    break;
  }
  
  import_exit(error);
}

float framerates[] = { 0, 23.976, 24.0, 25.0, 29.970, 30.0, 50.0, 59.940, 60.0 };

void probe_yuv(info_t *ipipe)
{
  
    //only YUV4MPEG supported

    int code;

    yuv_read_header(ipipe->fd_in, &ipipe->probe_info->width, &ipipe->probe_info->height, &code);

    if(code>0 || code <=8) ipipe->probe_info->fps = framerates[code];
 
    ipipe->probe_info->frc=code;
    ipipe->probe_info->codec=TC_CODEC_YV12;
    ipipe->probe_info->magic=TC_MAGIC_YUV4MPEG;
}
