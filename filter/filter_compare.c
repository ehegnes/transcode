/*
 *  filter_compare
 *
 *  Copyright (C) Antonio Beamud Montero <antonio.beamud@linkend.com>
 *  Copyright (C) Microgenesis S.A.
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

#define MOD_NAME    "filter_compare.so"
#define MOD_VERSION "v0.1.3 (2006-01-24)"
#define MOD_CAP     "compare with other image to find a pattern"
#define MOD_AUTHOR  "Antonio Beamud"

#include "transcode.h"
#include "filter.h"
#include "optstr.h"

#include <math.h>
#include <inttypes.h>

#define DELTA_COLOR 45.0

// FIXME: Try to implement the YUV colorspace

// transcode defines this as well as ImageMagick.
#undef PACKAGE_NAME
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_STRING

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

static compareData *compare[MAX_FILTER];
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

#define RESULTS_FILE    "/tmp/compare.dat"
#define PATTERN_FILE    "/dev/null"

int tc_filter(vframe_list_t *ptr, char *options)
{
	int instance = ptr->filter_id;
	
	Image *pattern, *resized, *orig = 0;
	ImageInfo *image_info;
	
	PixelPacket *pixel_packet;
	pixelsMask *pixel_last;
	ExceptionInfo exception_info;
	
	if(ptr->tag & TC_FILTER_GET_CONFIG) {
		char buf[128];
		optstr_filter_desc(options, MOD_NAME, MOD_CAP, MOD_VERSION,
				   MOD_AUTHOR, "VRYMO", "1");
		
-		snprintf(buf, 128, PATTERN_FILE);
		optstr_param(options, "pattern", "Pattern image file path", "%s", buf);
		snprintf(buf, 128, RESULTS_FILE);
		optstr_param(options, "results", "Results file path" , "%s", buf);
		snprintf(buf, 128, "%f", compare[instance]->delta);
		optstr_param(options, "delta", "Delta error", "%f",buf,"0.0", "100.0");
		return 0;
	}
  
	//----------------------------------
	//
	// filter init
	//
	//----------------------------------


	if(ptr->tag & TC_FILTER_INIT) 
	{
		char pattern_name[PATH_MAX];
		char results_name[PATH_MAX];
		unsigned int t,r,index;
		pixelsMask *temp;

		/* defaults */
		strlcpy(results_name, RESULTS_FILE, sizeof(results_name));
		strlcpy(pattern_name, PATTERN_FILE, sizeof(pattern_name));

		if((compare[instance] = malloc(sizeof(compareData))) == NULL)
			return (-1);
		
		if((compare[instance]->vob = tc_get_vob())==NULL) return(-1);
		
		compare[instance]->delta=DELTA_COLOR;
		compare[instance]->step=1;
		compare[instance]->width=0;
		compare[instance]->height=0;
		compare[instance]->frames = 0;
		compare[instance]->pixel_mask = NULL;
		pixel_last = NULL;
		
		compare[instance]->width = compare[instance]->vob->ex_v_width;
		compare[instance]->height = compare[instance]->vob->ex_v_height;

		if (options != NULL) {
			if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);
	
			if (optstr_lookup (options, "help")) {
				help_optstr();
				return(0);
			}

			optstr_get(options, "pattern", "%[^:]", &pattern_name);
			optstr_get(options, "results", "%[^:]", &results_name);
			optstr_get(options, "delta", "%f", &compare[instance]->delta);
			
			if (verbose > 1) {
			printf("Compare Image Settings:\n");
			printf("      pattern = %s\n", pattern_name);
			printf("      results = %s\n", results_name);
			printf("        delta = %f\n", compare[instance]->delta);
			}		
			
			if (!pattern_name || !strlen(pattern_name)) {
				/* restore default */
				fprintf(stderr, "[%s] missing image file, using fake "
                               		"source\n", MOD_NAME);
				strlcpy(pattern_name, PATTERN_FILE, sizeof(pattern_name));
			}
			if (!results_name || !strlen(results_name)) {
				/* restore default */
				fprintf(stderr, "[%s] using default results file '%s'\n",
                               		MOD_NAME, RESULTS_FILE);
				strlcpy(results_name, RESULTS_FILE, sizeof(results_name));
			}
		}
            
		if (!(compare[instance]->results = fopen(results_name, "w"))) {
				perror("could not open file for writing");
			return(-1);
			}
			
			InitializeMagick("");
			if (verbose > 1) printf("[%s] Magick Initialized successfully\n", MOD_NAME);
				
			GetExceptionInfo(&exception_info);
			image_info = CloneImageInfo(NULL);
			strlcpy(image_info->filename, pattern_name, MaxTextExtent);
			if (verbose > 1)
			     printf("Trying to open image\n");
		        orig = ReadImage(image_info, &exception_info);
			
		        if (!orig) {
				MagickWarning(exception_info.severity,
					      exception_info.reason,
					      exception_info.description);
				fprintf(stderr, "[%s] Can't open image\n", MOD_NAME);
				return(-1);
			} else {
			       if (verbose > 1)
			       		printf("[%s] Image loaded successfully\n", MOD_NAME);
			     }
		
		fprintf(compare[instance]->results,"#fps:%f\n",compare[instance]->vob->fps);
		
		if (orig != NULL){
                        // Flip and resize
			if (compare[instance]->vob->im_v_codec == CODEC_YUV)
				TransformRGBImage(orig,YCbCrColorspace);
			if (verbose > 1) printf("[%s] Resizing the Image\n", MOD_NAME);
			resized = ResizeImage(orig,
					      compare[instance]->width,
					      compare[instance]->height,
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
							compare[instance]->pixel_mask = temp;
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
		
		if (compare[instance] != NULL) {
			fclose(compare[instance]->results);
			free(compare[instance]);
		}
		DestroyMagick();
		compare[instance]=NULL;
		
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
					
		if (compare[instance]->vob->im_v_codec == CODEC_RGB){
			
			int r,g,b,c;
			double width_long;
			
			if (compare[instance]->pixel_mask != NULL)
			{
				item = compare[instance]->pixel_mask;
				c = 0;
				
				sr = 0.0;
				sg = 0.0;
				sb = 0.0;
				
				width_long = compare[instance]->width*3;
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
			
				if ((avg_dr < compare[instance]->delta) && (avg_dg < compare[instance]->delta) && (avg_db < compare[instance]->delta))
					fprintf(compare[instance]->results,"1");
				else
					fprintf(compare[instance]->results,"n");
				fflush(compare[instance]->results);
			}
			compare[instance]->frames++;
			return(0);
		}else{
			
                        // The colospace is YUV
			
                        // FIXME: Doesn't works, I need to code all this part
			// again
  			
			int Y,Cr,Cb,c;
						
			if (compare[instance]->pixel_mask != NULL)
			{
				item = compare[instance]->pixel_mask;
				c = 0;
				
				sr = 0.0;
				sg = 0.0;
				sb = 0.0;
				
				while(item){
					Y  = item->row*compare[instance]->width + item->col;
					Cb = compare[instance]->height*compare[instance]->width
						+ (int)((item->row*compare[instance]->width + item->col)/4);
					Cr = compare[instance]->height*compare[instance]->width
						+ (int)((compare[instance]->height*compare[instance]->width)/4)
						+ (int)((item->row*compare[instance]->width + item->col)/4);

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
			
				if ((avg_dr < compare[instance]->delta) && (avg_dg < compare[instance]->delta) && (avg_db < compare[instance]->delta))
					fprintf(compare[instance]->results,"1");
				else
					fprintf(compare[instance]->results,"n");
			}
			compare[instance]->frames++;
			return(0);
			
		}
	}

	return(0);
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






