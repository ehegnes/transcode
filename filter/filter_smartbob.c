/*
    filter_smartbob.c
    
    This file is part of transcode, a video stream processing tool
    
    Smart Bob Filter for VirtualDub -- Break fields into frames using
    a motion-adaptive algorithm. Copyright (C) 1999-2001 Donald A. Graft

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

    modified 2003 by Tilmann Bitterberg for use with transcode
    
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
    http://sauron.mordor.net/dgraft/
*/

#define MOD_NAME    "filter_smartbob.so"
#define MOD_VERSION "v1.1beta2 (2003-06-23)"
#define MOD_CAP     "Motion-adaptive deinterlacing for double-frame-rate output."
#define MOD_AUTHOR  "Donald Graft, Tilmann Bitterberg"

#include "transcode.h"
#include "filter.h"
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


///////////////////////////////////////////////////////////////////////////

#define DENOISE_DIAMETER 5
#define DENOISE_THRESH 7

typedef struct MyFilterData {
	Pixel32			*convertFrameIn;
	Pixel32			*convertFrameOut;
	int			*prevFrame;
	unsigned char		*moving;
	unsigned char		*fmoving;
	int 			bShiftEven;
	int			bMotionOnly;
	int 			bDenoise;
	int 			threshold;
	int                     codec;
} MyFilterData;

static MyFilterData *mfd;

#warning **************** FIXME ****************** use imgconvert
/*
 * Colorspace conversions
 */

#define R_MASK  (0x00ff0000)
#define G_MASK  (0x0000ff00)
#define B_MASK  (0x000000ff)
#define R_SHIFT         (16)
#define G_SHIFT          (8)
#define B_SHIFT          (0)

/* convert transcode RGB (3*8 Bit) to vdub ARGB (32Bit) */
static void convert_rgb2argb (char * in, Pixel32 *out, int width, int height) 
{
	int run;
	int size = width*height;

	for (run = 0; run < size; run++) {
		*out = (((((Pixel32) *(in+0)) & 0xff) << R_SHIFT) | 
		        ((((Pixel32) *(in+1)) & 0xff) << G_SHIFT) | 
			((((Pixel32) *(in+2)) & 0xff)))      & 0x00ffffff;

		out++;
		in += 3;
	}
}

/* convert vdub ARGB (32Bit) to transcode RGB (3*8 Bit) */
static void convert_argb2rgb (Pixel32 *in, char * out, int width, int height)
{
	int run;
	int size = width*height;

	for (run = 0; run < size; run++) {

		*(out+0) = ((*in & R_MASK) >> R_SHIFT)&0xff;
		*(out+1) = ((*in & G_MASK) >> G_SHIFT)&0xff;
		*(out+2) = (*in) & B_MASK;

		in++;
		out += 3;
	}
}

static void help_optstr(void) 
{
   printf ("[%s] (%s) help\n", MOD_NAME, MOD_CAP);
   printf ("* Overview\n");
   printf ("   This filter only makes sence when fed by -J doublefps.\n");
   printf ("   It will take the field-frames which filter_doublefps\n");
   printf ("   produces and generates full-sized motion adaptive deinterlaced \n");
   printf ("   output at the double import framerate.\n");
   printf ("   If you force reading the imput file twice its actual frames \n");
   printf ("   per second, A/V will stay in sync (for PAL):\n");
   printf ("   -f 50 -J doublefps=shiftEven=1,smartbob=denoise=1:threshold=12\n");

   printf ("* Options\n");

   printf ("      'motionOnly' Show motion areas only (0=off, 1=on) [1]\n");
   printf ("       'threshold' Motion Threshold (0-255) [15]\n");
   printf ("         'denoise' denoise (0=off, 1=on) [0]\n");
   printf ("       'shiftEven' Phase shift (0=off, 1=on) [0]\n");

}

