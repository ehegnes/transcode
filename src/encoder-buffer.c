/*
 *  encoder-buffer.c - encoder interface to transcode ring frame buffer
 *
 *  Copyright (C) Thomas Oestreich - June 2001
 *  Updated and partially rewritten by
 *  Francesco Romani - January 2006
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "transcode.h"
#include "decoder.h"
#include "encoder.h"
#include "filter.h"
#include "framebuffer.h"
#include "counter.h"
#include "video_trans.h"
#include "audio_trans.h"
#include "frame_threads.h"

#include <stdint.h>
#include <sys/types.h>

/*
 * Quick Summary:
 *
 * This code provide glue between frames ringbuffer and real encoder
 * code, in order to make encoder modular and independent from
 * a single frame source, to promote reusability (tcexport).
 * This code also take acare of some oddities like handling filtering
 * if no frame threads are avalaible.
 *
 * This code isn't clean as one (i.e.: me) would like since it
 * must cope with a lot of legacy constraints and some other nasty
 * stuff. Of course situation will be improved in future releases
 * when we keep going away from legacy oddities and continue
 * sanitize/modernize the codebase.
 */

/*
 * XXX: critical changes:
 * - lost inlining for acquire/dispose -> speed loss (unavoidable?)
 * - moved counters update into dispose -> SHOULD be harmless
 * - wrapper on exit_flag check -> harmless?
 */

/*
 * NOTE about counter/condition/mutex handling inside various
 * encoder helpers.
 *
 * Code are still a little bit confusing since things aren't
 * updated or used at the same function level.
 * Code works, but isn't still well readable.
 * We need stil more cleanup and refactoring for future releases.
 */

/* more macro magic ;) */

