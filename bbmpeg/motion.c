/* motion.c, motion estimation                                              */

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
#include "mtable.h"
#include "../aclib/ac.h"

extern int tc_accel;

/* private prototypes */

static void frame_ME(unsigned char *oldorg, unsigned char *neworg,
      unsigned char *oldref, unsigned char *newref, unsigned char *cur,
      int i, int j, int sxf, int syf, int sxb, int syb, struct mbinfo *mbi);

static void field_ME(unsigned char *oldorg, unsigned char *neworg,
      unsigned char *oldref, unsigned char *newref, unsigned char *cur,
      unsigned char *curref, int i, int j, int sxf, int syf, int sxb, int syb,
      struct mbinfo *mbi, int secondfield, int ipflag);

static void frame_estimate(unsigned char *org,
      unsigned char *ref, unsigned char *mb,
      int i, int j,
      int sx, int sy, int *iminp, int *jminp, int *imintp, int *jmintp,
      int *iminbp, int *jminbp, int *dframep, int *dfieldp,
      int *tselp, int *bselp, int imins[2][2], int jmins[2][2]);

static void field_estimate(unsigned char *toporg,
      unsigned char *topref, unsigned char *botorg, unsigned char *botref,
      unsigned char *mb, int i, int j, int sx, int sy, int ipflag,
      int *iminp, int *jminp, int *imin8up, int *jmin8up, int *imin8lp,
      int *jmin8lp, int *dfieldp, int *d8p, int *selp, int *sel8up, int *sel8lp,
      int *iminsp, int *jminsp, int *dsp);

static void dpframe_estimate(unsigned char *ref,
      unsigned char *mb, int i, int j, int iminf[2][2], int jminf[2][2],
      int *iminp, int *jminp, int *imindmvp, int *jmindmvp,
      int *dmcp, int *vmcp);

static void dpfield_estimate(unsigned char *topref,
      unsigned char *botref, unsigned char *mb,
      int i, int j, int imins, int jmins, int *imindmvp, int *jmindmvp,
      int *dmcp, int *vmcp);

static int fullsearch(unsigned char *org, unsigned char *ref,
  unsigned char *blk,
  int lx, int i0, int j0, int sx, int sy, int h, int xmax, int ymax,
  int *iminp, int *jminp);

static int dist1(unsigned char *blk1, unsigned char *blk2,
  int lx, int hx, int hy, int h, int distlim);

static int dist2(unsigned char *blk1, unsigned char *blk2,
  int lx, int hx, int hy, int h);

static int bdist1(unsigned char *pf, unsigned char *pb,
  unsigned char *p2, int lx, int hxf, int hyf, int hxb, int hyb, int h);

static int bdist2(unsigned char *pf, unsigned char *pb,
  unsigned char *p2, int lx, int hxf, int hyf, int hxb, int hyb, int h);

static int variance(unsigned char *p, int lx);

static int edist1(unsigned char *blk1, unsigned char *blk2,
  int lx, int distlim);

static int (*dist1sub) (unsigned char *blk1, unsigned char *blk2,
  int lx, int hx, int hy, int h, int distlim);

static int (*dist2sub) (unsigned char *blk1, unsigned char *blk2,
  int lx, int hx, int hy, int h);

static int (*bdist1sub) (unsigned char *pf, unsigned char *pb,
  unsigned char *p2, int lx, int hxf, int hyf, int hxb, int hyb, int h);

static int (*bdist2sub) (unsigned char *pf, unsigned char *pb,
  unsigned char *p2, int lx, int hxf, int hyf, int hxb, int hyb, int h);

static int (*variance_sub) (unsigned char *p, int lx);

static int (*edist1sub) (unsigned char *blk1, unsigned char *blk2,
  int lx, int distlim);


int *mv_org0, *mv_org1; // mv_org[396][2];
int *mv_new0, *mv_new1; // mv_new[396][2];

int PRUNE_MV, PRUNE_MV_BOT, w4, h4, h42, wh4, wh42, mblok, forwEst, downSampleLim, *lumij;

unsigned char *Old, *New, *Cur;

//GM
extern int dist1mmx(unsigned char *blk1, unsigned char *blk2,
		    int lx, int hx, int hy, int h, int distlim);

//ThOe
extern int dist1_mmx(unsigned char *blk1, unsigned char *blk2,
		     int lx, int hx, int hy, int h, int distlim);
     
//GM
extern int dist1sse(unsigned char *blk1, unsigned char *blk2,
			 int lx, int hx, int hy, int h, int distlim);

//ThOe
extern int dist1_sse(unsigned char *blk1, unsigned char *blk2,
			 int lx, int hx, int hy, int h, int distlim);
     
extern int dist2mmx(unsigned char *blk1, unsigned char *blk2,
		    int lx, int hx, int hy, int h);

extern int bdist1mmx(unsigned char *pf, unsigned char *pb, 
		     unsigned char *p2, int lx, 
		     int hxf, int hyf, int hxb, int hyb, int h);

extern int bdist1sse(unsigned char *pf, unsigned char *pb, 
		     unsigned char *p2, int lx, 
		     int hxf, int hyf, int hxb, int hyb, int h);

extern int bdist2mmx(unsigned char *pf, unsigned char *pb, 
			 unsigned char *p2, int lx, 
			 int hxf, int hyf, int hxb, int hyb, int h);

extern  int edist1mmx(unsigned char *blk1, unsigned char *blk2,
		      int lx, int distlim);
//GM
extern int variancemmx(unsigned char *p,int lx);

//ThOe
extern int variance_16x16_mmx(unsigned char *p, int lx);

/* setup MMX, SIMD, etc */

void init_motion_est(int sh_info)
{

  //default for all
  dist1sub     = dist1;
  dist2sub     = dist2;
  bdist1sub    = bdist1;
  bdist2sub    = bdist2;
  edist1sub    = edist1;
  variance_sub = variance;
  
#ifdef ARCH_X86
#ifdef HAVE_ASM_NASM
  
  if(tc_accel & MM_MMX) {
    dist1sub = dist1mmx;
    dist2sub = dist2mmx;
    bdist1sub = bdist1mmx;
    //bdist2sub = bdist2mmx;    //segfaults
    edist1sub = edist1mmx;
    //variance_sub = variancemmx;    //picture quality problems with "-F v,1"
  }
  
#ifdef HAVE_SSE
  if(tc_accel & MM_SSE || tc_accel & MM_SSE2) {
    dist1sub = dist1sse;
    bdist1sub = bdist1sse;
    edist1sub = edist1sse;
  }
#endif

#endif
#endif
}


int init_motion_est2(int sh_info)
{
  w4 = width >> 2;
  h4 = height >> 2;
  h42 = height2 >> 2;
  wh4 = w4 * h4;
  wh42 = w4 * h42;
  mv_org0 = (int*)malloc((w4 >> 2) * (h4 >> 2) * sizeof(int));
  mv_org1 = (int*)malloc((w4 >> 2) * (h4 >> 2) * sizeof(int));
  mv_new0 = (int*)malloc((w4 >> 2) * (h4 >> 2) * sizeof(int));
  mv_new1 = (int*)malloc((w4 >> 2) * (h4 >> 2) * sizeof(int));
  if ((mv_org0 == NULL) || (mv_org1 == NULL) || (mv_new0 == NULL) || (mv_new1 == NULL))
  {
    DisplayError("Cannot allocate memory for motion vector tables.");
    return FALSE;
  }

  Old = (unsigned char*)malloc(wh4);
  New = (unsigned char*)malloc(wh4);
  Cur = (unsigned char*)malloc(wh4);
  lumij = (int*)malloc(wh4 * sizeof(int));
  if ((Old == NULL) || (New == NULL) || (Cur == NULL) || (lumij == NULL))
  {
    DisplayError("Cannot allocate memory for subsample images.");
    return FALSE;
  }
  downSampleLim = width * height2 * 20;
  return TRUE;
}


void finish_motion_est()
{
  if (mv_org0)
    free(mv_org0);
  if (mv_org1)
    free(mv_org1);
  if (mv_new0)
    free(mv_new0);
  if (mv_new1)
    free(mv_new1);
  if (lumij)
    free(lumij);

  if (Old)
    free(Old);
  if (New)
    free(New);
  if (Cur)
    free(Cur);
}

static char tbl[9] = {0, 1, 1, 0, 1, 0, 0, 0, 1};

