/* 
 *  enc_input.c
 *
 *     Copyright (C) Peter Schlaile - Feb 2001
 *
 *  This file is part of libdv, a free DV (IEC 61834/SMPTE 314M)
 *  codec.
 *
 *  libdv is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your
 *  option) any later version.
 *   
 *  libdv is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 *  The libdv homepage is http://libdv.sourceforge.net/.  
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include <transcode.h>

#include "dvenc.h"

/* FIXME: Just guessed! */
#define DCT_248_THRESHOLD  (17 * 65536 /10)


static int frame_height;

extern void dv_enc_rgb_to_ycb(unsigned char* img_rgb, int height,
			      short* img_y, short* img_cr, short* img_cb);

static unsigned char* readbuf = NULL;
static int force_dct = 0;

static short* img_y = NULL; /* [DV_PAL_HEIGHT * DV_WIDTH]; */
static short* img_cr = NULL; /* [DV_PAL_HEIGHT * DV_WIDTH / 2]; */
static short* img_cb = NULL; /* [DV_PAL_HEIGHT * DV_WIDTH / 2]; */


static int ppm_init(int wrong_interlace_, int force_dct_) 
{

  readbuf = (unsigned char*) calloc(DV_WIDTH * DV_PAL_HEIGHT, 3);
  
  force_dct = force_dct_;
  
  img_y = (short*) calloc(DV_PAL_HEIGHT * DV_WIDTH, sizeof(short));
  img_cr = (short*) calloc(DV_PAL_HEIGHT * DV_WIDTH / 2, sizeof(short));
  img_cb = (short*) calloc(DV_PAL_HEIGHT * DV_WIDTH / 2, sizeof(short));
  
  return 0;
}

static void ppm_finish()
{
	free(readbuf);
	free(img_y);
	free(img_cr);
	free(img_cb);
}

static int ppm_load(const char* filename, int *isPAL)
{
  
  *isPAL = (frame_height == DV_PAL_HEIGHT);
  tc_memcpy(readbuf, dvenc_vbuf, 3 * DV_WIDTH * frame_height);     
  dv_enc_rgb_to_ycb(readbuf, frame_height, img_y, img_cr, img_cb);
  
  return (0);
}

static int ppm_skip(const char* filename, int * isPAL)
{
    return(0);
}

#if !ARCH_X86

int need_dct_248_transposed(dv_coeff_t * bl)
{
	int res_cols = 1;
	int res_rows = 1;
	int i,j;
	
	for (j = 0; j < 7; j ++) {
		for (i = 0; i < 8; i++) {
			int a = bl[i * 8 + j] - bl[i * 8 + j + 1];
			int b = (a >> 15);
			a ^= b;
			a -= b;
			res_cols += a;
		}
	}

	for (j = 0; j < 7; j ++) {
		for (i = 0; i < 8; i++) {
			int a = bl[j * 8 + i] - bl[(j + 1) * 8 + i];
			int b = (a >> 15);
			a ^= b;
			a -= b;
			res_rows += a;
		}
	}

	return ((res_cols * 65536 / res_rows) > DCT_248_THRESHOLD);
}

#else

extern int need_dct_248_mmx_rows(dv_coeff_t * bl);

extern void transpose_mmx(short * dst);
extern void ppm_copy_y_block_mmx(short * dst, short * src);
extern void ppm_copy_pal_c_block_mmx(short * dst, short * src);
extern void ppm_copy_ntsc_c_block_mmx(short * dst, short * src);

static void finish_mb_mmx(dv_macroblock_t* mb)
{
	int b;
	int need_dct_248_rows[6];
	dv_block_t* bl = mb->b;

	if (force_dct != -1) {
		for (b = 0; b < 6; b++) {
			bl[b].dct_mode = force_dct;
		}
	} else {
		for (b = 0; b < 6; b++) {
			need_dct_248_rows[b]
				= need_dct_248_mmx_rows(bl[b].coeffs) + 1;
		}
	}
	transpose_mmx(bl[0].coeffs);
	transpose_mmx(bl[1].coeffs);
	transpose_mmx(bl[2].coeffs);
	transpose_mmx(bl[3].coeffs);
	transpose_mmx(bl[4].coeffs);
	transpose_mmx(bl[5].coeffs);

	if (force_dct == -1) {
		for (b = 0; b < 6; b++) {
			bl[b].dct_mode = 
				((need_dct_248_rows[b] * 65536 / 
				  (need_dct_248_mmx_rows(bl[b].coeffs) + 1))
				 > DCT_248_THRESHOLD) ? DV_DCT_248 : DV_DCT_88;
		}
	}
}

