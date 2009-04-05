/*
 *  encoder.c -- transcode export layer module, implementation.
 *
 *  Copyright (C) Thomas Oestreich - June 2001
 *  Updated and partially rewritten by
 *  Francesco Romani - January 2006
 *  New rotation code written by
 *  Francesco Romani - May 2006
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
#include "framebuffer.h"
#include "filter.h"
#include "counter.h"
#include "video_trans.h"
#include "audio_trans.h"
#include "decoder.h"
#include "encoder.h"
#include "frame_threads.h"

#include "libtc/tcframes.h"

#include <stdint.h>

/*************************************************************************/
/* Our data structure forward declaration                                */

typedef struct tcencoderdata_ TCEncoderData;

/*************************************************************************/
/* private function prototypes                                           */

/* new-style encoder */

static int encoder_export(TCEncoderData *data, vob_t *vob);
static void encoder_skip(TCEncoderData *data, int out_of_range);

/* rest of API is already public */

/* misc helpers */
static int need_stop(TCEncoderData *encdata);
static int is_last_frame(TCEncoderData *encdata, int cluster_mode);
static void export_update_formats(vob_t *vob, const TCModuleInfo *vinfo,
                                  const TCModuleInfo *ainfo);
static int alloc_buffers(TCEncoderData *data);
static void free_buffers(TCEncoderData *data);


/*************************************************************************/
/* real encoder code                                                     */


struct tcencoderdata_ {
    /* flags, used internally */
    int             error_flag;
    int             fill_flag;

    /* frame boundaries */
    int             frame_first; // XXX
    int             frame_last; // XXX
    /* needed by encoder_skip */
    int             saved_frame_last; // XXX

    int             this_frame_last; // XXX
    int             old_frame_last; // XXX

    TCEncoderBuffer *buffer;

    TCFrameVideo    *venc_ptr;
    TCFrameAudio    *aenc_ptr;

    TCFactory       factory;

    TCModule        vid_mod;
    TCModule        aud_mod;

    TCRotateContext rotor_data;
};

static TCEncoderData encdata = {
    .error_flag         = 0,
    .fill_flag          = 0,
    .frame_first        = 0,
    .frame_last         = 0,
    .saved_frame_last   = 0,
    .this_frame_last    = 0,
    .old_frame_last     = 0,
    .buffer             = NULL,
    .venc_ptr           = NULL,
    .aenc_ptr           = NULL,
    .factory            = NULL,
    .vid_mod            = NULL,
    .aud_mod            = NULL,
};


#define RESET_ATTRIBUTES(ptr)   do { \
        (ptr)->attributes = 0; \
} while (0)

/*
 * is_last_frame:
 *      check if current frame it's supposed to be the last one in
 *      encoding frame range. Catch all all known special cases
 * 
 * Parameters:
 *           encdata: fetch current frame id from this structure reference.
 *      cluster_mode: boolean flag. When in cluster mode we need to take
 *                    some special care.
 * Return value:
 *     !0: current frame is supposed to be the last one
 *      0: otherwise
 */
static int is_last_frame(TCEncoderData *encdata, int cluster_mode)
{
    int fid = encdata->buffer->frame_id;
    if (cluster_mode) {
        fid -= tc_get_frames_dropped();
    }

    if ((encdata->buffer->vptr->attributes & TC_FRAME_IS_END_OF_STREAM
      || encdata->buffer->aptr->attributes & TC_FRAME_IS_END_OF_STREAM)) {
        /* `consume' the flag(s) */
        encdata->buffer->vptr->attributes &= ~TC_FRAME_IS_END_OF_STREAM;
        encdata->buffer->aptr->attributes &= ~TC_FRAME_IS_END_OF_STREAM;
        return 1;
    }
    return (fid == encdata->frame_last);
}

/*
 * export_update_formats:
 *      coerce exported formats to the default ones from the loaded
 *      encoder modules IF AND ONLY IF user doesn't have requested
 *      specific ones.
 *
 *      That's a temporary workaround until we have a full-NMS
 *      export layer.
 *
 * Parameters:
 *        vob: pointer to vob_t structure to update.
 *      vinfo: pointer to TCModuleInfo of video encoder module.
 *      ainfo: pointer to TCModuleInfo of audio encoder module.
 * Return value:
 *      None
 */
