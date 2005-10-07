/*
    Copyright (C) 2002 Rémi Guyomarch <rguyom@pobox.com>

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

#define MOD_NAME      "filter_unsharp.so"
#define MOD_VERSION   "v1.0.1 (2003-10-27)"
#define MOD_CAP       "unsharp mask & gaussian blur"
#define MOD_AUTHOR    "Rémi Guyomarch"

#include "transcode.h"
#include "filter.h"
#include "optstr.h"

#include <math.h>
#ifndef SYS_BSD
# ifdef HAVE_MALLOC_H
# include <malloc.h>
# endif
#endif


#ifndef MIN
#define        MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define        MAX(a,b) (((a)>(b))?(a):(b))
#endif


//===========================================================================//

#define MIN_MATRIX_SIZE 3
#define MAX_MATRIX_SIZE 63

typedef struct FilterParam {
    int msizeX, msizeY;
    double amount;
    uint32_t *SC[MAX_MATRIX_SIZE-1];
    uint32_t *SC_unaligned[MAX_MATRIX_SIZE-1];
} FilterParam;

typedef struct vf_priv_s {
    FilterParam lumaParam;
    FilterParam chromaParam;
    int pre;
} MyFilterData;


static void *bufalloc(size_t size)
{

#ifdef HAVE_GETPAGESIZE
   long buffer_align=getpagesize();
#else
   long buffer_align=16;
#endif

   return malloc(size + buffer_align);
}

static void *bufalign(char *buf)
{

#ifdef HAVE_GETPAGESIZE
   long buffer_align=getpagesize();
#else
   long buffer_align=16;
#endif

   long adjust;

   if (buf == NULL) {
       fprintf(stderr, "(%s) out of memory", __FILE__);
   }
   
   adjust = buffer_align - ((long) buf) % buffer_align;

   if (adjust == buffer_align)
      adjust = 0;

   return (void *) (buf + adjust);
}


//===========================================================================//

/* This code is based on :

An Efficient algorithm for Gaussian blur using finite-state machines
Frederick M. Waltz and John W. V. Miller

SPIE Conf. on Machine Vision Systems for Inspection and Metrology VII
Originally published Boston, Nov 98

*/

static void unsharp( uint8_t *dst, uint8_t *src, int dstStride, int srcStride, int width, int height, FilterParam *fp ) {

    uint32_t **SC = fp->SC;
    uint32_t SR[MAX_MATRIX_SIZE-1], Tmp1, Tmp2;
    uint8_t* src2 = src; // avoid gcc warning

    int32_t res;
    int x, y, z;
    int amount = fp->amount * 65536.0;
    int stepsX = fp->msizeX/2;
    int stepsY = fp->msizeY/2;
    int scalebits = (stepsX+stepsY)*2;
    int32_t halfscale = 1 << ((stepsX+stepsY)*2-1);

    if( !fp->amount ) {
	if( src == dst )
	    return;
	if( dstStride == srcStride ) 
	    tc_memcpy( dst, src, srcStride*height );
	else
	    for( y=0; y<height; y++, dst+=dstStride, src+=srcStride )
		tc_memcpy( dst, src, width );
	return;
    }

    for( y=0; y<2*stepsY; y++ )
	memset( SC[y], 0, sizeof(SC[y][0]) * (width+2*stepsX) );

    for( y=-stepsY; y<height+stepsY; y++ ) {
	if( y < height ) src2 = src;
	memset( SR, 0, sizeof(SR[0]) * (2*stepsX-1) );
	for( x=-stepsX; x<width+stepsX; x++ ) {
	    Tmp1 = x<=0 ? src2[0] : x>=width ? src2[width-1] : src2[x];
	    for( z=0; z<stepsX*2; z+=2 ) {
		Tmp2 = SR[z+0] + Tmp1; SR[z+0] = Tmp1;
		Tmp1 = SR[z+1] + Tmp2; SR[z+1] = Tmp2;
	    }
	    for( z=0; z<stepsY*2; z+=2 ) {
		Tmp2 = SC[z+0][x+stepsX] + Tmp1; SC[z+0][x+stepsX] = Tmp1;
		Tmp1 = SC[z+1][x+stepsX] + Tmp2; SC[z+1][x+stepsX] = Tmp2;
	    }
	    if( x>=stepsX && y>=stepsY ) {
		uint8_t* srx = src - stepsY*srcStride + x - stepsX;
		uint8_t* dsx = dst - stepsY*dstStride + x - stepsX;
		
		res = (int32_t)*srx + ( ( ( (int32_t)*srx - (int32_t)((Tmp1+halfscale) >> scalebits) ) * amount ) >> 16 );
		*dsx = res>255 ? 255 : res<0 ? 0 : (uint8_t)res;
	    }
	}
	if( y >= 0 ) {
	    dst += dstStride;
	    src += srcStride;
	}
    }
}

//===========================================================================//

