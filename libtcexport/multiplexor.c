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

#include "multiplexor.h"

#include <stdint.h>

/*************************************************************************/
/* private function prototypes                                           */
/*************************************************************************/

/* new-style rotation support */
static void tc_rotate_init(TCRotateContext *rotor, const char *base_name);

static void tc_rotate_set_frames_limit(TCRotateContext *rotor,
                                       uint32_t frames);
static void tc_rotate_set_bytes_limit(TCRotateContext *rotor,
                                      uint64_t bytes);

static const char *tc_rotate_output_name(TCRotateContext *rotor);

static int tc_rotate_needed_never(TCRotateContext *rotor,
                                  uint32_t frames,
                                  uint32_t bytes);
static int tc_rotate_needed_by_frames(TCRotateContext *rotor,
                                      uint32_t frames,
                                      uint32_t bytes);
static int tc_rotate_needed_by_bytes(TCRotateContext *rotor,
                                     uint32_t frames,
                                     uint32_t bytes);

static int tc_rotate_needed(TCRotateContext *rotor,
                            uint32_t frames, uint32_t bytes);

/*************************************************************************/

typedef int (*TCExportRotate)(TCRotateContext *rotor, uint32_t bytes);


struct tcrotatecontext_ {
    char            path_buf[PATH_MAX+1];
    const char      *base_name;
    uint32_t        chunk_num;
    int             null_flag;

    uint32_t        chunk_frames;
    uint32_t        encoded_frames;

    uint64_t        encoded_bytes;
    uint64_t        chunk_bytes;

    TCExportRotate  rotate_needed;
};

/*************************************************************************/

static int tc_rotate_needed(TCRotateContext *rotor, uint32_t bytes)
{
    return rotor->rotate_needed(rotor, bytes);
}

static void tc_rotate_init(TCRotateContext *rotor, const char *base_name)
{
    if (rotor != NULL) {
        memset(rotor, 0, sizeof(TCRotateContext));

        rotor->base_name = base_name;
        if (base_name == NULL || strlen(base_name) == 0
         || strcmp(base_name, "/dev/null") == 0) {
            rotor->null_flag = TC_TRUE;
        } else {
            rotor->null_flag = TC_FALSE;
            strlcpy(rotor->path_buf, base_name, sizeof(rotor->path_buf));
        }
        rotor->rotate_needed = tc_rotate_needed_never;
    }
}

static void tc_rotate_set_frames_limit(TCRotateContext *rotor,
                                       uint32_t frames)
{
    if (rotor != NULL && !rotor->null_flag) {
        rotor->chunk_frames  = frames;
        rotor->rotate_needed = tc_rotate_needed_by_frames;
    }
}

static void tc_rotate_set_bytes_limit(TCRotateContext *rotor,
                                      uint64_t bytes)
{
    if (rotor != NULL && !rotor->null_flag) {
        rotor->chunk_bytes   = bytes;
        rotor->rotate_needed = tc_rotate_needed_by_bytes;
    }
}

static const char *tc_rotate_output_name(TCRotateContext *rotor)
{
    tc_snprintf(rotor->path_buf, sizeof(rotor->path_buf),
                "%s-%03i", rotor->base_name, rotor->chunk_num);
    rotor->chunk_num++;
    return rotor->path_buf;
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

#define ROTATE_UPDATE_COUNTERS(bytes, frames) do { \
    rotor->encoded_bytes  += (bytes); \
    rotor->encoded_frames += (frames); \
} while (0);

static int tc_rotate_needed_never(TCRotateContext *rotor,
                                  uint32_t frames,
                                  uint32_t bytes)
{
    ROTATE_UPDATE_COUNTERS(bytes, frames);
    return TC_FALSE;
}

static int tc_rotate_needed_by_frames(TCRotateContext *rotor,
                                      uint32_t frames,
                                      uint32_t bytes)
{
    int ret = TC_FALSE;
    ROTATE_UPDATE_COUNTERS(bytes, frames);

    if (rotor->encoded_frames >= rotor->chunk_frames) {
        ret = TC_TRUE;
    }
    return ret;
}

