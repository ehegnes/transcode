/*
 *  export_divx4.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  2-pass code OpenDivX port: "-R 1", "-R 2"
 *  Copyright (C) 2001 Christoph Lampert <lampert@math.chalmers.se> 
 *
 *  constant quantizer extensions "-R 3" by Gerhard Monzel 
 *  <gerhard.monzel@sap.com>
 *
 *  divx4/divx5 unification by Alex Stewart <alex@foogod.com>
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

#include "divx5_encore2.h"
#include "divx4_encparam.h"
#include "transcode.h"
#include "avilib.h"
#include "aud_aux.h"
#include "vbr.h"

#define MOD_NAME    "export_divx4.so"
#define MOD_VERSION "v0.3.10 (2003-07-24)"
#define MOD_CODEC   "(video) DivX 4.x/5.x | (audio) MPEG/AC3/PCM"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_YUV|TC_CAP_AC3|TC_CAP_AUD;

#define MOD_PRE divx4
#include "export_def.h"

int VbrMode=0;
int force_key_frame=-1;

static avi_t *avifile=NULL;
  
//temporary audio/video buffer
static char *buffer;

// We'll allocate (and zero) DIVX_STRUCT_PADDING beyond the required size of
// the ENC_PARAM structure.  This is in case a later version of the library API
// adds parameters to the end of the structure, but people try to use a version
// of this module written for the older library, we might still get away with
// using the older structure without risking segfaulting (hey, it's worth a try
// and it doesn't hurt anything).
#define DIVX_STRUCT_PADDING 256

ENC_PARAM   *divx;
ENC_FRAME  encode;
ENC_RESULT    key;

static int encore_version = 0;
static int divx_version = 0;

// dl stuff
static int (*divx_encore)(void *para0, int opt, void *para1, void *para2);
static void *handle;
static char module[TC_BUF_MAX];

#define MODULE "libdivxencore.so"
#define MODULE_V "libdivxencore.so.0"

static int divx_init(char *path) {
#ifdef SYS_BSD  /* Just in case ProjectMayo will release FreeBSD library :-) */  
  const
#endif  
  char *error;
  int *quiet_encore;
  
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
  
  
  divx_encore = dlsym(handle, "encore");   
  
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

// We need to do the initial call with a slightly different structure for the
// older version 4 of the library, so create the 4.x-style struct, copy all the
// data over, do the call, and then copy any results back to our main
// structure..

DIVX4_ENC_PARAM *divx4 = NULL;

int divx_v4_init_codec(ENC_PARAM *params) {
  int result;

  if ((divx4 = malloc(sizeof(*divx4)+DIVX_STRUCT_PADDING))==NULL) {
    perror("out of memory");
    return(TC_EXPORT_ERROR); 
  }
  memset(divx4, 0, sizeof(*divx4)+DIVX_STRUCT_PADDING);

  divx4->x_dim              = params->x_dim;
  divx4->y_dim              = params->y_dim;
  divx4->framerate          = params->framerate;
  divx4->bitrate            = params->bitrate;
  divx4->rc_period          = params->rc_period;
  divx4->rc_reaction_period = params->rc_reaction_period;
  divx4->rc_reaction_ratio  = params->rc_reaction_ratio;
  divx4->max_quantizer      = params->max_quantizer;
  divx4->min_quantizer      = params->min_quantizer;
  divx4->max_key_interval   = params->max_key_interval;
  divx4->quality            = params->quality;
  divx4->deinterlace        = params->deinterlace;

  divx4->handle = NULL;

  result = divx_encore(NULL, ENC_OPT_INIT, divx4, NULL);

  params->x_dim              = divx4->x_dim;
  params->y_dim              = divx4->y_dim;
  params->framerate          = divx4->framerate;
  params->bitrate            = divx4->bitrate;
  params->rc_period          = divx4->rc_period;
  params->rc_reaction_period = divx4->rc_reaction_period;
  params->rc_reaction_ratio  = divx4->rc_reaction_ratio;
  params->max_quantizer      = divx4->max_quantizer;
  params->min_quantizer      = divx4->min_quantizer;
  params->max_key_interval   = divx4->max_key_interval;
  params->quality            = divx4->quality;
  params->deinterlace        = divx4->deinterlace;
  params->handle             = divx4->handle;

  return result;
}

// The structure for version 5 is what we're using by default, so no additional
// fiddling is required here.

