/*
    Copyright (C) 2003 Daniel Moreno <comac@comac.darktech.org>
    Converted for use in transcode by Tilmann Bitterberg <transcode@tibit.org>
    Converted hqdn3d -> denoise3d and also heavily optimised for transcode
        by Erik Slagter <erik@oldconomy.org> (GPL) 2003

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#define MOD_NAME    "filter_denoise3d.so"
#define MOD_VERSION "v1.0.3 (2003-11-08)"
#define MOD_CAP     "High speed 3D Denoiser"
#define MOD_AUTHOR  "Daniel Moreno & A'rpi"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "transcode.h"
#include "framebuffer.h"
#include "filter.h"
#include "optstr.h"

#define PARAM1_DEFAULT 4.0
#define PARAM2_DEFAULT 3.0
#define PARAM3_DEFAULT 6.0

static int enable_luma = 1;
static int enable_chroma = 1;

//===========================================================================//

typedef struct vf_priv_s {
        int Coefs[4][512];
        unsigned char *Line;
		int pre;
} MyFilterData;

/***************************************************************************/

#define LowPass(prev, curr, coef) (curr + coef[prev - curr])

static inline int ABS(int i)
{
    return(((i) >= 0) ? i : (0 - i));
}

static void deNoise(unsigned char *Frame,        // mpi->planes[x]
                    unsigned char *FramePrev,    // pmpi->planes[x]
                    unsigned char *LineAnt,      // vf->priv->Line (width bytes)
                    int W, int H,
                    int *Horizontal, int *Vertical, int *Temporal)
{
    int X, Y;
    unsigned char PixelAnt;
    unsigned char * LineAntPtr = LineAnt;

    Horizontal += 256;
    Vertical   += 256;
    Temporal   += 256;


    /* First pixel has no left nor top neighbour. Only previous frame */

    *LineAntPtr = PixelAnt = *Frame;
    *Frame++ = *FramePrev++ = LowPass(*FramePrev, *LineAntPtr, Temporal);
    LineAntPtr++;

    /* First line has no top neighbour. Only left one for each pixel and
     * last frame */

    for (X = 1; X < W; X++)
    {
        PixelAnt = LowPass(PixelAnt, *Frame, Horizontal);
        *LineAntPtr = PixelAnt;
        *Frame++ = *FramePrev++ = LowPass(*FramePrev, *LineAntPtr, Temporal);
	LineAntPtr++;
    }

    for (Y = 1; Y < H; Y++)
    {
	LineAntPtr = LineAnt;

        /* First pixel on each line doesn't have previous pixel */
        PixelAnt = *Frame;
        *LineAntPtr = LowPass(*LineAntPtr, PixelAnt, Vertical);
        *Frame++ = *FramePrev++ = LowPass(*FramePrev, *LineAntPtr, Temporal);
	LineAntPtr++;

        for (X = 1; X < W; X++)
        {
            /* The rest are normal */
            PixelAnt = LowPass(PixelAnt, *Frame, Horizontal);
            *LineAntPtr = LowPass(*LineAntPtr, PixelAnt, Vertical);
            *Frame++ = *FramePrev++ = LowPass(*FramePrev, *LineAntPtr, Temporal);
	    LineAntPtr++;
        }
    }
}

//===========================================================================//

static void PrecalcCoefs(int *Ct, double Dist25)
{
    int i;
    double Gamma, Simil, C;

    Gamma = log(0.25) / log(1.0 - Dist25/255.0);

    for (i = -256; i <= 255; i++)
    {
        Simil = 1.0 - (double)ABS(i) / 255.0;
        C = pow(Simil, Gamma) * (double)i;
        Ct[256+i] = (int)((C<0) ? (C-0.5) : (C+0.5));
    }
}

