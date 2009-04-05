/*
 *  multiplexor.c -- transcode multiplexor, implementation.
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

typedef struct tcrotatecontext_ TCRotateContext;

/*************************************************************************/
/* private function prototypes                                           */

/* new-style rotation support */
static void tc_rotate_init(TCRotateContext *rotor,
                           const char *video_base_name,
                           const char *audio_base_name);

static void tc_rotate_set_frames_limit(TCRotateContext *rotor,
                                       vob_t *vob, uint32_t frames);
static void tc_rotate_set_bytes_limit(TCRotateContext *rotor,
                                      vob_t *vob, uint64_t bytes);

static void tc_rotate_output_name(TCRotateContext *rotor, vob_t *vob);

static int tc_rotate_if_needed_null(TCRotateContext *rotor,
                                    vob_t *vob, uint32_t bytes);
static int tc_rotate_if_needed_by_frames(TCRotateContext *rotor,
                                         vob_t *vob, uint32_t bytes);
static int tc_rotate_if_needed_by_bytes(TCRotateContext *rotor,
                                        vob_t *vob, uint32_t bytes);

/* rest of API is already public */

/*************************************************************************/

/*
 * new encoder module design principles
 * 1) keep it simple, stupid
 * 2) to have more than one encoder doesn't make sense in transcode, so
 * 3) new encoder will be monothread, like the old one
 */

/*************************************************************************/
/*************************************************************************/

/*
 * new-style output rotation support. Always avalaible, but
 * only new code is supposed to use it.
 * This code is private since only encoder code it's supposed
 * to use it. If this change, this code will be put in a
 * separate .c/.h pair.
 *
 * The tricky part of this code it's mainly the
 * vob->{video,audio}_out_file mangling.
 * This it's still done mainly for legacy reasons.
 * After every rotation, such fields will be updated to point
 * not to real initialization data, but to private buffers of (a)
 * TCRotateContext strucutre. This can hardly seen as good, and
 * should be changed/improved in future releases.
 * Anyway, original values of mentioned field isn't lost since it
 * will be stored in TCRotateContext.{video,audio}_base_name.
 * ------------------------------------------------------------
 */

/*
 * TCExportRotate:
 *    Generic function called after *every* frame was encoded.
 *    Rotate output file(s) if condition incapsulate in specific
 *    functions is satisfied.
 *
 * Parameters:
 *    rotor: TCRotateContext to use to check condition.
 *      vob: pointer to vob_t structure to update with new
 *           export file(s) name after succesfull rotation.
 *    bytes: total size of byte encoded (Audio + Video) in last
 *           rencoding loop.
 * Return value:
 *    TC_OK: successful.
 *    TC_ERROR: error happened and notified using tc_log*().
 *
 *    Of course no error can happen if rotating condition isn't met
 *    (so no rotation it's supposed to happen).
 *    Please note that caller code CANNOT know when rotation happens:
 *    This is a feature, not a bug! Having rotation policy incapsulated
 *    into this code and rotation machinery transparent to caller
 *    it's EXACTLY the purpose oft this code! :)
 */
typedef int (*TCExportRotate)(TCRotateContext *rotor, vob_t *vob,
                              uint32_t bytes);


struct tcrotatecontext_ {
    char            video_path_buf[PATH_MAX+1];
    char            audio_path_buf[PATH_MAX+1];
    const char      *video_base_name;
    const char      *audio_base_name;
    uint32_t        chunk_num;
    int             null_flag;

    uint32_t        chunk_frames;

    uint64_t        encoded_bytes;
    uint64_t        chunk_bytes;

    TCExportRotate  rotate_if_needed;
};

/*************************************************************************/


/* macro goody for output rotation request */
#define TC_ROTATE_IF_NEEDED(rotor, vob, bytes) \
    ((rotor)->rotate_if_needed((rotor), (vob), bytes))

/*
 * tc_rotate_init:
 *    initialize a TCRotateContext with given basenames both for
 *    audio and video output files.
 *    Uses null rotation function as default rotation function:
 *    this means that output rotation just never happen.
 *
 * Parameters:
 *              rotor: pointer to a TCRotateContext structure to
 *                     initialize.
 *    video_base_name: basename for main export file (Audio + Video).
 *    audio_base_name: basename for auxiliary export file
 *                     (separate audio track).
 * Return value:
 *    None.
 */
