/* fdctref.c, forward discrete cosine transform, double precision           */

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

#include "main.h"

static const double aansf[8] = {
  1.0, 1.387039845, 1.306562965, 1.175875602,
  1.0, 0.785694958, 0.541196100, 0.275899379
};

static double aanscales[64];

void bb_init_fdct()
{
  int i, j;

  for (i = 0; i < 8; i++)
    for (j = 0; j < 8; j++)
      aanscales[(i << 3) + j] = 1.0 / (aansf[i] * aansf[j] * 8.0);
}

/* A 2-D DCT can be done by 1-D DCT on each row followed by 1-D DCT
 * on each column.  Direct algorithms are also available, but they are
 * much more complex and seem not to be any faster when reduced to code.
 *
 * This implementation is based on Arai, Agui, and Nakajima's algorithm for
 * scaled DCT.  Their original paper (Trans. IEICE E-71(11):1095) is in
 * Japanese, but the algorithm is described in the Pennebaker & Mitchell
 * JPEG textbook (see REFERENCES section in file README).  The following code
 * is based directly on figure 4-8 in P&M.
 * While an 8-point DCT cannot be done in less than 11 multiplies, it is
 * possible to arrange the computation so that many of the multiplies are
 * simple scalings of the final outputs.  These multiplies can then be
 * folded into the multiplications or divisions by the JPEG quantization
 * table entries.  The AA&N method leaves only 5 multiplies and 29 adds
 * to be done in the DCT itself.
 * The primary disadvantage of this method is that with a fixed-point
 * implementation, accuracy is lost due to imprecise representation of the
 * scaled quantization values.  However, that problem does not arise if
 * we use floating point arithmetic.
 */

/*
 * Perform a floating point forward DCT on one block of samples.
 */

void bb_fdct(short *block)
{
  double tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  double tmp10, tmp11, tmp12, tmp13;
  double z1, z2, z3, z4, z5, z11, z13;
  double *dataptr;
  double data[64];
  short *blkptr;
  int i;

  /* Pass 1: process rows. */

  blkptr = block;
  dataptr = data;
  for (i = 0; i < 8; i++)
  {
    tmp0 = blkptr[0] + blkptr[7];
    tmp7 = blkptr[0] - blkptr[7];
    tmp1 = blkptr[1] + blkptr[6];
    tmp6 = blkptr[1] - blkptr[6];
    tmp2 = blkptr[2] + blkptr[5];
    tmp5 = blkptr[2] - blkptr[5];
    tmp3 = blkptr[3] + blkptr[4];
    tmp4 = blkptr[3] - blkptr[4];

    /* Even part */

    tmp10 = tmp0 + tmp3;	/* phase 2 */
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;

    dataptr[0] = tmp10 + tmp11; /* phase 3 */
    dataptr[4] = tmp10 - tmp11;

    z1 = (tmp12 + tmp13) * ((double) 0.707106781); /* c4 */
    dataptr[2] = tmp13 + z1;	/* phase 5 */
    dataptr[6] = tmp13 - z1;

    /* Odd part */

    tmp10 = tmp4 + tmp5;	/* phase 2 */
    tmp11 = tmp5 + tmp6;
    tmp12 = tmp6 + tmp7;

    /* The rotator is modified from fig 4-8 to avoid extra negations. */
    z5 = (tmp10 - tmp12) * ((double) 0.382683433); /* c6 */
    z2 = ((double) 0.541196100) * tmp10 + z5; /* c2-c6 */
    z4 = ((double) 1.306562965) * tmp12 + z5; /* c2+c6 */
    z3 = tmp11 * ((double) 0.707106781); /* c4 */

    z11 = tmp7 + z3;		/* phase 5 */
    z13 = tmp7 - z3;

    dataptr[5] = z13 + z2;	/* phase 6 */
    dataptr[3] = z13 - z2;
    dataptr[1] = z11 + z4;
    dataptr[7] = z11 - z4;

    dataptr += 8;		/* advance pointer to next row */
    blkptr += 8;
  }

  /* Pass 2: process columns. */

  dataptr = data;
  for (i = 0; i < 8; i++)
  {
    tmp0 = dataptr[0] + dataptr[56];
    tmp7 = dataptr[0] - dataptr[56];
    tmp1 = dataptr[8] + dataptr[48];
    tmp6 = dataptr[8] - dataptr[48];
    tmp2 = dataptr[16] + dataptr[40];
    tmp5 = dataptr[16] - dataptr[40];
    tmp3 = dataptr[24] + dataptr[32];
    tmp4 = dataptr[24] - dataptr[32];

    /* Even part */

    tmp10 = tmp0 + tmp3;	/* phase 2 */
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;

    dataptr[0] = tmp10 + tmp11; /* phase 3 */
    dataptr[32] = tmp10 - tmp11;

    z1 = (tmp12 + tmp13) * ((double) 0.707106781); /* c4 */
    dataptr[16] = tmp13 + z1; /* phase 5 */
    dataptr[48] = tmp13 - z1;

    /* Odd part */

    tmp10 = tmp4 + tmp5;	/* phase 2 */
    tmp11 = tmp5 + tmp6;
    tmp12 = tmp6 + tmp7;

    /* The rotator is modified from fig 4-8 to avoid extra negations. */
    z5 = (tmp10 - tmp12) * ((double) 0.382683433); /* c6 */
    z2 = ((double) 0.541196100) * tmp10 + z5; /* c2-c6 */
    z4 = ((double) 1.306562965) * tmp12 + z5; /* c2+c6 */
    z3 = tmp11 * ((double) 0.707106781); /* c4 */

    z11 = tmp7 + z3;		/* phase 5 */
    z13 = tmp7 - z3;

    dataptr[40] = z13 + z2; /* phase 6 */
    dataptr[24] = z13 - z2;
    dataptr[8] = z11 + z4;
    dataptr[56] = z11 - z4;

    dataptr++;			/* advance pointer to next column */
  }
  /* descale */
  for (i = 0; i < 64; i++)
    block[i] = (short int) floor(data[i] * aanscales[i] + 0.499999);
}


