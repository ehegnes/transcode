/* putbits.c, bit-level output                                              */

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

/* initialize buffer, call once before first putbits or alignbits */
int init_putbits(struct bitstream *bs, char *bs_filename)
{
  char tmpStr[MAXPATH];

  if ((bs->bitfile = fopen(bs_filename, "wb")) == NULL)
  {
    sprintf(tmpStr, "Unable to open file %s for writing.", bs_filename);
    DisplayError(tmpStr);
    return FALSE;
  }
  bs->bfr = (unsigned char*)malloc(BUFFER_SIZE);
  if (!bs->bfr)
  {
    fclose(bs->bitfile);
    sprintf(tmpStr, "Unable to allocate memory for bitstream file %s.", bs_filename);
    DisplayError(tmpStr);
    return FALSE;
  }
  bs->bitidx = 8;
  bs->byteidx = 0;
  bs->totbits = 0.0;
  bs->outbyte = 0;
  bs->fileOutError = FALSE;
  return TRUE;
}

void finish_putbits(struct bitstream *bs)
{
  if (bs->bitfile)
  {
    if ((bs->byteidx) && (!bs->fileOutError))
      fwrite(bs->bfr, sizeof(unsigned char), bs->byteidx, bs->bitfile);
    fclose(bs->bitfile);
    bs->bitfile = NULL;
  }
  if (bs->bfr)
  {
    free(bs->bfr);
    bs->bfr = NULL;
  }
}

static void putbyte(struct bitstream *bs)
{
  if (!bs->fileOutError)
  {
    bs->bfr[bs->byteidx++] = bs->outbyte;
    if (bs->byteidx == BUFFER_SIZE)
    {
      if (fwrite(bs->bfr, sizeof(unsigned char), BUFFER_SIZE, bs->bitfile) != BUFFER_SIZE)
        bs->fileOutError = TRUE;
      bs->byteidx = 0;
    }
  }
  bs->bitidx = 8;
}

/* write rightmost n (0<=n<=32) bits of val to outfile */
void putbits(struct bitstream *bs, int val, int n)
{
  int i;
  unsigned int mask;

  mask = 1 << (n - 1); /* selects first (leftmost) bit */
  for (i = 0; i < n; i++)
  {
    bs->totbits += 1;
    bs->outbyte <<= 1;
    if (val & mask)
      bs->outbyte |= 1;
    mask >>= 1; /* select next bit */
    bs->bitidx--;
    if (bs->bitidx == 0) /* 8 bit buffer full */
      putbyte(bs);
  }
}

/* write rightmost bit of val to outfile */
void put1bit(struct bitstream *bs, int val)
{
  bs->totbits += 1;
  bs->outbyte <<= 1;
  if (val & 0x1)
    bs->outbyte |= 1;
  bs->bitidx--;
  if (bs->bitidx == 0) /* 8 bit buffer full */
    putbyte(bs);
}

/* Prepare a upcoming undo at the beginning of a GOP */
void prepareundo(struct bitstream *bs, struct bitstream *undo)
{
  memcpy(ubuffer, bs->bfr, BUFFER_SIZE);
  fgetpos(bs->bitfile, &bs->actpos);
  *undo = *bs;
}

/* Reset old status, undo all changes made up to a point */
void undochanges(struct bitstream *bs, struct bitstream *old)
{
  memcpy(bs->bfr, ubuffer, BUFFER_SIZE);
  fsetpos(bs->bitfile, &bs->actpos);
  *bs = *old;
}

/* zero bit stuffing to next byte boundary (5.2.3, 6.2.1) */
void alignbits(struct bitstream *bs)
{
  if (bs->bitidx != 8)
    putbits(bs, 0, bs->bitidx);
}

/* return total number of generated bits */
double bitcount(struct bitstream *bs)
{
  return bs->totbits;
}

static int refill_buffer(struct bitstream *bs)
{
  int i;

  i = fread(bs->bfr, sizeof(unsigned char), BUFFER_SIZE, bs->bitfile);
  if (!i)
  {
    bs->eobs = TRUE;
    return FALSE;
  }
  bs->bufcount = i;
  return TRUE;
}

