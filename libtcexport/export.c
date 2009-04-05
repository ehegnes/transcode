/*
 *  encoder-common.c -- asynchronous encoder runtime control and statistics.
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
# include "config.h"
#endif

#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

#include "libtc/libtc.h"
#include "export.h"
#include "encoder.h"
#include "multiplexor.h"


/*************************************************************************/
/* frame counters                                                        */
/*************************************************************************/

/* counter, for stats and more */
static uint32_t frames_encoded = 0;
static uint32_t frames_dropped = 0;
static uint32_t frames_skipped = 0;
static uint32_t frames_cloned  = 0;
/* counters can be accessed by other (ex: import) threads */
static pthread_mutex_t frame_counter_lock = PTHREAD_MUTEX_INITIALIZER;


uint32_t tc_get_frames_encoded(void)
{
    uint32_t val;

    pthread_mutex_lock(&frame_counter_lock);
    val = frames_encoded;
    pthread_mutex_unlock(&frame_counter_lock);

    return val;
}

void tc_update_frames_encoded(uint32_t val)
{
    pthread_mutex_lock(&frame_counter_lock);
    frames_encoded += val;
    pthread_mutex_unlock(&frame_counter_lock);
}

uint32_t tc_get_frames_dropped(void)
{
    uint32_t val;

    pthread_mutex_lock(&frame_counter_lock);
    val = frames_dropped;
    pthread_mutex_unlock(&frame_counter_lock);

    return val;
}

void tc_update_frames_dropped(uint32_t val)
{
    pthread_mutex_lock(&frame_counter_lock);
    frames_dropped += val;
    pthread_mutex_unlock(&frame_counter_lock);
}

uint32_t tc_get_frames_skipped(void)
{
    uint32_t val;

    pthread_mutex_lock(&frame_counter_lock);
    val = frames_skipped;
    pthread_mutex_unlock(&frame_counter_lock);

    return val;
}

void tc_update_frames_skipped(uint32_t val)
{
    pthread_mutex_lock(&frame_counter_lock);
    frames_skipped += val;
    pthread_mutex_unlock(&frame_counter_lock);
}

uint32_t tc_get_frames_cloned(void)
{
    uint32_t val;

    pthread_mutex_lock(&frame_counter_lock);
    val = frames_cloned;
    pthread_mutex_unlock(&frame_counter_lock);

    return val;
}

void tc_update_frames_cloned(uint32_t val)
{
    pthread_mutex_lock(&frame_counter_lock);
    frames_cloned += val;
    pthread_mutex_unlock(&frame_counter_lock);
}

uint32_t tc_get_frames_skipped_cloned(void)
{
    uint32_t s, c;

    pthread_mutex_lock(&frame_counter_lock);
    s = frames_skipped;
    c = frames_cloned;
    pthread_mutex_unlock(&frame_counter_lock);

    return (c - s);
}

/*************************************************************************/
/* the export layer facade                                               */
/*************************************************************************/

/*************************************************************************/

/*
 * new encoder module design principles
 * 1) keep it simple, stupid
 * 2) to have more than one encoder doesn't make sense in transcode, so
 * 3) new encoder will be monothread, like the old one
 */


void tc_export_rotation_limit_frames(vob_t *vob, uint32_t frames)
{
}

void tc_export_rotation_limit_megabytes(vob_t *vob, uint32_t megabytes)
{
}


int tc_export_setup(vob_t *vob, TCEncoderBuffer *buffer, TCFactory factory)
{
}

void tc_export_shutdown(void)
{
}


int tc_export_init(vob_t *vob)
{
}

int tc_export_open(vob_t *vob)
{
}

int tc_export_stop(void)
{
}

int tc_export_close(void)
{
}


void tc_export_loop(vob_t *vob, int frame_first, int frame_last)
{
    return tc_encoder_loop(vob, frame_first, frame_last);
}

/* ------------------------------------------------------------
 *
 * encoder close
 *
 * ------------------------------------------------------------*/

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

#define RETURN_IF_NOT_OK(RET, KIND) do { \
    if ((RET) != TC_OK) { \
        tc_log_error(__FILE__, "error encoding final %s frame", (KIND)); \
        return TC_ERROR; \
    } \
} while (0)

#define RETURN_IF_MUX_ERROR(BYTES) do { \
    if ((BYTES) < 0) { \
        tc_log_error(__FILE__, "error multiplexing final audio frame"); \
        return TC_ERROR; \
    } \
} while (0)
        

/* DO NOT rotate here, this data belongs to current chunk */
static int encoder_flush(TCEncoderData *data)
{
    int ret = TC_ERROR, bytes = 0;

    do {
        RESET_ATTRIBUTES(data->aenc_ptr);
        ret = tc_module_encode_audio(data->aud_mod, NULL, data->aenc_ptr);
        RETURN_IF_NOT_OK(ret, "audio");

        bytes = tc_module_multiplex(data->mplex_mod, NULL, data->aenc_ptr);
    } while (bytes != 0);
    RETURN_IF_MUX_ERROR(bytes);

    do {
        RESET_ATTRIBUTES(data->venc_ptr);
        ret = tc_module_encode_video(data->vid_mod, NULL, data->venc_ptr);
        RETURN_IF_NOT_OK(ret, "video");

        bytes = tc_module_multiplex(data->mplex_mod, data->venc_ptr, NULL);
    } while (bytes != 0);
    RETURN_IF_MUX_ERROR(bytes);

    return TC_OK;
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