/* This routine is a slow-but-accurate integer implementation of the
 * forward DCT (Discrete Cosine Transform). Taken from the IJG software
 *
 * A 2-D DCT can be done by 1-D DCT on each row followed by 1-D DCT
 * on each column.  Direct algorithms are also available, but they are
 * much more complex and seem not to be any faster when reduced to code.
 *
 * This implementation is based on an algorithm described in
 *   C. Loeffler, A. Ligtenberg and G. Moschytz, "Practical Fast 1-D DCT
 *   Algorithms with 11 Multiplications", Proc. Int'l. Conf. on Acoustics,
 *   Speech, and Signal Processing 1989 (ICASSP '89), pp. 988-991.
 * The primary algorithm described there uses 11 multiplies and 29 adds.
 * We use their alternate method with 12 multiplies and 32 adds.
 * The advantage of this method is that no data path contains more than one
 * multiplication; this allows a very simple and accurate implementation in
 * scaled fixed-point arithmetic, with a minimal number of shifts.
 *
 * The poop on this scaling stuff is as follows:
 *
 * Each 1-D DCT step produces outputs which are a factor of sqrt(N)
 * larger than the true DCT outputs.  The final outputs are therefore
 * a factor of N larger than desired; since N=8 this can be cured by
 * a simple right shift at the end of the algorithm.  The advantage of
 * this arrangement is that we save two multiplications per 1-D DCT,
 * because the y0 and y4 outputs need not be divided by sqrt(N).
 * In the IJG code, this factor of 8 is removed by the quantization step
 * (in jcdctmgr.c), here it is removed.
 *
 * We have to do addition and subtraction of the integer inputs, which
 * is no problem, and multiplication by fractional constants, which is
 * a problem to do in integer arithmetic.  We multiply all the constants
 * by CONST_SCALE and convert them to integer constants (thus retaining
 * CONST_BITS bits of precision in the constants).  After doing a
 * multiplication we have to divide the product by CONST_SCALE, with proper
 * rounding, to produce the correct output.  This division can be done
 * cheaply as a right shift of CONST_BITS bits.  We postpone shifting
 * as long as possible so that partial sums can be added together with
 * full fractional precision.
 *
 * The outputs of the first pass are scaled up by PASS1_BITS bits so that
 * they are represented to better-than-integral precision.  These outputs
 * require 8 + PASS1_BITS + 3 bits; this fits in a 16-bit word
 * with the recommended scaling.  (For 12-bit sample data, the intermediate
 * array is INT32 anyway.)
 *
 * To avoid overflow of the 32-bit intermediate results in pass 2, we must
 * have 8 + CONST_BITS + PASS1_BITS <= 26.  Error analysis
 * shows that the values given below are the most effective.
 *
 * We can gain a little more speed, with a further compromise in accuracy,
 * by omitting the addition in a descaling shift.  This yields an incorrectly
 * rounded result half the time...
 */

