/* puthdr.c, generation of headers                                          */

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

static int frametotc(int frame);

/* generate sequence header (6.2.2.1, 6.3.3)
 *
 * matrix download not implemented
 */
void putseqhdr()
{
  int i;

  double Start;

  Start = bitcount (&videobs);
  
  //  alignbits(&videobs); moved to dovideo
  putbits(&videobs, SEQ_START_CODE,32); /* sequence_header_code */
  putbits(&videobs, horizontal_size,12); /* horizontal_size_value */
  putbits(&videobs, vertical_size,12); /* vertical_size_value */
  putbits(&videobs, aspectratio,4); /* aspect_ratio_information */
  switch (video_pulldown_flag)
  {
    case PULLDOWN_NONE:
    case PULLDOWN_AUTO:
      putbits(&videobs, frame_rate_code,4); /* use actual frame_rate_code */
      break;
    case PULLDOWN_32:
      putbits(&videobs, 4, 4);
      break;
    case PULLDOWN_23:
	  if (frame_rate_code == 1)
        putbits(&videobs, 4, 4); /* 23.976fps, make it 29.97fps */
	  else
        putbits(&videobs, 5, 4); /* 24.0fps, make it 30.0fps */
      break;
  }
  if (constant_bitrate)
    putbits(&videobs, (int)ceil(bit_rate/400.0),18); /* bit_rate_value */
  else
    if (video_type < MPEG_MPEG2)
      putbits(&videobs, 0x3FFFF,18); /* MPEG-1 variable bitrate indicator */
    else
      putbits(&videobs, (int)ceil(max_bit_rate/400.0),18); /* max bit_rate_value */
  putbits(&videobs, 1,1); /* marker_bit */
  putbits(&videobs, vbv_buffer_size,10); /* vbv_buffer_size_value */
  putbits(&videobs, constrparms,1); /* constrained_parameters_flag */

  putbits(&videobs, load_iquant,1); /* load_intra_quantizer_matrix */
  if (load_iquant)
    for (i=0; i<64; i++)  /* matrices are always downloaded in zig-zag order */
      putbits(&videobs, intra_q[zig_zag_scan[i]],8); /* intra_quantizer_matrix */

  putbits(&videobs, load_niquant,1); /* load_non_intra_quantizer_matrix */
  if (load_niquant)
    for (i=0; i<64; i++)
      putbits(&videobs, inter_q[zig_zag_scan[i]],8); /* non_intra_quantizer_matrix */

  headerSum += bitcount (&videobs) - Start;
}

/* generate sequence extension (6.2.2.3, 6.3.5) header (MPEG-2 only) */
void putseqext()
{
  double Start;

  Start = bitcount (&videobs);

  alignbits(&videobs);
  putbits(&videobs, EXT_START_CODE,32); /* extension_start_code */
  putbits(&videobs, SEQ_ID,4); /* extension_start_code_identifier */
  putbits(&videobs, (profile<<4)|level,8); /* profile_and_level_indication */
  putbits(&videobs, prog_seq,1); /* progressive sequence */
  putbits(&videobs, chroma_format,2); /* chroma_format */
  putbits(&videobs, horizontal_size>>12,2); /* horizontal_size_extension */
  putbits(&videobs, vertical_size>>12,2); /* vertical_size_extension */
  if (constant_bitrate)
    putbits(&videobs, ((int)ceil(bit_rate/400.0))>>18,12); /* bit_rate_extension */
  else
    putbits(&videobs, ((int)ceil(max_bit_rate/400.0))>>18,12); /* max bit_rate_extension */
  putbits(&videobs, 1,1); /* marker_bit */
  putbits(&videobs, vbv_buffer_size>>10,8); /* vbv_buffer_size_extension */
  putbits(&videobs, 0,1); /* low_delay  -- currently not implemented */
  putbits(&videobs, 0,2); /* frame_rate_extension_n */
  putbits(&videobs, 0,5); /* frame_rate_extension_d */

  headerSum += bitcount (&videobs) - Start;
}

/* generate sequence display extension (6.2.2.4, 6.3.6)
 *
 * content not yet user setable
 */
void putseqdispext()
{ 
  if (write_sde)
  {
    double Start;

    Start = bitcount (&videobs);

    alignbits(&videobs);
    putbits(&videobs, EXT_START_CODE,32); /* extension_start_code */
    putbits(&videobs, DISP_ID,4); /* extension_start_code_identifier */
    putbits(&videobs, video_format,3); /* video_format */
    putbits(&videobs, 1,1); /* colour_description */
    putbits(&videobs, color_primaries,8); /* colour_primaries */
    putbits(&videobs, transfer_characteristics,8); /* transfer_characteristics */
    putbits(&videobs, matrix_coefficients,8); /* matrix_coefficients */
    putbits(&videobs, display_horizontal_size,14); /* display_horizontal_size */
    putbits(&videobs, 1,1); /* marker_bit */
    putbits(&videobs, display_vertical_size,14); /* display_vertical_size */

    headerSum += bitcount (&videobs) - Start;
  }
}

void putseqhdr_all(void)
{
  putseqhdr();
  if (video_type > MPEG_VCD)
  {
    putseqext();
    putseqdispext();
  }
}

/* output a zero terminated string as user data (6.2.2.2.2, 6.3.4.1)
 *
 * string must not emulate start codes
 */
void putuserdata(char *userdata)
{
  double Start;
  
  Start = bitcount (&videobs);

  alignbits(&videobs);
  putbits(&videobs, USER_START_CODE,32); /* user_data_start_code */
  while (*userdata)
    putbits(&videobs, *userdata++,8);

  headerSum += bitcount (&videobs) - Start;
}

/* generate group of pictures header (6.2.2.6, 6.3.9)
 *
 * uses tc0 (timecode of first frame) and frame0 (number of first frame)
 */