int tc_filter(frame_list_t *ptr_, char *options)
{
  vframe_list_t *ptr = (vframe_list_t *)ptr_;

  //----------------------------------
  //
  // filter init
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_INIT) {

	unsigned int width, height;
    
	if((vob = tc_get_vob())==NULL) return(-1);
    

	mfd = (MyFilterData *) malloc(sizeof(MyFilterData));

	if (!mfd) {
		fprintf(stderr, "No memory!\n"); return (-1);
	}

	memset (mfd, 0, sizeof(MyFilterData));

	width  = vob->im_v_width;
	height = vob->im_v_height;

	/* default values */
	mfd->bShiftEven = 0;
	mfd->bMotionOnly = 0;
	mfd->bDenoise = 1;
	mfd->threshold = 12;
	mfd->codec          = vob->im_v_codec;

	if (options != NULL) {
    
	  if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);

	  optstr_get (options, "motionOnly",     "%d",  &mfd->bMotionOnly       );
	  optstr_get (options, "shiftEven",      "%d",  &mfd->bShiftEven        );
	  optstr_get (options, "threshold",      "%d",  &mfd->threshold        );
	  optstr_get (options, "denoise",        "%d",  &mfd->bDenoise         );

	  if (optstr_get (options, "help", "") >= 0) {
		  help_optstr();
	  }
	}

	if (verbose > 1) {

	  printf (" Smart Deinterlacer Filter Settings (%dx%d):\n", width, height);
	  printf ("        motionOnly = %d\n", mfd->bMotionOnly);
	  printf ("            denois = %d\n", mfd->bDenoise);
	  printf ("         threshold = %d\n", mfd->threshold);
	  printf ("         shiftEven = %d\n", mfd->bShiftEven);
	}

	/* fetch memory */

	mfd->convertFrameIn = (Pixel32 *) malloc (width * height * sizeof(Pixel32));
	memset(mfd->convertFrameIn, 0, width * height * sizeof(Pixel32));

	mfd->convertFrameOut = (Pixel32 *) malloc (width * height * sizeof(Pixel32));
	memset(mfd->convertFrameOut, 0, width * height * sizeof(Pixel32));

	mfd->prevFrame = (int *) malloc (width*height*sizeof(int));
	memset(mfd->prevFrame, 0, width*height*sizeof(int));

	mfd->moving = (unsigned char *) malloc(sizeof(unsigned char)*width*height);
	memset(mfd->moving, 0, width*height*sizeof(unsigned char));

	mfd->fmoving = (unsigned char *) malloc(sizeof(unsigned char)*width*height);
	memset(mfd->fmoving, 0, width*height*sizeof(unsigned char));

	if (mfd->codec == CODEC_YUV) {
	    tc_yuv2rgb_init(width, height/2);
	    tc_rgb2yuv_init(width, height);
	}

	// filter init ok.
	if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);

	return 0;

  } /* TC_FILTER_INIT */
	

  if(ptr->tag & TC_FILTER_GET_CONFIG) {
      char buf[255];
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRYO", "1");

      snprintf (buf, sizeof(buf), "%d", mfd->bMotionOnly);
      optstr_param (options, "motionOnly", "Show motion areas only" ,"%d", buf, "0", "1");
      snprintf (buf, sizeof(buf), "%d", mfd->bShiftEven);
      optstr_param (options, "shiftEven", "Blend instead of interpolate in motion areas", "%d", buf, "0", "1" );
      snprintf (buf, sizeof(buf), "%d", mfd->threshold);
      optstr_param (options, "threshold", "Motion Threshold", "%d", buf, "0", "255" );
      snprintf (buf, sizeof(buf), "%d", mfd->bDenoise);
      optstr_param (options, "denoise", "Phase shift", "%d", buf, "0", "1" );

      return (0);
  }

  if(ptr->tag & TC_FILTER_CLOSE) {

	if (!mfd)
		return 0;
	
	if (mfd->prevFrame)
	    free(mfd->prevFrame);
	mfd->prevFrame = NULL;

	if (mfd->moving)
	    free(mfd->moving);
	mfd->moving = NULL;

	if (mfd->fmoving)
	    free(mfd->fmoving);
	mfd->fmoving = NULL;

	if (mfd->convertFrameIn) {
		free (mfd->convertFrameIn);
		mfd->convertFrameIn = NULL;
	}

	if (mfd->convertFrameOut) {
		free (mfd->convertFrameOut);
		mfd->convertFrameOut = NULL;
	}

	if (mfd->codec == CODEC_YUV) {
	    tc_rgb2yuv_close();
	    tc_yuv2rgb_close();
	}
	if (mfd)
		free(mfd);

	return 0;

  } /* TC_FILTER_CLOSE */

