/*
 *  export_xvidraw.c
 *
 *  Copyright (C) Thomas Östreich - June 2002
 *  support for XviD_Codec: Christoph Lampert <gruel@web.de> 
 *
 *  Copyright (C) 2002 Christoph Lampert <gruel@web.de>
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

#ifndef __FreeBSD__ /* We have malloc() in stdlib.h */
#include <malloc.h>
#endif
#include "xvid.h"
#include "xvid_vbr.h"

#include "transcode.h"
#include "avilib.h"
#include "aud_aux.h"
#include "config.h"

#define MOD_NAME    "export_xvidraw.so"
#define MOD_VERSION "v0.3.3 (2002-06-14)"
#if API_VERSION == ((2 << 16) | (0))
#define MOD_CODEC   "(video) XviD API 2.0 (ES) | (audio) MPEG/AC3/PCM"
#else
#define MOD_CODEC   "(video) XviD API 1.0 (ES) | (audio) MPEG/AC3/PCM"
#endif

#define MOD_PRE xvidraw 
#include "export_def.h"

int VbrMode=0;
int force_key_frame=-1;
#if API_VERSION == ((2 << 16) | (0))
static int quality = 0;
#endif

static int fd;
  
//temporary audio/video buffer
char *buffer;
#define BUFFER_SIZE SIZE_RGB_FRAME<<1

float quant_array[100][100];

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_YUV|TC_CAP_AC3|TC_CAP_AUD;

// dl stuff
static int (*XviD_encore)(void *para0, int opt, void *para1, void *para2);
static int (*XviD_init)(void *para0, int opt, void *para1, void *para2);
static void *XviD_encore_handle=NULL;
static void *handle=NULL;
static char module[TC_BUF_MAX];

static int global_colorspace;
static int global_framesize;
static int global_fixedquant;

#define MODULE1 "libxvidcore.so"

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


static int xvid2_init(char *path) {
#ifdef __FreeBSD__
    const
#endif    
    char *error;

    //XviD comes now as a single core-module 
    
    sprintf(module, "%s/%s", path, MODULE1);
    
    // try transcode's module directory
    handle = dlopen(module, RTLD_GLOBAL| RTLD_LAZY);
    
    if (!handle) {

      //try the system default:
      handle = dlopen(MODULE1, RTLD_GLOBAL| RTLD_LAZY);
      
      if (!handle) {
	fputs (dlerror(), stderr);
	return(-1);
      } else {  
	if(verbose_flag & TC_DEBUG) 
	  fprintf(stderr, "loading external codec module %s\n", MODULE1); 
      }
      
    } else {  
      if(verbose_flag & TC_DEBUG) 
	fprintf(stderr, "loading external codec module %s\n", module); 
    }  
    
    
    XviD_encore = dlsym(handle, "xvid_encore");   	/* NEW XviD_API ! */
    XviD_init = dlsym(handle, "xvid_init");   		/* NEW XviD_API ! */
    
    if ((error = dlerror()) != NULL)  {
      fputs(error, stderr);
      return(-1);
    }
    
    return(0);
}