void putgophdr(
int frame, int closed_gop)
{
  int tc;
  double Start;

  Start = bitcount (&videobs);

  alignbits(&videobs);
  putbits(&videobs, GOP_START_CODE,32); /* group_start_code */
  tc = frametotc(tc0+frame);
  putbits(&videobs, tc, 25); /* time_code */
  putbits(&videobs, closed_gop,1); /* closed_gop */
  putbits(&videobs, 0,1); /* broken_link */

  headerSum += bitcount (&videobs) - Start;
}

/* convert frame number to time_code
 *
 * drop_frame not implemented
 */
int frametotc(int frame)
{
  int fps, pict, sec, minute, hour, tc, temp = frame;

  if ((video_pulldown_flag != PULLDOWN_32) &&
      (video_pulldown_flag != PULLDOWN_23))
    fps = (int)(frame_rate+0.5);
  else
  {
    /* convert 24fps frame number to 29.97fps frame number */
    fps = 30;
    frame = ((frame / 2) * 2 + (frame / 2) * 3) >> 1;
  }
  pict = frame%fps;
  frame = (frame-pict)/fps;
  sec = frame%60;
  frame = (frame-sec)/60;
  minute = frame%60;
  frame = (frame-minute)/60;
  hour = frame%24;
  tc = (hour<<19) | (minute<<13) | (1<<12) | (sec<<6) | pict;
  return tc;
}

/* generate picture header (6.2.3, 6.3.10) */
void putpicthdr()
{
  double Start;
  Start = bitcount(&videobs);

  alignbits(&videobs);
  putbits(&videobs, PICTURE_START_CODE,32); /* picture_start_code */
  if (constant_bitrate)
    calc_vbv_delay();
  putbits(&videobs, temp_ref,10); /* temporal_reference */
  putbits(&videobs, pict_type,3); /* picture_coding_type */
  //if (constant_bitrate)
  //  calc_vbv_delay();
  if (fixed_vbv_delay)
    putbits(&videobs, 0xFFFF, 16); /* force a vbv_delay value of 0xFFFF */
  else
    putbits(&videobs, vbv_delay,16); /* vbv_delay */

  if (pict_type==P_TYPE || pict_type==B_TYPE)
  {
    putbits(&videobs, 0,1); /* full_pel_forward_vector */
    if (video_type < MPEG_MPEG2)
      putbits(&videobs, forw_hor_f_code,3);
    else
      putbits(&videobs, 7,3); /* forward_f_code */
  }

  if (pict_type==B_TYPE)
  {
    putbits(&videobs, 0,1); /* full_pel_backward_vector */
    if (video_type < MPEG_MPEG2)
      putbits(&videobs, back_hor_f_code,3);
    else
      putbits(&videobs, 7,3); /* backward_f_code */
  }

  putbits(&videobs, 0,1); /* extra_bit_picture */
  headerSum += bitcount(&videobs) - Start;
}

/* generate picture coding extension (6.2.3.1, 6.3.11)
 *
 * composite display information (v_axis etc.) not implemented
 */
void putpictcodext()
{
  double Start;
    
  Start = bitcount (&videobs);
  
  alignbits(&videobs);
  putbits(&videobs, EXT_START_CODE,32); /* extension_start_code */
  putbits(&videobs, CODING_ID,4); /* extension_start_code_identifier */
  putbits(&videobs, forw_hor_f_code,4); /* forward_horizontal_f_code */
  putbits(&videobs, forw_vert_f_code,4); /* forward_vertical_f_code */
  putbits(&videobs, back_hor_f_code,4); /* backward_horizontal_f_code */
  putbits(&videobs, back_vert_f_code,4); /* backward_vertical_f_code */
  putbits(&videobs, dc_prec,2); /* intra_dc_precision */
  putbits(&videobs, pict_struct,2); /* picture_structure */
  putbits(&videobs, (pict_struct==FRAME_PICTURE)?tmp_topfirst:0,1); /* top_field_first */
  putbits(&videobs, frame_pred_dct,1); /* frame_pred_frame_dct */
  putbits(&videobs, 0,1); /* concealment_motion_vectors  -- currently not implemented */
  putbits(&videobs, q_scale_type,1); /* q_scale_type */
  putbits(&videobs, intravlc,1); /* intra_vlc_format */
  putbits(&videobs, altscan,1); /* alternate_scan */
  putbits(&videobs, tmp_repeatfirst,1); /* repeat_first_field */
  if (chroma_format != CHROMA420)
    putbits(&videobs, 0, 1);
  else
    putbits(&videobs, tmp_prog_frame,1); /* chroma_420_type */
  putbits(&videobs, tmp_prog_frame,1); /* progressive_frame */
  putbits(&videobs, 0,1); /* composite_display_flag */

  headerSum += bitcount(&videobs) - Start;
}

void putpictdispext()
{
  double Start;
  
  Start = bitcount (&videobs);
  alignbits(&videobs);
  putbits(&videobs, EXT_START_CODE,32); /* extension_start_code */
  putbits(&videobs, PANSCAN_ID,4); /* extension_start_code_identifier */
  putbits(&videobs, frame_centre_horizontal_offset,16);
  putbits(&videobs, 1, 1); /* marker bit */
  putbits(&videobs, frame_centre_vertical_offset,16);
  putbits(&videobs, 1, 1); /* marker bit */

  headerSum += bitcount (&videobs) - Start;
}

/* generate sequence_end_code (6.2.2) */
void putseqend()
{
  double Start;
  if (write_sec)
  {
    Start = bitcount (&videobs);
    alignbits(&videobs);
    putbits(&videobs, SEQ_END_CODE,32);

    headerSum += bitcount (&videobs) - Start;

  }
}
