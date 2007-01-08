/*
 *  filter_resample.c
 *
 *  Copyright (C) Thomas Oestreich - February 2002
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

#define MOD_NAME    "filter_resample.so"
#define MOD_VERSION "v0.1.5 (2007-01-07)"
#define MOD_CAP     "audio resampling filter plugin using libavcodec"
#define MOD_AUTHOR  "Thomas Oestreich, Stefan Scheffler"

#include "transcode.h"
#include "filter.h"
#include "libtc/libtc.h"
#include "libtc/optstr.h"
#include "libtc/tcmodule-plugin.h"

#include <ffmpeg/avcodec.h>

typedef struct
{
    uint8_t *resample_buf;
    size_t resample_bufsize;

    int bytes_per_sample;

    ReSampleContext *resample_ctx;
} ResamplePrivateData;

static const char *resample_help = ""
    "Overview:\n"
    "    This filter resample an audio stream using libavcodec facilties.\n"
    "    i.e. changes input sample rate to 22050 Hz to 48000 Hz.\n"
    "Options:\n"
    "    help    show this message.\n";


/*-------------------------------------------------*/

static int resample_init(TCModuleInstance *self)
{
    TC_MODULE_SELF_CHECK(self, "init");

    self->userdata = tc_malloc(sizeof(ResamplePrivateData));
    if (self->userdata == NULL) {
        tc_log_error(MOD_NAME, "init: out of memory!");
        return TC_ERROR;
    }
    
    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }
    return TC_OK;
}

static int resample_configure(TCModuleInstance *self,
                              const char *options, vob_t *vob)
{
    double samples_per_frame, ratio;
    ResamplePrivateData *pd = NULL;

    TC_MODULE_SELF_CHECK(self, "configure");
    TC_MODULE_SELF_CHECK(vob, "configure"); /* paranoia */

    pd = self->userdata;

    if (!vob->a_rate || !vob->mp3frequency) {
        tc_log_error(MOD_NAME, "Invalid settings");
        return TC_ERROR;
    }
    tc_log_info(MOD_NAME, "resampling: %i Hz -> %i Hz",
                vob->a_rate, vob->mp3frequency);
    if (vob->a_rate == vob->mp3frequency) {
        tc_log_error(MOD_NAME, "Frequencies are identical,"
                     " filter skipped");
        return TC_ERROR;
    }
 
    pd->bytes_per_sample = vob->a_chan * vob->a_bits/8;
    samples_per_frame = vob->a_rate/vob->ex_fps;
    ratio = (float)vob->mp3frequency/(float)vob->a_rate;

    pd->resample_bufsize = (int)(samples_per_frame * ratio) * pd->bytes_per_sample + 16 // frame + 16 bytes
                            + ((vob->a_leap_bytes > 0)?(int)(vob->a_leap_bytes * ratio) :0); 
                           // leap bytes .. kinda
    /* XXX */

    pd->resample_buf = tc_malloc(pd->resample_bufsize);
    if (pd->resample_buf == NULL) {
        tc_log_error(MOD_NAME, "Buffer allocation failed");
        return TC_ERROR;
    }

    if (verbose >= TC_DEBUG) {
        tc_log_info(MOD_NAME,
                    "bufsize : %lu, bytes : %i, bytesfreq/fps: %i, rest %i",
                    (unsigned long)pd->resample_bufsize, pd->bytes_per_sample,
                    vob->mp3frequency * pd->bytes_per_sample / (int)vob->fps,
                    (vob->a_leap_bytes > 0 )?(int)(vob->a_leap_bytes * ratio):0);
    }

    if ((size_t)(pd->bytes_per_sample * vob->mp3frequency / vob->fps) > pd->resample_bufsize) {
        goto abort;
    }

    pd->resample_ctx = audio_resample_init(vob->a_chan, vob->a_chan,
                                           vob->mp3frequency, vob->a_rate);
    if (pd->resample_ctx == NULL) {
        tc_log_error(MOD_NAME, "can't get a resample context");
        goto abort;
    }

    /* 
     * this will force this resample filter to do the job, not the export module.
     * Yeah, that's nasty. -- FR.
     */

    vob->a_rate = vob->mp3frequency;
    vob->mp3frequency = 0;
    vob->ex_a_size = pd->resample_bufsize;

    self->userdata = pd;

    return TC_OK;

abort:
    tc_free(pd->resample_buf);
    pd->resample_buf = NULL;
    return TC_ERROR;
}


static int resample_fini(TCModuleInstance *self)
{
    TC_MODULE_SELF_CHECK(self, "fini");

    tc_free(self->userdata);
    self->userdata = NULL;

    return TC_OK;
}

