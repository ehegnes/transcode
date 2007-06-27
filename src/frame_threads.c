/*
 *  frame_threads.c
 *
 *  Copyright (C) Thomas Oestreich - June 2001
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
#include "video_trans.h"
#include "audio_trans.h"
#include "decoder.h"
#include "filter.h"

#include "frame_threads.h"
#include "encoder.h"

/********* prototypes ****************************************************/

static void process_vframe(vob_t *vob);
static void process_aframe(vob_t *vob);

/*************************************************************************/


static pthread_t afthread[TC_FRAME_THREADS_MAX];
static pthread_t vfthread[TC_FRAME_THREADS_MAX];

static pthread_cond_t abuffer_fill_cv = PTHREAD_COND_INITIALIZER;
static pthread_cond_t vbuffer_fill_cv = PTHREAD_COND_INITIALIZER;

pthread_mutex_t abuffer_im_fill_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t abuffer_im_fill_ctr = 0;
pthread_mutex_t abuffer_xx_fill_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t abuffer_xx_fill_ctr = 0;
pthread_mutex_t abuffer_ex_fill_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t abuffer_ex_fill_ctr = 0;

pthread_mutex_t vbuffer_im_fill_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t vbuffer_im_fill_ctr = 0;
pthread_mutex_t vbuffer_xx_fill_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t vbuffer_xx_fill_ctr = 0;
pthread_mutex_t vbuffer_ex_fill_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t vbuffer_ex_fill_ctr = 0;

static int have_aframe_workers = 0;
static int aframe_threads_shutdown = 0;

static int have_vframe_workers = 0;
static int vframe_threads_shutdown = 0;



int tc_frame_threads_have_video_workers(void)
{
    return (have_vframe_workers > 0);
}

int tc_frame_threads_have_audio_workers(void)
{
    return (have_aframe_workers > 0);
}

