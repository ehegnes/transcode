/*
 *  decode_mpeg2.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *  Copyright (C) 1999-2001 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 *  This file is part of transcode, a video stream  processing tool
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
#include "ioaux.h"
#include "tc.h"

#include <mpeg2dec/mpeg2.h>
#include <mpeg2dec/mpeg2convert.h>

#define BUFFER_SIZE 262144
static uint8_t buffer[BUFFER_SIZE];

/* ------------------------------------------------------------ 
 *
 * decoder thread
 *
 * ------------------------------------------------------------*/

static void
show_accel(uint32_t mp_ac) {
	fprintf (stderr, "[%s] libmpeg2 acceleration: ", __FILE__);
	if (mp_ac & MPEG2_ACCEL_X86_3DNOW)
		fprintf (stderr, "3dnow\n");
	else if (mp_ac & MPEG2_ACCEL_X86_MMXEXT)
		fprintf (stderr, "mmxext\n");
	else if(mp_ac & MPEG2_ACCEL_X86_MMX)
		fprintf (stderr, "mmx\n");
	else
		fprintf (stderr, "none (plain C)\n");
}


void decode_mpeg2(decode_t *decode)
{
    mpeg2dec_t * decoder;
    const mpeg2_info_t * info;
    const mpeg2_sequence_t * sequence;
    mpeg2_state_t state;
    size_t size;
    int framenum = 0, len = 0;
    uint32_t ac;

    fprintf (stderr, "[%s] libmpeg2 0.4.0b loop decoder\n", 
		    __FILE__);
    
    ac = mpeg2_accel (MPEG2_ACCEL_DETECT);
    
    show_accel (ac);
    
    decoder = mpeg2_init ();
    if (decoder == NULL) {
	fprintf (stderr, "Could not allocate a decoder object.\n");
	import_exit(1);
    }
    info = mpeg2_info (decoder);

    size = (size_t)-1;
    do {
	state = mpeg2_parse (decoder);
	sequence = info->sequence;
	switch (state) {
	case STATE_BUFFER:
	    size = tc_pread (decode->fd_in, buffer, BUFFER_SIZE);
	    mpeg2_buffer (decoder, buffer, buffer + size);
	    break;
	case STATE_SEQUENCE:
    	    if(decode->format == TC_CODEC_RGB) {
	        mpeg2_convert (decoder, mpeg2convert_rgb24, NULL);
	    }	    
	    break;
	case STATE_SLICE:
	case STATE_END:
	case STATE_INVALID_END:
	    if (info->display_fbuf) {
		if (decode->verbose >= 4) {
			fprintf (stderr, "[%s] decoded frame #%09i\r", 
					__FILE__, framenum++);
		}
		len = sequence->width * sequence->height;
		if(len != tc_pwrite (decode->fd_out, info->display_fbuf->buf[0], len)) {
			fprintf (stderr, "failed to write Y plane of frame");
			import_exit (1);
		}
		len = sequence->chroma_width * sequence->chroma_height;
		if (len != tc_pwrite (decode->fd_out, info->display_fbuf->buf[1], len)) {
			fprintf (stderr, "failed to write U plane of frame");
			import_exit (1);
		}
		if (len != tc_pwrite (decode->fd_out, info->display_fbuf->buf[2], len)) {
			fprintf (stderr, "failed to write V plane of frame");
			import_exit (1);
		}
	    }
	    break;
	default:
	    break;
	}
    } while (size);

    mpeg2_close (decoder);
    import_exit(0);
}
