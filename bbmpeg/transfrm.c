/* transfrm.c,  forward / inverse transformation                            */

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
#include <math.h>

static void add_pred(unsigned char *pred, unsigned char *cur,
      int lx, short *blk);

static void sub_pred(unsigned char *pred, unsigned char *cur,
      int lx, short *blk);


static void (*add_pred_sub)(unsigned char *pred, unsigned char *cur,
      int lx, short *blk);

static void (*sub_pred_sub)(unsigned char *pred, unsigned char *cur,
      int lx, short *blk);

static void (*fdct_sub)(short *block);

static void (*idct_sub)(short *block);

extern void bb_fdct_mmx( short *block );
extern void bb_idct_mmx( short *block );

void init_transform(int sh_info)
{
  switch (MMXMode)
  {
    case MODE_3DNOWEXT:  // AMD 3DNOW extensions, use MMX
    case MODE_3DNOW:     // AMD 3DNOW, use MMX
      if (sh_info)
        fprintf(stderr, "INFO: dct with MMX-acceleration (AMD)!\n");
      sub_pred_sub = sub_pred_mmx;
      add_pred_sub = add_pred_mmx;
      idct_sub = bb_idct_mmx; 
      fdct_sub = bb_fdct_mmx;
      break;

    case MODE_SSE:       // Intel SSE, use MMX
    case MODE_MMX:       // Intel or AMD MMX
      if (sh_info)
        fprintf(stderr, "INFO: dct with MMX acceleration (intel)!\n");
      sub_pred_sub = sub_pred_mmx;
      add_pred_sub = add_pred_mmx;
      idct_sub = bb_idct_mmx; 
      fdct_sub = bb_fdct_mmx;
      break;

    default:  // straight IA
      if (sh_info)
        fprintf(stderr, "INFO: dct without acceleration!\n");
      sub_pred_sub = sub_pred;
      add_pred_sub = add_pred;
      idct_sub = bb_idct;
      fdct_sub = bb_intfdct;
  }
  if (UseFP)
  {
    fdct_sub = bb_fdct;
    idct_sub = bb_idct;
  }
}

/* subtract prediction and transform prediction error */
void transform(unsigned char *pred[], unsigned char *cur[],
                            struct mbinfo *mbi, short blocks[][64])
{
  int i, j, i1, j1, k, n, cc, offs, lx;

  k = 0;

  for (j=0; j<height2; j+=16)
  {
    for (i=0; i<width; i+=16)
    {
      for (n=0; n<block_count; n++)
      {
        cc = (n<4) ? 0 : (n&1)+1; /* color component index */
        if (cc==0)
        {
          /* luminance */
          if ((pict_struct==FRAME_PICTURE) && mbi[k].dct_type)
          {
            /* field DCT */
            offs = i + ((n&1)<<3) + width*(j+((n&2)>>1));
            lx = width<<1;
          }
          else
          {
            /* frame DCT */
            offs = i + ((n&1)<<3) + width2*(j+((n&2)<<2));
            lx = width2;
          }

          if (pict_struct==BOTTOM_FIELD)
            offs += width;
        }
        else
        {
          /* chrominance */

          /* scale coordinates */
          i1 = (chroma_format==CHROMA444) ? i : i>>1;
          j1 = (chroma_format!=CHROMA420) ? j : j>>1;

          if ((pict_struct==FRAME_PICTURE) && mbi[k].dct_type
              && (chroma_format!=CHROMA420))
          {
            /* field DCT */
            offs = i1 + (n&8) + chrom_width*(j1+((n&2)>>1));
            lx = chrom_width<<1;
          }
          else
          {
            /* frame DCT */
            offs = i1 + (n&8) + chrom_width2*(j1+((n&2)<<2));
            lx = chrom_width2;
          }

          if (pict_struct==BOTTOM_FIELD)
            offs += chrom_width;
        }

        (*sub_pred_sub)(pred[cc]+offs,cur[cc]+offs,lx,blocks[k*block_count+n]);
        (*fdct_sub)(blocks[k*block_count+n]);
      }

      k++;
    }
  }
}

/* inverse transform prediction error and add prediction */
void itransform(unsigned char *pred[], unsigned char *cur[],
                             struct mbinfo *mbi, short blocks[][64])
{
  int i, j, i1, j1, k, n, cc, offs, lx;

  k = 0;

