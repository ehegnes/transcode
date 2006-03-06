/*
 *  encoder.c
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

#include "framebuffer.h"
#include "filter.h"
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

/*
 * FIXME: STILL MISSING
 * - outstream rotation support
 * - interface updates
 */

typedef struct tcencoderdata_ TCEncoderData;
struct tcencoderdata_ {
    /* flags, used internally */
    int error_flag;
    int fill_flag;

    /* frame boundaries */
    int frame_first; // XXX
    int frame_last; // XXX
    /* needed by encoder_skip */
    int saved_frame_last; // XXX

    int this_frame_last; // XXX
    int old_frame_last; // XXX

    /* can be set'd by other threads */

    vframe_list_t *venc_ptr;
    aframe_list_t *aenc_ptr;
    
    TCEncoderBuffer *buffer;

    TCFactory factory;

    TCModule vid_mod;
    TCModule aud_mod;
    TCModule mplex_mod;
};

static TCEncoderData encdata = {
    .error_flag = 0,
    .fill_flag = 0,
    .frame_first = 0,
    .frame_last = 0,
    .saved_frame_last = 0,
    .this_frame_last = 0,
    .old_frame_last = 0,
    .venc_ptr = NULL,
    .aenc_ptr = NULL,
    .buffer = NULL,
    .factory = NULL,
    .vid_mod = NULL,
    .aud_mod = NULL,
    .mplex_mod = NULL,
};

#define ACQUIRE_VID_FRAME(encp, vob, last) \
    (encp)->buffer->acquire_video_frame((encp)->buffer)
#define ACQUIRE_AUD_FRAME(encp, vob, last) \
    (encp)->buffer->acquire_audio_frame((encp)->buffer)

#define DISPOSE_VID_FRAME(encp) \
    (encp)->buffer->dispose_video_frame((encp)->buffer)
#define DISPOSE_AUD_FRAME(encp) \
    (encp)->buffer->dispose_audio_frame((encp)->buffer)

#define HAVE_DATA(encp) \
    (encp)->buffer->have_data(encp)

/* ------------------------------------------------------------ 
 *
 * export init
 *
 * ------------------------------------------------------------*/

int export_init(TCEncoderBuffer *buffer, TCFactory factory)
{
    if (!buffer) {
        tc_log_error(__FILE__, "missing encoder buffer reference");
        return 1;
    }
    encdata.buffer = buffer;

    if (!factory) {
        tc_log_error(__FILE__, "missing factory reference");
        return 1;
    }
    encdata.factory = factory;
    return 0;
}

int export_setup(const char *a_mod, const char *v_mod, const char *m_mod)
{
    int match = 0;
    const char *mod_name = NULL;

    if (verbose & TC_DEBUG) {
        tc_log_info(__FILE__, "loading export modules");
    }
    
    mod_name = (a_mod == NULL) ?TC_DEFAULT_EXPORT_AUDIO :a_mod;
    encdata.aud_mod = tc_new_module(encdata.factory, "encode", mod_name);
    if (!encdata.aud_mod) {
        tc_log_error(__FILE__, "can't load audio encoder");
        return -1;
    }
    mod_name = (v_mod == NULL) ?TC_DEFAULT_EXPORT_VIDEO :v_mod;
    encdata.vid_mod = tc_new_module(encdata.factory, "encode", mod_name);
    if (!encdata.vid_mod) {
        tc_log_error(__FILE__, "can't load video encoder");
        return -1;
    }
    mod_name = (m_mod == NULL) ?TC_DEFAULT_EXPORT_MPLEX :m_mod;
    encdata.mplex_mod = tc_new_module(encdata.factory, "multiplex", mod_name);
    if (!encdata.mplex_mod) {
        tc_log_error(__FILE__, "can't load multiplexor");
        return -1;
    }
   
    match = tc_module_match(encdata.aud_mod, encdata.mplex_mod);
    if (!match) {
        tc_log_error(__FILE__, "audio encoder incompatible "
                               "with multiplexor");
        return -1;
    }
    match = tc_module_match(encdata.vid_mod, encdata.mplex_mod);
    if (!match) {
        tc_log_error(__FILE__, "video encoder incompatible "
                               "with multiplexor");
        return -1;
    }

    return 0;
}  

/* ------------------------------------------------------------ 
 *
 * export close, unload modules
 *
 * ------------------------------------------------------------*/

