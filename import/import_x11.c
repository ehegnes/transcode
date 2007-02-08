/*
 *  demultiplex_x11.c - extract full-screen images from an X11 connection.
 *  (C) 2006 Francesco Romani <fromani at gmail dot com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "transcode.h"
#include "libtc/optstr.h"

#include "libtc/tcmodule-plugin.h"
#include "libtc/tctimer.h"

#include "x11source.h"

#define LEGACY 1

#ifdef LEGACY
# define MOD_NAME    "import_x11.so"
#else
# define MOD_NAME    "demultiplex_x11.so"
#endif

#define MOD_VERSION "v0.0.2 (2006-08-03)"
#define MOD_CAP     "fetch full-screen frames from an X11 connection"

#define MOD_FEATURES \
    TC_MODULE_FEATURE_DEMULTIPLEX|TC_MODULE_FEATURE_VIDEO
#define MOD_FLAGS \
    TC_MODULE_FLAG_RECONFIGURABLE

static const char tc_x11_help[] = ""
    "Overview:\n"
    "    This module acts as a bridge from transcode an a X11 server.\n"
    "    It grabs screenshots at fixed rate from X11 connection, allowing\n"
    "    to record screencast and so on.\n"
    "Options:\n"
    "    help    produce module overview and options explanations\n";

typedef struct tcx11privatedata_ TCX11PrivateData;
struct tcx11privatedata_ {
    TCX11Source src;
    TCTimer timer;

    uint32_t frame_delay;

    uint32_t expired;
};

static int tc_x11_init(TCModuleInstance *self, uint32_t features)
{
    TCX11PrivateData *priv = NULL;

    TC_MODULE_SELF_CHECK(self, "init");
    TC_MODULE_INIT_CHECK(self, MOD_FEATURES, features);

    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }
    priv = tc_malloc(sizeof(TCX11PrivateData));
    if (priv == NULL) {
        return TC_ERROR;
    }

    self->userdata = priv;    
    return TC_OK;
}

static int tc_x11_fini(TCModuleInstance *self)
{
    TC_MODULE_SELF_CHECK(self, "fini");

    tc_free(self->userdata);
    self->userdata = NULL;

    return TC_OK;
}

static int tc_x11_configure(TCModuleInstance *self,
                            const char *options, vob_t *vob)
{
    TCX11PrivateData *priv = NULL;
    int ret = 0;

    TC_MODULE_SELF_CHECK(self, "configure");

    priv = self->userdata;

    priv->expired = 0;
    priv->frame_delay = (uint32_t)(1000000.0 / vob->fps); /* microsecs */
    if (verbose >= TC_DEBUG) {
        tc_log_info(MOD_NAME, "frame delay will be %lu microseconds",
                              (unsigned long)priv->frame_delay);
    }

    ret = tc_timer_init_soft(&priv->timer, 0); /* XXX */
    if (ret != 0) {
        tc_log_error(MOD_NAME, "configure: can't initialize timer");
        return TC_ERROR;
    }

    /* nothing to do here, yet */
    ret = tc_x11source_is_display_name(vob->video_in_file);
    if (ret == TC_FALSE) {
        tc_log_error(MOD_NAME, "configure: given source doesn't look like"
                               " a DISPLAY specifier");
        return TC_ERROR;
    }

    ret = tc_x11source_open(&priv->src, vob->video_in_file,
                            TC_X11_MODE_BEST, vob->im_v_codec);
    if (ret != 0) {
        tc_log_error(MOD_NAME, "configure: failed to open X11 connection"
                               " to '%s'", vob->video_in_file);
        return TC_ERROR;
    }

    return TC_OK;
}

static int tc_x11_inspect(TCModuleInstance *self,
                          const char *param, const char **value)
{
    TC_MODULE_SELF_CHECK(self, "inspect");

    if (optstr_lookup(param, "help")) {
        *value = tc_x11_help;
    }

    return TC_OK;
}

static int tc_x11_stop(TCModuleInstance *self)
{
    TCX11PrivateData *priv = NULL;
    int ret = 0;

    TC_MODULE_SELF_CHECK(self, "stop");

    priv = self->userdata;

    ret = tc_x11source_close(&priv->src);
    if (ret != 0) {
        tc_log_error(MOD_NAME, "stop: failed to close X11 connection");
        return TC_ERROR;
    }

    ret = tc_timer_fini(&priv->timer);
    if (ret != 0) {
        tc_log_error(MOD_NAME, "stop: failed to stop timer");
        return TC_ERROR;
    }

    if (verbose >= TC_DEBUG) {
        tc_log_info(MOD_NAME, "expired frames count: %lu",
                              (unsigned long)priv->expired);
    }
    return TC_OK;
}

