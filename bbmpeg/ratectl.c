/* ratectl.c, bitrate control routines (linear quantization only currently) */

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
#include "consts1.h"

/* private prototypes */

static double var_sblk(unsigned char *p, int lx);

/* rate control variables */
static double R, T, d;
static double actsum;
static int Np, Nb;
static double S, Q;
static int prev_mquant;
static double bitcnt_EOP;
static double next_ip_delay; /* due to frame reordering delay */
static double decoding_time;
static int Xi;
static int Xp;
static int Xb;
static int r;
static int d0i;
static int d0p;
static int d0b;
static double avg_act;
static double minPercent, padPercent;
static double prev_bitcount, prev_frame_bitcount;
static unsigned int frame_count, temp_frame_rate, mquant_count, total_mquant;
static unsigned int frame_index_underflow, frame_index_overflow, frame_index;
static double rc_frame_bitcount, rc_bitcount;
static unsigned int rc_max_frame, rc_min_frame, rc_max, rc_min, rc_frame_count;
static unsigned int rc_max_mquant, rc_min_mquant, rc_total_mquant, rc_mquant_count;

static unsigned int ratetab[8] = {24, 24, 25, 30, 30, 50, 60, 60};

static double min_picture_bits;
static double max_picture_bits;
static double vbv_max_delay;
static double Tmin;

void rc_init_seq()
{
  min_picture_bits = 0.0;
  max_picture_bits = 999999999.0;
  vbv_max_delay = (vbv_buffer_size) * 16384.0 * 90000.0 / bit_rate;
  Tmin = floor(bit_rate/(3.0*frame_rate) + 0.5);


  frame_index_underflow = (unsigned) -1;
  frame_index_overflow = (unsigned) -1;
  frame_index = 0;
  if (constant_bitrate)
  {
    bitcnt_EOP = 0.0;
    next_ip_delay = 0.0;
    decoding_time = 0.0;
    minPercent = (double) min_frame_percent / 100.0;
    padPercent = (double) pad_frame_percent / 100.0;

    /* reaction parameter (constant) */
    r = init_r;
    if (r==0)  r = (int)floor(2.0*bit_rate/frame_rate + 0.5);

    /* average activity */
    avg_act = init_avg_act;
    if (avg_act==0.0)  avg_act = 400.0;

    /* remaining # of bits in GOP */
    R = 0.0;

    /* global complexity measure */
    Xi = init_Xi;
    Xp = init_Xp;
    Xb = init_Xb;
    if (Xi==0) Xi = (int)floor(160.0*bit_rate/115.0 + 0.5);
    if (Xp==0) Xp = (int)floor( 60.0*bit_rate/115.0 + 0.5);
    if (Xb==0) Xb = (int)floor( 42.0*bit_rate/115.0 + 0.5);

    /* virtual buffer fullness */
    d0i = init_d0i;
    d0p = init_d0p;
    d0b = init_d0b;
    if (d0i==0) d0i = (int)floor(10.0*r/31.0 + 0.5);
    if (d0p==0) d0p = (int)floor(10.0*r/31.0 + 0.5);
    if (d0b==0) d0b = (int)floor(1.4*10.0*r/31.0 + 0.5);
  /*
    if (d0i==0) d0i = (int)floor(10.0*r/(qscale_tab[0] ? 56.0 : 31.0) + 0.5);
    if (d0p==0) d0p = (int)floor(10.0*r/(qscale_tab[1] ? 56.0 : 31.0) + 0.5);
    if (d0b==0) d0b = (int)floor(1.4*10.0*r/(qscale_tab[2] ? 56.0 : 31.0) + 0.5);
  */

    if (OutputStats)
    {
      fprintf(statfile,"\nrate control: sequence initialization\n");
      fprintf(statfile,
        " initial global complexity measures (I,P,B): Xi=%d, Xp=%d, Xb=%d\n",
        Xi, Xp, Xb);
      fprintf(statfile," reaction parameter: r=%d\n", r);
      fprintf(statfile,
        " initial virtual buffer fullness (I,P,B): d0i=%d, d0p=%d, d0b=%d\n",
        d0i, d0p, d0b);
      fprintf(statfile," initial average activity: avg_act=%.1f\n", avg_act);
    }
  }
  max_bitrate = 0;
  min_bitrate = 200000000;
  prev_bitcount = 0.0;
  max_frame_bitrate = 0;
  min_frame_bitrate = 200000000;
  prev_frame_bitcount = 0.0;
  frame_count = 0;
  avg_mquant = 0.0;
  min_mquant = 200;
  max_mquant = 0;
  mquant_count = 0;
  total_mquant = 0;
  temp_frame_rate = ratetab[frame_rate_code];
}

