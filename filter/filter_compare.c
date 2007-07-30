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

/* Note: because of ImageMagick bogosity, this must be included first, so
 * we can undefine the PACKAGE_* symbols it splats into our namespace */
#include <magick/api.h>
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#include "transcode.h"
#include "filter.h"
#include "libtc/libtc.h"
#include "libtc/optstr.h"

#define MOD_NAME    "filter_compare.so"
#define MOD_VERSION "v0.2.0 (2007-07-29)"
#define MOD_CAP     "compare with other image to find a pattern"
#define MOD_AUTHOR  "Antonio Beamud"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>

#define DELTA_COLOR 45.0


typedef struct PixelsMask {
	uint32_t row;
	uint32_t col;
	uint8_t r, g, b;
	uint8_t delta_r, delta_g, delta_b;

	struct PixelsMask *next;
} PixelsMask;

typedef struct ComparePrivateData {
	FILE *results;

	float delta;
	int step;

	PixelsMask *pixel_mask;

	vob_t *vob;

	uint32_t frames;

	int width, height;
	int size;
} ComparePrivateData;

/* FIXME: this uses the filter ID as an index--the ID can grow
 * arbitrarily large, so this needs to be fixed */
static ComparePrivateData *compare[100];

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

static void help_optstr(void)
{
    tc_log_info(MOD_NAME, "(%s) help"
"* Overview\n"
"    Generate a file in with information about the times, \n"
"    frame, etc the pattern defined in the image \n"
"    parameter is observed.\n"
"* Options\n"
"    'pattern' path to the file used like pattern\n"
"    'results' path to the file used to write the results\n"
"    'delta' delta error allowed\n"
		, MOD_CAP);
}


