/*
 *  dvenc.h
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


#ifndef _DVENC_H
#define _DVENC_H

#include "config.h"

#ifdef HAVE_DV
#include <libdv/dv.h>
#include <libdv/dv_types.h>

#include "enc_input.h"
#include "enc_output.h"
#include "enc_audio_input.h"

#include "transcode.h"

#endif

#define DV_PAL_HEIGHT  576
#define DV_NTSC_HEIGHT 480
#define DV_WIDTH       720

int dvenc_frame(char *dvenc_vbuf, char *dvenc_abuf, int aud_bytes, char *dvenc_dvbuf);
int dvenc_set_parameter(int codec, int format, int sample_rate);
int dvenc_init();
int dvenc_close();

extern unsigned char *dvenc_dvbuf, *dvenc_abuf, *dvenc_vbuf;

#ifdef HAVE_DV
void dvenc_init_input(dv_enc_input_filter_t *filter, int mode, int format);
void dvenc_init_output(dv_enc_output_filter_t *filter);
void dvenc_init_audio_input(dv_enc_audio_input_filter_t *filter, dv_enc_audio_info_t *audio_info);

extern int encoder_loop();

extern int write_meta_data(unsigned char* encoded_data, int frame_counter, 
			   int isPAL, time_t *now);
#endif

#ifdef HAVE_X86CPU
#define	emms()			__asm__ __volatile__ ("emms")
#endif

#endif