int divx_v5_init_codec(ENC_PARAM *params) {
  return divx_encore(NULL, ENC_OPT_INIT, params, NULL);
}

MOD_init
{

  struct stat fbuf;
  int ch;
  int result = 0;
  
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
    
    if ((buffer = malloc(vob->ex_v_height*vob->ex_v_width*3))==NULL) {
      perror("out of memory");
      return(TC_EXPORT_ERROR); 
    } else
      memset(buffer, 0, vob->ex_v_height*vob->ex_v_width*3);  

    //load the codec

    if(divx_init(vob->mod_path)<0) {
      printf("Failed to load DivX 4.x/5.x Codec");
      return(TC_EXPORT_ERROR); 
    }

    if ((divx = malloc(sizeof(*divx)+DIVX_STRUCT_PADDING))==NULL) {
      perror("out of memory");
      return(TC_EXPORT_ERROR); 
    }

    memset(divx, 0, sizeof(*divx)+DIVX_STRUCT_PADDING);

    //important parameter
    
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

    // The first thing we need to do is check what version of the library we're
    // working with.

    encore_version = divx_encore(NULL, ENC_OPT_VERSION, NULL, NULL);

    if (encore_version == DIVX4_ENCORE_VERSION) {
      divx_version = 4;
    } else if (encore_version == ENCORE_VERSION) {
      divx_version = 5;
    } else {
      if (encore_version < ENCORE_VERSION) {
	// The value returned is less than (what we believe to be) the earliest
	// 5.x interface, so we'll make a guess that it's a 4.x-compatible
	// interface.
        divx_version = 4;
      } else {
	// The value returned is greater than the known 5.x interface.  We'll
	// just hope it's more or less backwards-compatible.
        divx_version = 5;
      }
      fprintf(stderr, "[%s] WARNING: Unrecognized API version ID (%d) returned by DivX encore library: Making a guess that it's a %d.x-style interface (please report this message and your DivX library version to the transcode developers)\n", MOD_NAME, encore_version, divx_version);
    }

    if (verbose_flag && TC_DEBUG)
      fprintf(stderr, "[%s] DivX %d.x libraries detected.\n", MOD_NAME, divx_version);

    switch (divx_version) {
      case 4: result = divx_v4_init_codec(divx); break;
      case 5: result = divx_v5_init_codec(divx); break;
    }
    if (result) {
      printf("codec open error");
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

    encode.bitstream = buffer;

    encode.colorspace = (vob->im_v_codec==CODEC_RGB) ? ENC_CSP_RGB24:ENC_CSP_YV12;
    encode.mvs = NULL;
    
    VbrMode = vob->divxmultipass;
    // 0 for nothing,  
    // 1 for DivX 4.x/5.x - first-pass, 
    // 2 for DivX 4.x/5.x - second pass
    
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
    AVI_set_video(vob->avifile_out, vob->ex_v_width, vob->ex_v_height, 
		  vob->ex_fps, "DIVX");

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
          
      if(divx_encore(divx->handle, ENC_OPT_ENCODE_VBR, &encode, &key) < 0) 
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
	
      if(divx_encore(divx->handle, ENC_OPT_ENCODE_VBR, &encode, &key) < 0) {
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
	
	if(divx_encore(divx->handle, ENC_OPT_ENCODE_VBR, &encode, &key) < 0) {
	  printf("encoder error");
	  return(TC_EXPORT_ERROR); 
	}
	
	//reset
	force_key_frame=0; 

      } else {
	  
	  if(divx_encore(divx->handle, ENC_OPT_ENCODE, &encode, &key) < 0) {
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
	printf("avi video write error");
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
    if(divx_encore(divx->handle, ENC_OPT_RELEASE, NULL, NULL) < 0) {
      printf("encoder close error");
    }

    if(buffer!=NULL) {
	free(buffer);
	buffer=NULL;
    }
    
    //remove codec
    dlclose(handle);
    
    switch(VbrMode) {
	
    case 1:
    case 2:
    case 3:
	VbrControl_close();
	break;

    default:
	break;
    }
    
    if (divx) {
      free(divx);
      divx = NULL;
    }
    if (divx4) {
      free(divx4);
      divx4 = NULL;
    }

    return(0);
  }

  if(param->flag == TC_AUDIO) return(audio_stop());  
  
  return(TC_EXPORT_ERROR);     
}