void save_rc_max()
{
  rc_frame_bitcount = prev_frame_bitcount;
  rc_bitcount = prev_bitcount;
  rc_max_frame = max_frame_bitrate;
  rc_min_frame = min_frame_bitrate;
  rc_max = max_bitrate;
  rc_min = min_bitrate;
  rc_frame_count = frame_count;
  rc_max_mquant = max_mquant;
  rc_min_mquant = min_mquant;
  rc_total_mquant = total_mquant;
  rc_mquant_count = mquant_count;
}

void restore_rc_max()
{
  prev_frame_bitcount = rc_frame_bitcount;
  prev_bitcount = rc_bitcount;
  max_frame_bitrate = rc_max_frame;
  min_frame_bitrate = rc_min_frame;
  max_bitrate = rc_max;
  min_bitrate = rc_min;
  frame_count = rc_frame_count;
  max_mquant = rc_max_mquant;
  min_mquant = rc_min_mquant;
  total_mquant = rc_total_mquant;
  mquant_count = rc_mquant_count;
}

void rc_update_max()
{
  unsigned int bits_per_sec;
  double bit_count;

  /* only update after a full frame */
  if (pict_struct != FRAME_PICTURE)
  {
    if (topfirst && (pict_struct == TOP_FIELD))
      return;
    if (!topfirst && (pict_struct == BOTTOM_FIELD))
      return;
  }
  bit_count = bitcount(&videobs);
  bits_per_sec = (unsigned int)floor(bit_count - prev_frame_bitcount);
  prev_frame_bitcount = bit_count;
  if (bits_per_sec > max_frame_bitrate)
    max_frame_bitrate = bits_per_sec;
  if (bits_per_sec < min_frame_bitrate)
    min_frame_bitrate = bits_per_sec;
  frame_count++;
  if (frame_count >= temp_frame_rate)
  {
    bits_per_sec = (unsigned int)floor(bit_count - prev_bitcount);
    prev_bitcount = bit_count;
    if (bits_per_sec > max_bitrate)
      max_bitrate = bits_per_sec;
    if (bits_per_sec < min_bitrate)
      min_bitrate = bits_per_sec;
    frame_count = 0;
  }
}

void rc_init_GOP(int np, int nb)
{

  Np = fieldpic ? 2*np+1 : np;
  Nb = fieldpic ? 2*nb : nb;

  /* ensure that the rate control remains stable even if we can't reach the target bitrate */ 
  /* if target rate is exceeded however, the rate control parameters are carried forward   */

  /* if the target bit rate isn't reached, R is > 0, indicating we have bits left          */
  /* in addition (independently), some or all of the d0x parameters are negative           */
  /* net result is less bitrate variation                                                  */
  
  if (R > 0.0)
      R = 0.0;
  R  += floor((1 + np + nb) * bit_rate / frame_rate + 0.5);

  if (d0p < init_d0i)
      d0p = init_d0i;

  if (reset_d0pb)
  {
      d0p = init_d0p;
      d0b = init_d0b;
  }
  else
  {
      if (d0p < init_d0p)
          d0p = init_d0p;

      if (d0b < init_d0b)
          d0b = init_d0b;
  }

  if (OutputStats)
  {
    fprintf(statfile,"\nrate control: new group of pictures (GOP)\n");
    fprintf(statfile," target number of bits for GOP: R=%.1f\n",R);
    fprintf(statfile," number of P pictures in GOP: Np=%d\n",Np);
    fprintf(statfile," number of B pictures in GOP: Nb=%d\n",Nb);
  }
}

/* Note: we need to substitute K for the 1.4 and 1.0 constants -- this can
   be modified to fit image content */