#define USE_ACCURATE_ROUNDING

#define RIGHT_SHIFT(x, shft)  ((x) >> (shft))

#ifdef USE_ACCURATE_ROUNDING
#define ONE ((int) 1)
#define DESCALE(x, n)  RIGHT_SHIFT((x) + (ONE << ((n) - 1)), n)
#else
#define DESCALE(x, n)  RIGHT_SHIFT(x, n)
#endif

#define CONST_BITS  13
#define PASS1_BITS  2

#define FIX_0_298631336  ((int)  2446)	/* FIX(0.298631336) */
#define FIX_0_390180644  ((int)  3196)	/* FIX(0.390180644) */
#define FIX_0_541196100  ((int)  4433)	/* FIX(0.541196100) */
#define FIX_0_765366865  ((int)  6270)	/* FIX(0.765366865) */
#define FIX_0_899976223  ((int)  7373)	/* FIX(0.899976223) */
#define FIX_1_175875602  ((int)  9633)	/* FIX(1.175875602) */
#define FIX_1_501321110  ((int) 12299)	/* FIX(1.501321110) */
#define FIX_1_847759065  ((int) 15137)	/* FIX(1.847759065) */
#define FIX_1_961570560  ((int) 16069)	/* FIX(1.961570560) */
#define FIX_2_053119869  ((int) 16819)	/* FIX(2.053119869) */
#define FIX_2_562915447  ((int) 20995)	/* FIX(2.562915447) */
#define FIX_3_072711026  ((int) 25172)	/* FIX(3.072711026) */

/*
 * Perform an integer forward DCT on one block of samples.
 */