static void help_optstr()
{
    printf ("[%s] (%s) help\n", MOD_NAME, MOD_CAP);
    printf ("* Overview\n");
    printf ("  This filter aims to reduce image noise producing\n");
    printf ("  smooth images and making still images really still\n");
    printf ("  (This should enhance compressibility).\n");
    printf ("* Options\n");
    printf ("             luma : spatial luma strength (%f)\n", PARAM1_DEFAULT);
    printf ("           chroma : spatial chroma strength (%f)\n", PARAM2_DEFAULT);
    printf ("    luma_strength : temporal luma strength (%f)\n", PARAM3_DEFAULT);
    printf ("  chroma_strength : temporal chroma strength (%f)\n", 
	    PARAM3_DEFAULT/PARAM2_DEFAULT*PARAM1_DEFAULT);
    printf ("              pre : run as a pre filter (0)\n");
}

// main filter routine
int tc_filter(vframe_list_t *ptr, char *options) 
{

  static vob_t *vob=NULL;
  static MyFilterData *mfd[MAX_FILTER];
  static char *previous[MAX_FILTER];
  int instance = ptr->filter_id;

  if(ptr->tag & TC_AUDIO)
      return 0;

  if(ptr->tag & TC_FILTER_GET_CONFIG) {

      char buf[128];
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VYMOE", "2");

      snprintf(buf, 128, "%f", PARAM1_DEFAULT);
      optstr_param (options, "luma", "spatial luma strength", "%f", buf, "0.0", "100.0" );

      snprintf(buf, 128, "%f", PARAM2_DEFAULT);
      optstr_param (options, "chroma", "spatial chroma strength", "%f", buf, "0.0", "100.0" );

      snprintf(buf, 128, "%f", PARAM3_DEFAULT);
      optstr_param (options, "luma_strength", "temporal luma strength", "%f", buf, "0.0", "100.0" );

      snprintf(buf, 128, "%f", PARAM3_DEFAULT/PARAM2_DEFAULT*PARAM1_DEFAULT);
      optstr_param (options, "chroma_strength", "temporal chroma strength", "%f", buf, "0.0", "100.0" );
      snprintf(buf, 128, "%d", mfd[instance]->pre);
      optstr_param (options, "pre", "run as a pre filter", "%d", buf, "0", "1" );

      return 0;
  }

  if(ptr->tag & TC_FILTER_INIT) {

      double LumSpac, LumTmp, ChromSpac, ChromTmp;
      double Param1=0.0, Param2=0.0, Param3=0.0, Param4=0.0;
	  int ix;

      if((vob = tc_get_vob())==NULL) return(-1);
      
      if (vob->im_v_codec == CODEC_RGB) {
	  	fprintf(stderr, "[%s] This filter is only capable of YUV mode\n", MOD_NAME);
	  return -1;
      }

      if((mfd[instance] = malloc(sizeof(MyFilterData))))
      	memset(mfd[instance], 0, sizeof(MyFilterData));

      if (mfd[instance]) {
	  	if((mfd[instance]->Line = malloc(TC_MAX_V_FRAME_WIDTH*sizeof(int))))
	  		memset(mfd[instance]->Line, 0, TC_MAX_V_FRAME_WIDTH*sizeof(int));
      }

      if((previous[instance] = (char *)malloc(SIZE_RGB_FRAME)))
      	memset(previous[instance], 0, SIZE_RGB_FRAME);

      if(!mfd[instance] || !mfd[instance]->Line || !previous[instance]) {
  	fprintf(stderr, "[%s] Malloc failed\n", MOD_NAME);
          return -1;
      }

      // defaults

      LumSpac = PARAM1_DEFAULT;
      LumTmp = PARAM3_DEFAULT;

      ChromSpac = PARAM2_DEFAULT;
      ChromTmp = LumTmp * ChromSpac / LumSpac;

      if (options) {

	  if (optstr_lookup (options, "help")) {
	      help_optstr();
	  }

	  optstr_get (options, "luma",           "%lf",    &Param1);
	  optstr_get (options, "luma_strength",  "%lf",    &Param3);
	  optstr_get (options, "chroma",         "%lf",    &Param2);
	  optstr_get (options, "chroma_strength","%lf",    &Param4);
	  optstr_get (options, "pre", "%d",    &mfd[instance]->pre);

	  // recalculate only the needed params

	  if(Param1 != -1 && Param3 != -1)
	      enable_luma = 1;
	  else
	      enable_luma = 0;

	  if(Param2 != -1 && Param4 != -1)
	      enable_chroma = 1;
	  else
	      enable_chroma = 0;

	  if (Param1!=0.0) {

	      LumSpac = Param1;
	      LumTmp = PARAM3_DEFAULT * Param1 / PARAM1_DEFAULT;

	      ChromSpac = PARAM2_DEFAULT * Param1 / PARAM1_DEFAULT;
	      ChromTmp = LumTmp * ChromSpac / LumSpac;
	  } 
	  if (Param2!=0.0) {

	      ChromSpac = Param2;
	      ChromTmp = LumTmp * ChromSpac / LumSpac;
	  } 
	  if (Param3!=0.0) {

	      LumTmp = Param3;
	      ChromTmp = LumTmp * ChromSpac / LumSpac;

	  } 

	  if (Param4!=0.0) {
	      ChromTmp = Param4;
	  }
      }

      PrecalcCoefs(mfd[instance]->Coefs[0], LumSpac);
      PrecalcCoefs(mfd[instance]->Coefs[1], LumTmp);
      PrecalcCoefs(mfd[instance]->Coefs[2], ChromSpac);
      PrecalcCoefs(mfd[instance]->Coefs[3], ChromTmp);
      
      if(verbose) {
	  printf("[%s] %s %s #%d\n", MOD_NAME, MOD_VERSION, MOD_CAP, instance);
	  printf("[%s] Settings luma=%.2f chroma=%.2f luma_strength=%.2f chroma_strength=%.2f\n",
		  MOD_NAME, LumSpac, ChromSpac, LumTmp, ChromTmp);
	  printf("[%s] luma enabled: %s, chroma enabled: %s\n",
		  MOD_NAME, enable_luma ? "yes" : "no", enable_chroma ? "yes" : "no");
      }
      return 0;
  }

  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  
  if(ptr->tag & TC_FILTER_CLOSE) {

      if(previous[instance]) {free(previous[instance]); previous[instance]=NULL;}
      if(mfd[instance]) {
	  if(mfd[instance]->Line){free(mfd[instance]->Line);mfd[instance]->Line=NULL;}
	  free(mfd[instance]);
      }
      mfd[instance]=NULL;

      return(0);

  } /* filter close */

  //actually apply the filter

  if(((ptr->tag & TC_PRE_PROCESS  && mfd[instance]->pre) || 
	  (ptr->tag & TC_POST_PROCESS && !mfd[instance]->pre)) &&
	  !(ptr->attributes & TC_FRAME_IS_SKIPPED))
  {
	int yplane_size     = ptr->video_size * 4 / 6;
	int uplane_size     = ptr->video_size * 1 / 6;

	int yplane_offset   = 0;
	int u1plane_offset  = 0 + yplane_size;
	int u2plane_offset  = 0 + yplane_size + uplane_size;

	if(enable_luma)
	{
		deNoise(ptr->video_buf + yplane_offset,			// Frame
			previous[instance] + yplane_offset,		// FramePrev
			mfd[instance]->Line,				// LineAnt
			ptr->v_width,					// w
			ptr->v_height,					// h
			mfd[instance]->Coefs[0],			// horizontal
			mfd[instance]->Coefs[0],			// vertical
			mfd[instance]->Coefs[1]);			// temporal
	}

	if(enable_chroma)
	{
		deNoise(ptr->video_buf + u1plane_offset,		// Frame
			previous[instance] + u1plane_offset,		// FramePrev
			mfd[instance]->Line,				// LineAnt
			ptr->v_width >> 1,				// w
			ptr->v_height >> 1,				// h
			mfd[instance]->Coefs[2],			// horizontal
			mfd[instance]->Coefs[2],			// vertical
			mfd[instance]->Coefs[3]);			// temporal
	
		deNoise(ptr->video_buf + u2plane_offset,		// Frame
			previous[instance] + u2plane_offset,		// FramePrev
			mfd[instance]->Line,				// LineAnt
			ptr->v_width >> 1,				// w
			ptr->v_height >> 1,				// h
			mfd[instance]->Coefs[2],			// horizontal
			mfd[instance]->Coefs[2],			// vertical
			mfd[instance]->Coefs[3]);			// temporal
	}
  }

  return 0;
}

//===========================================================================//
