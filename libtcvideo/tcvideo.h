/*
 * tcvideo.c - video processing library for transcode
 * Written by Andrew Church <achurch@achurch.org>
 * Based on code written by Thomas Oestreich.
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#ifndef LIBTCVIDEO_TCVIDEO_H
#define LIBTCVIDEO_TCVIDEO_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

/*************************************************************************/

/* Modes for tcv_deinterlace(): */

typedef enum {
    TCV_DEINTERLACE_DROP_FIELD,
    TCV_DEINTERLACE_INTERPOLATE,
    TCV_DEINTERLACE_LINEAR_BLEND,
} TCVDeinterlaceMode;


/* Zoom filter types: */

typedef enum {
    TCV_ZOOM_BOX,
    TCV_ZOOM_TRIANGLE,
    TCV_ZOOM_HERMITE,
    TCV_ZOOM_BELL,
    TCV_ZOOM_B_SPLINE,
    TCV_ZOOM_MITCHELL,
    TCV_ZOOM_LANCZOS3,
} TCVZoomFilter;

/*************************************************************************/

int tcv_clip(uint8_t *src, uint8_t *dest, int width, int height, int Bpp,
             int clip_left, int clip_right, int clip_top, int clip_bottom,
             uint8_t black_pixel);

int tcv_deinterlace(uint8_t *src, uint8_t *dest, int width, int height,
                    int Bpp, TCVDeinterlaceMode mode);

int tcv_resize(uint8_t *src, uint8_t *dest, int width, int height, int Bpp,
               int resize_w, int resize_h, int scale_w, int scale_h);

int tcv_zoom(uint8_t *src, uint8_t *dest, int width, int height, int Bpp,
             int new_w, int new_h, TCVZoomFilter filter);

int tcv_reduce(uint8_t *src, uint8_t *dest, int width, int height, int Bpp,
               int reduce_w, int reduce_h);

int tcv_flip_v(uint8_t *src, uint8_t *dest, int width, int height, int Bpp);

int tcv_flip_h(uint8_t *src, uint8_t *dest, int width, int height, int Bpp);

int tcv_gamma_correct(uint8_t *src, uint8_t *dest, int width, int height,
                      int Bpp, double gamma);

int tcv_antialias(uint8_t *src, uint8_t *dest, int width, int height,
                  int Bpp, double weight, double bias);

/*************************************************************************/

#endif  /* LIBTCVIDEO_TCVIDEO_H */

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