static void export_update_formats(vob_t *vob, const TCModuleInfo *vinfo,
                                  const TCModuleInfo *ainfo)
{
    if (vob == NULL || vinfo == NULL || ainfo == NULL) {
        /* should never happen */
        tc_log_error(__FILE__, "missing export formats references");
    }
    /* 
     * OK, that's pretty hackish since export_attributes should
     * go away in near future. Neverthless, ex_a_codec features
     * a pretty unuseful default (CODEC_MP3), so we don't use
     * such default value to safely distinguish between -N given
     * or not given.
     * And so we must use another flag, and export_attributes are
     * the simplest things that work, now/
     */
    if (!(vob->export_attributes & TC_EXPORT_ATTRIBUTE_VCODEC)) {
        vob->ex_v_codec = vinfo->codecs_video_out[0];
    }
    if (!(vob->export_attributes & TC_EXPORT_ATTRIBUTE_ACODEC)) {
        vob->ex_a_codec = ainfo->codecs_audio_out[0];
    }
}

/* ------------------------------------------------------------
 *
 * export init
 *
 * ------------------------------------------------------------*/

int tc_export_init(TCEncoderBuffer *buffer, TCFactory factory)
{
    if (buffer == NULL) {
        tc_log_error(__FILE__, "missing encoder buffer reference");
        return TC_ERROR;
    }
    encdata.buffer  = buffer;
    encdata.factory = factory;
    return TC_OK;
}

int tc_export_setup(vob_t *vob,
                 const char *a_mod, const char *v_mod, const char *m_mod)
{
    int match = 0;
    const char *mod_name = NULL;

    tc_debug(TC_DEBUG_MODULES, "loading export modules");

    mod_name = (a_mod == NULL) ?TC_DEFAULT_EXPORT_AUDIO :a_mod;
    encdata.aud_mod = tc_new_module(encdata.factory, "encode", mod_name, TC_AUDIO);
    if (!encdata.aud_mod) {
        tc_log_error(__FILE__, "can't load audio encoder");
        return TC_ERROR;
    }
    mod_name = (v_mod == NULL) ?TC_DEFAULT_EXPORT_VIDEO :v_mod;
    encdata.vid_mod = tc_new_module(encdata.factory, "encode", mod_name, TC_VIDEO);
    if (!encdata.vid_mod) {
        tc_log_error(__FILE__, "can't load video encoder");
        return TC_ERROR;
    }
    mod_name = (m_mod == NULL) ?TC_DEFAULT_EXPORT_MPLEX :m_mod;
    encdata.mplex_mod = tc_new_module(encdata.factory, "multiplex", mod_name,
                                      TC_VIDEO|TC_AUDIO);
    if (!encdata.mplex_mod) {
        tc_log_error(__FILE__, "can't load multiplexor");
        return TC_ERROR;
    }
    export_update_formats(vob, tc_module_get_info(encdata.vid_mod),
                               tc_module_get_info(encdata.aud_mod));

    match = tc_module_match(vob->ex_a_codec,
                            encdata.aud_mod, encdata.mplex_mod);
    if (!match) {
        tc_log_error(__FILE__, "audio encoder incompatible "
                               "with multiplexor");
        return TC_ERROR;
    }
    match = tc_module_match(vob->ex_v_codec,
                            encdata.vid_mod, encdata.mplex_mod);
    if (!match) {
        tc_log_error(__FILE__, "video encoder incompatible "
                               "with multiplexor");
        return TC_ERROR;
    }
    tc_rotate_init(&encdata.rotor_data,
                   vob->video_out_file, vob->audio_out_file);

    return TC_OK;
}

/* ------------------------------------------------------------
 *
 * export close, unload modules
 *
 * ------------------------------------------------------------*/

void tc_export_shutdown(void)
{
    tc_debug(TC_DEBUG_MODULES, "unloading export modules");

    tc_del_module(encdata.factory, encdata.mplex_mod);
    tc_del_module(encdata.factory, encdata.vid_mod);
    tc_del_module(encdata.factory, encdata.aud_mod);
}


/* ------------------------------------------------------------
 *
 * encoder init
 *
 * ------------------------------------------------------------*/

