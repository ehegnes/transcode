/* putpic.c, block and motion vector encoding routines                      */

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

/* private prototype */

static void putmvs(int MV[2][2][2], int PMV[2][2][2], int mv_field_sel[2][2],
                int dmvector[2], int s, int motion_type, int hor_f_code, int vert_f_code);


/* quantization / variable length encoding of a complete picture */
int putpict(unsigned char *frame)
{
  int qi;
  static short sv_blocks[1536];
  int i, j, k, comp, cc;
  int mb_type;
  int PMV[2][2][2];
  int prev_mquant, mquant;
  int cbp, MBAinc;

  if (constant_bitrate)
    rc_init_pict(frame); /* set up rate control */
  else
  {
    if (q_scale_type)
      mquant = non_linear_mquant_table[mquant_value];
    else
      mquant = mquant_value << 1;
  }

  /* picture header and picture coding extension */
  putpicthdr();

  if (video_type > MPEG_VCD)
    putpictcodext();

  if (write_pde && write_sde && (video_type > MPEG_VCD) && (pict_struct == FRAME_PICTURE))
    putpictdispext();

  /* put in svcd scan information data */
  if (embed_SVCD_user_blocks && (pict_type == I_TYPE))
  {
    double Start;
    Start = bitcount(&videobs);

    alignbits(&videobs);
    putbits(&videobs, USER_START_CODE, 32); /* user_data_start_code */
    putbits(&videobs, 0x10, 8);             /* tag_name = scan information */
    putbits(&videobs, 0x0E, 8);             /* U_length */
    putbits(&videobs, 0x008080, 24);        /* Previous_I_offset */
    putbits(&videobs, 0x008080, 24);        /* Next_I_offset */
    putbits(&videobs, 0x008080, 24);        /* Backward_offset */
    putbits(&videobs, 0x008080, 24);        /* Forward_offset */

    headerSum += bitcount (&videobs) - Start;
  }

  if (constant_bitrate)
    prev_mquant = rc_start_mb(); /* initialize quantization parameter */
  else
    prev_mquant = mquant;

  k = 0;

  for (j=0; j<mb_height2; j++)
  {
    /* macroblock row loop */

    for (i=0; i<mb_width; i++)
    {
      /* macroblock loop */
      if ((i == 0) && ((j == 0) || slice_hdr_every_MBrow))
      {
        /* slice header (6.2.4) */
        alignbits(&videobs);

        if ((video_type < MPEG_MPEG2) || vertical_size<=2800)
          putbits(&videobs, SLICE_MIN_START+j,32); /* slice_start_code */
        else
        {
          putbits(&videobs, SLICE_MIN_START+(j&127),32); /* slice_start_code */
          putbits(&videobs, j>>7,3); /* slice_vertical_position_extension */
        }

        /* quantiser_scale_code */
        putbits(&videobs, q_scale_type ? map_non_linear_mquant[prev_mquant]
                             : prev_mquant >> 1, 5);

        putbits(&videobs, 0,1); /* extra_bit_slice */

        /* reset predictors */

        for (cc=0; cc<3; cc++)
          dc_dct_pred[cc] = 0;

        PMV[0][0][0]=PMV[0][0][1]=PMV[1][0][0]=PMV[1][0][1]=0;
        PMV[0][1][0]=PMV[0][1][1]=PMV[1][1][0]=PMV[1][1][1]=0;

        MBAinc = i + 1; /* first MBAinc denotes absolute position */
      }

      mb_type = mbinfo[k].mb_type;

      if (constant_bitrate)
        /* determine mquant (rate control) */
        mbinfo[k].mquant = rc_calc_mquant(k);
      else
        mbinfo[k].mquant = mquant;

      /* quantize macroblock */
      if (mb_type & MB_INTRA)
      {
        if (constant_bitrate)
        {
          memcpy(sv_blocks, blocks[k * block_count], block_count*64*sizeof(short)); // Save blocks

          while(1)
          {
            qi = 1;
            for (comp = 0; comp < block_count; comp++)
            {
              qi &= quant_intra (blocks[k * block_count + comp],
                 blocks[k * block_count + comp], dc_prec, intra_q, mbinfo[k].mquant);
              if (qi==0 && mbinfo[k].mquant < intra_q[31])
                break;
              else
                qi = 1;
            }
            if (qi ==1)
              break;
            mbinfo[k].mquant +=2;
            memcpy(blocks[k * block_count], sv_blocks, block_count*64*sizeof(short));
          }
          mbinfo[k].cbp = cbp = (1 << block_count) - 1;
        }
        else
        {
          for (comp=0; comp<block_count; comp++)
            quant_intra(blocks[k*block_count+comp],blocks[k*block_count+comp],
                        dc_prec,intra_q,mbinfo[k].mquant);
          mbinfo[k].cbp = cbp = (1<<block_count) - 1;
        }
      }
      else
      {
        cbp = 0;
        for (comp=0;comp<block_count;comp++)
          cbp = (cbp<<1) | quant_non_intra(blocks[k*block_count+comp],
                                           blocks[k*block_count+comp],
                                           s_inter_q,i_inter_q,mbinfo[k].mquant);

        mbinfo[k].cbp = cbp;

        if (cbp)
          mb_type|= MB_PATTERN;
      }
      if (constant_bitrate)
        update_mq(mbinfo[k].mquant);

      /* output mquant if it has changed */
      if (cbp && prev_mquant!=mbinfo[k].mquant)
        mb_type|= MB_QUANT;

      /* check if macroblock can be skipped */
      if (i!=0 && i!=mb_width-1 && !cbp)
      {
        /* no DCT coefficients and neither first nor last macroblock of slice */

        if (pict_type==P_TYPE && !(mb_type&MB_FORWARD))
        {
          /* P picture, no motion vectors -> skip */

          /* reset predictors */

          for (cc=0; cc<3; cc++)
            dc_dct_pred[cc] = 0;

          PMV[0][0][0]=PMV[0][0][1]=PMV[1][0][0]=PMV[1][0][1]=0;
          PMV[0][1][0]=PMV[0][1][1]=PMV[1][1][0]=PMV[1][1][1]=0;

          mbinfo[k].mb_type = mb_type;
          mbinfo[k].skipped = 1;
          MBAinc++;
          k++;
          continue;
        }

        if (pict_type==B_TYPE && pict_struct==FRAME_PICTURE
            && mbinfo[k].motion_type==MC_FRAME
            && ((mbinfo[k-1].mb_type^mb_type)&(MB_FORWARD|MB_BACKWARD))==0
            && (!(mb_type&MB_FORWARD) ||
                (PMV[0][0][0]==mbinfo[k].MV[0][0][0] &&
                 PMV[0][0][1]==mbinfo[k].MV[0][0][1]))
            && (!(mb_type&MB_BACKWARD) ||
                (PMV[0][1][0]==mbinfo[k].MV[0][1][0] &&
                 PMV[0][1][1]==mbinfo[k].MV[0][1][1])))
        {
          /* conditions for skipping in B frame pictures:
           * - must be frame predicted
           * - must be the same prediction type (forward/backward/interp.)
           *   as previous macroblock
           * - relevant vectors (forward/backward/both) have to be the same
           *   as in previous macroblock
           */

          mbinfo[k].mb_type = mb_type;
          mbinfo[k].skipped = 1;
          MBAinc++;
          k++;
          continue;
        }

        if (pict_type==B_TYPE && pict_struct!=FRAME_PICTURE
            && mbinfo[k].motion_type==MC_FIELD
            && ((mbinfo[k-1].mb_type^mb_type)&(MB_FORWARD|MB_BACKWARD))==0
            && (!(mb_type&MB_FORWARD) ||
                (PMV[0][0][0]==mbinfo[k].MV[0][0][0] &&
                 PMV[0][0][1]==mbinfo[k].MV[0][0][1] &&
                 mbinfo[k].mv_field_sel[0][0]==(pict_struct==BOTTOM_FIELD)))
            && (!(mb_type&MB_BACKWARD) ||
                (PMV[0][1][0]==mbinfo[k].MV[0][1][0] &&
                 PMV[0][1][1]==mbinfo[k].MV[0][1][1] &&
                 mbinfo[k].mv_field_sel[0][1]==(pict_struct==BOTTOM_FIELD))))
        {
          /* conditions for skipping in B field pictures:
           * - must be field predicted
           * - must be the same prediction type (forward/backward/interp.)
           *   as previous macroblock
           * - relevant vectors (forward/backward/both) have to be the same
           *   as in previous macroblock
           * - relevant motion_vertical_field_selects have to be of same
           *   parity as current field
           */

          mbinfo[k].mb_type = mb_type;
          mbinfo[k].skipped = 1;
          MBAinc++;
          k++;
          continue;
        }
      }

      /* macroblock cannot be skipped */
      mbinfo[k].skipped = 0;

      /* there's no VLC for 'No MC, Not Coded':
       * we have to transmit (0,0) motion vectors
       */
      if (pict_type==P_TYPE && !cbp && !(mb_type&MB_FORWARD))
        mb_type|= MB_FORWARD;

      putaddrinc(MBAinc); /* macroblock_address_increment */
      MBAinc = 1;

      putmbtype(pict_type,mb_type); /* macroblock type */

      if (mb_type & (MB_FORWARD|MB_BACKWARD) && !frame_pred_dct)
        putbits(&videobs, mbinfo[k].motion_type,2);

      if (pict_struct==FRAME_PICTURE && cbp && !frame_pred_dct)
        putbits(&videobs, mbinfo[k].dct_type,1);

      if (mb_type & MB_QUANT)
      {
        putbits(&videobs, q_scale_type ? map_non_linear_mquant[mbinfo[k].mquant]
                             : mbinfo[k].mquant>>1,5);
        prev_mquant = mbinfo[k].mquant;
      }

      if (mb_type & MB_FORWARD)
      {
        /* forward motion vectors, update predictors */
        putmvs(mbinfo[k].MV,PMV,mbinfo[k].mv_field_sel,mbinfo[k].dmvector,0,
          mbinfo[k].motion_type,forw_hor_f_code,forw_vert_f_code);
      }

      if (mb_type & MB_BACKWARD)
      {
        /* backward motion vectors, update predictors */
        putmvs(mbinfo[k].MV,PMV,mbinfo[k].mv_field_sel,mbinfo[k].dmvector,1,
          mbinfo[k].motion_type,back_hor_f_code,back_vert_f_code);
      }

      if (mb_type & MB_PATTERN)
      {
        putcbp((cbp >> (block_count-6)) & 63);
        if (chroma_format!=CHROMA420)
          putbits(&videobs, cbp,block_count-6);
      }

      for (comp=0; comp<block_count; comp++)
      {
        /* block loop */
        if (cbp & (1<<(block_count-1-comp)))
        {
          if (mb_type & MB_INTRA)
          {
            cc = (comp<4) ? 0 : (comp&1)+1;
            if (!putintrablk(blocks[k*block_count+comp],cc))
              return FALSE;
          }
          else
            if (!putnonintrablk(blocks[k*block_count+comp]))
              return FALSE;
        }
      }

      /* reset predictors */
      if (!(mb_type & MB_INTRA))
        for (cc=0; cc<3; cc++)
          dc_dct_pred[cc] = 0;

      if (mb_type & MB_INTRA || (pict_type==P_TYPE && !(mb_type & MB_FORWARD)))
      {
        PMV[0][0][0]=PMV[0][0][1]=PMV[1][0][0]=PMV[1][0][1]=0;
        PMV[0][1][0]=PMV[0][1][1]=PMV[1][1][0]=PMV[1][1][1]=0;
      }

      mbinfo[k].mb_type = mb_type;
      k++;
    }
  }

  // - padding added by rc_update_pict is part of this picture 
  //vbv_end_of_picture();

  if (constant_bitrate)
  {
    rc_update_pict();
    vbv_end_of_picture();
  }
  rc_update_max();
  return TRUE;
}