static void downsample_frame()
{
  int mbh, mbv, mto;
  int b;
  int x, y;
  int orx, ory;
  int ix, iy, fx, fy;
  int sxyf;
  int sxf, syf;
  int s;
  int mini, minj;
  unsigned char *o4, *r4, *bo, *br;
  int dmin, l, j, i, k;
  int prmv;
//  char tmpStr[256];

  if (pict_type >= P_TYPE)
  {
    mbh = w4 / 4;
    mbv = h4 / 4;
    mto = mbh * mbv;
    o4 = Cur;
    r4 = Old;

    for (b = 0; b < mto; b++)
    {
      y = b / mbh;
      x = b - y * mbh;
      orx = x * 4;
      ory = y * 4;

      sxf = syf = submotiop;
      ix = orx - sxf;
      if (ix < 0)
	ix = 0;
      fx = orx + sxf - 1;
      if (fx > w4 - 4)
	fx = w4 - 4;
      iy = ory - syf;
      if (iy < 0)
	iy = 0;
      fy = ory + syf - 1;
      if (fy > h4 - 4)
	fy = h4 - 4;

      dmin = 65536;
      bo = o4 + width * y + orx;
      br = r4 + ory * w4 + orx;
      s = (*edist1sub)(bo, br, w4, dmin);
      dmin = s;
      mini = orx;
      minj = ory;

      prmv = 0;
      sxyf = (sxf > syf) ? sxf : syf;
      for (l = 1; l <= sxyf; l++)
      {
	i = ory - l;
	j = orx - l;
	for (k = 0; k < 8 * l; k++)
	{
	  if (i >= iy && i < fy && j >= ix && j < fx)
	  {
	    br = r4 + i * w4 + j;
	    bo = o4 + width * y + orx;
	    s = (*edist1sub)(bo, br, w4, dmin);
	    if (s < dmin)
	    {
	      dmin = s;
	      mini = j;
	      minj = i;
	      prmv = 0;
	      if (dmin == 0)
	        break;
	    }
	  }

	  if (k < 2 * l)
	    j++;
	  else
            if (k < 4 * l)
	      i++;
	    else
              if (k < 6 * l)
		j--;
	      else
	        i--;
	}
	if (dmin == 0)
	  break;
        if (l < 9)
        {
	  if (tbl[l])
	  {
	    prmv++;
	    if (prmv > PRUNE_MV)
	      break;
          }
	}
      }
//      sprintf(tmpStr, "EBLOCK %d   MINI: %d  MINJ: %d  vector: %d,%d  DMIN: %d", b, mini, minj, 8 * (mini - orx), 8 * (minj - ory), dmin);
//      DisplayInfo(tmpStr);
      mv_org0[b] = 4 * mini;
      mv_org1[b] = 4 * minj;
    }
  }

  if (pict_type == B_TYPE)
  {
    mbh = w4 / 4;
    mbv = h4 / 4;
    mto = mbh * mbv;
    o4 = Cur;
    r4 = New;

    for (b = 0; b < mto; b++)
    {
      y = b / mbh;
      x = b - y * mbh;
      orx = x * 4;
      ory = y * 4;

      sxf = syf = submotiob;

      ix = orx - sxf;
      if (ix < 0)
	ix = 0;
      fx = orx + sxf - 1;
      if (fx > w4 - 4)
	fx = w4 - 4;
      iy = ory - syf;
      if (iy < 0)
	iy = 0;
      fy = ory + syf - 1;
      if (fy > h4 - 4)
	fy = h4 - 4;

      dmin = 65536;
      bo = o4 + width * y + orx;
      br = r4 + ory * w4 + orx;
      s = (*edist1sub)(bo, br, w4, dmin);
      dmin = s;
      mini = orx;
      minj = ory;

      prmv = 0;

      sxyf = (sxf > syf) ? sxf : syf;
      for (l = 1; l <= sxyf; l++)
      {
	i = ory - l;
	j = orx - l;
	for (k = 0; k < 8 * l; k++)
	{
	  if (i >= iy && i < fy && j >= ix && j < fx)
	  {
	    br = r4 + i * w4 + j;
	    bo = o4 + width * y + orx;
	    s = (*edist1sub)(bo, br, w4, dmin);
	    if (s < dmin)
	    {
	      dmin = s;
	      mini = j;
	      minj = i;
	      prmv = 0;
	      if (dmin == 0)
	        break;
	    }
	  }

	  if (k < 2 * l)
	    j++;
	  else
            if (k < 4 * l)
	      i++;
	    else
              if (k < 6 * l)
		j--;
	      else
		i--;
	}
	if (dmin == 0)
	  break;
        if (l < 9)
        {
	  if (tbl[l])
	  {
	    prmv++;
	    if (prmv > PRUNE_MV)
	      break;
          }
	}
      }
//      sprintf(tmpStr, "EBLOCK %d   MINI: %d  MINJ: %d  vector: %d,%d  DMIN: %d", b, mini, minj, 8 * (mini - orx), 8 * (minj - ory), dmin);
//      DisplayInfo(tmpStr);
      mv_new0[b] = 4 * mini;
      mv_new1[b] = 4 * minj;
    }
  }
}


static void downsample_field(int secondfield, int ipflag)
{
  int mbh, mbv, mto;
  int b;
  int x, y, lx;
  int orx, ory;
  int ix, iy, fx, fy;
  int sxyf;
  int sxf, syf;
  int s;
  int mini, minj;
  unsigned char *o4, *r4, *bo, *br;
  int dmin, l, j, i, k;
  int prmv, prune_val;
//  char tmpStr[256];

  lx = w4 << 1;
  if (secondfield)
    prune_val = PRUNE_MV_BOT;
  else
    prune_val = PRUNE_MV;

  if ((pict_type >= P_TYPE) || ipflag)
  {
    mbh = w4 / 4;
    mbv = h42 / 4;
    mto = mbh * mbv;
    if (secondfield)
    {
      o4 = Cur + w4;
      r4 = Old + w4;
    }
    else
    {
      o4 = Cur;
      r4 = Old;
    }

    for (b = 0; b < mto; b++)
    {
      y = b / mbh;
      x = b - y * mbh;
      orx = x * 4;
      ory = y * 4;

      sxf = syf = submotiop >> 1;
      ix = orx - sxf;
      if (ix < 0)
	ix = 0;
      fx = orx + sxf - 1;
      if (fx > w4 - 4)
	fx = w4 - 4;
      iy = ory - syf;
      if (iy < 0)
	iy = 0;
      fy = ory + syf - 1;
      if (fy > h42 - 4)
	fy = h42 - 4;

      dmin = 65536;
      bo = o4 + ory * lx + orx;
      br = r4 + ory * lx + orx;
      s = (*edist1sub)(bo, br, lx, dmin);
      dmin = s;
      mini = orx;
      minj = ory;

      prmv = 0;
      sxyf = (sxf > syf) ? sxf : syf;
      for (l = 1; l <= sxyf; l++)
      {
	i = ory - l;
	j = orx - l;
	for (k = 0; k < 8 * l; k++)
	{
	  if (i >= iy && i < fy && j >= ix && j < fx)
	  {
	    br = r4 + i * lx + j;
	    bo = o4 + ory * lx + orx;
	    s = (*edist1sub)(bo, br, lx, dmin);
	    if (s < dmin)
	    {
	      dmin = s;
	      mini = j;
	      minj = i;
	      prmv = 0;
	      if (dmin == 0)
	        break;
	    }
	  }

	  if (k < 2 * l)
	    j++;
	  else
            if (k < 4 * l)
	      i++;
	    else
              if (k < 6 * l)
		j--;
	      else
	        i--;
	}
	if (dmin == 0)
	  break;
        if (l < 9)
        {
	  if (tbl[l])
	  {
	    prmv++;
	    if (prmv > prune_val)
	      break;
          }
	}
      }
//      sprintf(tmpStr, "EBLOCK %d   MINI: %d  MINJ: %d  vector: %d,%d  DMIN: %d", b, mini, minj, 8 * (mini - orx), 8 * (minj - ory), dmin);
//      DisplayInfo(tmpStr);
      mv_org0[b] = 4 * mini;
      mv_org1[b] = 4 * minj;
    }
  }

  if (pict_type == B_TYPE)
  {
    mbh = w4 / 4;
    mbv = h42 / 4;
    mto = mbh * mbv;
    if (secondfield)
    {
      o4 = Cur + w4;
      r4 = New + w4;
    }
    else
    {
      o4 = Cur;
      r4 = New;
    }

    for (b = 0; b < mto; b++)
    {
      y = b / mbh;
      x = b - y * mbh;
      orx = x * 4;
      ory = y * 4;

      sxf = syf = submotiob >> 1;

      ix = orx - sxf;
      if (ix < 0)
	ix = 0;
      fx = orx + sxf - 1;
      if (fx > w4 - 4)
	fx = w4 - 4;
      iy = ory - syf;
      if (iy < 0)
	iy = 0;
      fy = ory + syf - 1;
      if (fy > h42 - 4)
	fy = h42 - 4;

      dmin = 65536;
      bo = o4 + ory * lx + orx;
      br = r4 + ory * lx + orx;
      s = (*edist1sub)(bo, br, lx, dmin);
      dmin = s;
      mini = orx;
      minj = ory;

      prmv = 0;

      sxyf = (sxf > syf) ? sxf : syf;
      for (l = 1; l <= sxyf; l++)
      {
	i = ory - l;
	j = orx - l;
	for (k = 0; k < 8 * l; k++)
	{
	  if (i >= iy && i < fy && j >= ix && j < fx)
	  {
	    br = r4 + i * lx + j;
	    bo = o4 + ory * lx + orx;
	    s = (*edist1sub)(bo, br, lx, dmin);
	    if (s < dmin)
	    {
	      dmin = s;
	      mini = j;
	      minj = i;
	      prmv = 0;
	      if (dmin == 0)
	        break;
	    }
	  }

	  if (k < 2 * l)
	    j++;
	  else
            if (k < 4 * l)
	      i++;
	    else
              if (k < 6 * l)
		j--;
	      else
		i--;
	}
	if (dmin == 0)
	  break;
        if (l < 9)
        {
	  if (tbl[l])
	  {
	    prmv++;
	    if (prmv > prune_val)
	      break;
          }
	}
      }
//      sprintf(tmpStr, "EBLOCK %d   MINI: %d  MINJ: %d  vector: %d,%d  DMIN: %d", b, mini, minj, 8 * (mini - orx), 8 * (minj - ory), dmin);
//      DisplayInfo(tmpStr);
      mv_new0[b] = 4 * mini;
      mv_new1[b] = 4 * minj;
    }
  }
}


/*
 * motion estimation for progressive and interlaced frame pictures
 *
 * oldorg: source frame for forward prediction (used for P and B frames)
 * neworg: source frame for backward prediction (B frames only)
 * oldref: reconstructed frame for forward prediction (P and B frames)
 * newref: reconstructed frame for backward prediction (B frames only)
 * cur:    current frame (the one for which the prediction is formed)
 * sxf,syf: forward search window (frame coordinates)
 * sxb,syb: backward search window (frame coordinates)
 * mbi:    pointer to macroblock info structure
 *
 * results:
 * mbi->
 *  mb_type: 0, MB_INTRA, MB_FORWARD, MB_BACKWARD, MB_FORWARD|MB_BACKWARD
 *  MV[][][]: motion vectors (frame format)
 *  mv_field_sel: top/bottom field (for field prediction)
 *  motion_type: MC_FRAME, MC_FIELD
 *
 * uses global vars: pict_type, frame_pred_dct
 */
