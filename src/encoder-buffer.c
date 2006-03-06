/*
 *  encoder_buffer.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *  Updated and partially rewritten by
 *  Francesco Romani -January 2006
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

#include "encoder.h"

#include "filter.h"
#include "counter.h"
#include "video_trans.h"
#include "audio_trans.h"
#include "decoder.h"

#include "frame_threads.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif


/*
 * XXX: critical changes:
 * - lost inlining for acquire/dispose -> speed loss (unavoidable?)
 * - moved counters update into dispose -> SHOULD be harmless
 * - wrapper on exit_flag check -> harmless?
 */

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

/*
 * Apply the filter chain to current video frame.
 * Used if no frame threads are avalaible.
 * this function should be never exported.
 */
static void apply_video_filters(vframe_list_t *vptr, vob_t *vob)
{
    if (!have_vframe_threads) {
        DEC_VBUF_COUNTER(im);
        INC_VBUF_COUNTER(xx);

        /* external plugin pre-processing */
        vptr->tag = TC_VIDEO|TC_PRE_M_PROCESS;
        process_vid_plugins(vptr);
  
        /* internal processing of video */
        vptr->tag = TC_VIDEO;
        process_vid_frame(vob, vptr);
      
        /* external plugin post-processing */
        vptr->tag = TC_VIDEO|TC_POST_M_PROCESS;
        process_vid_plugins(vptr);
  
        DEC_VBUF_COUNTER(xx);
        INC_VBUF_COUNTER(ex);
    }
    
    /* second stage post-processing - (synchronous) */
    vptr->tag = TC_VIDEO|TC_POST_S_PROCESS;
    process_vid_plugins(vptr);
    postprocess_vid_frame(vob, vptr);
}

/*
 * Apply the filter chain to current audio frame.
 * Used if no frame threads are avalaible.
 * this function should be never exported.
 */
static void apply_audio_filters(aframe_list_t *aptr, vob_t *vob)
{
    /* now we try to process the audio frame */
    if (!have_aframe_threads) {
        DEC_ABUF_COUNTER(im);
        INC_ABUF_COUNTER(xx);

        /* external plugin pre-processing */
        aptr->tag = TC_AUDIO|TC_PRE_PROCESS;
        process_aud_plugins(aptr);
      
        /* internal processing of audio */
        aptr->tag = TC_AUDIO;
        process_aud_frame(vob, aptr);
      
        /* external plugin post-processing */
        aptr->tag = TC_AUDIO|TC_POST_PROCESS;
        process_aud_plugins(aptr);

        DEC_ABUF_COUNTER(xx);
        INC_ABUF_COUNTER(ex);
    }
    
    /* second stage post-processing - (synchronous) */
    aptr->tag = TC_AUDIO|TC_POST_S_PROCESS;
    process_aud_plugins(aptr);
}

/*
 * wait until a new audio frame is avalaible for encoding
 * in frame buffer.
 * When a new frame is avalaible update the encoding context
 * adjusting video frame buffer, and returns 0.
 * Return -1 if no more frames are avalaible. 
 * This usually happens when video stream ends.
 */
static vframe_list_t *encoder_wait_vframe(TCEncoderBuffer *buf)
{     
    int ready = TC_FALSE;
    vframe_list_t *vptr = NULL;
  
    while (1) {
        /* check buffer fill level */
        pthread_mutex_lock(&vframe_list_lock);
        ready = vframe_fill_level(TC_BUFFER_READY);
        pthread_mutex_unlock(&vframe_list_lock);
  
        if (ready) {
            vptr = vframe_retrieve();
            if (vptr != NULL) {
                break;
            }
        } else { /* not ready */
            /* check import status */
            if (!vimport_status() || tc_export_stop_requested())  {
                if (verbose & TC_DEBUG) {
                    tc_log_warn(__FILE__, "import closed - buffer empty (V)");
                }
                vptr = NULL;
                break;
            }
            if (verbose & TC_STATS) {
                tc_log_info(__FILE__, "waiting for video frames");
            }
        }
      
        /* 
         * no frame available at this time
         * pthread_yield is probably a cleaner solution, but it's a GNU extension
         */
        usleep(tc_buffer_delay_enc);
    }
    return vptr;
}

