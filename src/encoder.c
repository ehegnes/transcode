/*
 *  encoder.c
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

#include "dl_loader.h"
#include "framebuffer.h"
#include "counter.h"
#include "video_trans.h"
#include "audio_trans.h"
#include "decoder.h"
#include "encoder.h"

#include "frame_threads.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

/*
 * new encoder module design principles
 * 1) keep it simple, stupid
 * 2) to have more than one encoder doesn't make sense in transcode, so
 * 3) new encoder will be monothread, like the old one
 */

typedef struct tcencoderdata_ TCEncoderData;
struct tcencoderdata_ {
    /* references to current frames */
    vframe_list_t *vptr;
    aframe_list_t *aptr;

    /* (video) frame identifier */
    int fid;

    /* flags */
    int error_flag;
    int fill_flag;

    /* frame boundaries */
    int frame_first;
    int frame_last;
    /* needed by encoder_skip */
    int saved_frame_last;

    void *export_ahandle;
    void *export_vhandle;

    uint32_t frames_encoded;
    uint32_t frames_dropped;
    uint32_t frames_skipped;
    uint32_t frames_cloned;
    pthread_mutex_t frame_counter_lock;

    volatile int exit_flag;

    transfer_t export_para;
};

static TCEncoderData encdata;


static void tc_encoder_data_init(void)
{
    memset(&encdata, 0, sizeof(TCEncoderData));
    
    encdata.vptr = NULL;
    encdata.aptr = NULL;

    encdata.export_ahandle = NULL;
    encdata.export_vhandle = NULL;

    encdata.frames_encoded = 0;
    encdata.frames_dropped = 0;
    encdata.frames_skipped = 0;
    encdata.frames_cloned = 0;

    encdata.exit_flag = TC_FALSE;

    pthread_mutex_init(&encdata.frame_counter_lock, NULL);
}


void tc_export_stop_nolock(void)
{
    encdata.exit_flag = TC_TRUE;
}

uint32_t tc_get_frames_encoded(void)
{
    uint32_t val;
    
    pthread_mutex_lock(&encdata.frame_counter_lock);
    val = encdata.frames_encoded;
    pthread_mutex_unlock(&encdata.frame_counter_lock);

    return val;
}

void tc_update_frames_encoded(uint32_t val)
{
    pthread_mutex_lock(&encdata.frame_counter_lock);
    encdata.frames_encoded += val;
    pthread_mutex_unlock(&encdata.frame_counter_lock);
}

uint32_t tc_get_frames_dropped(void)
{
    uint32_t val;
    
    pthread_mutex_lock(&encdata.frame_counter_lock);
    val = encdata.frames_dropped;
    pthread_mutex_unlock(&encdata.frame_counter_lock);
    
    return val;
}

void tc_update_frames_dropped(uint32_t val)
{
    pthread_mutex_lock(&encdata.frame_counter_lock);
    encdata.frames_dropped += val;
    pthread_mutex_unlock(&encdata.frame_counter_lock);
}

uint32_t tc_get_frames_skipped(void)
{
    uint32_t val;
    
    pthread_mutex_lock(&encdata.frame_counter_lock);
    val = encdata.frames_skipped;
    pthread_mutex_unlock(&encdata.frame_counter_lock);
    
    return val;
}

void tc_update_frames_skipped(uint32_t val)
{
    pthread_mutex_lock(&encdata.frame_counter_lock);
    encdata.frames_skipped += val;
    pthread_mutex_unlock(&encdata.frame_counter_lock);
}

uint32_t tc_get_frames_cloned(void)
{
    uint32_t val;
    
    pthread_mutex_lock(&encdata.frame_counter_lock);
    val = encdata.frames_cloned;
    pthread_mutex_unlock(&encdata.frame_counter_lock);
    
    return val;
}

void tc_update_frames_cloned(uint32_t val)
{
    pthread_mutex_lock(&encdata.frame_counter_lock);
    encdata.frames_cloned += val;
    pthread_mutex_unlock(&encdata.frame_counter_lock);
}

static uint32_t tc_get_frames_skipped_cloned(void)
{
    uint32_t s, c;
    
    pthread_mutex_lock(&encdata.frame_counter_lock);
    s = encdata.frames_skipped;
    c = encdata.frames_cloned;
    pthread_mutex_unlock(&encdata.frame_counter_lock);
    
    return(c - s);
}

