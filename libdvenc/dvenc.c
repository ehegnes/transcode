/*
 *  dvenc.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  This file is part of transcode, a linux video stream processing tool
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

#include <stdio.h>
#include "dvenc.h"
#include "../src/transcode.h"

#ifdef HAVE_DV

static int force_dct = -1;

unsigned char *dvenc_dvbuf, *dvenc_abuf, *dvenc_vbuf;

static dv_enc_input_filter_t  tc_in;
static dv_enc_output_filter_t tc_out;
static dv_enc_audio_input_filter_t audio_input;
static dv_enc_audio_info_t audio_info;

static int static_qno = 1;
static int vlc_encode_passes = 3;

int dvenc_init() {
  
#ifdef LIBDV_095
  dv_encoder_new(TRUE, FALSE, FALSE);
#else
  dv_init();
#endif
  
  return(0);
  
}

int dvenc_set_parameter(int codec, int format, int sample_rate) 
{

    audio_info.channels=2;
    audio_info.frequency=sample_rate;
    audio_info.bitspersample=16;
    audio_info.bytealignment=4;
    audio_info.bytespersecond=sample_rate*audio_info.bytealignment;
    
    if(format != DV_PAL_HEIGHT && format != DV_NTSC_HEIGHT) return(-1);
    
    dvenc_init_input(&tc_in, codec, format);
    dvenc_init_audio_input(&audio_input, &audio_info);

    tc_in.init(0, force_dct);
    
    //raw frames
    dvenc_init_output(&tc_out);

    return(0);
    
}

int dvenc_frame(char *_dvenc_vbuf, char *_dvenc_abuf, int aud_bytes, char *_dvenc_dvbuf)
{

  const char *dd="dummy";
  
  dvenc_dvbuf  = _dvenc_dvbuf;
  dvenc_abuf   = _dvenc_abuf;
  dvenc_vbuf   = _dvenc_vbuf;

  audio_info.bytesperframe=aud_bytes;
  
  //encode single frame
  encoder_loop(&tc_in, (dvenc_abuf==NULL)?NULL:&audio_input, &tc_out, 
	       0, 1, dd, dd, 
	       vlc_encode_passes, static_qno, 0, 25);

  //ready to go DV frame in target
  
  return(0);
}

int dvenc_close() 
{
  
  //close encoder
  tc_in.finish();
  audio_input.finish();
  tc_out.finish();
  return(0);
}

#else

int dvenc_init() {
    
    return(-1);
}

int dvenc_set_parameter(int codec, int format, int sample_rate) 
{
    return(-1);
}

int dvenc_frame(char *_dvenc_vbuf, char *_dvenc_abuf, int aud_bytes, char *_dvenc_dvbuf)
{
    return(-1);
}

int dvenc_close() 
{
    return(-1);
}

#endif
