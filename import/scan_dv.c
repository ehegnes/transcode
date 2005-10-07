/*
 *  scan_dv.c
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
#include "tc.h"

#ifdef HAVE_LIBDV
#include <libdv/dv.h>
#endif

int scan_header_dv(char *buf)
{
    int cc=-1;

#ifdef HAVE_LIBDV
    
    dv_decoder_t *dv_decoder=NULL;

    // Initialize DV decoder

    if((dv_decoder = dv_decoder_new(TRUE, FALSE, FALSE))==NULL) {
	fprintf(stderr, "(%s) dv decoder init failed\n", __FILE__);
	return(-1);
    }

    dv_decoder->prev_frame_decoded = 0;
    cc=dv_parse_header(dv_decoder, buf);

    dv_decoder_free(dv_decoder);

#endif
    return(cc);
}

