/* 
 *  enc_input.c
 *
 *     Copyright (C) Peter Schlaile - Feb 2001
 *
 *  This file is part of libdv, a free DV (IEC 61834/SMPTE 314M)
 *  codec.
 *
 *  libdv is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your
 *  option) any later version.
 *   
 *  libdv is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 *  The libdv homepage is http://libdv.sourceforge.net/.  
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include "dvenc.h"

#include <transcode.h>

static dv_enc_audio_info_t *audio_info;

static void (*audio_converter)(unsigned char* in_buf, unsigned char* out_buf,
			       int num_samples) = NULL;

static void convert_s16_le(unsigned char* in_buf, unsigned char* out_buf,
			   int num_samples)
{
	int i;
	for (i = 0; i < num_samples; i++) {
		*out_buf++ = in_buf[1];
		*out_buf++ = in_buf[0];
		in_buf += 2;
	}
}

#if 0  // unused converters
static void convert_u8(unsigned char* in_buf, unsigned char* out_buf,
		       int num_samples)
{
	int i;
	for (i = 0; i < num_samples; i++) {
		int val = *in_buf++ - 128;
		*out_buf++ = val >> 8;
		*out_buf++ = val & 0xff;
	}
}

static void convert_s16_be(unsigned char* in_buf, unsigned char* out_buf,
			   int num_samples)
{
	tc_memcpy(out_buf, in_buf, 2*num_samples);
}

static void convert_u16_le(unsigned char* in_buf, unsigned char* out_buf,
			   int num_samples)
{
	int i;
	for (i = 0; i < num_samples; i++) {
		int val = (in_buf[0] + (in_buf[1] << 8)) - 32768;
		*out_buf++ = val >> 8;
		*out_buf++ = val & 0xff;
		in_buf += 2;
	}
}

static void convert_u16_be(unsigned char* in_buf, unsigned char* out_buf,
			   int num_samples)
{
	int i;
	for (i = 0; i < num_samples; i++) {
		int val = (in_buf[1] + (in_buf[0] << 8)) - 32768;
		*out_buf++ = val >> 8;
		*out_buf++ = val & 0xff;
		in_buf += 2;
	}
}
#endif  // unused converters

int pcm_init(const char* filename, dv_enc_audio_info_t *_audio_info)
{

  audio_converter = convert_s16_le;
  
  _audio_info->channels=audio_info->channels;
  _audio_info->frequency=audio_info->frequency;
  _audio_info->bitspersample=audio_info->bitspersample;
  _audio_info->bytesperframe=audio_info->bytesperframe;
  _audio_info->bytealignment=audio_info->bytealignment;
  _audio_info->bytespersecond=audio_info->bytespersecond;

  return(0);
}

void pcm_finish()
{
}

int pcm_load(dv_enc_audio_info_t * audio_info, int isPAL)
{
	unsigned char data[DV_AUDIO_MAX_SAMPLES * 2 * 2];

	tc_memcpy(data, dvenc_abuf, audio_info->bytesperframe); 
	audio_converter(data, audio_info->data, 
			audio_info->bytesperframe / 2);
	return(0);
}

void dvenc_init_audio_input(dv_enc_audio_input_filter_t *filter, dv_enc_audio_info_t *_audio_info)
{

  audio_info = _audio_info;
  
  filter->init   = pcm_init;
  filter->finish = pcm_finish;
  filter->load   = pcm_load;
  filter->filter_name  = "tc_pcm";
}