static void help_optstr(void)
{
    printf ("[%s] (%s) help\n", MOD_NAME, MOD_CAP);
    printf ("* Overview\n");
    printf ("  This filter blurs or sharpens an image depending on\n");
    printf ("  the sign of \"amount\". You can either set amount for\n");
    printf ("  both luma and chroma or you can set it individually\n");
    printf ("  (recommended). A positive value for amount will sharpen\n");
    printf ("  the image, a negative value will blur it. A sane range\n");
    printf ("  for amount is -1.5 to 1.5.\n");
    printf ("  The matrix sizes must be odd and define the\n");
    printf ("  range/strength of the effect. Sensible ranges are 3x3\n");
    printf ("  to 7x7.\n");
    printf ("  It sometimes makes sense to sharpen the sharpen the\n");
    printf ("  luma and to blur the chroma. Sample string is:\n");
    printf ("  \n");
    printf ("  luma=0.8:luma_matrix=7x5:chroma=-0.2:chroma_matrix=3x3\n");
    printf ("* Options\n");
    printf ("         amount : Luma and chroma (un)sharpness amount (%f)\n", 0.0);
    printf ("         matrix : Luma and chroma search matrix size (%dx%d)\n", 0, 0);
    printf ("           luma : Luma (un)sharpness amount (%02.2f)\n", 0.0);
    printf ("         chroma : Chroma (un)sharpness amount (%02.2f)\n", 0.0);
    printf ("    luma_matrix : Luma search matrix size (%dx%d)\n", 0, 0);
    printf ("  chroma_matrix : Chroma search matrix size (%dx%d)\n", 0, 0);
    printf ("              pre : run as a pre filter (0)\n");
    printf ("\n");
}

//===========================================================================//

