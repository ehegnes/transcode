/*
    filter_smartyuv.c
    
    This file is part of transcode, a linux video stream processing tool
    
    2003 by Tilmann Bitterberg, based on code by Donald Graft.
    
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

*/

#define MOD_NAME    "filter_smartyuv.so"
#define MOD_VERSION "0.0.2 (2003-08-04)"
#define MOD_CAP     "Motion-adaptive deinterlacing"
#define MOD_AUTHOR  "Tilmann Bitterberg"

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "transcode.h"
#include "framebuffer.h"
#include "optstr.h"

static vob_t *vob=NULL;

///////////////////////////////////////////////////////////////////////////

// this value is "hardcoded" in the optimized code for speed reasons
#define DENOISE_DIAMETER 5

#define DENOISE_THRESH 7
#define BLACK_BYTE_Y 16
#define BLACK_BYTE_UV 128
#define MIN_Y 16
#define MAX_Y 240
#define FRAME_ONLY 0
#define FIELD_ONLY 1
#define FRAME_AND_FIELD 2

#define LUMA_THRESHOLD 14
#define CHROMA_THRESHOLD 7
#define SCENE_THRESHOLD 31

static unsigned char clamp_Y(unsigned char x) {
	return (x>MAX_Y?MAX_Y:(x<MIN_Y?MIN_Y:x));
}
static unsigned char clamp_UV(unsigned char x) {
	return (x);
}

static void smartyuv_core (char *_src, char *_dst, char *_prev, int _width, int _height, 
		int _srcpitch, int _dstpitch, 
		unsigned char *_moving, unsigned char *_fmoving, 
		unsigned char(*clamp_f)(unsigned char x), int _threshold );

typedef struct MyFilterData {
        char                    *buf;
	char			*prevFrame;
	unsigned char		*movingY;
	unsigned char		*movingU;
	unsigned char		*movingV;
	unsigned char		*fmovingY;
	unsigned char		*fmovingU;
	unsigned char		*fmovingV;
	int			motionOnly;
	int 			threshold;
	int 			chromathres;
	int                     codec;
	int                     diffmode;
	int			scenethreshold;
	int			cubic;
	int			highq;
	int 			Blend;
	int 			doChroma;
	int 			verbose;
} MyFilterData;

static MyFilterData *mfd;

static void help_optstr(void) 
{
   printf ("[%s] (%s) help\n", MOD_NAME, MOD_CAP);
   printf ("* Overview\n");
   printf ("   New filter smartyuv. This filter is basically a rewrite of the\n");
   printf ("   smartdeinter filter by Donald Graft (without advanced processing\n");
   printf ("   options) for YUV mode only. Its faster than using the smartdeinter\n");
   printf ("   in YUV mode and is also tuned with its threshold settings for YUV\n");
   printf ("   mode. The filter detects motion and static areas in an image and\n");
   printf ("   only deinterlaces (either by blending or by cubic interpolation)\n");
   printf ("   the moving areas. The result is an image with high detail in\n");
   printf ("   static areas, no information is lost there.\n");
   printf ("\n");
   printf ("   The threshold settings should be sufficent for most users. As a\n");
   printf ("   rule of thumb, I recommend setting the chroma threshold to about\n");
   printf ("   the half of the luma threshold. If you want more deinterlacing,\n");
   printf ("   lower the thresholds. The scene threshold can be easily found by\n");
   printf ("   turning on verbose mode and the preview filter. In verbose mode,\n");
   printf ("   the filter will print out, when it detects a scene change. If\n");
   printf ("   scenechanges go by unnoticed, lower the scene threshold. You can\n");
   printf ("   completly disable chroma processing with the doChroma=0 option.\n");
   printf ("   Here is a sample commandline\n");
   printf ("   -J smartyuv=hiqhq=1:diffmode=2:cubic=1:Blend=1:chromathres=4:threshold=8:doChroma=1\n");
   printf ("* Options\n");
   printf ("  'motionOnly' Show motion areas only (0=off, 1=on) [1]\n");
   printf ("    'diffmode' Motion Detection (0=frame, 1=field, 2=both) [0]\n");
   printf ("   'threshold' Motion Threshold (luma) (0-255) [14]\n");
   printf (" 'chromathres' Motion Threshold (chroma) (0-255) [7]\n");
   printf ("  'scenethres' Threshold for detecting scenechanges (0-255) [31]\n");
   printf ("       'cubic' Do cubic interpolation (0=off 1=on) [1]\n");
   printf ("       'highq' High-Quality processing (motion Map denoising) (0=off 1=on) [1]\n");
   printf ("       'Blend' Blend the frames for deinterlacing (0=off 1=on) [1]\n");
   printf ("    'doChroma' Enable chroma processing (slower but more accurate) (0=off 1=on) [1]\n");
   printf ("     'verbose' Verbose mode (0=off 1=on) [1]\n");

}

