/*
 *  decode_lzo.c
 *
 *  Copyright (C) Tilmann Bitterberg - 2003
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
#include "ioaux.h"
#include "avilib.h"
#include "tc.h"

#ifdef HAVE_LZO

#include <lzo1x.h>
#if (LZO_VERSION > 0x1070)
#  include <lzoutil.h>
#endif

#include "export/tc_lzo.h"

#define MOD_NAME    "decode_lzo"

#define BUFFER_SIZE SIZE_RGB_FRAME<<1

static int r;
static lzo_byte *out;
static lzo_byte *inbuf;
static lzo_byte *wrkmem;
static lzo_uint out_len;


inline static void str2long(unsigned char *bb, long *bytes)
{
    *bytes = (bb[0]<<24) | (bb[1]<<16) | (bb[2]<<8) | (bb[3]);
}

void decode_lzo(decode_t *decode)
{
    long bytes;
    ssize_t ss;
    tc_lzo_header_t h;

    /*
     * Step 1: initialize the LZO library
     */
    
    if (lzo_init() != LZO_E_OK) {
      fprintf(stderr, "[%s] lzo_init() failed\n", MOD_NAME);
      goto decoder_error;
    }

    wrkmem = (lzo_bytep) lzo_malloc(LZO1X_1_MEM_COMPRESS);
    out = (lzo_bytep) lzo_malloc(BUFFER_SIZE);
    inbuf = (lzo_bytep) lzo_malloc(BUFFER_SIZE);
    
    if (wrkmem == NULL || out == NULL) {
      fprintf(stderr, "[%s] out of memory\n", MOD_NAME);
      goto decoder_error;
    }

    r = 0;
    out_len = 0;
    verbose = decode->verbose;

    for (;;) {
	if ( (ss=p_read (decode->fd_in, (char *)&h, sizeof(h))) != sizeof(h)) {
	    //fprintf(stderr," (%s) failed to read frame size: EOF. expected (%ld) got (%d)\n", __FILE__, 4L, ss);
	    goto decoder_out;
	}

	// check magic
	if (h.magic != TC_CODEC_LZO2) {
	    fprintf(stderr," (%s) Wrong stream magic: expected (0x%x) got (0x%x)\n", __FILE__, TC_CODEC_LZO2, h.magic);
	    goto decoder_error;
	}

	//str2long(bb, &bytes);
	bytes = h.size;

	if (verbose & TC_DEBUG) 
	    fprintf (stderr, "got bytes (%ld)\n", bytes); 
	if ( (ss=p_read (decode->fd_in, inbuf, bytes))!=bytes) {
	    fprintf(stderr," (%s) failed to read frame: expected (%ld) got (%lu)\n", __FILE__, bytes, (unsigned long)ss);
	    goto decoder_error;
	}


	r = lzo1x_decompress(inbuf, bytes, out, &out_len, wrkmem);

	if (r == LZO_E_OK) {
	    if(verbose & TC_DEBUG) 
		fprintf(stderr, "decompressed %lu bytes into %lu bytes\n",
				    (long) bytes, (long) out_len);
	} else {
      
	    /* this should NEVER happen */
	    fprintf(stderr, "[%s] internal error - decompression failed: %d\n", MOD_NAME, r);
	    goto decoder_error;
	}

	if ( (ss = p_write (decode->fd_out, out, out_len)) != out_len) {
	    fprintf(stderr," (%s) failed to write frame: expected (%ld) wrote (%lu)\n", __FILE__, bytes, (unsigned long)ss);
	    goto decoder_error;
	}
    }

decoder_out:
    import_exit(0);

decoder_error:
    import_exit(1);
    
}












#else /* HAVE_LZO */

void decode_lzo(decode_t *decode)
{
    fprintf(stderr, "No support for LZO configured -- exiting\n");
    import_exit(1);
}


#endif /* HAVE_LZO */
