/*****************************************************************************
 *  - XviD Transcode Export Module -
 *
 *  Copyright (C) 2001-2003 - Thomas �streich
 *
 *  Original Author    : Christoph Lampert <gruel@web.de> 
 *  Current maintainer : Edouard Gomez <ed.gomez@free.fr>
 *
 *  This file is part of transcode, a linux video stream processing tool
 *      
 *  transcode is free software ; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation ; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY ; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program ; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 *****************************************************************************/

/*****************************************************************************
 * Includes
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#else
# ifdef SYSTEM_DARWIN
#  include "libdldarwin/dlfcn.h"
# endif
#endif

#ifndef SYS_BSD /* We have malloc() in stdlib.h */
# ifdef HAVE_MALLOC_H
# include <malloc.h>
# endif
#endif

#if 1
#define DEVELOPER_USE /* Turns on/off transcode xvid.cfg option file */
#endif

#include "xvid3.h"
#include "xvid_vbr.h"

#include "transcode.h"
#include "avilib.h"
#include "aud_aux.h"

#ifdef DEVELOPER_USE
#include "libioaux/configs.h"
#endif

/*****************************************************************************
 * Transcode module binding functions and strings
 ****************************************************************************/

#define MOD_NAME    "export_xvid3.so"
#define MOD_VERSION "v0.3.12 (2003-07-30)"
#define MOD_CODEC  \
"(video) XviD nonumber series (aka API 3.0)  | (audio) MPEG/AC3/PCM"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_PCM |
                             TC_CAP_RGB |
                             TC_CAP_YUV |
                             TC_CAP_AC3 |
                             TC_CAP_YUV422 |
                             TC_CAP_AUD;
#define MOD_PRE xvid3_ 
#include "export_def.h"

extern pthread_mutex_t delay_video_frames_lock;
extern int video_frames_delay;

/*****************************************************************************
 * Local data
 ****************************************************************************/

extern char* tc_config_dir;

static int VbrMode = 0;
static int encode_fields = 0;
static avi_t *avifile = NULL;
static int rawfd = -1;
  
/* temporary audio/video buffer */
static char *buffer;

/*****************************************************************************
 * Prototypes for shared library symbols
 ****************************************************************************/

/* XviD API bindings - Will be the pointers to XviD shared lib symbols */
static int (*XviD_encore)(void *para0, int opt, void *para1, void *para2);
static int (*XviD_init)(void *para0, int opt, void *para1, void *para2);

static void *XviD_encore_handle = NULL;
static void *handle = NULL;

static int global_colorspace;
static int global_framesize;

/*
 * As these variables are written only once (only by one thread) and then they
 * are just "read only" variables used in MOD_encode, this is not a problem to
 * make them global. We won't suffer simultaneous write/read by threads.
 */
static XVID_INIT_PARAM global_init;
static XVID_ENC_PARAM  global_param;
static XVID_ENC_FRAME  global_frame;
static vbr_control_t   vbr_state;

/* XviD shared library name */
#define XVID_SHARED_LIB_BASE "libxvidcore"
#ifdef SYSTEM_DARWIN
#define XVID_SHARED_LIB_SUFX "dylib"
#else
#define XVID_SHARED_LIB_SUFX "so"
#endif
#ifdef DEVELOPER_USE
#define XVID_CONFIG_FILE "xvid3.cfg"
#endif

#define Max(a,b) (((a)>(b))?(a):(b))
#define Min(a,b) (((a)<(b))?(a):(b))
#define Clamp(var, min, max) (Max(min,Min(var, max)))
#define MakeBool(a) (((a))?1:0)

static int xvid2_init(char *path);

static int xvid_config(XVID_INIT_PARAM *einit,
		       XVID_ENC_PARAM  *eparam,
		       XVID_ENC_FRAME  *eframe,
		       vbr_control_t   *vbr_state,
		       int quality);

static int xvid_print_config(XVID_INIT_PARAM *einit,
			     XVID_ENC_PARAM  *eparam,
			     XVID_ENC_FRAME  *eframe,
			     int quality,
			     int pass,
			     char *csp,
			     int bitrate);

static void xvid_print_vbr(vbr_control_t *state);

static int p_write(int fd, char *buf, size_t len);

/*****************************************************************************
 * Init codec
 ****************************************************************************/

