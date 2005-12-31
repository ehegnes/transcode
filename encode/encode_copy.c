/*
 *  encode_copy.c - passthrough A/V frames through deep copy.
 *  (C) Francesco Romani <fromani at gmail dot com> - December 2005
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
#define MOD_CAP     "copy"

static const char *copy_help = ""
    "Overview:\n"
    "\tthis module passthrough A/V frames copying them from input\n"
    "\tto output.\n"
    "\tFor a faster passthrough consider to use 'null' module.\n"
    "Options:\n"
    "\tHelp\tproduce module overview and options explanations\n";

static int copy_init(TCModuleInstance *self)
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }
    self->userdata = NULL;

    return 0;
}

static int copy_fini(TCModuleInstance *self)
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    return 0;
}

static const char *copy_inspect(TCModuleInstance *self,
                                const char *param)
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return NULL;
    }

    if (optstr_lookup(param, "help")) {
        return copy_help;
    }

    return "";
}

static int copy_configure(TCModuleInstance *self,
                          const char *options, vob_t *vob)
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    return TC_EXPORT_OK;
}

static int copy_stop(TCModuleInstance *self)
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    return 0;
}

static int copy_encode_video(TCModuleInstance *self,
                              vframe_list_t *inframe, vframe_list_t *outframe)
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    vframe_copy(outframe, inframe, 1);

    return 0;
}

static int copy_encode_audio(TCModuleInstance *self,
                              aframe_list_t *inframe, aframe_list_t *outframe)
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    /*
     * XXX
     * implement hardcopy
     * (this needs some changes at framebuffer handling code).
     */
    aframe_copy(outframe, inframe, 1);

    return 0;
}


/*************************************************************************/

static const int copy_codecs_in[] = { TC_CODEC_ANY, TC_CODEC_ERROR };

static const int copy_codecs_out[] = { TC_CODEC_ANY, TC_CODEC_ERROR };

static const TCModuleInfo copy_info = {
    .features    = TC_MODULE_FEATURE_ENCODE|TC_MODULE_FEATURE_VIDEO
                   |TC_MODULE_FEATURE_AUDIO,
    .flags       = TC_MODULE_FLAG_RECONFIGURABLE,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = copy_codecs_in,
    .codecs_out  = copy_codecs_out
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