#endif /* ARCH_X86 */

static void ppm_fill_macroblock(dv_macroblock_t *mb, int isPAL)
{
	int y = mb->y;
	int x = mb->x;
	dv_block_t* bl = mb->b;

#if !ARCH_X86
	if (isPAL || mb->x == DV_WIDTH- 16) { /* PAL or rightmost NTSC block */
		int i,j;
		for (j = 0; j < 8; j++) {
			for (i = 0; i < 8; i++) {
				bl[0].coeffs[8 * i + j] = 
					img_y[(y + j) * DV_WIDTH +  x + i];
				bl[1].coeffs[8 * i + j] = 
					img_y[(y + j) * DV_WIDTH +  x + 8 + i];
				bl[2].coeffs[8 * i + j] = 
					img_y[(y + 8 + j) * DV_WIDTH + x + i];
				bl[3].coeffs[8 * i + j] = 
					img_y[(y + 8 + j) * DV_WIDTH 
					     + x + 8 + i];
				bl[4].coeffs[8 * i + j] = 
					(img_cr[(y + 2*j) * DV_WIDTH/2 
					       + x / 2 + i]
					+ img_cr[(y + 2*j + 1) * DV_WIDTH/2
						+ x / 2 + i]) >> 1;
				bl[5].coeffs[8 * i + j] = 
					(img_cb[(y + 2*j) * DV_WIDTH/2
					      + x / 2 + i]
					+ img_cb[(y + 2*j + 1) * DV_WIDTH/2
						+ x / 2 + i]) >> 1;
			}
		}
	} else {                        /* NTSC */
		int i,j;
		for (j = 0; j < 8; j++) {
			for (i = 0; i < 8; i++) {
				bl[0].coeffs[8 * i + j] = 
					img_y[(y + j) * DV_WIDTH +  x + i];
				bl[1].coeffs[8 * i + j] = 
					img_y[(y + j) * DV_WIDTH +  x + 8 + i];
				bl[2].coeffs[8 * i + j] = 
					img_y[(y + j) * DV_WIDTH + x + 16 + i];
				bl[3].coeffs[8 * i + j] = 
					img_y[(y + j) * DV_WIDTH + x + 24 + i];
				bl[4].coeffs[8 * i + j] = 
					(img_cr[(y + j) * DV_WIDTH/2
					       + x / 2 + i*2]
					 + img_cr[(y + j) * DV_WIDTH/2 
						 + x / 2 + 1 + i*2]) >> 1;
				bl[5].coeffs[8 * i + j] = 
					(img_cb[(y + j) * DV_WIDTH/2
					       + x / 2 + i*2]
					 + img_cb[(y + j) * DV_WIDTH/2 
						 + x / 2 + 1 + i*2]) >> 1;
			}
		}
	}
	if (force_dct != -1) {
		int b;
		for (b = 0; b < 6; b++) {
			bl[b].dct_mode = force_dct;
		}
	} else {
		int b;
		for (b = 0; b < 6; b++) {
			bl[b].dct_mode = need_dct_248_transposed(bl[b].coeffs) 
				? DV_DCT_248 : DV_DCT_88;
		}
	}
#else
	if (isPAL || mb->x == DV_WIDTH- 16) { /* PAL or rightmost NTSC block */
		short* start_y = img_y + y * DV_WIDTH + x;
		ppm_copy_y_block_mmx(bl[0].coeffs, start_y);
		ppm_copy_y_block_mmx(bl[1].coeffs, start_y + 8);
		ppm_copy_y_block_mmx(bl[2].coeffs, start_y + 8 * DV_WIDTH);
		ppm_copy_y_block_mmx(bl[3].coeffs, start_y + 8 * DV_WIDTH + 8);
		ppm_copy_pal_c_block_mmx(bl[4].coeffs,
					 img_cr+y * DV_WIDTH/2+ x/2);
		ppm_copy_pal_c_block_mmx(bl[5].coeffs,
					 img_cb+y * DV_WIDTH/2+ x/2);
	} else {                              /* NTSC */
		short* start_y = img_y + y * DV_WIDTH + x;
		ppm_copy_y_block_mmx(bl[0].coeffs, start_y);
		ppm_copy_y_block_mmx(bl[1].coeffs, start_y + 8);
		ppm_copy_y_block_mmx(bl[2].coeffs, start_y + 16);
		ppm_copy_y_block_mmx(bl[3].coeffs, start_y + 24);
		ppm_copy_ntsc_c_block_mmx(bl[4].coeffs,
					  img_cr + y*DV_WIDTH/2 + x/2);
		ppm_copy_ntsc_c_block_mmx(bl[5].coeffs,
					  img_cb + y*DV_WIDTH/2 + x/2);
	}

	finish_mb_mmx(mb);

	emms();
#endif
}