static void inline Blendline_c (uint8_t *dst, uint8_t *src, uint8_t *srcminus, uint8_t *srcplus, 
	                 uint8_t *moving, uint8_t *movingminus, uint8_t *movingplus, const int w, const int scenechange)
{
    int	x=0;

    x = 0;
    do {
	if (!(movingminus[x] | moving[x] | movingplus[x]) && !scenechange)
	    dst[x] = src[x];
	else
	{
	    /* Blend fields. */
	    dst[x] = ((src[x]>>1) + (srcminus[x]>>2) + (srcplus[x]>>2)) & 0xff;
	}
    } while(++x < w);
}

// this works fine on OSX too
#define ABS_u8(a) (((a)^((a)>>7))-((a)>>7))

static void smartyuv_core (char *_src, char *_dst, char *_prev, int _width, int _height, 
		int _srcpitch, int _dstpitch, 
		unsigned char *_moving, unsigned char *_fmoving, 
		unsigned char(*clamp_f)(unsigned char x), int _threshold )
{
	const int		srcpitch = _srcpitch;
	const int		dstpitch = _dstpitch;

	const int		w = _width;
	const int		wminus1 = w - 1;

	const int		h = _height;
	const int		hminus1 = h - 1;
	const int		hminus3 = h - 3;

	char			*src, *dst, *srcminus=NULL, *srcplus, *srcminusminus=NULL, *srcplusplus=NULL;
	unsigned char		*moving, *movingminus, *movingplus;
	unsigned char		*fmoving, *mm, *fm;
	char    		*prev;
	int			scenechange=0;
	long			count;
	int			x, y;
	int			nextValue, prevValue, luma, luman, lumap, T;
	int 			p0, p1, p2;
	int 			rp, rn, rpp, rnn, R;
	unsigned char		frMotion, fiMotion;
	int			cubic = mfd->cubic;
	static int 		counter=0;
	int                     msize;


	char * dst_buf;
	char * src_buf;

	//memset(ptr->video_buf+h*w, BLACK_BYTE_UV, h*w/2);
	src_buf = _src;
	dst_buf = _dst;

	/* Not much deinterlacing to do if there aren't at least 2 lines. */
	if (h < 2) return;

	if (mfd->diffmode == FRAME_ONLY || mfd->diffmode == FRAME_AND_FIELD)
	{
		/* Skip first and last lines, they'll get a free ride. */
		src = src_buf + srcpitch;
		srcminus = src - srcpitch;
		prev = _prev + w;
		moving = _moving + w+4;
		if (mfd->diffmode == FRAME_ONLY) {
		    count = 0;
		    for (y = 1; y < hminus1; y++)
		    {
			for (x=0; x<w; x++) {
				// First check frame motion.
				// Set the moving flag if the diff exceeds the configured
				// threshold.
				int luma = *src++&0xff;
				int p0 = luma - (*prev&0xff);

				*prev++ = luma;
				*moving = ((ABS_u8(p0) > _threshold));

				/* Keep a count of the number of moving pixels for the
				   scene change detection. */
				count += *moving++;
				
			}

			srcminus += srcpitch;
			moving += 4;
		    }
		} else if (mfd->diffmode == FRAME_AND_FIELD) {

		    count = 0;
		    for (y = 1; y < hminus1; y++)
		    {
			x = 0;
			if (y & 1) { 

			    do {

				int luma = *src++&0xff;
				int p0 = luma - (*(srcminus+x)&0xff);
				int p1 = luma - (*prev&0xff);
				/* 15:11 < GomGom> abs can be replaced by i^(i>>31)-(i>>31) */

				*prev++ = luma;
				*moving = ((ABS_u8(p0) > _threshold) & (ABS_u8(p1) > _threshold));
				count += *moving++;

			    } while(++x < w);

			} else {

			    do {

				int luma = *src++ & 0xff;
				int p0 = luma - (*(prev+w)&0xff);
				int p1 = luma - (*prev&0xff);

				*prev++ = luma;
				*moving = ((ABS_u8(p0) > _threshold) & (ABS_u8(p1) > _threshold));
				count += *moving++;

			    } while(++x < w);
			}

			moving += 4;
			srcminus += srcpitch;
		    }
		}

		/* Determine whether a scene change has occurred. */
		if ((100L * count) / (h * w) >= mfd->scenethreshold) scenechange = 1;
		else scenechange = 0;

		if (scenechange && mfd->verbose) 
		    printf("[%s] Scenechange at %6d (%6d moving pixels)\n", MOD_NAME, counter, count);
		/*
		printf("Frame (%04d) count (%8ld) sc (%d) calc (%02ld)\n", 
				counter, count, scenechange, (100 * count) / (h * w));
				*/


		/* Perform a denoising of the motion map if enabled. */
		if (!scenechange && mfd->highq)
		{
#if 0
			int xlo, xhi, ylo, yhi;
			int u, v;
			int N = DENOISE_DIAMETER;
			int Nover2 = N/2;
			int sum;
			unsigned char *m;

			// Erode.
			fmoving = _fmoving;
			for (y = 0; y < h; y++)
			{
				for (x = 0; x < w; x++)
				{
					if (!((_moving + y * w)[x]))
					{
						fmoving[x] = 0;	
						continue;
					}
					xlo = x - Nover2; if (xlo < 0) xlo = 0;
					xhi = x + Nover2; if (xhi >= w) xhi = wminus1;
					ylo = y - Nover2; if (ylo < 0) ylo = 0;
					yhi = y + Nover2; if (yhi >= h) yhi = hminus1;
					m = _moving + ylo * w;
					sum = 0;
					for (u = ylo; u <= yhi; u++)
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
				fmoving += w;
			}
			// Dilate.
			N = 5;
			Nover2 = N/2;
			moving = _moving;
			for (y = 0; y < h; y++)
			{
				for (x = 0; x < w; x++)
				{
					if (!((_fmoving + y * w)[x]))
					{
						moving[x] = 0;	
						continue;
					}
					xlo = x - Nover2; if (xlo < 0) xlo = 0;
					xhi = x + Nover2; if (xhi >= w) xhi = wminus1;
					ylo = y - Nover2; if (ylo < 0) ylo = 0;
					yhi = y + Nover2; if (yhi >= h) yhi = hminus1;
					m = _moving + ylo * w;
					for (u = ylo; u <= yhi; u++)
					{
						for (v = xlo; v <= xhi; v++)
						{
							m[v] = 1;
						}
						m += w;
					}
				}
				moving += w;
			}
#else
			int sum;
			uint8_t  *m;
			int w4 = w+4;
			
			// this isn't exacly the same output like the original
			// but just because the original was buggy -- tibit


			// Erode.
			fmoving = _fmoving;
			moving = _moving;
			for (y = 0; y < h; y++)
			{
			    for (x = 0; x < w; x++)
			    {

				if (!(fmoving[x] = moving[x]) )
				    continue;

				m = moving + x - 2*w4 -2;
				sum = 1;

				//sum += m[0] + m[1] + m[2] + m[3] + m[4];
				//max sum is 25 or better 1<<25
				sum <<= m[0]; sum <<= m[1]; sum <<= m[2]; sum <<= m[3]; sum <<= m[4];
				m += w4;
				sum <<= m[0]; sum <<= m[1]; sum <<= m[2]; sum <<= m[3]; sum <<= m[4];
				m += w4;
				sum <<= m[0]; sum <<= m[1]; sum <<= m[2]; sum <<= m[3]; sum <<= m[4];
				m += w4;
				sum <<= m[0]; sum <<= m[1]; sum <<= m[2]; sum <<= m[3]; sum <<= m[4];
				m += w4;
				sum <<= m[0]; sum <<= m[1]; sum <<= m[2]; sum <<= m[3]; sum <<= m[4];

				// check if the only bit set has an index of 8 or greater (threshold is 7)
				fmoving[x] = (sum > 128);
			    }
			    fmoving += w4;
			    moving += w4;
			}
			// Dilate.
			fmoving = _fmoving;
			moving = _moving;
			for (y = 0; y < h; y++)
			{
			    for (x = 0; x < w; x++)
			    {
				if (!(moving[x] = fmoving[x]) )
				    continue;

				m = moving + x - 2*w4 -2;

				//memset(m, 1, 5);
				*(unsigned int *)m = 0x01010101; m[4] = 1;
				m += w4;
				*(unsigned int *)m = 0x01010101; m[4] = 1;
				m += w4;
				*(unsigned int *)m = 0x01010101; m[4] = 1;
				m += w4;
				*(unsigned int *)m = 0x01010101; m[4] = 1;
				m += w4;
				*(unsigned int *)m = 0x01010101; m[4] = 1;
			    }
			    moving += w4;
			    fmoving += w4;
			}
#endif
		}
	}
	else
	{
		/* Field differencing only mode. */
		T = _threshold * _threshold;
		src = src_buf + srcpitch;
		srcminus = src - srcpitch;
		srcplus = src + srcpitch;
		moving = _moving + w+4;
		for (y = 1; y < hminus1; y++)
		{
			x = 0;
			do
			{
				// Set the moving flag if the diff exceeds the configured
				// threshold.
				moving[x] = 0;
				if (y & 1)
				{
					// Now check field motion.
					fiMotion = 0;
					luma = (src[x]) & 0xff;
					lumap= (srcminus[x]) & 0xff;
					luman = (srcplus[x]) & 0xff;
						if ((lumap - luma) * (luman - luma) > T)
							moving[x] = 1;
				}
				/* Keep a count of the number of moving pixels for the
				   scene change detection. */
				if (moving[x]) count++;
			} while(++x < w);
			src = src + srcpitch;
			srcminus = srcminus + srcpitch;
			srcplus = srcplus + srcpitch;
			moving += (w+4);
		}

		/* Determine whether a scene change has occurred. */
		if ((100L * count) / (h * w) >= mfd->scenethreshold) scenechange = 1;
		else scenechange = 0;

		/* Perform a denoising of the motion map if enabled. */
		if (!scenechange && mfd->highq)
		{
			int xlo, xhi, ylo, yhi;
			int u, v;
			int N = 5;
			int Nover2 = N/2;
			int sum;
			unsigned char *m;

			// Erode.
			fmoving = _fmoving;
			for (y = 0; y < h; y++)
			{
				for (x = 0; x < w; x++)
				{
					if (!((_moving + y * (w+4))[x]))
					{
						fmoving[x] = 0;	
						continue;
					}
					xlo = x - Nover2; if (xlo < 0) xlo = 0;
					xhi = x + Nover2; if (xhi >= w) xhi = wminus1;
					ylo = y - Nover2; if (ylo < 0) ylo = 0;
					yhi = y + Nover2; if (yhi >= h) yhi = hminus1;
					m = _moving + ylo * (w+4);
					sum = 0;
					for (u = ylo; u <= yhi; u++)
					{
						for (v = xlo; v <= xhi; v++)
						{
							sum += m[v];
						}
						m += w;
					}
					if (sum > 9)
						fmoving[x] = 1;
					else
						fmoving[x] = 0;
				}
				fmoving += (w+4);
			}

			// Dilate.
			N = 5;
			Nover2 = N/2;
			moving = _moving;
			for (y = 0; y < h; y++)
			{
				for (x = 0; x < w; x++)
				{
					if (!((_fmoving + y * (w+4))[x]))
					{
						moving[x] = 0;	
						continue;
					}
					xlo = x - Nover2; if (xlo < 0) xlo = 0;
					xhi = x + Nover2; if (xhi >= w) xhi = wminus1;
					ylo = y - Nover2; if (ylo < 0) ylo = 0;
					yhi = y + Nover2; if (yhi >= h) yhi = hminus1;
					m = _moving + ylo * (w+4);
					for (u = ylo; u <= yhi; u++)
					{
						for (v = xlo; v <= xhi; v++)
						{
							m[v] = 1;
						}
						m += (w+4);
					}
				}
				moving += (w+4);
			}		
		}
	}

	// Render.
	// The first line gets a free ride.
	src = src_buf;
	dst = dst_buf;

	memcpy(dst, src, w);
	src = src_buf + srcpitch;
	srcminus = src - srcpitch;
	srcplus = src + srcpitch;

	if (cubic)
	{
		srcminusminus = src - 3 * srcpitch;
		srcplusplus = src + 3 * srcpitch;
	}

	dst = dst_buf + dstpitch;
	moving = _moving + w+4;
	movingminus = _moving;
	movingplus = moving + w+4;

	/*
	*/

	for (y = 1; y < hminus1; y++)
	{
		if (mfd->motionOnly)
		{
			if (mfd->Blend)
			{
				x = 0;
				do {
					if (!(movingminus[x] | moving[x] | movingplus[x]) && !scenechange)
						dst[x] = (clamp_f==clamp_Y)?BLACK_BYTE_Y:BLACK_BYTE_UV;
					else
					{	
						/* Blend fields. */
						dst[x] = (((src[x]&0xff)>>1) + ((srcminus[x]&0xff)>>2) + ((srcplus[x]&0xff)>>2))&0xff;
					}
				} while(++x < w);
			}
			else
			{
				x = 0;
				do {
					if (!(movingminus[x] | moving[x] | movingplus[x]) && !scenechange)
						dst[x] = (clamp_f==clamp_Y)?BLACK_BYTE_Y:BLACK_BYTE_UV;
					else if (y & 1)
					{
						if (cubic && (y > 2) && (y < hminus3))
						{
							rpp = (srcminusminus[x]) & 0xff;
							rp =  (srcminus[x]) & 0xff;
							rn =  (srcplus[x]) & 0xff;
							rnn = (srcplusplus[x]) & 0xff;
							R = (5 * (rp + rn) - (rpp + rnn)) >> 3;
							/*
							if (R>240) R = 240;
							else if (R<16) R=16;
							*/
							dst[x] = clamp_f(R & 0xff)&0xff;
						}
						else
						{
							p1 = srcminus[x] &0xff;
							p1 &= 0xfe;

							p2 = srcplus[x] &0xff;
							p2 &= 0xfe;
							dst[x] = ((p1>>1) + (p2>>1)) &0xff;
						}
					}
					else
						dst[x] = src[x];
				} while(++x < w);
			}
		}
		else  /* Not motion only */
		{
			if (mfd->Blend)
			{
			    Blendline_c (dst, src, srcminus, srcplus, moving, movingminus, movingplus, w, scenechange);
			}
			else
			{
				// Doing line interpolate. Thus, even lines are going through
				// for moving and non-moving mode. Odd line pixels will be subject
				// to the motion test.
				if (y&1)
				{
					x = 0;
					do {
						if (!(movingminus[x] | moving[x] | movingplus[x]) && !scenechange)
							dst[x] = src[x];
						else
						{
							if (cubic && (y > 2) && (y < hminus3))
							{
								R = (5 * ((srcminus[x]&0xff) + (srcplus[x]&0xff)) 
									- ((srcminusminus[x]&0xff) + (srcplusplus[x]&0xff))) >> 3;
								dst[x] = clamp_f(R & 0xff)&0xff;
							}
							else
							{
								dst[x] = (((srcminus[x]&0xff)>>1) + ((srcplus[x]&0xff)>>1)) & 0xff;
							}
						}
					} while(++x < w);
				}
				else
				{
					// Even line; pass it through.
					memcpy(dst, src, w);
				}
			}
		}
		src = src + srcpitch;
		srcminus = srcminus + srcpitch;
		srcplus = srcplus + srcpitch;

		if (cubic)
		{
			srcminusminus = srcminusminus + srcpitch;
			srcplusplus = srcplusplus + srcpitch;
		}

		dst = dst + dstpitch;
		moving += (w+4);
		movingminus += (w+4);
		movingplus += (w+4);
	}
	// The last line gets a free ride.
	memcpy(dst, src, w);
	if (clamp_f == clamp_Y)
	    counter++;

}

int tc_filter(vframe_list_t *ptr, char *options)
{

  //----------------------------------
  //
  // filter init
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_INIT) {

	unsigned int width, height;
	int msize;
    
	if((vob = tc_get_vob())==NULL) return(-1);
    

	mfd = (MyFilterData *) malloc(sizeof(MyFilterData));

	if (!mfd) {
		fprintf(stderr, "No memory!\n"); return (-1);
	}

	memset (mfd, 0, sizeof(MyFilterData));

	width  = vob->im_v_width;
	height = vob->im_v_height;

	/* default values */
	mfd->motionOnly     = 0;
	mfd->threshold      = LUMA_THRESHOLD;
	mfd->chromathres    = CHROMA_THRESHOLD;
	mfd->scenethreshold = SCENE_THRESHOLD;
	mfd->diffmode       = FRAME_ONLY;
	mfd->codec          = vob->im_v_codec;
	mfd->highq          = 1;
	mfd->cubic          = 1;
	mfd->doChroma       = 1;
	mfd->Blend          = 1;
	mfd->verbose        = 0;

	if (mfd->codec != CODEC_YUV) {
	    tc_warn ("[%s] This filter is only capable of YUV mode", MOD_NAME);
	    return -1;
	}

	if (options != NULL) {
    
	  if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);

	  optstr_get (options, "motionOnly",     "%d",  &mfd->motionOnly     );
	  optstr_get (options, "threshold",      "%d",  &mfd->threshold      );
	  optstr_get (options, "chromathres",    "%d",  &mfd->chromathres    );
	  optstr_get (options, "Blend",          "%d",  &mfd->Blend          );
	  optstr_get (options, "scenethres",     "%d",  &mfd->scenethreshold );
	  optstr_get (options, "highq",          "%d",  &mfd->highq          );
	  optstr_get (options, "cubic",          "%d",  &mfd->cubic          );
	  optstr_get (options, "diffmode",       "%d",  &mfd->diffmode       );
	  optstr_get (options, "doChroma",       "%d",  &mfd->doChroma       );
	  optstr_get (options, "verbose",        "%d",  &mfd->verbose        );

	  if (optstr_get (options, "help", "") >= 0) {
		  help_optstr();
	  }
	}

	if (verbose > 1) {

	  printf (" Smart YUV Deinterlacer Test Filter Settings (%dx%d):\n", width, height);
	  printf ("        motionOnly = %d\n", mfd->motionOnly);
	  printf ("          diffmode = %d\n", mfd->diffmode);
	  printf ("         threshold = %d\n", mfd->threshold);
	  printf ("       chromathres = %d\n", mfd->chromathres);
	  printf ("        scenethres = %d\n", mfd->scenethreshold);
	  printf ("             cubic = %d\n", mfd->cubic);
	  printf ("             highq = %d\n", mfd->highq);
	  printf ("             Blend = %d\n", mfd->Blend);
	  printf ("          doChroma = %d\n", mfd->doChroma);
	  printf ("           verbose = %d\n", mfd->verbose);
	}

	/* fetch memory */



	mfd->buf =  malloc (width*height*3);
	mfd->prevFrame =  malloc (width*height*3);

	msize = width*height + 4*(width+4) + 4*height;
	mfd->movingY = (unsigned char *) malloc(sizeof(unsigned char)*msize);
	mfd->fmovingY = (unsigned char *) malloc(sizeof(unsigned char)*msize);

	msize = width*height/4 + 4*(width+4) + 4*height;
	mfd->movingU  = (unsigned char *) malloc(sizeof(unsigned char)*msize);
	mfd->movingV  = (unsigned char *) malloc(sizeof(unsigned char)*msize);
	mfd->fmovingU = (unsigned char *) malloc(sizeof(unsigned char)*msize);
	mfd->fmovingV = (unsigned char *) malloc(sizeof(unsigned char)*msize);

	if ( !mfd->movingY || !mfd->movingU || !mfd->movingV || !mfd->fmovingY || 
	      !mfd->fmovingU || !mfd->fmovingV || !mfd->buf || !mfd->prevFrame) {
	    fprintf (stderr, "[%s] Memory allocation error\n", MOD_NAME);
	    return -1;
	}

	memset(mfd->prevFrame, BLACK_BYTE_Y, width*height);
	memset(mfd->prevFrame+width*height, BLACK_BYTE_UV, width*height/2);

	memset(mfd->buf, BLACK_BYTE_Y, width*height);
	memset(mfd->buf+width*height, BLACK_BYTE_UV, width*height/2);

	msize = width*height + 4*(width+4) + 4*height;
	memset(mfd->movingY,  0, msize);
	memset(mfd->fmovingY, 0, msize);

	msize = width*height/4 + 4*(width+4) + 4*height;
	memset(mfd->movingU,  0, msize);
	memset(mfd->movingV,  0, msize);
	memset(mfd->fmovingU, 0, msize);
	memset(mfd->fmovingV, 0, msize);

	// Optimisation
	// For the motion maps a little bit more than the needed memory is
	// allocated. This is done, because than we don't have to use
	// conditional borders int the erode and dilate routines. 2 extra lines
	// on top and bottom and 2 pixels left and right for each line.
	// This is also the reason for the w+4's all over the place.
	//
	// This gives an speedup factor in erode+denoise of about 3.
	// 
	// A lot of brain went into the optimisations, here are some numbers of
	// the separate steps. Note, to get these numbers I used the rdtsc
	// instruction to read the CPU cycle counter in seperate programms:
	// o  Motion map creation
	//      orig: 26.283.387 Cycles
	//       now:  8.991.686 Cycles
	// o  Erode+dilate
	//      orig: 55.847.077
	//       now: 21.764.997
	// o  Blending
	//      orig: 8.162.287
	//       now: 5.384.433
	// o  Cubic interpolation
	//      orig: 7.487.338
	//       now: 6.684.908
	//
	// Overall improvement in transcode:
	// 16.88 -> 24.72 frames per second for the test clip.
	//

	// filter init ok.
	if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);

	return 0;

  } /* TC_FILTER_INIT */
	

  if(ptr->tag & TC_FILTER_GET_CONFIG) {
      char buf[255];
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRYE", "1");

      sprintf (buf, "%d", mfd->motionOnly);
      optstr_param (options, "motionOnly", "Show motion areas only, blacking out static areas" ,"%d", buf, "0", "1");
      sprintf (buf, "%d", mfd->diffmode);
      optstr_param (options, "diffmode", "Motion Detection (0=frame, 1=field, 2=both)", "%d", buf, "0", "2" );
      sprintf (buf, "%d", mfd->threshold);
      optstr_param (options, "threshold", "Motion Threshold (luma)", "%d", buf, "0", "255" );
      sprintf (buf, "%d", mfd->chromathres);
      optstr_param (options, "chromathres", "Motion Threshold (chroma)", "%d", buf, "0", "255" );
      sprintf (buf, "%d", mfd->scenethreshold);
      optstr_param (options, "scenethres", "Threshold for detecting scenechanges", "%d", buf, "0", "255" );
      sprintf (buf, "%d", mfd->highq);
      optstr_param (options, "highq", "High-Quality processing (motion Map denoising)", "%d", buf, "0", "1" );
      sprintf (buf, "%d", mfd->cubic);
      optstr_param (options, "cubic", "Do cubic interpolation", "%d", buf, "0", "1" );
      sprintf (buf, "%d", mfd->Blend);
      optstr_param (options, "Blend", "Blend the frames for deinterlacing", "%d", buf, "0", "1" );
      sprintf (buf, "%d", mfd->doChroma);
      optstr_param (options, "doChroma", "Enable chroma processing (slower but more accurate)", "%d", buf, "0", "1" );
      sprintf (buf, "%d", mfd->verbose);
      optstr_param (options, "verbose", "Verbose mode", "%d", buf, "0", "1" );

      return (0);
  }

  if(ptr->tag & TC_FILTER_CLOSE) {

	if (!mfd)
		return 0;
	
	if (mfd->buf)
	    free(mfd->buf);
	mfd->buf = NULL;

	if (mfd->prevFrame)
	    free(mfd->prevFrame);
	mfd->prevFrame = NULL;

	if (mfd->movingY) free(mfd->movingY); mfd->movingY = NULL;
	if (mfd->movingU) free(mfd->movingU); mfd->movingU = NULL;
	if (mfd->movingV) free(mfd->movingV); mfd->movingV = NULL;

	if (mfd->fmovingY) free(mfd->fmovingY); mfd->fmovingY = NULL;
	if (mfd->fmovingU) free(mfd->fmovingU); mfd->fmovingU = NULL;
	if (mfd->fmovingV) free(mfd->fmovingV); mfd->fmovingV = NULL;

	if (mfd)
		free(mfd);

	return 0;

  } /* TC_FILTER_CLOSE */

