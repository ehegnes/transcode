/* quantize.c, quantization / inverse quantization                          */

/* Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved. */

/*
 * Disclaimer of Warranty
 *
 * These software programs are available to the user without any license fee or
 * royalty on an "as is" basis.  The MPEG Software Simulation Group disclaims
 * any and all warranties, whether express, implied, or statuary, including any
 * implied warranties or merchantability or of fitness for a particular
 * purpose.  In no event shall the copyright-holder be liable for any
 * incidental, punitive, or consequential damages of any kind whatsoever
 * arising from the use of these programs.
 *
 * This disclaimer of warranty extends to the user of these programs and user's
 * customers, employees, agents, transferees, successors, and assigns.
 *
 * The MPEG Software Simulation Group does not represent or warrant that the
 * programs furnished hereunder are free of infringement of any third-party
 * patents.
 *
 * Commercial implementations of MPEG-1 and MPEG-2 video, including shareware,
 * are subject to royalty fees to patent holders.  Many of these patents are
 * general enough such that they are unavoidable regardless of implementation
 * design.
 *
 */

#include "config.h" /* HAVE_MMX and HAVE_SSE #defines are here.*/
#include "main.h"
#include "../aclib/ac.h"

extern int tc_accel;

static void iquant1_intra(short *src, short *dst,
  int dc_prec, unsigned char *quant_mat, int mquant);
static void iquant1_non_intra(short *src, short *dst,
  unsigned char *quant_mat, int mquant);

/* Test Model 5 quantization
 *
 * this quantizer has a bias of 1/8 stepsize towards zero
 * (except for the DC coefficient)
 */
int bb_quant_intra(short *src, short *dst, int dc_prec,
                             unsigned char *quant_mat, int mquant)
{
  int i, ret = 1;
  int x, y, d;

  x = src[0];
  d = 8>>dc_prec; /* intra_dc_mult */
  dst[0] = (x>=0) ? (x+(d>>1))/d : -((-x+(d>>1))/d); /* round(x/d) */

  for (i=1; i<64; i++)
  {
    x = src[i];
    d = quant_mat[i];
    y = (32*(x>=0 ? x : -x) + (d>>1))/d; /* round(32*x/quant_mat) */
    d = (3*mquant+2)>>2;
    y = (y+d)/(2*mquant); /* (y+0.75*mquant) / (2*mquant) */

    /* clip to syntax limits */
    if (y > 255)
    {
      ret = 0;
      if (video_type < MPEG_MPEG2)
        y = 255;
      else if (y > 2047)
        y = 2047;
    }

    dst[i] = (x>=0) ? y : -y;

#if 0
    /* this quantizer is virtually identical to the above */
    if (x<0)
      x = -x;
    d = mquant*quant_mat[i];
    y = (16*x + ((3*d)>>3)) / d;
    dst[i] = (src[i]<0) ? -y : y;
#endif
  }

  return ret;
}

// N.b I've editted out a lot of scaffolding in the original used for
// instrumentation and testing and to allow adjustment to mquant if
// saturation occurs even in the 32-bit C code...
// A mis-matched bracket or two may have crept in.
// If you're interested in the math I can email you the analysis....
// In gcc/egcs "abs" is a very fast compiler intrinsic (probably uses
// x86 conditional moves) samesign is the bit-hackery I posted
// on delphi...
//

#define fabsshift ((8*sizeof(unsigned int))-1)
#define fastabs(x) (((x)-(((unsigned int)(x))>>fabsshift)) ^ ((x)>>fabsshift))
#define signmask(x) (((int)x)>>fabsshift)
#define samesign(x,y) (y+(signmask(x) & -(y<<1)))