int tc_filter(frame_list_t *ptr_, char *options)
{
  vframe_list_t *ptr = (vframe_list_t *)ptr_;
  static vob_t *vob=NULL;
  static MyFilterData *mfd=NULL;
  static char *buffer;

  if(ptr->tag & TC_AUDIO) return 0;

  if(ptr->tag & TC_FILTER_GET_CONFIG) {

      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VYO", "1");

      optstr_param (options, "amount", "Luma and chroma (un)sharpness amount", "%f", "0.0", "-2.0", "2.0" );
      optstr_param (options, "matrix", "Luma and chroma search matrix size", "%dx%d", "0x0",
	      "3", "63", "3", "63");

      optstr_param (options, "luma", "Luma (un)sharpness amount", "%f", "0.0", "-2.0", "2.0" );

      optstr_param (options, "chroma", "Chroma (un)sharpness amount", "%f", "0.0", "-2.0", "2.0" );

      optstr_param (options, "luma_matrix", "Luma search matrix size", "%dx%d", "0x0",
	      "3", "63", "3", "63");

      optstr_param (options, "chroma_matrix", "Chroma search matrix size", "%dx%d", "0x0",
	      "3", "63", "3", "63");

      optstr_param (options, "pre", "run as a pre filter", "%d", "0", "0", "1" );

      return 0;
  }

  //----------------------------------
  //
  // filter init
  //
  //----------------------------------


  if(ptr->tag & TC_FILTER_INIT) {

    int width, height;
    int z, stepsX, stepsY;
    FilterParam *fp;
    char *effect;
    double amount=0.0;
    int msizeX=0, msizeY=0;

    if((vob = tc_get_vob())==NULL) return(-1);
    	
    if (vob->im_v_codec != CODEC_YUV) {
	fprintf(stderr, "[%s] This filter is only capable of YUV mode\n", MOD_NAME);
	return -1;
    }

    mfd   = malloc( sizeof(MyFilterData) );
    memset( mfd, 0, sizeof(MyFilterData) );
    buffer = (char *)malloc(SIZE_RGB_FRAME);
    memset(buffer, 0, SIZE_RGB_FRAME);


    // GET OPTIONS
    if (options) {

	// l7x5:0.8:c3x3:-0.2

	if (optstr_lookup (options, "help")) {
	    help_optstr();
	}

	optstr_get (options, "amount",         "%lf",   &amount);
	optstr_get (options, "matrix",         "%dx%d", &msizeX, &msizeY);
	optstr_get (options, "luma",           "%lf",   &mfd->lumaParam.amount);
	optstr_get (options, "luma_matrix",    "%dx%d", &mfd->lumaParam.msizeX, &mfd->lumaParam.msizeY);
	optstr_get (options, "chroma",         "%lf",   &mfd->chromaParam.amount);
	optstr_get (options, "chroma_matrix",  "%dx%d", &mfd->chromaParam.msizeX, &mfd->chromaParam.msizeY);
	optstr_get (options, "pre",            "%d",    &mfd->pre);

	if (amount!=0.0 && msizeX && msizeY) {

	    msizeX = 1 | MIN( MAX( msizeX, MIN_MATRIX_SIZE), MAX_MATRIX_SIZE );
	    msizeY = 1 | MIN( MAX( msizeY, MIN_MATRIX_SIZE), MAX_MATRIX_SIZE );
	    mfd->lumaParam.msizeX = msizeX;
	    mfd->lumaParam.msizeY = msizeY;
	    mfd->chromaParam.msizeX = msizeX;
	    mfd->chromaParam.msizeY = msizeY;

	    mfd->lumaParam.amount = mfd->chromaParam.amount = amount;

	} else {
    
	    // min/max & odd
	    mfd->lumaParam.msizeX   = 1 | MIN( MAX( mfd->lumaParam.msizeX,   MIN_MATRIX_SIZE ), MAX_MATRIX_SIZE );
	    mfd->lumaParam.msizeY   = 1 | MIN( MAX( mfd->lumaParam.msizeY,   MIN_MATRIX_SIZE ), MAX_MATRIX_SIZE );
	    mfd->chromaParam.msizeX = 1 | MIN( MAX( mfd->chromaParam.msizeX, MIN_MATRIX_SIZE ), MAX_MATRIX_SIZE );
	    mfd->chromaParam.msizeY = 1 | MIN( MAX( mfd->chromaParam.msizeY, MIN_MATRIX_SIZE ), MAX_MATRIX_SIZE );
	}
    }

    if (mfd->pre) {
	width  = vob->im_v_width;
	height = vob->im_v_height;
    } else {
	width  = vob->ex_v_width;
	height = vob->ex_v_height;
    }
    
    // allocate buffers

    fp = &mfd->lumaParam;
    effect = fp->amount == 0 ? "don't touch" : fp->amount < 0 ? "blur" : "sharpen";
    fprintf(stderr, "[%s] unsharp: %dx%d:%0.2f (%s luma)\n", MOD_NAME, fp->msizeX, fp->msizeY, fp->amount, effect );
    memset( fp->SC, 0, sizeof( fp->SC ) );
    stepsX = fp->msizeX/2;
    stepsY = fp->msizeY/2;
    for( z=0; z<2*stepsY; z++ )
        {
	fp->SC_unaligned[z] = bufalloc(sizeof(*(fp->SC[z])) * (width+2*stepsX) );
	fp->SC[z] = bufalign((char *)(fp->SC_unaligned[z]));
        }

    fp = &mfd->chromaParam;
    effect = fp->amount == 0 ? "don't touch" : fp->amount < 0 ? "blur" : "sharpen";
    fprintf(stderr, "[%s] unsharp: %dx%d:%0.2f (%s chroma)\n", MOD_NAME, fp->msizeX, fp->msizeY, fp->amount, effect );
    memset( fp->SC, 0, sizeof( fp->SC ) );
    stepsX = fp->msizeX/2;
    stepsY = fp->msizeY/2;
    for( z=0; z<2*stepsY; z++ )
        {
	fp->SC_unaligned[z] = bufalloc( sizeof(*(fp->SC[z])) * (width+2*stepsX) );
	fp->SC[z] = bufalign((char *)(fp->SC_unaligned[z]));
        }


    if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);
    return 0;
  }


  if (ptr->tag & TC_FILTER_CLOSE) {
      unsigned int z;
      FilterParam *fp;

      if( !mfd ) return -1;

      fp = &mfd->lumaParam;
      for( z=0; z<sizeof(fp->SC)/sizeof(fp->SC[0]); z++ ) {
	  if( fp->SC_unaligned[z] ) free( fp->SC_unaligned[z] );
	  fp->SC_unaligned[z] = NULL;
	  fp->SC[z] = NULL;
      }
      fp = &mfd->chromaParam;
      for( z=0; z<sizeof(fp->SC)/sizeof(fp->SC[0]); z++ ) {
	  if( fp->SC_unaligned[z] ) free( fp->SC_unaligned[z] );
	  fp->SC_unaligned[z] = NULL;
	  fp->SC[z] = NULL;
      }

      free( mfd );
      mfd = NULL;
      return 0;
  }


  if( mfd && !mfd->lumaParam.msizeX && !mfd->chromaParam.msizeX ) {
      return 0; // nothing to do
  }


  if(((ptr->tag & TC_PRE_PROCESS  && mfd->pre) || 
	  (ptr->tag & TC_POST_PROCESS && !mfd->pre)) &&
	  !(ptr->attributes & TC_FRAME_IS_SKIPPED)) {

      int off = ptr->v_width * ptr->v_height;
      int h2  = ptr->v_height>>1, w2 = ptr->v_width>>1;

      tc_memcpy (buffer, ptr->video_buf, ptr->video_size);

      unsharp( ptr->video_buf, buffer, ptr->v_width, ptr->v_width, ptr->v_width,   ptr->v_height,   &mfd->lumaParam );

      unsharp( ptr->video_buf+off, buffer+off, w2, w2, w2, h2, &mfd->chromaParam );

      unsharp( ptr->video_buf+5*off/4, buffer+5*off/4, w2, w2, w2, h2, &mfd->chromaParam );

      return 0;
  }

  return 0;
    
} // tc_filter

