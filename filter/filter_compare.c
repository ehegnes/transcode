/*
 *  filter_compare
 *
 *  Copyright (C) Antonio Beamud Montero <antonio.beamud@linkend.com>
 *  Copyright (C) Microgenesis S.A.
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

#define MOD_NAME    "filter_compare.so"
#define MOD_VERSION "v0.1.1 (2003-08-26)"
#define MOD_CAP     "compare with other image to find a pattern"
#define MOD_AUTHOR  "Antonio Beamud"

#define DELTA_COLOR 45.0
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>

// FIXME: Try to implement the YUV colorspace

/* -------------------------------------------------
 *
 * mandatory include files
 *
 *-------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>


#include "transcode.h"
#include "framebuffer.h"
#include "optstr.h"
#include <magick/api.h>

typedef struct pixelsMask {
	unsigned int row;
	unsigned int col;
	unsigned char r,g,b;
	unsigned char delta_r,delta_g,delta_b;
	
	struct pixelsMask *next;

}pixelsMask;

typedef struct compareData {
	
	FILE *results;
	
	float delta;
	int step;

	pixelsMask *pixel_mask;

	vob_t *vob;
	
	unsigned int frames;
	
	int width, height;
	int size;
	
} compareData;

static compareData *compare = NULL;
extern int rgbswap;

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

static void help_optstr(void) 
{
	printf ("[%s] (%s) help\n", MOD_NAME, MOD_CAP);
	printf ("* Overview\n");
	printf ("    Generate a file in with information about the times, \n");
	printf ("    frame, etc the pattern defined in the image \n"); 
	printf ("    parameter is observed.\n");
	printf ("* Options\n");
	printf ("    'pattern' path to the file used like pattern\n");
	printf ("    'results' path to the file used to write the results\n");
	printf ("    'delta' delta error allowed\n");
	
}


int tc_filter(vframe_list_t *ptr, char *options)
{
	int w, h;
	//PixelPacket *pixpat;
	char *pattern_name;
	char *results_name;
	
	Image *pattern, *resized, *orig;
	ImageInfo *image_info;
	
	PixelPacket *pixel_packet;
	pixelsMask *pixel_last;
	ExceptionInfo exception_info;
	
	if(ptr->tag & TC_FILTER_GET_CONFIG) {
		char buf[128];
		optstr_filter_desc(options, MOD_NAME, MOD_CAP, MOD_VERSION,
				   MOD_AUTHOR, "VRYO", "1");
		
		snprintf(buf, 128, "/dev/null");
		optstr_param(options, "pattern", "Pattern image file path", "%s", buf);
		snprintf(buf, 128, "results.dat");
		optstr_param(options, "results", "Results file path" , "%s", buf);
		snprintf(buf, 128, "%f",compare->delta);
		optstr_param(options, "delta", "Delta error", "%f",buf,"0.0","100.0");
		return 0;
	}
  
	//----------------------------------
	//
	// filter init
	//
	//----------------------------------


	if(ptr->tag & TC_FILTER_INIT) 
	{

		unsigned int t,r,index;
		pixelsMask *temp;

		if((compare = (compareData *)malloc(sizeof(compareData))) == NULL)
			return (-1);
		
		if((compare->vob = tc_get_vob())==NULL) return(-1);
		

		compare->delta=DELTA_COLOR;
		compare->step=1;
		compare->width=0;
		compare->height=0;
		compare->frames = 0;
		compare->pixel_mask = NULL;
		pixel_last = NULL;
		
		compare->width = compare->vob->ex_v_width;
		compare->height = compare->vob->ex_v_height;

		if (options != NULL) {
			char pattern_name[PATH_MAX];
			char results_name[PATH_MAX];
			memset(pattern_name,0,PATH_MAX);
			memset(results_name,0,PATH_MAX);

			if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);
	
			optstr_get(options, "pattern", "%[^:]", &pattern_name);
			optstr_get(options, "results", "%[^:]", &results_name);
			optstr_get(options, "delta", "%f", &compare->delta);
			
			if (verbose > 1) {
			printf("Compare Image Settings:\n");
			printf("      pattern = %s\n", pattern_name);
			printf("      results = %s\n", results_name);
			printf("        delta = %f\n", compare->delta);
			}		
			
			if (strlen(results_name) == 0) {
				// Ponemos el nombre del fichero al original con extension dat
				strcpy(results_name,"/tmp/compare.dat");
				
			}
			if (!(compare->results = fopen(results_name, "w")))
			{
				perror("could not open file for writing");
			}
			
			InitializeMagick("");
			if (verbose > 1) printf("[%s] Magick Initialized successfully\n", MOD_NAME);
				
			GetExceptionInfo(&exception_info);
			image_info = CloneImageInfo ((ImageInfo *) NULL);
			strcpy(image_info->filename,pattern_name);
			if (verbose > 1)
			     printf("Trying to open image\n");
			orig = ReadImage(image_info,
					 &exception_info);
			
			if (orig == (Image *) NULL) {
				MagickWarning(exception_info.severity,
					      exception_info.reason,
					      exception_info.description);
				strcpy(pattern_name, "/dev/null");
			}else{
			       if (verbose > 1)
			       		printf("[%s] Image loaded successfully\n", MOD_NAME);
			     }
		}
		
		else{
			perror("Not image provided");
		}
 		
		if (options != NULL)
			if (optstr_lookup (options, "help")) {
				help_optstr();
			}
		
		
		fprintf(compare->results,"#fps:%f\n",compare->vob->fps);
		
		if (orig != NULL){
                        // Flip and resize
			if (compare->vob->im_v_codec == CODEC_YUV)
				TransformRGBImage(orig,YCbCrColorspace);
			if (verbose > 1) printf("[%s] Resizing the Image\n", MOD_NAME);
			resized = ResizeImage(orig,
					      compare->width,
					      compare->height,
					      GaussianFilter,
					      1,
					      &exception_info);
			if (verbose > 1)
				printf("[%s] Flipping the Image\n", MOD_NAME);
			pattern = FlipImage(resized, &exception_info);
			if (pattern == (Image *) NULL) {
				MagickError (exception_info.severity,
					     exception_info.reason,
					     exception_info.description);
			}			
		
			// Filling the matrix with the pixels values not
			// alpha

			if (verbose > 1) printf("[%s] GetImagePixels\n", MOD_NAME);
			pixel_packet = GetImagePixels(pattern,0,0,
						      pattern->columns,
						      pattern->rows);

			if (verbose > 1) printf("[%s] Filling the Image matrix\n", MOD_NAME);
			for (t = 0; t < pattern->rows; t++) 
				for (r = 0; r < pattern->columns; r++){
					index = t*pattern->columns + r;
					if (pixel_packet[index].opacity == 0){
						temp=(pixelsMask *)malloc(sizeof(struct pixelsMask));
						temp->row=t;
						temp->col=r;
						temp->r = (unsigned char)pixel_packet[index].red;
						temp->g = (unsigned char)pixel_packet[index].green;
						temp->b = (unsigned char)pixel_packet[index].blue;
						temp->next=NULL;
						
						if (pixel_last == NULL){
							pixel_last = temp;
							compare->pixel_mask = temp;
						}else{
							pixel_last->next = temp;
							pixel_last = temp;
						}
					}
				}
			
			if (verbose) printf("[%s] %s %s\n",
					    MOD_NAME,
					    MOD_VERSION,
					    MOD_CAP);
		}
		return(0);
	}
	
	
	//----------------------------------
	//
	// filter close
	//
	//----------------------------------
	
	
	if(ptr->tag & TC_FILTER_CLOSE) {
		
		if (compare != NULL) {
			fclose(compare->results);
			free(compare);
		}
		DestroyMagick();
		compare=NULL;
		
		return(0);

	} /* filter close */
	
	//----------------------------------
	//
	// filter frame routine
	//
	//----------------------------------
	
	
	// tag variable indicates, if we are called before
	// transcodes internal video/audio frame processing routines
	// or after and determines video/audio context
	
	if((ptr->tag & TC_POST_PROCESS) && (ptr->tag & TC_VIDEO))  {
		// For now I only support RGB color space
		pixelsMask *item = NULL;
		double sr,sg,sb;
		double avg_dr,avg_dg,avg_db;
					
		if (compare->vob->im_v_codec == CODEC_RGB){
			
			int index,r,g,b,c,col;
			double width_long;
			
			if (compare->pixel_mask != NULL)
			{
				item = compare->pixel_mask;
				c = 0;
				
				sr = 0.0;
				sg = 0.0;
				sb = 0.0;
				
				width_long = compare->width*3;
				while(item){
					r = item->row*width_long + item->col*3;
					g = item->row*width_long 
						+ item->col*3 + 1;
					b = item->row*width_long
						+ item->col*3 + 2;
				
				// diff between points
				// Interchange RGB values if necesary
					sr = sr + (double)abs((unsigned char)ptr->video_buf[r] - item->r);
					sg = sg + (double)abs((unsigned char)ptr->video_buf[g] - item->g);
					sb = sb + (double)abs((unsigned char)ptr->video_buf[b] - item->b);
					item = item->next;
					c++;
				}
			
				avg_dr = sr/(double)c;
				avg_dg = sg/(double)c;
				avg_db = sb/(double)c;
			
				if ((avg_dr < compare->delta) && (avg_dg < compare->delta) && (avg_db < compare->delta))
					fprintf(compare->results,"1");
				else
					fprintf(compare->results,"n");
				fflush(compare->results);
			}
			compare->frames++;
			return(0);
		}else{
			
                        // The colospace is YUV
			
                        // FIXME: Doesn't works, I need to code all this part
			// again
  			
			int index,Y,Cr,Cb,c,col;
						
			if (compare->pixel_mask != NULL)
			{
				item = compare->pixel_mask;
				c = 0;
				
				sr = 0.0;
				sg = 0.0;
				sb = 0.0;
				
				while(item){
					Y  = item->row*compare->width + item->col;
					Cb = compare->height*compare->width
						+ (int)((item->row*compare->width + item->col)/4);
					Cr = compare->height*compare->width
						+ (int)((compare->height*compare->width)/4)
						+ (int)((item->row*compare->width + item->col)/4);

				        // diff between points
				        // Interchange RGB values if necesary

					sr = sr + (double)abs((unsigned char)ptr->video_buf[Y] - item->r);
					sg = sg + (double)abs((unsigned char)ptr->video_buf[Cb] - item->g);
					sb = sb + (double)abs((unsigned char)ptr->video_buf[Cr] - item->b);
					item = item->next;
					c++;
				}
			
				avg_dr = sr/(double)c;
				avg_dg = sg/(double)c;
				avg_db = sb/(double)c;
			
				if ((avg_dr < compare->delta) && (avg_dg < compare->delta) && (avg_db < compare->delta))
					fprintf(compare->results,"1");
				else
					fprintf(compare->results,"n");
			}
			compare->frames++;
			return(0);
			
		}
	}
}
// Proposal:
// Tilmann Bitterberg Sat, 14 Jun 2003 00:29:06 +0200 (CEST)
// 
//    char *Y, *Cb, *Cr;
//    Y  = p->video_buf;
//    Cb = p->video_buf + mydata->width*mydata->height;
//    Cr = p->video_buf + 5*mydata->width*mydata->height/4;

//    for (i=0; i<mydata->width*mydata->height; i++) {
//      pixel_packet->red == *Y++;
//      get_next_pixel();
//    }

//    for (i=0; i<mydata->width*mydata->height>>2; i++) {
//      pixel_packet->green == *Cr++;
//      pixel_packet->blue == *Cb++;
//      get_next_pixel();
//    }