/* Step 1: compute target bits for current picture being coded */
void rc_init_pict(unsigned char *frame)
{

  switch (pict_type)
  {
  case I_TYPE:
    T = floor(R/(1.0+Np*Xp/(Xi*1.0)+Nb*Xb/(Xi*1.4)) + 0.5);
    d = d0i;
    break;
  case P_TYPE:
    T = floor(R/(Np+Nb*1.0*Xb/(1.4*Xp)) + 0.5);
    d = d0p;
    break;
  case B_TYPE:
    T = floor(R/(Nb+Np*1.4*Xp/(1.0*Xb)) + 0.5);
    d = d0b;
    break;
  }


  // apply any constraints from the vbv
  if (T > max_picture_bits)
      T = max_picture_bits;

  if (T < min_picture_bits)
      T = min_picture_bits;


  Tmin = floor(bit_rate/(3.0*frame_rate) + 0.5);

  if ((R <= 0) || (T / R > 0.90 && T < Tmin))
    T = Tmin;

  S = bitcount(&videobs);
  Q = 0.0;

  calc_actj(frame);
  actsum = 0.0;
  if (OutputStats)
  {
    fprintf(statfile,"\nrate control: start of picture\n");
	fprintf(statfile," min picture bits: %.1f\n",min_picture_bits);
    fprintf(statfile," target number of bits: T=%.1f\n",T);
	fprintf(statfile," max picture bits: %.1f\n",max_picture_bits);
  }
}


void calc_actj(unsigned char *frame)
{
  int i,j,k;
  unsigned char *p;
  double actj,var;

  k = 0;

  for (j=0; j<height2; j+=16)
    for (i=0; i<width; i+=16)
    {
      p = frame + ((pict_struct==BOTTOM_FIELD)?width:0) + i + width2*j;

      /* take minimum spatial activity measure of luminance blocks */

      actj = var_sblk(p,width2);
      var = var_sblk(p+8,width2);
      if (var<actj) actj = var;
      var = var_sblk(p+8*width2,width2);
      if (var<actj) actj = var;
      var = var_sblk(p+8*width2+8,width2);
      if (var<actj) actj = var;

      if (!fieldpic && !prog_seq)
      {
        /* field */
        var = var_sblk(p,width<<1);
        if (var<actj) actj = var;
        var = var_sblk(p+8,width<<1);
        if (var<actj) actj = var;
        var = var_sblk(p+width,width<<1);
        if (var<actj) actj = var;
        var = var_sblk(p+width+8,width<<1);
        if (var<actj) actj = var;
      }

      actj+= 1.0;
      mbinfo[k++].act = actj;
    }
}

void rc_update_pict()
{
  double X, Sinitial;
  unsigned int i;

  Sinitial = bitcount(&videobs) - S; /* total # of bits in picture */

  if (Sinitial < min_picture_bits)
  {
      unsigned int limit;
      limit = (unsigned int) (min_picture_bits - Sinitial);
      for (i = 0; i < limit; i += 32)
           putbits(&videobs, 0, 32);

      alignbits(&videobs);
  }

  S = bitcount(&videobs) - S;

  /* debug */
  //if (S > max_picture_bits)
  //{
  //    char tmpStr[256];
  //    sprintf(tmpStr,"max picture bits = %.0f, actual = %.0f", max_picture_bits, S);
  //    DisplayWarning (tmpStr);
 // }
  R -= S; /* remaining # of bits in GOP */
  X = floor(S*((0.5*Q)/(mb_width*mb_height2)) + 0.5);
  d += S - T;

  avg_act = actsum/(mb_width*mb_height2);

  switch (pict_type)
  {
  case I_TYPE:
    Xi = (int)X;
    d0i = (int)d;
    break;
  case P_TYPE:
    Xp = (int)X;
    d0p = (int)d;
    Np--;
    break;
  case B_TYPE:
    Xb = (int)X;
    d0b = (int)d;
    Nb--;
    break;
  }

  paddingSum += (S-Sinitial);
  if (S-Sinitial > maxPadding)
      maxPadding = (unsigned int)floor(S-Sinitial);

  if (OutputStats)
  {
    fprintf(statfile,"\nrate control: end of picture\n");
    fprintf(statfile," actual number of bits: S=%.1f\n",Sinitial);
    fprintf(statfile," padding bits added: P=%.1f\n", S-Sinitial);
    fprintf(statfile," average quantization parameter Q=%.1f\n",
      (double)Q/(mb_width*mb_height2));
    fprintf(statfile," remaining number of bits in GOP: R=%.1f\n",R);
    fprintf(statfile,
      " global complexity measures (I,P,B): Xi=%d, Xp=%d, Xb=%d\n",
      Xi, Xp, Xb);
    fprintf(statfile,
      " virtual buffer fullness (I,P,B): d0i=%d, d0p=%d, d0b=%d\n",
      d0i, d0p, d0b);
    fprintf(statfile," remaining number of P pictures in GOP: Np=%d\n",Np);
    fprintf(statfile," remaining number of B pictures in GOP: Nb=%d\n",Nb);
    fprintf(statfile," average activity: avg_act=%.1f\n", avg_act);
  }
}

/* compute initial quantization stepsize (at the beginning of picture) */
int rc_start_mb()
{
  int mquant;

  if (q_scale_type)
  {
    mquant = (int) floor(2.0*d*31.0/r + 0.5);

    /* clip mquant to legal (linear) range */
    if (mquant<1)
      mquant = 1;
    if (mquant>112)
      mquant = 112;

    /* map to legal quantization level */
    mquant = non_linear_mquant_table[map_non_linear_mquant[mquant]];
  }
  else
  {
    mquant = (int) floor(d*31.0/r + 0.5);
    mquant <<= 1;

    /* clip mquant to legal (linear) range */
    if (mquant<2)
      mquant = 2;
    if (mquant>62)
      mquant = 62;

    prev_mquant = mquant;
  }

/*
  fprintf(statfile,"rc_start_mb:\n");
  fprintf(statfile,"mquant=%d\n",mquant);
 */

  if (mquant > max_mquant)
    max_mquant = mquant;
  if (mquant < min_mquant)
    min_mquant = mquant;
  total_mquant += mquant;
  mquant_count++;
  avg_mquant = (double) total_mquant / (double) mquant_count;

  return mquant;
}

void update_mq(int q)
{
  prev_mquant = q;
  Q += q; /* for calculation of average mquant */
}

/* Step 2: measure virtual buffer - estimated buffer discrepancy */
int rc_calc_mquant(int j)
{
  int mquant;
  double dj, Qj, actj, N_actj;

  /* measure virtual buffer discrepancy from uniform distribution model */
  dj = d + (bitcount(&videobs)-S) - j*(T/(mb_width*mb_height2));

  /* scale against dynamic range of mquant and the bits/picture count */
  Qj = dj*31.0/r;
/*Qj = dj*(q_scale_type ? 56.0 : 31.0)/r;  */

  actj = mbinfo[j].act;
  actsum+= actj;

  /* compute normalized activity */
  N_actj = (2.0*actj+avg_act)/(actj+2.0*avg_act);

  if (q_scale_type)
  {
    /* modulate mquant with combined buffer and local activity measures */
    mquant = (int) floor(2.0*Qj*N_actj + 0.5);

    /* clip mquant to legal (linear) range */
    if (mquant<1)
      mquant = 1;
    if (mquant>112)
      mquant = 112;

    /* map to legal quantization level */
    mquant = non_linear_mquant_table[map_non_linear_mquant[mquant]];
  }
  else
  {
    /* modulate mquant with combined buffer and local activity measures */
    mquant = (int) floor(Qj*N_actj + 0.5);
    mquant <<= 1;

    /* clip mquant to legal (linear) range */
    if (mquant<2)
      mquant = 2;
    if (mquant>62)
      mquant = 62;

    /* ignore small changes in mquant */
    if (mquant>=8 && (mquant-prev_mquant)>=-4 && (mquant-prev_mquant)<=4)
      mquant = prev_mquant;

    //prev_mquant = mquant;
  }

//  Q+= mquant; /* for calculation of average mquant */

/*
  fprintf(statfile,"rc_calc_mquant(%d): ",j);
  fprintf(statfile,"bitcount=%.1f, dj=%f, Qj=%f, actj=%f, N_actj=%f, mquant=%d\n",
    bitcount(),dj,Qj,actj,N_actj,mquant);
*/

  if (mquant > max_mquant)
    max_mquant = mquant;
  if (mquant < min_mquant)
    min_mquant = mquant;
  total_mquant += mquant;
  mquant_count++;
  avg_mquant = (double) total_mquant / (double)mquant_count;

  return mquant;
}

