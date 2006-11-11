/*
 *  encode_copy.c - passthrough A/V frames through deep copy.
 *  (C) 2005-2006 Francesco Romani <fromani at gmail dot com>
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

#define MOD_NAME    "encode_copy.so"
#define MOD_VERSION "v0.0.2 (2005-12-29)"
#define MOD_CAP     "copy (passthrough) A/V frames"

static const char *copy_help = ""
    "Overview:\n"
    "    this module passthrough A/V frames copying them from input\n"
    "    to output.\n"
    "    For a faster passthrough consider usage of 'null' module.\n"
    "Options:\n"
    "    help    produce module overview and options explanations\n";

static int copy_init(TCModuleInstance *self)
{
    if (self == NULL) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_ERROR;
    }

    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }
    self->userdata = NULL;

    return TC_OK;
}

static int copy_fini(TCModuleInstance *self)
{
    TC_MODULE_SELF_CHECK(self, "fini");

    return TC_OK;
}

static int copy_inspect(TCModuleInstance *self,
                        const char *param, const char **value)
{
    TC_MODULE_SELF_CHECK(self, "inspect");

    if (optstr_lookup(param, "help")) {
        *value = copy_help;
    }

    return TC_OK;
}

static int copy_configure(TCModuleInstance *self,
                          const char *options, vob_t *vob)
{
    TC_MODULE_SELF_CHECK(self, "configure");

    return TC_OK;
}

static int copy_stop(TCModuleInstance *self)
{
    TC_MODULE_SELF_CHECK(self, "stop");

    return TC_OK;
}

static int copy_encode_video(TCModuleInstance *self,
                              vframe_list_t *inframe, vframe_list_t *outframe)
{
    TC_MODULE_SELF_CHECK(self, "encode_video");

    vframe_copy(outframe, inframe, 1);
    /* enforce full length (we deal with uncompressed frames */
    outframe->video_len = outframe->video_size;

    return TC_OK;
}

static int copy_encode_audio(TCModuleInstance *self,
                              aframe_list_t *inframe, aframe_list_t *outframe)
{
    TC_MODULE_SELF_CHECK(self, "encode_audio");

    if (inframe != NULL) {
        aframe_copy(outframe, inframe, 1);
        /* enforce full length (we deal with uncompressed frames */
        outframe->audio_len = outframe->audio_size;
    }

    return TC_OK;
}


/*************************************************************************/

static const uint32_t copy_codecs_in[] = { TC_CODEC_ANY, TC_CODEC_ERROR };
static const uint32_t copy_codecs_out[] = { TC_CODEC_ANY, TC_CODEC_ERROR };
static const TCFormatID copy_formats[] = { TC_FORMAT_ERROR };

static const TCModuleInfo copy_info = {
    .features    = TC_MODULE_FEATURE_ENCODE|TC_MODULE_FEATURE_VIDEO
                   |TC_MODULE_FEATURE_AUDIO,
    .flags       = TC_MODULE_FLAG_RECONFIGURABLE,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = copy_codecs_in,
    .codecs_out  = copy_codecs_out,
    .formats_in  = copy_formats,
    .formats_out = copy_formats
};

static const TCModuleClass copy_class = {
    .info         = &copy_info,

    .init         = copy_init,
    .fini         = copy_fini,
    .configure    = copy_configure,
    .stop         = copy_stop,
    .inspect      = copy_inspect,

    .encode_video = copy_encode_video,
    .encode_audio = copy_encode_audio,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &copy_class;
}

