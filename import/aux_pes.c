/*
 *  aux_pes.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  This file is part of transcode, a video stream processing tool
 *
 *  transcode is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  transcode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "transcode.h"
#include "probe.h"
#include "libtc/libtc.h"
#include "libtc/ratiocodes.h"

#include <math.h>

#include "ioaux.h"
#include "tc.h"
#include "aux_pes.h"


#if 0 // EMS not used
static int cmp_32_bits(char *buf, long x)
{

    if (buf[0] != ((x >> 24) & 0xff))
	return 0;
    if (buf[1] != ((x >> 16) & 0xff))
	return 0;
    if (buf[2] != ((x >>  8) & 0xff))
	return 0;
    if (buf[3] != ((x      ) & 0xff))
	return 0;

  // OK found it
  return 1;
}
#endif

static char * aspect_ratio_information_str[16] = {
  "Invalid Aspect Ratio",
  "1:1",
  "4:3",
  "16:9",
  "2.21:1",
  "Invalid Aspect Ratio",
  "Invalid Aspect Ratio",
  "Invalid Aspect Ratio",
  "4:3",
  "Invalid Aspect Ratio",
  "Invalid Aspect Ratio",
  "4:3",
  "Invalid Aspect Ratio",
  "Invalid Aspect Ratio",
  "Invalid Aspect Ratio"
};

static char * frame_rate_str[16] = {
  "Invalid frame_rate_code",
  "23.976", "24", "25" , "29.97",
  "30" , "50", "59.94", "60" ,
  "1", "5", "10", "12", "15",   //libmpeg3 only
  "Invalid frame_rate_code",
  "Invalid frame_rate_code"
};


int stats_sequence_silent(uint8_t * buffer, seq_info_t *seq_info)
{

  int horizontal_size;
  int vertical_size;
  int aspect_ratio_information;
  int frame_rate_code;
  int bit_rate_value;

  vertical_size = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];
  horizontal_size = ((vertical_size >> 12) + 15) & ~15;
  vertical_size = ((vertical_size & 0xfff) + 15) & ~15;

  aspect_ratio_information = buffer[3] >> 4;
  frame_rate_code = buffer[3] & 15;
  bit_rate_value = (buffer[4] << 10) | (buffer[5] << 2) | (buffer[6] >> 6);
  if(aspect_ratio_information < 0 || aspect_ratio_information>15) {
    tc_log_error(__FILE__, "****** invalid MPEG sequence header detected (%d/%d|%d/%d) ******",
		 aspect_ratio_information, 16, frame_rate_code, 16);
    return(-1);
  }

  if(frame_rate_code < 0 || frame_rate_code>15) {
    tc_log_error(__FILE__, "****** invalid MPEG sequence header detected (%d/%d|%d/%d) ******",
		 frame_rate_code, 16, aspect_ratio_information, 8);
    return(-1);
  }

  //fill out user structure

  seq_info->w = horizontal_size;
  seq_info->h = vertical_size;
  seq_info->ari = aspect_ratio_information;
  seq_info->frc = frame_rate_code;
  seq_info->brv = bit_rate_value;

  return(0);

}
int stats_sequence(uint8_t * buffer, seq_info_t *seq_info)
{

  int horizontal_size;
  int vertical_size;
  int aspect_ratio_information;
  int frame_rate_code;
  int bit_rate_value;
  int vbv_buffer_size_value;
  int constrained_parameters_flag;
  int load_intra_quantizer_matrix;
  int load_non_intra_quantizer_matrix;

  vertical_size = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];
  horizontal_size = ((vertical_size >> 12) + 15) & ~15;
  vertical_size = ((vertical_size & 0xfff) + 15) & ~15;

  aspect_ratio_information = buffer[3] >> 4;
  frame_rate_code = buffer[3] & 15;
  bit_rate_value = (buffer[4] << 10) | (buffer[5] << 2) | (buffer[6] >> 6);
  vbv_buffer_size_value = ((buffer[6] << 5) | (buffer[7] >> 3)) & 0x3ff;
  constrained_parameters_flag = buffer[7] & 4;
  load_intra_quantizer_matrix = buffer[7] & 2;
  if (load_intra_quantizer_matrix)
    buffer += 64;
  load_non_intra_quantizer_matrix = buffer[7] & 1;

  if(aspect_ratio_information < 0 || aspect_ratio_information>15) {
    tc_log_error(__FILE__, "****** invalid MPEG sequence header detected (%d/%d|%d/%d) ******",
		 aspect_ratio_information, 16, frame_rate_code, 16);
    return(-1);
  }

  if(frame_rate_code < 0 || frame_rate_code>15) {
    tc_log_error(__FILE__, "****** invalid MPEG sequence header detected (%d/%d|%d/%d) ******",
		 frame_rate_code, 16, aspect_ratio_information, 8);
    return(-1);
  }

  tc_log_msg(__FILE__,
	     "sequence: %dx%d %s, %s fps, %5.0f kbps, VBV %d kB%s%s%s",
	     horizontal_size, vertical_size,
	     aspect_ratio_information_str [aspect_ratio_information],
	     frame_rate_str [frame_rate_code],
	     bit_rate_value * 400.0 / 1000.0,
	     2 * vbv_buffer_size_value,
	     constrained_parameters_flag ? " , CP":"",
	     load_intra_quantizer_matrix ? " , Custom Intra Matrix":"",
	     load_non_intra_quantizer_matrix ? " , Custom Non-Intra Matrix":"");


  //fill out user structure

  seq_info->w = horizontal_size;
  seq_info->h = vertical_size;
  seq_info->ari = aspect_ratio_information;
  seq_info->frc = frame_rate_code;
  seq_info->brv = bit_rate_value;

  return(0);

}

int probe_sequence(uint8_t *buffer, ProbeInfo *probe_info)
{

  int horizontal_size;
  int vertical_size;
  int aspect_ratio_information;
  int frame_rate_code;
  int bit_rate_value;
  int vbv_buffer_size_value;
  int constrained_parameters_flag;
  int load_intra_quantizer_matrix;
  int load_non_intra_quantizer_matrix;

  vertical_size = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];
  horizontal_size = ((vertical_size >> 12) + 15) & ~15;
  vertical_size = ((vertical_size & 0xfff) + 15) & ~15;

  aspect_ratio_information = buffer[3] >> 4;
  frame_rate_code = buffer[3] & 15;
  bit_rate_value = (buffer[4] << 10) | (buffer[5] << 2) | (buffer[6] >> 6);
  vbv_buffer_size_value = ((buffer[6] << 5) | (buffer[7] >> 3)) & 0x3ff;
  constrained_parameters_flag = buffer[7] & 4;
  load_intra_quantizer_matrix = buffer[7] & 2;
  if (load_intra_quantizer_matrix)
    buffer += 64;
  load_non_intra_quantizer_matrix = buffer[7] & 1;

  //set some defaults, if invalid:
  if(aspect_ratio_information < 0 || aspect_ratio_information>15) aspect_ratio_information=1;

  if(frame_rate_code < 0 || frame_rate_code>15) frame_rate_code=3;

  //fill out user structure

  probe_info->width = horizontal_size;
  probe_info->height = vertical_size;
  probe_info->asr = aspect_ratio_information;
  probe_info->frc = frame_rate_code;
  probe_info->bitrate = bit_rate_value * 400.0 / 1000.0;
  tc_frc_code_to_value(frame_rate_code, &probe_info->fps);

  return(0);

}

int probe_extension(uint8_t *buffer, ProbeInfo *probe_info)
{

    int intra_dc_precision;
    int picture_structure;
    int top_field_first;
    int frame_pred_frame_dct;
    int concealment_motion_vectors;
    int q_scale_type;
    int intra_vlc_format;
    int alternate_scan;
    int repeat_first_field;
    int progressive_frame;

    intra_dc_precision = (buffer[2] >> 2) & 3;
    picture_structure = buffer[2] & 3;
    top_field_first = buffer[3] >> 7;
    frame_pred_frame_dct = (buffer[3] >> 6) & 1;
    concealment_motion_vectors = (buffer[3] >> 5) & 1;
    q_scale_type = (buffer[3] >> 4) & 1;
    intra_vlc_format = (buffer[3] >> 3) & 1;
    alternate_scan = (buffer[3] >> 2) & 1;
    repeat_first_field = (buffer[3] >> 1) & 1;
    progressive_frame = buffer[4] >> 7;

    //get infos
    probe_info->ext_attributes[2] = progressive_frame;
    probe_info->ext_attributes[3] = alternate_scan;

    if(top_field_first == 1 && repeat_first_field == 0) return(1);

  return(0);
}

#define BUF_WARN_COUNT 20

int probe_picext(uint8_t *buffer, size_t buflen)
{

  //  static char *picture_structure_str[4] = {
  //  "Invalid Picture Structure",
  //  "Top field",
  //  "Bottom field",
  //  "Frame Picture"
  //};
  static int buf_small_count = 0;
  if(buflen < 3) {
    if(buf_small_count == 0
      || (buf_small_count % BUF_WARN_COUNT) == 0) {
        tc_log_warn(__FILE__, "not enough buffer to probe picture extension "
                          "(buflen=%lu) [happened at least %i times]",
                          (unsigned long)buflen, buf_small_count);
    }
    buf_small_count++;
    return(-1); /* failed probe */
  }
  return(buffer[2] & 3);
}

