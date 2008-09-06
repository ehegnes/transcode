/*
 *  transform.c
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

#include "transform.h"
#include "libtc/libtc.h"
#include <stdlib.h>

int myround(double x){
  double x_;
  x_ = floor(x);
  if(x-x_ >= 0.5)
    return ((int)x_)+1;
  else
    return (int)x_;
}

transform_t null_transform(){ 
  transform_t t;
  t.x=0;
  t.y=0;
  t.alpha=0;
  t.extra=0;
  return t;
}

transform_t add_transforms(const transform_t* t1, const transform_t* t2){
  transform_t t;
  t.x = t1->x + t2->x;
  t.y = t1->y + t2->y;
  t.alpha = t1->alpha + t2->alpha;
  t.extra = 0;
  return t;
}

transform_t add_transforms_(const transform_t t1, const transform_t t2){
  return add_transforms(&t1,&t2);
}

transform_t sub_transforms(const transform_t* t1, const transform_t* t2){
  transform_t t;
  t.x = t1->x - t2->x;
  t.y = t1->y - t2->y;
  t.alpha = t1->alpha - t2->alpha;
  t.extra = 0;
  return t;
}

transform_t mult_transform(const transform_t* t1, double f){
  transform_t t;
  t.x = t1->x * f;
  t.y = t1->y * f;
  t.alpha = t1->alpha * f;
  t.extra = 0;
  return t;
}

transform_t mult_transform_(const transform_t t1, double f){
  return mult_transform(&t1,f);
}

static int cmptrans_x(const void *t1, const void* t2){
  double a = ((transform_t*)t1)->x;
  double b = ((transform_t*)t2)->x;
  return a<b ? -1: ( a>b ? 1: 0  );
}

static int cmptrans_y(const void *t1, const void* t2){
  double a = ((transform_t*)t1)->y;
  double b = ((transform_t*)t2)->y;
  return a<b ? -1: ( a>b ? 1: 0  );
}
/* static int cmptrans_alpha(const void *t1, const void* t2){ */
/*   double a = ((transform_t*)t1)->alpha; */
/*   double b = ((transform_t*)t2)->alpha; */
/*   return a<b ? -1: ( a>b ? 1: 0  ); */
/* } */


static int cmpdouble(const void *t1, const void* t2){
  double a = *((double*)t1);
  double b = *((double*)t2);
  return a<b ? -1: ( a>b ? 1: 0  );
}

transform_t median_xy_transform(const transform_t* transforms, int len){
  transform_t* ts = NEW(transform_t,len);
  transform_t t;
  memcpy(ts,transforms, sizeof(transform_t)*len ); 
  int half=len/2;
  qsort(ts,len, sizeof(transform_t), cmptrans_x);
  if(len%2==0) t.x = ts[half].x; else t.x = (ts[half].x + ts[half+1].x)/2;
  qsort(ts,len, sizeof(transform_t), cmptrans_y);
  if(len%2==0) t.y = ts[half].y; else t.y = (ts[half].y + ts[half+1].y)/2;
  t.alpha=0;
  t.extra=0;
  tc_free(ts);
  return t;
}

double median(double* ds, int len){
  qsort(ds,len, sizeof(double), cmpdouble);
  int half=len/2;
  if(len%2==0) return ds[half]; else return (ds[half] + ds[half+1])/2;
}

double mean(double* ds, int len){
  double sum=0;
  int i=0;
  for(i=0; i<len; i++)
    sum += ds[i];
  return sum/len;
}

// mean with cutted upper and lower pentile
double cleanmean(double* ds, int len){
  int cut = len/5;
  double sum=0;
  int i=0;
  qsort(ds,len, sizeof(double), cmpdouble);
  for(i=cut; i<len-cut; i++){ // all but first and last
    sum     +=ds[i];
  }
  return sum/(len-2.0*cut);
}

transform_t cleanmean_xy_transform(const transform_t* transforms, int len){
  transform_t* ts = NEW(transform_t,len);
  transform_t t = null_transform();
  int cut = len/5;
  int i;
  t.x=0; t.y=0; t.alpha=0;
  memcpy(ts,transforms, sizeof(transform_t)*len ); 
  qsort(ts,len, sizeof(transform_t), cmptrans_x);
  for(i=cut; i<len-cut; i++){ // all but cutted
    t.x     +=ts[i].x;
  }
  qsort(ts,len, sizeof(transform_t), cmptrans_y);
  for(i=cut; i<len-cut; i++){ // all but cutted
    t.y     +=ts[i].y;
  }
  tc_free(ts);
  return mult_transform(&t,1.0/(len-2.0*cut));
}
