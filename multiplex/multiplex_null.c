/*
 *  multiplex_raw.c - write a separate plain file for each stream
 *  (C) Francesco Romani <fromani at gmail dot com> - December 2005
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "transcode.h"
#include "libtc/optstr.h"

#include "libtc/tcmodule-plugin.h"

#define MOD_NAME    "multiplex_null.so"
#define MOD_VERSION "v0.0.2 (2005-12-29)"
#define MOD_CAP     "discard each encoded frame"

static const char *null_help = ""
    "Overview:\n"
    "\tthis module simply discard given encoded write audio and video frames.\n"
    "\tIs used for test, benchmark and debug purposes.\n"
    "Options:\n"
    "\thelp\tproduce module overview and options explanations\n";

static int null_init(TCModuleInstance *self)
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

static int null_fini(TCModuleInstance *self)
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    return 0;
}

static int null_configure(TCModuleInstance *self,
                          const char *options, vob_t *vob)
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }
    
    return TC_EXPORT_OK;
}

static const char *null_inspect(TCModuleInstance *self,
                                const char *param)
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return NULL;
    }

    if (optstr_lookup(param, "help")) {
        return null_help;
    }

    return "";
}

static int null_stop(TCModuleInstance *self)
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    return 0;
}

static int null_multiplex(TCModuleInstance *self,
                         vframe_list_t *vframe, aframe_list_t *aframe)
{
    int asize = 0, vsize = 0;

    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    if (vframe != NULL) {
        vsize = vframe->video_size;
    }

    if (aframe != NULL) {
        asize = aframe->audio_size;
    }

    return vsize + asize;
}

/*************************************************************************/

static const int null_codecs_in[] = { TC_CODEC_ANY, TC_CODEC_ERROR };

/* a multiplexor is at the end of pipeline */
static const int null_codecs_out[] = { TC_CODEC_ERROR };

static const TCModuleInfo null_info = {
    .features    = TC_MODULE_FEATURE_MULTIPLEX|TC_MODULE_FEATURE_VIDEO
                   |TC_MODULE_FEATURE_AUDIO,
    .flags       = TC_MODULE_FLAG_RECONFIGURABLE
                   |TC_MODULE_FLAG_REQUIRE_CONFIG,
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

    .multiplex    = null_multiplex,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &null_class;
}

