/*
 *  export_divx5.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  2-pass code OpenDivX port: "-R 1", "-R 2"
 *  Copyright (C) 2001 Christoph Lampert <gruel@web.de>
 *
 *  constant quantizer extensions "-R 3" by Gerhard Monzel 
 *  <gerhard.monzel@sap.com>
 *
 *  This module is derived from export_divx4.c, minor modification by
 *  Christoph Lampert <gruel@web.de>
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
#include <fcntl.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#else
# ifdef SYSTEM_DARWIN
#  include "../libdldarwin/dlfcn.h"
# endif
#endif



#ifdef HAVE_DIVX_ENCORE2
#include <encore2.h>
#else
#include "divx5_encore2.h"
#endif
#include "transcode.h"
#include "avilib.h"
#include "aud_aux.h"
#include "vbr.h"

#define MOD_NAME    "export_divx5.so"
#define MOD_VERSION "v0.1.8 (2003-07-24)"
#define MOD_CODEC   "(video) DivX 5.xx | (audio) MPEG/AC3/PCM"

#define MOD_PRE divx5
#include "export_def.h"

int VbrMode=0;
int force_key_frame=-1;

static avi_t *avifile=NULL;
  
//temporary audio/video buffer
static char *buffer;
#define BUFFER_SIZE SIZE_RGB_FRAME<<1

#if ENCORE_VERSION >= 20021024
DivXBitmapInfoHeader *format =NULL;
void* encore_handle = NULL;
SETTINGS *settings = NULL;
char *logfile_mv=NULL;
#else
ENC_PARAM   *divx;
#endif
ENC_FRAME  encode;
ENC_RESULT    key;

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_YUV|TC_CAP_AC3|TC_CAP_AUD;

// dl stuff
static int (*divx5_encore)(void *para0, int opt, void *para1, void *para2);
static void *handle;
static char module[TC_BUF_MAX];

#if ENCORE_VERSION >= 20021024
static char * prof2name(int n)
{
    switch (n) {
	 case 0: return "Free/No profile";
	 case 1: return "Handheld";
	 case 2: return "Portable";
	 case 3: return "Home Theatre";
	 case 4: return "High Definition";
	default: return "Free/No profile";
    }
}
#endif


#define MODULE "libdivxencore.so.0"

static int divx5_init(char *path) {
#if defined(__FreeBSD__) || defined(__APPLE__) /* Just in case ProjectMayo will release FreeBSD library :-) */  
  const
#endif  
  char *error;
  int *quiet_encore;
  
  sprintf(module, "%s/%s", path, MODULE);
  

  // try transcode's module directory
  
  handle = dlopen(module, RTLD_NOW); 
  
  if (!handle) {
    
    //try the default:
    
    handle = dlopen(MODULE, RTLD_GLOBAL| RTLD_LAZY);
    
    if (!handle) {
      fprintf(stderr, "[%s] %s\n", MOD_NAME, dlerror());
      return(-1);
    } else {  
      if(verbose_flag & TC_DEBUG) 
        fprintf(stderr, "[%s] Loading external codec module %s\n", MOD_NAME, MODULE);
    }
    
  } else {  
    if(verbose_flag & TC_DEBUG) 
      fprintf(stderr, "[%s] Loading external codec module %s\n", MOD_NAME, module);
  }
  
  divx5_encore = dlsym(handle, "encore");   
  
  if ((error = dlerror()) != NULL)  {
    fprintf(stderr, "[%s] %s\n", MOD_NAME, error);
    return(-1);
  }
  
  quiet_encore=dlsym(handle, "quiet_encore"); 
  
  if ((error = dlerror()) != NULL)  {
    fprintf(stderr, "[%s] %s\n", MOD_NAME, error);
    return(-1);
  }
  
  *quiet_encore=1;
  
  // debug
  if(verbose_flag & TC_STATS) *quiet_encore=0; 
  
  return(0);
}


