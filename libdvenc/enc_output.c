/* 
 *  enc_output_filters
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
#include <string.h>

#include "transcode.h"

#include "dvenc.h"

static int frame_counter = 0;

static int raw_init()
{
    return 0;
}

static void raw_finish()
{
}

extern int raw_insert_audio(unsigned char * frame_buf, 
			    dv_enc_audio_info_t * audio, int isPAL);

static int raw_store(unsigned char* encoded_data, 
		     dv_enc_audio_info_t* audio_data, 
		     int isPAL, time_t now)
{
  write_meta_data(encoded_data, frame_counter, isPAL, &now);

  if (audio_data) {
    int rval = raw_insert_audio(encoded_data, audio_data, isPAL);
    if (rval) {
      return rval;
    }
  }

  tc_memcpy(dvenc_dvbuf, encoded_data, (isPAL) ? TC_FRAME_DV_PAL:TC_FRAME_DV_NTSC);
  
  frame_counter++;
  return(0);
}

void dvenc_init_output(dv_enc_output_filter_t *filter)
{
  filter->init   = raw_init;
  filter->finish = raw_finish;
  filter->store  = raw_store;
}


