/*
 *  decode_ogg.c
 *
 *  Copyright (C) Tilmann Bitterberg
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <errno.h>
#include <unistd.h>

#include "transcode.h"
#include "ioaux.h"

#include <vorbis/vorbisfile.h>

static int quiet = 0;
static int bits = 16;
static int endian = 0;
static int sign = 1;

/* ------------------------------------------------------------ 
 *
 * decoder thread
 *
 * ------------------------------------------------------------*/

int decode_ogg_file(int fdin, int fdout)
{
    FILE *in, *out=NULL;
    OggVorbis_File vf;
    int bs = 0;
    char buf[8192];
    int buflen = 8192;
    unsigned int written = 0;
    int ret;
    ogg_int64_t length = 0;
    ogg_int64_t done = 0;
    int size = 0;
    int seekable = 0;
    int percent = 0;

    in  = fdopen (fdin,  "rb");
    out = fdopen (fdout, "wb");

    if(ov_open(in, &vf, NULL, 0) < 0) {
        fprintf(stderr, "ERROR: Failed to open input as vorbis\n");
        fclose(in);
        fclose(out);
        return 1;
    }

    if(ov_seekable(&vf)) {
        seekable = 1;
        length = ov_pcm_total(&vf, 0);
        size = bits/8 * ov_info(&vf, 0)->channels;
    }

    while((ret = ov_read(&vf, buf, buflen, endian, bits/8, sign, &bs)) != 0) {
        if(bs != 0) {
            fprintf(stderr, "Only one logical bitstream currently supported\n");
            break;
        }

        if(ret < 0 && !quiet) {
            fprintf(stderr, "Warning: hole in data\n");
            continue;
        }

        if(fwrite(buf, 1, ret, out) != ret) {
            fprintf(stderr, "Error writing to file: %s\n", strerror(errno));
            ov_clear(&vf);
            fclose(out);
            return 1;
        }

        written += ret;
        if(!quiet && seekable) {
            done += ret/size;
            if((double)done/(double)length * 200. > (double)percent) {
                percent = (double)done/(double)length *200;
                fprintf(stderr, "\r\t[%5.1f%%]", (double)percent/2.);
            }
        }
    }

    ov_clear(&vf);

    fclose(out);
    return 0;
}


void decode_ogg(info_t *ipipe)
{
  
#if (HAVE_OGG && HAVE_VORBIS) 
  
  quiet = ipipe->verbose;
  //bits = ipipe->a_bits;
  decode_ogg_file(ipipe->fd_in, ipipe->fd_out);

  import_exit(0);

#else
  
  fprintf(stderr, "(%s) no support for VORBIS decoding configured - exit.\n", __FILE__);
  import_exit(1);

#endif
  
}

  