MOD_init
{
	int xerr; 
	int quality;
	float bpp;

	if(param->flag == TC_AUDIO)
		return(audio_init(vob, verbose));

	if(param->flag == TC_VIDEO) {
		int fsize = vob->ex_v_width * vob->ex_v_height;
		bpp = 1000 * (vob->divxbitrate) / 
			(vob->ex_fps * vob->ex_v_width * vob->ex_v_height);
    
		if((buffer = malloc(fsize*3))==NULL) {
			perror("out of memory");
			return(TC_EXPORT_ERROR); 
		} else {
			memset(buffer, 0, fsize*3);  
		}

		/* Load the codec */
		if(xvid2_init(vob->mod_path)<0) {
			fprintf(stderr, "Failed to init XviD codec");
			return(TC_EXPORT_ERROR); 
		}

		VbrMode = vob->divxmultipass;

		encode_fields = vob->encode_fields;

		/* The quality parameter must be in the good range */
		quality = Clamp(vob->divxquality, 0, 5);

		/* Set VBR control defaults */
		vbrSetDefaults(&vbr_state);

		/* Read the config file or set defaults */
		xvid_config(&global_init,
			    &global_param,
			    &global_frame,
			    &vbr_state,
			    quality);

		/* Initialize XviD */
		XviD_init(NULL, 0, &global_init, NULL);

		/* Set values for the unitialized members */
		global_param.width  = vob->ex_v_width;
		global_param.height = vob->ex_v_height;
		if((vob->ex_fps - (int)vob->ex_fps) == 0) {
			global_param.fincr = 1;
			global_param.fbase = (int)vob->ex_fps;
		} else {
			global_param.fincr = 1001;
			global_param.fbase = (int)(1001 * vob->ex_fps);
		}

		if(VbrMode == 0) {
			/* Set the desired bitrate for cbr mode */
			global_param.rc_bitrate = vob->divxbitrate*1000;
		} else {
			/* Disabling rate-control for all vbr modes */
			global_param.rc_bitrate = 0;
		}

		/*
		 * These settings are passed through the command line and are
		 * enforced
		 */
		global_param.min_quantizer    = vob->min_quantizer;
		global_param.max_quantizer    = vob->max_quantizer;
		global_param.max_key_interval = vob->divxkeyframes;

		/* Same thing for vbr module */
		vbr_state.max_key_interval = vob->divxkeyframes;

		/* These xframe settings are used in the MOD_encode */
		if(encode_fields) global_frame.general |= XVID_INTERLACING;

		switch(vob->im_v_codec) {
		case CODEC_RGB:	
			global_framesize = fsize*3;
			global_colorspace = XVID_CSP_RGB24|XVID_CSP_VFLIP;
			break;
		case CODEC_YUV:
			global_framesize = fsize*3/2;
			global_colorspace = XVID_CSP_YV12;
			break;
		case CODEC_YUV422:
			global_framesize = fsize*2;
			global_colorspace = XVID_CSP_UYVY;
			break;
		default: /* down know... simply use YV12, too... */
			global_framesize = fsize*3/2;
			global_colorspace = XVID_CSP_YV12;
			break;
		}			

		global_frame.colorspace = global_colorspace;
		global_frame.length     = global_framesize;

		/* We create the encoder instance */
		xerr = XviD_encore(NULL, XVID_ENC_CREATE, &global_param, NULL);

		if(xerr == XVID_ERR_FAIL) {
			fprintf(stderr, "codec open error");
			return(TC_EXPORT_ERROR); 
		}

		/* Here is our XviD handle which identify our instance */
		XviD_encore_handle = global_param.handle;
    
		/* Overide the vbr settings with command line options */
		vbr_state.fps = (float)((float)global_param.fbase/(float)global_param.fincr);
		vbr_state.debug = (verbose_flag & TC_DEBUG)?1:0;

		switch(VbrMode) {
		case 1:
			/*
			 * Two pass mode - 1st pass : we will collect the
			 * encoder statistics for future use during the 2nd pass
			 */
			vbr_state.mode = VBR_MODE_2PASS_1;
			vbr_state.filename = vob->divxlogfile;
			break;
		case 2:	
			/*
			 * Two pass mode - 2nd pass : the vbr controler will
			 * analyse the statistics file generated during 1st pass
			 * and control XviD quantizer and intra frame values
			 */
			vbr_state.mode = VBR_MODE_2PASS_2;
			vbr_state.filename = vob->divxlogfile;

			/* divxbitrate represents the desired file size in Mo */
			vbr_state.desired_bitrate = vob->divxbitrate*1000;
			break;
		case 3:
			/*
			 * Fixed quantizer mode - the XviD bit rate controller
			 * is totally disabled and a fixed quant is used all
			 * frames long
			 */
			vbr_state.mode = VBR_MODE_FIXED_QUANT;

			/* divxbitrate represents the desired fixed quant */
			vbr_state.fixed_quant = vob->divxbitrate;
			break;
		default:
			/*
			 * One pass mode - The XviD bit rate allocator will be
			 * used. Nothing has to be initilaized.
			 */
			vbr_state.mode = VBR_MODE_1PASS;
			break;
		}


		/* Init the vbr state */
		if(vbrInit(&vbr_state) != 0)
			return(TC_EXPORT_ERROR);

		/* Print debug info if required */
		if(verbose_flag & TC_DEBUG) {

			xvid_print_config(&global_init,
					  &global_param,
					  &global_frame,
					  quality,
					  vob->divxmultipass,
					  (vob->im_v_codec==CODEC_RGB) ?
					  "RGB24":"YV12",
					  vob->divxbitrate);

			if(VbrMode == 2) xvid_print_vbr(&vbr_state);
		}
    
		return(TC_EXPORT_OK);
	}
  
	/* invalid flag */
	return(TC_EXPORT_ERROR); 
}


/*****************************************************************************
 * Open the output file
 ****************************************************************************/