int tc_filter(frame_list_t *ptr_, char *options)
{
	vframe_list_t *ptr = (vframe_list_t *)ptr_;
	int instance = ptr->filter_id;
	Image *pattern, *resized, *orig = 0;
	ImageInfo *image_info;

	PixelPacket *pixel_packet;
	PixelsMask *pixel_last;
	ExceptionInfo exception_info;

	if(ptr->tag & TC_FILTER_GET_CONFIG) {
		char buf[128];
		optstr_filter_desc(options, MOD_NAME, MOD_CAP, MOD_VERSION,
				   MOD_AUTHOR, "VRYMO", "1");

		tc_snprintf(buf, 128, "/dev/compare");
		optstr_param(options, "pattern", "Pattern image file path", "%s", buf);
		tc_snprintf(buf, 128, "results.dat");
		optstr_param(options, "results", "Results file path" , "%s", buf);
		tc_snprintf(buf, 128, "%f", pd->delta);
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

		unsigned int t,r,index;
		PixelsMask *temp;

		pd = tc_malloc(sizeof(ComparePrivateData));
		if(pd == NULL)
			return (-1);

		pd->vob = tc_get_vob();
		if(pd->vob ==NULL)
            return(-1);

		pd->delta=DELTA_COLOR;
		pd->step=1;
		pd->width=0;
		pd->height=0;
		pd->frames = 0;
		pd->pixel_mask = NULL;
		pixel_last = NULL;

		pd->width = pd->vob->ex_v_width;
		pd->height = pd->vob->ex_v_height;

		if (options != NULL) {
			char pattern_name[PATH_MAX];
			char results_name[PATH_MAX];
			memset(pattern_name,0,PATH_MAX);
			memset(results_name,0,PATH_MAX);

			if(verbose) tc_log_info(MOD_NAME, "options=%s", options);

			optstr_get(options, "pattern", "%[^:]", pattern_name);
			optstr_get(options, "results", "%[^:]", results_name);
			optstr_get(options, "delta", "%f", &pd->delta);

			if (verbose > 1) {
				tc_log_info(MOD_NAME, "Compare Image Settings:");
				tc_log_info(MOD_NAME, "      pattern = %s\n", pattern_name);
				tc_log_info(MOD_NAME, "      results = %s\n", results_name);
				tc_log_info(MOD_NAME, "        delta = %f\n", pd->delta);
			}

			if (strlen(results_name) == 0) {
				// Ponemos el nombre del fichero al original con extension dat
				strlcpy(results_name, "/tmp/compare.dat", sizeof(results_name));

			}
			if (!(pd->results = fopen(results_name, "w")))
			{
				tc_log_perror(MOD_NAME, "could not open file for writing");
			}

			InitializeMagick("");
			if (verbose > 1)
                tc_log_info(MOD_NAME, "Magick Initialized successfully");

			GetExceptionInfo(&exception_info);
			image_info = CloneImageInfo ((ImageInfo *) NULL);
			strlcpy(image_info->filename, pattern_name, MaxTextExtent);
			if (verbose > 1)
			     tc_log_info(MOD_NAME, "Trying to open image");
			orig = ReadImage(image_info,
					 &exception_info);

			if (orig == (Image *) NULL) {
				MagickWarning(exception_info.severity,
					      exception_info.reason,
					      exception_info.description);
				strlcpy(pattern_name, "/dev/compare", sizeof(pattern_name));
			}else{
			       if (verbose > 1)
			       		tc_log_info(MOD_NAME, "Image loaded successfully");
			     }
		}

		else{
			tc_log_perror(MOD_NAME, "Not image provided");
		}

		if (options != NULL)
			if (optstr_lookup (options, "help")) {
				help_optstr();
			}


		fprintf(pd->results,"#fps:%f\n",pd->vob->fps);

		if (orig != NULL){
                        // Flip and resize
			if (pd->vob->im_v_codec == CODEC_YUV)
				TransformRGBImage(orig,YCbCrColorspace);
			if (verbose > 1) tc_log_info(MOD_NAME, "Resizing the Image");
			resized = ResizeImage(orig,
					      pd->width,
					      pd->height,
					      GaussianFilter,
					      1,
					      &exception_info);
			if (verbose > 1)
				tc_log_info(MOD_NAME, "Flipping the Image");
			pattern = FlipImage(resized, &exception_info);
			if (pattern == (Image *) NULL) {
				MagickError (exception_info.severity,
					     exception_info.reason,
					     exception_info.description);
			}

			// Filling the matrix with the pixels values not
			// alpha

			if (verbose > 1) tc_log_info(MOD_NAME, "GetImagePixels");
			pixel_packet = GetImagePixels(pattern,0,0,
						      pattern->columns,
						      pattern->rows);

			if (verbose > 1) tc_log_info(MOD_NAME, "Filling the Image matrix");
			for (t = 0; t < pattern->rows; t++)
				for (r = 0; r < pattern->columns; r++){
					index = t*pattern->columns + r;
					if (pixel_packet[index].opacity == 0){
						temp=tc_malloc(sizeof(struct PixelsMask));
						temp->row=t;
						temp->col=r;
						temp->r = (uint8_t)ScaleQuantumToChar(pixel_packet[index].red);
						temp->g = (uint8_t)ScaleQuantumToChar(pixel_packet[index].green);
						temp->b = (uint8_t)ScaleQuantumToChar(pixel_packet[index].blue);
						temp->next=NULL;

						if (pixel_last == NULL){
							pixel_last = temp;
							pd->pixel_mask = temp;
						}else{
							pixel_last->next = temp;
							pixel_last = temp;
						}
					}
				}

			if (verbose)
                tc_log_info(MOD_NAME, "%s %s",
					    MOD_VERSION, MOD_CAP);
		}
		return(0);
	}


	//----------------------------------
	//
	// filter close
	//
	//----------------------------------


	if(ptr->tag & TC_FILTER_CLOSE) {

		if (pd != NULL) {
			fclose(pd->results);
			free(pd);
		}
		DestroyMagick();
		pd=NULL;

		return(0);

	} /* filter close */

	//----------------------------------
	//
	// filter frame routine
	//
	//----------------------------------


	if((ptr->tag & TC_POST_M_PROCESS) && (ptr->tag & TC_VIDEO))  {
		// For now I only support RGB color space
		PixelsMask *item = NULL;
		double sr,sg,sb;
		double avg_dr,avg_dg,avg_db;

		if (pd->vob->im_v_codec == CODEC_RGB){

			int r,g,b,c;
			double width_long;

			if (pd->pixel_mask != NULL)
			{
				item = pd->pixel_mask;
				c = 0;

				sr = 0.0;
				sg = 0.0;
				sb = 0.0;

				width_long = pd->width*3;
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

				if ((avg_dr < pd->delta) && (avg_dg < pd->delta) && (avg_db < pd->delta))
					fprintf(pd->results,"1");
				else
					fprintf(pd->results,"n");
				fflush(pd->results);
			}
			pd->frames++;
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

/*************************************************************************/

static const TCCodecID compare_codecs_in[] = { 
    TC_CODEC_RGB, TC_CODEC_ERROR
};
static const TCCodecID compare_codecs_out[] = {
    TC_CODEC_RGB, TC_CODEC_ERROR
};
static const TCFormatID compare_formats[] = {
    TC_FORMAT_ERROR
};

static const TCModuleInfo compare_info = {
    .features    = MOD_FEATURES,
    .flags       = MOD_FLAGS,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = compare_codecs_in,
    .codecs_out  = compare_codecs_out,
    .formats_in  = compare_formats,
    .formats_out = compare_formats
};

static const TCModuleClass compare_class = {
    .info         = &compare_info,

    .init         = compare_init,
    .fini         = compare_fini,
    .configure    = compare_configure,
    .stop         = compare_stop,
    .inspect      = compare_inspect,

    .filter_video = compare_filter_video,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &compare_class;
}

/*************************************************************************/

static int compare_get_config(TCModuleInstance *self, char *options)
{
    ComparePrivateData *pd = NULL;

    TC_MODULE_SELF_CHECK(self, "get_config");

    pd = self->userdata;

    /*
     * Valid flags for the string of filter capabilities:
     *  "V" :  Can do Video
     *  "A" :  Can do Audio
     *  "R" :  Can do RGB
     *  "Y" :  Can do YUV
     *  "4" :  Can do YUV422
     *  "M" :  Can do Multiple Instances
     *  "E" :  Is a PRE filter
     *  "O" :  Is a POST filter
     */
    optstr_filter_desc(options, MOD_NAME, MOD_CAP, MOD_VERSION,
                       MOD_AUTHOR, "VAMEO", "1");

    /* can be omitted */
    optstr_param(options, "help", "Prints out a short help", "", "0");
 
    /* use optstr_param to do introspection */

    return TC_OK;
}

static int compare_process(TCModuleInstance *self, frame_list_t *frame)
{
    TC_MODULE_SELF_CHECK(self, "process");

    /* choose what to do by frame->tag */
	if((ptr->tag & TC_POST_M_PROCESS) && (ptr->tag & TC_VIDEO))  {
        return compare_filter_video(self, (vframe_list_t*)frame);
    }
    return TC_OK;
}

/*************************************************************************/

/* Old-fashioned module interface. */

TC_FILTER_OLDINTERFACE(compare)

/*************************************************************************/

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