/* ------------------------------------------------------------ 
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{

#if ENCORE_VERSION >= 20021024
#else
  struct stat fbuf;
#endif
  int ch;
  
  if(param->flag == TC_VIDEO) {

    //check for odd frame parameter:

    if((ch = vob->ex_v_width - ((vob->ex_v_width>>3)<<3)) != 0) {
      printf("[%s] frame width %d (no multiple of 8)\n", MOD_NAME, vob->ex_v_width);
      printf("[%s] encoder may not work correctly or crash\n", MOD_NAME);
      
      if(ch & 1) {
	printf("[%s] invalid frame width\n", MOD_NAME); 
	return(TC_EXPORT_ERROR); 
      }
    }
   
    if((ch = vob->ex_v_height - ((vob->ex_v_height>>3)<<3)) != 0) {
      printf("[%s] frame height %d (no multiple of 8)\n", MOD_NAME, vob->ex_v_height);
      printf("[%s] encoder may not work correctly or crash\n", MOD_NAME);
     
      if(ch & 1) {
	printf("[%s] invalid frame height\n", MOD_NAME); 
	return(TC_EXPORT_ERROR); 
      }
    }
    
    if ((buffer = malloc(BUFFER_SIZE))==NULL) {
      perror("out of memory");
      return(TC_EXPORT_ERROR); 
    } else
      
    memset(buffer, 0, BUFFER_SIZE);  	

    //load the codec

    if(divx5_init(vob->mod_path)<0) {
      tc_warn("failed to init DivX 5.0 Codec");
      return(TC_EXPORT_ERROR); 
    }

    if (divx5_encore(0, ENC_OPT_VERSION, 0, 0) != ENCORE_VERSION) {
	tc_warn("API in encore.h is not compatible with installed lbdivxencore library");
	return (TC_EXPORT_ERROR);
    }

    VbrMode = vob->divxmultipass;
    // 0 for nothing,  
    // 1 for DivX 5.0 - first-pass, 
    // 2 for DivX 5.0 - second pass
    // 3 constant quantizer
    
#if ENCORE_VERSION >= 20021024
#define FOURCC(A, B, C, D) ( ((uint8_t) (A)) | (((uint8_t) (B))<<8) | (((uint8_t) (C))<<16) | (((uint8_t) (D))<<24) )

    if ((settings = malloc(sizeof(SETTINGS)))==NULL) {
      perror("out of memory");
      return(TC_EXPORT_ERROR); 
    }
    if ((format = malloc(sizeof(DivXBitmapInfoHeader)))==NULL) {
      perror("out of memory");
      return(TC_EXPORT_ERROR); 
    }
    memset (settings, 0, sizeof(SETTINGS));
    memset (format, 0, sizeof(DivXBitmapInfoHeader));

    format->biSize = sizeof(DivXBitmapInfoHeader);
    format->biWidth = vob->ex_v_width;
    format->biHeight = vob->ex_v_height;
    format->biCompression = (vob->im_v_codec==CODEC_RGB)?0:FOURCC('Y','V','1','2');
    format->biBitCount = (vob->im_v_codec==CODEC_RGB)?24:0;

    switch (vob->ex_frc) {
	case 1: // 23.976
	    settings->input_clock        = 24000;
	    settings->input_frame_period = 1001;
	    break;
	case 2: // 24.000
	    settings->input_clock        = 24000;
	    settings->input_frame_period = 1000;
	    break;
	case 3: // 25.000
	    settings->input_clock        = 25000;
	    settings->input_frame_period = 1000;
	    break;
	case 4: // 29.970
	    settings->input_clock        = 30000;
	    settings->input_frame_period = 1001;
	    break;
	case 5: // 30.000
	    settings->input_clock        = 30000;
	    settings->input_frame_period = 1000;
	    break;
	case 0: // notset
	default:
	    settings->input_clock        = (int)vob->ex_fps*1000;
	    settings->input_frame_period = 1000;
	    break;
    }

    if (vob->divxlogfile && *vob->divxlogfile) {
	logfile_mv = malloc (strlen(vob->divxlogfile)+4);
	sprintf(logfile_mv, "%s_mv", vob->divxlogfile);
    }

    // default -- expose this to user?
    settings->complexity_modulation = 0.5;

    settings->bitrate = vob->divxbitrate*1000;
    settings->max_key_interval = vob->divxkeyframes;

    settings->quality = vob->divxquality;

    if (VbrMode == 1 || VbrMode == 2){
	 /* 
	  * http://www.divx.com/support/divx/guide_mac.php
	  *
	  * Handheld 128000,262144,196608
	  * Portable 768000,1048576,786432
	  * Home Theatre Theatre 4000000,3145728,2359296
	  * High Definition 8000000,6291456,4718592
	  */

	 switch (vob->divx5_vbv_prof) {
	     case 1: // Handheld
		 settings->vbv_bitrate   =  128000;
		 settings->vbv_size      =  262144;
		 settings->vbv_occupancy =  196608;
		 break;
	     case 2: // Portable
		 settings->vbv_bitrate   =  768000;
		 settings->vbv_size      = 1048576;
		 settings->vbv_occupancy =  786432;
		 break;
	     case 3: // Home Theatre
		 settings->vbv_bitrate   = 4000000;
		 settings->vbv_size      = 3145728;
		 settings->vbv_occupancy = 2359296;
		 break;
	     case 4: // High Definition
		 settings->vbv_bitrate   = 8000000;
		 settings->vbv_size      = 6291456;
		 settings->vbv_occupancy = 4718592;
		 break;
	     case 0: // Free/user supplied
	     default:
		 settings->vbv_bitrate   = vob->divx5_vbv_bitrate*400;
		 settings->vbv_size      = vob->divx5_vbv_size*16384;
		 settings->vbv_occupancy = vob->divx5_vbv_occupancy*64;
		 break;
	 }
	 if (verbose & TC_DEBUG)
	     printf("[%s] Using VBV Profile [%d] (%s)\n", MOD_NAME, vob->divx5_vbv_prof, 
		     prof2name(vob->divx5_vbv_prof));

    }

    switch(VbrMode) {
     case 0:
	 break;
	 settings->vbr_mode = RCMODE_VBV_1PASS;
     case 1:
	 settings->vbr_mode = RCMODE_VBV_MULTIPASS_1ST;
	 settings->mv_file = logfile_mv;
	 settings->log_file_read = NULL;
	 settings->log_file_write = vob->divxlogfile;

	 break;
     case 2:
	 settings->vbr_mode = RCMODE_VBV_MULTIPASS_NTH;
	 settings->mv_file = logfile_mv;
	 settings->log_file_read = vob->divxlogfile;
	 // segfaults if !NULL;
	 settings->log_file_write = NULL;

	 break;

     case 3:
	 settings->vbr_mode = RCMODE_1PASS_CONSTANT_Q;
	 settings->quantizer = vob->divxbitrate;
	 break;
    }

    // bframes ..  lets see how to handle it
    // the codec is crippled anyway
    settings->use_bidirect = 0;

    // don't need this.
    settings->enable_crop = 0;
    settings->enable_resize = 0;

    if(divx5_encore(&encore_handle, ENC_OPT_INIT, format, settings) < 0) {
      tc_warn("Error doing ENC_OPT_INIT");
      return(TC_EXPORT_ERROR); 
    }

