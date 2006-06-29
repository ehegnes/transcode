/*
 *  encode_null.c - produce empty )as in zero-sized) A/V frames.
 *  (C) 2005/2006 Francesco Romani <fromani at gmail dot com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "transcode.h"
#include "framebuffer.h"
#include "libtc/optstr.h"

#include "libtc/tcmodule-plugin.h"

#define MOD_NAME    "encode_null.so"
#define MOD_VERSION "v0.0.3 (2005-06-05)"
#define MOD_CAP     "null (fake) A/V encoder"

static const char *null_help = ""
    "Overview:\n"
    "    this module absorb provided A/V frames and produces fake,"
    "    empty \"encoded\" frames.\n"
    "Options:\n"
    "    help    produce module overview and options explanations\n";

static int null_init(TCModuleInstance *self)
{
    if (self == NULL) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }
    self->userdata = NULL;

    return TC_EXPORT_OK;
}

static int null_fini(TCModuleInstance *self)
{
    if (self == NULL) {
        tc_log_error(MOD_NAME, "fini: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    return TC_EXPORT_OK;
}

static int null_configure(TCModuleInstance *self,
                          const char *options, vob_t *vob)
{
    if (self == NULL) {
        tc_log_error(MOD_NAME, "configure: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    return TC_EXPORT_OK;
}

static const char *null_inspect(TCModuleInstance *self,
                                const char *param)
{
    if (self == NULL) {
        tc_log_error(MOD_NAME, "inspect: bad instance data reference");
        return NULL;
    }

    if (optstr_lookup(param, "help")) {
        return null_help;
    }

    return "";
}

static int null_stop(TCModuleInstance *self)
{
    if (self == NULL) {
        tc_log_error(MOD_NAME, "stop: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    return TC_EXPORT_OK;
}

static int null_encode_video(TCModuleInstance *self,
                              vframe_list_t *inframe, vframe_list_t *outframe)
{
    if (self == NULL) {
        tc_log_error(MOD_NAME, "encode_video: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    outframe->video_len = 0;
    return TC_EXPORT_OK;
}

static int null_encode_audio(TCModuleInstance *self,
                              aframe_list_t *inframe, aframe_list_t *outframe)
{
    if (self == NULL) {
        tc_log_error(MOD_NAME, "encode_audio: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    outframe->audio_len = 0;
    return TC_EXPORT_OK;
}


/*************************************************************************/

static const int null_codecs_in[] = { TC_CODEC_ANY, TC_CODEC_ERROR };

static const int null_codecs_out[] = { TC_CODEC_ANY, TC_CODEC_ERROR };

static const TCModuleInfo null_info = {
    .features    = TC_MODULE_FEATURE_ENCODE|TC_MODULE_FEATURE_VIDEO
                   |TC_MODULE_FEATURE_AUDIO,
    .flags       = TC_MODULE_FLAG_RECONFIGURABLE,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = null_codecs_in,
    .codecs_out  = null_codecs_out
};

static const TCModuleClass null_class = {
    .info         = &null_info,

    .init         = null_init,
    .fini         = null_fini,
    .configure    = null_configure,
    .stop         = null_stop,
    .inspect      = null_inspect,

    .encode_video = null_encode_video,
    .encode_audio = null_encode_audio,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &null_class;
}