int quant_non_intra(
short *src, short *dst,
unsigned short *quant_mat,
unsigned short *i_quant_mat,
int mquant)
{
  int i;
  int x, y, d;
  int nzflag;
  int clipvalue  = (video_type < MPEG_MPEG2) ? 255 : 2047;
  int imquant = (IQUANT_SCALE/mquant);
  int ret;

  /* MMX Quantizer maintains its own local buffer... dst will be unchanged if
        it flags saturation...
    */

#ifdef ARCH_X86
#ifdef HAVE_ASM_NASM

  if (mquant > 1 && tc_accel & MM_MMX) {
    ret = bb_quant_ni_mmx(dst, src, quant_mat, i_quant_mat, imquant, mquant, clipvalue);
    nzflag = ret & 0xffff0000;
    
    
    /* The fast MMX routines have a limited dynamic range.  We simply fall back to
       stanard routines in the (rather rare) cases when they detected out of
       range values...
    */
    
    if ((ret & 0xffff) == 0)
      return !!nzflag;
  }
#endif
#endif
  nzflag = 0;
  for (i = 0; i < 64; i++)
  {
    /* RJ: save one divide operation */
    /* AS: Lets make this a little more accurate... */
    x = abs(src[i]);
    d = quant_mat[i];
    /* N.b. accurate would be: y = (int)rint(32.0*((double)x)/((double)(d*2*mquant))); */
    /* Code below does *not* compute this always! */
    y = (32*abs(x) + (d>>1))/(d*2*mquant);
    if (y > clipvalue)
      y = clipvalue;
    nzflag |= (dst[i] = samesign(src[i],y));
  }

  return !!nzflag;
}

/* MPEG-2 inverse quantization */
void iquant_intra(
short *src, short *dst,
int dc_prec,
unsigned char *quant_mat,
int mquant)
{
  int i, val, sum;

  if (video_type < MPEG_MPEG2)
    iquant1_intra(src,dst,dc_prec,quant_mat,mquant);
  else
  {
    sum = dst[0] = src[0] << (3-dc_prec);
    for (i=1; i<64; i++)
    {
      val = (int)(src[i]*quant_mat[i]*mquant)/16;
      sum+= dst[i] = (val>2047) ? 2047 : ((val<-2048) ? -2048 : val);
    }

    /* mismatch control */
    if ((sum&1)==0)
      dst[63]^= 1;
  }
}

void iquant_non_intra(
short *src, short *dst,
unsigned char *quant_mat,
int mquant)
{
  int i, val, sum;

  if (video_type < MPEG_MPEG2)
    iquant1_non_intra(src,dst,quant_mat,mquant);
  else
  {
    sum = 0;
    for (i=0; i<64; i++)
    {
      val = src[i];
      if (val!=0)
        val = (int)((2*val+(val>0 ? 1 : -1))*quant_mat[i]*mquant)/32;
      sum+= dst[i] = (val>2047) ? 2047 : ((val<-2048) ? -2048 : val);
    }

    /* mismatch control */
    if ((sum&1)==0)
      dst[63]^= 1;
  }
}

/* MPEG-1 inverse quantization */
static void iquant1_intra(
short *src, short *dst,
int dc_prec,
unsigned char *quant_mat,
int mquant)
{
  int i, val;

  dst[0] = src[0] << (3-dc_prec);
  for (i=1; i<64; i++)
  {
    val = (int)(src[i]*quant_mat[i]*mquant)/16;

    /* mismatch control */
    if ((val&1)==0 && val!=0)
      val+= (val>0) ? -1 : 1;

    /* saturation */
    dst[i] = (val>2047) ? 2047 : ((val<-2048) ? -2048 : val);
  }
}

static void iquant1_non_intra(
short *src, short *dst,
unsigned char *quant_mat,
int mquant)
{
  int i, val;

  for (i=0; i<64; i++)
  {
    val = src[i];
    if (val!=0)
    {
      val = (int)((2*val+(val>0 ? 1 : -1))*quant_mat[i]*mquant)/32;

      /* mismatch control */
      if ((val&1)==0 && val!=0)
        val+= (val>0) ? -1 : 1;
    }

    /* saturation */
    dst[i] = (val>2047) ? 2047 : ((val<-2048) ? -2048 : val);
  }
}
