/*
 *  export_iv50.cpp
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
#include <sys/stat.h>
#include <unistd.h>

#include <videoencoder.h>
#include <audioencoder.h>

#ifdef __cplusplus
extern "C" {
#endif
  
#include "avilib.h"
#include "ac3.h"
#include "transcode.h"
#include "aud_aux.h"
  
  static int verbose_flag=TC_QUIET;
  static int capability_flag=TC_CAP_PCM|TC_CAP_AC3|TC_CAP_RGB|TC_CAP_AUD;
  
  static avi_t *avifile=NULL;
  
  //tmp buffer
  unsigned char *framebuffer = new unsigned char[SIZE_RGB_FRAME];
  char *buffer = new char[SIZE_RGB_FRAME];
  
  static  BITMAPINFOHEADER  bh;
  static  WAVEFORMATEX     fmt;
  
  static  IVideoEncoder    *ve=NULL;
  static  IAudioEncoder    *ae=NULL;

#define MOD_NAME    "export_iv50.so"
#define MOD_VERSION "v0.2.2 (2001-10-10)"
#define MOD_CODEC   "(video) Indeo Video 5.0 | (audio) MPEG/AC3/PCM"

#define MOD_PRE iv50
#include "export_def.h"
  
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

      // set video parameter for AVI file
      AVI_set_video(vob->avifile_out, vob->ex_v_width, vob->ex_v_height, 
		    vob->fps, "IV50");      
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
    
    int ret, tt1, tt2, tt3;
    
    if(param->flag == TC_VIDEO) {
      
      memset(&bh,0,sizeof(BITMAPINFOHEADER));
      
      bh.biSize=sizeof(BITMAPINFOHEADER);
      bh.biWidth=vob->ex_v_width;
      bh.biHeight=vob->ex_v_height;
      bh.biPlanes=1;
      bh.biBitCount=vob->v_bpp;
      bh.biCompression=0;
      bh.biSizeImage = vob->ex_v_size;

      // proper IVideoEncoder::ExtendedAttr setting only seems to work before
      // creation of encoder object in IVideoEncoder::Create since all 
      // some parameters are read from the "registry" file
      
      if((ret = IVideoEncoder::SetExtendedAttr(fccIV50, "QuickCompress", 1))<0) printf("[%s] failed to set 'QuickCompress' for encoder\n", MOD_NAME);
      
      // create encoder object
      ve=IVideoEncoder::Create(fccIV50, bh);

      CImage im((BitmapInfo*)&bh, framebuffer, false);

      // start encocder
      ve->Start();
      
      // check:
      IVideoEncoder::GetExtendedAttr(fccDIV3, "BitRate",  tt1);
      IVideoEncoder::GetExtendedAttr(fccDIV3, "KeyFrames", tt2);
      IVideoEncoder::GetExtendedAttr(fccDIV3, "Crispness", tt3);

      if(verbose & TC_DEBUG) printf("[%s] BitRate %d kBits/s|KeyFrames %d|Crispness %d|Quality %d\n", MOD_NAME, tt1, tt2, tt3, ve->GetQuality());
      
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
    
    int i, is_key_frame, length, size_read, lpckid=0;

    if(param->flag == TC_VIDEO) {
      
      // encode video
      
      CImage imtarget((BitmapInfo*)&bh, (unsigned char *)param->buffer, false);
      
      ve->EncodeFrame(&imtarget, (char *) buffer, &is_key_frame, &length, &lpckid);
      
      if(AVI_write_frame(avifile, (char *) buffer, length, is_key_frame)<0) {
	AVI_print_error("avi video write error");
	return(TC_EXPORT_ERROR);
      }
      
      return(0);
    }
    
    if(param->flag == TC_AUDIO) return(audio_encode(param->buffer, param->size, avifile));
    
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
      
      ve->Stop();
      ve->Close();
      
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