int motion_estimation(
unsigned char *oldorg, unsigned char *neworg, unsigned char *oldref,
unsigned char *newref, unsigned char *cur, unsigned char *curref,
int sxf, int syf, int sxb, int syb,
struct mbinfo *mbi,
int secondfield, int ipflag)
{
  int i, j;
  int nh1, nh2, nh3, nmb, ntmb, lumi, myvar;
  unsigned char *or, *qor;

  if (maxmotion > 7)
  {
    if ((pict_struct == FRAME_PICTURE) || !secondfield)
    {
      qor = Cur;
      or = cur;
      nmb = 0;
      ntmb = wh42;
      lumi = 0;

      if (pict_struct == FRAME_PICTURE)
      {
        nh1 = width;
        nh2 = width * 2;
        nh3 = width * 3;
      }
      else
      {
        nh1 = width * 2;
        nh2 = width * 4;
        nh3 = width * 6;
      }

      // do frame or top field
      for (j = 0; j < h42; j++)
      {
        for (i = 0; i < w4; i++)
        {
          *qor = (*(or + 0) + *(or + 1) + *(or + 2) + *(or + 3) +
                  *(or + 0 + nh1) + *(or + 1 + nh1) + *(or + 2 + nh1) + *(or + 3 + nh1) +
                  *(or + 0 + nh2) + *(or + 1 + nh2) + *(or + 2 + nh2) + *(or + 3 + nh2) +
                  *(or + 0 + nh3) + *(or + 1 + nh3) + *(or + 2 + nh3) + *(or + 3 + nh3) + 8) / 16;
          lumi += *qor;
          lumij[nmb++] = *qor;
          qor++;
          or += 4;
        }
        or += nh3;
        if (pict_struct != FRAME_PICTURE)
          qor += width;
      }

      lumi /= ntmb;
      myvar = 0;
      for (i = 0; i < ntmb; i++)
        myvar += (lumi - lumij[i]) * (lumi - lumij[i]);

      if (myvar < downSampleLim)
        PRUNE_MV = 1;
      else
        PRUNE_MV = 2;

      if (pict_struct != FRAME_PICTURE)
      {
        nmb = 0;
        lumi = 0;
        qor = Cur + w4;
        or = cur + width;

        // do bottom field
        for (j = 0; j < h42; j++)
        {
          for (i = 0; i < w4; i++)
          {
            *qor = (*(or + 0) + *(or + 1) + *(or + 2) + *(or + 3) +
                    *(or + 0 + nh1) + *(or + 1 + nh1) + *(or + 2 + nh1) + *(or + 3 + nh1) +
                    *(or + 0 + nh2) + *(or + 1 + nh2) + *(or + 2 + nh2) + *(or + 3 + nh2) +
                    *(or + 0 + nh3) + *(or + 1 + nh3) + *(or + 2 + nh3) + *(or + 3 + nh3) + 8) / 16;
            lumi += *qor;
            lumij[nmb++] = *qor;
            qor++;
            or += 4;
          }
          or += nh3;
          qor += width;
        }

        lumi /= ntmb;
        myvar = 0;
        for (i = 0; i < ntmb; i++)
          myvar += (lumi - lumij[i]) * (lumi - lumij[i]);

        if (myvar < downSampleLim)
          PRUNE_MV_BOT = 1;
        else
          PRUNE_MV_BOT = 2;
      }

      if (pict_type <= 2)
      {
        memcpy(Old, New, wh4);
        memcpy(New, Cur, wh4);
      }
    }
    if ((pict_type != I_TYPE) || (ipflag))
      if (pict_struct == FRAME_PICTURE)
        downsample_frame();
      else
        downsample_field(secondfield, ipflag);
  }
  mblok = 0;

  /* loop through all macroblocks of the picture */
  for (j=0; j<height2; j+=16)
  {
    for (i=0; i<width; i+=16)
    {
      if (pict_struct==FRAME_PICTURE)
        frame_ME(oldorg,neworg,oldref,newref,cur,i,j,sxf,syf,sxb,syb,mbi);
      else
        field_ME(oldorg,neworg,oldref,newref,cur,curref,i,j,sxf,syf,sxb,syb,
          mbi,secondfield,ipflag);
      mblok++;
      mbi++;
    }
  }
  return TRUE;
}

void frame_ME(
unsigned char *oldorg, unsigned char *neworg, unsigned char *oldref,
unsigned char *newref, unsigned char *cur,
int i, int j, int sxf, int syf, int sxb, int syb,
struct mbinfo *mbi)
{
  int imin,jmin,iminf,jminf,iminr,jminr;
  int imint,jmint,iminb,jminb;
  int imintf,jmintf,iminbf,jminbf;
  int imintr,jmintr,iminbr,jminbr;
  int var,v0,vmc1;
  int dmc,dmcf,dmcr,dmci,vmc,vmcf,vmcr,vmci;
  int dmcfield,dmcfieldf,dmcfieldr,dmcfieldi;
  int tsel,bsel,tself,bself,tselr,bselr;
  unsigned char *mb;
  int imins[2][2],jmins[2][2];
  int imindp,jmindp,imindmv,jmindmv,dmc_dp,vmc_dp;

  mb = cur + i + width*j;

  var = (*variance_sub)(mb,width);

  if (pict_type==I_TYPE)
    mbi->mb_type = MB_INTRA;
  else if (pict_type==P_TYPE)
  {
    forwEst = 1;
    if (frame_pred_dct)
    {
      dmc = fullsearch(oldorg,oldref,mb,
                       width,i,j,sxf,syf,16,width,height,&imin,&jmin);
      vmc = (*dist2sub)(oldref+(imin>>1)+width*(jmin>>1),mb,
                  width,imin&1,jmin&1,16);
      mbi->motion_type = MC_FRAME;
    }
    else
    {
      frame_estimate(oldorg,oldref,mb,i,j,sxf,syf,
        &imin,&jmin,&imint,&jmint,&iminb,&jminb,
        &dmc,&dmcfield,&tsel,&bsel,imins,jmins);

      if (M==1)
        dpframe_estimate(oldref,mb,i,j>>1,imins,jmins,
          &imindp,&jmindp,&imindmv,&jmindmv,&dmc_dp,&vmc_dp);

      /* select between dual prime, frame and field prediction */
      if (M==1 && dmc_dp<dmc && dmc_dp<dmcfield)
      {
        mbi->motion_type = MC_DMV;
        dmc = dmc_dp;
        vmc = vmc_dp;
      }
      else if (dmc<=dmcfield)
      {
        mbi->motion_type = MC_FRAME;
        vmc = (*dist2sub)(oldref+(imin>>1)+width*(jmin>>1),mb,
                    width,imin&1,jmin&1,16);
      }
      else
      {
        mbi->motion_type = MC_FIELD;
        dmc = dmcfield;
        vmc = (*dist2sub)(oldref+(tsel?width:0)+(imint>>1)+(width<<1)*(jmint>>1),
                    mb,width<<1,imint&1,jmint&1,8);
        vmc+= (*dist2sub)(oldref+(bsel?width:0)+(iminb>>1)+(width<<1)*(jminb>>1),
                    mb+width,width<<1,iminb&1,jminb&1,8);
      }
    }

    /* select between intra or non-intra coding:
     *
     * selection is based on intra block variance (var) vs.
     * prediction error variance (vmc)
     *
     * blocks with small prediction error are always coded non-intra
     * even if variance is smaller (is this reasonable?)
     */
    vmc1 = vmc;
    if (maxmotion > 7)
      vmc1 <<= 1;

    if (vmc1>var && vmc>=9*256)
      mbi->mb_type = MB_INTRA;
    else
    {
      /* select between MC / No-MC
       *
       * use No-MC if var(No-MC) <= 1.25*var(MC)
       * (i.e slightly biased towards No-MC)
       *
       * blocks with small prediction error are always coded as No-MC
       * (requires no motion vectors, allows skipping)
       */
      v0 = (*dist2sub)(oldref+i+width*j,mb,width,0,0,16);
      if (4*v0>5*vmc && v0>=9*256)
      {
        /* use MC */
        var = vmc;
        mbi->mb_type = MB_FORWARD;
        if (mbi->motion_type==MC_FRAME)
        {
          mbi->MV[0][0][0] = imin - (i<<1);
          mbi->MV[0][0][1] = jmin - (j<<1);
        }
        else if (mbi->motion_type==MC_DMV)
        {
          /* these are FRAME vectors */
          /* same parity vector */
          mbi->MV[0][0][0] = imindp - (i<<1);
          mbi->MV[0][0][1] = (jmindp<<1) - (j<<1);

          /* opposite parity vector */
          mbi->dmvector[0] = imindmv;
          mbi->dmvector[1] = jmindmv;
        }
        else
        {
          /* these are FRAME vectors */
          mbi->MV[0][0][0] = imint - (i<<1);
          mbi->MV[0][0][1] = (jmint<<1) - (j<<1);
          mbi->MV[1][0][0] = iminb - (i<<1);
          mbi->MV[1][0][1] = (jminb<<1) - (j<<1);
          mbi->mv_field_sel[0][0] = tsel;
          mbi->mv_field_sel[1][0] = bsel;
        }
      }
      else
      {
        /* No-MC */
        var = v0;
        mbi->mb_type = 0;
        mbi->motion_type = MC_FRAME;
        mbi->MV[0][0][0] = 0;
        mbi->MV[0][0][1] = 0;
      }
    }
  }
  else /* if (pict_type==B_TYPE) */
  {
    if (frame_pred_dct)
    {
      /* forward */
      forwEst = 1;
      dmcf = fullsearch(oldorg,oldref,mb,
                        width,i,j,sxf,syf,16,width,height,&iminf,&jminf);
      vmcf = (*dist2sub)(oldref+(iminf>>1)+width*(jminf>>1),mb,
                   width,iminf&1,jminf&1,16);
      forwEst = 0;
      dmcr = fullsearch(neworg,newref,mb,
                        width,i,j,sxb,syb,16,width,height,&iminr,&jminr);
      vmcr = (*dist2sub)(newref+(iminr>>1)+width*(jminr>>1),mb,
                   width,iminr&1,jminr&1,16);

      /* interpolated (bidirectional) */
      vmci = (*bdist2sub)(oldref+(iminf>>1)+width*(jminf>>1),
                    newref+(iminr>>1)+width*(jminr>>1),
                    mb,width,iminf&1,jminf&1,iminr&1,jminr&1,16);

      /* decisions */

      /* select between forward/backward/interpolated prediction:
       * use the one with smallest mean sqaured prediction error
       */
      if (vmcf<=vmcr && vmcf<=vmci)
      {
        vmc = vmcf;
        mbi->mb_type = MB_FORWARD;
      }
      else if (vmcr<=vmci)
      {
        vmc = vmcr;
        mbi->mb_type = MB_BACKWARD;
      }
      else
      {
        vmc = vmci;
        mbi->mb_type = MB_FORWARD|MB_BACKWARD;
      }

      mbi->motion_type = MC_FRAME;
    }
    else
    {
      /* forward prediction */
      forwEst = 1;
      frame_estimate(oldorg,oldref,mb,i,j,sxf,syf,
        &iminf,&jminf,&imintf,&jmintf,&iminbf,&jminbf,
        &dmcf,&dmcfieldf,&tself,&bself,imins,jmins);

      /* backward prediction */
      forwEst = 0;
      frame_estimate(neworg,newref,mb,i,j,sxb,syb,
        &iminr,&jminr,&imintr,&jmintr,&iminbr,&jminbr,
        &dmcr,&dmcfieldr,&tselr,&bselr,imins,jmins);

      /* calculate interpolated distance */
      /* frame */
      dmci = (*bdist1sub)(oldref+(iminf>>1)+width*(jminf>>1),
                    newref+(iminr>>1)+width*(jminr>>1),
                    mb,width,iminf&1,jminf&1,iminr&1,jminr&1,16);

      /* top field */
      dmcfieldi = (*bdist1sub)(
                    oldref+(imintf>>1)+(tself?width:0)+(width<<1)*(jmintf>>1),
                    newref+(imintr>>1)+(tselr?width:0)+(width<<1)*(jmintr>>1),
                    mb,width<<1,imintf&1,jmintf&1,imintr&1,jmintr&1,8);

      /* bottom field */
      dmcfieldi+= (*bdist1sub)(
                    oldref+(iminbf>>1)+(bself?width:0)+(width<<1)*(jminbf>>1),
                    newref+(iminbr>>1)+(bselr?width:0)+(width<<1)*(jminbr>>1),
                    mb+width,width<<1,iminbf&1,jminbf&1,iminbr&1,jminbr&1,8);

      /* select prediction type of minimum distance from the
       * six candidates (field/frame * forward/backward/interpolated)
       */
      if (dmci<dmcfieldi && dmci<dmcf && dmci<dmcfieldf
          && dmci<dmcr && dmci<dmcfieldr)
      {
        /* frame, interpolated */
        mbi->mb_type = MB_FORWARD|MB_BACKWARD;
        mbi->motion_type = MC_FRAME;
        vmc = (*bdist2sub)(oldref+(iminf>>1)+width*(jminf>>1),
                     newref+(iminr>>1)+width*(jminr>>1),
                     mb,width,iminf&1,jminf&1,iminr&1,jminr&1,16);
      }
      else if (dmcfieldi<dmcf && dmcfieldi<dmcfieldf
               && dmcfieldi<dmcr && dmcfieldi<dmcfieldr)
      {
        /* field, interpolated */
        mbi->mb_type = MB_FORWARD|MB_BACKWARD;
        mbi->motion_type = MC_FIELD;
        vmc = (*bdist2sub)(oldref+(imintf>>1)+(tself?width:0)+(width<<1)*(jmintf>>1),
                     newref+(imintr>>1)+(tselr?width:0)+(width<<1)*(jmintr>>1),
                     mb,width<<1,imintf&1,jmintf&1,imintr&1,jmintr&1,8);
        vmc+= (*bdist2sub)(oldref+(iminbf>>1)+(bself?width:0)+(width<<1)*(jminbf>>1),
                     newref+(iminbr>>1)+(bselr?width:0)+(width<<1)*(jminbr>>1),
                     mb+width,width<<1,iminbf&1,jminbf&1,iminbr&1,jminbr&1,8);
      }
      else if (dmcf<dmcfieldf && dmcf<dmcr && dmcf<dmcfieldr)
      {
        /* frame, forward */
        mbi->mb_type = MB_FORWARD;
        mbi->motion_type = MC_FRAME;
        vmc = (*dist2sub)(oldref+(iminf>>1)+width*(jminf>>1),mb,
                    width,iminf&1,jminf&1,16);
      }
      else if (dmcfieldf<dmcr && dmcfieldf<dmcfieldr)
      {
        /* field, forward */
        mbi->mb_type = MB_FORWARD;
        mbi->motion_type = MC_FIELD;
        vmc = (*dist2sub)(oldref+(tself?width:0)+(imintf>>1)+(width<<1)*(jmintf>>1),
                    mb,width<<1,imintf&1,jmintf&1,8);
        vmc+= (*dist2sub)(oldref+(bself?width:0)+(iminbf>>1)+(width<<1)*(jminbf>>1),
                    mb+width,width<<1,iminbf&1,jminbf&1,8);
      }
      else if (dmcr<dmcfieldr)
      {
        /* frame, backward */
        mbi->mb_type = MB_BACKWARD;
        mbi->motion_type = MC_FRAME;
        vmc = (*dist2sub)(newref+(iminr>>1)+width*(jminr>>1),mb,
                    width,iminr&1,jminr&1,16);
      }
      else
      {
        /* field, backward */
        mbi->mb_type = MB_BACKWARD;
        mbi->motion_type = MC_FIELD;
        vmc = (*dist2sub)(newref+(tselr?width:0)+(imintr>>1)+(width<<1)*(jmintr>>1),
                    mb,width<<1,imintr&1,jmintr&1,8);
        vmc+= (*dist2sub)(newref+(bselr?width:0)+(iminbr>>1)+(width<<1)*(jminbr>>1),
                    mb+width,width<<1,iminbr&1,jminbr&1,8);
      }
    }

    /* select between intra or non-intra coding:
     *
     * selection is based on intra block variance (var) vs.
     * prediction error variance (vmc)
     *
     * blocks with small prediction error are always coded non-intra
     * even if variance is smaller (is this reasonable?)
     */
    vmc1 = vmc;
    if (maxmotion > 7)
      vmc1 <<= 1;

    if (vmc1>var && vmc>=9*256)
      mbi->mb_type = MB_INTRA;
    else
    {
      var = vmc;
      if (mbi->motion_type==MC_FRAME)
      {
        /* forward */
        mbi->MV[0][0][0] = iminf - (i<<1);
        mbi->MV[0][0][1] = jminf - (j<<1);
        /* backward */
        mbi->MV[0][1][0] = iminr - (i<<1);
        mbi->MV[0][1][1] = jminr - (j<<1);
      }
      else
      {
        /* these are FRAME vectors */
        /* forward */
        mbi->MV[0][0][0] = imintf - (i<<1);
        mbi->MV[0][0][1] = (jmintf<<1) - (j<<1);
        mbi->MV[1][0][0] = iminbf - (i<<1);
        mbi->MV[1][0][1] = (jminbf<<1) - (j<<1);
        mbi->mv_field_sel[0][0] = tself;
        mbi->mv_field_sel[1][0] = bself;
        /* backward */
        mbi->MV[0][1][0] = imintr - (i<<1);
        mbi->MV[0][1][1] = (jmintr<<1) - (j<<1);
        mbi->MV[1][1][0] = iminbr - (i<<1);
        mbi->MV[1][1][1] = (jminbr<<1) - (j<<1);
        mbi->mv_field_sel[0][1] = tselr;
        mbi->mv_field_sel[1][1] = bselr;
      }
    }
  }