void probe_group(uint8_t *buffer, size_t buflen)
{
    static int buf_small_count = 0;
    if(buflen < 5) {
        if(buf_small_count == 0
          || (buf_small_count % BUF_WARN_COUNT) == 0) {
            tc_log_warn(__FILE__, "not enough buffer to probe picture group "
                             "(buflen=%lu) [happened at least %i times]",
                             (unsigned long)buflen, buf_small_count);
        }
        buf_small_count++;
    } else {
       tc_log_msg(__FILE__, "%s%s", (buffer[4] & 0x40) ? " closed_gop" : "",
		  (buffer[4] & 0x20) ? " broken_link" : "");
    }
}

int get_pts_dts(char *buffer, unsigned long *pts, unsigned long *dts)
{
  unsigned int pes_header_bytes = 0;
  unsigned int pts_dts_flags;
  int pes_header_data_length;

  int has_pts_dts=0;

  unsigned int ptr=0;

  /* drop first 8 bits */
  ++ptr;
  pts_dts_flags = (buffer[ptr++] >> 6) & 0x3;
  pes_header_data_length = buffer[ptr++];

  switch(pts_dts_flags)

    {

    case 2:

      *pts = (buffer[ptr++] >> 1) & 7;  //low 4 bits (7==1111)
      *pts <<= 15;
      *pts |= (stream_read_int16(&buffer[ptr]) >> 1);
      ptr+=2;
      *pts <<= 15;
      *pts |= (stream_read_int16(&buffer[ptr]) >> 1);
      ptr+=2;

      pes_header_bytes += 5;

      has_pts_dts=1;

      break;

    case 3:

      *pts = (buffer[ptr++] >> 1) & 7;  //low 4 bits (7==1111)
      *pts <<= 15;
      *pts |= (stream_read_int16(&buffer[ptr]) >> 1);
      ptr+=2;
      *pts <<= 15;
      *pts |= (stream_read_int16(&buffer[ptr]) >> 1);
      ptr+=2;

      *dts = (buffer[ptr++] >> 1) & 7;
      *dts <<= 15;
      *dts |= (stream_read_int16(&buffer[ptr]) >> 1);
      ptr+=2;
      *dts <<= 15;
      *dts |= (stream_read_int16(&buffer[ptr]) >> 1);
      ptr+=2;

      pes_header_bytes += 10;

      has_pts_dts=1;

      break;

    default:

      has_pts_dts=0;
      *dts=*pts=0;
      break;
    }

  return(has_pts_dts);
}