static int pgm_init(int wrong_interlace_, int force_dct_) 
{
	force_dct = force_dct_;

	readbuf = (unsigned char*) calloc(DV_WIDTH * (DV_PAL_HEIGHT + 1), 3);

	return 0;
}

static void pgm_finish()
{
    free(readbuf);
}

static int pgm_load(const char* filename, int * isPAL)
{

  int n, off1, off2, block;
  
  *isPAL = (frame_height == DV_PAL_HEIGHT);
    
  //Y
  tc_memcpy(readbuf, dvenc_vbuf, DV_WIDTH * frame_height);  
  
  off1 = DV_WIDTH * frame_height;
  off2 = (DV_WIDTH * frame_height * 5)/4;
  
  block = DV_WIDTH/2;
  
  //interleave Cb and Cr
  for (n=0; n<frame_height/2; ++n) {
    tc_memcpy(readbuf+off1+2*n*block, dvenc_vbuf+off2 + n*block, block); 
    tc_memcpy(readbuf+off1+(2*n+1)*block, dvenc_vbuf+off1 + n*block, block);
  }
  
  return (0);
}

static int pgm_skip(const char* filename, int * isPAL)
{
    return(0);
}

#if !ARCH_X86
static inline short pgm_get_y(int y, int x)
{
	return (((short) readbuf[y * DV_WIDTH + x]) - 128 + 16)
		<< DCT_YUV_PRECISION;
}

static inline short pgm_get_cr_pal(int y, int x)
{
	return (readbuf[DV_PAL_HEIGHT * DV_WIDTH + DV_WIDTH/2  
			    + y * DV_WIDTH + x] 
		- 128) << DCT_YUV_PRECISION;

}

static inline short pgm_get_cb_pal(int y, int x)
{
	return (readbuf[DV_PAL_HEIGHT * DV_WIDTH +
			    + y * DV_WIDTH + x] - 128) << DCT_YUV_PRECISION;

}

static inline short pgm_get_cr_ntsc(int y, int x)
{
	return ((((short) readbuf[DV_NTSC_HEIGHT * DV_WIDTH+ DV_WIDTH/2
				      + y * DV_WIDTH + x]) - 128)
		+ (((short) readbuf[DV_NTSC_HEIGHT * DV_WIDTH+DV_WIDTH/2
					+ y * DV_WIDTH + x + 1]) - 128))
						<< (DCT_YUV_PRECISION - 1);
}

static inline short pgm_get_cb_ntsc(int y, int x)
{
	return ((((short) readbuf[DV_NTSC_HEIGHT * DV_WIDTH
				      + y * DV_WIDTH + x]) - 128)
		+ (((short) readbuf[DV_NTSC_HEIGHT * DV_WIDTH
					+ y * DV_WIDTH + x + 1]) - 128))
						<< (DCT_YUV_PRECISION - 1);
}

#else
extern void pgm_copy_y_block_mmx(short * dst, unsigned char * src);
extern void pgm_copy_pal_c_block_mmx(short * dst, unsigned char * src);
extern void pgm_copy_ntsc_c_block_mmx(short * dst, unsigned char * src);
#endif