  mbi->var = var;
}

/*
 * motion estimation for field pictures
 *
 * oldorg: original frame for forward prediction (P and B frames)
 * neworg: original frame for backward prediction (B frames only)
 * oldref: reconstructed frame for forward prediction (P and B frames)
 * newref: reconstructed frame for backward prediction (B frames only)
 * cur:    current original frame (the one for which the prediction is formed)
 * curref: current reconstructed frame (to predict second field from first)
 * sxf,syf: forward search window (frame coordinates)
 * sxb,syb: backward search window (frame coordinates)
 * mbi:    pointer to macroblock info structure
 * secondfield: indicates second field of a frame (in P fields this means
 *              that reference field of opposite parity is in curref instead
 *              of oldref)
 * ipflag: indicates a P type field which is the second field of a frame
 *         in which the first field is I type (this restricts predictions
 *         to be based only on the opposite parity (=I) field)
 *
 * results:
 * mbi->
 *  mb_type: 0, MB_INTRA, MB_FORWARD, MB_BACKWARD, MB_FORWARD|MB_BACKWARD
 *  MV[][][]: motion vectors (field format)
 *  mv_field_sel: top/bottom field
 *  motion_type: MC_FIELD, MC_16X8
 *
 * uses global vars: pict_type, pict_struct
 */
