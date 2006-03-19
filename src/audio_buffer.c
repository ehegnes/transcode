/*
 *  audio_buffer.c
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

#include "transcode.h"
#include "framebuffer.h"
#include "frame_threads.h"

pthread_mutex_t aframe_list_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t aframe_list_full_cv = PTHREAD_COND_INITIALIZER;

aframe_list_t *aframe_list_head;
aframe_list_t *aframe_list_tail;

static int aud_buf_max = 0;
static int aud_buf_next = 0;

static int aud_buf_fill = 0;
static int aud_buf_ready = 0;
static int aud_buf_locked = 0;
static int aud_buf_empty = 0;
static int aud_buf_wait = 0;

static aframe_list_t **aud_buf_ptr;
int8_t *aud_buf_mem;

/* ------------------------------------------------------------------ */

/* objectives:
   ===========

   allocate memory for ringbuffer structure
   return -1 on failure, 0 on success

*/

int aframe_alloc(int ex_num)
{
    int n = 0;

    if (ex_num < 0) {
        return -1;
    }
    ex_num++; /* alloc at least one buffer */

    aud_buf_ptr = tc_malloc(ex_num * sizeof(aframe_list_t *));
    if (aud_buf_ptr == NULL) {
        return -1;
    }

    aud_buf_mem = tc_malloc(ex_num * sizeof(aframe_list_t));
    if (aud_buf_mem == NULL) {
        return -1;
    }

    /* init ringbuffer */
    for (n = 0; n < ex_num; n++) {
        aud_buf_ptr[n] = (aframe_list_t *) (aud_buf_mem + n * sizeof(aframe_list_t));

        aud_buf_ptr[n]->status = FRAME_NULL;
    	aud_buf_ptr[n]->bufid = n;

	    /* allocate extra audio memory: */
	    aud_buf_ptr[n]->internal_audio_buf = tc_bufalloc(SIZE_PCM_FRAME);
        if (aud_buf_ptr[n]->internal_audio_buf == NULL) {
	        return -1;
        }

        AFRAME_INIT(aud_buf_ptr[n]);
    }
    /* assign to static */
    aud_buf_max = ex_num;
    return 0;
}



/* ------------------------------------------------------------------ */

/* objectives:
   ===========

   free memory for ringbuffer structure

*/

void aframe_free(void)
{
    int n = 0;

    if (aud_buf_max > 0) {
        for (n = 0; n < aud_buf_max; n++) {
              tc_buffree(aud_buf_ptr[n]->audio_buf);
        }
        free(aud_buf_mem);
        free(aud_buf_ptr);
    }
}

/* ------------------------------------------------------------------ */

/* objectives:
   ===========

   retrieve a valid pointer to a aframe_list_t structure
   return NULL on failure, valid pointer on success

   thread safe

*/

static aframe_list_t *aud_buf_retrieve(void)
{
    int i = 0;
    aframe_list_t *ptr = aud_buf_ptr[aud_buf_next];

    /* find an unused ptr, the next one may already be busy */
    while (ptr->status != FRAME_NULL && i < aud_buf_max) {
    	i++;
	    aud_buf_next++;
    	aud_buf_next %= aud_buf_max;
	    ptr = aud_buf_ptr[aud_buf_next];
    }

    /* check, if this structure is really free to reuse */
    if (ptr->status != FRAME_NULL) {
        if (verbose & TC_FLIST) {
            tc_log_warn(__FILE__, "A buffer=%d not empty", ptr->status);
        }
        return NULL;
    }

    /* ok */
    if (verbose & TC_FLIST) {
        tc_log_info(__FILE__, "A alloc  =%d [%d]",
                              aud_buf_next, ptr->bufid);
    }

    aud_buf_next++;
    aud_buf_next %= aud_buf_max;
    return ptr;
}



/* ------------------------------------------------------------------ */