static void pgm_fill_macroblock(dv_macroblock_t *mb, int isPAL)
{
	int y = mb->y;
	int x = mb->x;
	dv_block_t* bl = mb->b;
#if !ARCH_X86
	if (isPAL || mb->x == DV_WIDTH- 16) { /* PAL or rightmost NTSC block */
		int i,j;
		for (j = 0; j < 8; j++) {
			for (i = 0; i < 8; i++) {
				bl[0].coeffs[8*i + j] = pgm_get_y(y+j,x+i);
				bl[1].coeffs[8*i + j] = pgm_get_y(y+j,x+8+i);
				bl[2].coeffs[8*i + j] = pgm_get_y(y+8+j,x+i);
				bl[3].coeffs[8*i + j] = pgm_get_y(y+8+j,x+8+i);
				bl[4].coeffs[8*i + j] = 
					pgm_get_cr_pal(y/2+j, x/2+i);
				bl[5].coeffs[8*i + j] = 
					pgm_get_cb_pal(y/2+j, x/2+i);
			}
		}
	} else {                        /* NTSC */
		int i,j;
		for (i = 0; i < 8; i++) {
			for (j = 0; j < 8; j++) {
				bl[0].coeffs[8*i + j] = pgm_get_y(y+j,x+i);
				bl[1].coeffs[8*i + j] = pgm_get_y(y+j,x+8+i);
				bl[2].coeffs[8*i + j] = pgm_get_y(y+j,x+16+i);
				bl[3].coeffs[8*i + j] = pgm_get_y(y+j,x+24+i);
			}
			for (j = 0; j < 4; j++) {
				bl[4].coeffs[8*i + j*2] = 
					bl[4].coeffs[8*i + j*2 + 1] = 
					pgm_get_cr_ntsc(y + j, x/2 + i * 2);
				bl[5].coeffs[8*i + j*2] = 
					bl[5].coeffs[8*i + j*2 + 1] = 
					pgm_get_cb_ntsc(y + j, x/2 + i * 2);
			}
		}
	}
	if (force_dct != -1) {
		int b;
		for (b = 0; b < 6; b++) {
			bl[b].dct_mode = force_dct;
		}
	} else {
		int b;
		for (b = 0; b < 6; b++) {
			bl[b].dct_mode = need_dct_248_transposed(bl[b].coeffs) 
				? DV_DCT_248 : DV_DCT_88;
		}
	}
#else
	if (isPAL || mb->x == DV_WIDTH- 16) { /* PAL or rightmost NTSC block */
		unsigned char* start_y = readbuf + y * DV_WIDTH + x;
		unsigned char* img_cr = readbuf 
			+ (isPAL ? (DV_WIDTH * DV_PAL_HEIGHT)
			   : (DV_WIDTH * DV_NTSC_HEIGHT)) + DV_WIDTH / 2;
		unsigned char* img_cb = readbuf 
			+ (isPAL ? (DV_WIDTH * DV_PAL_HEIGHT) 
			   : (DV_WIDTH * DV_NTSC_HEIGHT));

		pgm_copy_y_block_mmx(bl[0].coeffs, start_y);
		pgm_copy_y_block_mmx(bl[1].coeffs, start_y + 8);
		pgm_copy_y_block_mmx(bl[2].coeffs, start_y + 8 * DV_WIDTH);
		pgm_copy_y_block_mmx(bl[3].coeffs, start_y + 8 * DV_WIDTH + 8);
		pgm_copy_pal_c_block_mmx(bl[4].coeffs,
					 img_cr + y * DV_WIDTH / 2 + x / 2);
		pgm_copy_pal_c_block_mmx(bl[5].coeffs,
					 img_cb + y * DV_WIDTH / 2 + x / 2);
	} else {                              /* NTSC */
		unsigned char* start_y = readbuf + y * DV_WIDTH + x;
		unsigned char* img_cr = readbuf 
			+ DV_WIDTH * DV_NTSC_HEIGHT + DV_WIDTH / 2;
		unsigned char* img_cb = readbuf 
			+ DV_WIDTH * DV_NTSC_HEIGHT;
		pgm_copy_y_block_mmx(bl[0].coeffs, start_y);
		pgm_copy_y_block_mmx(bl[1].coeffs, start_y + 8);
		pgm_copy_y_block_mmx(bl[2].coeffs, start_y + 16);
		pgm_copy_y_block_mmx(bl[3].coeffs, start_y + 24);
		pgm_copy_ntsc_c_block_mmx(bl[4].coeffs,
					  img_cr + y * DV_WIDTH / 2 + x / 2);
		pgm_copy_ntsc_c_block_mmx(bl[5].coeffs,
					  img_cb + y * DV_WIDTH / 2 + x / 2);
	}

	finish_mb_mmx(mb);

	emms();
#endif
}


void dvenc_init_input(dv_enc_input_filter_t *filter, int mode, int format)
{

    frame_height=format; //PAL or NTSC
    
    switch(mode) {
	
    case 0:
	
	filter->init   = ppm_init;
	filter->finish = ppm_finish;
	filter->load   = ppm_load;
	filter->skip   = ppm_skip;
	filter->fill_macroblock = ppm_fill_macroblock;
	filter->filter_name  = "tc_ppm";
	
	break;
	
    case 1:
	
	filter->init   = pgm_init;
	filter->finish = pgm_finish;
	filter->load   = pgm_load;
	filter->skip   = pgm_skip;
	filter->fill_macroblock = pgm_fill_macroblock;
	filter->filter_name  = "tc_yuv";
	
	break;
    }
}
