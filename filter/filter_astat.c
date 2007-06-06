/*
 *  filter_astat.c -- audio statistics plugin, revisited
 *
 *  Copyright (C) Thomas Oestreich - June 2001
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

#define MOD_NAME    "filter_astat.so"
#define MOD_VERSION "v0.2.0 (2007-06-06)"
#define MOD_CAP     "audio statistics filter plugin"
#define MOD_AUTHOR  "Thomas Oestreich"

#define MOD_FEATURES \
    TC_MODULE_FEATURE_FILTER|TC_MODULE_FEATURE_AUDIO
#define MOD_FLAGS \
    TC_MODULE_FLAG_RECONFIGURABLE


#include "transcode.h"
#include "filter.h"
#include "libtc/libtc.h"
#include "libtc/optstr.h"
#include "libtc/tcmodule-plugin.h"

// TODO: silence threshold(s)?

/*************************************************************************/

static const char help_string[] = ""
    "Overview:\n"
    "    This filter scan audio track and compute optimal rescale value.\n"
    "    It can also detect if the audio track is silence only.\n"
    "Options:\n"
    "    help    produce module overview and options explanations\n"
    "    file    save audio track statistics to given file instead to print them\n";


typedef struct {
    int32_t min;
    int32_t max;

    char *filepath;

    char optstr_buf[PATH_MAX+1];
} PrivateData;

/*************************************************************************/

static void set_range(PrivateData *pd, int v)
{
    if (v > pd->max) {
        pd->max = v;
    } else if (v < pd->min) {
        pd->min = v;
    }
}

/*************************************************************************/

/*************************************************************************/

/* Module interface routines and data. */

/*************************************************************************/

/**
 * astat_init:  Initialize this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int astat_init(TCModuleInstance *self, uint32_t features)
{
    PrivateData *pd = NULL;

    TC_MODULE_SELF_CHECK(self, "init");
    TC_MODULE_INIT_CHECK(self, MOD_FEATURES, features);

    pd = tc_malloc(sizeof(PrivateData));
    if (!pd) {
        tc_log_error(MOD_NAME, "init: out of memory!");
        return TC_ERROR;
    }
    self->userdata = pd;

    /* enforce defaults */
    pd->min           = 0;
    pd->max           = 0;
    pd->filepath      = NULL;

    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }
    return TC_OK;
}

/*************************************************************************/

/**
 * astat_fini:  Clean up after this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int astat_fini(TCModuleInstance *self)
{
    PrivateData *pd;

    TC_MODULE_SELF_CHECK(self, "fini");

    pd = self->userdata;

    /* nothing to do in here */

    tc_free(self->userdata);
    self->userdata = NULL;
    return TC_OK;
}

/*************************************************************************/

/**
 * astat_configure:  Configure this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int astat_configure(TCModuleInstance *self,
                           const char *options, vob_t *vob)
{
    PrivateData *pd = NULL;

    TC_MODULE_SELF_CHECK(self, "configure");

    pd = self->userdata;

    /* re-enforce defaults */
    pd->min           = 0;
    pd->max           = 0;
    pd->filepath      = NULL;

    if (options) {
        char buf[1024];
        int ret = optstr_get(options, "file", "%[^:]", buf); // XXX
        if (ret > 0) {
            pd->filepath = tc_strdup(buf);
            if (pd->filepath == NULL) {
                return TC_ERROR;
            }

            if (verbose) {
                tc_log_info(MOD_NAME, "saving audio scale value to '%s'", pd->filepath);
            }
        }
    }

    return TC_OK;
}

/*************************************************************************/

/**
 * astat_stop:  Reset this instance of the module.  See tcmodule-data.h
 * for function details.
 */

