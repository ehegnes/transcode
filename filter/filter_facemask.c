/*
 *  filter_facemask.c
 *
 *  Copyright (C) Julien Tierny <julien.tierny@wanadoo.fr> - October 2004
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
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA]. 
 *
 */

#define MOD_NAME    "filter_facemask.so"
#define MOD_VERSION "v0.2 (2004-11-01)"
#define MOD_CAP     "Mask people faces in video interviews."

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/* -------------------------------------------------
 *
 * mandatory include files
 *
 *-------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "transcode.h"
#include "framebuffer.h"
#include "filter.h"
/* RGB2YUV features */
#include "../export/vid_aux.h"
#include "optstr.h"

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/


typedef struct parameter_struct{
	int 	xpos, ypos, xresolution, yresolution, xdim, ydim;
} parameter_struct;

static parameter_struct *parameters = NULL;

static void help_optstr(void){
	printf ("[%s] Help:\n", MOD_NAME);
	printf ("\n* Overview:\n");
	printf("  This filter can mask people faces in video interviews.\n");
	printf("  Both YUV and RGB formats are supported, in multithreaded mode.\n");
	printf("\n* Warning:\n");
	printf("  You have to calibrate by your own the mask dimensions and positions so as it fits to your video sample.\n");
	printf("  You also have to choose a resolution that is multiple of the mask dimensions.\n");
	
	printf ("\n* Options:\n");
	printf ("  'xpos':\t\tPosition of the upper left corner of the mask (x)\n");
	printf ("  'ypos':\t\tPosition of the upper left corner of the mask (y)\n");
	printf ("  'xresolution':\tResolution of the mask (width)\n");
	printf ("  'yresolution':\tResolution of the mask (height)\n");
	printf ("  'xdim':\t\tWidth of the mask (= n*xresolution)\n");
	printf ("  'ydim':\t\tHeight of the mask (= m*yresolution)\n");
}

int check_parameters(int x, int y, int w, int h, int W, int H, vob_t *vob){
	
	/* First, we check if the face-zone is contained in the picture */
	if ((x+W) > vob->im_v_width){
		tc_error("[%s] Face zone is larger than the picture !\n", MOD_NAME);
		return -1;
	}
	if ((y+H) > vob->im_v_height){
		tc_error("[%s] Face zone is taller than the picture !\n", MOD_NAME);
		return -1;
	}
	
	/* Then, we check the resolution */
	if ((H%h) != 0) {
		tc_error("[%s] Uncorrect Y resolution !", MOD_NAME);
		return -1;
	}
	if ((W%w) != 0) {
		tc_error("[%s] Uncorrect X resolution !", MOD_NAME);
		return -1;
	}
	return 0;
}

int average_neighbourhood(int x, int y, int w, int h, unsigned char *buffer, int width){
	unsigned int 	red=0, green=0, blue=0;
	int 			i=0,j=0;
	
	for (j=y; j<=y+h; j++){
		for (i=3*(x + width*(j-1)); i<3*(x + w + (j-1)*width); i+=3){
			red 	+= (int) buffer[i];
			green 	+= (int) buffer[i+1];
			blue 	+= (int) buffer[i+2];
		}
	}
	
	red 	/= ((w+1)*h);
	green 	/= ((w+1)*h);
	blue 	/= ((w+1)*h);
	
	/* Now let's print values in buffer */
	for (j=y; j<y+h; j++)
		for (i=3*(x + width*(j-1)); i<3*(x + w + (j-1)*width); i+=3){
			buffer[i] 		= (char)red;
			buffer[i+1] 	= (char)green;
 			buffer[i+2]		= (char)blue;
		}
	return 0;
}

int print_mask(int x, int y, int w, int h, int W, int H, vframe_list_t *ptr){
	int				i=0,j=0;
	for (j=y; j<=y+H; j+=h)
		for (i=x; i<=x+W; i+=w)
			average_neighbourhood(i, j, w, h, ptr->video_buf, ptr->v_width);
	return 0;
}

