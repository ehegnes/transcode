/*
   filter_msharpen.c

   Copyright (C) 1999-2000 Donal A. Graft
     modified 2003 by William Hawkins for use with transcode

    MSharpen Filter for VirtualDub -- performs sharpening
	limited to edge areas of the frame.
	Copyright (C) 1999-2000 Donald A. Graft

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	The author can be contacted at:
	Donald Graft
	neuron2@home.com.
*/

#define MOD_NAME    "filter_msharpen.so"
#define MOD_VERSION "(1.0) (2003-07-17)"
#define MOD_CAP     "VirtualDub's MSharpen Filter"
#define MOD_AUTHOR  "Donald Graft, William Hawkins"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/* -------------------------------------------------
 *
 * mandatory include files
 *
 *-------------------------------------------------*/

#include "transcode.h"
#include "framebuffer.h"
#include "optstr.h"
#include "export/vid_aux.h"

static vob_t *vob=NULL;

/* vdub compat */
typedef unsigned int	Pixel;
typedef unsigned int	Pixel32;
typedef unsigned char	Pixel8;
typedef int		PixCoord;
typedef	int		PixDim;
typedef	int		PixOffset;


#define R_MASK  (0x00ff0000)
#define G_MASK  (0x0000ff00)
#define B_MASK  (0x000000ff)
#define R_SHIFT         (16)
#define G_SHIFT          (8)
#define B_SHIFT          (0)

/* convert transcode RGB (3*8 Bit) to vdub ARGB (32Bit) */
void convert_rgb2argb (char * in, Pixel32 *out, int width, int height) 
{
	int run;
	int size = width*height;

	for (run = 0; run < size; run++) {
		*out = (((((Pixel32) *(in+0)) & 0xff) << R_SHIFT) | 
		        ((((Pixel32) *(in+1)) & 0xff) << G_SHIFT)  | 
			((((Pixel32) *(in+2)) & 0xff)))      & 0x00ffffff;

		out++;
		in += 3;
	}
}

/* convert vdub ARGB (32Bit) to transcode RGB (3*8 Bit) */
void convert_argb2rgb (Pixel32 *in, char * out, int width, int height)
{
	int run;
	int size = width*height;

	for (run = 0; run < size; run++) {

		*(out+0) = ((*in & R_MASK) >> R_SHIFT);
		*(out+1) = ((*in & G_MASK) >> G_SHIFT);
		*(out+2) = (*in) & B_MASK;

		in++;
		out += 3;
	}

}



///////////////////////////////////////////////////////////////////////////

typedef struct MyFilterData {
        Pixel32                 *convertFrameIn;
        Pixel32                 *convertFrameOut;
	unsigned char		*blur;
	unsigned char		*work;
	int 		       	strength;
	int	      		threshold;
	int   			mask;
        int                     highq;
} MyFilterData;

static MyFilterData *mfd;

static void help_optstr(void)
{
  printf("[%s] (%s) help\n", MOD_NAME, MOD_CAP);
   printf ("* Overview\n");
   printf ("    This plugin implements an unusual concept in spatial sharpening.\n");
   printf ("    Although designed specifically for anime, it also works well with\n");
   printf ("    normal video. The filter is very effective at sharpening important\n");
   printf ("    edges without amplifying noise.\n");

   printf ("* Options\n");
   printf ("  * Strength 'strength' (0-255) [100]\n");
   printf ("    This is the strength of the sharpening to be applied to the edge\n"); 
   printf ("    detail areas. It is applied only to the edge detail areas as\n");
   printf ("    determined by the 'threshold' parameter. Strength 255 is the\n");
   printf ("    strongest sharpening.\n");

   printf ("  * Threshold 'threshold' (0-255) [10]\n");
   printf ("    This parameter determines what is detected as edge detail and\n");
   printf ("    thus sharpened. To see what edge detail areas will be sharpened,\n");
   printf ("    use the 'mask' parameter.\n");

   printf ("  * Mask 'mask' (0-1) [0]\n");
   printf ("    When set to true, the areas to be sharpened are shown in white\n");
   printf ("    against a black background. Use this to set the level of detail to\n");
   printf ("    be sharpened. This function also makes a basic edge detection filter.\n");

   printf ("  * HighQ 'highq' (0-1) [1]\n");
   printf ("    This parameter lets you tradeoff speed for quality of detail\n");
   printf ("    detection. Set it to true for the best detail detection. Set it to\n");
   printf ("    false for maximum speed.\n");
}

