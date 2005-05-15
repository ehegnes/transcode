/*
 *  decode_ac3.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *  Copyright (C) Aaron Holtzman - May 1999
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

#include "transcode.h"

#include <sys/errno.h>

#include "ac3.h"
#include "ioaux.h"

#define CHUNK_SIZE 2047
uint8_t buf[CHUNK_SIZE];

#define WRITE_BYTES 1024*6

static FILE *in_file;
static FILE *out_file;

static int verbose, banner=0;

static void fill_buffer(uint8_t **start, uint8_t **end)
{
  uint32_t bytes_read;
  
  *start = buf;

  bytes_read = fread(*start, 1, CHUNK_SIZE, in_file);
  
  if(bytes_read < CHUNK_SIZE) {
    if(verbose & TC_DEBUG) fprintf (stderr, "(%s@%d) read error (%d/%d)\n", __FILE__, __LINE__, bytes_read, CHUNK_SIZE);
    import_exit(1);
  }
  
  *end= *start + bytes_read;
}


/* ------------------------------------------------------------ 
 *
 * decoder thread
 *
 * ------------------------------------------------------------*/


void decode_ac3(decode_t *decode)
{
    ac3_frame_t *ac3_frame;
    ac3_config_t ac3_config;

    verbose = decode->verbose;
    
    if(verbose & TC_DEBUG) banner=1;

    in_file = fdopen(decode->fd_in, "r");
    out_file = fdopen(decode->fd_out, "w");
 
    ac3_config.fill_buffer_callback = fill_buffer;
    ac3_config.num_output_ch = 2;
    ac3_config.flags = 0;
    ac3_config.flags = 0;
    ac3_config.ac3_gain[0] = decode->ac3_gain[0];
    ac3_config.ac3_gain[1] = decode->ac3_gain[1];
    ac3_config.ac3_gain[2] = decode->ac3_gain[2];
#ifdef HAVE_MMX
    ac3_config.flags |= AC3_MMX_ENABLE;
#endif
#ifdef HAVE_ASM_3DNOW
    ac3_config.flags |= AC3_3DNOW_ENABLE;
#endif

    ac3_init(&ac3_config);
    
    while (!feof(in_file)) {
        ac3_frame = ac3_decode_frame(banner);
        if(!ac3_frame) {
            void *tmp;
            tmp = malloc(WRITE_BYTES);
            memset(tmp, 0, WRITE_BYTES);
            if(fwrite( tmp, WRITE_BYTES, 1, out_file)!=1) {
                fprintf(stderr, "(%s) write failed\n", __FILE__);
	        break;
	    }
            free(tmp);
        }
	else if(fwrite( ac3_frame->audio_data, WRITE_BYTES, 1, out_file)!=1) {
	    fprintf(stderr, "(%s) write failed\n", __FILE__);
	    break;
	}	
    }
    
    import_exit(0);
}