void field_ME(
unsigned char *oldorg, unsigned char *neworg, unsigned char *oldref,
unsigned char *newref, unsigned char *cur, unsigned char *curref,
int i, int j, int sxf, int syf, int sxb, int syb,
struct mbinfo *mbi,
int secondfield, int ipflag)
{
  int w2;
  unsigned char *mb, *toporg, *topref, *botorg, *botref;
  int var,vmc,v0,dmcfieldi,dmc8i,vmc1;
  int imin,jmin,imin8u,jmin8u,imin8l,jmin8l,dmcfield,dmc8,sel,sel8u,sel8l;
  int iminf,jminf,imin8uf,jmin8uf,imin8lf,jmin8lf,dmcfieldf,dmc8f,self,sel8uf,sel8lf;
  int iminr,jminr,imin8ur,jmin8ur,imin8lr,jmin8lr,dmcfieldr,dmc8r,selr,sel8ur,sel8lr;
  int imins,jmins,ds,imindmv,jmindmv,vmc_dp,dmc_dp;

  w2 = width<<1;

  mb = cur + i + w2*j;
  if (pict_struct==BOTTOM_FIELD)
    mb += width;

  var = (*variance_sub)(mb,w2);

  if (pict_type==I_TYPE)
    mbi->mb_type = MB_INTRA;
  else if (pict_type==P_TYPE)
  {
    forwEst = 1;
    toporg = oldorg;
    topref = oldref;
    botorg = oldorg + width;
    botref = oldref + width;

    if (secondfield)
    {
      /* opposite parity field is in same frame */
      if (pict_struct==TOP_FIELD)
      {
        /* current is top field */
        botorg = cur + width;
        botref = curref + width;
      }
      else
      {
        /* current is bottom field */
        toporg = cur;
        topref = curref;
      }
    }

    field_estimate(toporg,topref,botorg,botref,mb,i,j,sxf,syf,ipflag,
                   &imin,&jmin,&imin8u,&jmin8u,&imin8l,&jmin8l,
                   &dmcfield,&dmc8,&sel,&sel8u,&sel8l,&imins,&jmins,&ds);

    if (M==1 && !ipflag)  /* generic condition which permits Dual Prime */
      dpfield_estimate(topref,botref,mb,i,j,imins,jmins,&imindmv,&jmindmv,
        &dmc_dp,&vmc_dp);

    /* select between dual prime, field and 16x8 prediction */
    if (M==1 && !ipflag && dmc_dp<dmc8 && dmc_dp<dmcfield)
    {
      /* Dual Prime prediction */
      mbi->motion_type = MC_DMV;
//      dmc = dmc_dp;     /* L1 metric */
      vmc = vmc_dp;     /* we already calculated L2 error for Dual */

    }
    else if (dmc8<dmcfield)
    {
      /* 16x8 prediction */
      mbi->motion_type = MC_16X8;
      /* upper half block */
      vmc = (*dist2sub)((sel8u?botref:topref) + (imin8u>>1) + w2*(jmin8u>>1),
                  mb,w2,imin8u&1,jmin8u&1,8);
      /* lower half block */
      vmc+= (*dist2sub)((sel8l?botref:topref) + (imin8l>>1) + w2*(jmin8l>>1),
                  mb+8*w2,w2,imin8l&1,jmin8l&1,8);
    }
    else
    {
      /* field prediction */
      mbi->motion_type = MC_FIELD;
      vmc = (*dist2sub)((sel?botref:topref) + (imin>>1) + w2*(jmin>>1),
                  mb,w2,imin&1,jmin&1,16);
    }

    /* select between intra and non-intra coding */
    vmc1 = vmc;
    if (maxmotion > 7)
      vmc1 <<= 1;

    if (vmc1>var && vmc>=9*256)
      mbi->mb_type = MB_INTRA;
    else
    {
      /* zero MV field prediction from same parity ref. field
       * (not allowed if ipflag is set)
       */
      if (!ipflag)
        v0 = (*dist2sub)(((pict_struct==BOTTOM_FIELD)?botref:topref) + i + w2*j,
                   mb,w2,0,0,16);
      if (ipflag || (4*v0>5*vmc && v0>=9*256))
      {
        var = vmc;
        mbi->mb_type = MB_FORWARD;
        if (mbi->motion_type==MC_FIELD)
        {
          mbi->MV[0][0][0] = imin - (i<<1);
          mbi->MV[0][0][1] = jmin - (j<<1);
          mbi->mv_field_sel[0][0] = sel;
        }
        else if (mbi->motion_type==MC_DMV)
        {
          /* same parity vector */
          mbi->MV[0][0][0] = imins - (i<<1);
          mbi->MV[0][0][1] = jmins - (j<<1);

          /* opposite parity vector */
          mbi->dmvector[0] = imindmv;
          mbi->dmvector[1] = jmindmv;
        }
        else
        {
          mbi->MV[0][0][0] = imin8u - (i<<1);
          mbi->MV[0][0][1] = jmin8u - (j<<1);
          mbi->MV[1][0][0] = imin8l - (i<<1);
          mbi->MV[1][0][1] = jmin8l - ((j+8)<<1);
          mbi->mv_field_sel[0][0] = sel8u;
          mbi->mv_field_sel[1][0] = sel8l;
        }
      }
      else
      {
        /* No MC */
        var = v0;
        mbi->mb_type = 0;
        mbi->motion_type = MC_FIELD;
        mbi->MV[0][0][0] = 0;
        mbi->MV[0][0][1] = 0;
        mbi->mv_field_sel[0][0] = (pict_struct==BOTTOM_FIELD);
      }
    }
  }
  else /* if (pict_type==B_TYPE) */
  {
    /* forward prediction */
    forwEst = 1;
    field_estimate(oldorg,oldref,oldorg+width,oldref+width,mb,
                   i,j,sxf,syf,0,
                   &iminf,&jminf,&imin8uf,&jmin8uf,&imin8lf,&jmin8lf,
                   &dmcfieldf,&dmc8f,&self,&sel8uf,&sel8lf,&imins,&jmins,&ds);

    /* backward prediction */
    forwEst = 0;
    field_estimate(neworg,newref,neworg+width,newref+width,mb,
                   i,j,sxb,syb,0,
                   &iminr,&jminr,&imin8ur,&jmin8ur,&imin8lr,&jmin8lr,
                   &dmcfieldr,&dmc8r,&selr,&sel8ur,&sel8lr,&imins,&jmins,&ds);

    /* calculate distances for bidirectional prediction */
    /* field */
    dmcfieldi = (*bdist1sub)(oldref + (self?width:0) + (iminf>>1) + w2*(jminf>>1),
                       newref + (selr?width:0) + (iminr>>1) + w2*(jminr>>1),
                       mb,w2,iminf&1,jminf&1,iminr&1,jminr&1,16);

    /* 16x8 upper half block */
    dmc8i = (*bdist1sub)(oldref + (sel8uf?width:0) + (imin8uf>>1) + w2*(jmin8uf>>1),
                   newref + (sel8ur?width:0) + (imin8ur>>1) + w2*(jmin8ur>>1),
                   mb,w2,imin8uf&1,jmin8uf&1,imin8ur&1,jmin8ur&1,8);

    /* 16x8 lower half block */
    dmc8i+= (*bdist1sub)(oldref + (sel8lf?width:0) + (imin8lf>>1) + w2*(jmin8lf>>1),
                   newref + (sel8lr?width:0) + (imin8lr>>1) + w2*(jmin8lr>>1),
                   mb+8*w2,w2,imin8lf&1,jmin8lf&1,imin8lr&1,jmin8lr&1,8);

    /* select prediction type of minimum distance */
    if (dmcfieldi<dmc8i && dmcfieldi<dmcfieldf && dmcfieldi<dmc8f
        && dmcfieldi<dmcfieldr && dmcfieldi<dmc8r)
    {
      /* field, interpolated */
      mbi->mb_type = MB_FORWARD|MB_BACKWARD;
      mbi->motion_type = MC_FIELD;
      vmc = (*bdist2sub)(oldref + (self?width:0) + (iminf>>1) + w2*(jminf>>1),
                   newref + (selr?width:0) + (iminr>>1) + w2*(jminr>>1),
                   mb,w2,iminf&1,jminf&1,iminr&1,jminr&1,16);
    }
    else if (dmc8i<dmcfieldf && dmc8i<dmc8f
             && dmc8i<dmcfieldr && dmc8i<dmc8r)
    {
      /* 16x8, interpolated */
      mbi->mb_type = MB_FORWARD|MB_BACKWARD;
      mbi->motion_type = MC_16X8;
      /* upper half block */
      vmc = (*bdist2sub)(oldref + (sel8uf?width:0) + (imin8uf>>1) + w2*(jmin8uf>>1),
                   newref + (sel8ur?width:0) + (imin8ur>>1) + w2*(jmin8ur>>1),
                   mb,w2,imin8uf&1,jmin8uf&1,imin8ur&1,jmin8ur&1,8);

      /* lower half block */
      vmc+= (*bdist2sub)(oldref + (sel8lf?width:0) + (imin8lf>>1) + w2*(jmin8lf>>1),
                   newref + (sel8lr?width:0) + (imin8lr>>1) + w2*(jmin8lr>>1),
                   mb+8*w2,w2,imin8lf&1,jmin8lf&1,imin8lr&1,jmin8lr&1,8);
    }
    else if (dmcfieldf<dmc8f && dmcfieldf<dmcfieldr && dmcfieldf<dmc8r)
    {
      /* field, forward */
      mbi->mb_type = MB_FORWARD;
      mbi->motion_type = MC_FIELD;
      vmc = (*dist2sub)(oldref + (self?width:0) + (iminf>>1) + w2*(jminf>>1),
                  mb,w2,iminf&1,jminf&1,16);
    }
    else if (dmc8f<dmcfieldr && dmc8f<dmc8r)
    {
      /* 16x8, forward */
      mbi->mb_type = MB_FORWARD;
      mbi->motion_type = MC_16X8;
      /* upper half block */
      vmc = (*dist2sub)(oldref + (sel8uf?width:0) + (imin8uf>>1) + w2*(jmin8uf>>1),
                  mb,w2,imin8uf&1,jmin8uf&1,8);

      /* lower half block */
      vmc+= (*dist2sub)(oldref + (sel8lf?width:0) + (imin8lf>>1) + w2*(jmin8lf>>1),
                  mb+8*w2,w2,imin8lf&1,jmin8lf&1,8);
    }
    else if (dmcfieldr<dmc8r)
    {
      /* field, backward */
      mbi->mb_type = MB_BACKWARD;
      mbi->motion_type = MC_FIELD;
      vmc = (*dist2sub)(newref + (selr?width:0) + (iminr>>1) + w2*(jminr>>1),
                  mb,w2,iminr&1,jminr&1,16);
    }
    else
    {
      /* 16x8, backward */
      mbi->mb_type = MB_BACKWARD;
      mbi->motion_type = MC_16X8;
      /* upper half block */
      vmc = (*dist2sub)(newref + (sel8ur?width:0) + (imin8ur>>1) + w2*(jmin8ur>>1),
                  mb,w2,imin8ur&1,jmin8ur&1,8);

      /* lower half block */
      vmc+= (*dist2sub)(newref + (sel8lr?width:0) + (imin8lr>>1) + w2*(jmin8lr>>1),
                  mb+8*w2,w2,imin8lr&1,jmin8lr&1,8);
    }

    /* select between intra and non-intra coding */
    vmc1 = vmc;
    if (maxmotion > 7)
      vmc1 <<= 1;

    if (vmc1>var && vmc>=9*256)
      mbi->mb_type = MB_INTRA;
    else
    {
      var = vmc;
      if (mbi->motion_type==MC_FIELD)
      {
        /* forward */
        mbi->MV[0][0][0] = iminf - (i<<1);
        mbi->MV[0][0][1] = jminf - (j<<1);
        mbi->mv_field_sel[0][0] = self;
        /* backward */
        mbi->MV[0][1][0] = iminr - (i<<1);
        mbi->MV[0][1][1] = jminr - (j<<1);
        mbi->mv_field_sel[0][1] = selr;
      }
      else /* MC_16X8 */
      {
        /* forward */
        mbi->MV[0][0][0] = imin8uf - (i<<1);
        mbi->MV[0][0][1] = jmin8uf - (j<<1);
        mbi->mv_field_sel[0][0] = sel8uf;
        mbi->MV[1][0][0] = imin8lf - (i<<1);
        mbi->MV[1][0][1] = jmin8lf - ((j+8)<<1);
        mbi->mv_field_sel[1][0] = sel8lf;
        /* backward */
        mbi->MV[0][1][0] = imin8ur - (i<<1);
        mbi->MV[0][1][1] = jmin8ur - (j<<1);
        mbi->mv_field_sel[0][1] = sel8ur;
        mbi->MV[1][1][0] = imin8lr - (i<<1);
        mbi->MV[1][1][1] = jmin8lr - ((j+8)<<1);
        mbi->mv_field_sel[1][1] = sel8lr;
      }
    }
  }

  mbi->var = var;
}

/*
 * frame picture motion estimation
 *
 * org: top left pel of source reference frame
 * ref: top left pel of reconstructed reference frame
 * mb:  macroblock to be matched
 * i,j: location of mb relative to ref (=center of search window)
 * sx,sy: half widths of search window
 * iminp,jminp,dframep: location and value of best frame prediction
 * imintp,jmintp,tselp: location of best field pred. for top field of mb
 * iminbp,jminbp,bselp: location of best field pred. for bottom field of mb
 * dfieldp: value of field prediction
 */
