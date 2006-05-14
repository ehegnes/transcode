/*
 *  video_buffer.c
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

int tc_frame_width_max = 0;
int tc_frame_height_max = 0;

pthread_mutex_t vframe_list_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t vframe_list_full_cv = PTHREAD_COND_INITIALIZER;

vframe_list_t *vframe_list_head;
vframe_list_t *vframe_list_tail;

static int vid_buf_max = 0;
static int vid_buf_next = 0;

static int vid_buf_fill = 0;
static int vid_buf_ready = 0;
static int vid_buf_locked = 0;
static int vid_buf_empty = 0;
static int vid_buf_wait = 0;

static vframe_list_t **vid_buf_ptr;
static int8_t *vid_buf_mem;

/* ------------------------------------------------------------------ */

/* objectives:
   ===========

   allocate memory for ringbuffer structure
   return -1 on failure, 0 on success

*/

int vframe_alloc(int ex_num)
{
    int n = 0;
    int frame_size_max = (tc_frame_width_max * tc_frame_height_max) * BPP/8;

    if (ex_num < 0) {
        return -1;
    }
    ex_num++; /* alloc at least one buffer */

    vid_buf_ptr = tc_malloc(ex_num * sizeof(vframe_list_t *));
    if (vid_buf_ptr == NULL) {
        return -1;
    }

    vid_buf_mem = tc_malloc(ex_num * sizeof(vframe_list_t));
    if (vid_buf_mem == NULL) {
        return(-1);
    }

    /* init ringbuffer */
    for (n = 0; n < ex_num; n++) {
    	vid_buf_ptr[n] = (vframe_list_t *)(vid_buf_mem + n * sizeof(vframe_list_t));
        vid_buf_ptr[n]->status = FRAME_NULL;
        vid_buf_ptr[n]->bufid = n;

        /* allocate extra video memory: */
	    vid_buf_ptr[n]->internal_video_buf_0 = tc_bufalloc(frame_size_max);
    	if (vid_buf_ptr[n]->internal_video_buf_0 == NULL) {
    	    return -1;
        }

        vid_buf_ptr[n]->internal_video_buf_1 = tc_bufalloc(frame_size_max);
    	if (vid_buf_ptr[n]->internal_video_buf_1 == NULL) {
    	    return -1;
    	}

        VFRAME_INIT(vid_buf_ptr[n], tc_frame_width_max, tc_frame_height_max);

    	vid_buf_ptr[n]->free = 1;
    }
    vid_buf_max = ex_num;

    return 0;
}




/* ------------------------------------------------------------------ */

/* objectives:
   ===========

   free memory for ringbuffer structure

*/

void vframe_free(void)
{
    int n;

    if (vid_buf_max > 0) {
        for (n = 0; n < vid_buf_max; n++) {
            tc_buffree(vid_buf_ptr[n]->internal_video_buf_0);
            tc_buffree(vid_buf_ptr[n]->internal_video_buf_1);
        }
        free(vid_buf_mem);
        free(vid_buf_ptr);
    }
}

/* ------------------------------------------------------------------ */

/* objectives:
   ===========

   retrieve a valid pointer to a vframe_list_t structure
   return NULL on failure, valid pointer on success

   thread safe

*/

static vframe_list_t *vid_buf_retrieve(void)
{
    vframe_list_t *ptr = vid_buf_ptr[vid_buf_next];
    int i = 0;

    /* find an unused ptr, the next one may already be busy */
    while (ptr->status != FRAME_NULL && i < vid_buf_max) {
    	++i;
	    ++vid_buf_next;
    	vid_buf_next %= vid_buf_max;
	    ptr = vid_buf_ptr[vid_buf_next];
    }

    /* check, if this structure is really free to reuse */
    if (ptr->status != FRAME_NULL) {
        if (verbose & TC_FLIST) {
            tc_log_warn(__FILE__, "buffer=%d (at %p) not empty",
                        ptr->status, ptr);
            /* dump ptr list */
            for (i = 0; i < vid_buf_max; i++) {
                tc_log_warn(__FILE__, "  (%02d) %p<-%p->%p %d %03d", i,
                            vid_buf_ptr[i]->prev, vid_buf_ptr[i],
                            vid_buf_ptr[i]->next,
                            vid_buf_ptr[i]->status,
                            vid_buf_ptr[i]->id);
            }
        }
        return NULL;
    }

    /* ok */
    if (verbose & TC_FLIST) {
        tc_log_info(__FILE__, "alloc = %d [%d]", vid_buf_next, ptr->bufid);
    }

    vid_buf_next++;
    vid_buf_next %= vid_buf_max;
    return ptr;
}



/* ------------------------------------------------------------------ */

/* objectives:
   ===========

   release a valid pointer to a vframe_list_t structure
   return -1 on failure, 0 on success

  thread safe

*/