/* objectives:
   ===========

   release a valid pointer to a aframe_list_t structure
   return -1 on failure, 0 on success

   thread safe

*/

static int aud_buf_release(aframe_list_t *ptr)
{
    /*
     * instead of freeing the memory and setting the pointer
     * to NULL we only change a flag
     */
    if (ptr == NULL) {
        return -1;
    }
    if (ptr->status != FRAME_EMPTY) {
        tc_log_warn(__FILE__, "A internal error (%d)", ptr->status);
        return -1;
    } else {
        if (verbose & TC_FLIST) {
            tc_log_info(__FILE__, "A release=%d [%d]",
                                  aud_buf_next, ptr->bufid);
        }
        ptr->status = FRAME_NULL;
    }
    return 0;
}


/* ------------------------------------------------------------------ */

/* objectives:
   ===========

   register new frame
   allocate space for frame buffer and establish backward reference

   requirements:
   =============

   thread-safe
   global mutex: aframe_list_lock

*/

aframe_list_t *aframe_register(int id)
{
    aframe_list_t *ptr = NULL;

    pthread_mutex_lock(&aframe_list_lock);

    /* retrive a valid pointer from the pool */
#ifdef STATBUFFER
    if (verbose & TC_FLIST) {
        tc_log_info(__FILE__, "A frameid=%d", id);
    }
    ptr = aud_buf_retrieve();
#else
    ptr = malloc(sizeof(aframe_list_t));
#endif
    if (ptr != NULL) {
        aud_buf_empty++;
        ptr->status = FRAME_EMPTY;
        ptr->next = NULL;
        ptr->prev = NULL;
        ptr->id  = id;

        if (aframe_list_tail != NULL) {
            aframe_list_tail->next = ptr;
            ptr->prev = aframe_list_tail;
        }

        aframe_list_tail = ptr;

        /* first frame registered must set aframe_list_head */
        if (aframe_list_head == NULL) {
            aframe_list_head = ptr;
        }

        /* adjust fill level */
        aud_buf_fill++;

        if (verbose & TC_FLIST) {
            tc_log_msg(__FILE__, "A+  f=%d e=%d w=%d l=%d r=%d",
                                 aud_buf_fill, aud_buf_empty, aud_buf_wait,
                                 aud_buf_locked, aud_buf_ready);
        }

    }
    pthread_mutex_unlock(&aframe_list_lock);
    return ptr;
}

/* ------------------------------------------------------------------ */

void aframe_copy(aframe_list_t *dst, aframe_list_t *src, int copy_data)
{
    if (!dst || !src) {
    	return;
    }

    /*
     * we can't use memcpy here because we don't want
     * to overwrite the pointers to alloc'ed mem
     */

    dst->bufid = src->bufid;
    dst->tag = src->tag;
    dst->filter_id = src->filter_id;
    dst->a_codec = src->a_codec;
    dst->id = src->id;
    dst->status = src->status;
    dst->attributes = src->attributes;
    dst->thread_id = src->thread_id;
    dst->a_rate = src->a_rate;
    dst->a_bits = src->a_bits;
    dst->a_chan = src->a_chan;
    dst->audio_size = src->audio_size;

    if (copy_data == 1) {
        /* really copy video data */
        ac_memcpy(dst->audio_buf, src->audio_buf, dst->audio_size);
    } else {
        /* soft copy, new frame points to old audio data */
        dst->audio_buf = src->audio_buf;
    }
}

/* ------------------------------------------------------------------ */

/* objectives:
   ===========

   duplicate a frame (for cloning)
   insert a ptr after f;

   requirements:
   =============

   thread-safe
   global mutex: vframe_list_lock

*/