void frame_estimate(
unsigned char *org, unsigned char *ref, unsigned char *mb,
int i, int j, int sx, int sy,
int *iminp, int *jminp,
int *imintp, int *jmintp, int *iminbp, int *jminbp,
int *dframep, int *dfieldp,
int *tselp, int *bselp,
int imins[2][2], int jmins[2][2])
{
  int dt,db,dmint,dminb;
  int imint,iminb,jmint,jminb;

  /* frame prediction */
  *dframep = fullsearch(org,ref,mb,width,i,j,sx,sy,16,width,height,
                        iminp,jminp);

  /* predict top field from top field */
  dt = fullsearch(org,ref,mb,width<<1,i,j>>1,sx,sy>>1,8,width,height>>1,
                  &imint,&jmint);

  /* predict top field from bottom field */
  db = fullsearch(org+width,ref+width,mb,width<<1,i,j>>1,sx,sy>>1,8,width,height>>1,
                  &iminb,&jminb);

  imins[0][0] = imint;
  jmins[0][0] = jmint;
  imins[1][0] = iminb;
  jmins[1][0] = jminb;

  /* select prediction for top field */
  if (dt<=db)
  {
    dmint=dt; *imintp=imint; *jmintp=jmint; *tselp=0;
  }
  else
  {
    dmint=db; *imintp=iminb; *jmintp=jminb; *tselp=1;
  }
  /* predict bottom field from top field */
  dt = fullsearch(org,ref,mb+width,width<<1,i,j>>1,sx,sy>>1,8,width,height>>1,
                  &imint,&jmint);

  /* predict bottom field from bottom field */
  db = fullsearch(org+width,ref+width,mb+width,width<<1,i,j>>1,sx,sy>>1,8,width,height>>1,
                  &iminb,&jminb);
  imins[0][1] = imint;
  jmins[0][1] = jmint;
  imins[1][1] = iminb;
  jmins[1][1] = jminb;

  /* select prediction for bottom field */
  if (db<=dt)
  {
    dminb=db; *iminbp=iminb; *jminbp=jminb; *bselp=1;
  }
  else
  {
    dminb=dt; *iminbp=imint; *jminbp=jmint; *bselp=0;
  }

  *dfieldp=dmint+dminb;
}

/*
 * field picture motion estimation subroutine
 *
 * toporg: address of original top reference field
 * topref: address of reconstructed top reference field
 * botorg: address of original bottom reference field
 * botref: address of reconstructed bottom reference field
 * mb:  macroblock to be matched
 * i,j: location of mb (=center of search window)
 * sx,sy: half width/height of search window
 *
 * iminp,jminp,selp,dfieldp: location and distance of best field prediction
 * imin8up,jmin8up,sel8up: location of best 16x8 pred. for upper half of mb
 * imin8lp,jmin8lp,sel8lp: location of best 16x8 pred. for lower half of mb
 * d8p: distance of best 16x8 prediction
 * iminsp,jminsp,dsp: location and distance of best same parity field
 *                    prediction (needed for dual prime, only valid if
 *                    ipflag==0)
 */
void field_estimate(
unsigned char *toporg, unsigned char *topref, unsigned char *botorg,
unsigned char *botref, unsigned char *mb,
int i, int j, int sx, int sy,
int ipflag,
int *iminp, int *jminp,
int *imin8up, int *jmin8up, int *imin8lp, int *jmin8lp,
int *dfieldp, int *d8p,
int *selp, int *sel8up, int *sel8lp,
int *iminsp, int *jminsp, int *dsp)
{
  int dt, db, imint, jmint, iminb, jminb, notop, nobot;

  /* if ipflag is set, predict from field of opposite parity only */
  notop = ipflag && (pict_struct==TOP_FIELD);
  nobot = ipflag && (pict_struct==BOTTOM_FIELD);

  /* field prediction */

  /* predict current field from top field */
  if (notop)
    dt = 65536; /* infinity */
  else
    dt = fullsearch(toporg,topref,mb,width<<1,
                    i,j,sx,sy>>1,16,width,height>>1,
                    &imint,&jmint);

  /* predict current field from bottom field */
  if (nobot)
    db = 65536; /* infinity */
  else
    db = fullsearch(botorg,botref,mb,width<<1,
                    i,j,sx,sy>>1,16,width,height>>1,
                    &iminb,&jminb);

  /* same parity prediction (only valid if ipflag==0) */
  if (pict_struct==TOP_FIELD)
  {
    *iminsp = imint; *jminsp = jmint; *dsp = dt;
  }
  else
  {
    *iminsp = iminb; *jminsp = jminb; *dsp = db;
  }

  /* select field prediction */
  if (dt<=db)
  {
    *dfieldp = dt; *iminp = imint; *jminp = jmint; *selp = 0;
  }
  else
  {
    *dfieldp = db; *iminp = iminb; *jminp = jminb; *selp = 1;
  }


  /* 16x8 motion compensation */

  /* predict upper half field from top field */
  if (notop)
    dt = 65536;
  else
    dt = fullsearch(toporg,topref,mb,width<<1,
                    i,j,sx,sy>>1,8,width,height>>1,
                    &imint,&jmint);
  /* predict upper half field from bottom field */
  if (nobot)
    db = 65536;
  else
    db = fullsearch (botorg,botref,mb,width<<1,
                    i,j,sx,sy>>1,8,width,height>>1,
                    &iminb,&jminb);

  /* select prediction for upper half field */
  if (dt<=db)
  {
    *d8p = dt; *imin8up = imint; *jmin8up = jmint; *sel8up = 0;
  }
  else
  {
    *d8p = db; *imin8up = iminb; *jmin8up = jminb; *sel8up = 1;
  }

  /* predict lower half field from top field */
  if (notop)
    dt = 65536;
  else
    dt = fullsearch(toporg,topref,mb+(width<<4),width<<1,
                    i,j+8,sx,sy>>1,8,width,height>>1,
                    &imint,&jmint);

  /* predict lower half field from bottom field */
  if (nobot)
    db = 65536;
  else
    db = fullsearch(botorg,botref,mb+(width<<4),width<<1,
                    i,j+8,sx,sy>>1,8,width,height>>1,
                    &iminb,&jminb);

  /* select prediction for lower half field */
  if (dt<=db)
  {
    *d8p += dt; *imin8lp = imint; *jmin8lp = jmint; *sel8lp = 0;
  }
  else
  {
    *d8p += db; *imin8lp = iminb; *jmin8lp = jminb; *sel8lp = 1;
  }
}

void dpframe_estimate(
unsigned char *ref, unsigned char *mb,
int i, int j,
int iminf[2][2], int jminf[2][2],
int *iminp, int *jminp,
int *imindmvp, int *jmindmvp,
int *dmcp, int *vmcp)
{
  int pref,ppred,delta_x,delta_y;
  int is,js,it,jt,ib,jb,it0,jt0,ib0,jb0;
  int imins,jmins,imint,jmint,iminb,jminb,imindmv,jmindmv;
  int vmc,local_dist;

  /* Calculate Dual Prime distortions for 9 delta candidates
   * for each of the four minimum field vectors
   * Note: only for P pictures!
   */

  /* initialize minimum dual prime distortion to large value */
  vmc = 1 << 30;

  for (pref=0; pref<2; pref++)
  {
    for (ppred=0; ppred<2; ppred++)
    {
      /* convert Cartesian absolute to relative motion vector
       * values (wrt current macroblock address (i,j)
       */
      is = iminf[pref][ppred] - (i<<1);
      js = jminf[pref][ppred] - (j<<1);

      if (pref!=ppred)
      {
        /* vertical field shift adjustment */
        if (ppred==0)
          js++;
        else
          js--;

        /* mvxs and mvys scaling*/
        is<<=1;
        js<<=1;
        if (topfirst == ppred)
        {
          /* second field: scale by 1/3 */
          is = (is>=0) ? (is+1)/3 : -((-is+1)/3);
          js = (js>=0) ? (js+1)/3 : -((-js+1)/3);
        }
        else
          continue;
      }

      /* vector for prediction from field of opposite 'parity' */
      if (topfirst)
      {
        /* vector for prediction of top field from bottom field */
        it0 = ((is+(is>0))>>1);
        jt0 = ((js+(js>0))>>1) - 1;

        /* vector for prediction of bottom field from top field */
        ib0 = ((3*is+(is>0))>>1);
        jb0 = ((3*js+(js>0))>>1) + 1;
      }
      else
      {
        /* vector for prediction of top field from bottom field */
        it0 = ((3*is+(is>0))>>1);
        jt0 = ((3*js+(js>0))>>1) - 1;

        /* vector for prediction of bottom field from top field */
        ib0 = ((is+(is>0))>>1);
        jb0 = ((js+(js>0))>>1) + 1;
      }

      /* convert back to absolute half-pel field picture coordinates */
      is += i<<1;
      js += j<<1;
      it0 += i<<1;
      jt0 += j<<1;
      ib0 += i<<1;
      jb0 += j<<1;

      if (is >= 0 && is <= ((width-16)<<1) &&
          js >= 0 && js <= (height-16))
      {
        for (delta_y=-1; delta_y<=1; delta_y++)
        {
          for (delta_x=-1; delta_x<=1; delta_x++)
          {
            /* opposite field coordinates */
            it = it0 + delta_x;
            jt = jt0 + delta_y;
            ib = ib0 + delta_x;
            jb = jb0 + delta_y;

            if (it >= 0 && it <= ((width-16)<<1) &&
                jt >= 0 && jt <= (height-16) &&
                ib >= 0 && ib <= ((width-16)<<1) &&
                jb >= 0 && jb <= (height-16))
            {
              /* compute prediction error */
              local_dist = (*bdist2sub)(
                ref + (is>>1) + (width<<1)*(js>>1),
                ref + width + (it>>1) + (width<<1)*(jt>>1),
                mb,             /* current mb location */
                width<<1,       /* adjacent line distance */
                is&1, js&1, it&1, jt&1, /* half-pel flags */
                8);             /* block height */
              local_dist += (*bdist2sub)(
                ref + width + (is>>1) + (width<<1)*(js>>1),
                ref + (ib>>1) + (width<<1)*(jb>>1),
                mb + width,     /* current mb location */
                width<<1,       /* adjacent line distance */
                is&1, js&1, ib&1, jb&1, /* half-pel flags */
                8);             /* block height */

              /* update delta with least distortion vector */
              if (local_dist < vmc)
              {
                imins = is;
                jmins = js;
                imint = it;
                jmint = jt;
                iminb = ib;
                jminb = jb;
                imindmv = delta_x;
                jmindmv = delta_y;
                vmc = local_dist;
              }
            }
          }  /* end delta x loop */
        } /* end delta y loop */
      }
    }
  }
  /* Compute L1 error for decision purposes */
  local_dist = (*bdist1sub)(
    ref + (imins>>1) + (width<<1)*(jmins>>1),
    ref + width + (imint>>1) + (width<<1)*(jmint>>1),
    mb,
    width<<1,
    imins&1, jmins&1, imint&1, jmint&1,
    8);
  local_dist += (*bdist1sub)(
    ref + width + (imins>>1) + (width<<1)*(jmins>>1),
    ref + (iminb>>1) + (width<<1)*(jminb>>1),
    mb + width,
    width<<1,
    imins&1, jmins&1, iminb&1, jminb&1,
    8);

  *dmcp = local_dist;
  *iminp = imins;
  *jminp = jmins;
  *imindmvp = imindmv;
  *jmindmvp = jmindmv;
  *vmcp = vmc;
}

