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

#include <stdint.h>
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

#define TC_FRAME_COMMON \
    int id;         /* FIXME: comment */ \
    int bufid;      /* buffer id */ \
    int tag;        /* init, open, close, ... */ \
    int filter_id;  /* filter instance to run */ \
    int status;     /* FIXME: comment */ \
    int attributes; /* FIXME: comment */ \
    int thread_id;


typedef struct frame_list frame_list_t;
struct frame_list {
    TC_FRAME_COMMON

    int codec;   /* codec identifier */

    int size;    /* buffer size avalaible */
    int len;     /* how much data is valid? */

    int param1; // v_width or a_rate
    int param2; // v_height or a_bits
    int param3; // v_bpp or a_chan
};


typedef struct vframe_list vframe_list_t;
struct vframe_list {
    TC_FRAME_COMMON
    /* frame physical parameter */
    
    int v_codec;       /* codec identifier */

    int video_size;    /* buffer size avalaible */
    int video_len;     /* how much data is valid? */

    int v_width;
    int v_height;
    int v_bpp;

    struct vframe_list *next;
    struct vframe_list *prev;

    int clone_flag;     
    /* set to N if frame needs to be processed (encoded) N+1 times. */
    int deinter_flag;
    /* set to N for internal de-interlacing with "-I N" */

    uint8_t *video_buf;  /* pointer to current buffer */
    uint8_t *video_buf2; /* pointer to backup buffer */

    int free; /* flag */

    uint8_t *video_buf_RGB[2];

    uint8_t *video_buf_Y[2];
    uint8_t *video_buf_U[2];
    uint8_t *video_buf_V[2];

#ifdef STATBUFFER
    uint8_t *internal_video_buf_0;
    uint8_t *internal_video_buf_1;
#else
    uint8_t internal_video_buf_0[SIZE_RGB_FRAME];
    uint8_t internal_video_buf_1[SIZE_RGB_FRAME];
#endif
};


typedef struct aframe_list aframe_list_t;
struct aframe_list {
    TC_FRAME_COMMON

    int a_codec;       /* codec identifier */

    int audio_size;    /* buffer size avalaible */
    int audio_len;     /* how much data is valid? */

    int a_rate;
    int a_bits;
    int a_chan;

    struct aframe_list *next;
    struct aframe_list *prev;

    uint8_t *audio_buf;

#ifdef STATBUFFER
    uint8_t *internal_audio_buf;
#else
    uint8_t internal_audio_buf[SIZE_PCM_FRAME<<2];
#endif
};


#define VFRAME_INIT(vptr, W, H) \
    do { \
        (vptr)->video_buf_RGB[0] = (vptr)->internal_video_buf_0; \
        (vptr)->video_buf_RGB[1] = (vptr)->internal_video_buf_1; \
        \
        (vptr)->video_buf_Y[0] = (vptr)->internal_video_buf_0; \
        (vptr)->video_buf_U[0] = (vptr)->video_buf_Y[0] + (W) * (H); \
        (vptr)->video_buf_V[0] = (vptr)->video_buf_U[0] + ((W) * (H)); \
        \
        (vptr)->video_buf_Y[1] = (vptr)->internal_video_buf_1; \
        (vptr)->video_buf_U[1] = (vptr)->video_buf_Y[1] + (W) * (H); \
        (vptr)->video_buf_V[1] = (vptr)->video_buf_U[1] + ((W) * (H)); \
        \
        (vptr)->video_buf  = (vptr)->internal_video_buf_0; \
        (vptr)->video_buf2 = (vptr)->internal_video_buf_1; \
    } while(0)

#define AFRAME_INIT(ptr) \
do { \
        ptr->audio_buf  = ptr->internal_audio_buf; \
} while(0)


vframe_list_t *vframe_register(int id);
void vframe_remove(vframe_list_t *ptr);
vframe_list_t *vframe_retrieve(void);
vframe_list_t *vframe_dup(vframe_list_t *f);
void vframe_copy(vframe_list_t *dst, vframe_list_t *src, int copy_data);
vframe_list_t *vframe_retrieve_status(int old_status, int new_status);
void vframe_set_status(vframe_list_t *ptr, int status);
int vframe_alloc(int num, int width, int height);
void vframe_free(void);
void vframe_flush(void);
int vframe_fill_level(int status);
void vframe_fill_print(int r);


aframe_list_t *aframe_register(int id);
void aframe_remove(aframe_list_t *ptr);
aframe_list_t *aframe_retrieve(void);
aframe_list_t *aframe_dup(aframe_list_t *f);
void aframe_copy(aframe_list_t *dst, aframe_list_t *src, int copy_data);
aframe_list_t *aframe_retrieve_status(int old_status, int new_status);
void aframe_set_status(aframe_list_t *ptr, int status);
int aframe_alloc(int num);
void aframe_free(void);
void aframe_flush(void);
int aframe_fill_level(int status);
void aframe_fill_print(int r);


extern pthread_mutex_t vframe_list_lock;
extern pthread_cond_t vframe_list_full_cv;
extern vframe_list_t *vframe_list_head;
extern vframe_list_t *vframe_list_tail;

extern pthread_mutex_t aframe_list_lock;
extern pthread_cond_t aframe_list_full_cv;
extern aframe_list_t *aframe_list_head;
extern aframe_list_t *aframe_list_tail;

#endif