///////////////////////////////////////////////////////////////////////////

  //if(ptr->tag & TC_PRE_S_PROCESS && ptr->tag & TC_VIDEO && !(ptr->attributes & TC_FRAME_IS_SKIPPED)) {
  if(ptr->tag & TC_PRE_PROCESS && ptr->tag & TC_VIDEO && !(ptr->attributes & TC_FRAME_IS_SKIPPED)) {
	  
	  int U  = ptr->v_width*ptr->v_height;
	  int V  = ptr->v_width*ptr->v_height*5/4;
	  int w2 = ptr->v_width/2;
	  int h2 = ptr->v_height/2;
	  int msize = ptr->v_width*ptr->v_height + 4*(ptr->v_width+4) + 4*ptr->v_height;
	  int off = 2*(ptr->v_width+4)+2;

	  memset(mfd->movingY,  0, msize);
	  memset(mfd->fmovingY, 0, msize);
	  /*
	  */


	  smartyuv_core(ptr->video_buf, mfd->buf, mfd->prevFrame, 
		        ptr->v_width, ptr->v_height, ptr->v_width, ptr->v_width,
		        mfd->movingY+off, mfd->fmovingY+off, clamp_Y, mfd->threshold);


	  if (mfd->doChroma) {
	      msize = ptr->v_width*ptr->v_height/4 + 4*(ptr->v_width+4) + 4*ptr->v_height;
	      off = 2*(ptr->v_width/2+4)+2;

	      memset(mfd->movingU,  0, msize);
	      memset(mfd->fmovingU, 0, msize);
	      memset(mfd->movingV,  0, msize);
	      memset(mfd->fmovingV, 0, msize);
	      /*
	      */

	      smartyuv_core(ptr->video_buf+U, mfd->buf+U, mfd->prevFrame+U, 
			  w2, h2, w2, w2,
			  mfd->movingU+off, mfd->fmovingU+off, clamp_UV, mfd->chromathres);

	      smartyuv_core(ptr->video_buf+V, mfd->buf+V, mfd->prevFrame+V, 
			  w2, h2, w2, w2,
			  mfd->movingV+off, mfd->fmovingV+off, clamp_UV, mfd->chromathres);
	  } else {
	      //pass through
	      memcpy(mfd->buf+U, ptr->video_buf+U, ptr->v_width*ptr->v_height/2);
	      //memset(mfd->buf+U, BLACK_BYTE_UV, ptr->v_width*ptr->v_height/2);
	  }

	  /*
	  memset(mfd->buf, BLACK_BYTE_Y, ptr->v_width*ptr->v_height);
	  memset(mfd->buf+U, BLACK_BYTE_UV, ptr->v_width*ptr->v_height/2);
			  */

	  memcpy (ptr->video_buf, mfd->buf, ptr->video_size);

	  return 0;
  }
  return 0;
}