void tc_flush_audio_counters(void)
{
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

void tc_flush_video_counters(void)
{
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

/* ------------------------------------------------------------
 *
 * frame processing threads
 *
 * ------------------------------------------------------------*/

void tc_frame_threads_init(vob_t *vob, int vworkers, int aworkers)
{
    int n = 0;

    //video
    have_vframe_workers = vworkers;

    if (vworkers > 0) {
        if (verbose & TC_DEBUG)
            tc_log_msg(PACKAGE, "starting %d frame processing thread(s)", vworkers);

        // start the thread pool
        for (n = 0; n < vworkers; n++) {
            if (pthread_create(&vfthread[n], NULL, (void*)process_vframe, vob) != 0)
                tc_error("failed to start video frame processing thread");
        }
    }

    //audio
    have_aframe_workers = aworkers;

    if (aworkers > 0) {
        if (verbose & TC_DEBUG)
            tc_log_msg(PACKAGE, "starting %d frame processing thread(s)", aworkers);

        // start the thread pool
        for (n = 0; n < aworkers; n++) {
            if (pthread_create(&afthread[n], NULL, (void*)process_aframe, vob) != 0)
                tc_error("failed to start audio frame processing thread");
        }
    }
}

void tc_frame_threads_notify_audio(int broadcast)
{
    pthread_mutex_lock(&abuffer_im_fill_lock);
    if (broadcast) {
        pthread_cond_broadcast(&abuffer_fill_cv);
    } else {
        pthread_cond_signal(&abuffer_fill_cv);
    }
    pthread_mutex_unlock(&abuffer_im_fill_lock);
}
    
void tc_frame_threads_notify_video(int broadcast)
{
    pthread_mutex_lock(&vbuffer_im_fill_lock);
    if (broadcast) {
        pthread_cond_broadcast(&vbuffer_fill_cv);
    } else {
        pthread_cond_signal(&vbuffer_fill_cv);
    }
    pthread_mutex_unlock(&vbuffer_im_fill_lock);
}


void tc_frame_threads_close()
{
    int n = 0;
    void *status = NULL;

    // audio
    if (have_aframe_workers > 0) {
        pthread_mutex_lock(&abuffer_im_fill_lock);
        aframe_threads_shutdown = 1;
        pthread_mutex_unlock(&abuffer_im_fill_lock);

        //notify all threads of shutdown
        tc_frame_threads_notify_audio(TC_TRUE);

        for (n = 0; n < have_aframe_workers; n++)
            pthread_cancel(afthread[n]);

    //wait for threads to terminate
#ifdef BROKEN_PTHREADS // Used to be MacOSX specific; kernel 2.6 as well?
        pthread_cond_broadcast(&abuffer_fill_cv);
#endif
        for (n = 0; n < have_aframe_workers; n++)
            pthread_join(afthread[n], &status);

        if (verbose & TC_DEBUG)
            tc_log_msg(PACKAGE, "audio frame processing threads canceled");
    }

    //video
    if (have_vframe_workers > 0) {
        pthread_mutex_lock(&vbuffer_im_fill_lock);
        vframe_threads_shutdown = 1;
        pthread_mutex_unlock(&vbuffer_im_fill_lock);

        //notify all threads of shutdown
        tc_frame_threads_notify_video(TC_TRUE);
        for (n = 0; n < have_vframe_workers; n++)
            pthread_cancel(vfthread[n]);

    //wait for threads to terminate
#ifdef BROKEN_PTHREADS // Used to be MacOSX specific; kernel 2.6 as well?
        pthread_cond_broadcast(&vbuffer_fill_cv);
#endif
        for (n = 0; n < have_vframe_workers; n++)
            pthread_join(vfthread[n], &status);

        if (verbose & TC_DEBUG)
            tc_log_msg(PACKAGE, "video frame processing threads canceled");
    }
}

#define DUP_vptr_if_cloned(vptr) \
    if(vptr->attributes & TC_FRAME_IS_CLONED) { \
        vframe_list_t *tmptr = vframe_dup(vptr); \
        \
        if (!tmptr) {  \
            /* No free slot free to clone ptr */ \
            /* put ptr back into working cue */ \
            vframe_set_status (vptr, FRAME_WAIT); \
            \
            pthread_mutex_lock(&vbuffer_im_fill_lock); \
            vbuffer_im_fill_ctr++; \
            pthread_mutex_unlock(&vbuffer_im_fill_lock); \
            \
            pthread_mutex_lock(&vbuffer_xx_fill_lock); \
            vbuffer_xx_fill_ctr--; \
            pthread_mutex_unlock(&vbuffer_xx_fill_lock); \
            \
            continue; \
        } else {  \
            /* ptr was successfully cloned */ \
            /* delete clone flag */ \
            tmptr->attributes &= ~TC_FRAME_IS_CLONED; \
            vptr->attributes  &= ~TC_FRAME_IS_CLONED; \
            \
            /* set info for filters */ \
            tmptr->attributes |= TC_FRAME_WAS_CLONED; \
            \
            /* this frame is to be processed _after_ the current one */ \
            /* so put it back into the queue */ \
            vframe_set_status (tmptr, FRAME_WAIT); \
            \
            pthread_mutex_lock(&vbuffer_im_fill_lock); \
            vbuffer_im_fill_ctr++; \
            pthread_mutex_unlock(&vbuffer_im_fill_lock); \
        } \
    }

#define DROP_vptr_if_skipped(ptr) \
    if(ptr->attributes & TC_FRAME_IS_SKIPPED) { \
      vframe_remove(ptr);  /* release frame buffer memory */ \
 \
      pthread_mutex_lock(&vbuffer_xx_fill_lock); \
      --vbuffer_xx_fill_ctr; \
      pthread_mutex_unlock(&vbuffer_xx_fill_lock); \
 \
      tc_import_video_notify(); \
 \
      continue; \
    }

#define DUP_aptr_if_cloned(aptr) \
    if(aptr->attributes & TC_FRAME_IS_CLONED) {  \
    aframe_list_t *tmptr = aframe_dup(aptr);  \
  \
    if (!tmptr) {  \
  \
        /* No free slot free to clone ptr */ \
  \
        /* put ptr back into working cue */ \
        aframe_set_status (aptr, FRAME_WAIT);  \
  \
        pthread_mutex_lock(&abuffer_im_fill_lock);  \
        ++abuffer_im_fill_ctr;  \
        pthread_mutex_unlock(&abuffer_im_fill_lock);  \
  \
        pthread_mutex_lock(&abuffer_xx_fill_lock);  \
        --abuffer_xx_fill_ctr;  \
        pthread_mutex_unlock(&abuffer_xx_fill_lock);  \
  \
        continue;  \
  \
    } else {  \
  \
        /* ptr was successfully cloned */ \
  \
        /* delete clone flag */ \
        tmptr->attributes &= ~TC_FRAME_IS_CLONED;  \
        aptr->attributes  &= ~TC_FRAME_IS_CLONED;  \
  \
        /* set info for filters */ \
        tmptr->attributes |= TC_FRAME_WAS_CLONED;  \
  \
        /* this frame is to be processed _after_ the current one */ \
        /* so put it back into the queue */ \
        aframe_set_status (tmptr, FRAME_WAIT);  \
  \
        pthread_mutex_lock(&abuffer_im_fill_lock);  \
        ++abuffer_im_fill_ctr;  \
        pthread_mutex_unlock(&abuffer_im_fill_lock);  \
    }  \
    }

#define DROP_aptr_if_skipped(ptr) \
    if(ptr->attributes & TC_FRAME_IS_SKIPPED) { \
      aframe_remove(ptr);  /* release frame buffer memory */ \
 \
      pthread_mutex_lock(&abuffer_xx_fill_lock); \
      --abuffer_xx_fill_ctr; \
      pthread_mutex_unlock(&abuffer_xx_fill_lock); \
 \
      tc_import_audio_notify(); \
 \
      continue; \
    }

static void process_frame_lock_cleanup (void *arg)
{
    pthread_mutex_unlock((pthread_mutex_t *)arg);
}

static void process_vframe(vob_t *vob)
{
    vframe_list_t *ptr = NULL;

    for (;;) {
        pthread_testcancel();

        pthread_mutex_lock(&vbuffer_im_fill_lock);
        pthread_cleanup_push(process_frame_lock_cleanup, &vbuffer_im_fill_lock);
        while (vbuffer_im_fill_ctr == 0) {
            pthread_cond_wait(&vbuffer_fill_cv, &vbuffer_im_fill_lock);
#ifdef BROKEN_PTHREADS // Used to be MacOSX specific; kernel 2.6 as well?
            pthread_testcancel();
#endif
            if (vframe_threads_shutdown) {
                pthread_exit(0);
            }
        }
        pthread_cleanup_pop(0);
        pthread_mutex_unlock(&vbuffer_im_fill_lock);

        ptr = vframe_retrieve_status(FRAME_WAIT, FRAME_LOCKED);
        if (ptr == NULL) {
            if (verbose & TC_DEBUG)
                tc_log_msg(PACKAGE, "internal error (V|%d)", vbuffer_im_fill_ctr);

            pthread_testcancel();
            continue;
        }

        pthread_testcancel();

        pthread_mutex_lock(&vbuffer_im_fill_lock);
        vbuffer_im_fill_ctr--;
        pthread_mutex_unlock(&vbuffer_im_fill_lock);

        pthread_mutex_lock(&vbuffer_xx_fill_lock);
        vbuffer_xx_fill_ctr++;
        pthread_mutex_unlock(&vbuffer_xx_fill_lock);

        DROP_vptr_if_skipped(ptr)

        if (TC_FRAME_NEED_PROCESSING(ptr)) {
            // external plugin pre-processing
            ptr->tag = TC_VIDEO|TC_PRE_M_PROCESS;
            tc_filter_process((frame_list_t *)ptr);

            DROP_vptr_if_skipped(ptr)

            // clone if the filter told us to do so.
            DUP_vptr_if_cloned(ptr)

            // internal processing of video
            ptr->tag = TC_VIDEO;
            process_vid_frame(vob, ptr);

            // external plugin post-processing
            ptr->tag = TC_VIDEO|TC_POST_M_PROCESS;
            tc_filter_process((frame_list_t *)ptr);

            // Won't work, because the frame is already rescaled and such
            //DUP_vptr_if_cloned(ptr);

            DROP_vptr_if_skipped(ptr)
        }

        pthread_testcancel();

        pthread_mutex_lock(&vbuffer_xx_fill_lock);
        vbuffer_xx_fill_ctr--;
        pthread_mutex_unlock(&vbuffer_xx_fill_lock);

        pthread_mutex_lock(&vbuffer_ex_fill_lock);
        vbuffer_ex_fill_ctr++;
        pthread_mutex_unlock(&vbuffer_ex_fill_lock);

        vframe_set_status(ptr, FRAME_READY);
        tc_export_video_notify();
    }
    return;
}


static void process_aframe(vob_t *vob)
{
    aframe_list_t *ptr = NULL;

    for (;;) {
        pthread_testcancel();

        pthread_mutex_lock(&abuffer_im_fill_lock);
        pthread_cleanup_push(process_frame_lock_cleanup, &abuffer_im_fill_lock);

        while (abuffer_im_fill_ctr == 0) {
            pthread_cond_wait(&abuffer_fill_cv, &abuffer_im_fill_lock);
#ifdef BROKEN_PTHREADS // Used to be MacOSX specific; kernel 2.6 as well?
            pthread_testcancel();
#endif
            if (aframe_threads_shutdown) {
                pthread_exit(0);
            }
        }

        pthread_cleanup_pop(0);
        pthread_mutex_unlock(&abuffer_im_fill_lock);

        ptr = aframe_retrieve_status(FRAME_WAIT, FRAME_LOCKED);
        if (ptr == NULL) {
            if (verbose & TC_DEBUG)
                tc_log_msg(PACKAGE, "internal error (A|%d)", abuffer_im_fill_ctr);

            pthread_testcancel();
            continue;
        }

        pthread_testcancel();

        pthread_mutex_lock(&abuffer_im_fill_lock);
        abuffer_im_fill_ctr--;
        pthread_mutex_unlock(&abuffer_im_fill_lock);

        pthread_mutex_lock(&abuffer_xx_fill_lock);
        abuffer_xx_fill_ctr++;
        pthread_mutex_unlock(&abuffer_xx_fill_lock);

        DROP_aptr_if_skipped(ptr)

        if (TC_FRAME_NEED_PROCESSING(ptr)) {
            // external plugin pre-processing
            ptr->tag = TC_AUDIO|TC_PRE_M_PROCESS;
            tc_filter_process((frame_list_t *)ptr);

            DUP_aptr_if_cloned(ptr)

            DROP_aptr_if_skipped(ptr)

            // internal processing of audio
            ptr->tag = TC_AUDIO;
            process_aud_frame(vob, ptr);

            // external plugin post-processing
            ptr->tag = TC_AUDIO|TC_POST_M_PROCESS;
            tc_filter_process((frame_list_t *)ptr);

            DROP_aptr_if_skipped(ptr)
        }

        pthread_testcancel();

        pthread_mutex_lock(&abuffer_xx_fill_lock);
        abuffer_xx_fill_ctr--;
        pthread_mutex_unlock(&abuffer_xx_fill_lock);

        pthread_mutex_lock(&abuffer_ex_fill_lock);
        abuffer_ex_fill_ctr++;
        pthread_mutex_unlock(&abuffer_ex_fill_lock);

        aframe_set_status(ptr, FRAME_READY);
        tc_export_audio_notify();
    }
    return;
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