#define DEC_VBUF_COUNTER(BUFID) \
    pthread_mutex_lock(&vbuffer_ ## BUFID ## _fill_lock); \
    --vbuffer_ ## BUFID ## _fill_ctr; \
    pthread_mutex_unlock(&vbuffer_ ## BUFID ## _fill_lock)

#define INC_VBUF_COUNTER(BUFID) \
    pthread_mutex_lock(&vbuffer_## BUFID ## _fill_lock); \
    ++vbuffer_ ## BUFID ## _fill_ctr; \
    pthread_mutex_unlock(&vbuffer_ ## BUFID ## _fill_lock)

#define DEC_ABUF_COUNTER(BUFID) \
    pthread_mutex_lock(&abuffer_ ## BUFID ## _fill_lock); \
    --abuffer_ ## BUFID ## _fill_ctr; \
    pthread_mutex_unlock(&abuffer_ ## BUFID ## _fill_lock)

#define INC_ABUF_COUNTER(BUFID) \
    pthread_mutex_lock(&abuffer_ ## BUFID ## _fill_lock); \
    ++abuffer_ ## BUFID ## _fill_ctr; \
    pthread_mutex_unlock(&abuffer_ ## BUFID ## _fill_lock)

/*************************************************************************/
/*************************************************************************/

/* yes, we still need some globals :( */
static int have_aud_threads = 0;
static int have_vid_threads = 0;

/*************************************************************************/

/*
 * apply_video_filters. appli_auido_filters:
 *       Apply the filter chain to current respectively video
 *       or audio frame.
 *       This function is used if no frame threads are avalaible,
 *       but of course filtering still must be applied.
 *       This function should be never exported.
 *
 * Parameters:
 *       vptr, aptr: respectively video or aufio framebuffer
 *                   pointer to frame to be filtered.
 *              vob: pointer to vob_t structure holding stream
 *                   parameters.
 */
static void apply_video_filters(vframe_list_t *vptr, vob_t *vob);
static void apply_audio_filters(aframe_list_t *aptr, vob_t *vob);

/*
 * encoder_wait_vframe, encoder_wait_aframe:
 *      Wait until respectively a new video or audio frame buffer
 *      is avalaible for encoding, by querying appropriate
 *      ringbuffer.
 *      When a new frame is avalaible update the encoding context
 *      adjusting framebuffer pointer, and returns.
 *      Also returns if no more frames are avalaible: that;s usually
 *      happens when frame stream ends.
 *
 *      Yes, properly using a pthread condition would be much cleaner
 *      and simpler.
 *
 * Parameters:
 *      buf: Encoder (buffer) context to use.
 * Return Value:
 *      0: new framebuffer avalaible
 *     -1: no more framebuffers. (Stream ends?)
 */
static int encoder_wait_vframe(TCEncoderBuffer *buf);
static int encoder_wait_aframe(TCEncoderBuffer *buf);

/*
 * encoder_acquire_vframe, encoder_acquire_aframe:
 *      Get respectively a new video or audio framebuffer for encoding.
 *      This roughly means:
 *      1. to wait for a new frame avalaible for encoder using
 *         encoder_wait_vframe or encoder_wait_aframe
 *      2. apply the filters if no frame threads are avalaible
 *      3. apply the encoder filters (POST_S_PROCESS)
 *      4. verify the status of audio framebuffer after all filtering.
 *         if acquired framebuffer is skipped, we must acquire a new one
 *         before continue with encoding, so we must restart from step 1.
 *
 * Parameters:
 *      buf: encoder buffer to use (and fullfull with acquired frame).
 *      vob: pointer to vob_t structure describing stream parameters.
 *           Used Internally.
 * Return Value:
 *       0 when a new framebuffer is avalaible for encoding;
 *      <0 if no more framebuffers are avalaible.
 *         As for encoder_wait_{v,a}frame, this usually happens
 *         when video/audio stream ends.
 */
static int encoder_acquire_vframe(TCEncoderBuffer *buf, vob_t *vob);
static int encoder_acquire_aframe(TCEncoderBuffer *buf, vob_t *vob);

/*
 * encoder_dispose_vframe, encoder_dispose_aframe:
 *       Mark a framebuffer (respectively video or audio) as completed
 *       from encoder viewpoint, so release it to source ringbuffer,
 *       update counters and do all cleanup actions needed internally.
 *
 * Parameters:
 *       buf: encoder buffer to use (release currente framebuffers
 *            and update related counters and internal variables).
 * Return Value:
 *       None.
 */
static void encoder_dispose_vframe(TCEncoderBuffer *buf);
static void encoder_dispose_aframe(TCEncoderBuffer *buf);


/*************************************************************************/

static void apply_video_filters(vframe_list_t *vptr, vob_t *vob)
{
    if (!have_vid_threads) {
        DEC_VBUF_COUNTER(im);
        INC_VBUF_COUNTER(xx);

        if (TC_FRAME_NEED_PROCESSING(vptr)) {
            /* external plugin pre-processing */
            vptr->tag = TC_VIDEO|TC_PRE_M_PROCESS;
            tc_filter_process((frame_list_t *)vptr);

            /* internal processing of video */
            vptr->tag = TC_VIDEO;
            process_vid_frame(vob, vptr);

            /* external plugin post-processing */
            vptr->tag = TC_VIDEO|TC_POST_M_PROCESS;
            tc_filter_process((frame_list_t *)vptr);
        }

        DEC_VBUF_COUNTER(xx);
        INC_VBUF_COUNTER(ex);
    }

    if (TC_FRAME_NEED_PROCESSING(vptr)) {
        /* second stage post-processing - (synchronous) */
        vptr->tag = TC_VIDEO|TC_POST_S_PROCESS;
        tc_filter_process((frame_list_t *)vptr);
        postprocess_vid_frame(vob, vptr);
        /* preview _after_ all post-processing */
        vptr->tag = TC_VIDEO|TC_PREVIEW;
        tc_filter_process((frame_list_t *)vptr);
    }
}

static void apply_audio_filters(aframe_list_t *aptr, vob_t *vob)
{
    /* now we try to process the audio frame */
    if (!have_aud_threads) {
        DEC_ABUF_COUNTER(im);
        INC_ABUF_COUNTER(xx);

        if (TC_FRAME_NEED_PROCESSING(aptr)) {
            /* external plugin pre-processing */
            aptr->tag = TC_AUDIO|TC_PRE_M_PROCESS;
            tc_filter_process((frame_list_t *)aptr);

            /* internal processing of audio */
            aptr->tag = TC_AUDIO;
            process_aud_frame(vob, aptr);

            /* external plugin post-processing */
            aptr->tag = TC_AUDIO|TC_POST_M_PROCESS;
            tc_filter_process((frame_list_t *)aptr);
        }

        DEC_ABUF_COUNTER(xx);
        INC_ABUF_COUNTER(ex);
    }

    if (TC_FRAME_NEED_PROCESSING(aptr)) {
        /* second stage post-processing - (synchronous) */
        aptr->tag = TC_AUDIO|TC_POST_S_PROCESS;
        tc_filter_process((frame_list_t *)aptr);
        /* preview _after_ all post-processing */
        aptr->tag = TC_AUDIO|TC_PREVIEW;
        tc_filter_process((frame_list_t *)aptr);
    }
}


static int encoder_wait_vframe(TCEncoderBuffer *buf)
{
    buf->vptr = NULL;

    if (!tc_export_stop_requested()) {
        //check buffer fill level
        pthread_mutex_lock(&vframe_list_lock);

        if (verbose >= TC_FLIST)
            tc_log_msg(__FILE__, "requesting a new video frame");

        while (!vframe_fill_level(TC_BUFFER_READY)) {
            if (verbose >= TC_FLIST)
                tc_log_msg(__FILE__, "video frame not ready, waiting");
            pthread_cond_wait(&(buf->vframe_ready_cv), &vframe_list_lock);
            if (verbose >= TC_FLIST)
                tc_log_msg(__FILE__, "video wait just ended");

            if (!vimport_status() || tc_export_stop_requested()) {
                if (verbose >= TC_DEBUG) {
                    tc_log_warn(__FILE__, "import closed - buffer empty (V)");
                }
                break;
            }
        }
        pthread_mutex_unlock(&vframe_list_lock);

        buf->vptr = vframe_retrieve();
        if (verbose >= TC_FLIST)
            tc_log_msg(__FILE__, "got a new video frame reference: %p", buf->vptr);
    }
    return (buf->vptr != NULL) ?TC_OK :TC_ERROR;
}

static int encoder_wait_aframe(TCEncoderBuffer *buf)
{
    buf->aptr = NULL;

    if (!tc_export_stop_requested()) {
        //check buffer fill level
        pthread_mutex_lock(&aframe_list_lock);

        if (verbose >= TC_FLIST)
            tc_log_msg(__FILE__, "requesting a new audio frame");

        while (!aframe_fill_level(TC_BUFFER_READY)) {
            if (verbose >= TC_FLIST)
                tc_log_msg(__FILE__, "audio frame not ready, waiting");
            pthread_cond_wait(&(buf->aframe_ready_cv), &aframe_list_lock);
            if (verbose >= TC_FLIST)
                tc_log_msg(__FILE__, "audio wait just ended");

            if (!aimport_status() || tc_export_stop_requested()) {
                if (verbose >= TC_DEBUG) {
                    tc_log_warn(__FILE__, "import closed - buffer empty (A)");
                }
                break;
            }
        }
        pthread_mutex_unlock(&aframe_list_lock);

        buf->aptr = aframe_retrieve();
        if (verbose >= TC_FLIST)
            tc_log_msg(__FILE__, "got a new audio frame reference: %p", buf->aptr);
    }
    return (buf->aptr != NULL) ?TC_OK :TC_ERROR;
}


static int encoder_acquire_vframe(TCEncoderBuffer *buf, vob_t *vob)
{
    int err = 0;
    int got_frame = TC_TRUE;

    do {
        err = encoder_wait_vframe(buf);
        if (err) {
            return -1; /* can't acquire video frame */
        }
        got_frame = TC_TRUE;
        buf->frame_id = buf->vptr->id + tc_get_frames_skipped_cloned();

        if (verbose & TC_STATS) {
            tc_log_info(__FILE__, "got frame 0x%lux (%i)",
                                  (unsigned long) buf->vptr, buf->frame_id);
        }

        /*
         * now we do the post processing ... this way, if just a video frame is
         * skipped, we'll know.
         *
         * we have to check to make sure that before we do any processing
         * that this frame isn't out of range (if it is, and one is using
         * the "-t" split option, we'll see this frame again.
         */
        apply_video_filters(buf->vptr, vob);

        if (buf->vptr->attributes & TC_FRAME_IS_SKIPPED) {
            if (!have_vid_threads) {
                DEC_VBUF_COUNTER(im);
            } else {
                DEC_VBUF_COUNTER(ex);
            }

            if (buf->vptr != NULL
              && (buf->vptr->attributes & TC_FRAME_WAS_CLONED)
            ) {
                /* XXX do we want to track skipped cloned flags? */
                tc_update_frames_cloned(1);
            }

            if (buf->vptr != NULL
              && (buf->vptr->attributes & TC_FRAME_IS_CLONED)
            ) {
                /* XXX what to do when a frame is cloned and skipped? */
                /*
                 * I'd like to say they cancel, but perhaps they will end
                 * up also skipping the clone?  or perhaps they'll keep,
                 * but modify the clone?  Best to do the whole drill :/
                 */
                if (verbose & TC_DEBUG) {
                    tc_log_info (__FILE__, "(%i) V pointer done. "
                                           "Skipped and Cloned: (%i)",
                                           buf->vptr->id,
                                           (buf->vptr->attributes));
                }

                /* update flags */
                buf->vptr->attributes &= ~TC_FRAME_IS_CLONED;
                buf->vptr->attributes |= TC_FRAME_WAS_CLONED;
                /*
                 * this has to be done here,
                 * frame_threads.c won't see the frame again
                 */
                INC_VBUF_COUNTER(ex);
            }
            if (buf->vptr != NULL
              && !(buf->vptr->attributes & TC_FRAME_IS_CLONED)
            ) {
                vframe_remove(buf->vptr);

                tc_import_video_notify(); 

                /* reset pointer for next retrieve */
                buf->vptr = NULL;
            }
            // tc_update_frames_skipped(1);
            got_frame = TC_FALSE;
        }
    } while (!got_frame);

    return 0;
}

static int encoder_acquire_aframe(TCEncoderBuffer *buf, vob_t *vob)
{
    int err = 0;
    int got_frame = TC_TRUE;

    do {
        err = encoder_wait_aframe(buf);
        if (err) {
            return -1;
        }
        got_frame = TC_TRUE;

        if (verbose & TC_STATS) {
            tc_log_info(__FILE__, "got audio frame (%i)", buf->aptr->id);
        }

        apply_audio_filters(buf->aptr, vob);

        if (buf->aptr->attributes & TC_FRAME_IS_SKIPPED) {
            if (!have_aud_threads) {
                DEC_ABUF_COUNTER(im);
            } else {
                DEC_ABUF_COUNTER(ex);
            }

            if (buf->aptr != NULL
              && !(buf->aptr->attributes & TC_FRAME_IS_CLONED)
            ) {
                aframe_remove(buf->aptr);

                tc_import_audio_notify();

                /* reset pointer for next retrieve */
                buf->aptr = NULL;
            }

            if (buf->aptr != NULL
              && (buf->aptr->attributes & TC_FRAME_IS_CLONED)
            ) {
                if (verbose & TC_DEBUG) {
                    tc_log_info(__FILE__, "(%i) A pointer done. Skipped and Cloned: (%i)",
                                        buf->aptr->id, (buf->aptr->attributes));
                }

                /* adjust clone flags */
                buf->aptr->attributes &= ~TC_FRAME_IS_CLONED;
                buf->aptr->attributes |= TC_FRAME_WAS_CLONED;

                /*
                 * this has to be done here,
                 * frame_threads.c won't see the frame again
                 */
                INC_ABUF_COUNTER(ex);
            }
            got_frame = TC_FALSE;
        }
    } while (!got_frame);

    return 0;
}


static void encoder_dispose_vframe(TCEncoderBuffer *buf)
{
    if (buf->vptr->attributes & TC_FRAME_IS_OUT_OF_RANGE) {
        if (!have_vid_threads) {
            DEC_VBUF_COUNTER(im);
        } else {
            DEC_VBUF_COUNTER(ex);
        }
    } else {
        DEC_VBUF_COUNTER(ex);
    }

    if (buf->vptr != NULL
      && (buf->vptr->attributes & TC_FRAME_WAS_CLONED)
    ) {
        tc_update_frames_cloned(1);
    }

    if (buf->vptr != NULL
      && !(buf->vptr->attributes & TC_FRAME_IS_CLONED)
    ) {
        vframe_remove(buf->vptr);

        tc_import_video_notify();

        /* reset pointer for next retrieve */
        buf->vptr = NULL;
    }

    if (buf->vptr != NULL
      && (buf->vptr->attributes & TC_FRAME_IS_CLONED)
    ) {
        if(verbose & TC_DEBUG) {
            tc_log_info(__FILE__, "(%i) V pointer done. Cloned: (%i)",
                                buf->vptr->id, (buf->vptr->attributes));
        }
        buf->vptr->attributes &= ~TC_FRAME_IS_CLONED;
        buf->vptr->attributes |= TC_FRAME_WAS_CLONED;

        /*
         * this has to be done here,
         * frame_threads.c won't see the frame again
         */
        INC_VBUF_COUNTER(ex);

        // update counter
        //tc_update_frames_cloned(1);
    }
}


static void encoder_dispose_aframe(TCEncoderBuffer *buf)
{
    if (buf->aptr->attributes & TC_FRAME_IS_OUT_OF_RANGE) {
        if (!have_aud_threads) {
            DEC_ABUF_COUNTER(im);
        } else {
            DEC_ABUF_COUNTER(ex);
        }
    } else {
        DEC_ABUF_COUNTER(ex);
    }

    if (buf->aptr != NULL
      && !(buf->aptr->attributes & TC_FRAME_IS_CLONED)
    ) {
        aframe_remove(buf->aptr);

        tc_import_audio_notify();

        /* reset pointer for next retrieve */
        buf->aptr = NULL;
    }

    if (buf->aptr != NULL
      && (buf->aptr->attributes & TC_FRAME_IS_CLONED)
    ) {
        if (verbose & TC_DEBUG) {
            tc_log_info(__FILE__, "(%i) A pointer done. Cloned: (%i)",
                                 buf->aptr->id, (buf->aptr->attributes));
        }

        buf->aptr->attributes &= ~TC_FRAME_IS_CLONED;
        buf->aptr->attributes |= TC_FRAME_WAS_CLONED;

        /*
         * this has to be done here,
         * frame_threads.c won't see the frame again
         */
        INC_ABUF_COUNTER(ex);
    }
}


static int encoder_have_data(TCEncoderBuffer *buf)
{
    return import_status();
}


static TCEncoderBuffer tc_builtin_buffer = {
    .frame_id = 0,

    .vframe_ready_cv = PTHREAD_COND_INITIALIZER,
    .aframe_ready_cv = PTHREAD_COND_INITIALIZER,

    .vptr = NULL,
    .aptr = NULL,

    .acquire_video_frame = encoder_acquire_vframe,
    .acquire_audio_frame = encoder_acquire_aframe,
    .dispose_video_frame = encoder_dispose_vframe,
    .dispose_audio_frame = encoder_dispose_aframe,

    .have_data = encoder_have_data,
};

/* default main transcode buffer */
TCEncoderBuffer *tc_get_ringbuffer(int aworkers, int vworkers)
{
    have_aud_threads = aworkers;
    have_vid_threads = vworkers;

    return &tc_builtin_buffer;
}


/*************************************************************************/

/* 
 * OK, that sucks. I know that sucks. In order to doing better,
 * much deeper refactoring is needed (i.e.: kill frame_list s
 */

void tc_export_audio_notify(void)
{
    pthread_mutex_lock(&aframe_list_lock);
    pthread_cond_signal(&tc_builtin_buffer.aframe_ready_cv);
    pthread_mutex_unlock(&aframe_list_lock);
}

void tc_export_video_notify(void)
{
    pthread_mutex_lock(&vframe_list_lock);
    pthread_cond_signal(&tc_builtin_buffer.vframe_ready_cv);
    pthread_mutex_unlock(&vframe_list_lock);
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