static int astat_stop(TCModuleInstance *self)
{
    int ret = TC_OK; /* let's be optimistic... */
    PrivateData *pd = NULL;

    TC_MODULE_SELF_CHECK(self, "stop");

    pd = self->userdata;

    /* stats summary */
    if (pd->min == 0 && pd->max == 0) {
        tc_log_info(MOD_NAME, "audio track seems only silence");
    } else if (pd->min == 0 || pd->max == 0) {
        tc_log_warn(MOD_NAME, "bad minimum/maximum value,"
                              " unable to find scale value");
        ret = TC_ERROR;
    } else {
        double fmin = -((double) pd->min)/0x7FFF;
        double fmax =  ((double) pd->max)/0x7FFF;
        /* FIXME: constantize in libtcaudio */
        double vol = (fmin < fmax) ? 1./fmax : 1./fmin;

        if (pd->filepath == NULL) {
            tc_log_info(MOD_NAME, "(min=%.3f/max=%.3f), "
                                  "normalize volume with \"-s %.3f\"",
                                  -fmin, fmax, vol);
        } else {
            FILE *fh = fopen(pd->filepath, "w");
            if (fh == NULL) {
                tc_log_perror(MOD_NAME, "unable to open scale value file");
                ret = TC_ERROR;
            } else {
                fprintf(fh, "%.3f\n", vol);
                fclose(fh); // XXX
                if (verbose) {
                    tc_log_info(MOD_NAME, "wrote audio scale value to '%s'", pd->filepath);
                }
            }

            tc_free(pd->filepath);
            pd->filepath = NULL;
        }
    }

    return TC_OK;
}

/*************************************************************************/

/**
 * astat_inspect:  Return the value of an option in this instance of
 * the module.  See tcmodule-data.h for function details.
 */

static int astat_inspect(TCModuleInstance *self,
                             const char *param, const char **value)
{
    PrivateData *pd = NULL;

    TC_MODULE_SELF_CHECK(self, "inspect");
    TC_MODULE_SELF_CHECK(param, "inspect");
    TC_MODULE_SELF_CHECK(value, "inspect");

    pd = self->userdata;

    if (optstr_lookup(param, "help")) {
        *value = help_string; 
    }
    if (optstr_lookup(param, "file")) {
        if (pd->filepath == NULL) {
            *value = "None";
        } else {
            tc_snprintf(pd->optstr_buf, sizeof(pd->optstr_buf),
                        "%s", pd->filepath);
            *value = pd->optstr_buf;
        }
    }
    /* put back configurable options */

    return TC_OK;
}

/*************************************************************************/

/**
 * astat_filter_audio:  update the audio statistics of the stream with
 * this audio frame data. See tcmodule-data.h for function details.
 */

static int astat_filter_audio(TCModuleInstance *self, aframe_list_t *frame)
{
    PrivateData *pd = NULL;
    int16_t *s = NULL;
    int n;

    TC_MODULE_SELF_CHECK(self, "filter_audio");
    TC_MODULE_SELF_CHECK(frame, "filter_audio");

    pd = self->userdata;

    s = (int16_t*)frame->audio_buf;

    for (n = 0; n < frame->audio_size / 2; n++) {
        set_range(pd, (int) (*s));
        s++;
    }

    return TC_OK;
}

/*************************************************************************/

static const TCCodecID astat_codecs_in[] = { 
    TC_CODEC_PCM, TC_CODEC_ERROR 
};
static const TCCodecID astat_codecs_out[] = { 
    TC_CODEC_PCM, TC_CODEC_ERROR 
};
static const TCFormatID astat_formats[] = { 
    TC_FORMAT_ERROR
};

static const TCModuleInfo astat_info = {
    .features    = MOD_FEATURES,
    .flags       = MOD_FLAGS,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = astat_codecs_in,
    .codecs_out  = astat_codecs_out,
    .formats_in  = astat_formats,
    .formats_out = astat_formats
};

static const TCModuleClass astat_class = {
    .info         = &astat_info,

    .init         = astat_init,
    .fini         = astat_fini,
    .configure    = astat_configure,
    .stop         = astat_stop,
    .inspect      = astat_inspect,

    .filter_audio = astat_filter_audio,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &astat_class;
}

/*************************************************************************/

static int astat_get_config(TCModuleInstance *self, char *options)
{
    PrivateData *pd = NULL;

    TC_MODULE_SELF_CHECK(self, "get_config");

    pd = self->userdata;

    optstr_filter_desc(options, MOD_NAME, MOD_CAP, MOD_VERSION,
                       MOD_AUTHOR, "AE", "1");
    optstr_param(options, "file", "save rescale value to file", "%s", "");

    return TC_OK;
}

static int astat_process(TCModuleInstance *self, frame_list_t *frame)
{
    TC_MODULE_SELF_CHECK(self, "process");

    if (frame->tag & TC_PRE_S_PROCESS && frame->tag & TC_AUDIO
     && !(frame->attributes & TC_FRAME_IS_SKIPPED)) {
        return astat_filter_audio(self, (aframe_list_t*)frame);
    }
    return TC_OK;
}

/*************************************************************************/

/* Old-fashioned module interface. */

TC_FILTER_OLDINTERFACE(astat)

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

