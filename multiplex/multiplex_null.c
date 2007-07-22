/*
 *  multiplex_null.c - fake multiplexor that discards any given frame.
 *  (C) 2005-2007 Francesco Romani <fromani at gmail dot com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License version 2.  See the file COPYING for details.
 */

#include "transcode.h"
#include "libtc/optstr.h"

#include "libtc/tcmodule-plugin.h"

#define MOD_NAME    "multiplex_null.so"
#define MOD_VERSION "v0.0.2 (2005-12-29)"
#define MOD_CAP     "discard each encoded frame"

#define MOD_FEATURES \
    TC_MODULE_FEATURE_MULTIPLEX|TC_MODULE_FEATURE_VIDEO|TC_MODULE_FEATURE_AUDIO
    

#define MOD_FLAGS \
    TC_MODULE_FLAG_RECONFIGURABLE


static const char null_help[] = ""
    "Overview:\n"
    "    this module simply discard given encoded write audio and video frames.\n"
    "    Is used for test, benchmark and debug purposes.\n"
    "Options:\n"
    "    help    produce module overview and options explanations\n";

static int null_init(TCModuleInstance *self, uint32_t features)
{
    TC_MODULE_SELF_CHECK(self, "init");
    TC_MODULE_INIT_CHECK(self, MOD_FEATURES, features);

    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }
    self->userdata = NULL;

    return TC_EXPORT_OK;
}

static int null_fini(TCModuleInstance *self)
{
    TC_MODULE_SELF_CHECK(self, "fini");

    return TC_EXPORT_OK;
}

static int null_configure(TCModuleInstance *self,
                          const char *options, vob_t *vob)
{
    TC_MODULE_SELF_CHECK(self, "configure");
    
    return TC_EXPORT_OK;
}

static int null_inspect(TCModuleInstance *self,
                        const char *param, const char **value)
{
    TC_MODULE_SELF_CHECK(self, "inspect");

    if (optstr_lookup(param, "help")) {
        *value = null_help;
    }

    return TC_EXPORT_OK;
}

static int null_stop(TCModuleInstance *self)
{
    TC_MODULE_SELF_CHECK(self, "stop");

    return TC_EXPORT_OK;
}

static int null_multiplex(TCModuleInstance *self,
                         vframe_list_t *vframe, aframe_list_t *aframe)
{
    int asize = 0, vsize = 0;

    TC_MODULE_SELF_CHECK(self, "multiplex");

    if (vframe != NULL) {
        vsize = vframe->video_len;
    }

    if (aframe != NULL) {
        asize = aframe->audio_len;
    }

    return vsize + asize;
}

/*************************************************************************/

static const TCCodecID null_codecs_in[] = { TC_CODEC_ANY, TC_CODEC_ERROR };

/* a multiplexor is at the end of pipeline */
static const TCCodecID null_codecs_out[] = { TC_CODEC_ERROR };
static const TCFormatID null_formats_in[] = { TC_FORMAT_ERROR };
static const TCFormatID null_formats_out[] = { TC_FORMAT_NULL, TC_FORMAT_ERROR };

static const TCModuleInfo null_info = {
    .features    = MOD_FEATURES,
    .flags       = MOD_FLAGS,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = null_codecs_in,
    .codecs_out  = null_codecs_out,
    .formats_in  = null_formats_in,
    .formats_out = null_formats_out
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