static int vid_buf_release(vframe_list_t *ptr)
{
    /*
     * instead of freeing the memory and setting the pointer
     * to NULL we only change a flag
     */
    if (ptr == NULL || ptr->status != FRAME_EMPTY) {
        return -1;
    } else {
        if (verbose & TC_FLIST) {
            tc_log_info(__FILE__, "release=%d [%d]",
                        vid_buf_next, ptr->bufid);
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
   global mutex: vframe_list_lock

*/

vframe_list_t *vframe_register(int id)
{
    vframe_list_t *ptr = NULL;
    pthread_mutex_lock(&vframe_list_lock);

    /* retrive a valid pointer from the pool */
#ifdef STATBUFFER
    if (verbose & TC_FLIST) {
        tc_log_info(__FILE__, "vframe_register: frameid=%d", id);
    }
    ptr = vid_buf_retrieve();
#else
    ptr = tc_malloc(sizeof(vframe_list_t));
#endif
    if (ptr != NULL) {
        vid_buf_empty++;
        ptr->status = FRAME_EMPTY;
        ptr->next = NULL;
        ptr->prev = NULL;
        ptr->id  = id;
        ptr->clone_flag = 0;

        if (vframe_list_tail != NULL) {
            vframe_list_tail->next = ptr;
            ptr->prev = vframe_list_tail;
        }
        vframe_list_tail = ptr;

        /* first frame registered must set vframe_list_head */
        if (vframe_list_head == NULL) {
            vframe_list_head = ptr;
        }

        // adjust fill level
        vid_buf_fill++;
    }
    pthread_mutex_unlock(&vframe_list_lock);
    return ptr;
}

/* ------------------------------------------------------------------ */

void vframe_copy(vframe_list_t *dst, vframe_list_t *src, int copy_data)
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
    dst->v_codec = src->v_codec;
    dst->id = src->id;
    dst->status = src->status;
    dst->attributes = src->attributes;
    dst->thread_id = src->thread_id;
    dst->clone_flag = src->clone_flag;
    dst->deinter_flag = src->deinter_flag;
    dst->v_width = src->v_width;
    dst->v_height = src->v_height;
    dst->v_bpp = src->v_bpp;
    dst->video_size = src->video_size;
    dst->plane_mode = src->plane_mode;

    if (copy_data == 1) {
        /* really copy video data */
        ac_memcpy(dst->video_buf, src->video_buf, dst->video_size);
        ac_memcpy(dst->video_buf2, src->video_buf2, dst->video_size);
    } else {
        /* soft copy, new frame points to old video data */
        dst->video_buf = src->video_buf;
        dst->video_buf2 = src->video_buf2;
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

vframe_list_t *vframe_dup(vframe_list_t *f)
{
    vframe_list_t *ptr = NULL;

    if (f == NULL) {
        if (verbose & TC_FLIST) {
            tc_log_warn(__FILE__, "vframe_dup: empty frame");
        }
        return NULL;
    }

    pthread_mutex_lock(&vframe_list_lock);
    /* retrieve a valid pointer from the pool */
#ifdef STATBUFFER
    ptr = vid_buf_retrieve();
#else
    ptr = tc_malloc(sizeof(vframe_list_t));
#endif
    if (ptr != NULL) {
        vframe_copy (ptr, f, 1);
    
        vid_buf_wait++;
        ptr->status = FRAME_WAIT;
        ptr->next = NULL;
        ptr->prev = NULL;
        /* currently noone cares about this */
        ptr->clone_flag = f->clone_flag+1;

        /* insert after ptr */
        ptr->next = f->next;
        f->next = ptr;
        ptr->prev = f;

        if (!ptr->next) {
            /* must be last ptr in the list */
            vframe_list_tail = ptr;
        }

        /* adjust fill level */
        vid_buf_fill++;
#ifdef STATBUFFER
    } else { /* ptr == NULL */
        if (verbose & TC_FLIST) {
            tc_log_warn(__FILE__, "vframe_dup: cannot find a free slot"
                                  " (%d)", f->id);
        }
#endif
    }
    pthread_mutex_unlock(&vframe_list_lock);
    return ptr;
}


/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */

/* objectives:
   ===========

   remove frame from chained list

   requirements:
   =============

   thread-safe

*/

void vframe_remove(vframe_list_t *ptr)
{
    if (ptr != NULL) {
        pthread_mutex_lock(&vframe_list_lock);
        if (ptr->prev != NULL) {
            (ptr->prev)->next = ptr->next;
        }
        if (ptr->next != NULL) {
            (ptr->next)->prev = ptr->prev;
        }
        if (ptr == vframe_list_tail) {
            vframe_list_tail = ptr->prev;
        }
        if (ptr == vframe_list_head) {
            vframe_list_head = ptr->next;
        }
        if (ptr->status == FRAME_READY) {
            vid_buf_ready--;
        }
        if (ptr->status == FRAME_LOCKED) {
            vid_buf_locked--;
        }
        /* release valid pointer to pool */
        ptr->status = FRAME_EMPTY;
        vid_buf_empty++;

#ifdef STATBUFFER
        vid_buf_release(ptr);
#else
        tc_free(ptr);
#endif
        /* adjust fill level */
        vid_buf_fill--;
        vid_buf_empty--;
        pthread_mutex_unlock(&vframe_list_lock);
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

void vframe_flush()
{
    vframe_list_t *ptr = NULL;
    int i = 0;

    while ((ptr = vframe_retrieve()) != NULL) {
        if (verbose & TC_STATS) {
            tc_log_msg(__FILE__, "flushing video buffers (%d)", ptr->id);
        }
        vframe_remove(ptr);
        i++;
    }

    if (verbose & TC_DEBUG) {
        tc_log_msg(__FILE__, "flushing %d video buffer", i);
    }

    pthread_mutex_lock(&vbuffer_im_fill_lock);
    vbuffer_im_fill_ctr = 0;
    pthread_mutex_unlock(&vbuffer_im_fill_lock);

    pthread_mutex_lock(&vbuffer_ex_fill_lock);
    vbuffer_ex_fill_ctr = 0;
    pthread_mutex_unlock(&vbuffer_ex_fill_lock);

    pthread_mutex_lock(&vbuffer_xx_fill_lock);
    vbuffer_xx_fill_ctr = 0;
    pthread_mutex_unlock(&vbuffer_xx_fill_lock);
}


/* ------------------------------------------------------------------ */

/* objectives:
   ===========

   get pointer to next frame for rendering

   requirements:
   =============

   thread-safe

*/

vframe_list_t *vframe_retrieve()
{
    vframe_list_t *ptr = NULL;

    pthread_mutex_lock(&vframe_list_lock);
    ptr = vframe_list_head;
    /* move along the chain and check for status */
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
    pthread_mutex_unlock(&vframe_list_lock);
    return ptr;
}

/* ------------------------------------------------------------------ */

#define DEC_COUNTERS(status) \
        do { \
            if ((status) == FRAME_READY) { \
                vid_buf_ready--; \
            } \
            if ((status) == FRAME_LOCKED) { \
                vid_buf_locked--; \
            } \
            if ((status) == FRAME_WAIT) { \
                vid_buf_wait--; \
            } \
        } while(0)

#define INC_COUNTERS(status) \
        do { \
            if ((status) == FRAME_READY) { \
                vid_buf_ready++; \
            } \
            if ((status) == FRAME_LOCKED) { \
                vid_buf_locked++; \
            } \
            if ((status) == FRAME_WAIT) { \
                vid_buf_wait++; \
            } \
        } while(0)

/* objectives:
   ===========

   get pointer to next frame for rendering

   requirements:
   =============

   thread-safe

*/

vframe_list_t *vframe_retrieve_status(int old_status, int new_status)
{
    vframe_list_t *ptr = NULL;

    pthread_mutex_lock(&vframe_list_lock);
    ptr = vframe_list_head;

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
    
    pthread_mutex_unlock(&vframe_list_lock);
    return ptr;
}


/* ------------------------------------------------------------------ */

/* objectives:
   ===========

   get pointer to next frame for rendering

   requirements:
   =============

   thread-safe

*/

void vframe_set_status(vframe_list_t *ptr, int status)
{
    if (ptr != NULL) {
        pthread_mutex_lock(&vframe_list_lock);

        DEC_COUNTERS(ptr->status);
        if (ptr->status == FRAME_EMPTY) {
            vid_buf_empty--;
        }
        ptr->status = status;
        INC_COUNTERS(ptr->status);
        if (ptr->status == FRAME_EMPTY) {
            vid_buf_empty++;
        }

        pthread_mutex_unlock(&vframe_list_lock);
    }
}


/* ------------------------------------------------------------------ */

void vframe_fill_print(int r)
{
    tc_log_msg(__FILE__, "(V) fill=%d/%d, empty=%d wait=%d locked=%d,"
                         " ready=%d, tag=%d",
                         vid_buf_fill, vid_buf_max, vid_buf_empty,
                         vid_buf_wait, vid_buf_locked, vid_buf_ready, r);
}

/* ------------------------------------------------------------------ */


int vframe_fill_level(int status)
{
    if (verbose & TC_STATS) {
        vframe_fill_print(status);
    }
    /* user has to lock vframe_list_lock to obtain a proper result */

    /*
     * we return "full" (to the decoder) even if there is one framebuffer
     * left so that frames can be cloned without running out of buffers.
     */
    if (status == TC_BUFFER_FULL  && vid_buf_fill >= vid_buf_max-1) {
        return 1;
    }
    if (status == TC_BUFFER_READY && vid_buf_ready > 0) {
        return 1;
    }
    if (status == TC_BUFFER_EMPTY && vid_buf_fill == 0) {
        return 1;
    }
    if (status == TC_BUFFER_LOCKED && vid_buf_locked > 0) {
        return 1;
    }
    return 0;
}

void tc_adjust_frame_buffer(int height, int width)
{
    if (height > tc_frame_height_max) {
        tc_frame_height_max = height;
    }
    if (width > tc_frame_width_max) {
        tc_frame_width_max = width;
    }
}
