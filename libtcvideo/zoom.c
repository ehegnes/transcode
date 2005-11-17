/*
 * zoom.c - resize an image to an arbitrary size with filtering
 * Based on "Filtered Image Rescaling" by Dale Schumacher (public domain).
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "tcvideo.h"
#include "zoom.h"

#include "src/transcode.h"
#include <math.h>

/*************************************************************************/

/* Data structures for holding resizing data (used internally). */

/* A contributor to a single pixel */
struct contrib {
    int pixel;
    double weight;
};

/* List of all contributors to a single pixel */
struct clist {
    int n;                      /* Number of contributors */
    struct contrib *list;       /* Pointer to list of contributors */
};

/* Data for a resize operation */
struct zoominfo {
    int old_w, old_h;           /* Original width and height */
    int new_w, new_h;           /* New width and height */
    int Bpp;                    /* Bytes per pixel */
    double (*filter)(double);   /* Filter function */
    double fwidth;              /* Filter width */
    int32_t *x_contrib;         /* Contributors in the horizontal direction */
    int32_t *y_contrib;         /* Contributors in the vertical direction */
    uint8_t *tmpimage;          /* Temporary buffer */
};

/* Convert a double to a 16.16 fixed-point value */
#define DOUBLE_TO_FIXED(v) ((int32_t)((v)*65536))

/* Convert a 16.16 fixed-point value to an integer */
#define FIXED_TO_INT(v) ((v)>>16)

/*************************************************************************/
/*************************************************************************/

/**
 * *_filter:  Filter functions for resizing.
 *
 * Parameters: t: Filter parameter.
 * Return value: Filter result.
 * Preconditions: None.
 * Postconditions: 0 <= t && t <= 1
 */

/************************************/

static const double hermite_support = 1.0;

static double hermite_filter(double t)
{
    /* f(t) = 2|t|^3 - 3|t|^2 + 1, -1 <= t <= 1 */
    if (t < 0.0)
        t = -t;
    if (t < 1.0)
        return (2.0 * t - 3.0) * t * t + 1.0;
    return 0.0;
}

/************************************/

static const double box_support = 0.5;

static double box_filter(double t)
{
    if ((t > -0.5) && (t <= 0.5))
        return 1.0;
    return 0.0;
}

/************************************/

static const double triangle_support = 1.0;

static double triangle_filter(double t)
{
    if (t < 0.0)
        t = -t;
    if (t < 1.0)
        return 1.0 - t;
    return 0.0;
}

/************************************/

static const double bell_support = 1.5;

static double bell_filter(double t)
{
    if (t < 0)
        t = -t;
    if  (t < .5)
        return .75 - (t * t);
    if (t < 1.5) {
        t = (t - 1.5);
        return .5 * (t * t);
    }
    return 0.0;
}

/************************************/

static const double B_spline_support = 2.0;

static double B_spline_filter(double t)
{
    double tt;

    if (t < 0)
        t = -t;
    if (t < 1) {
        tt = t * t;
        return (.5 * tt * t) - tt + (2.0 / 3.0);
    } else if (t < 2) {
        t = 2 - t;
        return (1.0 / 6.0) * (t * t * t);
    }
    return 0.0;
}

/************************************/

static const double lanczos3_support = 3.0;

#ifndef PI
# define PI 3.14159265358979323846264338327950
#endif
#define SINC(x) ((x) != 0 ? sin((x)*PI) / ((x)*PI) : 1.0)

static double lanczos3_filter(double t)
{
    if (t < 0)
        t = -t;
    if (t < 3.0)
        return SINC(t) * SINC(t/3.0);
    return 0.0;
}

#undef SINC

/************************************/

static const double mitchell_support = 2.0;

#define B       (1.0 / 3.0)
#define C       (1.0 / 3.0)

static double mitchell_filter(double t)
{
    double tt;

    tt = t * t;
    if (t < 0)
        t = -t;
    if (t < 1.0) {
        t = (((12.0 - 9.0 * B - 6.0 * C) * (t * tt))
             + ((-18.0 + 12.0 * B + 6.0 * C) * tt)
             + (6.0 - 2 * B));
        return t / 6.0;
    } else if (t < 2.0) {
        t = (((-1.0 * B - 6.0 * C) * (t * tt))
             + ((6.0 * B + 30.0 * C) * tt)
             + ((-12.0 * B - 48.0 * C) * t)
             + (8.0 * B + 24 * C));
        return t / 6.0;
    }
    return 0.0;
}

#undef B
#undef C

/*************************************************************************/

/**
 * gen_contrib:  Helper function to generate the list of contributors to
 * each resized pixel in one direction (horizontal or vertical).
 *
 * Parameters: oldsize: Size of original image in the direction for which
 *                      contributors are being generated.
 *             newsize: Size of resized image in the direction for which
 *                      contributors are being generated.
 *              stride: Number of bytes between adjacent pixels in the
 *                      direction for which contributors are being generated.
 *              filter: As for zoom_process().
 *              fwidth: As for zoom_process().
 * Return value: A pointer to a `newsize'-element array of `struct clist'
 *               elements, or NULL on error (out of memory).
 * Preconditions: oldsize > 0
 *                newsize > 0
 *                stride > 0
 *                filter != NULL
 *                fwidth > 0
 * Postconditions: None.
 */

static struct clist *gen_contrib(int oldsize, int newsize, int stride,
                                 double (*filter)(double), double fwidth)
{
    struct clist *contrib;
    double scale = (double)newsize / (double)oldsize;
    double new_fwidth, fscale;
    int i, j;

    contrib = tc_zalloc(newsize * sizeof(struct clist));

    if (scale < 1.0) {
        fscale = 1.0 / scale;
    } else {
        fscale = 1.0;
    }
    new_fwidth = fwidth * fscale;
    for (i = 0; i < newsize; ++i) {
        double center = (double) i / scale;
        int left = ceil(center - new_fwidth);
        int right = floor(center + new_fwidth);
        contrib[i].n = 0;
        contrib[i].list = tc_zalloc((right-left+1) * sizeof(struct contrib));
        for (j = left; j <= right; ++j) {
            int k, n;
            double weight = center - (double) j;
            weight = (*filter)(weight / fscale) / fscale;
            if (j < 0) {
                n = -j;
            } else if (j >= oldsize) {
                n = (oldsize - j) + oldsize - 1;
            } else {
                n = j;
            }
            k = contrib[i].n++;
            contrib[i].list[k].pixel = n*stride;
            contrib[i].list[k].weight = weight;
        }
    }

    return contrib;
}

/*************************************************************************/
/*************************************************************************/

/**
 * zoom_init:  Allocate, initialize, and return a ZoomInfo structure for
 * use in subsequent zoom_process() calls.  The structure should be freed
 * with zoom_free() when no longer needed.
 *
 * Parameters:  old_w: Width of original image.
 *              old_h: Height of original image.
 *              new_w: Width of resized image.
 *              new_h: Height of resized image.
 *                Bpp: Bytes (not bits!) per pixel.
 *             filter: Filter identifier (TCV_ZOOM_*).
 * Return value: A pointer to a newly allocated ZoomInfo structure, or NULL
 *               on error (invalid parameters or out of memory).
 * Preconditions: None.
 * Postconditions: None.
 */

ZoomInfo *zoom_init(int old_w, int old_h, int new_w, int new_h, int Bpp,
                    TCVZoomFilter filter)
{
    ZoomInfo *zi;
    struct clist *x_contrib, *y_contrib;
    int count, i, j;
    int32_t *ptr;

    /* Sanity check */
    if (old_w <= 0 || old_h <= 0 || new_w <= 0 || new_h <= 0 || Bpp <= 0)
        return NULL;

    /* Allocate structure */
    zi = tc_malloc(sizeof(*zi));
    if (!zi)
        return NULL;

    /* Set up scalar members, and check filter value */
    zi->old_w = old_w;
    zi->old_h = old_h;
    zi->new_w = new_w;
    zi->new_h = new_h;
    zi->Bpp = Bpp;
    switch (filter) {
      case TCV_ZOOM_BOX:
        zi->filter = box_filter;
        zi->fwidth = box_support;
        break;
      case TCV_ZOOM_TRIANGLE:
        zi->filter = triangle_filter;
        zi->fwidth = triangle_support;
        break;
      case TCV_ZOOM_HERMITE:
        zi->filter = hermite_filter;
        zi->fwidth = hermite_support;
        break;
      case TCV_ZOOM_BELL:
        zi->filter = bell_filter;
        zi->fwidth = bell_support;
        break;
      case TCV_ZOOM_B_SPLINE:
        zi->filter = B_spline_filter;
        zi->fwidth = B_spline_support;
        break;
      case TCV_ZOOM_MITCHELL:
        zi->filter = mitchell_filter;
        zi->fwidth = mitchell_support;
        break;
      case TCV_ZOOM_LANCZOS3:
        zi->filter = lanczos3_filter;
        zi->fwidth = lanczos3_support;
        break;
      default:
        free(zi);
        return NULL;
    }

    /* Generate contributor lists and allocate temporary image buffer */
    zi->x_contrib = NULL;
    zi->y_contrib = NULL;
    x_contrib = gen_contrib(old_w, new_w, Bpp, zi->filter, zi->fwidth);
    y_contrib = gen_contrib(old_h, new_h, Bpp*new_w, zi->filter, zi->fwidth);
    zi->tmpimage = tc_malloc(new_w * old_h * Bpp);
    if (!x_contrib || !y_contrib || !zi->tmpimage) {
        zoom_free(zi);
        return NULL;
    }

    /* Convert contributor lists into flat arrays and fixed-point values.
     * The flat array consists of a contributor count plus two values per
     * contributor (index and fixed-point weight) for each output pixel.
     * Note that for the horizontal direction, we make `Bpp' copies of the
     * contributors, adjusting the offset for each byte of the pixel. */
    count = 0;
    for (i = 0; i < new_w; i++)
        count += 1 + 2 * x_contrib[i].n;
    zi->x_contrib = tc_malloc(sizeof(int32_t) * count * Bpp);
    count = 0;
    for (i = 0; i < new_h; i++)
        count += 1 + 2 * y_contrib[i].n;
    zi->y_contrib = tc_malloc(sizeof(int32_t) * count);
    if (!zi->x_contrib || !zi->y_contrib) {
        zoom_free(zi);
        return NULL;
    }
    for (ptr = zi->x_contrib, i = 0; i < new_w * Bpp; i++) {
        *ptr++ = x_contrib[i/Bpp].n;
        for (j = 0; j < x_contrib[i/Bpp].n; j++) {
            *ptr++ = x_contrib[i/Bpp].list[j].pixel + i%Bpp;
            *ptr++ = DOUBLE_TO_FIXED(x_contrib[i/Bpp].list[j].weight);
        }
    }
    for (ptr = zi->y_contrib, i = 0; i < new_h; i++) {
        *ptr++ = y_contrib[i].n;
        for (j = 0; j < y_contrib[i].n; j++) {
            *ptr++ = y_contrib[i].list[j].pixel;
            *ptr++ = DOUBLE_TO_FIXED(y_contrib[i].list[j].weight);
        }
    }

    /* Free original contributor lists */
    for (i = 0; i < new_w; i++)
        free(x_contrib[i].list);
    free(x_contrib);
    for (i = 0; i < new_h; i++)
        free(y_contrib[i].list);
    free(y_contrib);

    /* Done */
    return zi;
}

/*************************************************************************/

/**
 * zoom_process:  Image resizing core.
 *
 * Parameters:   zi: ZoomInfo structure allocated by zoom_init().
 *              src: Source data plane.
 *             dest: Destination data plane.
 * Return value: None.
 * Preconditions: zi was allocated by zoom_init()
 *                src != NULL
 *                src[0]..src[zi->old_w*zi->old_h*zi->Bpp-1] are readable
 *                dest != NULL
 *                dest[0]..dest[zi->new_w*zi->new_h*zi->Bpp-1] are writable
 *                src and dest do not overlap
 * Postconditions: dest[0]..dest[zi->new_w*zi->new_h*zi->Bpp-1] are set
 */

/* clamp the input to the specified range */
#define CLAMP(v,l,h)    ((v)<(l) ? (l) : (v) > (h) ? (h) : v)

void zoom_process(const ZoomInfo *zi, const uint8_t *src, uint8_t *dest)
{
    int x, y, i;
    const uint8_t *from;
    uint8_t *to;
    int32_t *contrib;

    /* Apply filter to zoom horizontally from src to tmp */
    for (y = 0, from = src, to = zi->tmpimage;
         y < zi->old_h;
         y++, from += zi->old_w * zi->Bpp, to += zi->new_w * zi->Bpp
    ) {
        contrib = zi->x_contrib;
        for (x = 0; x < zi->new_w * zi->Bpp; x++) {
            int32_t weight = DOUBLE_TO_FIXED(0.5);
            int n = *contrib++;
            for (i = 0; i < n; i++) {
                int pixel = *contrib++;
                weight += from[pixel] * (*contrib++);
            }
            to[x] = CLAMP(FIXED_TO_INT(weight), 0, 255);
        }
    }

    /* Apply filter to zoom vertically from tmp to dest */
    /* Use Y as the outside loop to avoid cache thrashing on output buffer */
    from = zi->tmpimage;
    contrib = zi->y_contrib;
    for (y = 0, to = dest; y < zi->new_h; y++, to += zi->new_w * zi->Bpp) {
        int n = *contrib++;
        for (x = 0; x < zi->new_w * zi->Bpp; x++) {
            int32_t weight = DOUBLE_TO_FIXED(0.5);
            for (i = 0; i < n; i++) {
                int pixel = contrib[i*2];
                weight += from[x+pixel] * contrib[i*2+1];
            }
            to[x] = CLAMP(FIXED_TO_INT(weight), 0, 255);
        }
        contrib += 2*n;
    }
}

/*************************************************************************/

/**
 * zoom_free():  Free a ZoomInfo structure.
 *
 * Parameters: zi: ZoomInfo structure allocated by zoom_init().
 * Return value: None.
 * Preconditions: zi was allocated by zoom_init()
 * Postconditions: zi is freed
 */
void zoom_free(ZoomInfo *zi)
{
    free(zi->x_contrib);
    free(zi->y_contrib);
    free(zi->tmpimage);
    free(zi);
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