/* ------------------------------------------------------------ 
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{
  XVID_INIT_PARAM xinit;
  XVID_ENC_PARAM xparam;

  struct stat fbuf;
  int xerr; 
  float bpp;

  if(param->flag == TC_VIDEO) 
  {
	bpp = 1000*(vob->divxbitrate)/(vob->fps)/(vob->ex_v_width)/(vob->ex_v_height);
    
	if ((buffer = malloc(BUFFER_SIZE))==NULL) {
		perror("out of memory");
		return(TC_EXPORT_ERROR); 
	} else
	      memset(buffer, 0, BUFFER_SIZE);  

	//load the codec

	if(xvid2_init(vob->mod_path)<0) {
		printf("failed to init Xvid codec");
		return(TC_EXPORT_ERROR); 
	}

	VbrMode = vob->divxmultipass;
    
	xinit.cpu_flags = 0;
	XviD_init(NULL, 0, &xinit, NULL);

	xparam.width = vob->ex_v_width;
	xparam.height = vob->ex_v_height;
	if ((vob->fps - (int)vob->fps) == 0)
        {
                xparam.fincr = 1;
                xparam.fbase = (int)vob->fps;
        }
        else
        {
                xparam.fincr = 1001;
                xparam.fbase = (int)(1001 * vob->fps);
        }
	if ( (VbrMode==3) )
		xparam.bitrate = 0;		// disable rate-control
	else 
	  	xparam.bitrate = vob->divxbitrate*1000;
		
	//recommended (advanced) parameter
#if API_VERSION == ((2 << 16) | (0))
	//	xparam.rc_buffersize      = 10 * vob->divxbitrate*1000;
	xparam.rc_buffersize      = 16;
	quality                   = (vob->divxquality>5)?5:vob->divxquality;

#else
	xparam.rc_period          = vob->rc_period;
	xparam.rc_reaction_period = vob->rc_reaction_period; 
	xparam.rc_reaction_ratio  = vob->rc_reaction_ratio;

        xparam.motion_search      = vob->divxquality;
        xparam.lum_masking = 0; // Luminance Masking is still under development
        xparam.quant_type = 0;  // 0=h.263, 1=mpeg4
#endif

	xparam.min_quantizer      = vob->min_quantizer;
	xparam.max_quantizer      = vob->max_quantizer;
	xparam.max_key_interval   = vob->divxkeyframes;

        xerr = XviD_encore(NULL, XVID_DEC_CREATE, &xparam, NULL);

	if(xerr == XVID_ERR_FAIL)
	{
		printf("codec open error");
		return(TC_EXPORT_ERROR); 
	}
        XviD_encore_handle=xparam.handle;
    
	if (verbose_flag & TC_DEBUG) 
	{
	fprintf(stderr, "[%s]     multi-pass session: %d\n", MOD_NAME, vob->divxmultipass);
#if API_VERSION == ((2 << 16) | (0))
	fprintf(stderr, "[%s]                quality: %d\n", MOD_NAME, quality);
#else
	fprintf(stderr, "[%s]                quality: %d\n", MOD_NAME, xparam.motion_search);
#endif
	fprintf(stderr, "[%s]      bitrate [kBits/s]: %d\n", MOD_NAME, xparam.bitrate/1000);
	fprintf(stderr, "[%s]  max keyframe interval: %d\n", MOD_NAME, xparam.max_key_interval);
	fprintf(stderr, "[%s]             frame rate: %.2f\n", MOD_NAME, vob->fps);
	fprintf(stderr, "[%s]            color space: %s\n", MOD_NAME, (vob->im_v_codec==CODEC_RGB) ? "RGB24":"YV12");
//	fprintf(stderr, "[%s]            deinterlace: %d\n", MOD_NAME, divx->deinterlace);
	}

	switch (vob->im_v_codec)
	{
	case CODEC_RGB:	
		global_framesize = SIZE_RGB_FRAME;
		global_colorspace = XVID_CSP_RGB24;
		break;
	case CODEC_YUV:
		global_framesize = SIZE_RGB_FRAME*2/3;
		global_colorspace = XVID_CSP_YV12;
		break;
	default: /* down know... simply use YV12, too... */
		global_framesize = SIZE_RGB_FRAME*2/3;
		global_colorspace = XVID_CSP_YV12;
		break;
	}			


	switch(VbrMode) 
	{
	
	case 1:
		VbrControl_init_2pass_vbr_analysis(vob->divxlogfile, vob->divxquality, bpp);   
		break;
	
	case 2:	 // check for logfile
		if(vob->divxlogfile==NULL || stat(vob->divxlogfile, &fbuf))
      		{
		fprintf(stderr, "(%s) pass-1 logfile \"%s\" not found exit\n", __FILE__, vob->divxlogfile);
		return(TC_EXPORT_ERROR);
		}
// second pass: read back the logfile
		VbrControl_init_2pass_vbr_encoding(vob->divxlogfile, vob->divxbitrate*1000, 
			vob->fps, vob->divxcrispness, vob->divxquality);
		break;
	case 3:
		global_fixedquant = vob->divxbitrate;
		vob->divxbitrate = 0;
		VbrControl_init_2pass_vbr_analysis(vob->divxlogfile, vob->divxquality, bpp);   
		break;
	default:	      // no option -> single pass
		VbrControl_init_1pass_vbr(vob->divxquality, vob->divxcrispness, bpp);   
	      break;
	}
    
	return(0);
  }
  
  if(param->flag == TC_AUDIO) 
  	return(audio_init(vob, verbose));    
  
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

  if(param->flag == TC_AUDIO) return(audio_open(vob, vob->avifile_out));  

  if(param->flag == TC_VIDEO) {
    
    int mask;
    
    // video
    mask = umask (0);
    umask (mask);
    
    if((fd = open(vob->video_out_file, O_RDWR|O_CREAT|O_TRUNC, (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) &~ mask))<0) {
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
	int xerr;
#if API_VERSION == ((2 << 16) | (0))
int motion_presets[7] = {
                0,
                PMV_QUICKSTOP16,
                PMV_EARLYSTOP16,
                PMV_EARLYSTOP16 | PMV_HALFPELREFINE16,
                PMV_EARLYSTOP16 | PMV_HALFPELREFINE16 | PMV_EARLYSTOP8,
                PMV_EARLYSTOP16 | PMV_HALFPELREFINE16 | PMV_EXTSEARCH16 |
		PMV_EARLYSTOP8 | PMV_HALFPELREFINE8,
                PMV_HALFPELREFINE16 | PMV_EXTSEARCH16 | PMV_HALFPELREFINE8
        };
#endif
	
	XVID_ENC_FRAME xframe;
	XVID_ENC_STATS xstats;

	if(param->flag == TC_AUDIO) 
		return(audio_encode(param->buffer, param->size, NULL));  
  
  	if(param->flag != TC_VIDEO) 
		return(TC_EXPORT_ERROR); 

/* so the rest is TC_VIDEO only */

	xframe.bitstream  = buffer;
	xframe.image      = param->buffer;
	xframe.colorspace = global_colorspace;
	xframe.length     = global_framesize;

#if API_VERSION == ((2 << 16) | (0))
	xframe.general = XVID_HALFPEL | XVID_H263QUANT;

	if(quality > 3)
		xframe.general |= XVID_INTER4V;

	xframe.motion = motion_presets[quality];
	xframe.quant_intra_matrix = xframe.quant_inter_matrix = NULL;
#endif

	switch(VbrMode) 
	{

	case 3: 
		xframe.quant = global_fixedquant;
		xframe.intra = -1;
		if(force_key_frame) 
		{
			    force_key_frame=0; 
			    xframe.intra=1;    		// force keyframe
		}
	
		xerr = XviD_encore(XviD_encore_handle, XVID_ENC_ENCODE, &xframe, &xstats);
		VbrControl_update_2pass_vbr_analysis(xframe.intra, 
			xstats.hlength*8, (xframe.length-xstats.hlength)*8, 
			xframe.length*8, xstats.quant);
		break;

	case 2:	
		xframe.quant = VbrControl_get_quant();
		xframe.intra = VbrControl_get_intra();
		if(force_key_frame) 
		{
			    force_key_frame=0; 
			    xframe.intra=1;    		// force keyframe
		}
	
		xerr = XviD_encore(XviD_encore_handle, XVID_ENC_ENCODE, &xframe, &xstats);

		VbrControl_update_2pass_vbr_encoding(xstats.hlength*8, 
			(xframe.length-xstats.hlength)*8, xframe.length*8);
		break; 

	case 1:
		xframe.quant=0;       
		xframe.intra=-1;
		if (force_key_frame) 
		{
			force_key_frame=0; 
			xframe.intra=1;
		}
	  	xerr = XviD_encore(XviD_encore_handle, XVID_ENC_ENCODE, &xframe, &xstats);
// first pass of two-pass, save results
		VbrControl_update_2pass_vbr_analysis(xframe.intra, 
			xstats.hlength*8, (xframe.length-xstats.hlength)*8, 
			xframe.length*8, xstats.quant);
		break;
		
	default:
		xframe.quant=0;       
		xframe.intra = -1;
		if (force_key_frame) 
		{
			force_key_frame=0; 
			xframe.intra=1;
	      	}
		xerr = XviD_encore(XviD_encore_handle, XVID_ENC_ENCODE, &xframe, &xstats);
		VbrControl_update_1pass_vbr();   
		
    	}
	if (xerr == XVID_ERR_FAIL)
	{
		printf("codec encoding error %d\n",xerr);
		return(TC_EXPORT_ERROR); 
    	}

    // write bitstream
    
    if(p_write(fd, buffer, xframe.length) != xframe.length) {    
      perror("write frame");
      return(TC_EXPORT_ERROR);
    }     
    
    return(0);
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

  if(param->flag == TC_VIDEO) {
    close(fd);
    return(0);
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
	int xerr;
	if(param->flag == TC_VIDEO) 
	{ 
		xerr = XviD_encore(XviD_encore_handle, XVID_ENC_DESTROY, NULL, NULL);
		if (xerr == XVID_ERR_FAIL)	
		{
			printf("encoder close error");
		}

		if(buffer!=NULL) 
		{
			free(buffer);
			buffer=NULL;
		}
    
		//remove codec
		dlclose(handle);
    
		switch(VbrMode) 
		{
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