void dpfield_estimate(
unsigned char *topref, unsigned char *botref, unsigned char *mb,
int i, int j,
int imins, int jmins,
int *imindmvp, int *jmindmvp,
int *dmcp, int *vmcp)
{
  unsigned char *sameref, *oppref;
  int io0,jo0,io,jo,delta_x,delta_y,mvxs,mvys,mvxo0,mvyo0;
  int imino,jmino,imindmv,jmindmv,vmc_dp,local_dist;

  /* Calculate Dual Prime distortions for 9 delta candidates */
  /* Note: only for P pictures! */

  /* Assign opposite and same reference pointer */
  if (pict_struct==TOP_FIELD)
  {
    sameref = topref;
    oppref = botref;
  }
  else
  {
    sameref = botref;
    oppref = topref;
  }

  /* convert Cartesian absolute to relative motion vector
   * values (wrt current macroblock address (i,j)
   */
  mvxs = imins - (i<<1);
  mvys = jmins - (j<<1);

  /* vector for prediction from field of opposite 'parity' */
  mvxo0 = (mvxs+(mvxs>0)) >> 1;  /* mvxs // 2 */
  mvyo0 = (mvys+(mvys>0)) >> 1;  /* mvys // 2 */

  /* vertical field shift correction */
  if (pict_struct==TOP_FIELD)
    mvyo0--;
  else
    mvyo0++;

  /* convert back to absolute coordinates */
  io0 = mvxo0 + (i<<1);
  jo0 = mvyo0 + (j<<1);

  /* initialize minimum dual prime distortion to large value */
  vmc_dp = 1 << 30;

  for (delta_y = -1; delta_y <= 1; delta_y++)
  {
    for (delta_x = -1; delta_x <=1; delta_x++)
    {
      /* opposite field coordinates */
      io = io0 + delta_x;
      jo = jo0 + delta_y;

      if (io >= 0 && io <= ((width-16)<<1) &&
          jo >= 0 && jo <= ((height2-16)<<1))
      {
        /* compute prediction error */
        local_dist = (*bdist2sub)(
          sameref + (imins>>1) + width2*(jmins>>1),
          oppref  + (io>>1)    + width2*(jo>>1),
          mb,             /* current mb location */
          width2,         /* adjacent line distance */
          imins&1, jmins&1, io&1, jo&1, /* half-pel flags */
          16);            /* block height */

        /* update delta with least distortion vector */
        if (local_dist < vmc_dp)
        {
          imino = io;
          jmino = jo;
          imindmv = delta_x;
          jmindmv = delta_y;
          vmc_dp = local_dist;
        }
      }
    }  /* end delta x loop */
  } /* end delta y loop */
  /* Compute L1 error for decision purposes */
  *dmcp = (*bdist1sub)(
    sameref + (imins>>1) + width2*(jmins>>1),
    oppref  + (imino>>1) + width2*(jmino>>1),
    mb,             /* current mb location */
    width2,         /* adjacent line distance */
    imins&1, jmins&1, imino&1, jmino&1, /* half-pel flags */
    16);            /* block height */

  *imindmvp = imindmv;
  *jmindmvp = jmindmv;
  *vmcp = vmc_dp;
}

/*
 * full search block matching
 *
 * blk: top left pel of (16*h) block
 * h: height of block
 * lx: distance (in bytes) of vertically adjacent pels in ref,blk
 * org: top left pel of source reference picture
 * ref: top left pel of reconstructed reference picture
 * i0,j0: center of search window
 * sx,sy: half widths of search window
 * xmax,ymax: right/bottom limits of search area
 * iminp,jminp: pointers to where the result is stored
 *              result is given as half pel offset from ref(0,0)
 *              i.e. NOT relative to (i0,j0)
 */

static int fullsearch(
unsigned char *org, unsigned char *ref, unsigned char *blk,
int lx, int i0, int j0, int sx, int sy, int h, int xmax, int ymax,
int *iminp, int *jminp)
{
  int i,j,imin,jmin,ilow,ihigh,jlow,jhigh;
  int d,dmin;
  int k,l,sxy, zx, zy, I0, J0;

  if (maxmotion > 7)
  {
    zy = zx = maxmotion + 4;  // maxmotion is maximun search window for the MV.

    ilow = i0 - zx;
    ihigh = i0 + zx;

    if (ilow < 0)
      ilow = 0;

    if (ihigh > xmax - 16)
      ihigh = xmax - 16;

    jlow = j0 - zy;
    jhigh = j0 + zy;

    if (jlow < 0)
      jlow = 0;

    if (jhigh > ymax - h)
      jhigh = ymax - h;

    I0 = i0;
    J0 = j0;

    if (forwEst) // forward estimation (P or B))
    {
      i0 = mv_org0[mblok];
      j0 = mv_org1[mblok];
    }
    else  // ---->   B Backward             // retrieves estimated motion vectors
    {
      i0 = mv_new0[mblok];
      j0 = mv_new1[mblok];
    }

    sx = sy = 4;
  }
  else
  {
    ilow = i0 - sx;
    ihigh = i0 + sx;

    if (ilow<0)
      ilow = 0;

    if (ihigh>xmax-16)
      ihigh = xmax-16;

    jlow = j0 - sy;
    jhigh = j0 + sy;

    if (jlow<0)
      jlow = 0;

    if (jhigh>ymax-h)
      jhigh = ymax-h;
  }
  /* full pel search, spiraling outwards */

  imin = i0;
  jmin = j0;
  dmin = (*dist1sub)(org+imin+lx*jmin,blk,lx,0,0,h,65536);

  sxy = (sx>sy) ? sx : sy;

  for (l=1; l<=sxy; l++)
  {
    i = i0 - l;
    j = j0 - l;
    for (k=0; k<8*l; k++)
    {
      if (i>=ilow && i<=ihigh && j>=jlow && j<=jhigh)
      {
        d = (*dist1sub)(org+i+lx*j,blk,lx,0,0,h,dmin);
        if (d<dmin)
        {
          dmin = d;
          imin = i;
          jmin = j;
        }
      }

      if      (k<2*l) i++;
      else if (k<4*l) j++;
      else if (k<6*l) i--;
      else            j--;
    }
  }

  /* half pel */
  dmin = 65536;
  imin <<= 1;
  jmin <<= 1;
  ilow = imin - (imin>0);
  ihigh = imin + (imin<((xmax-16)<<1));
  jlow = jmin - (jmin>0);
  jhigh = jmin + (jmin<((ymax-h)<<1));

  for (j=jlow; j<=jhigh; j++)
    for (i=ilow; i<=ihigh; i++)
    {
      d = (*dist1sub)(ref+(i>>1)+lx*(j>>1),blk,lx,i&1,j&1,h,dmin);
      if (d<dmin)
      {
        dmin = d;
        imin = i;
        jmin = j;
      }
    }

  if (maxmotion > 7)
  {
    zx = abs(imin - I0 * 2) / 2;
    zy = abs(jmin - J0 * 2) / 2;

    if (forwEst) // forward estimation (P or B))
    {
      if (zx > Sxf)
        Sxf = zx;
      if (zy > Syf)
        Syf = zy;
    }
    else
    {
      if (zx > Sxb)
        Sxb = zx;
      if (zy > Syb)
        Syb = zy;
    }
  }

  *iminp = imin;
  *jminp = jmin;
  return dmin;
}