/*
 * wait until a new audio frame is avalaible for encoding
 * in frame buffer.
 * When a new frame is avalaible update the encoding context
 * adjusting video frame buffer, and returns 0.
 * Return -1 if no more frames are avalaible. This usually
 * means that video stream is ended.
 */
static aframe_list_t *encoder_wait_aframe(TCEncoderBuffer *buf)
{
    int ready = TC_FALSE;
    aframe_list_t *aptr = NULL;
 
    while (1) {
        /* check buffer fill level */
        pthread_mutex_lock(&aframe_list_lock);
        ready = aframe_fill_level(TC_BUFFER_READY);
        pthread_mutex_unlock(&aframe_list_lock);
      
        if (ready) {
            aptr = aframe_retrieve();
            if (aptr != NULL) {
                break;
            }
        } else { /* !ready */
            /* check import status */
            if (!aimport_status() || tc_export_stop_requested()) {
                if (verbose & TC_DEBUG) {
                    tc_log_warn(__FILE__, "import closed - buffer empty (A)");
                }
                aptr = NULL;
                break;
            }
            if (verbose & TC_STATS) {
                tc_log_info(__FILE__, "waiting for audio frames");
            }
        }
        /* 
         * no frame available at this time
         * pthread_yield is probably a cleaner solution, but it's a GNU extension
         */
        usleep(tc_buffer_delay_enc);
    }
    return aptr;
}

/*
 * get a new video frame for encoding. This means:
 * 1. to wait for a new frame avalaible for encoder using encoder_wait_vframe
 * 2. apply the filters if no frame threads are avalaible
 * 3. apply the encoder filters (POST_S_PROCESS)
 * 4. verify the status of video frame after all filtering.
 *    if acquired video frame is skipped, we must acquire a new one before
 *    continue with encoding, so we must restart from step 1.
 * returns 0 when a new frame is avalaible for encoding, or <0 if no more
 * video frames are avalaible. As for encoder_wait_vframe, this usually happens
 * when video stream ends.
 * returns >0 if frame id exceed the desired frame range
 */   
static int encoder_acquire_vframe(TCEncoderBuffer *buf)
{
    int got_frame = TC_TRUE;
  
    do {
        buf->vptr = encoder_wait_vframe(buf);
        if (buf->vptr == NULL) {
            return -1; /* can't acquire video frame */
        }
        got_frame = TC_TRUE;
      
        buf->frame_id = buf->vptr->id + tc_get_frames_skipped_cloned();
        if (verbose & TC_STATS) {
            tc_log_info(__FILE__, "got video frame (%i|%i)",
                                  buf->vptr->id, buf->frame_id);
        }
      
        /*
         * now we do the post processing ... this way, if just a video frame is
         * skipped, we'll know.
         *
         * we have to check to make sure that before we do any processing
         * that this frame isn't out of range (if it is, and one is using
         * the "-t" split option, we'll see this frame again.
         */

        if (buf->frame_id >= buf->frame_num) {
            if (verbose & TC_DEBUG) {
                tc_log_info(__FILE__, "encoder last frame finished (%i/%i)",
                                      buf->frame_id, buf->frame_num);
            }
            return 1;
        }

        apply_video_filters(buf->vptr, buf->vob);

        if (buf->vptr->attributes & TC_FRAME_IS_SKIPPED){
            if (!have_vframe_threads) {
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

                /* notify sleeping import thread */
                pthread_mutex_lock(&vframe_list_lock);
                pthread_cond_signal(&vframe_list_full_cv);
                pthread_mutex_unlock(&vframe_list_lock);

                /* reset pointer for next retrieve */
                buf->vptr = NULL;
            }
            // tc_update_frames_skipped(1);
            got_frame = TC_FALSE;
        }
    } while (!got_frame);

    return 0;
}