/* ------------------------------------------------------------ 
 *
 * export init
 *
 * ------------------------------------------------------------*/

int export_init(vob_t *vob, char *a_mod, char *v_mod)
{
    char *mod_name = NULL;
    transfer_t export_para;

    // load export modules
    mod_name = (a_mod == NULL) ?TC_DEFAULT_EXPORT_AUDIO :a_mod;
    encdata.export_ahandle = load_module(mod_name, TC_EXPORT + TC_AUDIO);
    if (encdata.export_ahandle == NULL) {
        tc_log_warn(__FILE__, "loading audio export module failed");
        return -1;
   }

    mod_name = (v_mod==NULL) ?TC_DEFAULT_EXPORT_VIDEO :v_mod;
    encdata.export_vhandle = load_module(mod_name, TC_EXPORT + TC_VIDEO);
    if (encdata.export_vhandle == NULL) {
        tc_log_warn(__FILE__, "loading video export module failed");
        return -1;
    }

    export_para.flag = verbose;
    tca_export(TC_EXPORT_NAME, &export_para, NULL); 

    if(export_para.flag != verbose) {
        // module returned capability flag
        int cc=0;
    
        if (verbose & TC_DEBUG) {
            tc_log_info(__FILE__, "audio capability flag 0x%x | 0x%x", 
                                  export_para.flag, vob->im_a_codec);
        }
    
        switch (vob->im_a_codec) {
          case CODEC_PCM: 
            cc = (export_para.flag & TC_CAP_PCM);
            break;
          case CODEC_AC3: 
            cc = (export_para.flag & TC_CAP_AC3);
            break;
          case CODEC_RAW: 
            cc = (export_para.flag & TC_CAP_AUD);
            break;
          default:
            cc = 0;
        }
    
        if (cc == 0) {
            tc_log_warn(__FILE__, "audio codec not supported by export module");
            return -1;
        }
    } else { /* export_para.flag == verbose */
        if (vob->im_a_codec != CODEC_PCM) {
            tc_log_warn(__FILE__, "audio codec not supported by export module");
            return -1;
        }
    }
  
    export_para.flag = verbose;
    tcv_export(TC_EXPORT_NAME, &export_para, NULL);

    if (export_para.flag != verbose) {
        // module returned capability flag
        int cc = 0;
    
        if (verbose & TC_DEBUG) {
            tc_log_info(__FILE__, "video capability flag 0x%x | 0x%x", 
                                  export_para.flag, vob->im_v_codec);
        }
    
        switch (vob->im_v_codec) {
          case CODEC_RGB: 
            cc = (export_para.flag & TC_CAP_RGB);
            break;
          case CODEC_YUV: 
            cc = (export_para.flag & TC_CAP_YUV);
            break;
          case CODEC_YUV422: 
            cc = (export_para.flag & TC_CAP_YUV422);
            break;
          case CODEC_RAW: 
          case CODEC_RAW_YUV: /* fallthrough */
            cc = (export_para.flag & TC_CAP_VID);
            break;
          default:
            cc = 0;
        }
    
        if (cc == 0) {
            tc_log_warn(__FILE__, "video codec not supported by export module"); 
            return -1;
        }
    } else { /* export_para.flag == verbose */
        if (vob->im_v_codec != CODEC_RGB) {
            tc_log_warn(__FILE__, "video codec not supported by export module"); 
            return -1;
        }
    }
    
    return 0;
}  

/* ------------------------------------------------------------ 
 *
 * export close, unload modules
 *
 * ------------------------------------------------------------*/

void export_shutdown()
{
    if (verbose & TC_DEBUG) {
        tc_log_info(__FILE__, "unloading export modules");
    }

    unload_module(encdata.export_ahandle);
    unload_module(encdata.export_vhandle);
}


/* ------------------------------------------------------------ 
 *
 * encoder init
 *
 * ------------------------------------------------------------*/