/* compute variance of 8x8 block */
static double var_sblk(unsigned char *p, int lx)
{
  int i, j;
  unsigned int v, s, s2;

  s = s2 = 0;

  for (j=0; j<8; j++)
  {
    for (i=0; i<8; i++)
    {
      v = *p++;
      s+= v;
      s2+= v*v;
    }
    p+= lx - 8;
  }

  return s2/64.0 - (s/64.0)*(s/64.0);
}

/* VBV calculations
 *
 * generates warnings if underflow or overflow occurs
 */

/* vbv_end_of_picture
 *
 * - has to be called directly after writing picture_data()
 * - needed for accurate VBV buffer overflow calculation
 * - assumes there is no byte stuffing prior to the next start code
 */

void vbv_end_of_picture()
{
  bitcnt_EOP = bitcount(&videobs);
  bitcnt_EOP = floor((bitcnt_EOP + 7.0) / 8.0) * 8.0; /* account for bit stuffing */  
}

/* calc_vbv_delay
 *
 * has to be called directly after writing the picture start code, the
 * reference point for vbv_delay
 */

void calc_vbv_delay()
{
  double picture_delay;
  double tmp_frame_rate;

  if (video_pulldown_flag != PULLDOWN_NONE)
	tmp_frame_rate = 29.97;
  else
	tmp_frame_rate = frame_rate;

  /* number of 1/90000 s ticks until next picture is to be decoded */
  if (pict_type == B_TYPE)
  {
    if (prog_seq)
    {
      if (!tmp_repeatfirst)
        picture_delay = 90000.0/frame_rate; /* 1 frame */
      else
      {
        if (!tmp_topfirst)
          picture_delay = 90000.0*2.0/frame_rate; /* 2 frames */
        else
          picture_delay = 90000.0*3.0/frame_rate; /* 3 frames */
      }
    }
    else
    {
      /* interlaced */
      if (fieldpic)
        picture_delay = 90000.0/(2.0*frame_rate); /* 1 field */
      else
      {
        if (!tmp_repeatfirst)
          picture_delay = 90000.0*2.0/(2.0*tmp_frame_rate); /* 2 flds */
        else
          picture_delay = 90000.0*3.0/(2.0*tmp_frame_rate); /* 3 flds */
      }
    }
  }
  else
  {
    /* I or P picture */
    if (fieldpic)
    {
      if(topfirst==(pict_struct==TOP_FIELD))
      {
        /* first field */
        picture_delay = 90000.0/(2.0*frame_rate);
      }
      else
      {
        /* second field */
        /* take frame reordering delay into account */
        picture_delay = next_ip_delay - 90000.0/(2.0*frame_rate);
      }
    }
    else
    {
      /* frame picture */
      /* take frame reordering delay into account*/
      picture_delay = next_ip_delay;
    }

    if (!fieldpic || topfirst!=(pict_struct==TOP_FIELD))
    {
      /* frame picture or second field */
      if (prog_seq)
      {
        if (!tmp_repeatfirst)
          next_ip_delay = 90000.0/frame_rate;
        else
        {
          if (!tmp_topfirst)
            next_ip_delay = 90000.0*2.0/frame_rate;
          else
            next_ip_delay = 90000.0*3.0/frame_rate;
        }
      }
      else
      {
        if (fieldpic)
          next_ip_delay = 90000.0/(2.0*frame_rate);
        else
        {
          if (!tmp_repeatfirst)
            next_ip_delay = 90000.0*2.0/(2.0*tmp_frame_rate);
          else
            next_ip_delay = 90000.0*3.0/(2.0*tmp_frame_rate);
        }
      }
    }
  }

  if (decoding_time==0.0)
  {
    /* first call of calc_vbv_delay */
    /* we start with a 7/8 filled VBV buffer (12.5% back-off) */
    picture_delay = ((vbv_buffer_size*16384*7)/8)*90000.0/bit_rate;
    if (fieldpic)
      next_ip_delay = (int)floor(90000.0/frame_rate+0.5);
    vbv_delay = (int)picture_delay;
  }

  /* VBV checks */

  /* check for underflow (previous picture) */
  if (!low_delay && (decoding_time < bitcnt_EOP*90000.0/bit_rate))
  {
    if (!fixed_vbv_delay)
    {
        if (frame_index != frame_index_underflow)
        {
            /* picture not completely in buffer at intended decoding time */
            if (bb_verbose)
            {
                sprintf(errortext,"vbv_delay underflow! (decoding_time=%.1f, t_EOP=%.1f, bitcount=%.1f, frame=%d)",
                    decoding_time, bitcnt_EOP*90000.0/bit_rate, bitcount(&videobs), currentFrame);
                DisplayWarning(errortext);
            }
        vbvUnderflows++;
        frame_index_underflow = frame_index;
        }
    }
  }


  /* when to decode current frame */
  decoding_time += picture_delay;
  frame_index += 1;

  vbv_delay = (int)floor(decoding_time - bitcnt_EOP*90000.0/bit_rate);

  /* check for overflow (current picture) */
  if (vbv_delay > vbv_max_delay)
  {
    if (!fixed_vbv_delay)
    {
      if (frame_index != frame_index_overflow)
      {
          if (bb_verbose)
          {
              sprintf(errortext, "vbv_delay overflow! (decoding_time=%.1f, bitcnt_EOP=%.1f, bit_rate=%.1f, vbv_buffer_size=%d, bitcount=%.1f, frame=%d)",
                           decoding_time, bitcnt_EOP, bit_rate, vbv_buffer_size, bitcount(&videobs), currentFrame);
              DisplayWarning(errortext);
          }
          vbvOverflows++;
          frame_index_overflow = frame_index;
      }
    }
  }
  

  if (OutputStats)
    fprintf(statfile,
      "\nvbv_delay=%d (bitcount=%.1f, decoding_time=%.2f, bitcnt_EOP=%.1f)\n",
      vbv_delay,bitcount(&videobs),decoding_time,bitcnt_EOP);

  vbv_delay = (int)floor(decoding_time - bitcount(&videobs)*90000.0/bit_rate);


  /* compute a minimum picture size, in order to prevent future vbv overflow        */
  /* the strategy is to look ahead two pictures, and assuming that no picture bits  */
  /* are generated between now and then.  Set the minimum picture size to guarantee */
  /* that the overflow is avoided.  Since the picture is padded to the min size,    */
  /* should completely eliminate vbvOverflows.  Mininum also limits target T        */

  min_picture_bits = 
      8.0 * floor(((bit_rate * (vbv_delay - vbv_max_delay)/90000.0 + 2.0*bit_rate/frame_rate) + 7.0)/8.0);
  if (min_picture_bits < 0.0)
      min_picture_bits = 0.0;


  /* compute a maximum picture size, in order to prevent future vbv underflow       */
  /* strategy is to compute the number of bits which will create on underflow on    */
  /* the current picture.  This will limit the target (T).                          */

  /* note the - 100.0 - this is to ensure that header overhead doesn't push us over */
  /* note also the the encoder will still sometimes exceed max_picture_bits         */

  max_picture_bits = 
      8.0 * (floor) (((decoding_time * bit_rate / 90000.0)- bitcount(&videobs))/8.0) - 100.0;

  
  if (vbv_delay<0)
  {
    if (!fixed_vbv_delay)
    {
      if (frame_index != frame_index_underflow)
      {
          if (bb_verbose)
          {
              sprintf(errortext,"vbv_delay underflow=%d, bitcount=%.1f, frame=%d",vbv_delay, bitcount(&videobs), currentFrame);
              DisplayWarning(errortext);
          }
          vbvUnderflows++;
          frame_index_underflow = frame_index;
      }
    }
    vbv_delay = 0;
  }

  if (vbv_delay > 65535.0)
  {
    if (!fixed_vbv_delay)
    {
      if (frame_index != frame_index_overflow)
      {
          if (bb_verbose)
          {
              sprintf(errortext,"vbv_delay overflow=%d, bitcount=%.1f, frame=%d",vbv_delay, bitcount(&videobs), currentFrame);
              DisplayWarning(errortext);
          }
          vbvOverflows++;
          frame_index_overflow = frame_index;
      }
    }
    vbv_delay = 65535;
  }
}
