/*
 *  dv_aux.h
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


#ifndef _DV_AUX_H
#define _DV_AUX_H

#include <libdv/dv.h>
#include <time.h>

#define DV_WIDTH       720
#define DV_PAL_HEIGHT  576
#define DV_NTSC_HEIGHT 480


typedef struct dv_enc_input_filter_s {
  int (*init)(int wrong_interlace, int force_dct);
  void (*finish)();
  int (*load)(const char* filename, int * isPAL);
  int (*skip)(const char* filename, int * isPAL);
  /* fills macroblock, determines dct_mode and
     transposes dv_blocks */
  void (*fill_macroblock)(dv_macroblock_t *mb, int isPAL);
  
  const char* filter_name;
} dv_enc_input_filter_t;

typedef struct dv_enc_audio_info_s {
  /* stored by init (but could be used for on the fly sample
     rate changes) */
  int channels;
  int frequency;
  int bitspersample;
  int bytespersecond;
  int bytealignment;
  /* stored by load */
  int bytesperframe;
  /* big endian 12/16 bit is assumed */
  unsigned char data[1920 * 2 * 2]; /* max 48000.0 Hz PAL */
} dv_enc_audio_info_t;

typedef struct dv_audio_enc_input_filter_s {
  int (*init)(const char* filename, 
	      dv_enc_audio_info_t * audio_info);
  void (*finish)();
  int (*load)(dv_enc_audio_info_t * audio_info, int isPAL);
  
  const char* filter_name;
} dv_enc_audio_input_filter_t;


typedef struct dv_enc_output_filter_s {
  int (*init)();
  void (*finish)();
  int (*store)(unsigned char* encoded_data, 
	       dv_enc_audio_info_t* audio_data,/* may be null */
	       int isPAL, time_t now);
  
  const char* filter_name;
} dv_enc_output_filter_t;

int dv_encode(char *buf, char *dv_frame);
int dv_encode_init();
int dv_encode_close();

extern void dv_enc_register_input_filter(dv_enc_input_filter_t filter);
extern void dv_enc_register_output_filter(dv_enc_output_filter_t filter);
extern void dv_enc_register_audio_input_filter(dv_enc_audio_input_filter_t filter);

extern void write_meta_data(unsigned char* target, int frame, int isPAL,
			    time_t * now);
extern int raw_insert_audio(unsigned char * frame_buf, 
		     dv_enc_audio_info_t * audio, int isPAL);

extern void ppm_copy_y_block_mmx(short * dst, short * src);
extern void ppm_copy_pal_c_block_mmx(short * dst, short * src);
extern void ppm_copy_ntsc_c_block_mmx(short * dst, short * src);
extern void finish_mb_mmx(dv_macroblock_t* mb);

#define	emms()			__asm__ __volatile__ ("emms")

void dv_enc_rgb_to_ycb(unsigned char* img_rgb, int height,
		       short* img_y, short* img_cr, short* img_cb);

int encoder_loop(dv_enc_input_filter_t * input,
		 dv_enc_audio_input_filter_t * audio_input,
		 dv_enc_output_filter_t * output,
		 int start, int end, const char* filename,
		 const char* audio_filename,
		 int vlc_encode_passes, int static_qno, int verbose_mode,
		 int fps);
#endif