static int tc_rotate_needed_by_bytes(TCRotateContext *rotor,
                                     uint32_t frames,
                                     uint32_t bytes)
{
    int ret = TC_FALSE;
    ROTATE_UPDATE_COUNTERS(bytes, frames);

    if (rotor->encoded_bytes >= rotor->chunk_bytes) {
        ret = TC_TRUE;
    }
    return ret;
}

#undef ROTATE_UPDATE_COUNTERS

/*************************************************************************/
/* real multiplexor code                                                 */
/*************************************************************************/

void tc_multiplexor_limit_frames(TCMultiplexor *mux, uint32_t frames);
{
    tc_rotate_set_frames_limit(mux->rotor, frames);
}

void tc_multiplexor_limit_megabytes(TCMultiplexor *mux, uint32_t megabytes)
{
    tc_rotate_set_bytes_limit(mux->rotor, megabytes * 1024 * 1024);
}

#define ROTATE_COMMON_CODE(rotor, job) do { \
    ret = tc_multiplexor_close(); \
    if (ret != TC_OK) { \
        tc_log_error(__FILE__, "unable to close output stream"); \
        ret = TC_ERROR; \
    } else { \
        tc_rotate_output_name((rotor), (job)); \
        tc_log_info(__FILE__, "rotating video output stream to %s", \
                               (rotor)->video_path_buf); \
        tc_log_info(__FILE__, "rotating audio output stream to %s", \
                               (rotor)->audio_path_buf); \
        ret = tc_multiplexor_open((job)); \
        if (ret != TC_OK) { \
            tc_log_error(__FILE__, "unable to reopen output stream"); \
            ret = TC_ERROR; \
        } \
    } \
} while (0)



/*************************************************************************/

static int mono_open(TCMultiplexor *mux)
{
    return TC_ERROR;
}

static int mono_close(TCMultiplexor *mux)
{
    return TC_ERROR;
}

static int mono_write(TCMultiplexor *mux,
                      TCFrameVideo *vframe, TCFrameAudio *aframe)
{
    return TC_ERROR;
}


static int dual_open(TCMultiplexor *mux)
{
    return TC_ERROR;
}

static int dual_close(TCMultiplexor *mux)
{
    return TC_ERROR;
}

static int dual_write(TCMultiplexor *mux,
                      TCFrameVideo *vframe, TCFrameAudio *aframe)
{
    return TC_ERROR;
}


/*************************************************************************/

int tc_multiplexor_init(TCMultiplexor *mux, TCJob *job, TCFactory factory)
{
    int ret = TC_ERROR;

    mux->job        = job;
    mux->factory    = factory;
    mux->mux_main   = NULL;
    mux->mux_aux    = NULL;

    mux->rotor      = NULL;
    mux->rotor_aux  = NULL;

    mux->vid_xdata  = NULL;
    mux->aud_xdata  = NULL;

    mux->has_aux    = TC_FALSE;

    ret = TC_OK;
    return ret;
}

int tc_multiplexor_fini(TCMultiplexor *mux)
{
    return TC_OK;
}


/*************************************************************************/

static TCModule muxer_setup(TCMultiplexor *mux,
                            const char *mux_mod_name, int mtype,
                            const char *tag)
{
    TCModuleExtraData *xdata[] = { NULL };
    const char *options = NULL;

    TCModule mux_mod = NULL;

    mux_mod = tc_new_module(mux->factory, "multiplex", mux_mod_name, mtype);
    if (!mux_mod) {
        tc_log_error(__FILE__, "can't load %s module ");
        return NULL;
    }

    options = (mux->job->ex_m_string) ?mux->job->ex_m_string :"";
    ret = tc_module_configure(mux_mod, options, mux->job, xdata);
    if (ret != TC_OK) {
        tc_log_error(__FILE__, "%s module error: init failed");
        return NULL;
    }
    return mux_mod;
}

int tc_multiplexor_setup(TCMultiplexor *mux,
                         const char *mux_mod_name,
                         const char *mux_mod_name_aux)
{
    int mtype = (mux_mod_name_aux) ?TC_VIDEO :(TC_VIDEO|TC_AUDIO);
    int ret = TC_ERROR;

    tc_debug(TC_DEBUG_MODULES, "loading multiplexor modules");

    mux->mux_main = muxer_setup(mux, mux_mod_name, mtype, "multiplexor");
    if (mux->mux_main) {
        if (!mux_mod_name_aux) {
            mux->has_aux = TC_FALSE;
            mux->mux_aux = mux->mux_main;
            ret          = TC_OK;
        } else {
            mux->has_aux = TC_TRUE;
            mux->mux_aux = muxer_setup(mux, mux_mod_name_aux,
                                       TC_AUDIO, "aux multiplexor");
            if (mux->mux_aux) {
                ret = TC_OK;
            }
        }
    }
    return ret;
}