  for (j=0; j<height2; j+=16)
    for (i=0; i<width; i+=16)
    {
      for (n=0; n<block_count; n++)
      {
        cc = (n<4) ? 0 : (n&1)+1; /* color component index */

        if (cc==0)
        {
          /* luminance */
          if ((pict_struct==FRAME_PICTURE) && mbi[k].dct_type)
          {
            /* field DCT */
            offs = i + ((n&1)<<3) + width*(j+((n&2)>>1));
            lx = width<<1;
          }
          else
          {
            /* frame DCT */
            offs = i + ((n&1)<<3) + width2*(j+((n&2)<<2));
            lx = width2;
          }

          if (pict_struct==BOTTOM_FIELD)
            offs += width;
        }
        else
        {
          /* chrominance */

          /* scale coordinates */
          i1 = (chroma_format==CHROMA444) ? i : i>>1;
          j1 = (chroma_format!=CHROMA420) ? j : j>>1;

          if ((pict_struct==FRAME_PICTURE) && mbi[k].dct_type
              && (chroma_format!=CHROMA420))
          {
            /* field DCT */
            offs = i1 + (n&8) + chrom_width*(j1+((n&2)>>1));
            lx = chrom_width<<1;
          }
          else
          {
            /* frame DCT */
            offs = i1 + (n&8) + chrom_width2*(j1+((n&2)<<2));
            lx = chrom_width2;
          }

          if (pict_struct==BOTTOM_FIELD)
            offs += chrom_width;
        }

        (*idct_sub)(blocks[k*block_count+n]);
        add_pred(pred[cc]+offs,cur[cc]+offs,lx,blocks[k*block_count+n]);
      }

      k++;
    }
}

/* add prediction and prediction error, saturate to 0...255 */
void add_pred(unsigned char *pred,
                           unsigned char *cur,
                           int lx, short *blk)
{
  int i, j;

  for (j=0; j<8; j++)
  {
    for (i=0; i<8; i++)
      cur[i] = clp[blk[i] + pred[i]];
    blk+= 8;
    cur+= lx;
    pred+= lx;
  }
}

/* subtract prediction from block data */
void sub_pred(unsigned char *pred,
                           unsigned char *cur,
                           int lx, short *blk)
{
  int i, j;

  for (j=0; j<8; j++)
  {
    for (i=0; i<8; i++)
      blk[i] = cur[i] - pred[i];
    blk+= 8;
    cur+= lx;
    pred+= lx;
  }
}

/*
 * select between frame and field DCT
 *
 * preliminary version: based on inter-field correlation
 */
void dct_type_estimation(unsigned char *pred,
                                      unsigned char *cur,
                                      struct mbinfo *mbi)
{
  short blk0[128], blk1[128];
  int i, j, i0, j0, k, offs, s0, s1, sq0, sq1, s01;
  double d, r;

  k = 0;

  for (j0=0; j0<height2; j0+=16)
    for (i0=0; i0<width; i0+=16)
    {
      if (frame_pred_dct || pict_struct!=FRAME_PICTURE)
        mbi[k].dct_type = 0;
      else
      {
        /* interlaced frame picture */
        /*
         * calculate prediction error (cur-pred) for top (blk0)
         * and bottom field (blk1)
         */
        for (j=0; j<8; j++)
        {
          offs = width*((j<<1)+j0) + i0;
          for (i=0; i<16; i++)
          {
            blk0[16*j+i] = cur[offs] - pred[offs];
            blk1[16*j+i] = cur[offs+width] - pred[offs+width];
            offs++;
          }
        }
        /* correlate fields */
        s0=s1=sq0=sq1=s01=0;

        for (i=0; i<128; i++)
        {
          s0+= blk0[i];
          sq0+= blk0[i]*blk0[i];
          s1+= blk1[i];
          sq1+= blk1[i]*blk1[i];
          s01+= blk0[i]*blk1[i];
        }

        d = (sq0-(s0*s0)/128.0)*(sq1-(s1*s1)/128.0);

        if (d>0.0)
        {
          r = (s01-(s0*s1)/128.0)/sqrt(d);
          if (r>0.5)
            mbi[k].dct_type = 0; /* frame DCT */
          else
            mbi[k].dct_type = 1; /* field DCT */
        }
        else
          mbi[k].dct_type = 1; /* field DCT */
      }
      k++;
    }
}