void bb_intfdct(short *block)
{
  int tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  int tmp10, tmp11, tmp12, tmp13;
  int z1, z2, z3, z4, z5;
  short *blkptr;
  int *dataptr;
  int data[64];
  int i;

  /* Pass 1: process rows. */
  /* Note results are scaled up by sqrt(8) compared to a true DCT; */
  /* furthermore, we scale the results by 2**PASS1_BITS. */

  dataptr = data;
  blkptr = block;
  for (i = 0; i < 8; i++)
  {
    tmp0 = blkptr[0] + blkptr[7];
    tmp7 = blkptr[0] - blkptr[7];
    tmp1 = blkptr[1] + blkptr[6];
    tmp6 = blkptr[1] - blkptr[6];
    tmp2 = blkptr[2] + blkptr[5];
    tmp5 = blkptr[2] - blkptr[5];
    tmp3 = blkptr[3] + blkptr[4];
    tmp4 = blkptr[3] - blkptr[4];

    /* Even part per LL&M figure 1 --- note that published figure is faulty;
     * rotator "sqrt(2)*c1" should be "sqrt(2)*c6".
     */

    tmp10 = tmp0 + tmp3;
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;

    dataptr[0] = (tmp10 + tmp11) << PASS1_BITS;
    dataptr[4] = (tmp10 - tmp11) << PASS1_BITS;

    z1 = (tmp12 + tmp13) * FIX_0_541196100;
    dataptr[2] = DESCALE(z1 + tmp13 * FIX_0_765366865, CONST_BITS - PASS1_BITS);
    dataptr[6] = DESCALE(z1 + tmp12 * (-FIX_1_847759065), CONST_BITS - PASS1_BITS);

    /* Odd part per figure 8 --- note paper omits factor of sqrt(2).
     * cK represents cos(K*pi/16).
     * i0..i3 in the paper are tmp4..tmp7 here.
     */

    z1 = tmp4 + tmp7;
    z2 = tmp5 + tmp6;
    z3 = tmp4 + tmp6;
    z4 = tmp5 + tmp7;
    z5 = (z3 + z4) * FIX_1_175875602; /* sqrt(2) * c3 */

    tmp4 *= FIX_0_298631336; /* sqrt(2) * (-c1+c3+c5-c7) */
    tmp5 *= FIX_2_053119869; /* sqrt(2) * ( c1+c3-c5+c7) */
    tmp6 *= FIX_3_072711026; /* sqrt(2) * ( c1+c3+c5-c7) */
    tmp7 *= FIX_1_501321110; /* sqrt(2) * ( c1+c3-c5-c7) */
    z1 *= -FIX_0_899976223; /* sqrt(2) * (c7-c3) */
    z2 *= -FIX_2_562915447; /* sqrt(2) * (-c1-c3) */
    z3 *= -FIX_1_961570560; /* sqrt(2) * (-c3-c5) */
    z4 *= -FIX_0_390180644; /* sqrt(2) * (c5-c3) */

    z3 += z5;
    z4 += z5;

    dataptr[7] = DESCALE(tmp4 + z1 + z3, CONST_BITS - PASS1_BITS);
    dataptr[5] = DESCALE(tmp5 + z2 + z4, CONST_BITS - PASS1_BITS);
    dataptr[3] = DESCALE(tmp6 + z2 + z3, CONST_BITS - PASS1_BITS);
    dataptr[1] = DESCALE(tmp7 + z1 + z4, CONST_BITS - PASS1_BITS);

    dataptr += 8;		/* advance pointer to next row */
    blkptr += 8;
  }

  /* Pass 2: process columns.
   * We remove the PASS1_BITS scaling, but leave the results scaled up
   * by an overall factor of 8.
   */

  dataptr = data;
  for (i = 0; i < 8; i++)
  {
    tmp0 = dataptr[0] + dataptr[56];
    tmp7 = dataptr[0] - dataptr[56];
    tmp1 = dataptr[8] + dataptr[48];
    tmp6 = dataptr[8] - dataptr[48];
    tmp2 = dataptr[16] + dataptr[40];
    tmp5 = dataptr[16] - dataptr[40];
    tmp3 = dataptr[24] + dataptr[32];
    tmp4 = dataptr[24] - dataptr[32];

    /* Even part per LL&M figure 1 --- note that published figure is faulty;
     * rotator "sqrt(2)*c1" should be "sqrt(2)*c6".
     */

    tmp10 = tmp0 + tmp3;
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;

    dataptr[0] = DESCALE(tmp10 + tmp11, PASS1_BITS);
    dataptr[32] = DESCALE(tmp10 - tmp11, PASS1_BITS);

    z1 = (tmp12 + tmp13) * FIX_0_541196100;
    dataptr[16] = DESCALE(z1 + tmp13 * FIX_0_765366865, CONST_BITS + PASS1_BITS);
    dataptr[48] = DESCALE(z1 + tmp12 * (-FIX_1_847759065), CONST_BITS + PASS1_BITS);

    /* Odd part per figure 8 --- note paper omits factor of sqrt(2).
     * cK represents cos(K*pi/16).
     * i0..i3 in the paper are tmp4..tmp7 here.
     */

    z1 = tmp4 + tmp7;
    z2 = tmp5 + tmp6;
    z3 = tmp4 + tmp6;
    z4 = tmp5 + tmp7;
    z5 = (z3 + z4) * FIX_1_175875602; /* sqrt(2) * c3 */

    tmp4 *= FIX_0_298631336; /* sqrt(2) * (-c1+c3+c5-c7) */
    tmp5 *= FIX_2_053119869; /* sqrt(2) * ( c1+c3-c5+c7) */
    tmp6 *= FIX_3_072711026; /* sqrt(2) * ( c1+c3+c5-c7) */
    tmp7 *= FIX_1_501321110; /* sqrt(2) * ( c1+c3-c5-c7) */
    z1 *= -FIX_0_899976223; /* sqrt(2) * (c7-c3) */
    z2 *= -FIX_2_562915447; /* sqrt(2) * (-c1-c3) */
    z3 *= -FIX_1_961570560; /* sqrt(2) * (-c3-c5) */
    z4 *= -FIX_0_390180644; /* sqrt(2) * (c5-c3) */

    z3 += z5;
    z4 += z5;

    dataptr[56] = DESCALE(tmp4 + z1 + z3, CONST_BITS + PASS1_BITS);
    dataptr[40] = DESCALE(tmp5 + z2 + z4, CONST_BITS + PASS1_BITS);
    dataptr[24] = DESCALE(tmp6 + z2 + z3, CONST_BITS + PASS1_BITS);
    dataptr[8] = DESCALE(tmp7 + z1 + z4, CONST_BITS + PASS1_BITS);

    dataptr++;			/* advance pointer to next column */
  }
  /* descale */
  for (i = 0; i < 64; i++)
    block[i] = (short int) DESCALE(data[i], 3);
}

