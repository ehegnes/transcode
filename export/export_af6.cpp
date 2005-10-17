/*
 *  export_af6.cpp
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *  Copyright (C) Gerhard Monzel <gerhard.monzel@sap.com> 
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>


#if HAVE_AVIFILE_INCLUDES == 7
#  include <videoencoder.h>
#  include <audioencoder.h>
#  include <avm_fourcc.h>
#  include <avm_creators.h>
#  include <image.h>
#elif HAVE_AVIFILE_INCLUDES == 0
#  include <avifile/videoencoder.h>
#  include <avifile/audioencoder.h>
#  include <avifile/avm_fourcc.h>
#  include <avifile/creators.h>
#  include <avifile/image.h>
#endif


using namespace Creators;

#ifdef __cplusplus
extern "C" {
#endif
  
#include "avilib.h"
#include "ac3.h"
#include "transcode.h"
#include "aud_aux.h"
  
  static int verbose_flag=TC_QUIET;
  static int capability_flag=TC_CAP_PCM|TC_CAP_AC3|TC_CAP_RGB|TC_CAP_AUD|TC_CAP_YUV;
  
  static avi_t *avifile=NULL;
  
  //tmp buffer
  unsigned char *framebuffer = new unsigned char[SIZE_RGB_FRAME];
  char *buffer = new char[SIZE_RGB_FRAME];

#define CODEC_default "DivX ;-) low-motion"
  
  static  BITMAPINFOHEADER  bh;
  
  static  IVideoEncoder    *ve=NULL;

  static  fourcc_t fourcc = 0xFFFFFFFF;

  static  int force_keyframe = -1;
  
#define MOD_NAME    "export_af6.so"
#define MOD_VERSION "v0.2.3 (2003-06-09)"
#define MOD_CODEC   "(video) Win32 dll | (audio) MPEG/AC3/PCM"
  
#define MOD_PRE af6
#include "export_def.h"

#include "af6_aux.h"

  /* ------------------------------------------------------------ 
   *
   * open outputfile
   *
   * ------------------------------------------------------------*/
  
  MOD_open
  {
    // prepare outputfile 
    
    if(vob->avifile_out==NULL) 
      if(NULL == (vob->avifile_out = AVI_open_output_file(vob->video_out_file))) {
	AVI_print_error("avi open error");
	return(TC_EXPORT_ERROR);
      }

    /* save locally */
    avifile = vob->avifile_out;
    
    if(param->flag == TC_VIDEO) {

	unsigned char id[5];

	long2str((unsigned char*) &id[0], fourcc);
	    

        // set video parameter for AVI file
	AVI_set_video(vob->avifile_out, vob->ex_v_width, vob->ex_v_height, 
		      vob->fps, (char*) id);      

	if (vob->avi_comment_fd>0)
	    AVI_set_comment_fd(vob->avifile_out, vob->avi_comment_fd);

        //do not force key frame at the very beginning of encoding, since
        //first frame will be a key fame anayway. Therefore key.quantizer
        //is well defined for any frame to follow
        force_keyframe=(force_keyframe<0) ? 0:1;

	return(0);
    }
    
    if(param->flag == TC_AUDIO) return(audio_open(vob, vob->avifile_out));
    
    return(TC_EXPORT_ERROR);
  }
  
  /* ------------------------------------------------------------ 
   *
   * init codec
   *
   * ------------------------------------------------------------*/
  
  MOD_init
  {
    const CodecInfo *codec_info;
    char *pfcc;
    
    if(param->flag == TC_VIDEO) {
      
      memset(&bh,0,sizeof(BITMAPINFOHEADER));
      
      bh.biSize=sizeof(BITMAPINFOHEADER);
      bh.biWidth=vob->ex_v_width;
      bh.biHeight=vob->ex_v_height;
      bh.biPlanes=1;
      bh.biBitCount=vob->v_bpp;
      bh.biSizeImage = vob->ex_v_size;

      switch (vob->im_v_codec) {
      case CODEC_RGB:
	  bh.biCompression=0;
	  break;
      case CODEC_YUV:
	  bh.biCompression=fccYV12;
	  break;
      default:
	  tc_log_warn(MOD_NAME, "codec not supported");
	  return(TC_EXPORT_ERROR); 
	  break;
      }
      
      //-- search for codec by name --
      //------------------------------

      if(vob->ex_v_fcc == NULL || strlen(vob->ex_v_fcc)==0) 
	vob->ex_v_fcc=CODEC_default;

      if((codec_info = is_valid_codec(vob->ex_v_fcc, &fourcc)) == NULL) {
        
        tc_log_warn(MOD_NAME, "invalid codec string: \"%s\"", vob->ex_v_fcc);
	list_codecs();
	return(TC_EXPORT_ERROR);
      }
      
      pfcc = (char *)&fourcc;
      tc_log_info(MOD_NAME "\"%s\" FOURCC=0x%lx (%c%c%c%c)", 
             vob->ex_v_fcc, (long)fourcc, 
             pfcc[0], pfcc[1], pfcc[2], pfcc[3]);

      //-- setup codec properties before(!) creating codec object --
      //------------------------------------------------------------
      //ThOe: file parameter overwrite transcode defaults
      setup_codec_byFile(MOD_NAME, codec_info, vob, verbose);
      //ThOe: command line parameter overwrite file/trancode defaults
      setup_codec_byParam(MOD_NAME, codec_info, vob, verbose);
      
      //-- create encoder object --
      //---------------------------
      if( (ve = CreateVideoEncoder(*codec_info, bh)) == NULL ) 
      {
	tc_log_warn(MOD_NAME, "failed to create encoder for FOURCC=0x%lx", 
                (long)fourcc);
	return(TC_EXPORT_ERROR);
      }
      
      CImage im((BitmapInfo*)&bh, framebuffer, false);
      
      ve->Start();
      ve->SetQuality(2000*vob->divxquality);
      
      return(0);
    }
    
    if(param->flag == TC_AUDIO) return(audio_init(vob, verbose));
    
    return(TC_EXPORT_ERROR);
  }
  
  /* ------------------------------------------------------------ 
   *
   * encode and export frame
   *
   * ------------------------------------------------------------*/
  
  MOD_encode
  {
    int is_key_frame = 0; 
    int length, lpckid = 0;
    
    if(param->flag == TC_VIDEO) 
      {
	
	//-- force keyframe ? --
	//----------------------      
	if (force_keyframe)
	  {
	    //-- Restart encoder -> this will not influence     --
	    //-- parameter seetings of DivX :-) or DivX4 Codecs --
	    //----------------------------------------------------
	    force_keyframe = 0;
	    ve->Stop();
	    ve->Start();
	  }
	
	//-- encode Image --
	//------------------      
	CImage imtarget((BitmapInfo*)&bh, (unsigned char *)param->buffer, false);
	ve->EncodeFrame(&imtarget, buffer, &is_key_frame, (uint_t *) &length, &lpckid);
	
	if(AVI_write_frame(avifile, (char *) buffer, length, is_key_frame)<0) {
	  AVI_print_error("avi video write error");
	  return(TC_EXPORT_ERROR);
	}
	
	return(0);
      }
    
    if(param->flag == TC_AUDIO) 
      return(audio_encode((char *)param->buffer, param->size, avifile));
    
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
      
      FreeVideoEncoder(ve);
      return(0);
    }
    
    if(param->flag == TC_AUDIO) return(audio_stop());
    
    return(TC_EXPORT_ERROR);
  }
    
    /* ------------------------------------------------------------ 
     *
     * close outputfile
     *
     * ------------------------------------------------------------*/
    
    MOD_close
    {  

      vob_t *vob = tc_get_vob();
      if(param->flag == TC_AUDIO) return(audio_close());
	
      if(vob->avifile_out!=NULL) {
	
	if(AVI_close(vob->avifile_out)<0) {
	  AVI_print_error("avi close error");
	  // prevent others from trying to close it again
	  vob->avifile_out=NULL;
	  return(TC_EXPORT_ERROR);
	}
	vob->avifile_out=NULL;
      }

      if(param->flag == TC_VIDEO) return(0);

      return(TC_EXPORT_ERROR);

    }
  
  
#ifdef __cplusplus
}
#endif