int tc_filter(vframe_list_t *ptr, char *options)
{

  //----------------------------------
  //
  // filter init
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_INIT) {

	int width, height;
    
	if((vob = tc_get_vob())==NULL) return(-1);
    
	mfd = (MyFilterData *) malloc(sizeof(MyFilterData));

	if (!mfd) {
		fprintf(stderr, "[%s] No memory at %d!\n", MOD_NAME, __LINE__); return (-1);
	}

	height = vob->ex_v_height;
	width  = vob->ex_v_width;

	/* default values */
	mfd->strength       = 100; /* A little bird told me this was a good value */
	mfd->threshold      = 10;
	mfd->mask           = TC_FALSE; /* not sure what this does at the moment */
	mfd->highq          = TC_TRUE; /* high Q or not? */

	if (options != NULL) {
    
	  if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);

	  optstr_get (options, "strength",  "%d", &mfd->strength);
	  optstr_get (options, "threshold", "%d", &mfd->threshold);
	  optstr_get (options, "highq", "%d", &mfd->highq);
	  optstr_get (options, "mask", "%d", &mfd->mask);
	
	}

	if (verbose > 1) {

	  printf (" MSharpen Filter Settings (%dx%d):\n", width,height);
	  printf ("          strength = %d\n", mfd->strength);
	  printf ("         threshold = %d\n", mfd->threshold);
	  printf ("             highq = %d\n", mfd->highq);
	  printf ("              mask = %d\n", mfd->mask);
	}

	if (options)
		if ( optstr_get(options, "help", "") >= 0) {
			help_optstr();
		}
	
	/* fetch memory */

	mfd->blur = (unsigned char *) malloc(4 * width * height);
	if (!mfd->blur){
                fprintf(stderr, "[%s] No memory at %d!\n", MOD_NAME, __LINE__); return (-1);
	}
	mfd->work = (unsigned char *) malloc(4 * width * height);
	if (!mfd->work){
                fprintf(stderr, "[%s] No memory at %d!\n", MOD_NAME, __LINE__); return (-1);
	}
	mfd->convertFrameIn = (Pixel32 *) malloc (width*height*sizeof(Pixel32));
	if (!mfd->convertFrameIn) {
		fprintf(stderr, "[%s] No memory at %d!\n", MOD_NAME, __LINE__); return (-1);
	}
	memset(mfd->convertFrameIn, 0, width*height*sizeof(Pixel32));

	mfd->convertFrameOut = (Pixel32 *) malloc (width*height*sizeof(Pixel32));
	if (!mfd->convertFrameOut) {
		fprintf(stderr, "[%s] No memory at %d!\n", MOD_NAME, __LINE__); return (-1);
	}
	memset(mfd->convertFrameOut, 0, width*height*sizeof(Pixel32));

	if (vob->im_v_codec == CODEC_YUV) {
	  tc_rgb2yuv_init(width, height);
	  tc_yuv2rgb_init(width, height);
	}

	// filter init ok.
	if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);

	return 0;

  } /* TC_FILTER_INIT */

  if(ptr->tag & TC_FILTER_GET_CONFIG) {
    if (options) {
	    char buf[256];
	    optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRYO", "1");
	    snprintf (buf, sizeof(buf), "%d", mfd->strength);
	    optstr_param (options, "strength", "How much  of the effect", "%d", buf, "0", "255");

	    snprintf (buf, sizeof(buf), "%d", mfd->threshold);
	    optstr_param (options, "threshold", 
			  "How close a pixel must be to the brightest or dimmest pixel to be mapped", 
			  "%d", buf, "0", "255");
	    snprintf (buf, sizeof(buf), "%d", mfd->highq);
	    optstr_param (options, "highq",  "Tradeoff speed for quality of detail detection",
		          "%d", buf, "0", "1");
	    snprintf (buf, sizeof(buf), "%d", mfd->mask);
	    optstr_param (options, "mask",  "Areas to be sharpened are shown in white",
		          "%d", buf, "0", "1");

    }
  }


  if(ptr->tag & TC_FILTER_CLOSE) {

	if (mfd->convertFrameIn)
		free (mfd->convertFrameIn);
	mfd->convertFrameIn = NULL;

	if (mfd->convertFrameOut) 
		free (mfd->convertFrameOut);
	mfd->convertFrameOut = NULL;
	
	if (mfd->blur)
                free ( mfd->blur);
	mfd->blur = NULL;
	if (mfd->work)
                free(mfd->work);
	mfd->work = NULL;

	if (mfd)
		free(mfd);
	mfd = NULL;

	if (vob->im_v_codec == CODEC_YUV) {
	  tc_rgb2yuv_close();
	  tc_yuv2rgb_close();
	}

	return 0;

  } /* TC_FILTER_CLOSE */