static void tc_rotate_init(TCRotateContext *rotor,
                           const char *video_base_name,
                           const char *audio_base_name)
{
    if (rotor != NULL) {
        memset(rotor, 0, sizeof(TCRotateContext));
        rotor->video_base_name = video_base_name;
        rotor->audio_base_name = audio_base_name;
        if (video_base_name == NULL || strlen(video_base_name) == 0
         || strcmp(video_base_name, "/dev/null") == 0) {
            rotor->null_flag = TC_TRUE;
        } else {
            rotor->null_flag = TC_FALSE;
            strlcpy(rotor->video_path_buf, video_base_name,
                    sizeof(rotor->video_path_buf));
            /*
             * FIXME: Yep, this taste like a duplicate.
             * The whole *_out_file thing need a deep review,
             * but I want to go a little ahead with the whole
             * NMS-powered export layer and write a few more
             * NMS export modules before to go with this. -- FR
             */
            if (audio_base_name == NULL || strlen(audio_base_name) == 0
              || strcmp(audio_base_name, video_base_name) == 0
              || strcmp(audio_base_name, "/dev/null") == 0) {
               /*
                * DO NOT separate export audio track, use the same
                * export file both for audio and for video
                */
                strlcpy(rotor->audio_path_buf, video_base_name,
                        sizeof(rotor->audio_path_buf));
            } else {
                /* separate audio file */
                strlcpy(rotor->audio_path_buf, audio_base_name,
                        sizeof(rotor->audio_path_buf));
            }
        }
        rotor->rotate_if_needed = tc_rotate_if_needed_null;
    }
}

/*
 * tc_rotate_set {frames,bytes}_limit:
 *    setup respecitvely frames and bytes limit for each output chunk.
 *    When calling this function user ask for rotation, so they also
 *    directly updates vob.{video,audio}_out_file so even first
 *    tc_encoder_open() later call will uses names of the right format
 *    (i.e. with the same layout of second and further chunks).
 *    This is done in order to avoid any later rename() and disomogeneities
 *    in output file name as experienced in transcode 1.0.x and before.
 *
 *    Calling this functions multiple times will not hurt anything,
 *    but only the last limit set will be honoured. In other words,
 *    it's impossible (yet) to limit output BOTH for frames and for size.
 *    This may change in future releases.
 *
 * Parameters:
 *    rotor: TCRotateContext to set limit on.
 *      vob: pointer to vob structure to update.
 *   frames: frame limit for each output chunk.
 *    bytes: size limit for each output chunk.
 * Return value:
 *    None
 * Side effects:
 *    vob parameter will be updated. Modified fields:
 *    video_out_file, audio_out_file.
 */

#define PREPARE_OUTPUT_NAME(rotor, vob) \
    if ((rotor)->chunk_num == 0) \
        tc_rotate_output_name((rotor), (vob))

static void tc_rotate_set_frames_limit(TCRotateContext *rotor,
                                       vob_t *vob, uint32_t frames)
{
    if (rotor != NULL && !rotor->null_flag) {
        rotor->chunk_frames = frames;
        rotor->rotate_if_needed = tc_rotate_if_needed_by_frames;
        PREPARE_OUTPUT_NAME(rotor, vob);
    }
}

static void tc_rotate_set_bytes_limit(TCRotateContext *rotor,
                                      vob_t *vob, uint64_t bytes)
{
    if (rotor != NULL && !rotor->null_flag) {
        rotor->chunk_bytes = bytes;
        rotor->rotate_if_needed = tc_rotate_if_needed_by_bytes;
        PREPARE_OUTPUT_NAME(rotor, vob);
    }
}

#undef PREPARE_OUTPUT_NAME

/* helpers ***************************************************************/

/*
 * all rotation helpers uses at least if()s as possible, so we must
 * drop paranoia here
 */

/* 
 * tc_rotate_output_name:
 *    make names of new main/auxiliary output file chunk and updates
 *    vob fields accordingly.
 *
 * Parameters:
 *    rotor: TCRotateContext to use to make new output name(s).
 *      vob: pointer to vob_t structure to update.
 * Return value:
 *    none.
 */

/* pretty naif, yet. */
/* FIXME: OK, we must deeply review the whole *out-file thing ASAP */
static void tc_rotate_output_name(TCRotateContext *rotor, vob_t *vob)
{
    tc_snprintf(rotor->video_path_buf, sizeof(rotor->video_path_buf),
                "%s-%03i", rotor->video_base_name, rotor->chunk_num);
    tc_snprintf(rotor->audio_path_buf, sizeof(rotor->audio_path_buf),
                "%s-%03i", rotor->audio_base_name, rotor->chunk_num);
    vob->video_out_file = rotor->video_path_buf;
    vob->audio_out_file = rotor->audio_path_buf;
    rotor->chunk_num++;
}