void export_shutdown(void)
{
    if (verbose & TC_DEBUG) {
        tc_log_info(__FILE__, "unloading export modules");
    }

    tc_del_module(encdata.factory, encdata.mplex_mod);
    tc_del_module(encdata.factory, encdata.vid_mod);
    tc_del_module(encdata.factory, encdata.aud_mod);
}


/* ------------------------------------------------------------ 
 *
 * encoder init
 *
 * ------------------------------------------------------------*/

int encoder_init(vob_t *vob)
{
    int ret;
    const char *options = NULL;
 
    options = (vob->ex_v_string) ?vob->ex_v_string :""; 
    ret = tc_module_configure(encdata.vid_mod, options, vob);
    if (ret == TC_EXPORT_ERROR) {
        tc_log_warn(__FILE__, "video export module error: init failed");
        return -1;
    }
  
    options = (vob->ex_a_string) ?vob->ex_a_string :""; 
    ret = tc_module_configure(encdata.aud_mod, options, vob);
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
    const char *options = NULL;
 
    options = (vob->ex_m_string) ?vob->ex_m_string :""; 
    ret = tc_module_configure(encdata.mplex_mod, options, vob);
    if (ret == TC_EXPORT_ERROR) {
        tc_log_warn(__FILE__, "multiplexor module error: init failed");
        return -1;
    }

    // XXX
    tc_module_pass_extradata(encdata.vid_mod, encdata.mplex_mod);
    
    return 0;
}


/* ------------------------------------------------------------ 
 *
 * encoder close
 *
 * ------------------------------------------------------------*/

int encoder_close(void)
{
    int ret = tc_module_stop(encdata.mplex_mod);
    if (ret == TC_EXPORT_ERROR) {
        tc_log_warn(__FILE__, "multiplexor module error: stop failed");
        return -1;
    }

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

    ret = tc_module_stop(encdata.vid_mod);
    if (ret == TC_EXPORT_ERROR) {
        tc_log_warn(__FILE__, "video export module error: stop failed");
        return -1;
    }
  
    ret = tc_module_stop(encdata.aud_mod);
    if (ret == TC_EXPORT_ERROR) {
        tc_log_warn(__FILE__, "audio export module error: stop failed");
        return -1;
    }
 
    if(verbose & TC_DEBUG) {
        tc_log_info(__FILE__, "encoder stopped");
    }
    return 0;
}

/* ------------------------------------------------------------ 
 *
 * encoder main loop helpers
 *
 * ------------------------------------------------------------*/

static int alloc_buffers(TCEncoderData *data)
{
    data->venc_ptr = tc_malloc(sizeof(vframe_list_t));
    if (!data->venc_ptr) {
        goto no_vptr;
    }
    data->aenc_ptr = tc_malloc(sizeof(aframe_list_t));
    if (!data->aenc_ptr) {
        goto no_aptr;
    }
#ifdef STATBUFFER
    data->venc_ptr->internal_video_buf_0 = tc_bufalloc(SIZE_RGB_FRAME);
    data->venc_ptr->internal_video_buf_1 = data->venc_ptr->internal_video_buf_0;
    if (!data->venc_ptr->internal_video_buf_0) {
        goto no_vmem;
    }
    data->aenc_ptr->internal_audio_buf = tc_bufalloc(SIZE_PCM_FRAME);
    if (!data->aenc_ptr->internal_audio_buf) {
        goto no_amem;
    }
#endif /* STATBUFFER */
    return 0;

no_amem:
    tc_buffree(data->venc_ptr->internal_video_buf_0);
no_vmem:
    tc_free(data->aenc_ptr);
no_aptr:
    tc_free(data->venc_ptr);
no_vptr:
    return -1;
}

static void free_buffers(TCEncoderData *data)
{
#ifdef STATBUFFER
    tc_buffree(data->venc_ptr->internal_video_buf_0);
    tc_buffree(data->aenc_ptr->internal_audio_buf);
#endif /* STATBUFFER */
    tc_free(data->venc_ptr);
    tc_free(data->aenc_ptr);
}

/*
 * NOTE about counter/condition/mutex handling inside various 
 * encoder helpers.
 *
 * Code are still a little bit confusing since things aren't
 * updated or used at the same function level.
 * Code works, but isn't still well readable. 
 * We need stil more cleanup and refactoring for future releases.
 */


