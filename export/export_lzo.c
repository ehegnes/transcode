/*
 *  export_lzo.c
 *
 *  Copyright (C) Thomas Östreich - October 2002
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

#include "transcode.h"
#include "avilib.h"
#include "aud_aux.h"
#include "../import/magic.h"

#include <lzo1x.h>
#if (LZO_VERSION > 0x1070)
#  include <lzoutil.h>
#endif

#define MOD_NAME    "export_lzo.so"
#define MOD_VERSION "v0.0.3 (2002-11-14)"
#define MOD_CODEC   "(video) LZO real-time compression | (audio) MPEG/AC3/PCM"

#define MOD_PRE lzo
#include "export_def.h"

#define BUFFER_SIZE SIZE_RGB_FRAME<<1

static avi_t *avifile1=NULL;
static avi_t *avifile2=NULL;

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_DV|TC_CAP_PCM|TC_CAP_RGB|TC_CAP_YUV|TC_CAP_AC3|TC_CAP_AUD|TC_CAP_VID;

static int info_shown=0, force_kf=0;

int r;
lzo_byte *out;
lzo_byte *wrkmem;
lzo_uint out_len;


/* ------------------------------------------------------------ 
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{
    
    if(param->flag == TC_VIDEO) {
      if(verbose & TC_DEBUG) printf("[%s] max AVI-file size limit = %lu bytes\n", MOD_NAME, (unsigned long) AVI_max_size());

      /*
       * Step 1: initialize the LZO library
       */

      if (lzo_init() != LZO_E_OK) {
	printf("[%s] lzo_init() failed\n", MOD_NAME);
	return(TC_EXPORT_ERROR); 
      }

      wrkmem = (lzo_bytep) lzo_malloc(LZO1X_1_MEM_COMPRESS);
      out = (lzo_bytep) lzo_malloc(BUFFER_SIZE);

      if (wrkmem == NULL || out == NULL) {
	printf("[%s] out of memory\n", MOD_NAME);
	return(TC_EXPORT_ERROR); 
      }
      
      return(0);
    }
    
    if(param->flag == TC_AUDIO) return(audio_init(vob, verbose_flag));

    // invalid flag
    return(TC_EXPORT_ERROR); 
}

/* ------------------------------------------------------------ 
 *
 * open outputfile
 *
 * ------------------------------------------------------------*/

MOD_open
{
  
    // open out file
    if(vob->avifile_out==NULL) 
      if(NULL == (vob->avifile_out = AVI_open_output_file(vob->video_out_file))) {
	AVI_print_error("avi open error");
	exit(TC_EXPORT_ERROR);
      }
    
    /* save locally */
    avifile2 = vob->avifile_out;
    
    if(param->flag == TC_VIDEO) {
      
      //video
      
      //force keyframe
      force_kf=1;
      
      AVI_set_video(vob->avifile_out, vob->ex_v_width, vob->ex_v_height, vob->fps, "LZO1");
      
      if(!info_shown && verbose_flag) 
	fprintf(stderr, "[%s] codec=%s, fps=%6.3f, width=%d, height=%d\n", 
		MOD_NAME, "LZO1", vob->fps, vob->ex_v_width, vob->ex_v_height);
      
      info_shown=1;
      
      return(0);
    }

    if(param->flag == TC_AUDIO) return(audio_open(vob, vob->avifile_out));
    
    // invalid flag
    return(TC_EXPORT_ERROR); 
}   

/* ------------------------------------------------------------ 
 *
 * encode and export
 *
 * ------------------------------------------------------------*/

MOD_encode
{

  int key;
  
  if(param->flag == TC_VIDEO) { 
    
    //write video

    //encode

    /*
     * compress from `in' to `out' with LZO1X-1
     */

    r = lzo1x_1_compress(param->buffer, param->size, out, &out_len, wrkmem);
    
    if (r == LZO_E_OK) {
      if(verbose & TC_DEBUG) printf("compressed %lu bytes into %lu bytes\n",
				    (long) param->size, (long) out_len);
    } else {
      
      /* this should NEVER happen */
      printf("[%s] internal error - compression failed: %d\n", MOD_NAME, r);
      return(TC_EXPORT_ERROR); 
    }
    
    /* check for an incompressible block */
    if (out_len >= param->size) 
      if(verbose & TC_DEBUG) printf("[%s] block contains incompressible data\n", MOD_NAME);
    
    //0.5.0-pre8:
    key = ((param->attributes & TC_FRAME_IS_KEYFRAME) || force_kf) ? 1:0;

    //0.6.2: switch outfile on "C" and -J pv
    //0.6.2: enforce auto-split at 2G (or user value) for normal AVI files
    if((uint32_t)(AVI_bytes_written(avifile2)+out_len+16+8)>>20 >= tc_avi_limit) tc_outstream_rotate_request();
    
    if(key) tc_outstream_rotate();

    if(AVI_write_frame(avifile2, out, out_len, key)<0) {
      AVI_print_error("avi video write error");
      
      return(TC_EXPORT_ERROR); 
    }
    
    return(0);
    
  }
  
  if(param->flag == TC_AUDIO) return(audio_encode(param->buffer, param->size, avifile2));
  
  // invalid flag
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * stop encoder
 *
 * ------------------------------------------------------------*/

MOD_stop 
{
  
  if(param->flag == TC_VIDEO) {

    lzo_free(wrkmem);
    lzo_free(out);
    
    return(0);
  }
  
  if(param->flag == TC_AUDIO) return(audio_stop());
  
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * close outputfiles
 *
 * ------------------------------------------------------------*/

MOD_close
{  

  vob_t *vob = tc_get_vob();

  //inputfile
  if(avifile1!=NULL) {
    AVI_close(avifile1);
    avifile1=NULL;
  }

  if(param->flag == TC_AUDIO) return(audio_close());
  
  //outputfile
  if(vob->avifile_out!=NULL) {
    AVI_close(vob->avifile_out);
    vob->avifile_out=NULL;
  }

  if(param->flag == TC_VIDEO) return(0);
  
  return(TC_EXPORT_ERROR);  

}