aframe_list_t *aframe_dup(aframe_list_t *f)
{
    aframe_list_t *ptr = NULL;

    if (f == NULL) {
        if (verbose & TC_FLIST) {
            tc_log_warn(__FILE__, "aframe_dup: empty frame");
        }
        return NULL;
    }

    pthread_mutex_lock(&aframe_list_lock);
    /* retrieve a valid pointer from the pool */
#ifdef STATBUFFER
    ptr = aud_buf_retrieve();
#else
    ptr = malloc(sizeof(aframe_list_t));
#endif
    if (ptr != NULL) {
        aframe_copy(ptr, f, 1);

        ptr->status = FRAME_WAIT;
        aud_buf_wait++;

        ptr->next = NULL;
        ptr->prev = NULL;

        /* insert after ptr */
        ptr->next = f->next;
        f->next = ptr;
        ptr->prev = f;

        if (ptr->next == NULL) {
            /* must be last ptr in the list */
            aframe_list_tail = ptr;
        }

        /* adjust fill level */
        aud_buf_fill++;
#ifdef STATBUFFER
    } else { /* ptr == NULL */
        if (verbose & TC_FLIST) {
            tc_log_warn(__FILE__, "aframe_dup: cannot find a free slot"
                                  " (%d)", f->id);
        }
#endif
    }
    pthread_mutex_unlock(&aframe_list_lock);
    return ptr;
}

/* ------------------------------------------------------------------ */

/* objectives:
===========

remove frame from chained list

requirements:
=============

thread-safe

*/

void aframe_remove(aframe_list_t *ptr)
{
    if (ptr != NULL) {
        pthread_mutex_lock(&aframe_list_lock);

        if (ptr->prev != NULL) {
            (ptr->prev)->next = ptr->next;
        }
        if (ptr->next != NULL) {
            (ptr->next)->prev = ptr->prev;
        }

        if (ptr == aframe_list_tail) {
            aframe_list_tail = ptr->prev;
        }
        if (ptr == aframe_list_head) {
            aframe_list_head = ptr->next;
        }

        if (ptr->status == FRAME_READY) {
            aud_buf_ready--;
        }
        if (ptr->status == FRAME_LOCKED) {
            aud_buf_locked--;
        }

        /* release valid pointer to pool */
        ptr->status = FRAME_EMPTY;
        aud_buf_empty++;

#ifdef STATBUFFER
        aud_buf_release(ptr);
#else
        free(ptr);
#endif
        /* adjust fill level */
        aud_buf_empty--;
        aud_buf_fill--;

        if (verbose & TC_FLIST) {
            tc_log_msg(__FILE__, "A-  f=%d e=%d w=%d l=%d r=%d",
                                 aud_buf_fill, aud_buf_empty, aud_buf_wait,
                                 aud_buf_locked, aud_buf_ready);
        }

        pthread_mutex_unlock(&aframe_list_lock);
    }
}

/* ------------------------------------------------------------------ */

/* objectives:
   ===========

   remove all frame from chained list

   requirements:
   =============

   thread-safe

*/

void aframe_flush()
{
    aframe_list_t *ptr = NULL;
    int i = 0;

    while ((ptr=aframe_retrieve()) != NULL) {
        if (verbose & TC_STATS) {
            tc_log_msg(__FILE__, "flushing audio buffers");
        }
        aframe_remove(ptr);
        i++;
    }

    if (verbose & TC_DEBUG) {
        tc_log_info(__FILE__, "flushing %d audio buffer", i);
    }

    pthread_mutex_lock(&abuffer_im_fill_lock);
    abuffer_im_fill_ctr = 0;
    pthread_mutex_unlock(&abuffer_im_fill_lock);

    pthread_mutex_lock(&abuffer_ex_fill_lock);
    abuffer_ex_fill_ctr = 0;
    pthread_mutex_unlock(&abuffer_ex_fill_lock);

    pthread_mutex_lock(&abuffer_xx_fill_lock);
    abuffer_xx_fill_ctr = 0;
    pthread_mutex_unlock(&abuffer_xx_fill_lock);
}


/* ------------------------------------------------------------------ */

/* objectives:
   ===========

   get pointer to next frame for rendering

   requirements:
   =============

   thread-safe

*/

