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

#define MOD_NAME    "import_divx.so"
#define MOD_VERSION "v0.2.9 (2003-07-30)"
#define MOD_CODEC   "(video) DivX;-)/XviD/OpenDivX/DivX 4.xx/5.xx"

#include "transcode.h"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_RGB | TC_CAP_YUV | TC_CAP_VID;

#define MOD_PRE divx
#include "import_def.h"

#include "avilib/avilib.h"

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#else
# ifdef SYSTEM_DARWIN
#  include "libdldarwin/dlfcn.h"
# endif
#endif

#ifdef HAVE_DIVX_DECORE
#include <decore.h>
#else
#include "divx_decore.h"
#endif


extern int errno;
char import_cmd_buf[TC_BUF_MAX];

static int codec, frame_size=0;

#ifndef DECORE_VERSION
#define DECORE_VERSION 0
#endif

static unsigned long divx_version=DEC_OPT_FRAME;

static int done_seek=0;

static DEC_FRAME    *decFrame = NULL;
static DEC_FRAME_INFO *decInfo = NULL;

#if DECORE_VERSION >= 20021112

  static void* dec_handle = NULL;
  static DEC_INIT *decInit = NULL;
  DivXBitmapInfoHeader* pbi=NULL;
  static int (*divx_decore)(void *para0, unsigned long opt,
                            void *para1, void *para2);

#else

  static DEC_PARAM    *divx = NULL;
  static int (*divx_decore)(unsigned long para0, unsigned long opt,
                            void *para1, void *para2);
  static unsigned long divx_id=0x4711;

#endif

static int decore_in_use = 0; // semaphore to keep track of usage count

// dl stuff
static void *handle=NULL;
static char module[TC_BUF_MAX];


#define MODULE   "libdivxdecore.so"
#define MODULE_V "libdivxdecore.so.0"

static avi_t *avifile=NULL;

//special codec information
static int pass_through=0, pass_through_decode=0;

//temporary video buffer
static char *buffer = NULL;


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
   long buffer_align=getpagesize();
#else
   long buffer_align=0;
#endif

   char *buf = malloc(size + buffer_align);

   int adjust;

   if (buf == NULL) {
       fprintf(stderr, "(%s) out of memory", __FILE__);
   }

   adjust = buffer_align - ((long) buf) % buffer_align;

   if (adjust == buffer_align)
      adjust = 0;

   return (unsigned char *) (buf + adjust);
}