int encoder_init(vob_t *vob)
{
    int ret;
    transfer_t export_para;
  
    export_para.flag = TC_VIDEO;
    ret = tcv_export(TC_EXPORT_INIT, &export_para, vob);
    if (ret == TC_EXPORT_ERROR) {
        tc_log_warn(__FILE__, "video export module error: init failed");
        return -1;
    }
  
    export_para.flag = TC_AUDIO;
    ret = tca_export(TC_EXPORT_INIT, &export_para, vob);
    if (ret == TC_EXPORT_ERROR) {
        tc_log_warn(__FILE__, "audio export module error: init failed");
        return -1;
    }
  
    return 0;
}


/* ------------------------------------------------------------ 
 *
 * encoder open
 *
 * ------------------------------------------------------------*/

int encoder_open(vob_t *vob)
{
    int ret;
    transfer_t export_para;
  
    export_para.flag = TC_VIDEO; 
    ret = tcv_export(TC_EXPORT_OPEN, &export_para, vob);
    if (ret == TC_EXPORT_ERROR) {
        tc_log_warn(__FILE__, "video export module error: open failed");
        return -1;
    }
  
    export_para.flag = TC_AUDIO;
    ret = tca_export(TC_EXPORT_OPEN, &export_para, vob);
    if (ret == TC_EXPORT_ERROR) {
        tc_log_warn(__FILE__, "audio export module error: open failed");
        return -1;
    }
  
    return 0;
}


/* ------------------------------------------------------------ 
 *
 * encoder close
 *
 * ------------------------------------------------------------*/

int encoder_close(void)
{
    transfer_t export_para;
    /* close, errors not fatal */

    export_para.flag = TC_AUDIO;
    tca_export(TC_EXPORT_CLOSE, &export_para, NULL);

    export_para.flag = TC_VIDEO;
    tcv_export(TC_EXPORT_CLOSE, &export_para, NULL);
  
    if(verbose & TC_DEBUG) {
        tc_log_info(__FILE__, "encoder closed");
    }
    
    return 0;
}


/* ------------------------------------------------------------ 
 *
 * encoder stop
 *
 * ------------------------------------------------------------*/

int encoder_stop(void)
{
    int ret;
    transfer_t export_para;

    export_para.flag = TC_VIDEO;
    ret = tcv_export(TC_EXPORT_STOP, &export_para, NULL);
    if (ret == TC_EXPORT_ERROR) {
        tc_log_warn(__FILE__, "video export module error: stop failed");
        return -1;
    }
  
    export_para.flag = TC_AUDIO;
    ret = tca_export(TC_EXPORT_STOP, &export_para, NULL);
    if (ret == TC_EXPORT_ERROR) {
        tc_log_warn(__FILE__, "audio export module error: stop failed");
        return -1;
    }
  
    return 0;
}

