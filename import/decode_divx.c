/*
 *  decode_divx.c
 *
 *  Copyright (C) Tilmann Bitterberg - March 2003
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
#include <dlfcn.h>

#include "transcode.h"
#include "ioaux.h"
#include "divx_decore.h"
#include "avilib.h"

#define BUFFER_SIZE SIZE_RGB_FRAME
#define MOD_NAME "decode_divx"

static FILE *in_file;
static FILE *out_file;

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static int verbose_flag=TC_QUIET;
static int codec, frame_size=0, uv_size=0;
static unsigned long divx_version=DEC_OPT_FRAME;

static int black_frames = 0;


static DEC_PARAM    *divx = NULL;
static DEC_FRAME    *decFrame = NULL;
static DEC_FRAME_INFO *decInfo = NULL;

static int decore_in_use = 0; // semaphore to keep track of usage count

// dl stuff
static int (*divx_decore)(unsigned long para0, unsigned long opt, void *para1, void *para2);
static void *handle=NULL;
static unsigned long divx_id=0x4711;
static char module[TC_BUF_MAX];

#define MODULE   "libdivxdecore.so"
#define MODULE_V "libdivxdecore.so.0"

//special codec information
static int pass_through=0, pass_through_decode=0;

//temporary video buffer
static char *in_buffer = NULL;
static char *out_buffer = NULL;


inline static int stream_read_char(char *d)
{
    return (*d & 0xff);
}

inline static unsigned int stream_read_dword(char *s)
{
    unsigned int y;
    y=stream_read_char(s);
    y=(y<<8)|stream_read_char(s+1);
    y=(y<<8)|stream_read_char(s+2);
    y=(y<<8)|stream_read_char(s+3);
    return y;
}

// Determine of the compressed frame is a keyframe for direct copy
int divx4_is_key(unsigned char *data, long size)
{
        int result = 0;
        int i;

        for(i = 0; i < size - 5; i++)
        {
                if( data[i]     == 0x00 && 
                        data[i + 1] == 0x00 &&
                        data[i + 2] == 0x01 &&
                        data[i + 3] == 0xb6)
                {
                        if((data[i + 4] & 0xc0) == 0x0) 
                                return 1;
                        else
                                return 0;
                }
        }
        
        return result;
}

int divx3_is_key(char *d)
{
    int32_t c=0;
    
    c=stream_read_dword(d);
    if(c&0x40000000) return(0);
    
    return(1);
}

static unsigned char *bufalloc(size_t size)
{

#ifdef HAVE_GETPAGESIZE
   int buffer_align=getpagesize();
#else
   int buffer_align=0;
#endif

   char *buf = malloc(size + buffer_align);

   int adjust;

   if (buf == NULL) {
       fprintf(stderr, "(%s) out of memory", __FILE__);
   }
   
   adjust = buffer_align - ((int) buf) % buffer_align;

   if (adjust == buffer_align)
      adjust = 0;

   return (unsigned char *) (buf + adjust);
}

static int divx_init(char *path) {
#ifdef __FreeBSD__
    const
#endif    
    char *error;

    handle = NULL;

    // try transcode's module directory
    if (!handle) {
      // (try 5.x "libdivxencore.so.0" style)
      sprintf(module, "%s/%s", path, MODULE_V);
      handle = dlopen(module, RTLD_LAZY); 
    }
    if (!handle) {
      // (try 4.x "libdivxencore.so" style)
      sprintf(module, "%s/%s", path, MODULE);
      handle = dlopen(module, RTLD_LAZY); 
    }

    //try the default:
    if (!handle) {
      // (try 5.x "libdivxencore.so.0" style)
      sprintf(module, "%s", MODULE_V);
      handle = dlopen(module, RTLD_LAZY); 
    }
    if (!handle) {
      // (try 4.x "libdivxencore.so" style)
      sprintf(module, "%s", MODULE);
      handle = dlopen(module, RTLD_LAZY); 
    }
    
    if (!handle) {
      fprintf(stderr, "[%s] %s\n", MOD_NAME, dlerror());
      return(-1);
    } else {  
      if(verbose_flag & TC_DEBUG) 
        fprintf(stderr, "[%s] Loading external codec module %s\n", MOD_NAME, module); 
    }

    divx_decore = dlsym(handle, "decore");   
    
    if ((error = dlerror()) != NULL)  {
      fprintf(stderr, "[%s] %s\n", MOD_NAME, error);
      return(-1);
    }

    return(0);
}


/* ------------------------------------------------------------ 
 *
 * decoder thread
 *
 * ------------------------------------------------------------*/

