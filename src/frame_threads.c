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

#include <pthread.h>

#include "transcode.h"
#include "framebuffer.h"
#include "video_trans.h"
#include "audio_trans.h"
#include "decoder.h"
#include "filter.h"

#include "frame_threads.h"
#include "encoder.h"

/********* prototypes ****************************************************/

static void *process_video_frame(void *_vob);
static void *process_audio_frame(void *_vob);

/*************************************************************************/

static pthread_t afthread[TC_FRAME_THREADS_MAX];
static pthread_t vfthread[TC_FRAME_THREADS_MAX];

static int aframe_workers_count = 0;
static int vframe_workers_count = 0;


/*************************************************************************/

typedef struct tcstopmarker_ TCStopMarker;
struct tcstopmarker_ {
    pthread_mutex_t lock;
    volatile int flag;
};

TCStopMarker audio_stop = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .flag = TC_FALSE,
};
TCStopMarker video_stop = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .flag = TC_FALSE,
};

static void set_stop_flag(TCStopMarker *mark)
{
    pthread_mutex_lock(&mark->lock);
    mark->flag = TC_TRUE;
    pthread_mutex_unlock(&mark->lock);
}

static int get_stop_flag(TCStopMarker *mark)
{
    int ret;
    pthread_mutex_lock(&mark->lock);
    ret = mark->flag;
    pthread_mutex_unlock(&mark->lock);
    return ret;
}

static int stop_requested(TCStopMarker *mark)
{
    return (!tc_running() || get_stop_flag(mark));
}

/*************************************************************************/


/*************************************************************************/


int tc_frame_threads_have_video_workers(void)
{
    return (vframe_workers_count > 0);
}

int tc_frame_threads_have_audio_workers(void)
{
    return (aframe_workers_count > 0);
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
    vframe_workers_count = vworkers;

    if (vworkers > 0) {
        if (verbose >= TC_DEBUG)
            tc_log_info(__FILE__, "starting %i video frame"
                                 " processing thread(s)", vworkers);

        // start the thread pool
        for (n = 0; n < vworkers; n++) {
            if (pthread_create(&vfthread[n], NULL, process_video_frame, vob) != 0)
                tc_error("failed to start video frame processing thread");
        }
    }

    //audio
    aframe_workers_count = aworkers;

    if (aworkers > 0) {
        if (verbose >= TC_DEBUG)
            tc_log_info(__FILE__, "starting %i audio frame"
                                 " processing thread(s)", aworkers);

        // start the thread pool
        for (n = 0; n < aworkers; n++) {
            if (pthread_create(&afthread[n], NULL, process_audio_frame, vob) != 0)
                tc_error("failed to start audio frame processing thread");
        }
    }
}

void tc_frame_threads_close(void)
{
    int n = 0;
    void *status = NULL;

    // audio
    if (aframe_workers_count > 0) {
        set_stop_flag(&audio_stop);
        if (verbose >= TC_CLEANUP)
            tc_log_msg(__FILE__, "wait for %i audio frame processing threads",
                       aframe_workers_count);
        for (n = 0; n < aframe_workers_count; n++)
            pthread_join(afthread[n], &status);
        if (verbose >= TC_CLEANUP)
            tc_log_msg(__FILE__, "audio frame processing threads canceled");
    }

    //video
    if (vframe_workers_count > 0) {
        set_stop_flag(&video_stop);
        if (verbose >= TC_CLEANUP)
            tc_log_msg(__FILE__, "wait for %i video frame processing threads",
                       vframe_workers_count);
        for (n = 0; n < vframe_workers_count; n++)
            pthread_join(vfthread[n], &status);
        if (verbose >= TC_CLEANUP)
            tc_log_msg(__FILE__, "video frame processing threads canceled");
    }
}

#define DUP_vptr_if_cloned(vptr) do { \
    if(vptr->attributes & TC_FRAME_IS_CLONED) { \
        vframe_list_t *tmptr = vframe_dup(vptr); \
        \
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
        vframe_push_next(tmptr, TC_FRAME_WAIT); \
        \
    } \
} while (0)



