/*
 *  export_ffmpeg4.c
 *
 *  Copyright (C) Thomas Östreich - March 2002
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
#include <assert.h>

#include "transcode.h"
#include "avilib.h"
#include "aud_aux.h"
#include "../ffmpeg/libavcodec/avcodec.h"
#include "vbr.h"

#define MOD_NAME    "export_ffmpeg4.so"
#define MOD_VERSION "v0.0.2 (2002-05-28)"
#define MOD_CODEC   "(video) MPEG4 | (audio) MPEG/AC3/PCM"

#define MOD_PRE ffmpeg4
#include "export_def.h"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_YUV|TC_CAP_RGB|TC_CAP_PCM|TC_CAP_AC3|TC_CAP_AUD;

static uint8_t tmp_buffer[SIZE_RGB_FRAME];

//static int codec, width, height;

static AVCodec        *mpa_codec = NULL;
static AVCodecContext mpa_ctx;
static AVPicture      mpa_picture;

static avi_t *avifile=NULL;

int VbrMode = 0;
int force_key_frame=-1;

/* ------------------------------------------------------------ 
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{
  struct stat fbuf;
  
  if(param->flag == TC_VIDEO) {
    
    avcodec_init();
    register_avcodec(&mpeg4_encoder);
    
    //-- get it --
    mpa_codec = avcodec_find_encoder(CODEC_ID_MPEG4);
    if (!mpa_codec) {
      fprintf(stderr, "[%s] codec not found !\n", MOD_NAME);
      return(TC_EXPORT_ERROR); 
    }

    //-- set parameters 
    //--------------------------------------------------------
    memset(&mpa_ctx, 0, sizeof(mpa_ctx));       // default all

    if(vob->im_v_codec == CODEC_YUV) {
      mpa_ctx.pix_fmt = PIX_FMT_YUV420P;

      mpa_picture.linesize[0]=vob->ex_v_width;     
      mpa_picture.linesize[1]=vob->ex_v_width/2;     
      mpa_picture.linesize[2]=vob->ex_v_width/2;     
    
    }
     
    if(vob->im_v_codec == CODEC_RGB) {
      mpa_ctx.pix_fmt = PIX_FMT_RGB24;

      mpa_picture.linesize[0]=vob->ex_v_width*3;     
      mpa_picture.linesize[1]=0;     
      mpa_picture.linesize[2]=0;
    }

    mpa_ctx.width              = vob->ex_v_width;
    mpa_ctx.height             = vob->ex_v_height;
    mpa_ctx.frame_rate         = vob->fps * FRAME_RATE_BASE;
    mpa_ctx.bit_rate           = vob->divxbitrate*1000;
    mpa_ctx.bit_rate_tolerance = 1024 * 8 * 1000;
    mpa_ctx.qmin               = vob->min_quantizer;
    mpa_ctx.qmax               = vob->max_quantizer;
    mpa_ctx.max_qdiff          = 3;
    mpa_ctx.qcompress          = 0.5;
    mpa_ctx.qblur              = 0.5;
    mpa_ctx.max_b_frames       = 0;
    mpa_ctx.b_quant_factor     = 2.0;
    mpa_ctx.rc_strategy        = 2;
    mpa_ctx.b_frame_strategy   = 0;
    mpa_ctx.gop_size           = vob->divxkeyframes;
    mpa_ctx.flags              = CODEC_FLAG_HQ;
    mpa_ctx.me_method          = 5;
    
    if(verbose_flag & TC_DEBUG) 
      {
       //-- GMO start -- 
        if (vob->divxmultipass == 3) { 
          fprintf(stderr, "[%s]    single-pass session: %d (VBR)\n", MOD_NAME, vob->divxmultipass);
          fprintf(stderr, "[%s]          VBR-quantizer: %d\n", MOD_NAME, vob->divxbitrate);
        } else {
	  fprintf(stderr, "[%s]     multi-pass session: %d\n", MOD_NAME, vob->divxmultipass);
	  fprintf(stderr, "[%s]      bitrate [kBits/s]: %d\n", MOD_NAME, mpa_ctx.bit_rate/1000);
	}
	
        //-- GMO end --

	fprintf(stderr, "[%s]              crispness: %d\n", MOD_NAME, vob->divxcrispness);
	fprintf(stderr, "[%s]  max keyframe interval: %d\n", MOD_NAME, vob->divxkeyframes);
	fprintf(stderr, "[%s]             frame rate: %.2f\n", MOD_NAME, vob->fps);
	fprintf(stderr, "[%s]            color space: %s\n", MOD_NAME, (vob->im_v_codec==CODEC_RGB) ? "RGB24":"YV12");
        fprintf(stderr, "[%s]             quantizers: %d/%d\n", MOD_NAME, mpa_ctx.qmin, mpa_ctx.qmax);
    }

    VbrMode = vob->divxmultipass;
    
    switch(VbrMode) {
	
    case 1:
        mpa_ctx.flags |= CODEC_FLAG_PASS1;
	if (VbrControl_init_2pass_vbr_analysis(vob->divxlogfile, 5) == -1) {
          fprintf(stderr, "(%s) Could not initialize pass-1 vbr analysis\n", __FILE__);
          return(TC_EXPORT_ERROR);
        }
	break;
	
    case 2:
      
      // check for logfile
      
        if(vob->divxlogfile==NULL || stat(vob->divxlogfile, &fbuf)){
	  fprintf(stderr, "(%s) pass-1 logfile \"%s\" not found exit\n", __FILE__, 
		  vob->divxlogfile);
  	  return(TC_EXPORT_ERROR);
        }
        mpa_ctx.flags |= CODEC_FLAG_PASS2;
        // second pass: read back the logfile
        if (VbrControl_init_2pass_vbr_encoding(vob->divxlogfile, 
		    			       mpa_ctx.bit_rate, 
					       vob->fps, 
					       vob->divxcrispness, 
					       5) == -1) {
          fprintf(stderr, "(%s) Could not initialize pass-2 vbr encoding\n", __FILE__);
          return(TC_EXPORT_ERROR);
        }
        mpa_ctx.flags |= CODEC_FLAG_QSCALE | CODEC_FLAG_TYPE; // VBR
        break;
    case 3:
        if (VbrControl_init_2pass_vbr_analysis(vob->divxlogfile, 5) == -1) {
          fprintf(stderr, "(%s) Could not initialize single pass vbr analysis\n", __FILE__);
          return(TC_EXPORT_ERROR);
        }

        mpa_ctx.quality = vob->divxbitrate;
        mpa_ctx.key_frame = -1;
        break;
        //-- GMO end --
 
    default:
      // none
      break;
    }
    

    //-- open codec --
    //----------------
    if (avcodec_open(&mpa_ctx, mpa_codec) < 0) {
      fprintf(stderr, "[%s] could not open FFMPEG/MPEG4 codec\n", MOD_NAME);
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

  // open output file
  
  if(avifile==NULL) { 
    if(NULL == (avifile = AVI_open_output_file(vob->video_out_file))) {
      AVI_print_error("avi open error");
      exit(TC_EXPORT_ERROR);
    }
  }
  
  if(param->flag == TC_VIDEO) {
    
      // video
      AVI_set_video(avifile, vob->ex_v_width, vob->ex_v_height, vob->fps, "DIVX");

    //do not force key frame at the very beginning of encoding, since
    //first frame will be a key fame anayway. Therefore key.quantizer
    //is well defined for any frame to follow
    force_key_frame=(force_key_frame<0) ? 0:1;

    return(0);
  }
  
  
  if(param->flag == TC_AUDIO) return(audio_open(vob, avifile));
  
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
  
  int out_size;
  
  if(param->flag == TC_VIDEO) { 
    
    if(mpa_ctx.pix_fmt == PIX_FMT_YUV420P) {
      mpa_picture.linesize[0]=mpa_ctx.width;     
      mpa_picture.linesize[1]=mpa_ctx.width/2;     
      mpa_picture.linesize[2]=mpa_ctx.width/2;     
    
    }
     
    if(mpa_ctx.pix_fmt == PIX_FMT_RGB24) {
      mpa_picture.linesize[0]=mpa_ctx.width*3;     
      mpa_picture.linesize[1]=0;     
      mpa_picture.linesize[2]=0;
    }

    mpa_picture.data[0]=param->buffer;
    mpa_picture.data[2]=param->buffer + mpa_ctx.width * mpa_ctx.height;
    mpa_picture.data[1]=param->buffer + (mpa_ctx.width * mpa_ctx.height*5)/4;

    switch (VbrMode) {
      case 3:
        /* Don't know what I did here... */