#if 0  // unused
static unsigned long read_ts(char *_s)
{

  unsigned long pts;

  char *buffer=_s;

  unsigned int ptr=0;

  pts = (buffer[ptr++] >> 1) & 7;  //low 4 bits (7==1111)
  pts <<= 15;
  pts |= (stream_read_int16(&buffer[ptr]) >> 1);
  ptr+=2;
  pts <<= 15;
  pts |= (stream_read_int16(&buffer[ptr]) >> 1);

  return pts;
}
#endif

typedef struct timecode_struc	/* Time_code Struktur laut MPEG		*/
{   unsigned long msb;		/* fuer SCR, DTS, PTS			*/
    unsigned long lsb;
    unsigned long reference_ext;
    unsigned long negative;     /* for delays when doing multiple files */
} Timecode_struc;

#define MAX_FFFFFFFF 4294967295.0 	/* = 0xffffffff in hex.	*/


static void make_timecode (double timestamp, Timecode_struc *pointer)
{
    double temp_ts;

    if (timestamp < 0.0) {
      pointer->negative = 1;
      timestamp = -timestamp;
    } else
      pointer->negative = 0;

    temp_ts = floor(timestamp / 300.0);

    if (temp_ts > MAX_FFFFFFFF) {
      pointer->msb=1;
      temp_ts -= MAX_FFFFFFFF;
      pointer->lsb=(unsigned long)temp_ts;
    } else {
      pointer->msb=0;
      pointer->lsb=(unsigned long)temp_ts;
    }

    pointer->reference_ext = (unsigned long)(timestamp - (floor(timestamp / 300.0) * 300.0));

}

