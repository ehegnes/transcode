/* putvlc.c, generation of variable length codes                            */

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
#include "consts1.h"

/* private prototypes */

static int putDC(sVLCtable *tab, int val);

/* generate variable length code for luminance DC coefficient */
int putDClum(int val)
{
  return putDC(DClumtab,val);
}

/* generate variable length code for chrominance DC coefficient */
int putDCchrom(int val)
{
  return putDC(DCchromtab,val);
}

/* generate variable length code for DC coefficient (7.2.1) */
int putDC(sVLCtable *tab, int val)
{
  int absval, size;

  absval = (val<0) ? -val : val; /* abs(val) */

  if (absval>2047 || ((video_type < MPEG_MPEG2) && absval>255))
  {
    /* should never happen */
    sprintf(errortext,"DC value out of range (%d)\n",val);
    DisplayError(errortext);
    return FALSE;
  }

  /* compute dct_dc_size */
  size = 0;

  while (absval)
  {
    absval >>= 1;
    size++;
  }

  /* generate VLC for dct_dc_size (Table B-12 or B-13) */
  putbits(&videobs, tab[size].code,tab[size].len);

  /* append fixed length code (dc_dct_differential) */
  if (size!=0)
  {
    if (val>=0)
      absval = val;
    else
      absval = val + (1<<size) - 1; /* val + (2 ^ size) - 1 */
    putbits(&videobs, absval,size);
  }
  return TRUE;
}

/* generate variable length code for first coefficient
 * of a non-intra block (7.2.2.2) */
int putACfirst(int run, int val)
{
  if (run==0 && (val==1 || val==-1)) /* these are treated differently */
  {
    /* generate '1s' (s=sign), (Table B-14, line 2) */
    if (val < 0)
      putbits(&videobs, 3, 2);
    else
      putbits(&videobs, 2, 2);
    return TRUE;
  }
  else
    return putAC(run,val,0); /* no difference for all others */
}

/* generate variable length code for other DCT coefficients (7.2.2) */
int putAC(
int run, int signed_level, int vlcformat)
{
  int level, len;
  VLCtable *ptab;

  level = (signed_level<0) ? -signed_level : signed_level; /* abs(signed_level) */

  /* make sure run and level are valid */
  if (run<0 || run>63 || level==0 || level>2047 || ((video_type < MPEG_MPEG2) && level>255))
  {
    sprintf(errortext,"AC value out of range (run=%d, signed_level=%d)\n",
      run,signed_level);
    DisplayError(errortext);
    return FALSE;
  }

  len = 0;

  if (run<2 && level<41)
  {
    /* vlcformat selects either of Table B-14 / B-15 */
    if (vlcformat)
      ptab = &dct_code_tab1a[run][level-1];
    else
      ptab = &dct_code_tab1[run][level-1];

    len = ptab->len;
  }
  else if (run<32 && level<6)
  {
    /* vlcformat selects either of Table B-14 / B-15 */
    if (vlcformat)
      ptab = &dct_code_tab2a[run-2][level-1];
    else
      ptab = &dct_code_tab2[run-2][level-1];

    len = ptab->len;
  }

  if (len!=0) /* a VLC code exists */
  {
    putbits(&videobs, ptab->code,len);
    putbits(&videobs, signed_level<0,1); /* sign */
  }
  else
  {
    /* no VLC for this (run, level) combination: use escape coding (7.2.2.3) */
    putbits(&videobs, 1l,6); /* Escape */
    putbits(&videobs, run,6); /* 6 bit code for run */
    if (video_type < MPEG_MPEG2)
    {
      /* ISO/IEC 11172-2 uses a 8 or 16 bit code */
      if (signed_level>127)
        putbits(&videobs, 0,8);
      if (signed_level<-127)
        putbits(&videobs, 128,8);
      putbits(&videobs, signed_level,8);
    }
    else
    {
      /* ISO/IEC 13818-2 uses a 12 bit code, Table B-16 */
      putbits(&videobs, signed_level,12);
    }
  }
  return TRUE;
}

/* generate variable length code for macroblock_address_increment (6.3.16) */
void putaddrinc(int addrinc)
{
  while (addrinc>33)
  {
    putbits(&videobs, 0x08,11); /* macroblock_escape */
    addrinc-= 33;
  }

  putbits(&videobs, addrinctab[addrinc-1].code,addrinctab[addrinc-1].len);
}

/* generate variable length code for macroblock_type (6.3.16.1) */
void putmbtype(int pict_type, int mb_type)
{
  putbits(&videobs, mbtypetab[pict_type-1][mb_type].code,
          mbtypetab[pict_type-1][mb_type].len);
}

/* generate variable length code for motion_code (6.3.16.3) */
void putmotioncode(int motion_code)
{
  int abscode;

  abscode = (motion_code>=0) ? motion_code : -motion_code; /* abs(motion_code) */
  putbits(&videobs, motionvectab[abscode].code,motionvectab[abscode].len);
  if (motion_code!=0)
    putbits(&videobs, motion_code<0,1); /* sign, 0=positive, 1=negative */
}

/* generate variable length code for dmvector[t] (6.3.16.3), Table B-11 */
void putdmv(int dmv)
{
  if (dmv==0)
    putbits(&videobs, 0,1);
  else if (dmv>0)
    putbits(&videobs, 2,2);
  else
    putbits(&videobs, 3,2);
}

/* generate variable length code for coded_block_pattern (6.3.16.4)
 *
 * 4:2:2, 4:4:4 not implemented
 */
void putcbp(int cbp)
{
  putbits(&videobs, cbptable[cbp].code,cbptable[cbp].len);
}
