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

// import export module handles
static void *export_ahandle = NULL;
static void *export_vhandle = NULL;

long frames_encoded = 0;
long frames_dropped = 0;
long frames_skipped = 0;
long frames_cloned = 0;
static pthread_mutex_t frame_counter_lock = PTHREAD_MUTEX_INITIALIZER;

static int export = 0;
static pthread_mutex_t export_lock = PTHREAD_MUTEX_INITIALIZER;

static volatile int force_exit = TC_FALSE;
static pthread_mutex_t force_exit_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t delay_video_frames_lock = PTHREAD_MUTEX_INITIALIZER;
int video_frames_delay = 0;

static int counter_encoding = 0;
static int counter_skipping = 0;

static long startsec;
static long startusec;

void tc_export_stop_nolock(void)
{
    force_exit=1;
}


int export_status(void)
{
    int ret = 0;
    
    pthread_mutex_lock(&export_lock);
    if (export == TC_ON) {
        ret = 1;
    }
    pthread_mutex_unlock(&export_lock);
    
    return ret;
}

long tc_get_frames_encoded(void)
{
    long cc;
    
    pthread_mutex_lock(&frame_counter_lock);
    cc = frames_encoded;
    pthread_mutex_unlock(&frame_counter_lock);

    return cc;
}

void tc_update_frames_encoded(long cc)
{
    pthread_mutex_lock(&frame_counter_lock);
    frames_encoded += cc;
    pthread_mutex_unlock(&frame_counter_lock);
}

long tc_get_frames_dropped(void)
{
    long cc;
    
    pthread_mutex_lock(&frame_counter_lock);
    cc = frames_dropped;
    pthread_mutex_unlock(&frame_counter_lock);
    
    return cc;
}

void tc_update_frames_dropped(long cc)
{
    pthread_mutex_lock(&frame_counter_lock);
    frames_dropped += cc;
    pthread_mutex_unlock(&frame_counter_lock);
}

long tc_get_frames_skipped(void)
{
    long cc;
    
    pthread_mutex_lock(&frame_counter_lock);
    cc = frames_skipped;
    pthread_mutex_unlock(&frame_counter_lock);
    
    return cc;
}

void tc_update_frames_skipped(long cc)
{
    pthread_mutex_lock(&frame_counter_lock);
    frames_skipped += cc;
    pthread_mutex_unlock(&frame_counter_lock);
}

long tc_get_frames_cloned(void)
{
    long cc;
    
    pthread_mutex_lock(&frame_counter_lock);
    cc = frames_cloned;
    pthread_mutex_unlock(&frame_counter_lock);
    
    return cc;
}

void tc_update_frames_cloned(long cc)
{
    pthread_mutex_lock(&frame_counter_lock);
    frames_cloned += cc;
    pthread_mutex_unlock(&frame_counter_lock);
}

static long tc_get_frames_skipped_cloned(void)
{
    long cc, cc2;
    
    pthread_mutex_lock(&frame_counter_lock);
    cc = frames_skipped;
    cc2 = frames_cloned;
    pthread_mutex_unlock(&frame_counter_lock);
    
    return(-cc + cc2);
}


void tc_set_force_exit(void)
{
    pthread_mutex_lock(&force_exit_lock);
    force_exit = 1;
    pthread_mutex_unlock(&force_exit_lock);
}

