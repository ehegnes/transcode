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
#define MOD_VERSION "0.1.4 (2003-10-13)"
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

//#undef HAVE_MMX
//#undef CAN_COMPILE_C_ALTIVEC

// mmx gives a speedup of about 3 fps
// when running without highq, mmx gives 12 fps

// altivec does not give much, about 1 fps

#ifdef HAVE_MMX
# include "mmx.h"
#endif

#ifndef HAVE_MMX
# define emms() do{}while(0)
#endif

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))


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

// We pad the moving maps with 16 pixels left and right, to make sure
// that we always can do aligned loads and stores at a multiple of 16.
// this is especially important when doing altivec but might help in 
// other cases as well.
#define PAD 32

static unsigned char clamp_Y(unsigned char x) {
	return (x>MAX_Y?MAX_Y:(x<MIN_Y?MIN_Y:x));
}
static unsigned char clamp_UV(unsigned char x) {
	return (x);
}


/*
size: 1-32
count: 1-256
stride: -32000 - 320000
*/

#if 0 // unused, EMS
static unsigned int get_prefetch_const (int blocksize_in_vectors, int block_count, int block_stride) 
{
    return ((blocksize_in_vectors<<24)&0x1F000000) | 
	   ((block_count<<16)&0x00FF0000)          |
	    (block_stride&0xFFFF);
}
#endif

static unsigned char *bufalloc(size_t size, char *location)
{

#ifdef HAVE_GETPAGESIZE
   long buffer_align=getpagesize();
#else
   long buffer_align=16;
#endif

   char *buf = malloc(size + buffer_align);

   long adjust;

   if (buf == NULL) {
       fprintf(stderr, "(%s) out of memory", __FILE__);
   }

   location = buf;
   
   adjust = buffer_align - ((long) buf) % buffer_align;

   if (adjust == buffer_align)
      adjust = 0;

   return (unsigned char *) (buf + adjust);
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

	// These are the locations of the real malloc buffers
        char                    *Rbuf;
	char			*RprevFrame;
	unsigned char		*RmovingY;
	unsigned char		*RmovingU;
	unsigned char		*RmovingV;
	unsigned char		*RfmovingY;
	unsigned char		*RfmovingU;
	unsigned char		*RfmovingV;
} MyFilterData;

static MyFilterData *mfd;