///////////////////////////////////////////////////////////////////////////
  if(ptr->tag & TC_POST_PROCESS && ptr->tag & TC_VIDEO) {
    
    
	const PixDim	width  = ptr->v_width;
	const PixDim	height = ptr->v_height;
	const long	pitch = ptr->v_width*sizeof(Pixel32);
	int		bwidth = 4 * width;
	unsigned char   *src;
	unsigned char   *dst;
	unsigned char   *srcpp, *srcp, *srcpn, *workp, *blurp, *blurpn, *dstp;
	int r1, r2, r3, r4, g1, g2, g3, g4, b1, b2, b3, b4;
	int x, y, max;
	int strength = mfd->strength, invstrength = 255 - strength;
	int threshold = mfd->threshold;
	// const int	srcpitch = ptr->v_width*sizeof(Pixel32);
	const int	dstpitch = ptr->v_width*sizeof(Pixel32);

	if (vob->im_v_codec == CODEC_YUV) {
	  tc_yuv2rgb_core(ptr->video_buf);
	}

	convert_rgb2argb (ptr->video_buf, mfd->convertFrameIn, ptr->v_width, ptr->v_height);

	src = (unsigned char *)mfd->convertFrameIn;
	dst = (unsigned char *)mfd->convertFrameOut; 

	/* Blur the source image prior to detail detection. Separate
	   dimensions for speed. */
	/* Vertical. */
	srcpp = src;
	srcp = srcpp + pitch;
	srcpn = srcp + pitch;
	workp = mfd->work + bwidth;
	for (y = 1; y < height - 1; y++)
	{
		for (x = 0; x < bwidth; x++)
		{
			workp[x] = (srcpp[x] + srcp[x] + srcpn[x]) / 3;
		}
		srcpp += pitch;
		srcp += pitch;
		srcpn += pitch;
		workp += bwidth;
	}

	/* Horizontal. */
	workp  = mfd->work;
	blurp  = mfd->blur;
	for (y = 0; y < height; y++)
	{
		for (x = 4; x < bwidth - 4; x++)
		{
			blurp[x] = (workp[x-4] + workp[x] + workp[x+4]) / 3;
		}
		workp += bwidth;
		blurp += bwidth;
	}

	/* Fix up blur frame borders. */
	srcp = src;
	blurp = mfd->blur;
	tc_memcpy(blurp, srcp, bwidth);
	tc_memcpy(blurp + (height-1)*bwidth, srcp + (height-1)*pitch, bwidth);
	for (y = 0; y < height; y++)
	{
		*((unsigned int *)(&blurp[0])) = *((unsigned int *)(&srcp[0]));
		*((unsigned int *)(&blurp[bwidth-4])) = *((unsigned int *)(&srcp[bwidth-4]));
		srcp += pitch;
		blurp += bwidth;
	}

	/* Diagonal detail detection. */
	blurp = mfd->blur;
	blurpn = blurp + bwidth;
	workp = mfd->work;
	for (y = 0; y < height - 1; y++) 
	{
		b1 = blurp[0];
		g1 = blurp[1];
		r1 = blurp[2];
		b3 = blurpn[0];
		g3 = blurpn[1];
		r3 = blurpn[2];
		for (x = 0; x < bwidth - 4; x+=4)
		{
			b2 = blurp[x+4];
			g2 = blurp[x+5];
			r2 = blurp[x+6];
			b4 = blurpn[x+4];
			g4 = blurpn[x+5];
			r4 = blurpn[x+6];
			if ((abs(b1 - b4) >= threshold) || (abs(g1 - g4) >= threshold) || (abs(r1 - r4) >= threshold) ||
				(abs(b2 - b3) >= threshold) || (abs(g2 - g3) >= threshold) || (abs(g2 - g3) >= threshold))
			{
				*((unsigned int *)(&workp[x])) = 0xffffffff;
			}
			else
			{
				*((unsigned int *)(&workp[x])) = 0x0;
			}
			b1 = b2; b3 = b4;
			g1 = g2; g3 = g4;
			r1 = r2; r3 = r4;
		}
		workp += bwidth;
		blurp += bwidth;
		blurpn += bwidth;
	}

	if (mfd->highq == TC_TRUE)
//	if (1)
	{
		/* Vertical detail detection. */
		for (x = 0; x < bwidth; x+=4)
		{
 			blurp = mfd->blur;
			blurpn = blurp + bwidth;
			workp = mfd->work;
			b1 = blurp[x];
			g1 = blurp[x+1];
			r1 = blurp[x+2];
			for (y = 0; y < height - 1; y++)
			{
				b2 = blurpn[x];
				g2 = blurpn[x+1];
				r2 = blurpn[x+2];
				if (abs(b1 - b2) >= threshold || abs(g1 - g2) >= threshold || abs(r1 - r2) >= threshold)
				{
					*((unsigned int *)(&workp[x])) = 0xffffffff;
				}
				b1 = b2;
				g1 = g2;
				r1 = r2;
				workp += bwidth;
				blurp += bwidth;
				blurpn += bwidth;
			}
		}

		/* Horizontal detail detection. */
		blurp = mfd->blur;
		workp = mfd->work;
		for (y = 0; y < height; y++)
		{
			b1 = blurp[0];
			g1 = blurp[1];
			r1 = blurp[2];
			for (x = 0; x < bwidth - 4; x+=4)
			{
				b2 = blurp[x+4];
				g2 = blurp[x+5];
				r2 = blurp[x+6];
				if (abs(b1 - b2) >= threshold || abs(g1 - g2) >= threshold || abs(r1 - r2) >= threshold)
				{
					*((unsigned int *)(&workp[x])) = 0xffffffff;
				}
				b1 = b2;
				g1 = g2;
				r1 = r2;
			}
			workp += bwidth;
			blurp += bwidth;
		}
	}

	/* Fix up detail map borders. */
	memset(mfd->work + (height-1)*bwidth, 0, bwidth);
	workp = mfd->work;
	for (y = 0; y < height; y++)
	{
		*((unsigned int *)(&workp[bwidth-4])) = 0;
		workp += bwidth;
	}

	if (mfd->mask == TC_TRUE)
	{
		workp	= mfd->work;
		dstp	= dst;
		for (y = 0; y < height; y++)
		{
			for (x = 0; x < bwidth; x++)
			{
				dstp[x] = workp[x];
			}
			workp += bwidth;
			dstp = dstp + dstpitch;
		}
		return 0;
	}

	/* Fix up output frame borders. */
	srcp = src;
	dstp = dst;
	tc_memcpy(dstp, srcp, bwidth);
	tc_memcpy(dstp + (height-1)*pitch, srcp + (height-1)*pitch, bwidth);
	for (y = 0; y < height; y++)
	{
		*((unsigned int *)(&dstp[0])) = *((unsigned int *)(&srcp[0]));
		*((unsigned int *)(&dstp[bwidth-4])) = *((unsigned int *)(&srcp[bwidth-4]));
		srcp += pitch;
		dstp += pitch;
	}

	/* Now sharpen the edge areas and we're done! */
 	srcp = src + pitch;
 	dstp = dst + pitch;
	workp = mfd->work + bwidth;
	blurp = mfd->blur + bwidth;
	for (y = 1; y < height - 1; y++)
	{
		for (x = 4; x < bwidth - 4; x+=4)
		{
			int xplus1 = x + 1, xplus2 = x + 2;

			if (workp[x])
			{
				b4 = (4*(int)srcp[x] - 3*blurp[x]);
				g4 = (4*(int)srcp[x+1] - 3*blurp[x+1]);
				r4 = (4*(int)srcp[x+2] - 3*blurp[x+2]);

				if (b4 < 0) b4 = 0;
				if (g4 < 0) g4 = 0;
				if (r4 < 0) r4 = 0;
				max = b4;
				if (g4 > max) max = g4;
				if (r4 > max) max = r4;
				if (max > 255)
				{
					b4 = (b4 * 255) / max;
					g4 = (g4 * 255) / max;
					r4 = (r4 * 255) / max;
				}
				dstp[x]      = (strength * b4 + invstrength * srcp[x])      >> 8;
				dstp[xplus1] = (strength * g4 + invstrength * srcp[xplus1]) >> 8;
				dstp[xplus2] = (strength * r4 + invstrength * srcp[xplus2]) >> 8;
			}
			else
			{
				dstp[x]   = srcp[x];
				dstp[xplus1] = srcp[xplus1];
				dstp[xplus2] = srcp[xplus2];
			}
		}
		srcp += pitch;
		dstp += pitch;
		workp += bwidth;
		blurp += bwidth;
	}

	convert_argb2rgb (mfd->convertFrameOut, ptr->video_buf, ptr->v_width, ptr->v_height);

	if (vob->im_v_codec == CODEC_YUV) {
	  tc_rgb2yuv_core(ptr->video_buf);
	}

	return 0;
  }
  return 0;
}

