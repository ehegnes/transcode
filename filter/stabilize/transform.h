/*
 *  transform.h
 *
 *  Copyright (C) Georg Martius - June 2007
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

#ifndef __TRANSFORM_H
#define __TRANSFORM_H

#include <math.h>

#define NEW(type,cnt) (type*)tc_malloc(sizeof(type)*cnt) 
#define min(a,b) ((a) < (b)) ? (a) : (b)

typedef struct _transform_t {
  double x;
  double y;
  double alpha;
  int extra; // 0 for normal trans; 1 for inter scene cut 
} transform_t;

int myround(double x);
transform_t null_transform(void);
transform_t add_transforms(const transform_t* t1, const transform_t* t2);
transform_t add_transforms_(const transform_t t1, const transform_t t2);
transform_t sub_transforms(const transform_t* t1, const transform_t* t2);
transform_t mult_transform(const transform_t* t1, double f);
transform_t mult_transform_(const transform_t t1, double f);
static int cmptrans_x(const void *t1, const void* t2);
static int cmptrans_y(const void *t1, const void* t2);
// static int cmptrans_alpha(const void *t1, const void* t2);
static int cmpdouble(const void *t1, const void* t2);
transform_t median_xy_transform(const transform_t* transforms, int len);
double median(double* ds, int len);
double mean(double* ds, int len);
/// mean with cutted upper and lower pentile
double cleanmean(double* ds, int len);
transform_t cleanmean_xy_transform(const transform_t* transforms, int len);


#endif