/* ------------------------------------------------------------ 
 *
 * encoder main loop helpers
 *
 * ------------------------------------------------------------*/

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
static int encoder_wait_vframe(TCEncoderData *data)
{     
    int ready = TC_FALSE;
    int have_frame = TC_FALSE;
  
    while (!have_frame) {
        /* check buffer fill level */
        pthread_mutex_lock(&vframe_list_lock);
        ready = vframe_fill_level(TC_BUFFER_READY);
        pthread_mutex_unlock(&vframe_list_lock);
  
        if (ready) {
            data->vptr = vframe_retrieve();
            if (data->vptr != NULL) {
                data->fid = data->vptr->id + tc_get_frames_skipped_cloned();
                have_frame = TC_TRUE;
                break;
            }
        } else { /* !ready */
            /* check import status */
            if (!vimport_status() || encdata.exit_flag)  {
                if (verbose & TC_DEBUG) {
                    tc_log_warn(__FILE__, "import closed - buffer empty (V)");
                }
                return -1;
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
    return 0;
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
static int encoder_acquire_vframe(TCEncoderData *data, vob_t *vob)
{
    int err = 0;
    int got_frame = TC_TRUE;
  
    do {
        err = encoder_wait_vframe(data);
        if (err) {
            return -1; /* can't acquire video frame */
        }
      
        if (verbose & TC_STATS) {
            tc_log_info(__FILE__, "got frame 0x%lux (%d)", 
                                  (unsigned long) data->vptr, data->fid);
        }
      
        /*
         * now we do the post processing ... this way, if just a video frame is
         * skipped, we'll know.
         *
         * we have to check to make sure that before we do any processing
         * that this frame isn't out of range (if it is, and one is using
         * the "-t" split option, we'll see this frame again.
         */

        if (data->fid >= data->frame_last) {
            if (verbose & TC_DEBUG) {
                tc_log_info(__FILE__, "encoder last frame finished (%d/%d)",
                                      data->fid, data->frame_last);
            }
            return 1;
        }

        apply_video_filters(data->vptr, vob);

        if (data->vptr->attributes & TC_FRAME_IS_SKIPPED){
            if (!have_vframe_threads) {
                DEC_VBUF_COUNTER(im);
            } else {
                DEC_VBUF_COUNTER(ex);
            }
    
            if (data->vptr != NULL 
              && (data->vptr->attributes & TC_FRAME_WAS_CLONED)
            ) {
                /* XXX do we want to track skipped cloned flags? */
                tc_update_frames_cloned(1);
            }

            if (data->vptr != NULL 
              && (data->vptr->attributes & TC_FRAME_IS_CLONED)
            ) {
                /* XXX what to do when a frame is cloned and skipped? */
                /*
                 * I'd like to say they cancel, but perhaps they will end
                 * up also skipping the clone?  or perhaps they'll keep,
                 * but modify the clone?  Best to do the whole drill :/
                 */
                if (verbose & TC_DEBUG) {
                    tc_log_info (__FILE__, "(%d) V pointer done. "
                                           "Skipped and Cloned: (%d)", 
                                           data->vptr->id, 
                                           (data->vptr->attributes));
                }

                /* update flags */
                data->vptr->attributes &= ~TC_FRAME_IS_CLONED;
                data->vptr->attributes |= TC_FRAME_WAS_CLONED;
                /*
                 * this has to be done here, 
                 * frame_threads.c won't see the frame again
                 */
                INC_VBUF_COUNTER(ex);
            }
            if (data->vptr != NULL 
              && !(data->vptr->attributes & TC_FRAME_IS_CLONED)
            ) {
                vframe_remove(data->vptr);

                /* notify sleeping import thread */
                pthread_mutex_lock(&vframe_list_lock);
                pthread_cond_signal(&vframe_list_full_cv);
                pthread_mutex_unlock(&vframe_list_lock);

                /* reset pointer for next retrieve */
                data->vptr = NULL;
            }
            // tc_update_frames_skipped(1);
            got_frame = TC_FALSE;
        }
    } while (!got_frame);

    return 0;
}

/*
 * wait until a new audio frame is avalaible for encoding
 * in frame buffer.
 * When a new frame is avalaible update the encoding context
 * adjusting video frame buffer, and returns 0.
 * Return -1 if no more frames are avalaible. This usually
 * means that video stream is ended.
 */
static int encoder_wait_aframe(TCEncoderData *data)
{
    int ready = TC_FALSE;
    int have_frame = TC_FALSE;
 
    while (!have_frame) {
        /* check buffer fill level */
        pthread_mutex_lock(&aframe_list_lock);
        ready = aframe_fill_level(TC_BUFFER_READY);
        pthread_mutex_unlock(&aframe_list_lock);
      
        if (ready) {
            data->aptr = aframe_retrieve();
            if (data->aptr != NULL) {
                have_frame = 1;
                break;
            }
        } else { /* !ready */
            /* check import status */
            if (!aimport_status() || encdata.exit_flag) {
                if (verbose & TC_DEBUG) {
                    tc_log_warn(__FILE__, "import closed - buffer empty (A)");
                }
                return -1;
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
static int encoder_acquire_aframe(TCEncoderData *data, vob_t *vob)
{
    int err = 0;
    int got_frame = TC_TRUE;
  
    do {
        err = encoder_wait_aframe(data);
        if (err) {
            return -1;
        }
      
        if (verbose & TC_STATS) {
            tc_log_info(__FILE__, "got audio frame (%d)", data->aptr->id );
        }
      
        apply_audio_filters(data->aptr, vob);

        if (data->aptr->attributes & TC_FRAME_IS_SKIPPED) {
            if (!have_aframe_threads) {
                DEC_ABUF_COUNTER(im);
            } else {
                DEC_ABUF_COUNTER(ex);
            }

            if (data->aptr != NULL 
              && !(data->aptr->attributes & TC_FRAME_IS_CLONED)
            ) {
                aframe_remove(data->aptr);  

                /* notify sleeping import thread */
                pthread_mutex_lock(&aframe_list_lock);
                pthread_cond_signal(&aframe_list_full_cv);
                pthread_mutex_unlock(&aframe_list_lock);

                /* reset pointer for next retrieve */
                data->aptr = NULL;
            }

            if (data->aptr != NULL 
              && (data->aptr->attributes & TC_FRAME_IS_CLONED)
            ) {
                if (verbose & TC_DEBUG) {
                    tc_log_info(__FILE__, "(%d) A pointer done. Skipped and Cloned: (%d)", 
                                        data->aptr->id, (data->aptr->attributes));
                }
 
                /* adjust clone flags */
                data->aptr->attributes &= ~TC_FRAME_IS_CLONED;
                data->aptr->attributes |= TC_FRAME_WAS_CLONED;

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

/*
 * dispatch the acquired frames to encoder modules, and adjust frame counters
 */
static int encoder_export(TCEncoderData *data, vob_t *vob)
{
    int video_delayed = 0;
    
    /* encode and export video frame */
    data->export_para.buffer = data->vptr->video_buf;
    data->export_para.size   = data->vptr->video_size;
    data->export_para.attributes = data->vptr->attributes;
    
    if (data->vptr->attributes & TC_FRAME_IS_KEYFRAME) {
        data->export_para.attributes |= TC_FRAME_IS_KEYFRAME;
    }    
    
    data->export_para.flag   = TC_VIDEO;

    if(tcv_export(TC_EXPORT_ENCODE, &data->export_para, vob) < 0) {
        tc_log_warn(__FILE__, "error encoding video frame");
        data->error_flag = 1;
    }

    if (data->export_para.attributes == TC_FRAME_IS_DELAYED) {
        data->export_para.attributes &= ~TC_FRAME_IS_DELAYED;
        video_delayed = 1;
    }
    /* maybe clone? */
    data->vptr->attributes = data->export_para.attributes;

    DEC_VBUF_COUNTER(ex);

    /* encode and export audio frame */
    data->export_para.buffer = data->aptr->audio_buf;
    data->export_para.size   = data->aptr->audio_size;
    data->export_para.attributes = data->aptr->attributes;
    
    data->export_para.flag   = TC_AUDIO;
    
    // XXX
    if(video_delayed) {
        data->aptr->attributes |= TC_FRAME_IS_CLONED; 
        tc_log_info(__FILE__, "Delaying audio");
    } else {
        if (tca_export(TC_EXPORT_ENCODE, &data->export_para, vob) < 0) {
            tc_log_warn(__FILE__, "error encoding audio frame");
            data->error_flag = 1;
        }
 
        /* maybe clone? */
        data->aptr->attributes = data->export_para.attributes;
    }

    DEC_ABUF_COUNTER(ex);

    if (verbose & TC_INFO) {
        int last = (data->frame_last == TC_FRAME_LAST) ?(-1) :data->frame_last;
        if (!data->fill_flag) {
            data->fill_flag = 1;
        }
        counter_print(1, data->fid, data->frame_first, last);
    }
    
    /* on success, increase global frame counter */
    tc_update_frames_encoded(1); 
    return data->error_flag;
}


/* 
 * fake encoding, simply adjust frame counters.
 */
static void encoder_skip(TCEncoderData *data)
{
    if (verbose & TC_INFO) {
        if (!data->fill_flag) {
            data->fill_flag = 1;
        }
        counter_print(0, data->fid, data->saved_frame_last, data->frame_first-1);
    }
    
    /*
     * we know we're not finished yet, because we did a quick
     * check before processing the frame
     */
    if (!have_aframe_threads) {
        DEC_VBUF_COUNTER(im);
        DEC_ABUF_COUNTER(im);
    } else {
        DEC_VBUF_COUNTER(ex);
        DEC_ABUF_COUNTER(ex);
    }
    return;
}


/*
 * put back to frame buffer an acquired video frame.
 */
static void encoder_dispose_vframe(TCEncoderData *data)
{
    if (data->vptr != NULL 
      && (data->vptr->attributes & TC_FRAME_WAS_CLONED)
    ) {
        tc_update_frames_cloned(1);
    }
      
    if (data->vptr != NULL 
      && !(data->vptr->attributes & TC_FRAME_IS_CLONED)
    ) {
        vframe_remove(data->vptr);  
    
        /* notify sleeping import thread */
        pthread_mutex_lock(&vframe_list_lock);
        pthread_cond_signal(&vframe_list_full_cv);
        pthread_mutex_unlock(&vframe_list_lock);
    
        /* reset pointer for next retrieve */
        data->vptr = NULL;           
    }
      
    if (data->vptr != NULL 
      && (data->vptr->attributes & TC_FRAME_IS_CLONED)
    ) {
        if(verbose & TC_DEBUG) {
            tc_log_info(__FILE__, "(%d) V pointer done. Cloned: (%d)", 
                                data->vptr->id, (data->vptr->attributes));
        }   
        data->vptr->attributes &= ~TC_FRAME_IS_CLONED;
        data->vptr->attributes |= TC_FRAME_WAS_CLONED;

        /*
         * this has to be done here, 
         * frame_threads.c won't see the frame again
         */
        INC_VBUF_COUNTER(ex);
    
        // update counter
        //tc_update_frames_cloned(1);
    }
}

   
/*
 * put back to frame buffer an acquired audio frame
 */
static void encoder_dispose_aframe(TCEncoderData *data)
{
    if (data->aptr != NULL 
      && !(data->aptr->attributes & TC_FRAME_IS_CLONED)
    ) {
        aframe_remove(data->aptr);
    
        /* notify sleeping import thread */
        pthread_mutex_lock(&aframe_list_lock);
        pthread_cond_signal(&aframe_list_full_cv);
        pthread_mutex_unlock(&aframe_list_lock);
    
        /* reset pointer for next retrieve */
        data->aptr=NULL;
    }           

    if (data->aptr!=NULL 
      && (data->aptr->attributes & TC_FRAME_IS_CLONED)
    ) {
        if (verbose & TC_DEBUG) {
            tc_log_info(__FILE__, "(%d) A pointer done. Cloned: (%d)", 
                                 data->aptr->id, (data->aptr->attributes));
        }
    
        data->aptr->attributes &= ~TC_FRAME_IS_CLONED;
        data->aptr->attributes |= TC_FRAME_WAS_CLONED;

        /*
         * this has to be done here, 
         * frame_threads.c won't see the frame again
         */
        INC_ABUF_COUNTER(ex);
    }
}

/* ------------------------------------------------------------ 
 *
 * encoder main loop
 *
 * ------------------------------------------------------------*/


void encoder(vob_t *vob, int frame_first, int frame_last)
{
    int err = 0;
    TCEncoderData data;

    // FIXME
    static int this_frame_last=0;
    static int saved_frame_last=0;

    if (this_frame_last != frame_last) {
        saved_frame_last = this_frame_last;
        this_frame_last = frame_last;
    }

    tc_encoder_data_init();

    data.frame_first = frame_first;
    data.frame_last = frame_last;
    data.saved_frame_last = saved_frame_last;
    
    do {
        /* check for ^C signal */
        if (encdata.exit_flag) {
            if (verbose & TC_DEBUG) {
                tc_log_warn(__FILE__, "export canceled on user request");
            }
            return;
        }
        tc_pause();
        
        err = encoder_acquire_vframe(&data, vob);
        if (err) {
            return; /* can't acquire video frame */
        }
      
        err = encoder_acquire_aframe(&data, vob);
        if (err) {
            return;  /* can't acquire frame */
        }
      
        //--------------------------------
        // need a valid pointer to proceed
        //--------------------------------
      
        /* cluster mode must take dropped frames into account */
        if (tc_cluster_mode 
          && (data.fid - tc_get_frames_dropped()) == frame_last) {
            return;
        }
      
        /* check frame id */
        if (frame_first <= data.fid && data.fid < frame_last) {
            encoder_export(&data, vob);
        } else { /* frame not in range */
            encoder_skip(&data);
        } /* frame processing loop */
      
        /* release frame buffer memory */
        encoder_dispose_vframe(&data);
        encoder_dispose_aframe(&data);
      
    } while (import_status() && !data.error_flag);
    /* main frame decoding loop */
    
    if (verbose & TC_DEBUG) {
        tc_log_info(__FILE__, "export terminated - buffer(s) empty");
    }
}

