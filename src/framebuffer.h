/*
 *  framebuffer.h
 *
 *  Copyright (C) Thomas Östreich - June 2001
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

#ifndef _FRAMEBUFFER_H
#define _FRAMEBUFFER_H 

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <pthread.h>

#define FRAME_NULL  -1
#define FRAME_EMPTY  0
#define FRAME_READY  1
#define FRAME_LOCKED 2
#define FRAME_WAIT   3

#define TC_BUFFER_EMPTY  0
#define TC_BUFFER_FULL   1
#define TC_BUFFER_READY  2
#define TC_BUFFER_LOCKED 3

/*
 * BIG FAT WARNING:
 *
 * These structures must be kept in sync: meaning that if you add
 * another field to the vframe_list_t you must add it at the end
 * of the structure.
 *
 * aframe_list_t, vframe_list_t and the wrapper frame_list_t share
 * the same offsets to their elements up to the field "size". That
 * means that when a filter is called with at init time with the
 * anonymouse frame_list_t, it can already access the size.
 *
 *          -- tibit
 */

typedef struct frame_list {
  
  int bufid;     // buffer id
  int tag;       // init, open, close, ...
  int filter_id; // filter instance to run
  int codec;     // v_codec or a_codec
  int id;        // 
  int status;
  int attributes;
  int thread_id;
  int param1; // v_width or a_rate
  int param2; // v_height or a_bits
  int param3; // v_bpp or a_chan
  int size;

} frame_list_t;

typedef struct vframe_list {
  
    //frame accounting parameter

  int bufid;     // buffer id
  int tag;       // init, open, close, ...

  int filter_id; // filter instance to run
  
  int v_codec;   // video frame codec
  
  int id;        // frame number
  int status;    // frame status

  int attributes;    //this flag must be set to activate action for the following flags:
    
  int thread_id;

    //frame physical parameter

  int v_width;
  int v_height;
  int v_bpp;
  
  int video_size;

  struct vframe_list *next;
  struct vframe_list *prev;
  
  int plane_mode;

  int clone_flag;    // set to N if frame needs to be processed (encoded) N+1 times.
  int deinter_flag;  // set to N for internal de-interlacing with "-I N"

  
  //pointer to current buffer
  char *video_buf;

  //pointer to backup buffer
  char *video_buf2;

  //flag
  int free;

  //RGB 
  char *video_buf_RGB[2];
  
  //YUV planes
  char *video_buf_Y[2];
  char *video_buf_U[2];
  char *video_buf_V[2];

#ifdef STATBUFFER
  char *internal_video_buf_0;
  char *internal_video_buf_1;
#else
  char internal_video_buf_0[SIZE_RGB_FRAME];
  char internal_video_buf_1[SIZE_RGB_FRAME];
#endif

} vframe_list_t;

vframe_list_t *vframe_register(int id);
void vframe_remove(vframe_list_t *ptr);
vframe_list_t *vframe_retrieve(void);
vframe_list_t *vframe_dup(vframe_list_t *f);
vframe_list_t *vframe_retrieve_status(int old_status, int new_status);
void vframe_set_status(vframe_list_t *ptr, int status);
int vframe_alloc(int num);
void vframe_free(void);
void vframe_flush(void);
int vframe_fill_level(int status);
void vframe_fill_print(int r);

extern pthread_mutex_t vframe_list_lock;
extern pthread_cond_t vframe_list_full_cv;
extern vframe_list_t *vframe_list_head;
extern vframe_list_t *vframe_list_tail;


typedef struct aframe_list {
  
    //frame accounting parameter

  int bufid;     // buffer id
  int tag;       // init, open, close, ...

  int filter_id; // filter instance to run

  int a_codec;   // audio frame codec
  
  int id;        // frame number
  int status;    // frame status

  int attributes;
  
  int thread_id;
  
  
  int a_rate;
  int a_bits;
  int a_chan;
  
  int audio_size;

  struct aframe_list *next;
  struct aframe_list *prev;

#ifdef STATBUFFER
  char *audio_buf;
#else
  char audio_buf[SIZE_PCM_FRAME<<2];
#endif

} aframe_list_t;

aframe_list_t *aframe_register(int id);
void aframe_remove(aframe_list_t *ptr);
aframe_list_t *aframe_retrieve(void);
aframe_list_t *aframe_dup(aframe_list_t *f);
aframe_list_t *aframe_retrieve_status(int old_status, int new_status);
void aframe_set_status(aframe_list_t *ptr, int status);
int aframe_alloc(int num);
void aframe_free(void);
void aframe_flush(void);
int aframe_fill_level(int status);
void aframe_fill_print(int r);

extern pthread_mutex_t aframe_list_lock;
extern pthread_cond_t aframe_list_full_cv;
extern aframe_list_t *aframe_list_head;
extern aframe_list_t *aframe_list_tail;

#define FINFO printf("(%s@%d) w=%d h=%d size=%d\n", __FILE__, __LINE__, ptr->v_width, ptr->v_height, ptr->video_size);

#endif
