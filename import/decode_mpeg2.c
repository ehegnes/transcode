/*
 *  decode_mpeg2.c
 *
 *  Copyright (C) Thomas �streich - June 2001
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <mpeg2dec/mpeg2.h>
#include <mpeg2dec/mpeg2convert.h>

#include "ioaux.h"

#define BUFFER_SIZE 262144
static uint8_t buffer[BUFFER_SIZE];

/* ------------------------------------------------------------
 * helper functions
 * ------------------------------------------------------------*/

typedef void (*WriteDataFn)(decode_t *decode, const mpeg2_info_t *info,
                            const mpeg2_sequence_t *sequence);

static void show_accel(uint32_t mp_ac)
{
    fprintf(stderr, "[%s] libmpeg2 acceleration: %s",
                __FILE__,
                (mp_ac & MPEG2_ACCEL_X86_3DNOW)  ? "3dnow" :
                (mp_ac & MPEG2_ACCEL_X86_MMXEXT) ? "mmxext" :
                (mp_ac & MPEG2_ACCEL_X86_MMX)    ? "mmx" :
                                                   "none (plain C)");
}

#define WRITE_DATA(PBUF, LEN, TAG) do { \
    int ret = p_write(decode->fd_out, PBUF, LEN); \
    if(LEN != ret) { \
        fprintf(stderr, "[%s] failed to write %s data" \
                         " of frame (len=%i)", \
                         __FILE__, TAG, ret); \
        import_exit(1); \
    } \
} while (0)

#define WRITE_YUV_PLANE(ID, LEN) do { \
    static const char *plane_id[] = { "Y", "U", "V" }; \
    WRITE_DATA(info->display_fbuf->buf[ID], LEN, plane_id[ID]); \
} while (0)


static void write_rgb24(decode_t *decode, const mpeg2_info_t *info,
                        const mpeg2_sequence_t *sequence)
{
    int len = 0;
    /* FIXME: move to libtc/tcframes routines? */

    len = 3 * info->sequence->width * info->sequence->height;
    WRITE_DATA(info->display_fbuf->buf[0], len, "RGB"); 
}

static void write_yuv420p(decode_t *decode, const mpeg2_info_t *info,
                          const mpeg2_sequence_t *sequence)
{
    int len = 0;
    /* FIXME: move to libtc/tcframes routines? */

    len = sequence->width * sequence->height;
    WRITE_YUV_PLANE(0, len);
                
    len = sequence->chroma_width * sequence->chroma_height;
    WRITE_YUV_PLANE(1, len);
    WRITE_YUV_PLANE(2, len);
}


/* ------------------------------------------------------------
 * decoder entry point
 * ------------------------------------------------------------*/

void decode_mpeg2(decode_t *decode)
{
    int framenum = 0;
    mpeg2dec_t *decoder = NULL;
    const mpeg2_info_t *info = NULL;
    const mpeg2_sequence_t *sequence = NULL;
    mpeg2_state_t state;
    size_t size;
    uint32_t ac = 0;

    WriteDataFn writer = write_yuv420p;
    if (decode->format == TC_CODEC_RGB) {
        fprintf(stderr, "[%s] using libmpeg2convert"
                        " RGB24 conversion", __FILE__);
        writer = write_rgb24;
    }

    ac = mpeg2_accel(MPEG2_ACCEL_DETECT);
    show_accel(ac);

    decoder = mpeg2_init();
    if (decoder == NULL) {
        fprintf(stderr, "[%s] Could not allocate a decoder object.",
                __FILE__);
        import_exit(1);
    }
    info = mpeg2_info(decoder);

    size = (size_t)-1;
    do {
        state = mpeg2_parse(decoder);
        sequence = info->sequence;
        switch (state) {
          case STATE_BUFFER:
            size = p_read(decode->fd_in, buffer, BUFFER_SIZE);
            mpeg2_buffer(decoder, buffer, buffer + size);
            break;
          case STATE_SEQUENCE:
            if (decode->format == TC_CODEC_RGB) {
                mpeg2_convert(decoder, mpeg2convert_rgb24, NULL);
            }
            break;
          case STATE_SLICE:
          case STATE_END:
          case STATE_INVALID_END:
            if (info->display_fbuf) {
                if (decode->verbose >= 4) {
                    fprintf(stderr, "[%s] decoded frame #%09i\r",
                                    __FILE__, framenum);
                    framenum++;
                }
                writer(decode, info, sequence);
            }
            break;
          default:
            /* can't happen */
            break;
        }
    } while (size);

    mpeg2_close(decoder);
    import_exit(0);
}

/*************************************************************************/

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */

