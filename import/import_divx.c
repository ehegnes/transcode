/*
 *  import_divx.c
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>

#include "transcode.h"
#include "divx_decore.h"
#include "avilib.h"

#define MOD_NAME    "import_divx.so"
#define MOD_VERSION "v0.2.5 (2002-10-10)"
#define MOD_CODEC   "(video) DivX;-)/XviD/OpenDivX/DivX 4.xx/5.xx"
#define MOD_PRE divx
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_RGB|TC_CAP_YUV|TC_CAP_VID;
static int codec, frame_size=0;
static unsigned long divx_version=DEC_OPT_FRAME;

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

static avi_t *avifile=NULL;

//special codec information
static int pass_through=0, pass_through_decode=0;

//temporary video buffer
static char *buffer = NULL;
#define BUFFER_SIZE SIZE_RGB_FRAME


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
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  
//  DEC_SET dec_set;
  char *codec_str=NULL;

  if(param->flag == TC_VIDEO) {
    
    if(avifile==NULL) 
      if(NULL == (avifile = AVI_open_input_file(vob->video_in_file,1))){
	AVI_print_error("avi open error");
	return(TC_IMPORT_ERROR); 
      }
    
    //load the codec
    
    if (!decore_in_use) {
      if(divx_init(vob->mod_path)<0) {
	printf("failed to init DivX 4.xx/5.xx codec");
	return(TC_IMPORT_ERROR); 
      }
    }
    
    if ((divx = malloc(sizeof(DEC_PARAM)))==NULL) {
      perror("out of memory");
      return(TC_IMPORT_ERROR); 
    } else
      memset(divx,0x00,sizeof(DEC_PARAM));
  
    //important parameter
    divx->x_dim = AVI_video_width(avifile);
    divx->y_dim = AVI_video_height(avifile);
    divx->time_incr = 15; //default


    //check DivX version:
    codec_str = AVI_video_compressor(avifile);

    if(strlen(codec_str)==0) {
	printf("invalid AVI file codec");
	return(TC_IMPORT_ERROR); 
    }

#if DECORE_VERSION >= 20020303
      if (verbose & TC_DEBUG) 
      		printf("[%s] using DivX5 decoder syntax.\n", MOD_NAME);

      if (strcasecmp(codec_str,"DIV3")==0)
		divx->codec_version = 311;
	else
		divx->codec_version = 500;

      divx->build_number = 0;
      divx_version=DEC_OPT_FRAME;
#else
    if(strcasecmp(codec_str,"DIV3")==0) {
	divx_version=DEC_OPT_FRAME_311;
        if(verbose & TC_DEBUG) 
		printf("[%s] detected DivX divx_version3.11 codec\n", MOD_NAME);
    }
    else
	divx_version=DEC_OPT_FRAME;

#endif

    codec=vob->im_v_codec;
    
    switch(codec) {
      
    case CODEC_RGB:
      
      divx->output_format=DEC_RGB24;
      frame_size = divx->x_dim * divx->y_dim * 3;
      break;
      
    case CODEC_YUV:
      
      divx->output_format=DEC_YV12;
      frame_size = (divx->x_dim * divx->y_dim * 3)/2;
      break;
      
    case CODEC_RAW:
	
      pass_through=1;
      divx->output_format=DEC_420;
      break;

    case CODEC_RAW_YUV:
      
      pass_through=1;
      pass_through_decode=1;
      divx->output_format=DEC_YV12;
      frame_size = (divx->x_dim * divx->y_dim * 3)/2;
      break;
    }
    
    //----------------------------------------
    //
    // setup decoder
    //
    //----------------------------------------
    
    if(divx_decore(divx_id, DEC_OPT_INIT, divx, NULL) < 0) {
      printf("codec DEC_OPT_INIT error");
      return(TC_IMPORT_ERROR); 
    } else
	++decore_in_use;
    
    if ((decFrame = malloc(sizeof(DEC_FRAME)))==NULL) {
      perror("out of memory");
      return(TC_IMPORT_ERROR); 
    } else
      memset(decFrame,0x00,sizeof(DEC_FRAME));
 
    if ((decInfo = malloc(sizeof(DEC_FRAME_INFO)))==NULL) {
      perror("out of memory");
      return(TC_IMPORT_ERROR); 
    }
  
    if (!buffer) {
      if ((buffer = bufalloc(BUFFER_SIZE))==NULL) {
	perror("out of memory");
	return(TC_EXPORT_ERROR); 
      } else
	memset(buffer, 0, BUFFER_SIZE);  
    }
    
    //postproc
/*    
    dec_set.postproc_level = vob->quality * 20;  //0-100;
    if(verbose & TC_DEBUG) printf("[%s] decoder postprocessing quality = %d\n", MOD_NAME, dec_set.postproc_level);

    divx_decore(divx_id, DEC_OPT_SETPP, &dec_set, NULL);
  */  
    param->fd = NULL;
    
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

    int key;
    
    long bytes_read=0;

    if(param->flag != TC_VIDEO) return(TC_IMPORT_ERROR);
    
    bytes_read = (pass_through) ? AVI_read_frame(avifile, param->buffer, &key):AVI_read_frame(avifile, buffer, &key);
    
    if(bytes_read<0) return(TC_IMPORT_ERROR); 

    if(pass_through) {
      
      int cc=0;
      
      param->size = (int) bytes_read;
      
      //determine keyframe
      
#if DECORE_VERSION >= 20020303
      cc=divx4_is_key((unsigned char *)param->buffer, (long) param->size);
#else
      switch(divx_version) {
      case DEC_OPT_FRAME:
	cc=divx4_is_key((unsigned char *)param->buffer, (long) param->size);
	break;
      case DEC_OPT_FRAME_311:
	if(param->size>4) cc=divx3_is_key((unsigned char *)param->buffer);
	break;
      }
#endif
      if(cc) param->attributes |= TC_FRAME_IS_KEYFRAME;
      if(verbose & TC_DEBUG) printf("keyframe info (AVI|bitstream)=(%d|%d)\n", key, cc);

    } else {
      
      //need to decode the frame
      decFrame->bitstream = buffer;
      decFrame->stride = divx->x_dim;
      decFrame->bmp = param->buffer;
      decFrame->length = (int) bytes_read;
      decFrame->render_flag = 1;
      
      if(divx_decore(divx_id, divx_version, decFrame, NULL) < 0) {
	printf("codec DEC_OPT_FRAME error");
	return(TC_IMPORT_ERROR); 
      }
      
      param->size = frame_size;
    }
    
    //for preview/pass-through feature
    
    if(pass_through_decode) {

      decFrame->bitstream = param->buffer; //read only??
      decFrame->stride = divx->x_dim;
      decFrame->bmp = param->buffer2; 
      decFrame->length = (int) bytes_read;
      decFrame->render_flag = 1;
      
      if(divx_decore(divx_id, divx_version, decFrame, NULL) < 0) {
	printf("codec DEC_OPT_FRAME error");
	return(TC_IMPORT_ERROR); 
      }
    }
    
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
	
	int status;

	//fprintf (stderr, "DEBUG [%s:%d] 1\n", __FILE__, __LINE__);

	if (decore_in_use>0) {
	    --decore_in_use;
	    status = divx_decore(divx_id, DEC_OPT_RELEASE, NULL, NULL);
	    if(verbose_flag & TC_DEBUG) 
		fprintf(stderr, "DivX decore module returned %d\n", status); 

	    //remove codec
	    dlclose(handle);
	}

	// free memory
	if (divx)     { free (divx);         divx=NULL;     }
	if (decFrame) { free (decFrame);     decFrame=NULL; }
	if (decInfo)  { free (decInfo);      decInfo=NULL;  }
	if (avifile)  { AVI_close (avifile); avifile=NULL;  }
 
	return(0);
    }

    
    return(TC_IMPORT_ERROR);
}

