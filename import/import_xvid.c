/*
 *  import_xvid.c
 *
 *  Copyright (C) Thomas Östreich - January 2002
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#else
# ifdef SYSTEM_DARWIN
#  include "../libdldarwin/dlfcn.h"
# endif
#endif

#include "transcode.h"
#include "avilib.h"

/* Decoder API hasn't changed from API2 to API3, so we can use both */
#include "../export/xvid3.h"

#define MOD_NAME    "import_xvid.so"
#define MOD_VERSION "v0.0.3 (2003-11-29)"
#define MOD_CODEC   "(video) XviD/OpenDivX/DivX 4.xx/5.xx"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_RGB|TC_CAP_YUV|TC_CAP_VID;

#define MOD_PRE xvid
#include "import_def.h"
static int frame_size=0;

static int (*XviD_decore)(void *para0, int opt, void *para1, void *para2);
static int (*XviD_init)(void *para0, int opt, void *para1, void *para2);
static void *XviD_decore_handle=NULL;
static void *handle=NULL;

static int global_colorspace;
static int done_seek=0;

static int x_dim, y_dim;

#define XVID_SHARED_LIB_NAME "libxvidcore.so"

static int xvid2_init(char *path) {
#ifdef SYS_BSD
	const
#endif    
		char *error;
	char modules[6][TC_BUF_MAX];
	char *module;
	int i;
	

	/* First we build all lib names we will try to load
	 *  - xvid3 decoders to have bframe support
	 *  - then xvid2 decoders
	 *  - bare soname as a fallback */
	sprintf(modules[0], "%s/%s.%d", path, XVID_SHARED_LIB_NAME, 3);
	sprintf(modules[1], "%s.%d", XVID_SHARED_LIB_NAME, 3);
	sprintf(modules[2], "%s/%s.%d", path, XVID_SHARED_LIB_NAME, 2);
	sprintf(modules[3], "%s.%d", XVID_SHARED_LIB_NAME, 2);
	sprintf(modules[4], "%s/%s", path, XVID_SHARED_LIB_NAME);
	sprintf(modules[5], "%s", XVID_SHARED_LIB_NAME);

	for(i=0; i<6; i++) {
		module = modules[i];

		if(verbose_flag & TC_DEBUG)
			fprintf(stderr,	"[%s] Trying to load shared lib %s\n",
				MOD_NAME, module);

		/* Try loading the shared lib */
		handle = dlopen(modules[i], RTLD_GLOBAL| RTLD_LAZY);

		/* We need to fetch every dlerror() */
		error = dlerror();

		/* Test wether loading succeeded */
		if(handle != NULL)
			goto so_loaded;
	}

	/* None of the modules were available */
	fprintf(stderr, "dlopen: %s\n", error);
	return(-1);

 so_loaded:
	if(verbose_flag & TC_DEBUG)
		fprintf(stderr,	"[%s] Using shared lib %s\n",
			MOD_NAME, module);

	/* Import the XviD init entry point */
	XviD_init   = dlsym(handle, "xvid_init");
    
	/* Something went wrong */
	error = dlerror();
	if(error != NULL)  {
		fprintf(stderr, "XviD_init: %s\n", error);
		return(-1);
	}

	/* Import the XviD encoder entry point */
	XviD_decore = dlsym(handle, "xvid_decore");

	error = dlerror();
	/* Something went wrong */
	if(error != NULL)  {
		fprintf(stderr, "XviD_decore: %s\n", error);
		return(-1);
	}

	return(0);
}

static avi_t *avifile=NULL;

static int pass_through=0;

//temporary video buffer
static char *buffer;

static unsigned char *bufalloc(size_t size)
{

#ifdef HAVE_GETPAGESIZE
   long buffer_align=getpagesize();
#else
   long buffer_align=0;
#endif

   char *buf = malloc(size + buffer_align);

   long adjust;

   if (buf == NULL) {
       fprintf(stderr, "(%s) out of memory", __FILE__);
   }
   
   adjust = buffer_align - ((long) buf) % buffer_align;

   if (adjust == buffer_align)
      adjust = 0;

   return (unsigned char *) (buf + adjust);
}

