/*
 *  frame_threads.h
 *
 *  Copyright (C) Thomas Östreich - June 2001
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

#include <pthread.h>
#include "zoom.h"

#ifndef _FRAME_THREADS_H
#define _FRAME_THREADS_H

void frame_threads_init(vob_t *vob, int vworkers, int aworkers);
void process_vframe(vob_t *vob);
void process_aframe(vob_t *vob);
void frame_threads_close();
int get_fthread_id(int flag);
void frame_threads_notify(int what);

extern int have_aframe_threads;
extern int have_vframe_threads;

extern pthread_cond_t vbuffer_fill_cv;
extern pthread_mutex_t vbuffer_im_fill_lock;
extern uint32_t vbuffer_im_fill_ctr;

extern pthread_mutex_t vbuffer_xx_fill_lock;
extern uint32_t vbuffer_xx_fill_ctr;

extern pthread_mutex_t vbuffer_ex_fill_lock;
extern uint32_t vbuffer_ex_fill_ctr;

extern pthread_cond_t abuffer_fill_cv;
extern pthread_mutex_t abuffer_im_fill_lock;
extern uint32_t abuffer_im_fill_ctr;

extern pthread_mutex_t abuffer_xx_fill_lock;
extern uint32_t abuffer_xx_fill_ctr;

extern pthread_mutex_t abuffer_ex_fill_lock;
extern uint32_t abuffer_ex_fill_ctr;


typedef struct _fthbuf_t {
  
  //-Z (YUV)
  
  zoomer_t *zoomerY;
  zoomer_t *zoomerUV;
  image_t srcImageY;
  image_t srcImageUV; 
  image_t dstImageY;
  image_t dstImageUV;

  //-Z (RGB)

  zoomer_t *zoomer;
  image_t srcImage;
  image_t dstImage;

  //common
  pixel_t *tmpBuffer;

} fthbuf_t;

fthbuf_t tbuf[TC_FRAME_THREADS_MAX];
fthbuf_t tbuf_DI[TC_FRAME_THREADS_MAX];

#endif