/* open the device to read the bit stream from it */
int init_getbits(struct bitstream *bs, char *bs_filename)
{
  char tmpStr[MAXPATH];

  if ((bs->bitfile = fopen(bs_filename, "rb")) == NULL)
  {
    sprintf(tmpStr, "Unable to open file %s for reading.", bs_filename);
    DisplayError(tmpStr);
    return FALSE;
  }
  bs->bfr = (unsigned char*)malloc(BUFFER_SIZE);
  if (!bs->bfr)
  {
    fclose(bs->bitfile);
    sprintf(tmpStr, "Unable to allocate memory for bitstream file %s.", bs_filename);
    DisplayError(tmpStr);
    return FALSE;
  }
  bs->byteidx = 0;
  bs->bitidx = 8;
  bs->totbits = 0.0;
  bs->bufcount = 0;
  bs->eobs = FALSE;
  if (!refill_buffer(bs))
  {
    if (bs->eobs)
    {
      sprintf(tmpStr, "Unable to read from file %s.", bs_filename);
      DisplayError(tmpStr);
      return FALSE;
    }
  }
  return TRUE;
}

/*close the device containing the bit stream after a read process*/
void finish_getbits(struct bitstream *bs)
{
  if (bs->bitfile)
    fclose(bs->bitfile);
  bs->bitfile = NULL;
}

int masks[8]={0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80};

/*read 1 bit from the bit stream */
int get1bit(struct bitstream *bs, unsigned int *retval)
{
  unsigned int bit;

  if (bs->eobs)
    return FALSE;

  bit = (bs->bfr[bs->byteidx] & masks[bs->bitidx - 1]) >> (bs->bitidx - 1);
  bs->totbits++;
  bs->bitidx--;
  if (!bs->bitidx)
  {
    bs->bitidx = 8;
    bs->byteidx++;
    if (bs->byteidx == bs->bufcount)
    {
      if (bs->bufcount == BUFFER_SIZE)
        refill_buffer(bs);
      else
        bs->eobs = TRUE;
      bs->byteidx = 0;
    }
  }
  *retval = bit;
  return TRUE;
}

/*read N bits from the bit stream */
int getbits(struct bitstream *bs, unsigned int *retval, int N)
{
  unsigned int val = 0;
  int i = N;
  unsigned int j;

  // Optimize: we are on byte boundary and want to read multiple of bytes!
  if ((bs->bitidx == 8) && ((N & 7) == 0))
  {
    i = N >> 3;
    while (i > 0)
    {
      if (bs->eobs)
        return FALSE;
      val = (val << 8) | bs->bfr[bs->byteidx];
      bs->byteidx++;
      bs->totbits += 8;
      if (bs->byteidx == bs->bufcount)
      {
        if (bs->bufcount == BUFFER_SIZE)
          refill_buffer(bs);
        else
          bs->eobs = TRUE;
        bs->byteidx = 0;
      }
      i--;
    }
  }
  else
  {
    while (i > 0)
    {
      if (bs->eobs)
        return FALSE;

      j = (bs->bfr[bs->byteidx] & masks[bs->bitidx - 1]) >> (bs->bitidx - 1);
      bs->totbits++;
      bs->bitidx--;
      if (!bs->bitidx)
      {
        bs->bitidx = 8;
        bs->byteidx++;
        if (bs->byteidx == bs->bufcount)
        {
          if (bs->bufcount == BUFFER_SIZE)
            refill_buffer(bs);
          else
            bs->eobs = TRUE;
          bs->byteidx = 0;
        }
      }
      val = (val << 1) | j;
      i--;
    }
  }
  *retval = val;
  return TRUE;
}

/*return the status of the bit stream*/
/* returns 1 if end of bit stream was reached */
/* returns 0 if end of bit stream was not reached */

int end_bs(struct bitstream *bs)
{
  return bs->eobs;
}

/*this function seeks for a byte aligned sync word (max 32 bits) in the bit stream and
  places the bit stream pointer right after the sync.
  This function returns 1 if the sync was found otherwise it returns 0  */

int seek_sync(struct bitstream *bs, unsigned int sync, int N)
{
  unsigned int val, val1;
  unsigned int maxi = (int)pow(2.0, (double)N) - 1;

  while (bs->bitidx != 8)
    if (!get1bit(bs, &val))
      return FALSE;
  if (!getbits(bs, &val, N))
    return FALSE;

  while ((val & maxi) != sync)
  {
    val <<= 8;
    if (!getbits(bs, &val1, 8))
      return FALSE;
    val |= val1;
  }
  return TRUE;
}