#else

    if ((divx = malloc(sizeof(ENC_PARAM)))==NULL) {
	perror("out of memory");
      return(TC_EXPORT_ERROR); 
    }

    memset(divx, 0, sizeof(ENC_PARAM));

    //important parameter (Note: use_bidirect and obmc have been removed since DivX4)
    
    divx->x_dim     = vob->ex_v_width;
    divx->y_dim     = vob->ex_v_height;
    divx->framerate = vob->ex_fps;
    divx->bitrate   = vob->divxbitrate*1000;

    //recommended (advanced) parameter

    divx->min_quantizer      = vob->min_quantizer;
    divx->max_quantizer      = vob->max_quantizer;
    divx->rc_period          = vob->rc_period;
    divx->rc_reaction_period = vob->rc_reaction_period; 
    divx->rc_reaction_ratio  = vob->rc_reaction_ratio;

    divx->max_key_interval   = vob->divxkeyframes;
    divx->quality            = vob->divxquality;

    divx->deinterlace=(vob->deinterlace==2) ? 1:0; // fast deinterlace = 1
    divx->handle=NULL;

    if(divx5_encore(NULL, ENC_OPT_INIT, divx, NULL) < 0) {
      tc_warn("DivX codec init error");
      return(TC_EXPORT_ERROR); 
    }

    // catch API mismatch
    if(!divx || !divx->handle) {
      tc_warn("DivX codec open error");
      return(TC_EXPORT_ERROR); 
    }
    
    if(verbose_flag & TC_DEBUG) 
      {
       //-- GMO start -- 
        if (vob->divxmultipass == 3) { 
          fprintf(stderr, "[%s]    single-pass session: %d (VBR)\n", MOD_NAME, vob->divxmultipass);
          fprintf(stderr, "[%s]          VBR-quantizer: %d\n", MOD_NAME, vob->divxbitrate);
        } else {
	  fprintf(stderr, "[%s]     multi-pass session: %d\n", MOD_NAME, vob->divxmultipass);
	  fprintf(stderr, "[%s]      bitrate [kBits/s]: %d\n", MOD_NAME, divx->bitrate/1000);
	}
	
	fprintf(stderr, "[%s]                quality: %d\n", MOD_NAME, divx->quality);
        //-- GMO end --

	fprintf(stderr, "[%s]              crispness: %d\n", MOD_NAME, vob->divxcrispness);
	fprintf(stderr, "[%s]  max keyframe interval: %d\n", MOD_NAME, divx->max_key_interval);
	fprintf(stderr, "[%s]             frame rate: %.2f\n", MOD_NAME, vob->ex_fps);
	fprintf(stderr, "[%s]            color space: %s\n", MOD_NAME, (vob->im_v_codec==CODEC_RGB) ? "RGB24":"YV12");
	fprintf(stderr, "[%s]            deinterlace: %d\n", MOD_NAME, divx->deinterlace);
    }

    encode.colorspace = (vob->im_v_codec==CODEC_RGB) ? ENC_CSP_RGB24:ENC_CSP_YV12;
    encode.mvs = NULL;

    encode.bitstream = buffer;

    
    switch(VbrMode) {
	
    case 1:
	VbrControl_init_2pass_vbr_analysis(vob->divxlogfile, divx->quality);   
	break;
	
    case 2:
      
      // check for logfile
      
      if(vob->divxlogfile==NULL || stat(vob->divxlogfile, &fbuf)){
	fprintf(stderr, "(%s) pass-1 logfile \"%s\" not found exit\n", __FILE__, 
		vob->divxlogfile);
	return(TC_EXPORT_ERROR);
    }


      // second pass: read back the logfile
      VbrControl_init_2pass_vbr_encoding(vob->divxlogfile, 
					 divx->bitrate, 
					 divx->framerate, 
					 vob->divxcrispness, 
					 divx->quality);
      break;

      //-- GMO start --  
    case 3:
	VbrControl_init_2pass_vbr_analysis(vob->divxlogfile, divx->quality);   

      encode.quant = vob->divxbitrate;
      encode.intra = -1;
      break;
      //-- GMO end --
 
    default:
      // none
      break;
    }
