/*
 *  export_divx5raw.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  2-pass code OpenDivX port: "-R 1", "-R 2"
 *  Copyright (C) 2001 Christoph Lampert <lampert@math.chalmers.se> 
 *
 *  constant quantizer extensions "-R 3" by Gerhard Monzel 
 *  <gerhard.monzel@sap.com>
 *
 *  This module is derived from export_divx4raw.c, minor modification by
 *  Loïc Le Loarer <lll_tr@m4x.org>
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
#include <dlfcn.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "divx5_encore2.h"
#include "transcode.h"
#include "avilib.h"
#include "aud_aux.h"
#include "vbr.h"

#define MOD_NAME    "export_divx5raw.so"
#define MOD_VERSION "v0.3.5 (2002-12-19)"
#define MOD_CODEC   "(video) DivX 5.xx (ES) | (audio) MPEG/AC3/PCM"

#define MOD_PRE divx5raw
#include "export_def.h"

int VbrMode=0;
int force_key_frame=-1;

static int fd;
  
//temporary audio/video buffer
static char *buffer;
#define BUFFER_SIZE SIZE_RGB_FRAME<<1

ENC_PARAM   *divx;
ENC_FRAME  encode;
ENC_RESULT    key;

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_YUV|TC_CAP_AC3|TC_CAP_AUD;

// dl stuff
static int (*divx5_encore)(void *para0, int opt, void *para1, void *para2);
static void *handle;
static char module[TC_BUF_MAX];

#define MODULE "libdivxencore.so"

static int p_write (int fd, char *buf, size_t len)
{
   size_t n = 0;
   size_t r = 0;

   while (r < len) {
      n = write (fd, buf + r, len - r);
      if (n < 0)
         return n;
      
      r += n;
   }
   return r;
}


static int divx5_init(char *path) {
#ifdef __FreeBSD__ /* Just in case ProjectMayo will release FreeBSD library :-) */  
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
      fputs (dlerror(), stderr);
      return(-1);
    } else {  
      if(verbose_flag & TC_DEBUG) 
	fprintf(stderr, "loading external codec module %s\n", MODULE); 
    }
  } else {  
    if(verbose_flag & TC_DEBUG) 
      fprintf(stderr, "loading external codec module %s\n", module); 
  }
  
  divx5_encore = dlsym(handle, "encore");   
  
  if ((error = dlerror()) != NULL)  {
    fputs(error, stderr);
    return(-1);
  }
  
  quiet_encore=dlsym(handle, "quiet_encore"); 
  
  if ((error = dlerror()) != NULL)  {
    fputs(error, stderr);
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

  struct stat fbuf;
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
      printf("failed to init DivX 5.0 Codec");
      return(TC_EXPORT_ERROR); 
    }

    if ((divx = malloc(sizeof(ENC_PARAM)))==NULL) {
      perror("out of memory");
      return(TC_EXPORT_ERROR); 
    }

    //important parameter
    divx->x_dim     = vob->ex_v_width;
    divx->y_dim     = vob->ex_v_height;
    divx->framerate = vob->fps;
    divx->bitrate   = vob->divxbitrate*1000;

    //recommended (advanced) parameter
    divx->min_quantizer      = vob->min_quantizer;
    divx->max_quantizer      = vob->max_quantizer;
    divx->rc_period          = vob->rc_period;
    divx->rc_reaction_period = vob->rc_reaction_period; 
    divx->rc_reaction_ratio  = vob->rc_reaction_ratio;

    divx->max_key_interval   = vob->divxkeyframes;
    divx->quality            = vob->divxquality;

    //divx->use_bidirect=0;            // unused, set to 0
    divx->deinterlace=(vob->deinterlace==2) ? 1:0; // fast deinterlace = 1
    //divx->obmc=0;                    // unused, set to 0
    divx->handle=NULL;

    if(divx5_encore(NULL, ENC_OPT_INIT, divx, NULL) < 0) {
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
	fprintf(stderr, "[%s]             frame rate: %.2f\n", MOD_NAME, vob->fps);
	fprintf(stderr, "[%s]            color space: %s\n", MOD_NAME, (vob->im_v_codec==CODEC_RGB) ? "RGB24":"YV12");
	fprintf(stderr, "[%s]            deinterlace: %d\n", MOD_NAME, divx->deinterlace);
    }

    encode.bitstream = buffer;

    encode.colorspace = (vob->im_v_codec==CODEC_RGB) ? ENC_CSP_RGB24:ENC_CSP_YV12;
    encode.mvs = NULL;
    
    VbrMode = vob->divxmultipass;
    // 0 for nothing,  
    // 1 for DivX 5.0 - first-pass, 
    // 2 for DivX 5.0 - second pass
    
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

  if(param->flag == TC_AUDIO) return(audio_open(vob, NULL));  
    
  if(param->flag == TC_VIDEO) {

    // video
    if((fd = open(vob->video_out_file, O_RDWR|O_CREAT|O_TRUNC,
		  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH))<0) {
      perror("open file");
      
      return(TC_EXPORT_ERROR);
    }     

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
          
      if(divx5_encore(divx->handle, ENC_OPT_ENCODE_VBR, &encode, &key) < 0) 
      {
	printf("encoder error");
	return(TC_EXPORT_ERROR); 
      }
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
    
    if(p_write(fd, buffer, encode.length) != encode.length) {    
      perror("write frame");
      return(TC_EXPORT_ERROR);
    }     
    
    return(0);
  }
  
  if(param->flag == TC_AUDIO) return(audio_encode(param->buffer, param->size, NULL));  
  
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

  if(param->flag == TC_AUDIO) return(audio_close()); 
  
  if(param->flag == TC_VIDEO) {
    close(fd);
    return(0);
  }
  
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
    if(divx5_encore(divx->handle, ENC_OPT_RELEASE, NULL, NULL) < 0) {
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
	VbrControl_close();
	break;

    default:
	break;
    }
    
    return(0);
  }

  if(param->flag == TC_AUDIO) return(audio_stop());  
  
  return(TC_EXPORT_ERROR);     
}