/*************************************************************************/
/*
 * real rotation policy implementations. Rotate output file(s)
 * respectively:
 *  - never (_null)
 *  - when encoded frames reach limit (_by_frames)
 *  - when encoded AND written *bytes* reach limit (_by_bytes).
 *
 * For details see documentation of TCExportRotate above.
 */

#define ROTATE_UPDATE_COUNTERS(bytes) do { \
    rotor->encoded_bytes += (bytes); \
} while (0);

static int tc_rotate_if_needed_null(TCRotateContext *rotor,
                                    vob_t *vob, uint32_t bytes)
{
    ROTATE_UPDATE_COUNTERS(bytes);
    return TC_OK;
}

#define ROTATE_COMMON_CODE(rotor, vob) do { \
    ret = tc_encoder_close(); \
    if (ret != TC_OK) { \
        tc_log_error(__FILE__, "unable to close output stream"); \
        ret = TC_ERROR; \
    } else { \
        tc_rotate_output_name((rotor), (vob)); \
        tc_log_info(__FILE__, "rotating video output stream to %s", \
                               (rotor)->video_path_buf); \
        tc_log_info(__FILE__, "rotating audio output stream to %s", \
                               (rotor)->audio_path_buf); \
        ret = tc_encoder_open((vob)); \
        if (ret != TC_OK) { \
            tc_log_error(__FILE__, "unable to reopen output stream"); \
            ret = TC_ERROR; \
        } \
    } \
} while (0)


static int tc_rotate_if_needed_by_frames(TCRotateContext *rotor,
                                         vob_t *vob, uint32_t bytes)
{
    int ret = TC_OK;
    ROTATE_UPDATE_COUNTERS(bytes);

    if (tc_get_frames_encoded() >= rotor->chunk_frames) {
        ROTATE_COMMON_CODE(rotor, vob);
    }
    return ret;
}

static int tc_rotate_if_needed_by_bytes(TCRotateContext *rotor,
                                        vob_t *vob, uint32_t bytes)
{
    int ret = TC_OK;
    ROTATE_UPDATE_COUNTERS(bytes);

    if (rotor->encoded_bytes >= rotor->chunk_bytes) {
        ROTATE_COMMON_CODE(rotor, vob);
    }
    return ret;
}

#undef ROTATE_COMMON_CODE
#undef ROTATE_UPDATE_COUNTERS

/*************************************************************************/
/*************************************************************************/
/* real encoder code                                                     */


struct tcencoderdata_ {
    TCFactory       factory;

    TCModule        mplex_main;
    TCModule        mplex_aux;

    TCRotateContext rotor_data;
};

static TCEncoderData encdata = {
    .factory    = NULL,
    .mplex_main = NULL,
    .mlex_aux   = NULL,
    /* rotor_data explicitely initialized later */
};


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
 * encoder open
 *
 * ------------------------------------------------------------*/

int tc_encoder_open(vob_t *vob)
{
    int ret;
    const char *options = NULL;

    options = vob->ex_m_string ? vob->ex_m_string : "";
    ret = tc_module_configure(encdata.mplex_mod, options, vob);
    if (ret == TC_ERROR) {
        tc_log_warn(__FILE__, "multiplexor module error: init failed");
        return TC_ERROR;
    }

    // XXX
    tc_module_pass_extradata(encdata.vid_mod, encdata.mplex_mod);

    return TC_OK;
}


/* ------------------------------------------------------------
 *
 * encoder close
 *
 * ------------------------------------------------------------*/

int tc_encoder_close(void)
{
    int ret;

    /* old style code handle flushing in modules, not here */
    ret = encoder_flush(&encdata);
    if (ret != TC_OK) {
        tc_log_warn(__FILE__, "error while closing encoder: flush failed");
        return TC_ERROR;
    }

    ret = tc_module_stop(encdata.mplex_mod);
    if (ret != TC_OK) {
        tc_log_warn(__FILE__, "multiplexor module error: stop failed");
        return TC_ERROR;
    }

    tc_debug(TC_DEBUG_CLEANUP, "encoder closed");
    return TC_OK;
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


void tc_export_rotation_limit_frames(vob_t *vob, uint32_t frames)
{
    tc_rotate_set_frames_limit(&encdata.rotor_data, vob, frames);
}

void tc_export_rotation_limit_megabytes(vob_t *vob, uint32_t megabytes)
{
    tc_rotate_set_bytes_limit(&encdata.rotor_data,
                              vob, megabytes * 1024 * 1024);
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