#define MPEG2_MARKER_SCR	1		/* MPEG2 Marker SCR	*/

/*************************************************************************
    Kopiert einen TimeCode in einen Bytebuffer. Dabei wird er nach
    MPEG-Verfahren in bits aufgesplittet.

    Makes a Copy of a TimeCode in a Buffer, splitting it into bitfields
    according to MPEG-System
*************************************************************************/

static void buffer_timecode_scr (Timecode_struc *pointer, unsigned char **buffer)
{

  unsigned char temp;
  unsigned char marker=MPEG2_MARKER_SCR;


  temp = (marker << 6) | (pointer->msb << 5) |
    ((pointer->lsb >> 27) & 0x18) | 0x4 | ((pointer->lsb >> 28) & 0x3);
  *((*buffer)++)=temp;
  temp = (pointer->lsb & 0x0ff00000) >> 20;
  *((*buffer)++)=temp;
  temp = ((pointer->lsb & 0x000f8000) >> 12) | 0x4 |
    ((pointer->lsb & 0x00006000) >> 13);
  *((*buffer)++)=temp;
  temp = (pointer->lsb & 0x00001fe0) >> 5;
  *((*buffer)++)=temp;
  temp = ((pointer->lsb & 0x0000001f) << 3) | 0x4 |
    ((pointer->reference_ext & 0x00000180) >> 7);
  *((*buffer)++)=temp;
  temp = ((pointer->reference_ext & 0x0000007F) << 1) | 1;
  *((*buffer)++)=temp;

}


void scr_rewrite(char *buf, uint32_t pts)
{
  Timecode_struc timecode;
  unsigned char * ucbuf = (unsigned char *)buf;

  timecode.msb = 0;
  timecode.lsb = 0;
  timecode.reference_ext = 0;
  timecode.negative = 0;

  make_timecode((double) pts, &timecode);

  buffer_timecode_scr(&timecode, &ucbuf);
}