MOD_open
{
	int avi_output = 1;

	/* Check for raw output */
	if((vob->ex_v_fcc != NULL) && (strlen(vob->ex_v_fcc) != 0) &&
	   (strcasecmp(vob->ex_v_fcc, "raw") == 0))
		avi_output = 0;

	/* Open file */
	if(avi_output && vob->avifile_out == NULL) {

		vob->avifile_out = AVI_open_output_file(vob->video_out_file);

		if((vob->avifile_out) == NULL) {
			AVI_print_error("avi open error");
			return(TC_EXPORT_ERROR); 
		}
	}

	/* Save locally */
	avifile = vob->avifile_out;
    
	if(param->flag == TC_AUDIO)
		return(audio_open(vob, vob->avifile_out));
    
	if(param->flag == TC_VIDEO) {

		if(verbose_flag & TC_DEBUG)
			fprintf(stderr, "[%s] Using %s output\n",
				MOD_NAME, avi_output?"AVI":"Raw");
    
		if(avi_output) {
			/* AVI Video output */
			AVI_set_video(vob->avifile_out, vob->ex_v_width,
				      vob->ex_v_height, vob->ex_fps, "XVID");

			if(vob->avi_comment_fd > 0)
				AVI_set_comment_fd(vob->avifile_out,
						   vob->avi_comment_fd);
		} else {
			/* Raw Video output */
			rawfd = open(vob->video_out_file,
				     O_RDWR|O_CREAT|O_TRUNC,
				     S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
			if(rawfd < 0) {
				perror("open file");
				return(TC_EXPORT_ERROR);
			}
		}

		return(TC_EXPORT_OK);
	}
  
	/* invalid flag */
	return(TC_EXPORT_ERROR); 
}


/*****************************************************************************
 * Encode and export a frame
 ****************************************************************************/

MOD_encode
{
	int xerr;

	XVID_ENC_FRAME xframe;
	XVID_ENC_STATS xstats;

	if(param->flag == TC_AUDIO) 
		return(audio_encode(param->buffer, param->size, avifile));  
  
  	if(param->flag != TC_VIDEO) 
		return(TC_EXPORT_ERROR); 

	/* Initialize the local frame copy */
	xframe.bitstream  = buffer;
	xframe.image      = param->buffer;
	xframe.general    = global_frame.general;
	xframe.motion     = global_frame.motion;
	xframe.colorspace = global_frame.colorspace;
	xframe.quant_intra_matrix = global_frame.quant_intra_matrix;
	xframe.quant_inter_matrix = global_frame.quant_inter_matrix;
	xframe.quant = vbrGetQuant(&vbr_state);
	xframe.intra = vbrGetIntra(&vbr_state);
	xframe.bquant = 0;
	xframe.bframe_threshold = global_frame.bframe_threshold;
	xframe.stride = global_param.width;
	if(global_frame.colorspace == (int)(XVID_CSP_VFLIP|XVID_CSP_RGB24))
		xframe.stride *= 3;

  	/* Encode the frame */
	xerr = XviD_encore(XviD_encore_handle, XVID_ENC_ENCODE, &xframe,
			   &xstats);

	/* Error ? */
	if(xerr == XVID_ERR_FAIL) {
		fprintf(stderr, "codec encoding error %d\n", xerr);
		return(TC_EXPORT_ERROR); 
    	}

	/* Update the VBR controler */
	vbrUpdate(&vbr_state,
		  xstats.quant,
		  xframe.intra,
		  xstats.hlength,
		  xframe.length,
		  xstats.kblks,
		  xstats.mblks,
		  xstats.ublks);

	/*
	 * Ported from an anonymous(?) change in export_xvid.c
	 * Original comment:
	 *   0.6.2: switch outfile on "C" and -J pv
	 *
	 * NB: we now test xframe.intra == 1 because xvidocre can return 
	 *     0 == PFrame (predicted frame)
	 *     1 == IFrame (intra frame)
	 *     2 == BFrame (bidirectional frame)
	 *     3 == SFrame (sprite frame)
	 *     4 == packed frame (group of (I|P)B..B frames)
	 *     5 == skiped frame (mostly filling internal buffer)
	 */

	if(rawfd < 0) {
		/* Split the AVI */
		if((uint32_t)(AVI_bytes_written(avifile)+xframe.length+16+8)>>20 >= tc_avi_limit) 
			tc_outstream_rotate_request();

		if(xframe.intra == 1) tc_outstream_rotate();
	}

	if(xframe.intra == 5) { // skipped frame
	    pthread_mutex_lock(&delay_video_frames_lock);
	    video_frames_delay++;
	    pthread_mutex_unlock(&delay_video_frames_lock);
	} else {
		/* Write bitstream */
		if(rawfd < 0) {
			if(AVI_write_frame(avifile, buffer, xframe.length, xframe.intra == 1) < 0) {
				fprintf(stderr, "avi video write error");
				return(TC_EXPORT_ERROR); 
			}
		} else if(p_write(rawfd, buffer, xframe.length)  != xframe.length) {    
			perror("write frame");
			return(TC_EXPORT_ERROR);
		}
	}
    
	return(TC_EXPORT_OK);
}

/*****************************************************************************
 * Close codec
 ****************************************************************************/

MOD_close
{  
	vob_t *vob = tc_get_vob();

	if(param->flag == TC_AUDIO)
		return(audio_close()); 
    
	if(param->flag == TC_VIDEO) {
		if(rawfd >= 0) {
			close(rawfd);
			rawfd = -1;
		}
		if(vob->avifile_out != NULL) {
			AVI_close(vob->avifile_out);
			vob->avifile_out = NULL;
		}
		return(TC_EXPORT_OK);
	}
  
	return(TC_EXPORT_ERROR);
}


/*****************************************************************************
 * Stop encoder
 ****************************************************************************/

MOD_stop
{  
	int xerr;

	if(param->flag == TC_AUDIO)
		return(audio_stop());

	if(param->flag == TC_VIDEO) { 

		/* Stop the encoder */
		xerr = XviD_encore(XviD_encore_handle, XVID_ENC_DESTROY, NULL,
				   NULL);

		if(xerr == XVID_ERR_FAIL) {
			printf("encoder close error");
		}

		/* Free matrices */
		if(global_frame.quant_inter_matrix) {
			free(global_frame.quant_inter_matrix);
			global_frame.quant_inter_matrix = NULL;
		}

		if(global_frame.quant_intra_matrix) {
			free(global_frame.quant_intra_matrix);
			global_frame.quant_intra_matrix = NULL;
		}

		/* Free frame buffer */
		if(buffer != NULL) {
			free(buffer);
			buffer=NULL;
		}
    
		/* Unload XviD core shared library */
		dlclose(handle);
    
		/* Stops the VBR controler */
		vbrFinish(&vbr_state);
    
		return(TC_EXPORT_OK);
  	}

	return(TC_EXPORT_ERROR);
}

/*****************************************************************************
 * Utility functions
 ****************************************************************************/

static int xvid2_init(char *path)
{
#ifdef SYS_BSD
	const
#endif    
		char *error;
	char modules[4][TC_BUF_MAX];
	char *module;
	int i;
	

	/* First we build all lib names we will try to load */
#ifdef SYSTEM_DARWIN
	sprintf(modules[0], "%s/%s.%d.%s", path, XVID_SHARED_LIB_BASE,
		API_VERSION>>16, XVID_SHARED_LIB_SUFX);
#else
	sprintf(modules[0], "%s/%s.%s.%d", path, XVID_SHARED_LIB_BASE,
		XVID_SHARED_LIB_SUFX, API_VERSION>>16);
#endif
#ifdef SYSTEM_DARWIN
	sprintf(modules[1], "%s.%d.%s", XVID_SHARED_LIB_BASE,
		API_VERSION>>16, XVID_SHARED_LIB_SUFX);
#else
	sprintf(modules[1], "%s.%s.%d", XVID_SHARED_LIB_BASE,
		XVID_SHARED_LIB_SUFX, API_VERSION>>16);
#endif
	sprintf(modules[2], "%s/%s.%s", path, XVID_SHARED_LIB_BASE,
		XVID_SHARED_LIB_SUFX);
	sprintf(modules[3], "%s.%s", XVID_SHARED_LIB_BASE,
		XVID_SHARED_LIB_SUFX);

	for(i=0; i<4; i++) {
		module = modules[i];

		if(verbose_flag & TC_DEBUG)
			fprintf(stderr,	"[%s] Trying to load shared lib %s\n",
				MOD_NAME, module);

		/* Try loading the shared lib */
		handle = dlopen(modules[i], RTLD_GLOBAL| RTLD_LAZY);

		/* Test wether loading succeeded */
		if(handle != NULL)
			goto so_loaded;
	}

	/* None of the modules were available */
	fprintf(stderr, dlerror());
	return(-1);

 so_loaded:
	if(verbose_flag & TC_DEBUG)
		fprintf(stderr,	"[%s] Using shared lib %s\n",
			MOD_NAME, module);

	/* Import the XviD init entry point */
	XviD_init   = dlsym(handle, "xvid_init");
    
	/* Something went wrong */
	if((error = dlerror()) != NULL)  {
		fprintf(stderr, error);
		return(-1);
	}

	/* Import the XviD encoder entry point */
	XviD_encore = dlsym(handle, "xvid_encore");

	/* Something went wrong */
	if((error = dlerror()) != NULL)  {
		fprintf(stderr, error);
		return(-1);
	}

	return(0);
}

/*****************************************************************************
 *
 * Configuration of the codec through a configuration file read at runtime
 *
 ****************************************************************************/

static void xvid_config_get_init(XVID_INIT_PARAM *einit,
				 CF_ROOT_TYPE *pRoot,
				 CF_SECTION_TYPE *pSection);

static void xvid_config_get_param(XVID_ENC_PARAM *eparam,
				  CF_ROOT_TYPE *pRoot,
				  CF_SECTION_TYPE *pSection);

static void xvid_config_get_frame(XVID_ENC_FRAME *eframe,
				  CF_ROOT_TYPE *pRoot,
				  CF_SECTION_TYPE *pSection);

static void xvid_config_get_vbr(vbr_control_t *vbr_state,
				CF_ROOT_TYPE *pRoot,
				CF_SECTION_TYPE *pSection);

static void *xvid_read_matrixfile(unsigned char *filename);

static int xvid_config(XVID_INIT_PARAM *einit,
		       XVID_ENC_PARAM  *eparam,
		       XVID_ENC_FRAME  *eframe,
		       vbr_control_t   *evbr_state,
		       int quality)
{
	CF_ROOT_TYPE	* pRoot;
	CF_SECTION_TYPE * pSection;
	struct stat statfile;
	char buffer[1024];

	unsigned long const motion_presets[6] = {
		0,
		0,
		PMV_HALFPELREFINE16,
		PMV_HALFPELREFINE16,
		PMV_HALFPELREFINE16 | PMV_HALFPELREFINE8,
		PMV_HALFPELREFINE16 | PMV_EXTSEARCH16 |
		PMV_USESQUARES16 | PMV_HALFPELREFINE8
	};

	unsigned long const general_presets[6] = {
		XVID_MPEGQUANT,
		XVID_H263QUANT,
		XVID_H263QUANT | XVID_HALFPEL,
		XVID_H263QUANT | XVID_HALFPEL | XVID_INTER4V,
		XVID_H263QUANT | XVID_HALFPEL | XVID_INTER4V,
		XVID_H263QUANT | XVID_HALFPEL | XVID_INTER4V
	};

	/* Avoid quality overflows */
	quality = Clamp(quality, 0, 5);

	/* Hardcoded defaults */

	/* Init flags */
	einit->cpu_flags = 0;

	/* Param flags (-1 = xvid default) */
	eparam->global        = 0;
	eparam->max_bframes   = -1;
	eparam->bquant_ratio  = 150;
	eparam->bquant_offset = 100;
	eparam->frame_drop_ratio = 0;
	eparam->rc_buffer                = -1;
	eparam->rc_reaction_delay_factor = -1;
	eparam->rc_averaging_period      = -1;

	/* Frame flags */
	eframe->general  = general_presets[quality];
	eframe->motion   =  motion_presets[quality];
	eframe->bframe_threshold = 0;  
	eframe->quant_inter_matrix = NULL;
	eframe->quant_intra_matrix = NULL;

#ifdef DEVELOPER_USE

	/* Check conf file existence */
	if (tc_config_dir) {
	    snprintf(buffer, 1024, "%s/%s", tc_config_dir, XVID_CONFIG_FILE);
	} else {
	    snprintf(buffer, 1024, "%s", XVID_CONFIG_FILE);
	}
	if(stat(buffer, &statfile) == -1) {

		if(errno == ENOENT) {
			char *home = getenv("HOME");

			if(home != NULL) {
				snprintf(buffer, 1023, "%s/.transcode/%s", home,
					 XVID_CONFIG_FILE);

				if(stat(buffer, &statfile) == -1) {
					fprintf(stderr,
						"No ./xvid3.cfg nor ~/.transcode/xvid3.cfg"
						" file found, falling back to"
						" hardcoded defaults\n");
					return(0);
				}
			} else {
				return(0);
			}
		} else {
			fprintf(stderr, "Error: %s\nFalling back to hardcoded"
				" defaults\n", strerror(errno));
			return(0);
		}
	}

	if(!S_ISREG(statfile.st_mode)) {
		fprintf(stderr, "%s file is not a regular file ! Falling back"
			" to defaults\n", buffer);
		return(0);
	}

	/*
	 * Now look if the xvid.cfg config file exists and has a [qualityX]
	 * section
	 */
	if(( pRoot = cf_read( buffer ) ) == NULL ) {
		fprintf(stderr, "Error reading configuration file\n");
		return(0);
	}

	snprintf(buffer, 15, "%s%d", "quality", quality);

	if((pSection = cf_get_section(pRoot)) != NULL) {
		do {

			int n;

			/* Test the [qualityX] section */
			n = strncmp(pSection->name, buffer,
				    strlen(buffer));

			if(!n) {
				xvid_config_get_param(eparam, pRoot, pSection);
				xvid_config_get_frame(eframe, pRoot, pSection);
				xvid_config_get_init(einit  , pRoot, pSection);
				continue;
			}

			/* Test the [vbr] section */
			n = strncmp(pSection->name, "vbr", strlen("vbr"));

			if(!n)
				xvid_config_get_vbr(evbr_state, pRoot,pSection);

		} while((pSection=cf_get_next_section(pRoot,pSection)) != NULL);
	}

        CF_FREE_ROOT(pRoot);
#endif

	return(0);
}

/*----------------------------------------------------------------------------
 | Configuration utility functions
 *--------------------------------------------------------------------------*/

/* This flags couples have been obtained using a little sh script
 *
 * #!/bin/sh
 * echo "typedef _config_flag_t"
 * echo "{"
 * echo "  char *flag_string;"
 * echo "  unsigned long flag_value;"
 * echo "}config_flag_t;"
 *
 * echo
 * echo "static config_flag_t xvid_flags[] = {"
 * grep "#define XVID_" xvidcvs.h | awk '{printf("\t{\"%s\", %s},\n", $2, $2)}'
 * echo "};"
 *
 * echo
 * echo "static config_flag_t motion_flags[] = {"
 * grep "#define PMV_" xvidcvs.h | awk '{printf("\t{\"%s\", %s},\n", $2, $2)}'
 * echo "};"
 * EOF
 *
 * Then i splitted/corrected the resulting array according to the different xvid
 * flags. I've also disabled those flags that should not be used with transcode
 * because they lack support.
 */

typedef struct _config_flag_t
{
	char *flag_string;
	unsigned long flag_value;
}config_flag_t;

static config_flag_t const cpu_flags[] = {
	{ "XVID_CPU_MMX",      XVID_CPU_MMX},
	{ "XVID_CPU_MMXEXT",   XVID_CPU_MMXEXT},
	{ "XVID_CPU_SSE",      XVID_CPU_SSE},
	{ "XVID_CPU_SSE2",     XVID_CPU_SSE2},
	{ "XVID_CPU_3DNOW",    XVID_CPU_3DNOW},
	{ "XVID_CPU_3DNOWEXT", XVID_CPU_3DNOWEXT},
	{ "XVID_CPU_TSC",      XVID_CPU_TSC},
	{ "XVID_CPU_FORCE",    XVID_CPU_FORCE},
	{ NULL,                0}
};

static config_flag_t const global_flags[] = {
	{ "XVID_GLOBAL_PACKED",   XVID_GLOBAL_PACKED},
	{ "XVID_GLOBAL_DX50BVOP", XVID_GLOBAL_DX50BVOP},
	{ "XVID_GLOBAL_DEBUG",    XVID_GLOBAL_DEBUG},
	{ NULL,                   0}
};

static config_flag_t const general_flags[] = {
	{ "XVID_VALID_FLAGS",       XVID_VALID_FLAGS},
	{ "XVID_CUSTOM_QMATRIX",    XVID_CUSTOM_QMATRIX},
	{ "XVID_H263QUANT",         XVID_H263QUANT},
	{ "XVID_MPEGQUANT",         XVID_MPEGQUANT},
	{ "XVID_HALFPEL",           XVID_HALFPEL},
	{ "XVID_QUARTERPEL",        XVID_QUARTERPEL},
	{ "XVID_ADAPTIVEQUANT",     XVID_ADAPTIVEQUANT},
	{ "XVID_LUMIMASKING",       XVID_LUMIMASKING},
	{ "XVID_INTERLACING",       XVID_INTERLACING},
	{ "XVID_TOPFIELDFIRST",     XVID_TOPFIELDFIRST},
	{ "XVID_ALTERNATESCAN",     XVID_ALTERNATESCAN},
	{ "XVID_HINTEDME_GET",      XVID_HINTEDME_GET},
	{ "XVID_HINTEDME_SET",      XVID_HINTEDME_SET},
	{ "XVID_INTER4V",           XVID_INTER4V},
	{ "XVID_GREYSCALE",         XVID_GREYSCALE},
	{ "XVID_GRAYSCALE",         XVID_GREYSCALE},
	{ "XVID_GMC",               XVID_GMC},
	{ "XVID_GMC_TRANSLATIONAL", XVID_GMC_TRANSLATIONAL},
	{ "XVID_HQACPRED",          XVID_HQACPRED},
	{ "XVID_MODEDECISION_BITS", XVID_MODEDECISION_BITS},
	{ "XVID_CHROMAOPT",         XVID_CHROMAOPT},
	{ NULL,                     0}
};

static config_flag_t const motion_flags[] = {
	{ "PMV_ADVANCEDDIAMOND8",   PMV_ADVANCEDDIAMOND8},
	{ "PMV_ADVANCEDDIAMOND16",  PMV_ADVANCEDDIAMOND16},
	{ "PMV_HALFPELDIAMOND16",   PMV_HALFPELDIAMOND16},
	{ "PMV_HALFPELREFINE16",    PMV_HALFPELREFINE16},
	{ "PMV_QUARTERPELREFINE16", PMV_QUARTERPELREFINE16},
	{ "PMV_EXTSEARCH16",        PMV_EXTSEARCH16},
	{ "PMV_UNRESTRICTED16",     PMV_UNRESTRICTED16},
	{ "PMV_OVERLAPPING16",      PMV_OVERLAPPING16},
	{ "PMV_USESQUARES16",       PMV_USESQUARES16},
	{ "PMV_CHROMA16",           PMV_CHROMA16},
	{ "PMV_HALFPELDIAMOND8",    PMV_HALFPELDIAMOND8},
	{ "PMV_HALFPELREFINE8",     PMV_HALFPELREFINE8},
	{ "PMV_QUARTERPELREFINE8",  PMV_QUARTERPELREFINE8},
	{ "PMV_EXTSEARCH8",         PMV_EXTSEARCH8},
	{ "PMV_CHROMA8",            PMV_CHROMA8},
	{ "PMV_UNRESTRICTED8",      PMV_UNRESTRICTED8},
	{ "PMV_OVERLAPPING8",       PMV_OVERLAPPING8},
	{ "PMV_USESQUARES8",        PMV_USESQUARES8},
	{ "HALFPELREFINE16_BITS",   HALFPELREFINE16_BITS},
	{ "HALFPELREFINE8_BITS",    HALFPELREFINE8_BITS},
	{ "QUARTERPELREFINE16_BITS",QUARTERPELREFINE16_BITS},
	{ "QUARTERPELREFINE8_BITS", QUARTERPELREFINE8_BITS},
	{ "EXTSEARCH_BITS",         EXTSEARCH_BITS},
	{ "CHECKPREDICTION_BITS",   CHECKPREDICTION_BITS},
	{ NULL,                     0}
};

static unsigned long string2flags(char *string, config_flag_t const *flags);

#define cf_get_named_key(x,y,z) cf_get_named_section_value_of_key(x,y,z)
#define cf_get(key) cf_get_named_key( pRoot, pSection->name, key)

static void xvid_config_get_init(XVID_INIT_PARAM *einit,
				 CF_ROOT_TYPE *pRoot,
				 CF_SECTION_TYPE *pSection)
{

	char *pTemp;

	/* Get the cpu value */
	if( ( pTemp = cf_get("init.cpu_flags" ) ) != NULL ) {
		einit->cpu_flags = string2flags(pTemp, cpu_flags);
	}

	return;

}

static void xvid_config_get_param(XVID_ENC_PARAM *eparam,
				  CF_ROOT_TYPE *pRoot,
				  CF_SECTION_TYPE *pSection)
{
	char *pTemp;

	/* Get the global value */
	if( ( pTemp = cf_get("param.global" ) ) != NULL ) {
		eparam->global = string2flags(pTemp, global_flags);
	}

	/* Get the max_bframes value */
	if( ( pTemp = cf_get("param.max_bframes" ) ) != NULL ) {
		int n = atoi(pTemp);
		eparam->max_bframes = Clamp(n, -1, 4);
	}

	/* Get the bquant_ratio value */
	if( ( pTemp = cf_get("param.bquant_ratio" ) ) != NULL ) {
		int n = atoi(pTemp);
		eparam->bquant_ratio = Clamp(n, 0, 200);
	}

	/* Get the bquant_offset value */
	if( ( pTemp = cf_get("param.bquant_offset" ) ) != NULL ) {
		int n = atoi(pTemp);
		eparam->bquant_offset = Clamp(n, 0, 3000);
	}

	/* Get the frame drop ratio value */
	if( ( pTemp = cf_get("param.frame_drop_ratio" ) ) != NULL ) {
		int n = atoi(pTemp);
		eparam->frame_drop_ratio = Clamp(n, 0, 100);
	}

	/* Get the rc_reaction_delay_factor value */
	if( ( pTemp = cf_get("param.rc_reaction_delay_factor")) != NULL ) {
		eparam->rc_reaction_delay_factor = Max(0, atoi(pTemp));
	}

	/* Get the rc_averaging_period value */
	if( (pTemp = cf_get("param.rc_averaging_period")) != NULL ) {
		eparam->rc_averaging_period = Max(0, atoi(pTemp));
	}

	/* Get the rc_buffer value */
	if( (pTemp = cf_get("param.rc_buffer")) != NULL ) {
		eparam->rc_buffer = Max(0, atoi(pTemp));
	}

	/* Get the min_quantizer value */
	if( (pTemp = cf_get("param.min_quantizer")) != NULL ) {
		int n = atoi(pTemp);
		eparam->min_quantizer = Clamp(n, 1, 31);
	}

	/* Get the max_quantizer value */
	if( (pTemp = cf_get("param.max_quantizer")) != NULL ) {
		int n = atoi(pTemp);
		eparam->max_quantizer = Clamp(n, 1, 31);
	}

	return;
}

static void xvid_config_get_frame(XVID_ENC_FRAME *eframe,
				  CF_ROOT_TYPE *pRoot,
				  CF_SECTION_TYPE *pSection)
{
	char *pTemp;

	/* Get the motion value */
	if( ( pTemp = cf_get("frame.motion" ) ) != NULL ) {
		eframe->motion = string2flags(pTemp, motion_flags);
	}

	/* Get the general value */
	if( ( pTemp = cf_get("frame.general" ) ) != NULL ) {
		eframe->general = string2flags(pTemp, general_flags);
	}

	if( ( pTemp = cf_get("frame.bframe_threshold")) != NULL) {
		int n = atoi(pTemp);
		eframe->bframe_threshold = Clamp(n, -255, +255);
	}

	/* Get the inter matrix filename and read the matrix */
	if( (pTemp = cf_get("frame.quant_inter_matrix")) != NULL ) {
		eframe->quant_inter_matrix = xvid_read_matrixfile(pTemp);
	}

	/* Get the inter matrix filename and read the matrix */
	if( (pTemp = cf_get("frame.quant_intra_matrix")) != NULL ) {
		eframe->quant_intra_matrix = xvid_read_matrixfile(pTemp);
	}

	/*
	 * Just to avoid common error - custom matrices are just usable if
	 * using mpeg quantization type - Enforce it.
	 */
	if(eframe->general & XVID_CUSTOM_QMATRIX) {

		/* Is there at least one filled matrix ? */
		if(eframe->quant_inter_matrix != NULL ||
		   eframe->quant_intra_matrix != NULL) {

			/* There's one - turn off h263 and turn on mpeg */
			eframe->general &= ~XVID_H263QUANT;
			eframe->general |= XVID_MPEGQUANT;
		} else {
			/* There's none - why custom matrix is set ?! */
			eframe->general &= ~XVID_CUSTOM_QMATRIX;
		}
	}

	return;
}

static void *xvid_read_matrixfile(unsigned char *filename)
{
	int i;
	unsigned char *matrix;
	FILE *input;
	
	/* Allocate matrix space */
	if((matrix = malloc(64*sizeof(unsigned char))) == NULL)
	   return(NULL);

	/* Open the matrix file */
	if((input = fopen(filename, "rb")) == NULL) {
		free(matrix);
		return(NULL);
	}

	/* Read the matrix */
	for(i=0; i<64; i++) {

		int value;

		/* If fscanf fails then get out of the loop */
		if(fscanf(input, "%d", &value) != 1) {
			fprintf(stderr,
				"[%s]\tError: The matrix file %s is corrupted\n",
				MOD_NAME,
				filename);
			free(matrix);
			fclose(input);
			return(NULL);
		}

		/* Clamp the value to safe range */
		matrix[i] = Clamp(value, 1, 255);
	}

	/* Fills the rest with 1 */
	while(i<64) matrix[i++] = 1;

	/* We're done */
	fclose(input);

	return(matrix);
	
}

#if 0
static config_flag_t const xvidvbr_mode[] = {
	{"VBR_MODE_1PASS",       VBR_MODE_1PASS},
	{"VBR_MODE_2PASS_1",     VBR_MODE_2PASS_1},
	{"VBR_MODE_2PASS_2",     VBR_MODE_2PASS_2},
	{"VBR_MODE_FIXED_QUANT", VBR_MODE_FIXED_QUANT},
	{NULL, 0}
};
#endif

static config_flag_t const credits_mode[] = {
	{"VBR_CREDITS_MODE_RATE",  VBR_CREDITS_MODE_RATE},
	{"VBR_CREDITS_MODE_QUANT", VBR_CREDITS_MODE_QUANT},
	{"VBR_CREDITS_MODE_SIZE",  VBR_CREDITS_MODE_SIZE},
	{NULL, 0}
};

static config_flag_t const altcc_mode[] = {
	{"VBR_ALT_CURVE_SOFT",      VBR_ALT_CURVE_SOFT},
	{"VBR_ALT_CURVE_LINEAR",    VBR_ALT_CURVE_LINEAR},
	{"VBR_ALT_CURVE_AGGRESIVE", VBR_ALT_CURVE_AGGRESIVE},
	{NULL, 0}
};

static config_flag_t const payback_mode[] = {
	{"VBR_PAYBACK_BIAS",         VBR_PAYBACK_BIAS},
	{"VBR_PAYBACK_PROPORTIONAL", VBR_PAYBACK_PROPORTIONAL},
	{NULL, 0}
};

static unsigned long string2mode(char *string, config_flag_t const *modes);

static void xvid_config_get_vbr(vbr_control_t *evbr_state,
				CF_ROOT_TYPE *pRoot,
				CF_SECTION_TYPE *pSection)
{
	char *pTemp;

/* Managed by the command line options */
#if 0
	/* Get the global vbr mode */
	if( ( pTemp = cf_get("mode" ) ) != NULL ) {
		evbr_state->mode = string2mode(pTemp, xvidvbr_mode);
	}
#endif

	/* Get the credits mode */
	if( ( pTemp = cf_get("credits_mode" ) ) != NULL ) {
		evbr_state->credits_mode = (int)string2mode(pTemp,credits_mode);
	}

	/* Do we enable credits mode at starting */
	if( ( pTemp = cf_get("credits_start" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->credits_start = MakeBool(n);
	}

	/* Get the starting credits start frame */
	if( ( pTemp = cf_get("credits_start_begin" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->credits_start_begin = Max(0, n);
	}

	/* Get the starting credits end frame */
	if( ( pTemp = cf_get("credits_start_end" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->credits_start_end = Max(0, n);
	}

	if(evbr_state->credits_start_end < evbr_state->credits_start_begin) {
		int tmp;
		tmp = evbr_state->credits_start_end;
		evbr_state->credits_start_end = evbr_state->credits_start_begin;
		evbr_state->credits_start_begin = tmp;
	}

	/* Do we enable credits mode at starting */
	if( ( pTemp = cf_get("credits_end" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->credits_end = MakeBool(n);
	}

	/* Get the starting credits start frame */
	if( ( pTemp = cf_get("credits_end_begin" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->credits_end_begin = Max(0, n);
	}

	/* Get the starting credits end frame */
	if( ( pTemp = cf_get("credits_end_end" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->credits_end_end = Max(0, n);
	}

	if(evbr_state->credits_end_end < evbr_state->credits_end_begin) {
		int tmp;
		tmp = evbr_state->credits_end_end;
		evbr_state->credits_end_end = evbr_state->credits_end_begin;
		evbr_state->credits_end_begin = tmp;
	}

	/* Get the value */
	if( ( pTemp = cf_get("credits_quant_ratio" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->credits_quant_ratio = Clamp(n, 0, 100);
	}

	/* Get the value */
	if( ( pTemp = cf_get("credits_fixed_quant" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->credits_fixed_quant = Clamp(n, 1, 31);
	}

	/* Get the value */
	if( ( pTemp = cf_get("credits_quant_i" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->credits_quant_i = Clamp(n, 1, 31);
	}

	/* Get the value */
	if( ( pTemp = cf_get("credits_quant_p" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->credits_quant_p = Clamp(n, 1, 31);
	}

	/* Get the value */
	if( ( pTemp = cf_get("credits_start_size" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->credits_start_size = Max(0, n);
	}

	/* Get the value */
	if( ( pTemp = cf_get("credits_end_size" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->credits_end_size = Max(0, n);
	}

	/* Get the value */
	if( ( pTemp = cf_get("keyframe_boost" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->keyframe_boost = Clamp(n, 0, 1000);
	}

	/* Get the value */
	if( ( pTemp = cf_get("kftreshold" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->kftreshold = Max(0, n);
	}
	/* Get the value */
	if( ( pTemp = cf_get("curve_compression_high" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->curve_compression_high = Clamp(n, 0, 100);
	}

	/* Get the value */
	if( ( pTemp = cf_get("curve_compression_low" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->curve_compression_low = Clamp(n, 0, 100);
	}

	/* Get the value */
	if( ( pTemp = cf_get("use_alt_curve" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->use_alt_curve = MakeBool(n);
	}

	/* Get the value */
	if( ( pTemp = cf_get("alt_curve_type" ) ) != NULL ) {
		evbr_state->alt_curve_type =(int)string2mode(pTemp, altcc_mode);
	}

	/* Get the value */
	if( ( pTemp = cf_get("alt_curve_low_dist" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->alt_curve_low_dist = Max(0, n);
	}

	/* Get the value */
	if( ( pTemp = cf_get("alt_curve_high_dist" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->alt_curve_high_dist = Max(0, n);
	}

	/* Get the value */
	if( ( pTemp = cf_get("alt_curve_min_rel_qual" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->alt_curve_min_rel_qual = Clamp(n, 0, 100);
	}

	/* Get the value */
	if( ( pTemp = cf_get("alt_curve_use_auto" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->alt_curve_use_auto = MakeBool(n);
	}

	/* Get the value */
	if( ( pTemp = cf_get("alt_curve_auto_str" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->alt_curve_auto_str = Max(0, n);
	}

	/* Get the value */
	if( ( pTemp = cf_get("alt_curve_use_auto_bonus_bias" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->alt_curve_use_auto_bonus_bias = MakeBool(n);
	}

	/* Get the value */
	if( ( pTemp = cf_get("alt_curve_bonus_bias" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->alt_curve_bonus_bias = Max(0, n);
	}

	/* Get the value */
	if( ( pTemp = cf_get("bitrate_payback_method" ) ) != NULL ) {
		evbr_state->bitrate_payback_method =
			(int)string2mode(pTemp, payback_mode);
	}

	/* Get the value */
	if( ( pTemp = cf_get("bitrate_payback_delay" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->bitrate_payback_delay = Max(0, n);
	}

	/* Get the value */
	if( ( pTemp = cf_get("max_iquant" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->max_iquant = Clamp(n, 1, 31);
	}

	/* Get the value */
	if( ( pTemp = cf_get("min_iquant" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->min_iquant = Clamp(n, 1, 31);
	}

	if(evbr_state->min_iquant > evbr_state->max_iquant) {
		int tmp;
		tmp = evbr_state->min_iquant;
		evbr_state->min_iquant = evbr_state->max_iquant;
		evbr_state->max_iquant = tmp;
	}

	/* Get the value */
	if( ( pTemp = cf_get("max_pquant" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->max_pquant = Clamp(n, 1, 31);
	}

	/* Get the value */
	if( ( pTemp = cf_get("min_pquant" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->min_pquant = Clamp(n, 1, 31);
	}

	if(evbr_state->min_pquant > evbr_state->max_pquant) {
		int tmp;
		tmp = evbr_state->min_pquant;
		evbr_state->min_pquant = evbr_state->max_pquant;
		evbr_state->max_pquant = tmp;
	}

	/* Get the value */
	if( ( pTemp = cf_get("fixed_quant" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->fixed_quant = Clamp(n, 1, 31);
	}

	/* Get the value */
	if( ( pTemp = cf_get("min_key_interval" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->min_key_interval = Max(0, n);
	}

	/* Get the value */
	if( ( pTemp = cf_get("max_key_interval" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->max_key_interval = Max(0, n);
	}

	if(evbr_state->max_key_interval < evbr_state->max_key_interval) {
		int tmp;
		tmp = evbr_state->max_key_interval;
		evbr_state->max_key_interval = evbr_state->min_key_interval;
		evbr_state->min_key_interval = tmp;
	}

	/* Get the value */
	if( ( pTemp = cf_get("debug" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->debug = MakeBool(n);
	}

	/* Get the value */
	if( ( pTemp = cf_get("twopass_max_bitrate" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->twopass_max_bitrate = Max(0, n);
	}

	/* Get the value */
	if( ( pTemp = cf_get("twopass_max_overflow_improvement" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->twopass_max_overflow_improvement = Max(0, n);
	}
	
	if( ( pTemp = cf_get("twopass_max_overflow_degradation" ) ) != NULL ) {
		int n = atoi(pTemp);
		evbr_state->twopass_max_overflow_degradation = Max(0, n);
	}

	return;
}

#undef cf_get
#undef cf_get_named_key

static unsigned long string2mode(char *string, config_flag_t const *modes)
{
	unsigned long mode;
	int i;

	mode = 0;

	for(i=0; modes[i].flag_string != NULL; i++) {

		if(strstr(string, modes[i].flag_string) != NULL) {
			mode = modes[i].flag_value;
			break;
		}


	}

	return(mode);
}

static unsigned long string2flags(char *string, config_flag_t const *flags)
{
	unsigned long flag;
	int i;

	flag = 0;

	for(i=0; flags[i].flag_string != NULL; i++) {
		if(strstr(string, flags[i].flag_string) != NULL)
			flag |= flags[i].flag_value;
	}

	return(flag);
}

static int xvid_print_config(XVID_INIT_PARAM *einit,
			     XVID_ENC_PARAM  *eparam,
			     XVID_ENC_FRAME  *eframe,
			     int quality,
			     int pass,
			     char *csp,
			     int bitrate)
{
	int i;
	
	char *passtype[] =
		{
			"ABR 1 Pass",
			"VBR 1st Pass",
			"VBR 2nd Pass",
			"Constant Quantizer"
		};

	/* What pass is it ? */
	fprintf(stderr, "[%s]\tPass Type: %s\n",
		MOD_NAME, passtype[Clamp(pass, 0 , 3)]);

	/* Quality used */
	fprintf(stderr,	"[%s]\tQuality: %d\n",
		MOD_NAME, quality);

	/* Bitrate */
	switch(pass) {
	case 0:
	case 2:
		fprintf(stderr,	"[%s]\tBitrate [kBits/s]: %d\n",
			MOD_NAME, bitrate);
		break;
	case 3:
		fprintf(stderr,	"[%s]\tConstant Quantizer: %d\n",
			MOD_NAME, bitrate);
		break;
	default:
		fprintf(stderr,	"[%s]\tBitrate: Unknown\n",
			MOD_NAME);
	}

	/* Key frame interval */
	fprintf(stderr,	"[%s]\tMax keyframe Interval: %d\n",
		MOD_NAME, eparam->max_key_interval);

	/* Max bframe sequence */
	fprintf(stderr,	"[%s]\tMax BFrame Sequence: %d\n",
		MOD_NAME, eparam->max_bframes);

	/* Bframe quant ratio */
	fprintf(stderr,	"[%s]\tBFrame Quant Ratio: %d\n",
		MOD_NAME, eparam->bquant_ratio);

	/* Bframe quant offset */
	fprintf(stderr,	"[%s]\tBFrame Quant Offset: %d\n",
		MOD_NAME, eparam->bquant_offset);

	/* Bframe thresholding */
	fprintf(stderr,	"[%s]\tBFrame Threshold: %d\n",
		MOD_NAME, eframe->bframe_threshold);

	/* Motion flags */
	fprintf(stderr,	"[%s]\tMotion Flags:\n",
		MOD_NAME);

	for(i=0; motion_flags[i].flag_string != NULL; i++) {
		if(motion_flags[i].flag_value & eframe->motion)
			fprintf(stderr, "\t\t\t%s\n",
				motion_flags[i].flag_string);
	}

	/* Global flags */
	fprintf(stderr,	"[%s]\tGlobal Flags:\n",
		MOD_NAME);

	for(i=0; global_flags[i].flag_string != NULL; i++) {
		if(global_flags[i].flag_value & eparam->global)
			fprintf(stderr, "\t\t\t%s\n",
				global_flags[i].flag_string);
	}

	/* General flags */
	fprintf(stderr,	"[%s]\tGeneral Flags:\n",
		MOD_NAME);

	for(i=0; general_flags[i].flag_string != NULL; i++) {
		if(general_flags[i].flag_value & eframe->general)
			fprintf(stderr, "\t\t\t%s\n",
				general_flags[i].flag_string);
	}

	/* CPU flags */
	fprintf(stderr,	"[%s]\tCPU Flags:\n",
		MOD_NAME);

	for(i=0; cpu_flags[i].flag_string != NULL; i++) {
		if(cpu_flags[i].flag_value & einit->cpu_flags)
			fprintf(stderr, "\t\t\t%s\n",
				cpu_flags[i].flag_string);
	}

	/* Frame Rate */
	fprintf(stderr,	"[%s]\tFrame Rate: %.2f\n",
		MOD_NAME, (float)((float)eparam->fbase/(float)eparam->fincr));

	/* Color Space */
	fprintf(stderr,	"[%s]\tColor Space: %s\n",
		MOD_NAME, csp);

	/* Matrices */
	if(eframe->quant_intra_matrix != NULL) {

		fprintf(stderr, "[%s]\tIntra Matrix\n", MOD_NAME);

		for(i=0; i<8; i++) {
			int j;
			fprintf(stderr,"\t\t\t");
			for(j=0; j<8; j++)
				fprintf(stderr, "%3d ",
					eframe->quant_intra_matrix[i*8+j]);
			fprintf(stderr,"\n");

		}

	}

	if(eframe->quant_inter_matrix != NULL) {

		fprintf(stderr, "[%s]\tInter Matrix\n", MOD_NAME);

		for(i=0; i<8; i++) {
			int j;
			fprintf(stderr,"\t\t\t");
			for(j=0; j<8; j++)
				fprintf(stderr, "%3d ",
					eframe->quant_inter_matrix[i*8+j]);
			fprintf(stderr,"\n");
		}
	}

	return(0);
}

static void xvid_print_vbr(vbr_control_t *state)
{
	fprintf(stderr, "[%s]\tXviD VBR settings\n",
		MOD_NAME);
	fprintf(stderr, "\t\t\tmode : %d\n",
		state->mode);
	fprintf(stderr, "\t\t\tcredits_mod = %d\n",
		state->credits_mode);
	fprintf(stderr, "\t\t\tcredits_start = %d\n", 
		state->credits_start);
	fprintf(stderr, "\t\t\tcredits_start_begin = %d\n",
		state->credits_start_begin);
	fprintf(stderr, "\t\t\tcredits_start_end = %d\n",
		state->credits_start_end);
	fprintf(stderr, "\t\t\tcredits_end = %d\n",
		state->credits_end);
	fprintf(stderr, "\t\t\tcredits_end_begin = %d\n",
		state->credits_end_begin);
	fprintf(stderr, "\t\t\tcredits_end_end = %d\n",
		state->credits_end_end);
	fprintf(stderr, "\t\t\tcredits_quant_ratio = %d\n",
		state->credits_quant_ratio);
	fprintf(stderr, "\t\t\tcredits_fixed_quant = %d\n",
		state->credits_fixed_quant);
	fprintf(stderr, "\t\t\tcredits_quant_i = %d\n",
		state->credits_quant_i);
	fprintf(stderr, "\t\t\tcredits_quant_p = %d\n",
		state->credits_quant_p);
	fprintf(stderr, "\t\t\tcredits_start_size = %d\n",
		state->credits_start_size);
	fprintf(stderr, "\t\t\tcredits_end_size = %d\n",
		state->credits_end_size);
	fprintf(stderr, "\t\t\tkeyframe_boost = %d\n",
		state->keyframe_boost);
	fprintf(stderr, "\t\t\tkftreshold = %d\n",
		state->kftreshold);
	fprintf(stderr, "\t\t\tkfreduction = %d\n",
		state->kfreduction);
	fprintf(stderr, "\t\t\tmin_key_interval = %d\n",
		state->min_key_interval);
	fprintf(stderr, "\t\t\tmax_key_interval = %d\n",
		state->max_key_interval);
	fprintf(stderr, "\t\t\tcurve_comp_high = %d\n",
		state->curve_compression_high);
	fprintf(stderr, "\t\t\tcurve_comp_low = %d\n",
		state->curve_compression_low);
	fprintf(stderr, "\t\t\tuse_alt_curve = %d\n",
		state->use_alt_curve);
	fprintf(stderr, "\t\t\talt_curve_type = %d\n",
		state->alt_curve_type);
	fprintf(stderr, "\t\t\talt_curve_low_dist = %d\n",
		state->alt_curve_low_dist);
	fprintf(stderr, "\t\t\talt_curve_high_dist = %d\n",
		state->alt_curve_high_dist);
	fprintf(stderr, "\t\t\talt_curve_min_rel_qual = %d\n",
		state->alt_curve_min_rel_qual);
	fprintf(stderr, "\t\t\talt_curve_use_auto = %d\n",
		state->alt_curve_use_auto);
	fprintf(stderr, "\t\t\talt_curve_auto_str = %d\n",
		state->alt_curve_auto_str);
	fprintf(stderr, "\t\t\talt_curve_use_auto_bonus_bias = %d\n",
		state->alt_curve_use_auto_bonus_bias);
	fprintf(stderr, "\t\t\talt_curve_bonus_bias = %d\n",
		state->alt_curve_bonus_bias);
	fprintf(stderr, "\t\t\tbitrate_payback_method = %d\n",
		state->bitrate_payback_method);
	fprintf(stderr, "\t\t\tbitrate_payback_delay = %d\n",
		state->bitrate_payback_delay);
	fprintf(stderr, "\t\t\ttwopass_max_bitrate = %d\n",
		state->twopass_max_bitrate);
	fprintf(stderr, "\t\t\ttwopass_max_overflow_improvement = %d\n",
		state->twopass_max_overflow_improvement);
	fprintf(stderr, "\t\t\ttwopass_max_overflow_degradation = %d\n",
		state->twopass_max_overflow_degradation);
	fprintf(stderr, "\t\t\tmax_iquant = %d\n",
		state->max_iquant);
	fprintf(stderr, "\t\t\tmin_iquant = %d\n",
		state->min_iquant);
	fprintf(stderr, "\t\t\tmax_pquant = %d\n",
		state->max_pquant);
	fprintf(stderr, "\t\t\tmin_pquant = %d\n",
		state->min_pquant);
	fprintf(stderr, "\t\t\tfixed_quant = %d\n",
		state->fixed_quant);
}

static int p_write(int fd, char *buf, size_t len)
{
   size_t n = 0;
   size_t r = 0;

   while(r < len) {
      n = write(fd, buf + r, len - r);
      if(n < 0)
         return n;
      
      r += n;
   }
   return r;
}

/*
 * Please do not modify the tag line.
 *
 * arch-tag: 16c618a5-6cda-4c95-a418-602fc4837824 export_xvid module
 */