static int resample_stop(TCModuleInstance *self)
{
    ResamplePrivateData *pd = NULL;

    TC_MODULE_SELF_CHECK(self, "stop");

    pd = self->userdata;

    if (pd->resample_ctx != NULL) {
        audio_resample_close(pd->resample_ctx);
        pd->resample_ctx = NULL;
    }
    if (pd->resample_buf != NULL) {
        tc_free(pd->resample_buf);
        pd->resample_buf = NULL;
    }

    return TC_OK;
}

static int resample_inspect(TCModuleInstance *self,
                          const char *param, const char **value)
{
    TC_MODULE_SELF_CHECK(self, "inspect");
    TC_MODULE_SELF_CHECK(param, "inspect");
    
    if (optstr_lookup(param, "help")) {
        *value = resample_help;
    }

    return TC_OK;
}

/* internal helper to avoid an useless double if() */
static int resample_process(TCModuleInstance *self, aframe_list_t *frame)
{
    ResamplePrivateData *pd = self->userdata;

    if (pd->resample_bufsize == 0) {
        /* XXX: really useful? can happen? */
        tc_log_error(__FILE__, "wrong (insane) buffer size");
        return TC_ERROR;
    }
    if (verbose >= TC_STATS)
        tc_log_info(MOD_NAME, "inbuf: %i, bufsize: %lu",
                    frame->audio_size, (unsigned long)pd->resample_bufsize);
    frame->audio_size = audio_resample(pd->resample_ctx,
                                       (short *)pd->resample_buf,
                                       (short *)frame->audio_buf,
                                       frame->audio_size/pd->bytes_per_sample);
    frame->audio_size *= pd->bytes_per_sample;
    if (verbose >= TC_STATS)
        tc_log_info(MOD_NAME, "outbuf: %i", frame->audio_size);

    if (frame->audio_size < 0)
        frame->audio_size = 0;

    ac_memcpy(frame->audio_buf, pd->resample_buf, frame->audio_size);

    return TC_OK;
}

static int resample_filter(TCModuleInstance *self,
                           aframe_list_t *frame)
{
    TC_MODULE_SELF_CHECK(self, "filter");
    TC_MODULE_SELF_CHECK(frame, "filter");

    if (frame->tag & TC_PRE_S_PROCESS && frame->tag & TC_AUDIO) {
        return resample_process(self, frame);
    }
    return TC_OK;
}


/*-------------------------------------------------*/

static TCModuleInstance mod;

int tc_filter(frame_list_t *ptr_, char *options)
{
    aframe_list_t *ptr = (aframe_list_t *)ptr_;

    if (ptr->tag & TC_FILTER_GET_CONFIG) {
        optstr_filter_desc(options, MOD_NAME, MOD_CAP, MOD_VERSION,
                           "Thomas Oestreich", "AE", "1");
        return TC_OK;
    }

    if  (ptr->tag & TC_FILTER_INIT) {
        int ret = resample_init(&mod);
        if (ret != TC_OK) {
            return ret;
        }
        return resample_configure(&mod, options, tc_get_vob());
    }

    if (ptr->tag & TC_FILTER_CLOSE) {
        int ret = resample_stop(&mod);
        if (ret != TC_OK) {
            return ret;
        }
        return resample_fini(&mod);
    }

    /* filter frame routine */

    /*
     * tag variable indicates, if we are called before
     * transcodes internal video/audo frame processing routines
     * or after and determines video/audio context
     */

    if (ptr->tag & TC_PRE_S_PROCESS && ptr->tag & TC_AUDIO) {
        return resample_process(&mod, ptr);
    }
    return TC_OK;
}

/***********yy**************************************************************/

static const TCCodecID resample_codecs_in[] = { TC_CODEC_PCM, TC_CODEC_ERROR };
static const TCCodecID resample_codecs_out[] = { TC_CODEC_PCM, TC_CODEC_ERROR };
static const TCFormatID resample_formats[] = { TC_FORMAT_ERROR };

/* new module support */
static const TCModuleInfo resample_info = {
    .features    = TC_MODULE_FEATURE_FILTER|TC_MODULE_FEATURE_AUDIO,
    .flags       = TC_MODULE_FLAG_RECONFIGURABLE,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = resample_codecs_in,
    .codecs_out  = resample_codecs_out,
    .formats_in  = resample_formats,
    .formats_out = resample_formats
};

static const TCModuleClass resample_class = {
    .info         = &resample_info,

    .init         = resample_init,
    .fini         = resample_fini,
    .configure    = resample_configure,
    .stop         = resample_stop,
    .inspect      = resample_inspect,

    .filter_audio = resample_filter,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &resample_class;
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