#define DUP_aptr_if_cloned(aptr) do { \
    if(aptr->attributes & TC_FRAME_IS_CLONED) {  \
        aframe_list_t *tmptr = aframe_dup(aptr);  \
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
        aframe_push_next(tmptr, TC_FRAME_WAIT);  \
        \
    }  \
} while (0)


#define SET_STOP_FLAG(MARKP, MSG) do { \
    if (verbose >= TC_CLEANUP) \
        tc_log_msg(__FILE__, "%s", (MSG)); \
    set_stop_flag((MARKP)); \
} while (0)

static void *process_video_frame(void *_vob)
{
    static int res = 0; // XXX
    vframe_list_t *ptr = NULL;
    vob_t *vob = _vob;

    while (!stop_requested(&video_stop)) {
        ptr = vframe_reserve();
        if (ptr == NULL) {
            SET_STOP_FLAG(&video_stop, "video interrupted: exiting!");
            res = 1;
            break;
        }
        if (ptr->attributes & TC_FRAME_IS_END_OF_STREAM) {
            SET_STOP_FLAG(&video_stop, "video stream end: marking!");
        }
 
        if (ptr->attributes & TC_FRAME_IS_SKIPPED) {
            vframe_remove(ptr);  /* release frame buffer memory */
            continue;
        }

        if (TC_FRAME_NEED_PROCESSING(ptr)) {
            // external plugin pre-processing
            ptr->tag = TC_VIDEO|TC_PRE_M_PROCESS;
            tc_filter_process((frame_list_t *)ptr);

            if (ptr->attributes & TC_FRAME_IS_SKIPPED) {
                vframe_remove(ptr);  /* release frame buffer memory */
                continue;
            }

            // clone if the filter told us to do so.
            DUP_vptr_if_cloned(ptr);

            // internal processing of video
            ptr->tag = TC_VIDEO;
            process_vid_frame(vob, ptr);

            // external plugin post-processing
            ptr->tag = TC_VIDEO|TC_POST_M_PROCESS;
            tc_filter_process((frame_list_t *)ptr);

            if (ptr->attributes & TC_FRAME_IS_SKIPPED) {
                vframe_remove(ptr);  /* release frame buffer memory */
                continue;
            }
        }

        vframe_push_next(ptr, TC_FRAME_READY);
    }
    if (verbose >= TC_CLEANUP)
        tc_log_msg(__FILE__, "video stream end: got, so exiting!");
           
    pthread_exit(&res);
    return NULL;
}


static void *process_audio_frame(void *_vob)
{
    static int res = 0; // XXX
    aframe_list_t *ptr = NULL;
    vob_t *vob = _vob;

    while (!stop_requested(&audio_stop)) {
        ptr = aframe_reserve();
        if (ptr == NULL) {
            SET_STOP_FLAG(&audio_stop, "audio interrupted: exiting!");
            break;
            res = 1;
        }
        if (ptr->attributes & TC_FRAME_IS_END_OF_STREAM) {
            SET_STOP_FLAG(&audio_stop, "audio stream end: marking!");
        }
 
        if (ptr->attributes & TC_FRAME_IS_SKIPPED) {
            aframe_remove(ptr);  /* release frame buffer memory */
            continue;
        }

        if (TC_FRAME_NEED_PROCESSING(ptr)) {
            // external plugin pre-processing
            ptr->tag = TC_AUDIO|TC_PRE_M_PROCESS;
            tc_filter_process((frame_list_t *)ptr);

            DUP_aptr_if_cloned(ptr);

            if (ptr->attributes & TC_FRAME_IS_SKIPPED) {
                aframe_remove(ptr);  /* release frame buffer memory */
                continue;
            }

            // internal processing of audio
            ptr->tag = TC_AUDIO;
            process_aud_frame(vob, ptr);

            // external plugin post-processing
            ptr->tag = TC_AUDIO|TC_POST_M_PROCESS;
            tc_filter_process((frame_list_t *)ptr);

            if (ptr->attributes & TC_FRAME_IS_SKIPPED) {
                aframe_remove(ptr);  /* release frame buffer memory */
                continue;
            }
        }

        aframe_push_next(ptr, TC_FRAME_READY);
    }
    if (verbose >= TC_CLEANUP)
        tc_log_msg(__FILE__, "audio stream end: got, so exiting!");

    pthread_exit(&res);
    return NULL;
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