int tc_encoder_init(vob_t *vob)
{
    int ret;
    const char *options = NULL;

    ret = alloc_buffers(&encdata);
    if (ret != TC_OK) {
        tc_log_error(__FILE__, "can't allocate encoder buffers");
        return TC_ERROR;
    }

    options = (vob->ex_v_string) ?vob->ex_v_string :"";
    ret = tc_module_configure(encdata.vid_mod, options, vob);
    if (ret != TC_OK) {
        tc_log_error(__FILE__, "video export module error: init failed");
        return TC_ERROR;
    }

    options = (vob->ex_a_string) ?vob->ex_a_string :"";
    ret = tc_module_configure(encdata.aud_mod, options, vob);
    if (ret != TC_OK) {
        tc_log_warn(__FILE__, "audio export module error: init failed");
        return TC_ERROR;
    }

    return TC_OK;
}



/* ------------------------------------------------------------
 *
 * encoder stop
 *
 * ------------------------------------------------------------*/

int tc_encoder_stop(void)
{
    int ret;

    ret = tc_module_stop(encdata.vid_mod);
    if (ret != TC_OK) {
        tc_log_warn(__FILE__, "video export module error: stop failed");
        return TC_ERROR;
    }

    ret = tc_module_stop(encdata.aud_mod);
    if (ret != TC_OK) {
        tc_log_warn(__FILE__, "audio export module error: stop failed");
        return TC_ERROR;
    }

    free_buffers(&encdata);

    tc_debug(TC_DEBUG_CLEANUP, "encoder stopped");
    return TC_OK;
}

/* ------------------------------------------------------------
 *
 * encoder main loop helpers
 *
 * ------------------------------------------------------------*/

static int alloc_buffers(TCEncoderData *data)
{
    data->venc_ptr = vframe_alloc_single();
    if (data->venc_ptr == NULL) {
        goto no_vframe;
    }
    data->aenc_ptr = aframe_alloc_single();
    if (data->aenc_ptr == NULL) {
        goto no_aframe;
    }
    return TC_OK;

no_aframe:
    tc_del_video_frame(data->venc_ptr);
no_vframe:
    return TC_ERROR;
}

static void free_buffers(TCEncoderData *data)
{
    tc_del_video_frame(data->venc_ptr);
    tc_del_audio_frame(data->aenc_ptr);
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
static int encoder_export(TCEncoderData *data, vob_t *vob)
{
    int video_delayed = 0;
    int ret;

    /* remove spurious attributes */
    RESET_ATTRIBUTES(data->venc_ptr);
    RESET_ATTRIBUTES(data->aenc_ptr);

    /* step 1: encode video */
    ret = tc_module_encode_video(data->vid_mod,
                                 data->buffer->vptr, data->venc_ptr);
    if (ret != TC_OK) {
        tc_log_error(__FILE__, "error encoding video frame");
        data->error_flag = 1;
    }
    if (data->venc_ptr->attributes & TC_FRAME_IS_DELAYED) {
        data->venc_ptr->attributes &= ~TC_FRAME_IS_DELAYED;
        video_delayed = 1;
    }

    /* step 2: encode audio */
    if (video_delayed) {
        data->buffer->aptr->attributes |= TC_FRAME_IS_CLONED;
        tc_log_info(__FILE__, "Delaying audio");
    } else {
        ret = tc_module_encode_audio(data->aud_mod,
                                     data->buffer->aptr, data->aenc_ptr);
        if (ret != TC_OK) {
            tc_log_error(__FILE__, "error encoding audio frame");
            data->error_flag = 1;
        }
    }

    /* step 3: multiplex and rotate */
    // FIXME: Do we really need bytes-written returned from this, or can
    //        we just return TC_OK/TC_ERROR like other functions? --AC
    ret = tc_module_multiplex(data->mplex_mod,
                              data->venc_ptr, data->aenc_ptr);
    if (ret < 0) {
        tc_log_error(__FILE__, "error multiplexing encoded frames");
        data->error_flag = 1;
    }
    data->error_flag = TC_ROTATE_IF_NEEDED(&encdata.rotor_data, vob, ret);

    /* step 4: show and update stats */
    if (tc_progress_meter) {
        int last = (data->frame_last == TC_FRAME_LAST)
                        ?(-1) :data->frame_last;
        if (!data->fill_flag) {
            data->fill_flag = 1;
        }
        counter_print(1, data->buffer->frame_id, data->frame_first, last);
    }

    tc_update_frames_encoded(1);
    return (data->error_flag) ?TC_ERROR :TC_OK;
}


#define RETURN_IF_NOT_OK(RET, KIND) do { \
    if ((RET) != TC_OK) { \
        tc_log_error(__FILE__, "error encoding final %s frame", (KIND)); \
        return TC_ERROR; \
    } \
} while (0)