static int tc_x11_demultiplex(TCModuleInstance *self,
                              vframe_list_t *vframe, aframe_list_t *aframe)
{
    TCX11PrivateData *priv = NULL;
    int ret = 0;

    TC_MODULE_SELF_CHECK(self, "demultiplex");
    priv = self->userdata;

    if (aframe != NULL) {
        aframe->audio_len = 0; /* no audio from here */
    }

    if (vframe != NULL) {
        uint32_t elapsed = 0;

        ret = tc_x11source_acquire(&priv->src, vframe->video_buf,
                                   vframe->video_size);
        if (ret > 0) {
            vframe->attributes |= TC_FRAME_IS_KEYFRAME;
            vframe->video_len = ret;
       
            /* see (upcoming) figure above */
            elapsed = tc_timer_elapsed(&priv->timer);
            if (elapsed >= priv->frame_delay) {
                /* don't sleep at all if delay is already excessive */
                priv->expired++;
            } else {
                tc_timer_sleep(&priv->timer, priv->frame_delay - elapsed);
            }
        }
    }

    return (ret > 0) ?ret :-1;
}

/*************************************************************************/

static const TCCodecID tc_x11_codecs_in[] = { TC_CODEC_ERROR };

/* a multiplexor is at the end of pipeline */
static const TCCodecID tc_x11_codecs_out[] = { 
    TC_CODEC_RGB, TC_CODEC_YUV420P, TC_CODEC_YUV422P, TC_CODEC_ERROR 
};

static const TCFormatID tc_x11_formats_in[] = { TC_FORMAT_X11, TC_FORMAT_ERROR };
static const TCFormatID tc_x11_formats_out[] = { TC_FORMAT_ERROR };

static const TCModuleInfo tc_x11_info = {
    .features    = MOD_FEATURES,
    .flags       = MOD_FLAGS,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = tc_x11_codecs_in,
    .codecs_out  = tc_x11_codecs_out,
    .formats_in  = tc_x11_formats_in,
    .formats_out = tc_x11_formats_out
};

static const TCModuleClass tc_x11_class = {
    .info         = &tc_x11_info,

    .init         = tc_x11_init,
    .fini         = tc_x11_fini,
    .configure    = tc_x11_configure,
    .stop         = tc_x11_stop,
    .inspect      = tc_x11_inspect,

    .demultiplex  = tc_x11_demultiplex,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &tc_x11_class;
}

/*************************************************************************/
/*************************************************************************/

/* Old-fashioned module interface. */

static TCModuleInstance mod_video;

static int verbose_flag;
static int capability_flag = TC_CAP_YUV|TC_CAP_RGB|TC_CAP_YUV422|TC_CAP_VID;

#define MOD_PRE x11
#define MOD_CODEC "(video) X11"

#include "import_def.h"

/*************************************************************************/

#define RETURN_IF_FAILED(ret) do { \
    if ((ret) != TC_OK) { \
        return ret; \
    } \
} while (0)

#define COMMON_CHECK(param) do { \
    if ((param)->flag != TC_VIDEO) { \
        return TC_ERROR; \
    } \
} while (0)


MOD_open
{
    int ret;

    COMMON_CHECK(param);

    /* XXX */
    ret = tc_x11_init(&mod_video, TC_MODULE_FEATURE_DEMULTIPLEX);
    RETURN_IF_FAILED(ret);

    ret = tc_x11_configure(&mod_video, "", vob);
    RETURN_IF_FAILED(ret);

    return TC_OK;
}

MOD_decode
{
    vframe_list_t vframe;
    int ret = 0;

    COMMON_CHECK(param);

    vframe.attributes = 0;
    vframe.video_buf = param->buffer;
    vframe.video_size = param->size;

    ret = tc_x11_demultiplex(&mod_video, &vframe, NULL);

    if (ret <= 0) {
        /* well, frames from X11 never "ends", really :) */
        return TC_ERROR;
    }

    param->size = ret;
    param->attributes = vframe.attributes;
    return TC_OK;
}

MOD_close
{
    int ret;

    COMMON_CHECK(param);
    
    ret = tc_x11_stop(&mod_video);
    RETURN_IF_FAILED(ret);

    ret = tc_x11_fini(&mod_video);
    RETURN_IF_FAILED(ret);

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