static int dist1(unsigned char *blk1, unsigned char *blk2,
		 int lx, int hx, int hy, int h, int distlim)
{
  unsigned char *p1, *p2, *p1a;
  int j, s;

  s = 0;
  p1 = blk1;
  p2 = blk2;

  if (!hx && !hy)
  {
    for (j=0; j<h; j++)
    {
      s += motion_lookup[p1[0]][p2[0]];
      s += motion_lookup[p1[1]][p2[1]];
      s += motion_lookup[p1[2]][p2[2]];
      s += motion_lookup[p1[3]][p2[3]];
      s += motion_lookup[p1[4]][p2[4]];
      s += motion_lookup[p1[5]][p2[5]];
      s += motion_lookup[p1[6]][p2[6]];
      s += motion_lookup[p1[7]][p2[7]];
      s += motion_lookup[p1[8]][p2[8]];
      s += motion_lookup[p1[9]][p2[9]];
      s += motion_lookup[p1[10]][p2[10]];
      s += motion_lookup[p1[11]][p2[11]];
      s += motion_lookup[p1[12]][p2[12]];
      s += motion_lookup[p1[13]][p2[13]];
      s += motion_lookup[p1[14]][p2[14]];
      s += motion_lookup[p1[15]][p2[15]];

      if (s >= distlim)
        break;

      p1+= lx;
      p2+= lx;
    }
  }
  else if (hx && !hy)
  {
    for (j=0; j<h; j++)
    {
      s += motion_lookup[(p1[0]+p1[1]+1)>>1][p2[0]];
      s += motion_lookup[(p1[1]+p1[2]+1)>>1][p2[1]];
      s += motion_lookup[(p1[2]+p1[3]+1)>>1][p2[2]];
      s += motion_lookup[(p1[3]+p1[4]+1)>>1][p2[3]];
      s += motion_lookup[(p1[4]+p1[5]+1)>>1][p2[4]];
      s += motion_lookup[(p1[5]+p1[6]+1)>>1][p2[5]];
      s += motion_lookup[(p1[6]+p1[7]+1)>>1][p2[6]];
      s += motion_lookup[(p1[7]+p1[8]+1)>>1][p2[7]];
      s += motion_lookup[(p1[8]+p1[9]+1)>>1][p2[8]];
      s += motion_lookup[(p1[9]+p1[10]+1)>>1][p2[9]];
      s += motion_lookup[(p1[10]+p1[11]+1)>>1][p2[10]];
      s += motion_lookup[(p1[11]+p1[12]+1)>>1][p2[11]];
      s += motion_lookup[(p1[12]+p1[13]+1)>>1][p2[12]];
      s += motion_lookup[(p1[13]+p1[14]+1)>>1][p2[13]];
      s += motion_lookup[(p1[14]+p1[15]+1)>>1][p2[14]];
      s += motion_lookup[(p1[15]+p1[16]+1)>>1][p2[15]];

      p1+= lx;
      p2+= lx;
    }
  }
  else if (!hx && hy)
  {
    p1a = p1 + lx;
    for (j=0; j<h; j++)
    {
      s += motion_lookup[(p1[0]+p1a[0]+1)>>1][p2[0]];
      s += motion_lookup[(p1[1]+p1a[1]+1)>>1][p2[1]];
      s += motion_lookup[(p1[2]+p1a[2]+1)>>1][p2[2]];
      s += motion_lookup[(p1[3]+p1a[3]+1)>>1][p2[3]];
      s += motion_lookup[(p1[4]+p1a[4]+1)>>1][p2[4]];
      s += motion_lookup[(p1[5]+p1a[5]+1)>>1][p2[5]];
      s += motion_lookup[(p1[6]+p1a[6]+1)>>1][p2[6]];
      s += motion_lookup[(p1[7]+p1a[7]+1)>>1][p2[7]];
      s += motion_lookup[(p1[8]+p1a[8]+1)>>1][p2[8]];
      s += motion_lookup[(p1[9]+p1a[9]+1)>>1][p2[9]];
      s += motion_lookup[(p1[10]+p1a[10]+1)>>1][p2[10]];
      s += motion_lookup[(p1[11]+p1a[11]+1)>>1][p2[11]];
      s += motion_lookup[(p1[12]+p1a[12]+1)>>1][p2[12]];
      s += motion_lookup[(p1[13]+p1a[13]+1)>>1][p2[13]];
      s += motion_lookup[(p1[14]+p1a[14]+1)>>1][p2[14]];
      s += motion_lookup[(p1[15]+p1a[15]+1)>>1][p2[15]];

      p1 = p1a;
      p1a+= lx;
      p2+= lx;
    }
  }
  else // if (hx && hy)
  {
    p1a = p1 + lx;
    for (j=0; j<h; j++)
    {
      s += motion_lookup[(p1[0]+p1[1]+p1a[0]+p1a[1]+2)>>2][p2[0]];
      s += motion_lookup[(p1[1]+p1[2]+p1a[1]+p1a[2]+2)>>2][p2[1]];
      s += motion_lookup[(p1[2]+p1[3]+p1a[2]+p1a[3]+2)>>2][p2[2]];
      s += motion_lookup[(p1[3]+p1[4]+p1a[3]+p1a[4]+2)>>2][p2[3]];
      s += motion_lookup[(p1[4]+p1[5]+p1a[4]+p1a[5]+2)>>2][p2[4]];
      s += motion_lookup[(p1[5]+p1[6]+p1a[5]+p1a[6]+2)>>2][p2[5]];
      s += motion_lookup[(p1[6]+p1[7]+p1a[6]+p1a[7]+2)>>2][p2[6]];
      s += motion_lookup[(p1[7]+p1[8]+p1a[7]+p1a[8]+2)>>2][p2[7]];
      s += motion_lookup[(p1[8]+p1[9]+p1a[8]+p1a[9]+2)>>2][p2[8]];
      s += motion_lookup[(p1[9]+p1[10]+p1a[9]+p1a[10]+2)>>2][p2[9]];
      s += motion_lookup[(p1[10]+p1[11]+p1a[10]+p1a[11]+2)>>2][p2[10]];
      s += motion_lookup[(p1[11]+p1[12]+p1a[11]+p1a[12]+2)>>2][p2[11]];
      s += motion_lookup[(p1[12]+p1[13]+p1a[12]+p1a[13]+2)>>2][p2[12]];
      s += motion_lookup[(p1[13]+p1[14]+p1a[13]+p1a[14]+2)>>2][p2[13]];
      s += motion_lookup[(p1[14]+p1[15]+p1a[14]+p1a[15]+2)>>2][p2[14]];
      s += motion_lookup[(p1[15]+p1[16]+p1a[15]+p1a[16]+2)>>2][p2[15]];

      p1 = p1a;
      p1a+= lx;
      p2+= lx;
    }
  }

  return s;
}

/*
 * total squared difference between two (16*h) blocks
 * including optional half pel interpolation of blk1 (hx,hy)
 * blk1,blk2: addresses of top left pels of both blocks
 * lx:        distance (in bytes) of vertically adjacent pels
 * hx,hy:     flags for horizontal and/or vertical interpolation
 * h:         height of block (usually 8 or 16)
 */

static int dist2(unsigned char *blk1, unsigned char *blk2,
		 int lx, int hx, int hy, int h)
{
  unsigned char *p1,*p1a,*p2;
  int i,j;
  int s,v;

  s = 0;
  p1 = blk1;
  p2 = blk2;
  if (!hx && !hy)
    for (j=0; j<h; j++)
    {
      for (i=0; i<16; i++)
      {
        v = p1[i] - p2[i];
        s+= v*v;
      }
      p1+= lx;
      p2+= lx;
    }
  else if (hx && !hy)
    for (j=0; j<h; j++)
    {
      for (i=0; i<16; i++)
      {
        v = ((unsigned int)(p1[i]+p1[i+1]+1)>>1) - p2[i];
        s+= v*v;
      }
      p1+= lx;
      p2+= lx;
    }
  else if (!hx && hy)
  {
    p1a = p1 + lx;
    for (j=0; j<h; j++)
    {
      for (i=0; i<16; i++)
      {
        v = ((unsigned int)(p1[i]+p1a[i]+1)>>1) - p2[i];
        s+= v*v;
      }
      p1 = p1a;
      p1a+= lx;
      p2+= lx;
    }
  }
  else // if (hx && hy)
  {
    p1a = p1 + lx;
    for (j=0; j<h; j++)
    {
      for (i=0; i<16; i++)
      {
        v = ((unsigned int)(p1[i]+p1[i+1]+p1a[i]+p1a[i+1]+2)>>2) - p2[i];
        s+= v*v;
      }
      p1 = p1a;
      p1a+= lx;
      p2+= lx;
    }
  }

  return s;
}



/*
 * absolute difference error between a (16*h) block and a bidirectional
 * prediction
 *
 * p2: address of top left pel of block
 * pf,hxf,hyf: address and half pel flags of forward ref. block
 * pb,hxb,hyb: address and half pel flags of backward ref. block
 * h: height of block
 * lx: distance (in bytes) of vertically adjacent pels in p2,pf,pb
 */

static int bdist1(unsigned char *pf, unsigned char *pb, unsigned char *p2,
		  int lx, int hxf, int hyf, int hxb, int hyb, int h)
{
  unsigned char *pfa,*pfb,*pfc,*pba,*pbb,*pbc;
  int i,j;
  int s;

  pfa = pf + hxf;
  pfb = pf + lx*hyf;
  pfc = pfb + hxf;

  pba = pb + hxb;
  pbb = pb + lx*hyb;
  pbc = pbb + hxb;

  s = 0;

  for (j=0; j<h; j++)
  {
    for (i=0; i<16; i++)
    {
        s += motion_lookup[(((pf[i] + pfa[i] + pfb[i] + pfc[i] + 2)>>2) +
                           (((pb[i] + pba[i] + pbb[i] + pbc[i] + 2)>>2)) + 1)>>1][p2[i]];
    }

    p2+= lx;
    pf+= lx;
    pfa+= lx;
    pfb+= lx;
    pfc+= lx;
    pb+= lx;
    pba+= lx;
    pbb+= lx;
    pbc+= lx;
  }

  return s;
}


/*
 * squared error between a (16*h) block and a bidirectional
 * prediction
 *
 * p2: address of top left pel of block
 * pf,hxf,hyf: address and half pel flags of forward ref. block
 * pb,hxb,hyb: address and half pel flags of backward ref. block
 * h: height of block
 * lx: distance (in bytes) of vertically adjacent pels in p2,pf,pb
 */

static int bdist2(unsigned char *pf, unsigned char *pb, unsigned char *p2,
		  int lx, int hxf, int hyf, int hxb, int hyb, int h)
{
  unsigned char *pfa,*pfb,*pfc,*pba,*pbb,*pbc;
  int i,j;
  int s,v;

  pfa = pf + hxf;
  pfb = pf + lx*hyf;
  pfc = pfb + hxf;

  pba = pb + hxb;
  pbb = pb + lx*hyb;
  pbc = pbb + hxb;

  s = 0;

  for (j=0; j<h; j++)
  {
    for (i=0; i<16; i++)
    {
      v = ((((unsigned int)(*pf++ + *pfa++ + *pfb++ + *pfc++ + 2)>>2) +
            ((unsigned int)(*pb++ + *pba++ + *pbb++ + *pbc++ + 2)>>2) + 1)>>1)
          - *p2++;
      s+=v*v;
    }
    p2+= lx-16;
    pf+= lx-16;
    pfa+= lx-16;
    pfb+= lx-16;
    pfc+= lx-16;
    pb+= lx-16;
    pba+= lx-16;
    pbb+= lx-16;
    pbc+= lx-16;
  }

  return s;
}

static int edist1(unsigned char *bo, unsigned char *br, int lx, int distlim)
{
  int k;
  int s = 0;

  for (k = 0; k < 4; k++)
  {
    s += motion_lookup[bo[0]][br[0]];
    s += motion_lookup[bo[1]][br[1]];
    s += motion_lookup[bo[2]][br[2]];
    s += motion_lookup[bo[3]][br[3]];
    if (s >= distlim)
      break;
    bo += lx;
    br += lx;
  }
  return s;
}


/*
 * variance of a (16*16) block, multiplied by 256
 * p:  address of top left pel of block
 * lx: distance (in bytes) of vertically adjacent pels
 */

static int variance(unsigned char *p, int lx)
{
  int i,j;
  unsigned int v,s,s2;

  s = s2 = 0;

  for (j=0; j<16; j++)
  {
    for (i=0; i<16; i++)
    {
      v = *p++;
      s+= v;
      s2+= v*v;
    }
    p+= lx-16;
  }
  return s2 - (s*s)/256;
}


