/*
 *  export_raw.c
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

#include <stdio.h>
#include <stdlib.h>

#include "transcode.h"
#include "avilib.h"
#include "aud_aux.h"
#include "../import/magic.h"
#include "../src/iodir.h"

#define MOD_NAME    "export_raw.so"
#define MOD_VERSION "v0.3.12 (2003-08-04)"
#define MOD_CODEC   "(video) * | (audio) MPEG/AC3/PCM"

#define MOD_PRE raw
#include "export_def.h"

static avi_t *avifile1=NULL;
static avi_t *avifile2=NULL;

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_DV|TC_CAP_PCM|TC_CAP_RGB|TC_CAP_YUV|TC_CAP_AC3|TC_CAP_AUD|TC_CAP_VID;

static int info_shown=0, force_kf=0;
static int width=0, height=0, im_v_codec=-1;
static int mpeg_passthru;
static FILE *mpeg_f = NULL;

static int scan(char *name) 
{
  struct stat fbuf;
  
  if(stat(name, &fbuf)) {
    fprintf(stderr, "(%s) invalid file \"%s\"\n", __FILE__, name);
    exit(1);
  }
  
  // file or directory?
  
  if(S_ISDIR(fbuf.st_mode)) return(1);
  return(0);
}


/* ------------------------------------------------------------ 
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{
    
    if(param->flag == TC_VIDEO) {
      if(verbose & TC_DEBUG) printf("[%s] max AVI-file size limit = %lu bytes\n", MOD_NAME, (unsigned long) AVI_max_size());
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
  
    
    double fps;
    
    char *codec;
    char *dir_name = NULL;
    char *to_open;

    im_v_codec = vob->im_v_codec;

    // open out file
    if(param->flag==TC_AUDIO && vob->out_flag) goto further; 
    if(param->flag==TC_VIDEO && vob->codec_flag == TC_CODEC_MPEG2 && (vob->pass_flag & TC_VIDEO)) goto further;
    if(vob->avifile_out==NULL) {
      if(NULL == (vob->avifile_out = AVI_open_output_file( 
	      (param->flag==TC_VIDEO)?  vob->video_out_file: vob->audio_out_file))) {
	AVI_print_error("avi open error");
	exit(TC_EXPORT_ERROR);
      }
    }

further:
    
    /* save locally */
    avifile2 = vob->avifile_out;
    
    if(param->flag == TC_VIDEO) {
      
      // video
      
      switch(vob->im_v_codec) {
	
      case CODEC_RGB:
	
	//force keyframe
	force_kf=1;
	
	width = vob->ex_v_width;
	height = vob->ex_v_height;
	
	AVI_set_video(vob->avifile_out, vob->ex_v_width, vob->ex_v_height, vob->ex_fps, "RGB");

	if (vob->avi_comment_fd>0)
	    AVI_set_comment_fd(vob->avifile_out, vob->avi_comment_fd);

	if(!info_shown && verbose_flag) 
	  fprintf(stderr, "[%s] codec=%s, fps=%6.3f, width=%d, height=%d\n", 
		  MOD_NAME, "RGB", vob->ex_fps, vob->ex_v_width, vob->ex_v_height);
	  break;
	    
      case CODEC_YUV:
	
	//force keyframe
	force_kf=1;

	width = vob->ex_v_width;
	height = vob->ex_v_height;
	
	AVI_set_video(vob->avifile_out, vob->ex_v_width, vob->ex_v_height, vob->ex_fps, "YV12");
	
	if(!info_shown && verbose_flag) 
	  fprintf(stderr, "[%s] codec=%s, fps=%6.3f, width=%d, height=%d\n", 
		MOD_NAME, "YV12", vob->ex_fps, vob->ex_v_width, vob->ex_v_height);
	break;

	    
      case CODEC_RAW:
      case CODEC_RAW_YUV:

	if (vob->codec_flag == TC_CODEC_MPEG2) {

	    if (vob->pass_flag & TC_VIDEO) {
		mpeg_passthru = 1;
		fprintf(stderr, "[%s] icodec (0x%08x) and codec_flag (0x%08lx) - passthru\n", MOD_NAME,
		    vob->im_v_codec, vob->codec_flag);

		mpeg_f = fopen(vob->video_out_file, "w");
		if (!mpeg_f) {
		    tc_warn("[%s] Cannot open outfile \"%s\": %s", MOD_NAME, vob->video_out_file,
			strerror(errno));
		    return (TC_EXPORT_ERROR);
		}
	    }
	}
	else
	switch(vob->format_flag) {
	  
	case TC_MAGIC_DV_PAL:
	case TC_MAGIC_DV_NTSC:

	  //force keyframe
	  force_kf=1;
	  
	  width = vob->ex_v_width;
	  height = vob->ex_v_height;
	
	  AVI_set_video(vob->avifile_out, vob->ex_v_width, vob->ex_v_height, vob->ex_fps, "DVSD");
	  
	  if(!info_shown && verbose_flag) 
	    fprintf(stderr, "[%s] codec=%s, fps=%6.3f, width=%d, height=%d\n", 
		    MOD_NAME, "DVSD", vob->ex_fps, vob->ex_v_width, vob->ex_v_height);
	  break;
	  
	default:
	  
	  // pass-through mode is the default, works only with import_avi.so
	  
	  if(vob->pass_flag & TC_VIDEO) {

	    to_open = vob->video_in_file;

	    if (scan(vob->video_in_file)) { 

	      dir_name = vob->video_in_file;
	      if((tc_open_directory(dir_name))<0) { 
		tc_error("unable to open directory \"%s\"", dir_name);
	      }
	      to_open = tc_scan_directory(dir_name);

	      tc_close_directory();
	    }

	    if(avifile1==NULL) 
	      if(NULL == (avifile1 = AVI_open_input_file(to_open,1))) {
		AVI_print_error("avi open error in export_raw");
		return(TC_EXPORT_ERROR); 
	      }
	    
	    //read all video parameter from input file
	    width  =  AVI_video_width(avifile1);
	    height =  AVI_video_height(avifile1);
	    
	    fps    =  AVI_frame_rate(avifile1);
	    codec  =  AVI_video_compressor(avifile1);
	    
	    //same for outputfile
	    AVI_set_video(vob->avifile_out, width, height, fps, codec); 
	    
	    if(!info_shown && (verbose_flag)) 
	      fprintf(stderr, "[%s] codec=%s, fps=%6.3f, width=%d, height=%d\n", MOD_NAME, codec, fps, width, height);
	    
	    //free resources
	    if(avifile1!=NULL) {
	      AVI_close(avifile1);
	      avifile1=NULL;
	    }
	  }
	}
	
	break;
	
      default:
	
	fprintf(stderr, "[%s] codec (0x%08x) and format (0x%08lx)not supported\n", MOD_NAME,
		vob->im_v_codec, vob->format_flag);
	return(TC_EXPORT_ERROR); 
	
	break;
      }

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
  int i, mod=width%4;
  
  if(param->flag == TC_VIDEO) { 

    if (mpeg_f) {
      if (fwrite (param->buffer, 1, param->size, mpeg_f) != param->size) {
	tc_warn("[%s] Cannot write data: %s", MOD_NAME, strerror(errno));
	return(TC_EXPORT_ERROR); 
      }
      return (TC_EXPORT_OK);
    }
      
    
    //0.5.0-pre8:
    key = ((param->attributes & TC_FRAME_IS_KEYFRAME) || force_kf) ? 1:0;

    //0.6.2: switch outfile on "r/R" and -J pv
    //0.6.2: enforce auto-split at 2G (or user value) for normal AVI files
    if((uint32_t)(AVI_bytes_written(avifile2)+param->size+16+8)>>20 >= tc_avi_limit) tc_outstream_rotate_request();
    
    if(key) tc_outstream_rotate();

    // Fixup: For uncompressed AVIs, it must be aligned at
    // a 4-byte boundary
    if (mod && (im_v_codec == CODEC_RGB)) {
	for (i = height; i>0; i--) {
	    memmove (param->buffer+(i*width*3) + mod*i,
		     param->buffer+(i*width*3) , 
		     width*3);
	}
	param->size = height*width*3 + (4-mod)*height;
	//fprintf(stderr, "going here mod = |%d| width (%d) size (%d)||\n", mod, width, param->size);
    }
    // write video
    if(AVI_write_frame(avifile2, param->buffer, param->size, key)<0) {
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
  
  if(param->flag == TC_VIDEO) return(0);
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

  if (mpeg_f) {
    fclose (mpeg_f);
    mpeg_f = NULL;
  }

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