/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  XVID_INIT_PARAM xinit;
  XVID_DEC_PARAM xparam;
  int xerr;
  char *codec_str;

  if(param->flag == TC_VIDEO) {
    
    if(avifile==NULL)  {
      if(vob->nav_seek_file) {
	if(NULL == (avifile = AVI_open_input_indexfile(vob->video_in_file,0,vob->nav_seek_file))){
	  AVI_print_error("avi open error");
	  return(TC_IMPORT_ERROR); 
	} 
      } else {
	if(NULL == (avifile = AVI_open_input_file(vob->video_in_file,1))){
	  AVI_print_error("avi open error");
	  return(TC_IMPORT_ERROR); 
	} 
      }
    }
   
    // vob->offset contains the last keyframe
    if (!done_seek && vob->vob_offset>0) {
	AVI_set_video_position(avifile, vob->vob_offset);
	done_seek=1;
    }

    
    codec_str = AVI_video_compressor(avifile);
    if(strlen(codec_str)==0) {
      printf("invalid AVI file codec\n");
      return(TC_IMPORT_ERROR); 
    }
    if (!strcasecmp(codec_str, "DIV3") ||
        !strcasecmp(codec_str, "MP43") ||
        !strcasecmp(codec_str, "MPG3") ||
        !strcasecmp(codec_str, "AP41")) {
      fprintf(stderr, "[%s] The XviD codec does not support MS-MPEG4v3 " \
              "(aka DivX ;-) aka DivX3).\n", MOD_NAME);
      return(TC_IMPORT_ERROR);
    }

    //load the codec
    //if(xvid2_init("/data/scr/comp/video/xvid/xvid_20030610/xvidcore/build/generic")<0) {
    if(xvid2_init(vob->mod_path)<0) {
      printf("failed to init Xvid codec\n");
      return(TC_IMPORT_ERROR); 
    }
    
    xinit.cpu_flags = 0;
    XviD_init(NULL, 0, &xinit, NULL);
  
    //important parameter
    xparam.width = AVI_video_width(avifile);
    xparam.height = AVI_video_height(avifile);
    x_dim = xparam.width;
    y_dim = xparam.height;

    xerr = XviD_decore(NULL, XVID_DEC_CREATE, &xparam, NULL);

    if(xerr == XVID_ERR_FAIL) {
      printf("codec open error\n");
      return(TC_EXPORT_ERROR); 
    }
    XviD_decore_handle=xparam.handle;

    frame_size = xparam.width * xparam.height * 3;
    switch(vob->im_v_codec) {
      case CODEC_RGB:
        global_colorspace = XVID_CSP_RGB24 | XVID_CSP_VFLIP;
        frame_size = xparam.width * xparam.height * 3;
        break;
      case CODEC_YUV:
        global_colorspace = XVID_CSP_YV12;
	frame_size = (xparam.width * xparam.height * 3)/2;
	break;
      case CODEC_RAW:
      case CODEC_RAW_YUV:
	pass_through=1;
	break;
    }
    
    if ((buffer = bufalloc(frame_size))==NULL) {
      perror("out of memory");
      return(TC_EXPORT_ERROR); 
    } else
      memset(buffer, 0, frame_size);  
    
    param->fd = NULL;
    
    return(0);
  }
  
  return(TC_IMPORT_ERROR);
}

// Determine of the compressed frame is a keyframe for direct copy
static int divx4_is_key(unsigned char *data, long size)
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

/* ------------------------------------------------------------ 
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/

MOD_decode {
  XVID_DEC_FRAME xframe;
  int key, xerr;
  long bytes_read;

  if(param->flag == TC_VIDEO) {
    bytes_read = (pass_through) ?
                 AVI_read_frame(avifile, param->buffer, &key) : 
                 AVI_read_frame(avifile, buffer, &key);
	
    if( bytes_read < 0)
      return(TC_IMPORT_ERROR); 

    if (key)
      param->attributes |= TC_FRAME_IS_KEYFRAME;

    // PASS_THROUGH MODE
    if(pass_through) {
      if (divx4_is_key((unsigned char *)param->buffer, (long) param->size))
	  param->attributes |= TC_FRAME_IS_KEYFRAME;
      param->size = (int) bytes_read;
      memcpy(param->buffer, buffer, bytes_read); 

      return(0);
    }

    xframe.bitstream = buffer;
    xframe.length = bytes_read;
    xframe.stride = x_dim;
    xframe.image = param->buffer;
    xframe.colorspace = global_colorspace;
    xframe.general = 0;
    param->size = frame_size;

    xerr = XviD_decore(XviD_decore_handle, XVID_DEC_DECODE, &xframe, NULL);
    if (xerr != XVID_ERR_OK) {
      fprintf(stderr, "[%s] frame decoding failed. Perhaps you're trying to " \
             "decode MS-MPEG4v3 (aka DivX ;-) aka DivX3)?\n", MOD_NAME);
      return(TC_IMPORT_ERROR);
    }
    
    return(0);
  }
   
  return(TC_IMPORT_ERROR);
}
    
/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{  
  if(param->flag == TC_VIDEO) {
    int xerr;

    xerr = XviD_decore(XviD_decore_handle, XVID_DEC_DESTROY, NULL, NULL);
    if (xerr == XVID_ERR_FAIL)
      printf("encoder close error\n");

    //remove codec
    dlclose(handle);
    done_seek=0;

    return(0);
  }

  return(TC_IMPORT_ERROR);
}