/*************************************************************************/

/*
 * fake encoding, simply adjust frame counters.
 */
static void encoder_skip(TCEncoderData *data, int out_of_range)
{
    if (tc_progress_meter) {
        if (!data->fill_flag) {
            data->fill_flag = 1;
        }
        if (out_of_range) {
            counter_print(0, data->buffer->frame_id, data->saved_frame_last,
                          data->frame_first-1);
        } else { /* skipping from --frame_interval */
            int last = (data->frame_last == TC_FRAME_LAST) ?(-1) :data->frame_last;
            counter_print(1, data->buffer->frame_id, data->frame_first, last);
        }
    }
    if (out_of_range) {
        data->buffer->vptr->attributes |= TC_FRAME_IS_OUT_OF_RANGE;
        data->buffer->aptr->attributes |= TC_FRAME_IS_OUT_OF_RANGE;
    }
}

static int need_stop(TCEncoderData *encdata)
{
    return (!tc_running() || encdata->error_flag);
}

/* ------------------------------------------------------------
 *
 * encoder main loop
 *
 * ------------------------------------------------------------*/

void tc_encoder_loop(vob_t *vob, int frame_first, int frame_last)
{
    int err  = 0;
    int eos  = 0; /* End Of Stream flag */
    int skip = 0; /* Frames to skip before next frame to encode */

    if (verbose >= TC_DEBUG) {
        tc_log_info(__FILE__,
                    "encoder loop started [%i/%i)",
                    frame_first, frame_last);
    }

    if (encdata.this_frame_last != frame_last) {
        encdata.old_frame_last  = encdata.this_frame_last;
        encdata.this_frame_last = frame_last;
    }

    encdata.error_flag  = 0; /* reset */
    encdata.frame_first = frame_first;
    encdata.frame_last  = frame_last;
    encdata.saved_frame_last = encdata.old_frame_last;

    while (!eos && !need_stop(&encdata)) {
        /* stop here if pause requested */
        tc_pause();

        err = encdata.buffer->acquire_video_frame(encdata.buffer, vob);
        if (err) {
            tc_debug(TC_DEBUG_PRIVATE,
                     "failed to acquire next raw video frame");
            break; /* can't acquire video frame */
        }

        err = encdata.buffer->acquire_audio_frame(encdata.buffer, vob);
        if (err) {
            tc_debug(TC_DEBUG_PRIVATE,
                     "failed to acquire next raw audio frame");
            break;  /* can't acquire frame */
        }

        eos = is_last_frame(&encdata, tc_cluster_mode);

        /* check frame id */
        if (!eos && (frame_first <= encdata.buffer->frame_id
          && encdata.buffer->frame_id < frame_last)) {
            if (skip > 0) { /* skip frame */
                encoder_skip(&encdata, 0);
                skip--;
            } else { /* encode frame */
                encoder_export(&encdata, vob);
                skip = vob->frame_interval - 1;
            }
        } else { /* frame not in range */
            encoder_skip(&encdata, 1);
        } /* frame processing loop */

        /* release frame buffer memory */
        encdata.buffer->dispose_video_frame(encdata.buffer);
        encdata.buffer->dispose_audio_frame(encdata.buffer);
    }
    /* main frame decoding loop */

    if (eos) {
        tc_debug(TC_DEBUG_CLEANUP,
                 "encoder last frame finished (%i/%i)",
                    encdata.buffer->frame_id, encdata.frame_last);
    } 
    tc_debug(TC_DEBUG_CLEANUP,
             "export terminated - buffer(s) empty");
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