#endif
    
    return(0);
  }
  
  if(param->flag == TC_AUDIO) return(audio_init(vob, verbose));    
  
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

  // open file
  if(vob->avifile_out==NULL) 
    if(NULL == (vob->avifile_out = AVI_open_output_file(vob->video_out_file))) {
      AVI_print_error("avi open error");
      return(TC_EXPORT_ERROR); 
    }
  
  /* save locally */
  avifile = vob->avifile_out;

  if(param->flag == TC_AUDIO) return(audio_open(vob, vob->avifile_out));  
  
  if(param->flag == TC_VIDEO) {
    
    // video
#if ENCORE_MAJOR_VERSION >= 5010
    AVI_set_video(vob->avifile_out, vob->ex_v_width, vob->ex_v_height, 
		  vob->ex_fps, "DX50");
#else
    AVI_set_video(vob->avifile_out, vob->ex_v_width, vob->ex_v_height, 
		  vob->ex_fps, "DIVX");
#endif
    
    if (vob->avi_comment_fd>0)
	AVI_set_comment_fd(vob->avifile_out, vob->avi_comment_fd);

    //do not force key frame at the very beginning of encoding, since
    //first frame will be a key fame anayway. Therefore key.quantizer
    //is well defined for any frame to follow
    force_key_frame=(force_key_frame<0) ? 0:1;
    
    return(0);
  }
  
  // invalid flag
  return(TC_EXPORT_ERROR); 
}


/* ------------------------------------------------------------ 
 *
 * encode and export frame
 *
 * ------------------------------------------------------------*/