/*
 * get a new audio frame for encoding. This means:
 * 1. to wait for a new frame avalaible for encoder using encoder_wait_aframe
 * 2. apply the filters if no frame threads are avalaible
 * 3. apply the encoder filters (POST_S_PROCESS)
 * 4. verify the status of audio frame after all filtering.
 *    if acquired audio frame is skipped, we must acquire a new one before
 *    continue with encoding, so we must restart from step 1.
 * returns 0 when a new frame is avalaible for encoding, or <0 if no more
 * video frames are avalaible. As for encoder_wait_aframe, this usually happens
 * when audio stream ends.
 */   
static int encoder_acquire_aframe(TCEncoderBuffer *buf)
{
    int got_frame = TC_TRUE;
  
    do {
        buf->aptr = encoder_wait_aframe(buf);
        if (buf->aptr == NULL) {
            return -1;
        }
        got_frame = TC_TRUE;
      
        if (verbose & TC_STATS) {
            tc_log_info(__FILE__, "got audio frame (%i)", buf->aptr->id);
        }
      
        apply_audio_filters(buf->aptr, buf->vob);

        if (buf->aptr->attributes & TC_FRAME_IS_SKIPPED) {
            if (!have_aframe_threads) {
                DEC_ABUF_COUNTER(im);
            } else {
                DEC_ABUF_COUNTER(ex);
            }

            if (buf->aptr != NULL 
              && !(buf->aptr->attributes & TC_FRAME_IS_CLONED)
            ) {
                aframe_remove(buf->aptr);  

                /* notify sleeping import thread */
                pthread_mutex_lock(&aframe_list_lock);
                pthread_cond_signal(&aframe_list_full_cv);
                pthread_mutex_unlock(&aframe_list_lock);

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

static int encoder_dispose_vframe(TCEncoderBuffer *buf)
{
    if (!have_vframe_threads) {
        DEC_VBUF_COUNTER(im);
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
    
        /* notify sleeping import thread */
        pthread_mutex_lock(&vframe_list_lock);
        pthread_cond_signal(&vframe_list_full_cv);
        pthread_mutex_unlock(&vframe_list_lock);
    
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
    return 0;
}

   
/*
 * put back to frame buffer an acquired audio frame
 */
static int encoder_dispose_aframe(TCEncoderBuffer *buf)
{
    if (!have_aframe_threads) {
        DEC_ABUF_COUNTER(im);
    } else {
        DEC_ABUF_COUNTER(ex);
    }

    if (buf->aptr != NULL 
      && !(buf->aptr->attributes & TC_FRAME_IS_CLONED)
    ) {
        aframe_remove(buf->aptr);
    
        /* notify sleeping import thread */
        pthread_mutex_lock(&aframe_list_lock);
        pthread_cond_signal(&aframe_list_full_cv);
        pthread_mutex_unlock(&aframe_list_lock);
    
        /* reset pointer for next retrieve */
        buf->aptr=NULL;
    }           

    if (buf->aptr!=NULL 
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
    return 0;
}

static int encoder_have_data(TCEncoderBuffer *buf)
{
    return import_status();
}

static TCEncoderBuffer tc_buffer = {
    .frame_id = 0,
    .frame_num = 0,

    .vob = NULL,

    .vptr = NULL,
    .aptr = NULL,

    .acquire_video_frame = encoder_acquire_vframe,
    .acquire_audio_frame = encoder_acquire_aframe,
    .dispose_video_frame = encoder_dispose_vframe,
    .dispose_audio_frame = encoder_dispose_aframe,

    .have_data = encoder_have_data,
};
            
TCEncoderBuffer *tc_builtin_buffer(vob_t *vob, int frame_num)
{
    if (!vob || frame_num < 0) {
        return NULL;
    }

    tc_buffer.vob = vob;
    tc_buffer.frame_num = frame_num;

    return &tc_buffer;
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