int tc_get_force_exit(void)
{
    int cc = 0;

    pthread_mutex_lock(&force_exit_lock);
    cc = force_exit;
    pthread_mutex_unlock(&force_exit_lock);

    return cc;
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
    export_ahandle = load_module(mod_name, TC_EXPORT + TC_AUDIO);
    if (export_ahandle == NULL) {
        tc_log_warn(__FILE__, "loading audio export module failed");
        return -1;
   }

    mod_name = (v_mod==NULL) ?TC_DEFAULT_EXPORT_VIDEO :v_mod;
    export_vhandle = load_module(mod_name, TC_EXPORT + TC_VIDEO);
    if (export_vhandle == NULL) {
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

    unload_module(export_ahandle);
    unload_module(export_vhandle);
}


/* ------------------------------------------------------------ 
 *
 * encoder init
 *
 * ------------------------------------------------------------*/

int encoder_init(transfer_t *export_para, vob_t *vob)
{
    int ret;
  
    pthread_mutex_lock(&export_lock);
    export = TC_ON;   
    pthread_mutex_unlock(&export_lock);
  
    export_para->flag = TC_VIDEO;
    ret = tcv_export(TC_EXPORT_INIT, export_para, vob);
    if (ret == TC_EXPORT_ERROR) {
        tc_log_warn(__FILE__, "video export module error: init failed");
        return -1;
    }
  
    export_para->flag = TC_AUDIO;
    ret = tca_export(TC_EXPORT_INIT, export_para, vob);
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

int encoder_open(transfer_t *export_para, vob_t *vob)
{
    int ret;
  
    export_para->flag = TC_VIDEO; 
    ret = tcv_export(TC_EXPORT_OPEN, export_para, vob);
    if (ret == TC_EXPORT_ERROR) {
        tc_log_warn(__FILE__, "video export module error: open failed");
        return -1;
    }
  
    export_para->flag = TC_AUDIO;
    ret = tca_export(TC_EXPORT_OPEN, export_para, vob);
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

int encoder_close(transfer_t *export_para)
{
    /* close, errors not fatal */

    export_para->flag = TC_AUDIO;
    tca_export(TC_EXPORT_CLOSE, export_para, NULL);

    export_para->flag = TC_VIDEO;
    tcv_export(TC_EXPORT_CLOSE, export_para, NULL);
  
    pthread_mutex_lock(&export_lock);
    export = TC_OFF;  
    pthread_mutex_unlock(&export_lock);

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

int encoder_stop(transfer_t *export_para)
{
    int ret;

    export_para->flag = TC_VIDEO;
    ret = tcv_export(TC_EXPORT_STOP, export_para, NULL);
    if (ret == TC_EXPORT_ERROR) {
        tc_log_warn(__FILE__, "video export module error: stop failed");
        return -1;
    }
  
    export_para->flag = TC_AUDIO;
    ret = tca_export(TC_EXPORT_STOP, export_para, NULL);
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
static void encoder_apply_vfilters(vframe_list_t *vptr, vob_t *vob)
{
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

/*
 * Apply the filter chain to current audio frame.
 * Used if no frame threads are avalaible.
 * this function should be never exported.
 */
static void encoder_apply_afilters(aframe_list_t *aptr, vob_t *vob)
{
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

/*
 * wait until a new audio frame is avalaible for encoding
 * in frame buffer.
 * When a new frame is avalaible update the encoding context
 * adjusting video frame buffer, and returns 0.
 * Return -1 if no more frames are avalaible. 
 * This usually happens when video stream ends.
 * 
 * PLEASE NOTE: here is if*N*def, in encoder.h is ifdef
 */
#ifndef ENCODER_EXPORT
static
#endif
int encoder_wait_vframe(TcEncoderData *data)
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
            if (!vimport_status() || tc_get_force_exit())  {
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
 * PLEASE NOTE: here is if*N*def, in encoder.h is ifdef
 *
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
#ifndef ENCODER_EXPORT
static
#endif
int encoder_acquire_vframe(TcEncoderData *data, vob_t *vob)
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

        if (data->fid >= data->frame_b) {
            if (verbose & TC_DEBUG) {
                tc_log_info(__FILE__, "encoder last frame finished (%d/%d)",
                                      data->fid, data->frame_b);
            }
            return 1;
        }

        tc_pause();

        if (!have_vframe_threads) {
            encoder_apply_vfilters(data->vptr, vob); 
        }
    
        /* second stage post-processing - (synchronous) */
        data->vptr->tag = TC_VIDEO|TC_POST_S_PROCESS;
        process_vid_plugins(data->vptr);
        postprocess_vid_frame(vob, data->vptr);

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
 * 
 * PLEASE NOTE: here is if*N*def, in encoder.h is ifdef
 */
#ifndef ENCODER_EXPORT
static
#endif
int encoder_wait_aframe(TcEncoderData *data)
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
            if (!aimport_status() || tc_get_force_exit()) {
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
 * PLEASE NOTE: here is if*N*def, in encoder.h is ifdef
 *
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
#ifndef ENCODER_EXPORT
static
#endif
int encoder_acquire_aframe(TcEncoderData *data, vob_t *vob)
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
      
        /* now we try to process the audio frame */
        if (!have_aframe_threads) {
            encoder_apply_afilters(data->aptr, vob);
        }
    
        /* second stage post-processing - (synchronous) */
        data->aptr->tag = TC_AUDIO|TC_POST_S_PROCESS;
        process_aud_plugins(data->aptr);

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
 * PLEASE NOTE: here is if*N*def, in encoder.h is ifdef
 *
 * dispatch the acquired frames to encoder modules, and adjust frame counters
 */
#ifndef ENCODER_EXPORT
static
#endif
int encoder_export(TcEncoderData *data, vob_t *vob)
{
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
        data->exit_on_encoder_error = 1;
    }

    /* maybe clone? */
    data->vptr->attributes = data->export_para.attributes;

    DEC_VBUF_COUNTER(ex);

    /* encode and export audio frame */
    data->export_para.buffer = data->aptr->audio_buf;
    data->export_para.size   = data->aptr->audio_size;
    data->export_para.attributes = data->aptr->attributes;
    
    data->export_para.flag   = TC_AUDIO;
    
    if(video_frames_delay > 0) {
        pthread_mutex_lock(&delay_video_frames_lock);
        --video_frames_delay;
        pthread_mutex_unlock(&delay_video_frames_lock);
        data->aptr->attributes |= TC_FRAME_IS_CLONED; 
        tc_log_info(__FILE__, "Delaying audio (%d)", 
                              vob->video_frames_delay);
    } else {
        if (tca_export(TC_EXPORT_ENCODE, &data->export_para, vob) < 0) {
            tc_log_warn(__FILE__, "error encoding audio frame");
            data->exit_on_encoder_error = 1;
        }
 
        /* maybe clone? */
        data->aptr->attributes = data->export_para.attributes;
    }

    DEC_ABUF_COUNTER(ex);

    if (verbose & TC_INFO) {
        if (!data->fill_flag) {
            data->fill_flag = 1;
        }
     
        counter_print(data->frame_a, data->fid, "encoding", startsec, startusec, 
                        ((vob->video_out_file==NULL) ?vob->audio_out_file :vob->video_out_file), 
                        data->vptr->thread_id);
    }
    
    /* on success, increase global frame counter */
    tc_update_frames_encoded(1); 
    return data->exit_on_encoder_error;
}


/* 
 * PLEASE NOTE: here is if*N*def, in encoder.h is ifdef
 * 
 * fake encoding, simply adjust frame counters.
 */
#ifndef ENCODER_EXPORT
static
#endif
void encoder_skip(TcEncoderData *data)
{
    if (verbose & TC_INFO) {
        if (!data->fill_flag) {
            data->fill_flag = 1;
        }
        counter_print(data->last_frame_b, data->fid, "skipping", startsec, startusec, 
                      "/dev/null", data->vptr->thread_id);
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
 * PLEASE NOTE: here is if*N*def, in encoder.h is ifdef
 *
 * put back to frame buffer an acquired video frame.
 */
#ifndef ENCODER_EXPORT
static
#endif
void encoder_dispose_vframe(TcEncoderData *data)
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
 * PLEASE NOTE: here is if*N*def, in encoder.h is ifdef
 *
 * put back to frame buffer an acquired audio frame
 */
#ifndef ENCODER_EXPORT
static
#endif
void encoder_dispose_aframe(TcEncoderData *data)
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


void encoder(vob_t *vob, int frame_a, int frame_b)
{
    int err = 0;
    TcEncoderData data;

    static int this_frame_b=0;
    static int last_frame_b=0;

    if (this_frame_b != frame_b) {
        last_frame_b = this_frame_b;
        this_frame_b = frame_b;
    }

    TC_ENCODER_DATA_INIT(&data);

    data.frame_a = frame_a;
    data.frame_b = frame_b;
    data.last_frame_b = last_frame_b;
    
    counter_encoding = 0;
    counter_skipping = 0;

    do {
        /* check for ^C signal */
        if (tc_get_force_exit()) {
            if (verbose & TC_DEBUG) {
                tc_log_warn(__FILE__, "export canceled on user request");
            }
            return;
        }
     
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
          && (data.fid - tc_get_frames_dropped()) == frame_b) {
            return;
        }
      
        /* check frame id */
        if (frame_a <= data.fid && data.fid < frame_b) {
            if (!counter_encoding) {
                counter_init(&startsec, &startusec);
                ++counter_encoding;
                if (verbose & TC_INFO && counter_skipping) { 
                    tc_log_info(__FILE__, "\n"); // XXX
                }
            }

            encoder_export(&data, vob);
        } else { /* frame not in range */
            if (!counter_skipping) {
                counter_init(&startsec, &startusec);
                ++counter_skipping;
            }
    
            encoder_skip(&data);
        } /* frame processing loop */
      
        /* release frame buffer memory */
        encoder_dispose_vframe(&data);
        encoder_dispose_aframe(&data);
      
    } while (import_status() && !data.exit_on_encoder_error);
    /* main frame decoding loop */
    
    if (verbose & TC_DEBUG) {
        tc_log_info(__FILE__, "export terminated - buffer(s) empty");
    }
}