MOD_encode
{
  
  if(param->flag == TC_VIDEO) { 
    
    // encode video
    
    encode.image = param->buffer;
    encode.bitstream = buffer;
    
#if ENCORE_VERSION >= 20021024
    encode.produce_empty_frame = 0;

    do {
	if(divx5_encore(encore_handle, ENC_OPT_ENCODE, &encode, &key) < 0) {
	    tc_warn("DivX encoder error");
	    return(TC_EXPORT_ERROR); 
	}	
	// write bitstream
	if(key.cType != '\0') {
	    /*
	    printf("Quant(%d) Type(%c) Motion(%d) Texture(%d) Stuffing(%d) Total(%d) SeqNr(%d) length(%d)\n",
		    key.iQuant, key.cType, key.iMotionBits, key.iTextureBits, 
		    key.iStuffingBits, key.iTotalBits, key.iSequenceNumber, encode.length);
		    */

	    /* split the AVI */
	    if((uint32_t)(AVI_bytes_written(avifile)+encode.length+16+8)>>20 >= tc_avi_limit) 
		tc_outstream_rotate_request();

	    //0.6.2: switch outfile on "C" and -J pv
	    if(key.cType == 'I') tc_outstream_rotate();

	    if(AVI_write_frame(avifile, buffer, encode.length, (key.cType == 'I')?1:0)<0) {
		tc_warn("DivX avi video write error");
		return(TC_EXPORT_ERROR); 
	    }
	}
	encode.image = NULL;
    } while (encode.length >= 0 && key.cType != '\0');
#else
    switch(VbrMode) {

   //-- GMO start --   
    case 3:
    
      if (force_key_frame)
      {
        encode.intra    = 1;
        force_key_frame = 0;
      }
      else
        encode.intra = -1;
          
      if(divx5_encore(divx->handle, ENC_OPT_ENCODE_VBR, &encode, &key) < 0) 
      {
	printf("encoder error");
	return(TC_EXPORT_ERROR); 
      }


      VbrControl_update_2pass_vbr_analysis(key.is_key_frame, 
					       key.motion_bits, 
					       key.texture_bits, 
					       key.total_bits, 
					       key.quantizer);
      break;
    //-- GMO end --
 	
    case 2:
	// second pass of 2-pass, just a hack for the moment

	encode.quant = VbrControl_get_quant();		
	encode.intra = VbrControl_get_intra();

	if(force_key_frame) {
	    encode.intra=1;    //key frame
	    force_key_frame=0; //reset
	}
	
      if(divx5_encore(divx->handle, ENC_OPT_ENCODE_VBR, &encode, &key) < 0) {
	printf("encoder error");
	return(TC_EXPORT_ERROR); 
      }
      
      VbrControl_update_2pass_vbr_encoding(key.motion_bits, 
					   key.texture_bits, 
					   key.total_bits);
      break;

    default:
      
      if(force_key_frame) {
	
	encode.intra=1; //key frame
	encode.quant=key.quantizer; //well defined for frames != first frame.
	
	if(divx5_encore(divx->handle, ENC_OPT_ENCODE_VBR, &encode, &key) < 0) {
	  printf("encoder error");
	  return(TC_EXPORT_ERROR); 
	}
	
	//reset
	force_key_frame=0; 

      } else {
	  
	  if(divx5_encore(divx->handle, ENC_OPT_ENCODE, &encode, &key) < 0) {
	      printf("encoder error");
	      return(TC_EXPORT_ERROR); 
	  }
      }
      
      // first pass of two-pass, save results
      if(VbrMode==1) 
	  VbrControl_update_2pass_vbr_analysis(key.is_key_frame, 
					       key.motion_bits, 
					       key.texture_bits, 
					       key.total_bits, 
					       key.quantizer);
      break;
    }

    // write bitstream

    /* split the AVI */
    if((uint32_t)(AVI_bytes_written(avifile)+encode.length+16+8)>>20 >= tc_avi_limit) 
	tc_outstream_rotate_request();

    //0.6.2: switch outfile on "C" and -J pv
    if(key.is_key_frame) tc_outstream_rotate();
  
    if(AVI_write_frame(avifile, buffer, encode.length, key.is_key_frame)<0) {
	fprintf(stderr, "DivX avi video write error\n");
	return(TC_EXPORT_ERROR); 
    }
#endif
    
    return(0);
  }
  
  if(param->flag == TC_AUDIO) return(audio_encode(param->buffer, param->size, avifile));  
  
  // invalid flag
  return(TC_EXPORT_ERROR); 
}

/* ------------------------------------------------------------ 
 *
 * close codec
 *
 * ------------------------------------------------------------*/

MOD_close
{  

  vob_t *vob = tc_get_vob();
  if(param->flag == TC_AUDIO) return(audio_close()); 
    
  if(vob->avifile_out!=NULL) {
    AVI_close(vob->avifile_out);
    vob->avifile_out=NULL;
  }
  
  if(param->flag == TC_VIDEO) return(0);
  
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
#if ENCORE_VERSION >= 20021024
    if(divx5_encore(encore_handle, ENC_OPT_RELEASE, NULL, NULL) < 0) {
      tc_warn("DivX encoder close error\n");
    }
#else
    if(divx5_encore(divx->handle, ENC_OPT_RELEASE, NULL, NULL) < 0) {
      fprintf(stderr, "DivX encoder close error\n");
    }
#endif

    if(buffer!=NULL) {
	free(buffer);
	buffer=NULL;
    }
    
    //remove codec
    dlclose(handle);
    
#if ENCORE_VERSION >= 20021024
#else
    switch(VbrMode) {
	
    case 1:
    case 2:
    case 3:
	VbrControl_close();
	break;

    default:
	break;
    }
#endif
    
    return(0);
  }

  if(param->flag == TC_AUDIO) return(audio_stop());  
  
  return(TC_EXPORT_ERROR);     
}