void decode_divx(info_t *ipipe)
{
  
  char *codec_str=NULL;
  char *mp4_ptr=NULL;

  long bytes_read=0;
  long frame_length=0;
  int frames=0;

  verbose_flag = ipipe->verbose;


  fprintf(stderr, "DECODE_DIVX is broken!\n");

  //load the codec

  if (!decore_in_use) {
      if(divx_init(MOD_PATH)<0) {
	  fprintf(stderr, "failed to init DivX 4.xx/5.xx codec");
	  import_exit(1);
      }
  }

  if ((divx = malloc(sizeof(DEC_PARAM)))==NULL) {
      perror("out of memory");
      import_exit(1); 
  } else
      memset(divx,0x00,sizeof(DEC_PARAM));

  //important parameter
  //divx->x_dim = ipipe->width;
  //divx->y_dim = ipipe->height;
  divx->time_incr = 15; //default

  fprintf(stderr, "widht (%d), height (%d)\n", ipipe->width, ipipe->height);


  //check DivX version:
  codec_str = "DIVX"; // XXX

  if(strlen(codec_str)==0) {
      fprintf(stderr, "invalid AVI file codec");
      import_exit(1); 
  }

#if DECORE_VERSION >= 20020303
  if (verbose_flag & TC_DEBUG) 
      fprintf(stderr, "[%s] using DivX5 decoder syntax.\n", MOD_NAME);

  if (strcasecmp(codec_str,"DIV3")==0)
      divx->codec_version = 311;
  else
      divx->codec_version = 500;

  divx->build_number = 0;
  divx_version=DEC_OPT_FRAME;
#else
  if(strcasecmp(codec_str,"DIV3")==0) {
      divx_version=DEC_OPT_FRAME_311;
      if(verbose_flag & TC_DEBUG) 
	  fprintf(stderr, "[%s] detected DivX divx_version3.11 codec\n", MOD_NAME);
  }
  else
      divx_version=DEC_OPT_FRAME;

#endif

  codec = ipipe->format;

  switch(codec) {

      case TC_CODEC_RGB:

	  divx->output_format=DEC_RGB24;
	  frame_size = divx->x_dim * divx->y_dim * 3;
	  fprintf(stderr, "RGB\n");
	  break;

      case TC_CODEC_YV12:

	  divx->output_format=DEC_YV12;
	  frame_size = (divx->x_dim * divx->y_dim * 3)/2;
	  fprintf(stderr, "VY12\n");
	  break;

#if 0
      case TC_CODEC_RAW:

	  pass_through=1;
	  divx->output_format=DEC_420;
	  break;

      case TC_CODEC_RAW_YUV:

	  pass_through=1;
	  pass_through_decode=1;
	  divx->output_format=DEC_YV12;
	  frame_size = (divx->x_dim * divx->y_dim * 3)/2;
	  break;
#endif

      default:

	  fprintf(stderr, "Error: no known codec set (0x%x)!\n", codec);
	  import_exit(1);
	  break;
  }

  //----------------------------------------
  //
  // setup decoder
  //
  //----------------------------------------

  if(divx_decore(divx_id, DEC_OPT_INIT, divx, NULL) < 0) {
      fprintf(stderr, "codec DEC_OPT_INIT error");
      import_exit(1); 
  } else
      ++decore_in_use;

  if ((decFrame = malloc(sizeof(DEC_FRAME)))==NULL) {
      perror("out of memory");
      import_exit(1); 
  } else
      memset(decFrame,0x00,sizeof(DEC_FRAME));

  if ((decInfo = malloc(sizeof(DEC_FRAME_INFO)))==NULL) {
      perror("out of memory");
      import_exit(1); 
  }

  if (!in_buffer) {
      if ((in_buffer = bufalloc(BUFFER_SIZE))==NULL) {
	  perror("out of memory");
	  import_exit(1); 
      } 
      else {
	  memset(in_buffer, 0, BUFFER_SIZE);
      }
  }

  if (!out_buffer) {
      if ((out_buffer = bufalloc(BUFFER_SIZE))==NULL) {
	  perror("out of memory");
	  import_exit(1); 
      } 
      else {
	  memset(out_buffer, 0, BUFFER_SIZE);
      }
  }


  // DECODE MAIN LOOP
 
  //in_file = fdopen(ipipe->fd_in, "r");
  //out_file = fdopen(ipipe->fd_out, "w");

  bytes_read = p_read(ipipe->fd_in, (char*) in_buffer, BUFFER_SIZE);
  mp4_ptr = in_buffer;

  do {

      int mp4_size = (in_buffer + BUFFER_SIZE - mp4_ptr);
      char * tptr=NULL;

      fprintf(stderr, "mp4_size %d\n", mp4_size);

    
      if(bytes_read<0) import_exit(1);

      /* buffer more than half empty -> Fill it */
      if (mp4_ptr > in_buffer + BUFFER_SIZE/2) {
	  int rest = (in_buffer + BUFFER_SIZE - mp4_ptr);
	  fprintf(stderr, "FILL rest %d\n", rest);

	  /* Move data if needed */
	  if (rest)
	      memcpy(in_buffer, mp4_ptr, rest);

	  /* Update mp4_ptr */
	  mp4_ptr = in_buffer; 

	  /* read new data */
	  if ( (bytes_read = p_read(ipipe->fd_in, (char*) (in_buffer+rest), BUFFER_SIZE - rest) ) 
		  != (BUFFER_SIZE - rest)) {
	      fprintf(stderr, "read failed read (%ld) should (%d)\n", bytes_read, BUFFER_SIZE - rest);
	      import_exit(1);
	  }
      }


      // BLACK FRAME HERE

      //need to decode the frame
      tptr = mp4_ptr;
      decFrame->bitstream = mp4_ptr;
      decFrame->stride = divx->x_dim;
      decFrame->bmp = out_buffer;
      decFrame->length = (int) mp4_size;
      decFrame->render_flag = 1;

      
      if(divx_decore(divx_id, divx_version, decFrame, NULL) != DEC_OK) {
	fprintf(stderr, "codec DEC_OPT_FRAME error");
	import_exit(1);
      }
      frame_length = decFrame->length;

      mp4_ptr += frame_length;

      fprintf(stderr, "[%s] decoded frame (%ld) (%d) |%p| |%p|\n", MOD_NAME,
	      frame_length, frame_size, tptr, decFrame->bitstream);

      if (p_write (ipipe->fd_out, (char *)out_buffer, frame_length) != frame_length) {
	  fprintf(stderr, "writeout failed\n");
	  import_exit(1);
      }
      
      //fprintf(stderr, "Next frame\n");

      
    } while (1);

  import_exit(0);
}















#if 0
      /*
	Divx 3.11 encoded video sometimes have frames with zero
	length data. These frames need to be cleared (eg filled
	with black). The frame after the zero length frame 
	sometimes needs to be cleared too, I don't know why but
	it solves my problems... Works for me. (TM)
       */

      if(black_frames) {
	fprintf(stderr, "bytes_read=(%ld)\n", bytes_read);
      }

      if (bytes_read == 0) {
	black_frames = 2;
      }

      if(black_frames > 0) {
	if(verbose_flag & TC_DEBUG) fprintf(stderr, "bytes_read=(%ld)\n", bytes_read);

	switch(codec) {
      
	case CODEC_RGB:
	  memset(in_buffer,0x00,frame_size);
	  break;
      
	case CODEC_YUV:
	  uv_size = frame_size/3;
	  memset(in_buffer,0x10,uv_size*2);
	  memset(in_buffer + uv_size*2,0x80,uv_size);
	  break;
	}

	black_frames--;
      }
#endif