void tc_export_shutdown(TCEncoder *enc)
{
    tc_debug(TC_DEBUG_MODULES, "unloading multiplexor modules");

    tc_del_module(mux->factory, mux->mux_main);
    tc_free(mux->rotor);
    if (mux->has_aux) {
        tc_del_module(mux->factory, mux->mux_aux);
    }
}

/*************************************************************************/

static int muxer_open(TCMultiplexor *mux)
{
    TCModuleExtraData *xdata[] = { mux->vid_xdata, mux->aud_xdata, NULL };
    int ret;

    if (!mux->has_aux) {
        xdata[1] = NULL;
    }

    ret = tc_module_open(mux->mux_main,
                         tc_rotate_output_name(mux->rotor),
                         xdata);
    if (ret != TC_OK) {
        tc_log_error(__FILE__, "multiplexor module error: open failed");
        return TC_ERROR;
    }

    if (mux->has_aux) {
        xdata[0] = mux->aud_xdata;

        ret = tc_module_open(mux->mux_aux,
                             tc_rotate_output_name(mux->rotor_aux),
                             xdata);
        if (ret != TC_OK) {
            tc_log_error(__FILE__, "aux multiplexor module error: open failed");
            return TC_ERROR;
        }
    }

    return TC_OK;
}

int tc_multiplexor_open(TCMultiplexor *mux,
                        const char *sink_name,
                        const char *sink_name_aux,
                        TCModuleExtraData *vid_xdata,
                        TCModuleExtraData *aud_xdata)
{
    tc_debug(TC_DEBUG_MODULES, "multiplexor opened");

    mux->vid_xdata = vid_xdata;
    mux->aux_xdata = aud_xdata;

    /* sanity checks */
    if (mux->has_aux && !sink_name_aux) {
        tc_log_error(__FILE__, "foobar");
        return TC_ERROR;
    }

    /* pre allocation */
    mux->rotor = tc_zalloc(sizeof(TCRotateContext));
    if (!mux->rotor_aux) {
        goto no_rotor;
    }
    tc_rotate_init(mux->rotor, sink_name);

    if (!mux->has_aux) {
        mux->rotor_aux = mux->rotor;
    } else {
        mux->rotor_aux = tc_zalloc(sizeof(TCRotateContext));
        if (!mux->rotor_aux) {
            goto no_rotor_aux;
        }
        tc_rotate_init(mux->rotor_aux, sink_name_aux);
    }

    ret = muxer_open(mux);
    if (ret != TC_OK) {
        goto no_rotor_aux;
    }
    return ret;

no_rotor_aux:
    tc_free(mux->rotor);
no_rotor:
    return TC_ERROR;
}


static muxer_close(TCMultiplexor *mux)
{
    int ret;

    ret = tc_module_close(mux->mux_main);
    if (ret != TC_OK) {
        tc_log_warn(__FILE__, "multiplexor module error: close failed");
        return TC_ERROR;
    }

    if (mux->has_aux) {
        ret = tc_module_stop(mux->mux_aux);
        if (ret != TC_OK) {
            tc_log_warn(__FILE__, "aux multiplexor module error: close failed");
            return TC_ERROR;
        }
    }

    tc_debug(TC_DEBUG_CLEANUP, "multiplexor closed");
    return TC_OK;
}

#define MUX_FREE(FIELD) do { \
    if (mux->FIELD) { \
        tc_free(mux->FIELD); \
        mux->FIELD = NULL; \
    } \
} while (0)

int tc_multiplexor_close(TCMultiplexor *mux)
{
    int ret = muxer_close(mux);

    if (ret == TC_OK) {
        MUX_FREE(rotor);
        if (mux->has_aux) {
            MUX_FREE(rotor_aux);
        }
    }
    return ret;
 }

/*************************************************************************/
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

