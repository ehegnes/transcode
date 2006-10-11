/*
 * encode_faac.c - encode audio frames using FAAC
 * Written by Andrew Church <achurch@achurch.org>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "transcode.h"
#include "libtc/libtc.h"
#include "libtc/optstr.h"
#include "libtc/tcmodule-plugin.h"
#include "libtcvideo/tcvideo.h"

#include <faac.h>

#define MOD_NAME    	"encode_faac.so"
#define MOD_VERSION 	"v0.1 (2006-10-11)"
#define MOD_CAP         "Encodes audio to AAC using FAAC (currently BROKEN)"
#define MOD_AUTHOR      "Andrew Church"

/*************************************************************************/

/* Local data structure: */

typedef struct {
    faacEncHandle handle;
    unsigned long framesize;  // samples per AAC frame
    int bps;  // bytes per sample
    /* FAAC only takes complete frames as input, so we buffer as needed. */
    uint8_t *audiobuf;
    int audiobuf_len;  // in samples
} PrivateData;

/*************************************************************************/
/*************************************************************************/

/* Module interface routines and data. */

/*************************************************************************/

/**
 * faacmod_init:  Initialize this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int faac_init(TCModuleInstance *self)
{
    PrivateData *pd;

    if (!self) {
        tc_log_error(MOD_NAME, "init: self == NULL!");
        return -1;
    }

    self->userdata = pd = tc_malloc(sizeof(PrivateData));
    if (!pd) {
        tc_log_error(MOD_NAME, "init: out of memory!");
        return -1;
    }
    pd->handle = 0;
    pd->audiobuf = NULL;

    /* FIXME: shouldn't this test a specific flag? */
    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
        if (verbose & TC_INFO) {
            char *id, *copyright;
            faacEncGetVersion(&id, &copyright);
            tc_log_info(MOD_NAME, "Using FAAC %s", id);
        }
    }
    return 0;
}

/*************************************************************************/

/**
 * faac_configure:  Configure this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int faac_configure(TCModuleInstance *self,
                          const char *options, vob_t *vob)
{
    PrivateData *pd;
    int samplerate = vob->mp3frequency ? vob->mp3frequency : vob->a_rate;
    unsigned long dummy;
    faacEncConfiguration conf;

    if (!self) {
       return -1;
    }
    pd = self->userdata;

    /* Save bytes per sample */
    pd->bps = (vob->dm_chan * vob->dm_bits) / 8;

    /* Create FAAC handle (freeing any old one that might be left over) */
    if (pd->handle)
        faacEncClose(pd->handle);
    pd->handle = faacEncOpen(samplerate, vob->dm_chan, &pd->framesize, &dummy);
    if (!pd->handle) {
        tc_log_error(MOD_NAME, "FAAC initialization failed");
        return -1;
    }

    /* Set up audio parameters */
    conf = *faacEncGetCurrentConfiguration(pd->handle);
    conf.mpegVersion = MPEG4;
    conf.aacObjectType = MAIN;
    conf.allowMidside = 1;
    conf.useLfe = 0;
    conf.useTns = 1;
    conf.bitRate = vob->mp3bitrate / vob->dm_chan;
    conf.bandWidth = 0;  // automatic configuration
    conf.quantqual = 100;  // FIXME: quality should be a per-module setting
    conf.outputFormat = 1;
    if (vob->dm_bits != 16) {
        tc_log_error(MOD_NAME, "Only 16-bit samples supported");
        return -1;
    }
    conf.inputFormat = FAAC_INPUT_16BIT;
    conf.shortctl = SHORTCTL_NORMAL;
    if (!faacEncSetConfiguration(pd->handle, &conf)) {
        tc_log_error(MOD_NAME, "Failed to set FAAC configuration");
        faacEncClose(pd->handle);
        pd->handle = 0;
        return -1;
    }

    /* Allocate local audio buffer */
    if (pd->audiobuf)
        free(pd->audiobuf);
    pd->audiobuf = tc_malloc(pd->framesize * pd->bps);
    if (!pd->audiobuf) {
        tc_log_error(MOD_NAME, "Unable to allocate audio buffer");
        faacEncClose(pd->handle);
        pd->handle = 0;
        return -1;
    }

    return 0;
}

