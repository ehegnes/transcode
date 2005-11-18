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

#include "tcvideo.h"
#include "zoom.h"

#define zoom zoom_  // temp to avoid name conflict
#include "src/transcode.h"
#undef zoom
#include <math.h>

/*************************************************************************/

#ifndef PI
# define PI 3.14159265358979323846264338327950
#endif

/* Data for generating a resized pixel. */
struct resize_table_elem {
    int source;
    uint32_t weight1, weight2;
};

/* Antialiasing threshold for determining whether two pixels are the same
 * color. */
#define AA_DIFFERENT    25

/*************************************************************************/

/* Lookup tables for fast resizing, gamma correction, and antialiasing. */
/* FIXME: These are not thread-safe!  To handle threads properly, we should
 *        probably have a function to give a context handle to the caller,
 *        in which we store these lookup tables.  This would also prevent
 *        multiple callers from interfering with each others' tables and
 *        degrading performance.  Maybe we could even add in things like
 *        video data format to reduce the number of parameters passed to
 *        these functions...
 */

static struct resize_table_elem resize_table_x[TC_MAX_V_FRAME_WIDTH/8];
static struct resize_table_elem resize_table_y[TC_MAX_V_FRAME_HEIGHT/8];

static uint8_t gamma_table[256];

static uint32_t aa_table_c[256];
static uint32_t aa_table_x[256];
static uint32_t aa_table_y[256];
static uint32_t aa_table_d[256];

/*************************************************************************/

/* Internal-use functions (defined at the bottom of the file). */

static void init_resize_tables(int oldw, int neww, int oldh, int newh);
static void init_one_resize_table(struct resize_table_elem *table,
                                  int oldsize, int newsize);
static void init_gamma_table(double gamma);
static void init_aa_table(double aa_weight, double aa_bias);

/*************************************************************************/
/*************************************************************************/

/* External interface functions. */

/*************************************************************************/

/**
 * tcv_clip:  Clip the given image by removing the specified number of
 * pixels from each edge.  If a clip value is negative, instead expands the
 * frame by inserting the given number of black pixels (the value to be
 * inserted is given by the `black_pixel' parameter).  Conceptually,
 * expansion is done before clipping, so that if, for example,
 *     width == 640
 *     clip_left == 642
 *     clip_right == -4
 * then the result is a two-pixel-wide black frame (this is not considered
 * an error).
 *
 * Parameters:         src: Source data plane.
 *                    dest: Destination data plane.
 *                   width: Width of frame.
 *                  height: Height of frame.
 *                     Bpp: Bytes (not bits!) per pixel.
 *               clip_left: Number of pixels to clip from left edge.
 *              clip_right: Number of pixels to clip from right edge.
 *                clip_top: Number of pixels to clip from top edge.
 *             clip_bottom: Number of pixels to clip from bottom edge.
 *             black_pixel: Value to be filled into expanded areas.
 * Return value: Nonzero on success, zero on error (invalid parameters).
 * Preconditions: src != NULL: src[0]..src[width*height*Bpp-1] are readable
 *                dest != NULL:
 *                    destw = width - clip_left - clip_right;
 *                    desth = height - clip_top - clip_bottom;
 *                    dest[0]..dest[destw*desth*Bpp-1] are writable
 *                src != dest: src and dest do not overlap
 * Postconditions: (on success) dest[0]..dest[destw*desth*Bpp-1] are set
 */

int tcv_clip(uint8_t *src, uint8_t *dest, int width, int height, int Bpp,
             int clip_left, int clip_right, int clip_top, int clip_bottom,
             uint8_t black_pixel)
{
    int new_w, copy_w, copy_h, y;


    if (!src || !dest || width <= 0 || height <= 0 || (Bpp != 1 && Bpp != 3)) {
        tc_log_error("libtcvideo", "tcv_clip: invalid frame parameters!");
        return 0;
    }
    if (clip_left + clip_right >= width || clip_top + clip_bottom >= height) {
        tc_log_error("libtcvideo", "tcv_clip: clipping parameters"
                     " (%d,%d,%d,%d) invalid for frame size %dx%d",
                     clip_top, clip_left, clip_bottom, clip_right,
                     width, height);
        return 0;
    }
    /* Normalize clipping values (e.g. clip_left > width, clip_right < 0) */
    if (clip_left > width) {
        clip_right += clip_left - width;
        clip_left = width;
    }
    if (clip_right > width) {
        clip_left += clip_right - width;
        clip_right = width;
    }
    if (clip_top > height) {
        clip_bottom += clip_top - height;
        clip_top = height;
    }
    if (clip_bottom > height) {
        clip_top += clip_bottom - height;
        clip_bottom = height;
    }

    new_w = width - clip_left - clip_right;
    copy_w = width - (clip_left<0 ? 0 : clip_left)
                   - (clip_right<0 ? 0 : clip_right);
    copy_h = height - (clip_top<0 ? 0 : clip_top)
                    - (clip_bottom<0 ? 0 : clip_bottom);

    if (clip_top < 0) {
        memset(dest, black_pixel, (-clip_top) * new_w * Bpp);
        dest += (-clip_top) * new_w * Bpp;
    } else {
        src += clip_top * width * Bpp;
    }
    if (clip_left > 0)
        src += clip_left * Bpp;
    for (y = 0; y < copy_h; y++) {
        if (clip_left < 0) {
            memset(dest, black_pixel, (-clip_left) * Bpp);
            dest += (-clip_left) * Bpp;
        }
        if (copy_w > 0)
            ac_memcpy(dest, src, copy_w * Bpp);
        dest += copy_w * Bpp;
        src += width * Bpp;
        if (clip_right < 0) {
            memset(dest, black_pixel, (-clip_right) * Bpp);
            dest += (-clip_right) * Bpp;
        }
    }
    if (clip_bottom < 0) {
        memset(dest, black_pixel, (-clip_bottom) * new_w * Bpp);
    }
    return 1;
}

/*************************************************************************/

/**
 * tcv_deinterlace:  Deinterlace the given image.
 *
 * Parameters:    src: Source data plane.
 *               dest: Destination data plane.
 *              width: Width of frame.
 *             height: Height of frame.
 *                Bpp: Bytes (not bits!) per pixel.
 *               mode: Deinterlacing mode (TCV_DEINTERLACE_* constant).
 * Return value: Nonzero on success, zero on error (invalid parameters).
 * Preconditions: src != NULL: src[0]..src[width*height*Bpp-1] are readable
 *                dest != NULL:
 *                    mode == TCV_DEINTERLACE_DROP_FIELD:
 *                        dest[0]..dest[width*(height/2)*Bpp-1] are writable
 *                    mode != TCV_DEINTERLACE_DROP_FIELD:
 *                        dest[0]..dest[width*height*Bpp-1] are writable
 *                src != dest: src and dest do not overlap
 * Postconditions: (on success)
 *                     mode == TCV_DEINTERLACE_DROP_FIELD:
 *                         dest[0]..dest[width*(height/2)*Bpp-1] are set
 *                     mode != TCV_DEINTERLACE_DROP_FIELD:
 *                         dest[0]..dest[width*height*Bpp-1] are set
 */

static int deint_drop_field(uint8_t *src, uint8_t *dest, int width,
                            int height, int Bpp);
static int deint_interpolate(uint8_t *src, uint8_t *dest, int width,
                             int height, int Bpp);
static int deint_linear_blend(uint8_t *src, uint8_t *dest, int width,
                              int height, int Bpp);

int tcv_deinterlace(uint8_t *src, uint8_t *dest, int width, int height,
                    int Bpp, TCVDeinterlaceMode mode)
{
    if (!src || !dest || width <= 0 || height <= 0 || (Bpp != 1 && Bpp != 3)) {
        tc_log_error("libtcvideo", "tcv_deinterlace: invalid frame parameters!");
        return 0;
    }
    switch (mode) {
      case TCV_DEINTERLACE_DROP_FIELD:
        return deint_drop_field(src, dest, width, height, Bpp);
      case TCV_DEINTERLACE_INTERPOLATE:
        return deint_interpolate(src, dest, width, height, Bpp);
      case TCV_DEINTERLACE_LINEAR_BLEND:
        return deint_linear_blend(src, dest, width, height, Bpp);
      default:
        tc_log_error("libtcvideo", "tcv_deinterlace: invalid mode %d!", mode);
        return 0;
    }
}

/**
 * deint_drop_field, deint_interpolate, deint_linear_blend:  Helper
 * functions for tcv_deinterlace() that implement the individual
 * deinterlacing methods.
 *
 * Parameters: As for tcv_deinterlace().
 * Return value: As for tcv_deinterlace().
 * Side effects: (for deint_linear_blend())
 *                   src[0..width*height-1] are destroyed.
 * Preconditions: As for tcv_deinterlace(), plus:
 *                src != NULL
 *                dest != NULL
 *                width > 0
 *                height > 0
 *                Bpp == 1 || Bpp == 3
 *                (for deint_linear_blend())
 *                    src[0..width*height-1] are writable
 * Postconditions: As for tcv_deinterlace().
 */

static int deint_drop_field(uint8_t *src, uint8_t *dest, int width,
                            int height, int Bpp)
{
    int Bpl = width * Bpp;
    int y;

    for (y = 0; y < height/2; y++)
        ac_memcpy(dest + y*Bpl, src + (y*2)*Bpl, Bpl);
    return 1;
}


static int deint_interpolate(uint8_t *src, uint8_t *dest, int width,
                             int height, int Bpp)
{
    int Bpl = width * Bpp;
    int y;

    for (y = 0; y < height; y++) {
        if (y%2 == 0) {
            ac_memcpy(dest + y*Bpl, src + y*Bpl, Bpl);
        } else if (y == height-1) {
            /* if the last line is odd, copy from the previous line */
            ac_memcpy(dest + y*Bpl, src + (y-1)*Bpl, Bpl);
        } else {
            ac_average(src + (y-1)*Bpl, src + (y+1)*Bpl, dest + y*Bpl, Bpl);
        }
    }
    return 1;
}


static int deint_linear_blend(uint8_t *src, uint8_t *dest, int width,
                              int height, int Bpp)
{
    int Bpl = width * Bpp;
    int y;

    /* First interpolate odd lines into the target buffer */
    deint_interpolate(src, dest, width, height, Bpp);

    /* Now interpolate even lines in the source buffer; we don't use it
     * after this so it's okay to destroy it */
    ac_memcpy(src, src+Bpl, Bpl);
    for (y = 2; y < height-1; y += 2)
        ac_average(src + (y-1)*Bpl, src + (y+1)*Bpl, src + y*Bpl, Bpl);
    if (y < height)
        ac_memcpy(src + y*Bpl, src + (y-1)*Bpl, Bpl);

    /* Finally average the two frames together */
    ac_average(src, dest, dest, height*Bpl);

    return 1;
}

/*************************************************************************/

/**
 * tcv_resize:  Resize the given image using a lookup table.  `scale_w' and
 * `scale_h' are the number of blocks the image is divided into (normally
 * 8; 4 for subsampled U/V).  `resize_w' and `resize_h' are given in units
 * of `scale_w' and `scale_h' respectively.  Only one of `resize_w' and
 * `resize_h' may be nonzero.
 * N.B. doesn't work well if shrinking by more than a factor of 2 (only
 *      averages 2 adjacent lines/pixels)
 *
 * Parameters:      src: Source data plane.
 *                 dest: Destination data plane.
 *                width: Width of frame.
 *               height: Height of frame.
 *                  Bpp: Bytes (not bits!) per pixel.
 *             resize_w: Amount to add to width, in units of `scale_w'.
 *             resize_h: Amount to add to width, in units of `scale_h'.
 *              scale_w: Size in pixels of a `resize_w' unit.
 *              scale_h: Size in pixels of a `resize_h' unit.
 * Return value: Nonzero on success, zero on error (invalid parameters).
 * Preconditions: src != NULL: src[0]..src[width*height*Bpp-1] are readable
 *                dest != NULL:
 *                    destw = width + resize_w*scale_w;
 *                    desth = height + resize_h*scale_h;
 *                    dest[0]..dest[destw*desth*Bpp-1] are writable
 *                src != dest: src and dest do not overlap
 * Postconditions: (on success) dest[0]..dest[destw*desth*Bpp-1] are set
 */

static inline void rescale_pixel(const uint8_t *src1, const uint8_t *src2,
                                 uint8_t *dest, int bytes,
                                 uint32_t weight1, uint32_t weight2);

int tcv_resize(uint8_t *src, uint8_t *dest, int width, int height, int Bpp,
               int resize_w, int resize_h, int scale_w, int scale_h)
{
    int new_w, new_h;


    if (!src || !dest || width <= 0 || height <= 0 || (Bpp != 1 && Bpp != 3)) {
        tc_log_error("libtcvideo", "tcv_resize: invalid frame parameters!");
        return 0;
    }
    if ((scale_w != 1 && scale_w != 2 && scale_w != 4 && scale_w != 8)
     || (scale_h != 1 && scale_h != 2 && scale_h != 4 && scale_h != 8)) {
        tc_log_error("libtcvideo", "tcv_resize: invalid scale parameters!");
        return 0;
    }
    if (width % scale_w != 0 || height % scale_h != 0) {
        tc_log_error("libtcvideo", "tcv_resize: scale parameters (%d,%d)"
                     "invalid for frame size %dx%d",
                     scale_w, scale_h, width, height);
        return 0;
    }

    new_w = width + resize_w*scale_w;
    new_h = height + resize_h*scale_h;
    if (new_w <= 0 || new_h <= 0) {
        tc_log_error("libtcvideo", "tcv_resize: resizing parameters"
                     " (%d,%d,%d,%d) invalid for frame size %dx%d",
                     resize_w, resize_h, scale_w, scale_h, width, height);
        return 0;
    }

    /* Resize vertically (fast, using accelerated routine) */
    if (resize_h) {
        int Bpl = width * Bpp;  /* bytes per line */
        int i, y;

        init_resize_tables(0, 0, height*8/scale_h, new_h*8/scale_h);
        for (i = 0; i < scale_h; i++) {
            uint8_t *sptr = src  + (i * (height/scale_h)) * Bpl;
            uint8_t *dptr = dest + (i * (new_h /scale_h)) * Bpl;
            for (y = 0; y < new_h / scale_h; y++) {
                ac_rescale(sptr + (resize_table_y[y].source  ) * Bpl,
                           sptr + (resize_table_y[y].source+1) * Bpl,
                           dptr + y*Bpl, Bpl,
                           resize_table_y[y].weight1,
                           resize_table_y[y].weight2);
            }
        }
    }

    /* Resize horizontally; calling the accelerated routine for each pixel
     * has far too much overhead, so we just perform the calculations
     * directly. */
    if (resize_w) {
        int i, x;

        init_resize_tables(width*8/scale_w, new_w*8/scale_w, 0, 0);
        /* Treat the image as an array of blocks */
        for (i = 0; i < new_h * scale_w; i++) {
            /* This `if' is an optimization hint to the compiler, to
             * suggest that it generate a separate version of the loop
             * code for Bpp==1 without the unnecessary multiply ops. */
            if (Bpp == 1) {  /* optimization hint */
                uint8_t *sptr = src  + (i * (width/scale_w)) * Bpp;
                uint8_t *dptr = dest + (i * (new_w/scale_w)) * Bpp;
                for (x = 0; x < new_w / scale_w; x++) {
                    rescale_pixel(sptr + (resize_table_x[x].source  ) * Bpp,
                                  sptr + (resize_table_x[x].source+1) * Bpp,
                                  dptr + x*Bpp, Bpp,
                                  resize_table_x[x].weight1,
                                  resize_table_x[x].weight2);
                }
            } else {  /* exactly the same thing */
                uint8_t *sptr = src  + (i * (width/scale_w)) * Bpp;
                uint8_t *dptr = dest + (i * (new_w/scale_w)) * Bpp;
                for (x = 0; x < new_w / scale_w; x++) {
                    rescale_pixel(sptr + (resize_table_x[x].source  ) * Bpp,
                                  sptr + (resize_table_x[x].source+1) * Bpp,
                                  dptr + x*Bpp, Bpp,
                                  resize_table_x[x].weight1,
                                  resize_table_x[x].weight2);
                }
            }
        }
    }

    return 1;
}

static inline void rescale_pixel(const uint8_t *src1, const uint8_t *src2,
                                 uint8_t *dest, int bytes,
                                 uint32_t weight1, uint32_t weight2)
{
    int byte;
    for (byte = 0; byte < bytes; byte++) {
        /* Watch out for trying to access beyond the end of the frame on
         * the last pixel */
        if (weight1 < 0x10000)  /* this is the more likely case */
            dest[byte] = (src1[byte]*weight1 + src2[byte]*weight2 + 32768)
                         >> 16;
        else
            dest[byte] = src1[byte];
    }
}

/*************************************************************************/

/**
 * tcv_zoom:  Resize the given image to an arbitrary size, with filtering.
 *
 * Parameters:    src: Source data plane.
 *               dest: Destination data plane.
 *              width: Width of frame.
 *             height: Height of frame.
 *                Bpp: Bytes (not bits!) per pixel.
 *              new_w: New frame width.
 *              new_h: New frame height.
 *             filter: Filter type (TCV_ZOOM_*).
 * Return value: Nonzero on success, zero on error (invalid parameters).
 * Preconditions: src != NULL: src[0]..src[width*height*Bpp-1] are readable
 *                dest != NULL: dest[0]..dest[new_w*new_h*Bpp-1] are writable
 *                src != dest: src and dest do not overlap
 * Postconditions: (on success) dest[0]..dest[new_w*new_h*Bpp-1] are set
 */

/* ZoomInfo cache */
#define ZOOM_CACHE_SIZE 10
static struct {
    int old_w, old_h, new_w, new_h, Bpp;
    TCVZoomFilter filter;
    ZoomInfo *zi;
} zoominfo_cache[ZOOM_CACHE_SIZE];

int tcv_zoom(uint8_t *src, uint8_t *dest, int width, int height, int Bpp,
             int new_w, int new_h, TCVZoomFilter filter)
{
    ZoomInfo *zi;
    int free_zi = 0;  // Should the ZoomInfo be freed after use?
    int i;

    if (!src || !dest || width <= 0 || height <= 0 || (Bpp != 1 && Bpp != 3)) {
        tc_log_error("libtcvideo", "tcv_zoom: invalid frame parameters!");
        return 0;
    }
    if (new_w <= 0 || new_h <= 0) {
        tc_log_error("libtcvideo", "tcv_zoom: invalid target size %dx%d!",
                     new_w, new_h);
        return 0;
    }
    switch (filter) {
      case TCV_ZOOM_BOX:
      case TCV_ZOOM_TRIANGLE:
      case TCV_ZOOM_HERMITE:
      case TCV_ZOOM_BELL:
      case TCV_ZOOM_B_SPLINE:
      case TCV_ZOOM_MITCHELL:
      case TCV_ZOOM_LANCZOS3:
        break;
      default:
        tc_log_error("libtcvideo", "tcv_zoom: invalid filter %d!", filter);
        return 0;
    }

    for (i = 0, zi = NULL; i < ZOOM_CACHE_SIZE && zi == NULL; i++) {
        if (zoominfo_cache[i].zi     != NULL
         && zoominfo_cache[i].old_w  == width
         && zoominfo_cache[i].old_h  == height
         && zoominfo_cache[i].new_w  == new_w
         && zoominfo_cache[i].new_h  == new_h
         && zoominfo_cache[i].Bpp    == Bpp
         && zoominfo_cache[i].filter == filter
        ) {
            zi = zoominfo_cache[i].zi;
        }
    }
    if (!zi) {
        zi = zoom_init(width, height, new_w, new_h, Bpp, filter);
        if (!zi) {
            tc_log_error("libtcvideo", "tcv_zoom: zoom_init() failed!");
            return 0;
        }
        free_zi = 1;
        for (i = 0; i < ZOOM_CACHE_SIZE; i++) {
            if (!zoominfo_cache[i].zi) {
                zoominfo_cache[i].zi     = zi;
                zoominfo_cache[i].old_w  = width;
                zoominfo_cache[i].old_h  = height;
                zoominfo_cache[i].new_w  = new_w;
                zoominfo_cache[i].new_h  = new_h;
                zoominfo_cache[i].Bpp    = Bpp;
                zoominfo_cache[i].filter = filter;
                free_zi = 0;
                break;
            }
        }
    }
    zoom_process(zi, src, dest);
    if (free_zi)
        zoom_free(zi);
    return 1;
}

/*************************************************************************/

/**
 * tcv_reduce:  Efficiently reduce the image size by a specified integral
 * amount, by removing intervening pixels.
 *
 * Parameters:      src: Source data plane.
 *                 dest: Destination data plane.
 *                width: Width of frame.
 *               height: Height of frame.
 *                  Bpp: Bytes (not bits!) per pixel.
 *                new_w: New frame width.
 *                new_h: New frame height.
 *             reduce_w: Ratio to reduce width by.
 *             reduce_h: Ratio to reduce height by.
 * Return value: Nonzero on success, zero on error (invalid parameters).
 * Preconditions: src != NULL: src[0]..src[width*height*Bpp-1] are readable
 *                dest != NULL && reduce_w > 0 && reduce_h > 0:
 *                    destw = width / reduce_w;
 *                    desth = height / reduce_h;
 *                    dest[0]..dest[destw*desth*Bpp-1] are writable
 *                src != dest: src and dest do not overlap
 * Postconditions: (on success) dest[0]..dest[destw*desth*Bpp-1] are set
 */

int tcv_reduce(uint8_t *src, uint8_t *dest, int width, int height, int Bpp,
               int reduce_w, int reduce_h)
{
    if (!src || !dest || width <= 0 || height <= 0 || (Bpp != 1 && Bpp != 3)) {
        tc_log_error("libtcvideo", "tcv_reduce: invalid frame parameters!");
        return 0;
    }
    if (reduce_w <= 0 || reduce_h <= 0) {
        tc_log_error("libtcvideo", "tcv_reduce: invalid reduction parameters"
                     " (%d,%d)!", reduce_w, reduce_h);
        return 0;
    }

    if (reduce_w != 1) {
        /* Standard case: width and (possibly) height are being reduced */
        int x, y, i;
        int xstep = Bpp * reduce_w;
        for (y = 0; y < height / reduce_h; y++) {
            for (x = 0; x < width / reduce_w; x++) {
                for (i = 0; i < Bpp; i++)
                    *dest++ = src[x*xstep+i];
            }
            src += width*Bpp * reduce_h;
        }
    } else if (reduce_h != 1) {
        /* Optimized case 1: only height is being reduced */
        int y;
        int Bpl = width * Bpp;
        for (y = 0; y < height / reduce_h; y++)
            ac_memcpy(dest + y*Bpl, src + y*(Bpl*reduce_h), Bpl);
    } else {
        /* Optimized case 2: no reduction, direct copy */
        ac_memcpy(dest, src, width*height*Bpp);
    }

    return 1;
}

/*************************************************************************/

/**
 * tcv_flip_v:  Flip the given image vertically.
 *
 * Parameters:    src: Source data plane.
 *               dest: Destination data plane.
 *              width: Width of frame.
 *             height: Height of frame.
 *                Bpp: Bytes (not bits!) per pixel.
 * Return value: Nonzero on success, zero on error (invalid parameters).
 * Preconditions: src != NULL: src[0]..src[width*height*Bpp-1] are readable
 *                dest != NULL: dest[0]..dest[width*height*Bpp-1] are writable
 *                src != dest: src and dest do not overlap
 * Postconditions: (on success) dest[0]..dest[width*height*Bpp-1] are set
 */

int tcv_flip_v(uint8_t *src, uint8_t *dest, int width, int height, int Bpp)
{
    int Bpl = width * Bpp;  /* bytes per line */
    int y;
    uint8_t buf[TC_MAX_V_FRAME_WIDTH * TC_MAX_V_BYTESPP];

    if (!src || !dest || width <= 0 || height <= 0 || (Bpp != 1 && Bpp != 3)) {
        tc_log_error("libtcvideo", "tcv_flip_v: invalid frame parameters!");
        return 0;
    }

    /* Note that GCC4 can optimize this perfectly; no need for extra
     * pointer variables */
    if (src != dest) {
        for (y = 0; y < height; y++) {
            ac_memcpy(dest + ((height-1)-y)*Bpl, src + y*Bpl, Bpl);
        }
    } else {
        for (y = 0; y < (height+1)/2; y++) {
            ac_memcpy(buf, src + y*Bpl, Bpl);
            ac_memcpy(dest + y*Bpl, src + ((height-1)-y)*Bpl, Bpl);
            ac_memcpy(dest + ((height-1)-y)*Bpl, buf, Bpl);
        }
    }

    return 1;
}

/*************************************************************************/

/**
 * tcv_flip_h:  Flip the given image horizontally.
 *
 * Parameters:    src: Source data plane.
 *               dest: Destination data plane.
 *              width: Width of frame.
 *             height: Height of frame.
 *                Bpp: Bytes (not bits!) per pixel.
 * Return value: Nonzero on success, zero on error (invalid parameters).
 * Preconditions: src != NULL: src[0]..src[width*height*Bpp-1] are readable
 *                dest != NULL: dest[0]..dest[width*height*Bpp-1] are writable
 *                src != dest: src and dest do not overlap
 * Postconditions: (on success) dest[0]..dest[width*height*Bpp-1] are set
 */

int tcv_flip_h(uint8_t *src, uint8_t *dest, int width, int height, int Bpp)
{
    int x, y, i;

    if (!src || !dest || width <= 0 || height <= 0 || (Bpp != 1 && Bpp != 3)) {
        tc_log_error("libtcvideo", "tcv_flip_h: invalid frame parameters!");
        return 0;
    }

    for (y = 0; y < height; y++) {
        uint8_t *srcline = src + y*width*Bpp;
        uint8_t *destline = dest + y*width*Bpp;
        if (src != dest) {
            for (x = 0; x < width; x++) {
                for (i = 0; i < Bpp; i++) {
                    destline[((width-1)-x)*Bpp+i] = srcline[x*Bpp+i];
                }
            }
        } else {
            for (x = 0; x < (width+1)/2; x++) {
                for (i = 0; i < Bpp; i++) {
                    uint8_t tmp = srcline[x*Bpp+i];
                    destline[x*Bpp+i] = srcline[((width-1)-x)*Bpp+i];
                    destline[((width-1)-x)*Bpp+i] = tmp;
                }
            }
        }
    }

    return 1;
}

/*************************************************************************/

/**
 * tcv_gamma_correct:  Perform gamma correction on the given image.
 *
 * Parameters:    src: Source data plane.
 *               dest: Destination data plane.
 *              width: Width of frame.
 *             height: Height of frame.
 *                Bpp: Bytes (not bits!) per pixel.
 *              gamma: Gamma value.
 * Return value: Nonzero on success, zero on error (invalid parameters).
 * Preconditions: src != NULL: src[0]..src[width*height*Bpp-1] are readable
 *                dest != NULL: dest[0]..dest[width*height*Bpp-1] are writable
 *                src != dest: src and dest do not overlap
 * Postconditions: (on success) dest[0]..dest[width*height*Bpp-1] are set
 */

int tcv_gamma_correct(uint8_t *src, uint8_t *dest, int width, int height,
                      int Bpp, double gamma)
{
    int i;

    if (!src || !dest || width <= 0 || height <= 0 || (Bpp != 1 && Bpp != 3)) {
        tc_log_error("libtcvideo", "tcv_gamma: invalid frame parameters!");
        return 0;
    }
    if (gamma <= 0) {
        tc_log_error("libtcvideo", "tcv_gamma: invalid gamma (%.3f)!", gamma);
        return 0;
    }

    init_gamma_table(gamma);
    for (i = 0; i < width*height*Bpp; i++)
        dest[i] = gamma_table[src[i]];

    return 1;
}

/*************************************************************************/

/**
 * tcv_antialias:  Perform antialiasing on the given image.
 *
 * Parameters:    src: Source data plane.
 *               dest: Destination data plane.
 *              width: Width of frame.
 *             height: Height of frame.
 *                Bpp: Bytes (not bits!) per pixel.
 *             weight: `weight' antialiasing parameter.
 *               bias: `bias' antialiasing parameter.
 * Return value: Nonzero on success, zero on error (invalid parameters).
 * Preconditions: src != NULL: src[0]..src[width*height*Bpp-1] are readable
 *                dest != NULL: dest[0]..dest[width*height*Bpp-1] are writable
 *                src != dest: src and dest do not overlap
 * Postconditions: (on success) dest[0]..dest[width*height*Bpp-1] are set
 */

static void antialias_line(uint8_t *src, uint8_t *dest, int width, int Bpp);

int tcv_antialias(uint8_t *src, uint8_t *dest, int width, int height,
                  int Bpp, double weight, double bias)
{
    int y;

    if (!src || !dest || width <= 0 || height <= 0 || (Bpp != 1 && Bpp != 3)) {
        tc_log_error("libtcvideo", "tcv_antialias: invalid frame parameters!");
        return 0;
    }
    if (weight < 0 || weight > 1 || bias < 0 || bias > 1) {
        tc_log_error("libtcvideo", "tcv_antialias: invalid antialiasing"
                     " parameters (weight=%.3f, bias=%.3f)");
        return 0;
    }

    init_aa_table(weight, bias);
    ac_memcpy(dest, src, width*Bpp);
    for (y = 1; y < height-1; y++)
        antialias_line(src + y*width*Bpp, dest + y*width*Bpp, width, Bpp);
    ac_memcpy(dest + (height-1)*width*Bpp, src + (height-1)*width*Bpp,
              width*Bpp);

    return 1;
}


/* Helper functions: */

static inline int samecolor(uint8_t *pixel1, uint8_t *pixel2, int Bpp)
{
    int i;
    int maxdiff = abs(pixel2[0]-pixel1[0]);
    for (i = 1; i < Bpp; i++) {
        int diff = abs(pixel2[i]-pixel1[i]);
        if (diff > maxdiff)
            maxdiff = diff;
    }
    return maxdiff < AA_DIFFERENT;
}

#define C (src + x*Bpp)
#define U (C - width*Bpp)
#define D (C + width*Bpp)
#define L (C - Bpp)
#define R (C + Bpp)
#define UL (U - Bpp)
#define UR (U + Bpp)
#define DL (D - Bpp)
#define DR (D + Bpp)
#define SAME(pix1,pix2) samecolor((pix1),(pix2),Bpp)
#define DIFF(pix1,pix2) !samecolor((pix1),(pix2),Bpp)

static void antialias_line(uint8_t *src, uint8_t *dest, int width, int Bpp)
{
    int i, x;

    for (i = 0; i < Bpp; i++)
        dest[i] = src[i];
    for (x = 1; x < width-1; x++) {
        if ((SAME(L,U) && DIFF(L,D) && DIFF(L,R))
         || (SAME(L,D) && DIFF(L,U) && DIFF(L,R))
         || (SAME(R,U) && DIFF(R,D) && DIFF(R,L))
         || (SAME(R,D) && DIFF(R,U) && DIFF(R,L))
        ) {
            for (i = 0; i < Bpp; i++) {
                uint32_t tmp = aa_table_d[UL[i]]
                             + aa_table_y[U [i]]
                             + aa_table_d[UR[i]]
                             + aa_table_x[L [i]]
                             + aa_table_c[C [i]]
                             + aa_table_x[R [i]]
                             + aa_table_d[DL[i]]
                             + aa_table_y[D [i]]
                             + aa_table_d[DR[i]]
                             + 32768;
                dest[x*Bpp+i] = (verbose & TC_DEBUG) ? 255 : tmp>>16;
            }
        } else {
            for (i = 0; i < Bpp; i++)
                dest[x*Bpp+i] = src[x*Bpp+i];
        }
    }
    for (i = 0; i < Bpp; i++)
        dest[(width-1)*Bpp+i] = src[(width-1)*Bpp+i];
}

/*************************************************************************/
/*************************************************************************/

/* Internal-use helper functions. */

/*************************************************************************/

/**
 * init_resize_tables:  Initialize the lookup tables used for resizing.  If
 * either of `oldw' and `neww' is nonpositive, the horizontal resizing
 * table will not be initialized; likewise for `oldh', `newh', and the
 * vertical resizing table.  Initialization will also not be performed if
 * the values given are the same as in the previous call (thus repeated
 * calls with the same values suffer only the penalty of entering and
 * exiting the procedure).  Note the order of parameters!
 *
 * Parameters: oldw: Original image width.
 *             neww: New image width.
 *             oldh: Original image height.
 *             newh: New image height.
 * Return value: None.
 * Preconditions: oldw % 8 == 0
 *                neww % 8 == 0
 *                oldh % 8 == 0
 *                newh % 8 == 0
 * Postconditions: If oldw > 0 && neww > 0:
 *                     resize_table_x[0..neww/8-1] are initialized
 *                 If oldh > 0 && newh > 0:
 *                     resize_table_y[0..newh/8-1] are initialized
 */

static void init_resize_tables(int oldw, int neww, int oldh, int newh)
{
    static int saved_oldw = 0, saved_neww = 0,
               saved_oldh = 0, saved_newh = 0;

    if (oldw > 0 && neww > 0 && (oldw != saved_oldw || neww != saved_neww)) {
        init_one_resize_table(resize_table_x, oldw, neww);
        saved_oldw = oldw;
        saved_neww = neww;
    }
    if (oldh > 0 && newh > 0 && (oldh != saved_oldh || newh != saved_newh)) {
        init_one_resize_table(resize_table_y, oldh, newh);
        saved_oldh = oldh;
        saved_newh = newh;
    }
}


/**
 * init_one_resize_table:  Helper function for init_resize_tables() to
 * initialize a single table.
 *
 * Parameters:   table: Table to initialize.
 *             oldsize: Size to resize from.
 *             newsize: Size to resize to.
 * Return value: None.
 * Preconditions: table != NULL
 *                oldsize > 0
 *                oldsize % 8 == 0
 *                newsize > 0
 *                newsize % 8 == 0
 * Postconditions: table[0..newsize/8-1] are initialized
 */

static void init_one_resize_table(struct resize_table_elem *table,
                                  int oldsize, int newsize)
{
    int i;

    /* Compute the number of source pixels per destination pixel */
    double width_ratio = (double)oldsize / (double)newsize;

    for (i = 0; i < newsize/8; i++) {
        double oldpos;

        /* Left/topmost source pixel to use */
        oldpos = (double)i * (double)oldsize / (double)newsize;
        table[i].source = (int)oldpos;

        /* Is the new pixel contained entirely within the old? */
        if (oldpos+width_ratio < table[i].source+1) {
            /* Yes, weight ratio is 1.0:0.0 */
            table[i].weight1 = 65536;
            table[i].weight2 = 0;
        } else {
            /* No, compute appropriate weight ratio */
            double temp = ((table[i].source+1) - oldpos) / width_ratio * PI/2;
            table[i].weight1 = (uint32_t)(sin(temp)*sin(temp) * 65536 + 0.5);
            table[i].weight2 = 65536 - table[i].weight1;
        }
    }
}

/*************************************************************************/

/**
 * init_gamma_table:  Initialize the gamma correction lookup table.
 * Initialization will not be performed for repeated calls with the same
 * value.
 *
 * Parameters: gamma: Gamma value.
 * Return value: None.
 * Preconditions: gamma > 0
 * Postconditions: gamma_table[0..255] are initialized
 */

static void init_gamma_table(double gamma)
{
    static double saved_gamma = 0;
    int i;

    if (gamma != saved_gamma) {
        for (i = 0; i < 256; i++)
            gamma_table[i] = (uint8_t) (pow((i/255.0),gamma) * 255);
        saved_gamma = gamma;
    }
}

/*************************************************************************/

/**
 * init_aa_table:  Initialize the antialiasing lookup tables.
 * Initialization will not be performed for repeated calls with the same
 * values.
 *
 * Parameters: aa_weight: Antialiasing weight value.
 *               aa_bias: Antialiasing bias value.
 * Return value: None.
 * Preconditions: 0 <= aa_weight && aa_weight <= 1
 *                0 <= aa_bias && aa_bias <= 1
 * Postconditions: gamma_table[0..255] are initialized
 */

static void init_aa_table(double aa_weight, double aa_bias)
{
    static double saved_weight = -1, saved_bias = -1;
    int i;

    if (aa_weight != saved_weight || aa_bias != saved_bias) {
        for (i = 0; i < 256; ++i) {
            aa_table_c[i] = i*aa_weight * 65536;
            aa_table_x[i] = i*aa_bias*(1-aa_weight)/4 * 65536;
            aa_table_y[i] = i*(1-aa_bias)*(1-aa_weight)/4 * 65536;
            aa_table_d[i] = (aa_table_x[i]+aa_table_y[i]+1)/2;
        }
        saved_weight = aa_weight;
        saved_bias = aa_bias;
    }
}

/*************************************************************************/
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
