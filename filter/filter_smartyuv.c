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
#define MOD_VERSION "0.0.1 (2003-06-19)"
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
#include "../export/vid_aux.h"

static vob_t *vob=NULL;

///////////////////////////////////////////////////////////////////////////

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

   printf ("* Options\n");

   printf ("      'motionOnly' Show motion areas only (0=off, 1=on) [1]\n");
   printf ("       'threshold' Motion Threshold (0-255) [15]\n");
   printf ("         'denoise' denoise (0=off, 1=on) [0]\n");
   printf ("       'shiftEven' Phase shift (0=off, 1=on) [0]\n");

}

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
	unsigned char		*fmoving;
	char    		*prev;
	int			scenechange=0;
	long			count;
	int			x, y;
	int			nextValue, prevValue, luma, luman, lumap, T;
	int 			p0, p1, p2;
	int 			rp, rn, rpp, rnn, R;
	unsigned char		frMotion, fiMotion;
	int			cubic = mfd->cubic;
	static int counter=0;


	char * dst_buf;
	char * src_buf;

	//memset(ptr->video_buf+h*w, BLACK_BYTE_UV, h*w/2);
	src_buf = _src;
	dst_buf = _dst;

	/* Not much deinterlacing to do if there aren't at least 2 lines. */
	if (h < 2) return;

	count = 0;
	if (mfd->diffmode == FRAME_ONLY || mfd->diffmode == FRAME_AND_FIELD)
	{
		/* Skip first and last lines, they'll get a free ride. */
		src = src_buf + srcpitch;
		srcminus = src - srcpitch;
		prev = _prev + w;
		moving = _moving + w;
		for (y = 1; y < hminus1; y++)
		{
			x = 0;
			do
			{
				// First check frame motion.
				// Set the moving flag if the diff exceeds the configured
				// threshold.
				moving[x] = 0;
				frMotion = 0;
				prevValue = *prev&0xff;

				luma = src[x] & 0xff;
				if (abs(luma - prevValue) > _threshold) frMotion = 1;

				// Now check field motion if applicable.
				if (mfd->diffmode == FRAME_ONLY) moving[x] = frMotion;
				else
				{
					fiMotion = 0;
					if (y & 1)
						prevValue = srcminus[x]&0xff;
					else
						prevValue = *(prev + w)&0xff;

					luma = src[x] & 0xff;
					if (abs(luma - prevValue) > _threshold) fiMotion = 1;

					moving[x] = (fiMotion && frMotion);
				}
				*prev++ = src[x];
				/* Keep a count of the number of moving pixels for the
				   scene change detection. */
				if (moving[x]) count++;
			} while(++x < w);

			src = src + srcpitch;
			srcminus = srcminus + srcpitch;
			moving += w;
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
		}
	}
	else
	{
		/* Field differencing only mode. */
		T = _threshold * _threshold;
		src = src_buf + srcpitch;
		srcminus = src - srcpitch;
		srcplus = src + srcpitch;
		moving = _moving + w;
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
			moving += w;
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
					if (sum > 9)
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
	moving = _moving + w;
	movingminus = moving - w;
	movingplus = moving + w;
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
						p0 = src[x]&0xff;
						p0 &= 0xfe;

						p1 = srcminus[x]&0xff;
						p1 &= 0xfc;

						p2 = srcplus[x]&0xff;
						p2 &= 0xfc;

						dst[x] = ((p0>>1) + (p1>>2) + (p2>>2))&0xff;
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
				x = 0;
				do {
					if (!(movingminus[x] | moving[x] | movingplus[x]) && !scenechange)
						dst[x] = src[x];
					else
					{
						/* Blend fields. */
						p0 = src[x] & 0xff;
						p0 &= 0xfe;

						p1 = srcminus[x] & 0xff;
						p1 &= 0xfc;

						p2 = srcplus[x] & 0xff;
						p2 &= 0xfc;

						dst[x] = ((p0>>1) + (p1>>2) + (p2>>2)) & 0xff;
					}
				} while(++x < w);
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
								rpp = (srcminusminus[x]) & 0xff;
								rp =  (srcminus[x]) & 0xff;
								rn =  (srcplus[x]) & 0xff;
								rnn = (srcplusplus[x]) & 0xff;
								R = (5 * (rp + rn) - (rpp + rnn)) >> 3;
								dst[x] = clamp_f(R & 0xff)&0xff;
							}
							else
							{
								p1 = srcminus[x] & 0xff;
								p1 &= 0xfe;

								p2 = srcplus[x] & 0xff;
								p2 &= 0xfe;

								dst[x] = ((p1>>1) + (p2>>1)) & 0xff;
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
		moving += w;
		movingminus += w;
		movingplus += w;
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

	mfd->prevFrame =  malloc (width*height*3);
	memset(mfd->prevFrame, BLACK_BYTE_Y, width*height);
	memset(mfd->prevFrame+width*height, BLACK_BYTE_UV, width*height/2);

	mfd->buf =  malloc (width*height*3);
	memset(mfd->buf, BLACK_BYTE_Y, width*height);
	memset(mfd->buf+width*height, BLACK_BYTE_UV, width*height/2);

	mfd->movingY = (unsigned char *) malloc(sizeof(unsigned char)*width*height);
	memset(mfd->movingY, 0, width*height*sizeof(unsigned char));
	mfd->movingU = (unsigned char *) malloc(sizeof(unsigned char)*width*height/4);
	memset(mfd->movingU, 0, width*height*sizeof(unsigned char)/4);
	mfd->movingV = (unsigned char *) malloc(sizeof(unsigned char)*width*height/4);
	memset(mfd->movingV, 0, width*height*sizeof(unsigned char)/4);

	mfd->fmovingY = (unsigned char *) malloc(sizeof(unsigned char)*width*height);
	memset(mfd->fmovingY, 0, width*height*sizeof(unsigned char));
	mfd->fmovingU = (unsigned char *) malloc(sizeof(unsigned char)*width*height/4);
	memset(mfd->fmovingU, 0, width*height*sizeof(unsigned char)/4);
	mfd->fmovingV = (unsigned char *) malloc(sizeof(unsigned char)*width*height/4);
	memset(mfd->fmovingV, 0, width*height*sizeof(unsigned char)/4);


	// filter init ok.
	if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);

	return 0;

  } /* TC_FILTER_INIT */
	

  if(ptr->tag & TC_FILTER_GET_CONFIG) {
      char buf[255];
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRYE", "1");

      sprintf (buf, "%d", mfd->motionOnly);
      optstr_param (options, "motionOnly", "Show motion areas only, blacking out static areas" ,"%d", buf, "0", "1");
      sprintf (buf, "%d", mfd->threshold);
      optstr_param (options, "threshold", "Motion Threshold (luma)", "%d", buf, "0", "255" );
      sprintf (buf, "%d", mfd->chromathres);
      optstr_param (options, "chromathres", "Motion Threshold (chroma)", "%d", buf, "0", "255" );
      sprintf (buf, "%d", mfd->scenethreshold);
      optstr_param (options, "scenethres", "Threshold for detecting scenechanges", "%d", buf, "0", "255" );
      sprintf (buf, "%d", mfd->highq);
      optstr_param (options, "highq", "High-Quality processing (motion Map denoising)", "%d", buf, "0", "1" );
      sprintf (buf, "%d", mfd->cubic);
      optstr_param (options, "cubic", "Use cubic interpolation", "%d", buf, "0", "1" );
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

  if(ptr->tag & TC_PRE_PROCESS && ptr->tag & TC_VIDEO) {
	  
	  int U  = ptr->v_width*ptr->v_height;
	  int V  = ptr->v_width*ptr->v_height*5/4;
	  int w2 = ptr->v_width/2;
	  int h2 = ptr->v_height/2;


	  smartyuv_core(ptr->video_buf, mfd->buf, mfd->prevFrame, 
		        ptr->v_width, ptr->v_height, ptr->v_width, ptr->v_width,
		        mfd->movingY, mfd->fmovingY, clamp_Y, mfd->threshold);

	  if (mfd->doChroma) {
	      smartyuv_core(ptr->video_buf+U, mfd->buf+U, mfd->prevFrame+U, 
			  w2, h2, w2, w2,
			  mfd->movingU, mfd->fmovingU, clamp_UV, mfd->chromathres);

	      smartyuv_core(ptr->video_buf+V, mfd->buf+V, mfd->prevFrame+V, 
			  w2, h2, w2, w2,
			  mfd->movingV, mfd->fmovingV, clamp_UV, mfd->chromathres);
	  } else {
	      //pass through
	      memcpy(mfd->buf+U, ptr->video_buf+U, ptr->v_width*ptr->v_height/2);
	  }

	  /*
	  memset(mfd->buf, BLACK_BYTE_Y, ptr->v_width*ptr->v_height);
	  memset(mfd->buf+U, BLACK_BYTE_UV, ptr->v_width*ptr->v_height/4);
			  */

	  memcpy (ptr->video_buf, mfd->buf, ptr->video_size);

	  return 0;
  }
  return 0;
}