///////////////////////////////////////////////////////////////////////////

  if(ptr->tag & TC_POST_S_PROCESS && ptr->tag & TC_VIDEO) {
    
	Pixel32 *src, *dst, *srcn, *srcnn, *srcp;
	unsigned char *moving, *fmoving;
	int x, y, *prev;
	long currValue, prevValue, nextValue, nextnextValue, luma, lumap, luman;
	int r, g, b, rp, gp, bp, rn, gn, bn, rnn, gnn, bnn, R, G, B, T = mfd->threshold * mfd->threshold;
	int h = ptr->v_height/2;
	int w = ptr->v_width;
	int hminus = ptr->v_height/2 - 1;
	int hminus2 = ptr->v_height/2 - 2;
	int wminus = ptr->v_width - 1;
	int iOddEven =          mfd->bShiftEven ? 0 : 1;
	int pitch = ptr->v_width*4;

	Pixel32 * dst_buf;
	Pixel32 * src_buf;

	if (mfd->codec == CODEC_YUV) {
	    tc_yuv2rgb_core(ptr->video_buf);
	}

	convert_rgb2argb (ptr->video_buf, mfd->convertFrameIn, ptr->v_width, ptr->v_height/2);

	src_buf = mfd->convertFrameIn;
	dst_buf = mfd->convertFrameOut;

#if 0
#endif
	/* Calculate the motion map. */
	moving = mfd->moving;
	/* Threshold 0 means treat all areas as moving, i.e., dumb bob. */
	if (mfd->threshold == 0)
	{
		memset(moving, 1, ptr->v_height/2 * ptr->v_width);
	}
	else
	{
		memset(moving, 0, ptr->v_height/2 * ptr->v_width);
		src = (Pixel32 *)src_buf;
		srcn = (Pixel32 *)((char *)src + pitch);
		prev = mfd->prevFrame;
		if((ptr->tag & TC_FRAME_WAS_CLONED) == iOddEven)
			prev += ptr->v_width;
		for (y = 0; y < hminus; y++)
		{
			for (x = 0; x < w; x++)
			{
				currValue = prev[x];
				nextValue = srcn[x];
				prevValue = src[x];
				r = (currValue >> 16) & 0xff;
				rp = (prevValue >> 16) & 0xff;
				rn = (nextValue >> 16) & 0xff;
				g = (currValue >> 8) & 0xff;
				gp = (prevValue >> 8) & 0xff;
				gn = (nextValue >> 8) & 0xff;
				b = currValue & 0xff;
				bp = prevValue & 0xff;
				bn = nextValue & 0xff;
				luma = (55 * r + 182 * g + 19 * b) >> 8;
				lumap = (55 * rp + 182 * gp + 19 * bp) >> 8;
				luman = (55 * rn + 182 * gn + 19 * bn) >> 8;
				if ((lumap - luma) * (luman - luma) >= T)
					moving[x] = 1;
			}
			src = (Pixel32 *)((char *)src + pitch);
			srcn = (Pixel32 *)((char *)srcn + pitch);
			moving += w;
			prev += w;
		}
		/* Can't diff the last line. */
		memset(moving, 0, ptr->v_width);

		/* Motion map denoising. */
		if (mfd->bDenoise)
		{
			int xlo, xhi, ylo, yhi, xsize;
			int u, v;
			int N = DENOISE_DIAMETER;
			int Nover2 = N/2;
			int sum;
			unsigned char *m;


			// Erode.
			moving = mfd->moving;
			fmoving = mfd->fmoving;
			for (y = 0; y < h; y++)
			{
				for (x = 0; x < w; x++)
				{
					if (moving[x] == 0)
					{
						fmoving[x] = 0;	
						continue;
					}
					xlo = x - Nover2; if (xlo < 0) xlo = 0;
					xhi = x + Nover2; if (xhi >= w) xhi = wminus;
					ylo = y - Nover2; if (ylo < 0) ylo = 0;
					yhi = y + Nover2; if (yhi >= h) yhi = hminus;
					for (u = ylo, sum = 0, m = mfd->moving + ylo * w; u <= yhi; u++)
					{
						for (v = xlo; v <= xhi; v++)
						{
							sum += m[v];
						}
						m += w;
					}
					if (sum > DENOISE_THRESH)
						fmoving[x] = 1;
					else
						fmoving[x] = 0;
				}
				moving += w;
				fmoving += w;
			}

			// Dilate.
			moving = mfd->moving;
			fmoving = mfd->fmoving;
			for (y = 0; y < h; y++)
			{
				for (x = 0; x < w; x++)
				{
					if (fmoving[x] == 0)
					{
						moving[x] = 0;	
						continue;
					}
					xlo = x - Nover2;
					if (xlo < 0) xlo = 0;
					xhi = x + Nover2;
					/* Use w here instead of wminus so we don't have to add 1 in the
					   the assignment of xsize. */
					if (xhi >= w) xhi = w;
					xsize = xhi - xlo;
					ylo = y - Nover2;
					if (ylo < 0) ylo = 0;
					yhi = y + Nover2;
					if (yhi >= h) yhi = hminus;
					m = mfd->moving + ylo * w;
					for (u = ylo; u <= yhi; u++)
					{
						memset(&m[xlo], 1, xsize); 
						m += w;
					}
				}
				moving += w;
				fmoving += w;
			}
		}
	}

	/* Output the destination frame. */
	if (!mfd->bMotionOnly)
	{
		/* Output the destination frame. */
		src = (Pixel32 *)src_buf;
		srcn = (Pixel32 *)((char *)src_buf + pitch);
		srcnn = (Pixel32 *)((char *)src_buf + 2 * pitch);
		srcp = (Pixel32 *)((char *)src_buf - pitch);
		dst = (Pixel32 *)dst_buf;
		if((ptr->tag & TC_FRAME_WAS_CLONED) == iOddEven)
		{
			/* Shift this frame's output up by one line. */
			ac_memcpy(dst, src, ptr->v_width * sizeof(Pixel32));
			dst = (Pixel32 *)((char *)dst + pitch);
			prev = mfd->prevFrame + w;
		}
		else
		{
			prev = mfd->prevFrame;
		}
		moving = mfd->moving;
		for (y = 0; y < hminus; y++)
		{
			/* Even output line. Pass it through. */
			ac_memcpy(dst, src, ptr->v_width * sizeof(Pixel32));
			dst = (Pixel32 *)((char *)dst + pitch);
			/* Odd output line. Synthesize it. */
			for (x = 0; x < w; x++)
			{
				if (moving[x] == 1)
				{
					/* Make up a new line. Use cubic interpolation where there
					   are enough samples and linear where there are not enough. */
					nextValue = srcn[x];
					r = (src[x] >> 16) & 0xff;
					rn = (nextValue >> 16) & 0xff;
					g = (src[x] >> 8) & 0xff;
					gn = (nextValue >>8) & 0xff;
					b = src[x] & 0xff;
					bn = nextValue & 0xff;
					if (y == 0 || y == hminus2)
					{	/* Not enough samples; use linear. */
						R = (r + rn) >> 1;
						G = (g + gn) >> 1;
						B = (b + bn) >> 1;
					}
					else
					{
						/* Enough samples; use cubic. */
						prevValue = srcp[x];
						nextnextValue = srcnn[x];
						rp = (prevValue >> 16) & 0xff;
						rnn = (nextnextValue >>16) & 0xff;
						gp = (prevValue >> 8) & 0xff;
						gnn = (nextnextValue >> 8) & 0xff;
						bp = prevValue & 0xff;
						bnn = nextnextValue & 0xff;
						R = (5 * (r + rn) - (rp + rnn)) >> 3;
						if (R > 255) R = 255;
						else if (R < 0) R = 0;
						G = (5 * (g + gn) - (gp + gnn)) >> 3;
						if (G > 255) G = 255;
						else if (G < 0) G = 0;
						B = (5 * (b + bn) - (bp + bnn)) >> 3;
						if (B > 255) B = 255;
						else if (B < 0) B = 0;
					}
					dst[x] = ( ((R << 16)&0xff0000) | ((G << 8)&0xff00) | (B&0xff)) & 0x00ffffff;  
				}
				else
				{
					/* Use line from previous field. */
					dst[x] = prev[x];
				}
			}
			src = (Pixel32 *)((char *)src + pitch);
			srcn = (Pixel32 *)((char *)srcn + pitch);
			srcnn = (Pixel32 *)((char *)srcnn + pitch);
			srcp = (Pixel32 *)((char *)srcp + pitch);
			dst = (Pixel32 *)((char *)dst + pitch);
			moving += w;
			prev += w;
		}
		/* Copy through the last source line. */

		ac_memcpy(dst, src, ptr->v_width * sizeof(Pixel32));
		if((ptr->tag & TC_FRAME_WAS_CLONED)!= iOddEven)
		{
			dst = (Pixel32 *)((char *)dst + pitch);
			ac_memcpy(dst, src, ptr->v_width * sizeof(Pixel32));
		}
	}
	else
	{
		/* Show motion only. */
		moving = mfd->moving;
		src = (Pixel32 *)src_buf;
		dst = (Pixel32 *)dst_buf;
		for (y = 0; y < hminus; y++)
		{
			for (x = 0; x < w; x++)
			{
				if (moving[x])
				{
					dst[x] = ((Pixel32 *)((char *)dst + pitch))[x] = src[x];
				}
				else
				{
					dst[x] = ((Pixel32 *)((char *)dst + pitch))[x] = 0;
				}
			}
			src = (Pixel32 *)((char *)src + pitch);
			dst = (Pixel32 *)((char *)dst + pitch);
			dst = (Pixel32 *)((char *)dst + pitch);
			moving += w;
		}
	}


	/* Buffer the input frame (aka field). */
	src = (Pixel32 *)src_buf;
	prev = mfd->prevFrame;
	for (y = 0; y < h; y++)
	{
		ac_memcpy(prev, src, w * sizeof(Pixel32));
		src = (Pixel32 *)((char *)src + pitch);
		prev += w;
	}


	convert_argb2rgb (mfd->convertFrameOut, ptr->video_buf, ptr->v_width, ptr->v_height);

	if (mfd->codec == CODEC_YUV)
	    tc_rgb2yuv_core(ptr->video_buf);

	return 0;
  }
  return 0;
}