/*
 * dispatch the acquired frames to encoder modules, and adjust frame counters
 */
static int encoder_export(TCEncoderData *data)
{
    int video_delayed = 0;
    int ret;
    
    ret = tc_module_encode_video(data->vid_mod,
                                 data->buffer->vptr, data->venc_ptr);
    if (ret != TC_EXPORT_OK) {
        tc_log_error(__FILE__, "error encoding video frame");
        data->error_flag = 1;
    }

    if (data->venc_ptr->attributes == TC_FRAME_IS_DELAYED) {
        data->venc_ptr->attributes &= ~TC_FRAME_IS_DELAYED;
        video_delayed = 1;
    }

    if(video_delayed) {
        data->buffer->aptr->attributes |= TC_FRAME_IS_CLONED; 
        tc_log_info(__FILE__, "Delaying audio");
    } else {
        ret = tc_module_encode_audio(data->aud_mod,
                                     data->buffer->aptr, data->aenc_ptr);
        if (ret != TC_EXPORT_OK) {
            tc_log_error(__FILE__, "error encoding audio frame");
            data->error_flag = 1;
        }
    }

    ret = tc_module_multiplex(data->mplex_mod,
                              data->venc_ptr, data->aenc_ptr);
    if (ret < 0) {
        tc_log_error(__FILE__, "error multiplexing encoded frames");
        data->error_flag = 1;
    }
    
    if (verbose & TC_INFO) {
        int last = (data->frame_last == TC_FRAME_LAST) ?(-1) :data->frame_last;
        if (!data->fill_flag) {
            data->fill_flag = 1;
        }
        counter_print(1, data->buffer->frame_id, data->frame_first, last);
    }
    
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
        counter_print(0, data->buffer->frame_id, data->saved_frame_last,
                      data->frame_first-1);
    }
}

/* ------------------------------------------------------------ 
 *
 * encoder main loop
 *
 * ------------------------------------------------------------*/

#define IS_LAST_FRAME(fid) \
    (((fid) - tc_get_frames_dropped()) == frame_last)

void encoder(vob_t *vob, int frame_first, int frame_last)
{
    int err = 0;

    if (encdata.this_frame_last != frame_last) {
        encdata.old_frame_last = encdata.this_frame_last;
        encdata.this_frame_last = frame_last;
    }

    encdata.frame_first = frame_first;
    encdata.frame_last = frame_last;
    encdata.saved_frame_last = encdata.old_frame_last;
    
    err = alloc_buffers(&encdata);
    if (err) {
        tc_log_error(__FILE__, "can't allocate encoder buffers");
        return;
    }
    
    do {
        /* check for ^C signal */
        if (tc_export_stop_requested()) {
            if (verbose & TC_DEBUG) {
                tc_log_warn(__FILE__, "export canceled on user request");
            }
            return;
        }
        tc_pause();
        
        err = ACQUIRE_VID_FRAME(&encdata, vob, frame_last);
        if (err) {
            return; /* can't acquire video frame */
        }
      
        err = ACQUIRE_AUD_FRAME(&encdata, vob, frame_last);
        if (err) {
            return;  /* can't acquire frame */
        }
      
        //--------------------------------
        // need a valid pointer to proceed
        //--------------------------------
      
        /* cluster mode must take dropped frames into account */
        if (tc_cluster_mode 
          && IS_LAST_FRAME(encdata.buffer->frame_id)) {
            return;
        }
      
        /* check frame id */
        if (frame_first <= encdata.buffer->frame_id
          && encdata.buffer->frame_id < frame_last) {
            // XXX
            VFRAME_INIT(encdata.venc_ptr,
                        tc_frame_width_max, tc_frame_height_max);
            AFRAME_INIT(encdata.aenc_ptr);
            encoder_export(&encdata);
        } else { /* frame not in range */
            encoder_skip(&encdata);
        } /* frame processing loop */
      
        /* release frame buffer memory */
        DISPOSE_VID_FRAME(&encdata);
        DISPOSE_AUD_FRAME(&encdata);
      
    } while (HAVE_DATA(&encdata) && !encdata.error_flag);
    /* main frame decoding loop */
    
    free_buffers(&encdata);
    
    if (verbose & TC_DEBUG) {
        tc_log_info(__FILE__, "export terminated - buffer(s) empty");
    }
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