aframe_list_t *aframe_retrieve()
{
    aframe_list_t *ptr = NULL;

    pthread_mutex_lock(&aframe_list_lock);
    ptr = aframe_list_head;

    for (; ptr != NULL; ptr = ptr->next) {
        /*
         * we cannot skip a locked frame, since
         * we have to preserve order in which frames are encoded
         */
        if (ptr->status == FRAME_LOCKED) {
            ptr = NULL;
            break;
        }

        /* this frame is ready to go */
        if (ptr->status == FRAME_READY) {
            break;
        }
    }

    pthread_mutex_unlock(&aframe_list_lock);
    return ptr;
}

/* ------------------------------------------------------------------ */

#define DEC_COUNTERS(status) \
        do { \
            if ((status) == FRAME_READY) { \
                aud_buf_ready--; \
            } \
            if ((status) == FRAME_LOCKED) { \
                aud_buf_locked--; \
            } \
            if ((status) == FRAME_WAIT) { \
                aud_buf_wait--; \
            } \
        } while(0)

#define INC_COUNTERS(status) \
        do { \
            if ((status) == FRAME_READY) { \
                aud_buf_ready++; \
            } \
            if ((status) == FRAME_LOCKED) { \
                aud_buf_locked++; \
            } \
            if ((status) == FRAME_WAIT) { \
                aud_buf_wait++; \
            } \
        } while(0)


/* objectives:
   ===========

   get pointer to next frame for rendering

   requirements:
   =============

   thread-safe

*/

aframe_list_t *aframe_retrieve_status(int old_status, int new_status)
{
    aframe_list_t *ptr = NULL;

    pthread_mutex_lock(&aframe_list_lock);
    ptr = aframe_list_head;

    /* move along the chain and check for status */
    for (; ptr != NULL; ptr = ptr->next) {
        if (ptr->status == old_status) {
            /* found matching frame */
            DEC_COUNTERS(ptr->status);      
            ptr->status = new_status;
            INC_COUNTERS(ptr->status);
            break;
        }
    }

    pthread_mutex_unlock(&aframe_list_lock);
    return ptr;
}


/* ------------------------------------------------------------------ */

/* objectives:
   ===========

   get pointer to next frame for rendering

   requirements:
   =============

*/

void aframe_set_status(aframe_list_t *ptr, int status)
{
    if (ptr != NULL) {
        pthread_mutex_lock(&aframe_list_lock);

        DEC_COUNTERS(ptr->status);      
        if (ptr->status == FRAME_EMPTY) {
            aud_buf_empty--;
        }
        ptr->status = status;
        INC_COUNTERS(ptr->status);
        if (ptr->status == FRAME_EMPTY) {
            aud_buf_empty++;
        }

        pthread_mutex_unlock(&aframe_list_lock);
    }
}

/* ------------------------------------------------------------------ */

void aframe_fill_print(int r)
{
    tc_log_msg(__FILE__, "(A) fill=%d/%d, empty=%d wait=%d locked=%d,"
                         " ready=%d, tag=%d",
                         aud_buf_fill, aud_buf_max, aud_buf_empty,
                         aud_buf_wait, aud_buf_locked, aud_buf_ready, r);
}


/* ------------------------------------------------------------------ */


int aframe_fill_level(int status)
{
    if (verbose & TC_STATS) {
        aframe_fill_print(status);
    }

    /* user has to lock aframe_list_lock to obtain a proper result */

    if (status == TC_BUFFER_FULL  && aud_buf_fill >= aud_buf_max-1) {
        return 1;
    }
    if (status == TC_BUFFER_READY && aud_buf_ready > 0) {
        return 1;
    }
    if (status == TC_BUFFER_EMPTY && aud_buf_fill == 0) {
        return 1;
    }
    if (status == TC_BUFFER_LOCKED && aud_buf_locked > 0) {
        return 1;
    }
    return 0;
}