/*************************************************************************/

/**
 * faac_inspect:  Return the value of an option in this instance of the
 * module.  See tcmodule-data.h for function details.
 */

static int faac_inspect(TCModuleInstance *self,
                       const char *param, const char **value)
{
    static char buf[TC_BUF_MAX];

    if (!self || !param)
       return -1;

    if (optstr_lookup(param, "help")) {
        tc_snprintf(buf, sizeof(buf),
                "Overview:\n"
                "    Encodes audio to AAC using the FAAC library.\n"
                "No options available.\n");
        *value = buf;
    }
    return 0;
}

/*************************************************************************/

/**
 * faac_stop:  Reset this instance of the module.  See tcmodule-data.h for
 * function details.
 */

static int faac_stop(TCModuleInstance *self)
{
    PrivateData *pd;

    if (!self) {
       return -1;
    }
    pd = self->userdata;

    if (pd->handle) {
        faacEncClose(pd->handle);
        pd->handle = NULL;
    }

    return 0;
}

/*************************************************************************/

/**
 * faac_fini:  Clean up after this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int faac_fini(TCModuleInstance *self)
{
    if (!self) {
       return -1;
    }
    faac_stop(self);
    tc_free(self->userdata);
    self->userdata = NULL;
    return 0;
}

/*************************************************************************/

/**
 * faac_encode:  Encode a frame of data.  See tcmodule-data.h for
 * function details.
 */

static int faac_encode(TCModuleInstance *self,
                       aframe_list_t *in, aframe_list_t *out)
{
    PrivateData *pd;
    uint8_t *inptr;
    int nsamples;

    if (!self) {
        tc_log_error(MOD_NAME, "encode: self == NULL!");
        return -1;
    }
    pd = self->userdata;

    if (in) {
        inptr = in->audio_buf;
        nsamples = in->audio_size / pd->bps;
    } else {
        inptr = NULL;
        nsamples = 0;
    }
    out->audio_len = 0;

    while (pd->audiobuf_len + nsamples >= pd->framesize) {
        int res;
        const int tocopy = (pd->framesize - pd->audiobuf_len) * pd->bps;
        ac_memcpy(pd->audiobuf + pd->audiobuf_len*pd->bps, inptr, tocopy);
        inptr += tocopy;
        nsamples -= tocopy / pd->bps;
        pd->audiobuf_len = 0;
        res = faacEncEncode(pd->handle, (int32_t *)pd->audiobuf, pd->framesize,
                            out->audio_buf + out->audio_len,
                            out->audio_size - out->audio_len);
        if (res > out->audio_size - out->audio_len) {
            tc_log_error(MOD_NAME,
                         "Output buffer overflow!  Try a lower bitrate.");
            return -1;
        }
        out->audio_len += res;
    }

    if (nsamples > 0) {
        ac_memcpy(pd->audiobuf + pd->audiobuf_len*pd->bps, inptr,
                  nsamples*pd->bps);
        pd->audiobuf_len += nsamples;
    }
    return 0;
}

/*************************************************************************/

static const int faac_codecs_in[] = { TC_CODEC_PCM, TC_CODEC_ERROR };
static const int faac_codecs_out[] = { TC_CODEC_AAC, TC_CODEC_ERROR };

static const TCModuleInfo faac_info = {
    .features    = TC_MODULE_FEATURE_ENCODE
                 | TC_MODULE_FEATURE_AUDIO,
    .flags       = TC_MODULE_FLAG_RECONFIGURABLE,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = faac_codecs_in,
    .codecs_out  = faac_codecs_out
};

static const TCModuleClass faac_class = {
    .info         = &faac_info,

    .init         = faac_init,
    .fini         = faac_fini,
    .configure    = faac_configure,
    .stop         = faac_stop,
    .inspect      = faac_inspect,

    .encode_audio = faac_encode,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &faac_class;
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