static void help_optstr(void) 
{
   printf ("[%s] (%s) help\n", MOD_NAME, MOD_CAP);
   printf ("* Overview\n");
   printf ("   This filter is basically a rewrite of the\n");
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
   printf ("   -J smartyuv=highq=1:diffmode=2:cubic=1:Blend=1:chromathres=4:threshold=8:doChroma=1\n");
   printf ("* Options\n");
   printf ("  'motionOnly' Show motion areas only (0=off, 1=on) [0]\n");
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
static void Erode_Dilate (uint8_t *_moving, uint8_t *_fmoving, int width, int height)
{
    int sum, x, y;
    uint8_t  *m, *fmoving, *moving, *p;
    int w4 = width+PAD;
    int can_use_mmx = !(width%4);

    // Erode.
    fmoving = _fmoving;
    moving = _moving;
    p = moving - 2*w4 -2;

    for (y = 0; y < height; y++)
    {
#ifdef HAVE_MMX
	/*
	 * The motion map as either 1 or 0.
	 * moving[x] is the current position.
	 * to decide if fmoving[x] should be 1, we need to sum up all 24 values.
	 * Because of mmx, we can do that also with the next 3 positions since
	 * the values are read in memory anyway.
	 */

	if (can_use_mmx) {
	    for (x = 0; x < width; x+=4)
	    {
		uint8_t  res[8];

		tc_memcpy(fmoving, moving, 4);

		m = p;

		movq_m2r   (*m, mm0); m += w4;
		paddusb_m2r(*m, mm0); m += w4;
		paddusb_m2r(*m, mm0); m += w4;
		paddusb_m2r(*m, mm0); m += w4;
		paddusb_m2r(*m, mm0);

		movq_r2m(mm0, *res);

		if (*moving++) {
		    res[0]+=res[1];
		    res[0]+=res[2];
		    res[0]+=res[3];
		    res[0]+=res[4];
		    *fmoving = (res[0] > 7);
		}
		fmoving++;

		if (*moving++) {
		    res[1]+=res[2];
		    res[1]+=res[3];
		    res[1]+=res[4];
		    res[1]+=res[5];
		    *fmoving = (res[1] > 7);
		}
		fmoving++;

		if (*moving++) {
		    res[2]+=res[3];
		    res[2]+=res[4];
		    res[2]+=res[5];
		    res[2]+=res[6];
		    *fmoving = (res[2] > 7);
		}
		fmoving++;

		if (*moving++) {
		    res[3]+=res[4];
		    res[3]+=res[5];
		    res[3]+=res[6];
		    res[3]+=res[7];
		    *fmoving = (res[3] > 7);
		}
		fmoving++;

		p += 4;

	    }
	    fmoving += PAD;
	    moving += PAD;
	    p += PAD;
	} else 
#endif
	{
	    for (x = 0; x < width; x++)
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

	} // else can use mmx

    }
    emms();


    // Dilate.
    fmoving = _fmoving;
    moving = _moving;
    for (y = 0; y < height; y++)
    {
	for (x = 0; x < width; x++)
	{
	    if ((moving[x] = fmoving[x])) {

		m = moving + x - 2*w4 -2;

		memset(m, 1, 5);
		m += w4;
		memset(m, 1, 5);
		m += w4;
		memset(m, 1, 5);
		m += w4;
		memset(m, 1, 5);
		m += w4;
		memset(m, 1, 5);
	    }
	}
	moving += w4;
	fmoving += w4;
    }
}
static void inline Blendline_c (uint8_t *dst, uint8_t *src, uint8_t *srcminus, uint8_t *srcplus, 
	                 uint8_t *moving, uint8_t *movingminus, uint8_t *movingplus, const int w, const int scenechange)
{
    int	x=0;

    do {
	if (movingminus[x] | moving[x] | movingplus[x] | scenechange)
	    /* Blend fields. */
	    dst[x] = ((src[x]>>1) + (srcminus[x]>>2) + (srcplus[x]>>2)) & 0xff;
	else
	{
	    dst[x] = src[x];
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
	unsigned char		*fmoving;
	char    		*prev;
	int			scenechange=0;
	long			count=0;
	int			x, y;
	int			luma, luman, lumap, T;
	int 			p1, p2;
	int 			rp, rn, rpp, rnn, R;
	unsigned char		fiMotion;
	int			cubic = mfd->cubic;
	static int 		counter=0;
	const int		can_use_mmx = !(w%8); // width must a multiple of 8
	// const int		can_use_altivec = !(w%16); // width must a multiple of 16


	char * dst_buf;
	char * src_buf;

	//memset(ptr->video_buf+h*w, BLACK_BYTE_UV, h*w/2);
	src_buf = _src;
	dst_buf = _dst;

	/* Not much deinterlacing to do if there aren't at least 2 lines. */
	if (h < 2) return;

	/* Skip first and last lines, they'll get a free ride. */
	src = src_buf + srcpitch;
	srcminus = src - srcpitch;
	srcplus = src + srcpitch;
	moving = _moving + w+PAD;
	prev = _prev + w;
	//printf("Aligned src %p prev %p moving %p\n", src, prev, moving);

	if (mfd->diffmode == FRAME_ONLY || mfd->diffmode == FRAME_AND_FIELD)
	{
		if (mfd->diffmode == FRAME_ONLY) {

#ifdef HAVE_MMX
		  if (can_use_mmx) {

		    uint64_t mask1 = 0x00FF00FF00FF00FFULL;

		    uint64_t thres = (_threshold<<16) | (_threshold);
		    thres = (thres << 32) | (thres);

		    movq_m2r (mask1, mm6);
		    movq_m2r (thres, mm5);        // thres -> mm6

		    count = 0;
		    for (y = 1; y < hminus1; y++)
		    {
			for (x=0; x<w; x+=4) {

			    movd_m2r (*src, mm0);         // a b c d 0 0 0 0

			    punpcklbw_r2r (mm0, mm0);     // a a b b c c d d
			    pand_r2r (mm6, mm0);          // 0 a 0 b 0 c 0 d

			    movd_m2r(*prev, mm1);         // e f g h 0 0 0 0

			    punpcklbw_r2r (mm1, mm1);     // e e f f g g h h
			    pand_r2r (mm6, mm1);          // 0 e 0 f 0 g 0 h

			    psubsw_r2r(mm1, mm0);         // mm0 = mm0 - mm1; !!

			    movq_r2r(mm0, mm3);

			    // abs()
			    psraw_i2r(15, mm3);
			    pxor_r2r(mm3, mm0);
			    psubw_r2r(mm3, mm0);

			    // compare if greater than thres
			    pcmpgtw_r2r(mm5, mm0);

			    // norm
			    psrlw_i2r(15, mm0);
			    // pack to bytes
			    packuswb_r2r(mm0, mm0);

			    // write to moving
			    movd_r2m(mm0, *moving);

			    tc_memcpy(prev, src, 4);

			    src+=4;
			    prev+=4;

			    count += *moving++;
			    count += *moving++;
			    count += *moving++;
			    count += *moving++;

			}

			moving += PAD;
		    }
		    emms();

		  }  else  // cannot use mmx
#elif CAN_COMPILE_C_ALTIVEC
		  if (can_use_altivec) {
#if 0
		      typedef union {
			  vector unsigned char v;
			  unsigned char e[16];
		      } uchar_t;
		      uchar_t res;
		      //printf("mov: "); for (i=0; i<16;i++) printf("%02x ", res.e[i]&0xff);  printf("\n");
#endif

		      vector unsigned char vthres;
		      vector unsigned char shift = vec_splat_u8(7);
		      unsigned char __attribute__ ((aligned(16))) tdata[16];
		      int i;
		      memset(tdata, _threshold, 16);
		      vthres = vec_ld(0, tdata);

		      count = 0;
		      for (y = 1; y < hminus1; y++)
		      {
			  for (x=0; x<w; x+=16) {

			      vector unsigned char luma = vec_ld(0, (unsigned char *)src);
			      vector unsigned char prv = vec_ld(0, (unsigned char *)prev);
			      vector unsigned char vmov;
			      vmov = vec_sub (vec_max (luma, prv), vec_min(luma, prv));

			      // FF -> 01
			      vmov = (vector unsigned char)vec_cmpgt(vmov, vthres);
			      vmov = vec_sr(vmov, shift);

			      vec_st(vmov, 0, (unsigned char *)moving);

			      /* Keep a count of the number of moving pixels for the
				 scene change detection. */
			      for (i=0; i<16; i++) {
				  count += *moving++;
				  *prev++ = *src++;
			      }

			  }

			  moving += PAD;
		    }
#if 0
		    {
			FILE *f = fopen("mov.dat", "w");
			printf ("COUNT %d size1 (%d) size2 (%d)\n", count, sizeof (*moving), sizeof (unsigned char));
			fwrite (_moving, h*w, 1, f);
			fclose (f);
			exit(0);
		    }
#endif


		      
		  } else
#endif
		  {
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

			moving += PAD;
		    }
		  } // cannot use mmx

		} else if (mfd->diffmode == FRAME_AND_FIELD) {

#ifdef HAVE_MMX
		  if (can_use_mmx) {

		    uint64_t mask1 = 0x00FF00FF00FF00FFULL;

		    uint64_t thres = (_threshold<<16) | (_threshold);
		    thres = (thres << 32) | (thres);

		    movq_m2r (mask1, mm6);
		    movq_m2r (thres, mm5);        // thres -> mm6

		    // ---------------------
		    // create the motion map
		    // ---------------------

		    count = 0;
		    for (y = 1; y < hminus1; y++)
		    {
			if (y & 1) { // odd lines

			    for (x=0; x<w; x+=4) {

				movd_m2r(*src, mm0);          // a b c d 0 0 0 0
				movd_m2r(*srcminus, mm1); // e f g h 0 0 0 0
				movd_m2r(*prev, mm2);

				punpcklbw_r2r (mm0, mm0);     // a a b b c c d d
				punpcklbw_r2r (mm1, mm1);     // e e f f g g h h
				punpcklbw_r2r (mm2, mm2);
				pand_r2r (mm6, mm0);          // 0 a 0 b 0 c 0 d
				pand_r2r (mm6, mm1);          // 0 e 0 f 0 g 0 h
				pand_r2r (mm6, mm2);

				movq_r2r (mm0, mm7);          // save in mm7

				psubsw_r2r(mm1, mm0);         // mm0 = mm0 - mm1; !!
				psubsw_r2r(mm2, mm7);
				movq_r2r(mm0, mm3);
				movq_r2r(mm7, mm4);

				// abs() ((mm0^(mm0>>15))-(mm0>>15))

				psraw_i2r(15, mm3);
				psraw_i2r(15, mm4);
				pxor_r2r(mm3, mm0);
				pxor_r2r(mm4, mm7);
				psubw_r2r(mm3, mm0);
				psubw_r2r(mm4, mm7);

				pcmpgtw_r2r(mm5, mm0);     //compare if greater than thres
				pcmpgtw_r2r(mm5, mm7);
				psrlw_i2r(15, mm0);       // norm
				psrlw_i2r(15, mm7);       // norm
				packuswb_r2r(mm0, mm0);   // pack to bytes
				packuswb_r2r(mm7, mm7);   // pack to bytes

				// mm0: result first compare
				// mm1-mm4: free
				// mm5: threshold
				// mm6: mask
				// mm7: copy of src

				pand_r2r(mm7, mm0);

				// write to moving
				movd_r2m(mm0, *moving);

				tc_memcpy(prev, src, 4);

				src+=4;
				prev+=4;
				srcminus+=4;

				count += *moving++;
				count += *moving++;
				count += *moving++;
				count += *moving++;

			    }

			} else { // even lines

			    for (x=0; x<w; x+=4) {
				movd_m2r(*src, mm0);         // a b c d 0 0 0 0
				movd_m2r(*(prev+w), mm1);    // e f g h 0 0 0 0
				movd_m2r(*prev, mm2);

				punpcklbw_r2r (mm0, mm0);     // a a b b c c d d
				punpcklbw_r2r (mm1, mm1);     // e e f f g g h h
				punpcklbw_r2r (mm2, mm2);
				pand_r2r (mm6, mm0);          // 0 a 0 b 0 c 0 d
				pand_r2r (mm6, mm1);          // 0 e 0 f 0 g 0 h
				pand_r2r (mm6, mm2);

				movq_r2r (mm0, mm7);          // save in mm7

				psubsw_r2r(mm1, mm0);         // mm0 = mm0 - mm1; !!
				psubsw_r2r(mm2, mm7);
				movq_r2r(mm0, mm3);
				movq_r2r(mm7, mm4);

				// abs() ((mm0^(mm0>>15))-(mm0>>15))

				psraw_i2r(15, mm3);
				psraw_i2r(15, mm4);
				pxor_r2r(mm3, mm0);
				pxor_r2r(mm4, mm7);
				psubw_r2r(mm3, mm0);
				psubw_r2r(mm4, mm7);

				pcmpgtw_r2r(mm5, mm0);     //compare if greater than thres
				pcmpgtw_r2r(mm5, mm7);
				psrlw_i2r(15, mm0);       // norm
				psrlw_i2r(15, mm7);       // norm
				packuswb_r2r(mm0, mm0);   // pack to bytes
				packuswb_r2r(mm7, mm7);   // pack to bytes

				// mm0: result first compare
				// mm1-mm4: free
				// mm5: threshold
				// mm6: mask
				// mm7: copy of src

				pand_r2r(mm7, mm0);

				// write to moving
				movd_r2m(mm0, *moving);

				tc_memcpy(prev, src, 4);

				src+=4;
				prev+=4;

				count += *moving++;
				count += *moving++;
				count += *moving++;
				count += *moving++;

			    }

			}
			srcminus += srcpitch;
			moving += PAD;
		    }

		    emms();

		  } else // cannot use mmx
#elif CAN_COMPILE_C_ALTIVEC_FIXME_BROKEN
		  if (can_use_altivec) {

		    vector unsigned char vthres;
		    vector unsigned char shift = vec_splat_u8(7);
		    unsigned char __attribute__ ((aligned(16))) tdata[16];
		    int i;
		    memset(tdata, _threshold, 16);
		    vthres = vec_ld(0, tdata);

		    count = 0;
		    //printf ("Align: %p %p %p\n", src, srcminus, prev);
		    for (y = 1; y < hminus1; y++)
		    {
			if (y & 1) { // odd lines

			    for (x=0; x<w; x+=16) {

				vector unsigned char luma = vec_ld(0, (unsigned char *)src);
				vector unsigned char p0 = vec_ld(x, (unsigned char *)srcminus);
				vector unsigned char p1 = vec_ld(0, (unsigned char *)prev);
				vector unsigned char vmov;

				p0 = vec_sub (vec_max (luma, p0), vec_min (luma, p0));
				p1 = vec_sub (vec_max (luma, p1), vec_min (luma, p1));
				p0 = (vector unsigned char)vec_cmpgt(p0, vthres);
				p1 = (vector unsigned char)vec_cmpgt(p1, vthres);
				vmov = vec_and(p0, p1);

				// FF -> 01
				vmov = vec_sr(vmov, shift);

				vec_st(vmov, 0, (unsigned char *)moving);

				/* Keep a count of the number of moving pixels for the
				   scene change detection. */
				for (i=0; i<16; i++) {
				    count += *moving++;
				    *prev++ = *src++;
				}

			    }

			} else { // even lines

			    for (x=0; x<w; x+=16) {

				vector unsigned char luma = vec_ld(0, (unsigned char *)src);
				vector unsigned char p0 = vec_ld(w, (unsigned char *)prev);
				vector unsigned char p1 = vec_ld(0, (unsigned char *)prev);
				vector unsigned char vmov;

				p0 = vec_sub (vec_max (luma, p0), vec_min (luma, p0));
				p1 = vec_sub (vec_max (luma, p1), vec_min (luma, p1));
				vmov = vec_and(
					(vector unsigned char)vec_cmpgt(p0, vthres), 
					(vector unsigned char)vec_cmpgt(p1, vthres));

				// FF -> 01
				vmov = vec_sr(vmov, shift);

				vec_st(vmov, 0, (unsigned char *)moving);

				/* Keep a count of the number of moving pixels for the
				   scene change detection. */
				for (i=0; i<16; i++) {
				    count += *moving++;
				    *prev++ = *src++;
				}
			    }
			} // odd vs. even

			srcminus += srcpitch;
			moving += PAD;

		    } // height

		    printf ("COUNT %d|\n", count);

		  } else
#endif
		  {
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

			moving += PAD;
			srcminus += srcpitch;
		    }
		    //printf ("XXXCOUNT %d|\n", count);
		  }
		}

		/* Determine whether a scene change has occurred. */
		if ((100L * count) / (h * w) >= mfd->scenethreshold) scenechange = 1;
		else scenechange = 0;

		if (scenechange && mfd->verbose) 
		    printf("[%s] Scenechange at %6d (%6ld moving pixels)\n", MOD_NAME, counter, count);
		/*
		printf("Frame (%04d) count (%8ld) sc (%d) calc (%02ld)\n", 
				counter, count, scenechange, (100 * count) / (h * w));
				*/


		/* Perform a denoising of the motion map if enabled. */
		if (!scenechange && mfd->highq)
		{
		    //uint64_t before = 0;
		    //uint64_t after = 0;
		    //rdtscll(before);

		    Erode_Dilate(_moving, _fmoving, w, h);

		    //rdtscll(after);
		    //printf("%6d : %8lld\n", count, after-before);
		}
	}
	if (mfd->diffmode == FIELD_ONLY) {

		/* Field differencing only mode. */
		T = _threshold * _threshold;
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
			moving += (w+PAD);
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
					if (!((_moving + y * (w+PAD))[x]))
					{
						fmoving[x] = 0;	
						continue;
					}
					xlo = x - Nover2; if (xlo < 0) xlo = 0;
					xhi = x + Nover2; if (xhi >= w) xhi = wminus1;
					ylo = y - Nover2; if (ylo < 0) ylo = 0;
					yhi = y + Nover2; if (yhi >= h) yhi = hminus1;
					m = _moving + ylo * (w+PAD);
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
				fmoving += (w+PAD);
			}

			// Dilate.
			N = 5;
			Nover2 = N/2;
			moving = _moving;
			for (y = 0; y < h; y++)
			{
				for (x = 0; x < w; x++)
				{
					if (!((_fmoving + y * (w+PAD))[x]))
					{
						moving[x] = 0;	
						continue;
					}
					xlo = x - Nover2; if (xlo < 0) xlo = 0;
					xhi = x + Nover2; if (xhi >= w) xhi = wminus1;
					ylo = y - Nover2; if (ylo < 0) ylo = 0;
					yhi = y + Nover2; if (yhi >= h) yhi = hminus1;
					m = _moving + ylo * (w+PAD);
					for (u = ylo; u <= yhi; u++)
					{
						for (v = xlo; v <= xhi; v++)
						{
							m[v] = 1;
						}
						m += (w+PAD);
					}
				}
				moving += (w+PAD);
			}		
		}
	}

	// -----------------
	// Render.
	// -----------------

	// The first line gets a free ride.
	src = src_buf;
	dst = dst_buf;

	tc_memcpy(dst, src, w);
	src = src_buf + srcpitch;
	srcminus = src - srcpitch;
	srcplus = src + srcpitch;

	if (cubic)
	{
		srcminusminus = src - 3 * srcpitch;
		srcplusplus = src + 3 * srcpitch;
	}

	dst = dst_buf + dstpitch;
	moving = _moving + w+PAD;
	movingminus = _moving;
	movingplus = moving + w+PAD;

	/*
	*/

	if (mfd->motionOnly)
	{
	    for (y = 1; y < hminus1; y++)
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
		src = src + srcpitch;
		srcminus = srcminus + srcpitch;
		srcplus = srcplus + srcpitch;

		if (cubic)
		{
		    srcminusminus = srcminusminus + srcpitch;
		    srcplusplus = srcplusplus + srcpitch;
		}

		dst = dst + dstpitch;
		moving += (w+PAD);
		movingminus += (w+PAD);
		movingplus += (w+PAD);
	    }
	    // The last line gets a free ride.
	    tc_memcpy(dst, src, w);

	    if (clamp_f == clamp_Y)
		counter++;

	    return;

	}
	
	if (mfd->Blend)
	{
	    // linear blend, see Blendline_c for a plainC version
	    for (y = 1; y < hminus1; y++)
	    {
#ifdef HAVE_MMX
	      if (can_use_mmx) {

		uint64_t scmask = (scenechange<<24) | (scenechange<<16) | (scenechange<<8) | scenechange;
		scmask = (scmask << 32) | scmask;

		pcmpeqw_r2r(mm4, mm4);
		psrlw_i2r(9,mm4);
		packuswb_r2r(mm4, mm4);         // build 0x7f7f7f7f7f7f7f7f

		pcmpeqw_r2r(mm6, mm6);
		psrlw_i2r(10,mm6);
		packuswb_r2r(mm6, mm6);         // build 0x3f3f3f3f3f3f3f3f

		for (x=0; x<w; x+=8) {

		    movq_m2r(scmask, mm0);          // has a scenechange happend?

		    pxor_r2r(mm5, mm5);             // clear mm5

		    por_m2r (moving     [x], mm0);
		    movq_m2r(src        [x], mm1);   // load src
		    por_m2r (movingminus[x], mm0);   // motion detected?
		    movq_m2r(src        [x-w], mm2);   // load srcminus
		    por_m2r (movingplus [x], mm0);
		    movq_m2r(src        [x+w], mm3);   // load srcplus

		    movq_r2r (mm1, mm7);

		    pcmpgtb_r2r(mm5, mm0);  // make FF out 1 and 0 out of 0

		    pcmpeqw_r2r(mm5, mm5);  // make all ff's (recycle mm5)
		    psubb_r2r  (mm0, mm5);  // inverse mask
		    pand_r2r   (mm0, mm7); 
		    pand_r2r   (mm5, mm1); 
		    psrlw_i2r  (1,   mm7);

		    pand_r2r   (mm4, mm7);  // clear highest bit
		    por_r2r    (mm7, mm1);  // merge src>>1 and src together dependand on moving mask

		    // mm0: mask, if 0 don't shift, if ff shift
		    // mm1: complete src
		    // mm2: srcminus
		    // mm3: srcplus
		    // mm4: 0x7f mask
		    // mm5: free
		    // mm6: 0x3f mask
		    // mm7: free

		    // handle srcm(inus) and srcp(lus)

		    pand_r2r (mm0, mm2);
		    pand_r2r (mm0, mm3);

		    psrlw_i2r(2,   mm2);  // srcm>>2
		    psrlw_i2r(2,   mm3);  // srcp>>2
		    pand_r2r (mm6, mm2);  // clear highest two bits
		    pand_r2r (mm6, mm3);

		    paddusb_r2r (mm2, mm1);   // src>>1 + srcn>>2 + srcp>>2
		    paddusb_r2r (mm3, mm1);

		    movq_r2m(mm1, dst[x]);

		}
	      } else // cannot use mmx
#elif CAN_COMPILE_C_ALTIVEC
	      if (can_use_altivec) {
		  unsigned char tdata[16];
		  memset (tdata, scenechange, 16);
		  vector unsigned char vscene = vec_ld(0, tdata);
		  vector unsigned char vmov, vsrc2, vdest;
		  vector unsigned char vsrc, vsrcminus, vsrcplus;
		  vector unsigned char zero = vec_splat_u8(0);
		  vector unsigned char ones = vec_splat_u8(1);
		  vector unsigned char twos = vec_splat_u8(2);


		  for (x=0; x<w; x+=16) {
		      vmov = vec_xor(vmov, vmov);
		      vmov = vec_or (vmov, vec_ld(x, moving));
		      vsrc = vec_ld(x, (unsigned char *)src);
		      vmov = vec_or (vmov, vec_ld(x, movingminus));
		      vsrcminus = vec_ld(x-w, (unsigned char *)src);
		      vmov = vec_or (vmov, vec_ld(x, movingplus));
		      vsrcplus = vec_ld(x+w, (unsigned char *)src);
		      vmov = vec_or(vmov, vscene);

		      vsrc2 = vec_sr(vsrc, ones);
		      vsrc2 = vec_add(vsrc2, vec_sr(vsrcminus, twos));
		      vsrc2 = vec_add(vsrc2, vec_sr(vsrcplus, twos));
		      vmov = (vector unsigned char)vec_cmpgt (vmov, zero);
		 vdest = vec_or (vec_sel(vsrc, zero, vmov), vec_sel (vsrc2, zero, vec_nor(vmov, vmov)));
		      vec_st(vdest, x, (unsigned char *)dst);
		  }





	      } else 
#endif
	      {
		Blendline_c (dst, src, srcminus, srcplus, moving, movingminus, movingplus, w, scenechange);
	      }

		src +=  srcpitch;
		srcminus += srcpitch;
		srcplus += srcpitch;

		dst += dstpitch;
		moving += (w+PAD);
		movingminus += (w+PAD);
		movingplus += (w+PAD);
	    }

	    emms();
	    return;
	}

	emms();

	// Doing line interpolate. Thus, even lines are going through
	// for moving and non-moving mode. Odd line pixels will be subject
	// to the motion test.

	for (y = 1; y < hminus1; y++)
	{
	    if (y&1)
	    {
		x = 0;
		do {
		    if (movingminus[x] | moving[x] | movingplus[x] | scenechange)
			if (cubic & (y > 2) & (y < hminus3))
			{
			    R = (5 * ((srcminus[x] & 0xff) + (srcplus[x] & 0xff)) 
				    - ((srcminusminus[x] & 0xff) + (srcplusplus[x] & 0xff))) >> 3;
			    dst[x] = clamp_f(R & 0xff)&0xff;
			}
			else
			{
			    dst[x] = (((srcminus[x]&0xff) >> 1) + ((srcplus[x]&0xff) >> 1)) & 0xff;
			}
		    else
		    {
			dst[x] = src[x];
		    }
		} while(++x < w);
	    }
	    else
	    {
		// Even line; pass it through.
		tc_memcpy(dst, src, w);
	    }
	    src +=  srcpitch;
	    srcminus += srcpitch;
	    srcplus += srcpitch;

	    if (cubic)
	    {
		srcminusminus += srcpitch;
		srcplusplus += srcpitch;
	    }

	    dst += dstpitch;
	    moving += (w+PAD);
	    movingminus += (w+PAD);
	    movingplus += (w+PAD);
	}

	// The last line gets a free ride.
	tc_memcpy(dst, src, w);
	if (clamp_f == clamp_Y)
	    counter++;

	return;
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



	mfd->buf =  bufalloc (width*height*3, mfd->Rbuf);
	mfd->prevFrame =  bufalloc (width*height*3, mfd->RprevFrame);

	msize = width*height + 4*(width+PAD) + PAD*height;
	mfd->movingY = (unsigned char *) bufalloc(sizeof(unsigned char)*msize, 
		(char *)mfd->RmovingY);
	mfd->fmovingY = (unsigned char *) bufalloc(sizeof(unsigned char)*msize,
		(char *)mfd->RfmovingY);

	msize = width*height/4 + 4*(width+PAD) + PAD*height;
	mfd->movingU  = (unsigned char *) bufalloc(sizeof(unsigned char)*msize,
		(char *)mfd->RmovingU);
	mfd->movingV  = (unsigned char *) bufalloc(sizeof(unsigned char)*msize,
		(char *)mfd->RmovingV);
	mfd->fmovingU = (unsigned char *) bufalloc(sizeof(unsigned char)*msize,
		(char *)mfd->RfmovingU);
	mfd->fmovingV = (unsigned char *) bufalloc(sizeof(unsigned char)*msize,
		(char *)mfd->RfmovingV);

	if ( !mfd->movingY || !mfd->movingU || !mfd->movingV || !mfd->fmovingY || 
	      !mfd->fmovingU || !mfd->fmovingV || !mfd->buf || !mfd->prevFrame) {
	    fprintf (stderr, "[%s] Memory allocation error\n", MOD_NAME);
	    return -1;
	}

	memset(mfd->prevFrame, BLACK_BYTE_Y, width*height);
	memset(mfd->prevFrame+width*height, BLACK_BYTE_UV, width*height/2);

	memset(mfd->buf, BLACK_BYTE_Y, width*height);
	memset(mfd->buf+width*height, BLACK_BYTE_UV, width*height/2);

	msize = width*height + 4*(width+PAD) + PAD*height;
	memset(mfd->movingY,  0, msize);
	memset(mfd->fmovingY, 0, msize);

	msize = width*height/4 + 4*(width+PAD) + PAD*height;
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
	//       mmx:  5.062.952
	// o  Erode+dilate
	//      orig: 55.847.077
	//       now: 21.764.997
	//  Erodemmx: 18.765.878
	// o  Blending
	//      orig: 8.162.287
	//       now: 5.384.433
	//       mmx: 4.569.875
	//   new mmx: 3.656.537
	// o  Cubic interpolation
	//      orig: 7.487.338
	//       now: 6.684.908
	//      more: 3.554.580
	//
	// Overall improvement in transcode:
	// 11.57 -> 22.78 frames per second for the test clip.
	//

	// filter init ok.

	if(verbose) printf("[%s] "
#ifdef HAVE_MMX
		"(MMX) "
#endif
#ifdef CAN_COMPILE_C_ALTIVEC
		"(ALTIVEC) "
#endif
		"%s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);

	return 0;

  } /* TC_FILTER_INIT */
	

  if(ptr->tag & TC_FILTER_GET_CONFIG) {
      char buf[255];
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VYE", "1");

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
	
	if (mfd->Rbuf)
	    free(mfd->Rbuf);
	mfd->buf = NULL;
	mfd->Rbuf = NULL;

	if (mfd->RprevFrame)
	    free(mfd->RprevFrame);
	mfd->prevFrame = NULL;
	mfd->RprevFrame = NULL;

	if (mfd->RmovingY) free(mfd->RmovingY); mfd->RmovingY = NULL; 
	mfd->movingY = NULL;
	if (mfd->RmovingU) free(mfd->RmovingU); mfd->RmovingU = NULL;
	mfd->movingU = NULL;
	if (mfd->RmovingV) free(mfd->RmovingV); mfd->RmovingV = NULL;
	mfd->movingV = NULL;

	if (mfd->RfmovingY) free(mfd->RfmovingY); mfd->RfmovingY = NULL;
	mfd->fmovingY = NULL;
	if (mfd->RfmovingU) free(mfd->RfmovingU); mfd->RfmovingU = NULL;
	mfd->fmovingU = NULL;
	if (mfd->RfmovingV) free(mfd->RfmovingV); mfd->RfmovingV = NULL;
	mfd->fmovingV = NULL;

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
	  int msize = ptr->v_width*ptr->v_height + 4*(ptr->v_width+PAD) + PAD*ptr->v_height;
	  int off = 2*(ptr->v_width+PAD)+PAD/2;

	  memset(mfd->movingY,  0, msize);
	  memset(mfd->fmovingY, 0, msize);
	  /*
	  */


	  smartyuv_core(ptr->video_buf, mfd->buf, mfd->prevFrame, 
		        ptr->v_width, ptr->v_height, ptr->v_width, ptr->v_width,
		        mfd->movingY+off, mfd->fmovingY+off, clamp_Y, mfd->threshold);


	  if (mfd->doChroma) {
	      msize = ptr->v_width*ptr->v_height/4 + 4*(ptr->v_width+PAD) + PAD*ptr->v_height;
	      off = 2*(ptr->v_width/2+PAD)+PAD/2;

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
	      tc_memcpy(mfd->buf+U, ptr->video_buf+U, ptr->v_width*ptr->v_height/2);
	      //memset(mfd->buf+U, BLACK_BYTE_UV, ptr->v_width*ptr->v_height/2);
	  }

	  /*
	  memset(mfd->buf, BLACK_BYTE_Y, ptr->v_width*ptr->v_height);
	  memset(mfd->buf+U, BLACK_BYTE_UV, ptr->v_width*ptr->v_height/2);
			  */

	  tc_memcpy (ptr->video_buf, mfd->buf, ptr->video_size);

	  return 0;
  }
  return 0;
}