int tc_filter(vframe_list_t *ptr, char *options){

	static 			vob_t *vob=NULL;

  
  if(ptr->tag & TC_FILTER_GET_CONFIG) {

	optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, "Julien Tierny", "VRYMEO", "1");
	optstr_param(options, "xpos", "Position of the upper left corner of the mask (x)", "%d", "0", "0", "oo");
	optstr_param(options, "ypos", "Position of the upper left corner of the mask (y)", "%d", "0", "0", "oo");
	optstr_param(options, "xresolution", "Resolution of the mask (width)", "%d", "0", "1", "oo");
	optstr_param(options, "yresolution", "Resolution of the mask (height)", "%d", "0", "1", "oo");
	optstr_param(options, "xdim", "Width of the mask (= n*xresolution)", "%d", "0", "1", "oo");
	optstr_param(options, "ydim", "Height of the mask (= m*yresolution)", "%d", "0", "1", "oo");
	return 0;
  }

  //----------------------------------
  //
  // filter init
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_INIT) {
    
    if((vob = tc_get_vob())==NULL)
		return(-1);

	
	/* Now, let's handle the options ... */
	if((parameters = (parameter_struct *) malloc (sizeof(parameter_struct))) == NULL)
		return -1;
	
	/* Filter default options */
	if (verbose & TC_DEBUG)
		tc_info("[%s] Preparing default options.\n", MOD_NAME);
	parameters->xpos 		= 0;
	parameters->ypos 		= 0;
	parameters->xresolution	= 1;
	parameters->yresolution	= 1;
	parameters->xdim		= 1;
	parameters->ydim		= 1;
	
	if (options){
		/* Get filter options via transcode core */
		if (verbose & TC_DEBUG)
			tc_info("[%s] Merging options from transcode.\n", MOD_NAME);
		optstr_get(options, "xpos",  		 	"%d",		&parameters->xpos);
		optstr_get(options, "ypos",   			"%d",		&parameters->ypos);
		optstr_get(options, "xresolution",   	"%d",		&parameters->xresolution);
		optstr_get(options, "yresolution",   	"%d",		&parameters->yresolution);
		optstr_get(options, "xdim",			   	"%d",		&parameters->xdim);
		optstr_get(options, "ydim",			   	"%d",		&parameters->ydim);
		if (optstr_get(options, "help",  "") >=0) help_optstr();
	}
		
	if (vob->im_v_codec == CODEC_YUV){
		if (tc_yuv2rgb_init(vob->im_v_width, vob->im_v_height)<0) {
			tc_error("[%s] Error at YUV to RGB conversion initialization.\n", MOD_NAME);
			return(-1); 
		}
		if (tc_rgb2yuv_init(vob->im_v_width, vob->im_v_height)<0) {
			tc_error("[%s] Error at RGB to YUV conversion initialization.\n", MOD_NAME);
			return(-1); 
		}
	}
	
	if (check_parameters(parameters->xpos, parameters->ypos, parameters->xresolution, parameters->yresolution, parameters->xdim, parameters->ydim, vob) < 0)
		return -1;
	
	if(verbose)
		fprintf(stdout, "[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);
    
    return(0);
  }

  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  
  if(ptr->tag & TC_FILTER_CLOSE) {
  
  	
	
	/* Let's free the parameter structure */
	free(parameters);
	parameters = NULL;
	
	if (vob->im_v_codec == CODEC_YUV){
		if (tc_yuv2rgb_close()<0) {
			tc_error("[%s] Error at YUV to RGB conversion closure.\n", MOD_NAME);
			return(-1); 
		}
		
		if (tc_rgb2yuv_close()<0) {
			tc_error("[%s] Error at RGB to YUV conversion closure.\n", MOD_NAME);
			return(-1); 
		}
	}
    return(0);
  }
  
  //----------------------------------
  //
  // filter frame routine
  //
  //----------------------------------
   
	if(ptr->tag & TC_POST_PROCESS && ptr->tag & TC_VIDEO && !(ptr->attributes & TC_FRAME_IS_SKIPPED)) {


		switch(vob->im_v_codec){
			case CODEC_RGB:
				return print_mask(parameters->xpos, parameters->ypos, parameters->xresolution, parameters->yresolution, parameters->xdim, parameters->ydim, ptr);
				break;
			
			case CODEC_YUV:
				
				if (tc_yuv2rgb_core(ptr->video_buf) == -1){
					tc_error("[%s] Error: cannot convert YUV stream to RGB format !\n", MOD_NAME);
					return -1;
				}
				
				if ((print_mask(parameters->xpos, parameters->ypos, parameters->xresolution, parameters->yresolution, parameters->xdim, parameters->ydim, ptr))<0) return -1;
				if (tc_rgb2yuv_core(ptr->video_buf) == -1){
					tc_error("[%s] Error: cannot convert RGB stream to YUV format !\n", MOD_NAME);
					return -1;
				}
				break;
			
			default: 
				tc_error("[%s] Internal video codec is not supported.\n", MOD_NAME);
				return -1;
		}
	}
	return(0);
}