static int divx_init(char *path) {
#ifdef SYS_BSD
    const
#endif    
    char *error;
    int sret;

    handle = NULL;

    // try transcode's module directory
    if (!handle) {
      // (try 5.x "libdivxencore.so.0" style)
      sret = snprintf(module, TC_BUF_MAX, "%s/%s", path, MODULE_V);
      tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno);
      handle = dlopen(module, RTLD_LAZY); 
    }
    if (!handle) {
      // (try 4.x "libdivxencore.so" style)
      sret = snprintf(module, TC_BUF_MAX, "%s/%s", path, MODULE);
      tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno);
      handle = dlopen(module, RTLD_LAZY); 
    }

    //try the default:
    if (!handle) {
      // (try 5.x "libdivxencore.so.0" style)
      sret = snprintf(module, TC_BUF_MAX, "%s", MODULE_V);
      tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno);
      handle = dlopen(module, RTLD_LAZY); 
    }
    if (!handle) {
      // (try 4.x "libdivxencore.so" style)
      sret = snprintf(module, TC_BUF_MAX, "%s", MODULE);
      tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno);
      handle = dlopen(module, RTLD_LAZY); 
    }

    if (!handle) {
      tc_warn("[%s] %s\n", MOD_NAME, dlerror());
      return(-1);
    } else {  
      if(verbose_flag & TC_DEBUG) 
        fprintf(stderr, "[%s] Loading external codec module %s\n",
                        MOD_NAME, module); 
    }

    divx_decore = dlsym(handle, "decore");   

    if ((error = dlerror()) != NULL)  {
      tc_warn("[%s] %s\n", MOD_NAME, error);
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

      /*
    if(avifile==NULL) 
      if(NULL == (avifile = AVI_open_input_file(vob->video_in_file,1))){
	AVI_print_error("avi open error");
	return(TC_IMPORT_ERROR); 
      }
      */

    if(avifile==NULL)  {
      if(vob->nav_seek_file) {
	if(NULL == (avifile = AVI_open_input_indexfile(vob->video_in_file,
                                                       0,vob->nav_seek_file))){
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
	//printf("starting decode from last key was (%d)\n", vob->vob_offset);
	AVI_set_video_position(avifile, vob->vob_offset);
	done_seek=1;
    }


    //load the codec

    if (!decore_in_use) {
      if(divx_init(vob->mod_path)<0) {
	fprintf(stderr, "[%s] failed to init DivX 4.xx/5.xx codec\n", MOD_NAME);
	return(TC_IMPORT_ERROR); 
      }
    }

    //check DivX version:
    codec_str = AVI_video_compressor(avifile);

    if(strlen(codec_str)==0) {
	fprintf(stderr, "[%s] invalid AVI file codec", MOD_NAME);
	return(TC_IMPORT_ERROR); 
    }

#if DECORE_VERSION >= 20020303
#  if DECORE_VERSION >= 20021112
    if ((decInit = malloc(sizeof(DEC_INIT)))==NULL) {
      perror("out of memory");
      return(TC_IMPORT_ERROR); 
    } else
      memset(decInit,0x00,sizeof(DEC_INIT));

      if (verbose & TC_DEBUG) 
      		printf("[%s] using DivX5.0.5 decoder syntax.\n", MOD_NAME);

      if (strcasecmp(codec_str,"DIV3")==0)
	  decInit->codec_version = 311;
      else if (strcasecmp(codec_str,"DIVX")==0)
	  decInit->codec_version = 412;
      else
	  decInit->codec_version = 500;

      // no smoothing of the CPU load
      decInit->smooth_playback = 0;
      divx_version=DEC_OPT_FRAME;
#  else

    if ((divx = malloc(sizeof(DEC_PARAM)))==NULL) {
      perror("out of memory");
      return(TC_IMPORT_ERROR); 
    } else
      memset(divx,0x00,sizeof(DEC_PARAM));

    //important parameter
    divx->x_dim = AVI_video_width(avifile);
    divx->y_dim = AVI_video_height(avifile);
    divx->time_incr = 15; //default


      if (verbose & TC_DEBUG)
      		printf("[%s] using DivX5 decoder syntax.\n", MOD_NAME);

      if (strcasecmp(codec_str,"DIV3")==0)
		divx->codec_version = 311;
	else
		divx->codec_version = 500;

      divx->build_number = 0;
      divx_version=DEC_OPT_FRAME;
#  endif
#else

    if(strcasecmp(codec_str,"DIV3")==0) {
	divx_version=DEC_OPT_FRAME_311;
        if(verbose & TC_DEBUG)
		fprintf(stderr, "[%s] detected DivX divx_version3.11 codec\n",
                                 MOD_NAME);
    }
    else
	divx_version=DEC_OPT_FRAME;

#endif

    codec=vob->im_v_codec;

#if DECORE_VERSION >= 20021112

    // yes this only works on little endian machines.
    // Anyway, we don't support divx4linux decoding on big endian machines.
#define FOURCC(A, B, C, D) ( ((uint8_t) (A)) | (((uint8_t) (B))<<8) | \
                             (((uint8_t) (C))<<16) | (((uint8_t) (D))<<24) )

    if ((pbi = malloc(sizeof(DivXBitmapInfoHeader)))==NULL) {
      perror("out of memory");
      return(TC_IMPORT_ERROR); 
    } else
      memset(pbi,0x00,sizeof(DivXBitmapInfoHeader));

    pbi->biSize=sizeof(DivXBitmapInfoHeader);
    pbi->biWidth = AVI_video_width(avifile);
    pbi->biHeight = AVI_video_height(avifile);

    frame_size = pbi->biWidth * pbi->biHeight * 3;
    switch(codec) {

    case CODEC_RGB:

      pbi->biCompression=0;
      pbi->biBitCount=24;
      frame_size = pbi->biWidth * pbi->biHeight * 3;
      break;

    case CODEC_YUV:

      pbi->biCompression = FOURCC('Y','V','1','2');
      frame_size = (pbi->biWidth * pbi->biHeight * 3)/2;
      break;

    case CODEC_RAW:

      pass_through=1;
      pbi->biCompression = FOURCC('I','4','2','0');
      break;

    case CODEC_RAW_YUV:

      pass_through=1;
      pass_through_decode=1;
      pbi->biCompression = FOURCC('Y','V','1','2');
      frame_size = pbi->biWidth * pbi->biHeight * 3;
      break;
    }


#else
    frame_size = divx->x_dim * divx->y_dim * 3;
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
#endif

    //----------------------------------------
    //
    // setup decoder
    //
    //----------------------------------------
#if DECORE_VERSION >= 20021112
    if(divx_decore(&dec_handle, DEC_OPT_INIT, decInit, NULL) < 0) {
      fprintf(stderr, "[%s] codec DEC_OPT_INIT error", MOD_NAME);
      return(TC_IMPORT_ERROR); 
    } else
	++decore_in_use;

    if(divx_decore(dec_handle, DEC_OPT_SETOUT, pbi, NULL) < 0) {
      fprintf(stderr, "[%s] codec DEC_OPT_SETOUT error", MOD_NAME);
      return(TC_IMPORT_ERROR); 
    }
#else

    if(divx_decore(divx_id, DEC_OPT_INIT, divx, NULL) < 0) {
      fprintf(stderr, "[%s] codec DEC_OPT_INIT error", MOD_NAME);
      return(TC_IMPORT_ERROR); 
    } else
	++decore_in_use;
#endif

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
      if ((buffer = bufalloc(frame_size))==NULL) {
	perror("out of memory");
	return(TC_EXPORT_ERROR); 
      } 
      else {
	memset(buffer, 0, frame_size);
      }
    }

    //postproc
/*   XXX enable this somehow. 
    dec_set.postproc_level = vob->quality * 20;  //0-100;
    if(verbose & TC_DEBUG) printf("[%s] decoder postprocessing quality = %d\n",
                                   MOD_NAME, dec_set.postproc_level);

    divx_decore(divx_id, DEC_OPT_SETPP, &dec_set, NULL);
  */
    param->fd = NULL;

    return(TC_IMPORT_OK);
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
    static char *working_frame = NULL;

    if (!working_frame) {
	working_frame = calloc(frame_size, 1);
	if (!working_frame) {
	  perror("out of memory");
	  return(TC_IMPORT_ERROR);
	}
    }

    if(param->flag != TC_VIDEO) return(TC_IMPORT_ERROR);

    bytes_read = (pass_through) ?
        AVI_read_frame(avifile, param->buffer, &key) :
        AVI_read_frame(avifile, buffer, &key);

    if(bytes_read<0) return(TC_IMPORT_ERROR); 

    if(pass_through) {

      int cc=0;

      param->size = (int) bytes_read;

      //determine keyframe

#if DECORE_VERSION >= 20020303
#  if DECORE_VERSION >= 20021112
      switch (decInit->codec_version){
      case 311:
	  if(param->size>4) cc=divx3_is_key((unsigned char *)param->buffer);
	  break;
      default:
	  cc=divx4_is_key((unsigned char *)param->buffer, (long) param->size);
	  break;
      }
#  else
      switch (divx->codec_version){
      case 311:
	  if(param->size>4) cc=divx3_is_key((unsigned char *)param->buffer);
	  break;
      default:
	  cc=divx4_is_key((unsigned char *)param->buffer, (long) param->size);
	  break;
      }
#  endif
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
      if(verbose & TC_DEBUG)
        printf("keyframe info (AVI|bitstream)=(%d|%d)\n", key, cc);

    } else {

      /* on null frames the previous frame should be reused.
         in, fact, divx seems to rely on the previous frame being
         there every once in a while even on non-null frames.
         go figure. */

      //need to decode the frame
      decFrame->bitstream = buffer;
      decFrame->bmp = working_frame;
      decFrame->length = (int) bytes_read;
      decFrame->render_flag = 1;

#if DECORE_VERSION >= 20021112
      decFrame->stride = pbi->biWidth;
      if(divx_decore(dec_handle, divx_version, decFrame, NULL) != DEC_OK) {
	fprintf(stderr, "[%s]:%d  codec DEC_OPT_FRAME error",
                         MOD_NAME, __LINE__);
	return(TC_IMPORT_ERROR); 
      }
#else
      decFrame->stride = divx->x_dim;

      if(divx_decore(divx_id, divx_version, decFrame, NULL) != DEC_OK) {
	fprintf(stderr, "[%s] codec DEC_OPT_FRAME error", MOD_NAME);
	return(TC_IMPORT_ERROR); 
      }
#endif

      param->size = frame_size;
      tc_memcpy(param->buffer, working_frame, frame_size);
    }

    //for preview/pass-through feature

    if(pass_through_decode) {

      decFrame->bitstream = param->buffer; //read only??
      decFrame->bmp = working_frame; 
      decFrame->length = (int) bytes_read;
      decFrame->render_flag = 1;

#if DECORE_VERSION >= 20021112
      decFrame->stride = pbi->biWidth;

      if(divx_decore(dec_handle, divx_version, decFrame, NULL) != DEC_OK) {
	  fprintf(stderr, "[%s] codec DEC_OPT_FRAME error", MOD_NAME);
	  return(TC_IMPORT_ERROR); 
      }
#else
      decFrame->stride = divx->x_dim;

      if(divx_decore(divx_id, divx_version, decFrame, NULL) != DEC_OK) {
	  fprintf(stderr, "[%s] codec DEC_OPT_FRAME error", MOD_NAME);
	  return(TC_IMPORT_ERROR); 
      }
#endif
      tc_memcpy(param->buffer2, working_frame, frame_size);
    }

    return(TC_IMPORT_OK);
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
#if DECORE_VERSION >= 20021112
	    status = divx_decore(dec_handle, DEC_OPT_RELEASE, NULL, NULL);
	    dec_handle = NULL;
#else
	    status = divx_decore(divx_id, DEC_OPT_RELEASE, NULL, NULL);
#endif
	    if(verbose_flag & TC_DEBUG) 
		fprintf(stderr, "DivX decore module returned %d\n", status); 

	    //remove codec
	    dlclose(handle);
	}

	// free memory
#if DECORE_VERSION >= 20021112
	if (pbi)      { free (pbi);          pbi=NULL;      }
#else
	if (divx)     { free (divx);         divx=NULL;     }
#endif
	if (decFrame) { free (decFrame);     decFrame=NULL; }
	if (decInfo)  { free (decInfo);      decInfo=NULL;  }
	if (avifile)  { AVI_close (avifile); avifile=NULL;  }

	done_seek=0;

	return(TC_IMPORT_OK);
    }

    return(TC_IMPORT_ERROR);
}