/*        if (force_key_frame)
        {
          mpa_ctx.key_frame = 1;
          force_key_frame = 0;
        }
        else*/
          mpa_ctx.key_frame = -1;

        out_size = avcodec_encode_video(&mpa_ctx, (unsigned char *) tmp_buffer, 
				      SIZE_RGB_FRAME, &mpa_picture);
        if(out_size < 0) 
        {
	  printf("encoder error");
	  return(TC_EXPORT_ERROR); 
        }


        VbrControl_update_2pass_vbr_analysis(mpa_ctx.key_frame, 
					         mpa_ctx.mv_bits, 
					         mpa_ctx.i_tex_bits + mpa_ctx.p_tex_bits, 
					         8*out_size, 
					         mpa_ctx.quality);
          break;
        
      case 2:
	// second pass of 2-pass, just a hack for the moment

	mpa_ctx.flags    |= CODEC_FLAG_QSCALE; // enable VBR
	mpa_ctx.quality   = VbrControl_get_quant();
	mpa_ctx.key_frame = VbrControl_get_intra();
	mpa_ctx.gop_size  = 0x3fffffff;

/*	if(force_key_frame) {
	    mpa_ctx.key_frame = 1;    //key frame
	    force_key_frame = 0; //reset
	}*/

        out_size = avcodec_encode_video(&mpa_ctx, (unsigned char *)tmp_buffer,
                                        SIZE_RGB_FRAME, &mpa_picture);
        if(out_size < 0) {
	  printf("encoder error");
	  return(TC_EXPORT_ERROR); 
        }

        VbrControl_update_2pass_vbr_encoding(mpa_ctx.mv_bits, 
					     mpa_ctx.i_tex_bits + mpa_ctx.p_tex_bits, 
					     out_size*8);
        break;
      default:

/*        if(force_key_frame) {

	  mpa_ctx.key_frame=1; //key frame
//  	  mpa_ctx.quality=key.quantizer; //well defined for frames != first frame.

          out_size = avcodec_encode_video(&mpa_ctx, (unsigned char *)tmp_buffer,
                                          SIZE_RGB_FRAME, &mpa_picture);
	  if(out_size < 0) {
	    printf("encoder error");
	    return(TC_EXPORT_ERROR); 
	  }

	  //reset
	  force_key_frame=0; 

        } else {*/

          out_size = avcodec_encode_video(&mpa_ctx, (unsigned char *)tmp_buffer,
                                           SIZE_RGB_FRAME, &mpa_picture);
          if(out_size < 0) {
              printf("encoder error");
	      return(TC_EXPORT_ERROR); 
          }
	  VbrControl_update_2pass_vbr_analysis(mpa_ctx.key_frame, 
					       mpa_ctx.mv_bits, 
					       mpa_ctx.i_tex_bits + mpa_ctx.p_tex_bits, 
					       out_size*8, 
					       mpa_ctx.quality);
//        }
        break;
    }
  
    if(AVI_write_frame(avifile, tmp_buffer, out_size, mpa_ctx.key_frame? 1:0)<0) {
      AVI_print_error("avi video write error");
      
      return(TC_EXPORT_ERROR); 
    }
    
    return(0);
  }
  
  if(param->flag == TC_AUDIO) return(audio_encode(param->buffer, param->size, avifile));
  
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

    //-- release encoder --
    if (mpa_codec) avcodec_close(&mpa_ctx);

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

  //inputfile
  if(avifile!=NULL) {
    AVI_close(avifile);
    avifile=NULL;
  }
  
  if(param->flag == TC_AUDIO) return(audio_close());
  if(param->flag == TC_VIDEO) return(0);
  
  return(TC_EXPORT_ERROR);  
  
}

