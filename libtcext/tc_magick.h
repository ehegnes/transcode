/*
 *  tc_magick.h -- transcode GraphicsMagick utilities.
 *  (C) 2009 Francesco Romani <fromani at gmail dot com>
 *
 * This file is part of transcode, a video stream processing tool.
 *
 * transcode is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * transcode is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TC_MAGICK_H
#define TC_MAGICK_H

#include "libtc/tcframes.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

#include <magick/api.h>

typedef struct tcmagickcontext_ TCMagicContext;
struct tcmagickcontext_ {
    ExceptionInfo exception_info;
    Image         *image;
    ImageInfo     *image_info;
    PixelPacket   *pixel_packet;
};

int tc_magick_init(TCMagickContext *ctx, const char *format);
int tc_magick_fini(TCMagickContext *ctx);

int tc_magick_log_error(TCMagickContext *ctx);

/* Can't find a good name, so let's mimic theora */
int tc_magick_encode_filein(TCMagickContext *ctx, const char *filename);
int tc_magick_encode_RGBin(TCMagickContext *ctx,
                           int width, int height, const uint8_t *data);
int tc_magick_encode_frameout(TCMagickContext *ctx, TCFrameVideo *frame);


#endif /* TC_MAGICK_H */

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

