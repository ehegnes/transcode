/*
 *  zoom.h
 *
 *  Copyright (C) Thomas Östreich - September 2001
 *
 *  Filtered Image Rescaling by Dale Schumacher
 *  2001/08/22: Modified by Vaclav Slavik for inclusion in transcode
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

#ifndef _ZOOMIMAGE_H_
#define _ZOOMIMAGE_H_

/* This type represents one pixel */
typedef unsigned char pixel_t;

/* Helper structure that holds information about image */
typedef struct
{
    int     xsize;      /* horizontal size of the image in pixel_ts */
    int     ysize;      /* vertical size of the image in pixel_ts */
    pixel_t *data;      /* pointer to first scanline of image */
    int     span;       /* byte offset between two scanlines */
    int     pixspan;    /* byte offset between two pixels on line (usually 1) */
} image_t;

/* Initializes image_t structure for given width and height.
   Please do NOT manipulate xsize, span and ysize properties directly,
   zooming code has inverted meaning of x and y axis 
   (i.e. w=ysize, h=xsize)  */
extern void zoom_setup_image(image_t *img, int w, int h, int depth, pixel_t *data);


typedef int fixdouble;
#define double2fixdouble(d) ((fixdouble)((d) * 65536))
#define fixdouble2int(d)    (((d) + 32768) >> 16)

typedef union
{
   pixel_t      *pixel;
   fixdouble     weight;
   int           index;
   int           count;
} instruction_t;

/* This structure holds the state of zooming function just after
   initialization and before image processing. The advantage of
   this approach is that you can do as much of (CPU intensive)
   processing as possible only once and use relatively fast
   function for per-frame processing.  */
typedef struct
{
    image_t       *src, *dst;
    pixel_t       *tmp;
    instruction_t *programY, *programX;
}  zoomer_t;


/* Initializes zooming for given destination and source dimensions
   and depths and filter function. */
extern zoomer_t *zoom_image_init(image_t *dst, image_t *src,
                                 double (*filterf)(double), double fwidth);

/* Processes frame.  */
extern void zoom_image_process(zoomer_t *zoomer);

/* Shuts down zoomer, deallocates memory. */
extern void zoom_image_done(zoomer_t *zoomer);

/* These are filter functions and their respective fwidth values
   that you can use when filtering. The higher fwidth is, the
   longer processing takes. Filter function's cost is much less
   important because it is computed only once, during zoomer_t
   initialization.
   
   Lanczos3 yields best result when downscaling video. */

#define       Box_support       (0.5)
extern double Box_filter(double t);
#define       Triangle_support  (1.0)
extern double Triangle_filter(double t);
#define       Hermite_support   (1.0)
extern double Hermite_filter(double t);
#define       Bell_support      (1.5)
extern double Bell_filter(double t);        /* box (*) box (*) box */
#define       B_spline_support  (2.0)
extern double B_spline_filter(double t);    /* box (*) box (*) box (*) box */
#define       Mitchell_support  (2.0)
extern double Mitchell_filter(double t);
#define       Lanczos3_support  (3.0)
extern double Lanczos3_filter(double t);

#endif
