/*
 *  XXX - XXX
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

#define MOD_NAME    "encode_XXX.so"
#define MOD_VERSION "v0.0.1 (2005-12-26)"
#define MOD_CAP     "XXX"

static const char *null_help = ""
    "Overview:\n"
    "\tXXX.\n"
    "Options:\n"
    "\tHelp\tproduce module overview and options explanations\n";

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

static const char *null_configure(TCModuleInstance *self,
                                 const char *options)
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return NULL;
    }
    
    if (optstr_lookup(options, "help")) {
        return null_help;
    }

    return "help";
}

static int null_stop(TCModuleInstance *self) 
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }
    
    return 0;
}

static int null_encode_video(TCModuleInstance *self,
                              vframe_list_t *inframe, vframe_list_t *outframe)
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    return 0;
}

static int null_encode_audio(TCModuleInstance *self,
                              aframe_list_t *inframe, aframe_list_t *outframe)
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    return 0;
}


/*************************************************************************/

static const int null_codecs_in[] = { TC_CODEC_ANY, TC_CODEC_ERROR };

/* a encodeor is at the end of pipeline */
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
    
    .encode_video = null_encode_video,
    .encode_audio = null_encode_audio,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &null_class;
}