/* output motion vectors (6.2.5.2, 6.3.16.2)
 *
 * this routine also updates the predictions for motion vectors (PMV)
 */

void putmvs(
int MV[2][2][2], int PMV[2][2][2],
int mv_field_sel[2][2],
int dmvector[2],
int s, int motion_type, int hor_f_code, int vert_f_code)
{
  if (pict_struct==FRAME_PICTURE)
  {
    if (motion_type==MC_FRAME)
    {
      /* frame prediction */
      putmv(MV[0][s][0]-PMV[0][s][0],hor_f_code);
      putmv(MV[0][s][1]-PMV[0][s][1],vert_f_code);
      PMV[0][s][0]=PMV[1][s][0]=MV[0][s][0];
      PMV[0][s][1]=PMV[1][s][1]=MV[0][s][1];
    }
    else if (motion_type==MC_FIELD)
    {
      /* field prediction */
      putbits(&videobs, mv_field_sel[0][s],1);
      putmv(MV[0][s][0]-PMV[0][s][0],hor_f_code);
      putmv((MV[0][s][1]>>1)-(PMV[0][s][1]>>1),vert_f_code);
      putbits(&videobs, mv_field_sel[1][s],1);
      putmv(MV[1][s][0]-PMV[1][s][0],hor_f_code);
      putmv((MV[1][s][1]>>1)-(PMV[1][s][1]>>1),vert_f_code);
      PMV[0][s][0]=MV[0][s][0];
      PMV[0][s][1]=MV[0][s][1];
      PMV[1][s][0]=MV[1][s][0];
      PMV[1][s][1]=MV[1][s][1];
    }
    else
    {
      /* dual prime prediction */
      putmv(MV[0][s][0]-PMV[0][s][0],hor_f_code);
      putdmv(dmvector[0]);
      putmv((MV[0][s][1]>>1)-(PMV[0][s][1]>>1),vert_f_code);
      putdmv(dmvector[1]);
      PMV[0][s][0]=PMV[1][s][0]=MV[0][s][0];
      PMV[0][s][1]=PMV[1][s][1]=MV[0][s][1];
    }
  }
  else
  {
    /* field picture */
    if (motion_type==MC_FIELD)
    {
      /* field prediction */
      putbits(&videobs, mv_field_sel[0][s],1);
      putmv(MV[0][s][0]-PMV[0][s][0],hor_f_code);
      putmv(MV[0][s][1]-PMV[0][s][1],vert_f_code);
      PMV[0][s][0]=PMV[1][s][0]=MV[0][s][0];
      PMV[0][s][1]=PMV[1][s][1]=MV[0][s][1];
    }
    else if (motion_type==MC_16X8)
    {
      /* 16x8 prediction */
      putbits(&videobs, mv_field_sel[0][s],1);
      putmv(MV[0][s][0]-PMV[0][s][0],hor_f_code);
      putmv(MV[0][s][1]-PMV[0][s][1],vert_f_code);
      putbits(&videobs, mv_field_sel[1][s],1);
      putmv(MV[1][s][0]-PMV[1][s][0],hor_f_code);
      putmv(MV[1][s][1]-PMV[1][s][1],vert_f_code);
      PMV[0][s][0]=MV[0][s][0];
      PMV[0][s][1]=MV[0][s][1];
      PMV[1][s][0]=MV[1][s][0];
      PMV[1][s][1]=MV[1][s][1];
    }
    else
    {
      /* dual prime prediction */
      putmv(MV[0][s][0]-PMV[0][s][0],hor_f_code);
      putdmv(dmvector[0]);
      putmv(MV[0][s][1]-PMV[0][s][1],vert_f_code);
      putdmv(dmvector[1]);
      PMV[0][s][0]=PMV[1][s][0]=MV[0][s][0];
      PMV[0][s][1]=PMV[1][s][1]=MV[0][s][1];
    }
  }
}
