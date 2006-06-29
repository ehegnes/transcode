/*
 *  encode_lzo.c - encode video frames individually using LZO
 *  (C) 2005/2006 Francesco Romani <fromani at gmail dot com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#define MOD_NAME    "encode_lzo.so"
#define MOD_VERSION "v0.0.1 (2006-03-24)"
#define MOD_CAP     "LZO lossless video encoder"

#include "transcode.h"
#include "aclib/imgconvert.h"
#include "libtc/optstr.h"
#include "libtc/tc_lzo.h"
#include "libtc/tcmodule-plugin.h"

#include <stdio.h>
#include <stdlib.h>

/* tc_lzo_ prefix was used to avoid any possible name clash with liblzo? */

static const char *tc_lzo_help = ""
    "Overview:\n"
    "    this module encodes raw RGB/YUV video frames in LZO, using liblzo V2.\n"
    "Options:\n"
    "    help    produce module overview and options explanations\n";

typedef struct {
    lzo_byte *work_mem; /* needed by encoder to work properly */
    
    int codec;
} LZOPrivateData;

static int tc_lzo_configure(TCModuleInstance *self,
                            const char *options, vob_t *vob)
{
    LZOPrivateData *pd = NULL;
    int ret;

    if (self == NULL) {
        tc_log_error(MOD_NAME, "configure: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    pd = self->userdata;
    pd->codec = vob->im_v_codec;

    pd->work_mem = (lzo_bytep)lzo_malloc(LZO1X_1_MEM_COMPRESS);
    if (pd->work_mem == NULL) {
        tc_log_error(MOD_NAME, "configure: can't allocate LZO"
                               " compression buffer");
        return TC_EXPORT_ERROR;
    }
    
    ret = lzo_init();
    if (ret != LZO_E_OK) {
        lzo_free(pd->work_mem);
        pd->work_mem = NULL;

        tc_log_error(MOD_NAME, "configure: failed to initialize"
                               " LZO encoder");
        return TC_EXPORT_ERROR;
    }

    return TC_EXPORT_OK;
}

static int tc_lzo_stop(TCModuleInstance *self)
{
    LZOPrivateData *pd = NULL;
    if (self == NULL) {
        tc_log_error(MOD_NAME, "stop: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    pd = self->userdata;
   
    if (pd->work_mem != NULL) {
        lzo_free(pd->work_mem);
        pd->work_mem = NULL;
    }
    return TC_EXPORT_OK;
}

static int tc_lzo_init(TCModuleInstance *self)
{
    LZOPrivateData *pd = NULL;

    if (self == NULL) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    pd = tc_malloc(sizeof(LZOPrivateData));
    if (!pd) {
        tc_log_error(MOD_NAME, "init: can't allocate private data");
        return TC_EXPORT_ERROR;
    }
    /* sane defaults */
    pd->work_mem = NULL;
    pd->codec = CODEC_YUV;
    
    self->userdata = pd;
    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }
    return TC_EXPORT_OK;
}

static int tc_lzo_fini(TCModuleInstance *self)
{
    LZOPrivateData *pd = NULL;

    if (self == NULL) {
        tc_log_error(MOD_NAME, "fini: bad instance data reference");
        return TC_EXPORT_ERROR;
    }
    pd = self->userdata;

    tc_lzo_stop(self);
    
    tc_free(self->userdata);
    self->userdata = NULL;
    return TC_EXPORT_OK;
}

static const char *tc_lzo_inspect(TCModuleInstance *self,
                                  const char *param)
{
    LZOPrivateData *pd = NULL;

    if (self == NULL) {
        tc_log_error(MOD_NAME, "inspect: bad instance data reference");
        return NULL;
    }

    pd = self->userdata;

    if (optstr_lookup(param, "help")) {
        return tc_lzo_help;
    }

    return "";
}

/* ------------------------------------------------------------
 *
 * encode and export
 *
 * ------------------------------------------------------------*/

/* assert(len(data) >= TC_LZ_HDR_SIZE) */
static void tc_lzo_put_header(tc_lzo_header_t *hdr, void *data)
{
    /* always CPU byte order */
    uint32_t *ptr = data;

    *(ptr)     = hdr->magic;
    *(ptr + 1) = hdr->size;
    *(ptr + 2) = hdr->flags;
    *(ptr + 3) = (uint32_t)(hdr->method << 24 | hdr->level << 16 | hdr->pad);
}

/* maybe translation should go away */
static int tc_lzo_format_translate(int tc_codec)
{
    int ret;
    switch (tc_codec) {
      case CODEC_YUV:
        ret = TC_LZO_FORMAT_YUV420P;
        break;
      case CODEC_YUY2:
        ret = TC_LZO_FORMAT_YUY2;
        break;
      case CODEC_RGB:
        ret = TC_LZO_FORMAT_RGB24;
        break;
      default:
        /* shouldn't happen */
        ret = 0;
        break;
    }
    return ret;
}

static int tc_lzo_encode_video(TCModuleInstance *self,
                               vframe_list_t *inframe, vframe_list_t *outframe)
{
    LZOPrivateData *pd = NULL;
    lzo_uint out_len = 0;
    tc_lzo_header_t hdr;
    int ret;

    if (self == NULL) {
        tc_log_error(MOD_NAME, "encode_video: bad instance data reference");
        return TC_EXPORT_ERROR;
    }
    pd = self->userdata;
   
    /* invariants */
    hdr.magic = TC_CODEC_LZO2;
    hdr.method = 1;
    hdr.level = 1;
    hdr.pad = 0;
    hdr.flags = 0; /* sane default */

    ret = lzo1x_1_compress(inframe->video_buf, inframe->video_size,
                           outframe->video_buf + TC_LZO_HDR_SIZE,
                           &out_len, pd->work_mem);
    if (ret != LZO_E_OK) {
        /* this should NEVER happen */
        tc_log_warn(MOD_NAME, "encode_video: LZO compression failed"
                              " (errcode=%i)", ret);
        return TC_EXPORT_ERROR;
    }

    /* check for an incompressible block */
    if (out_len >= inframe->video_size)  {
        hdr.flags |= TC_LZO_NOT_COMPRESSIBLE;
        out_len = inframe->video_size;
    }
    hdr.size = out_len;

    hdr.flags |= tc_lzo_format_translate(pd->codec);
    /* always put header */
    tc_lzo_put_header(&hdr, outframe->video_buf);

    if (hdr.flags & TC_LZO_NOT_COMPRESSIBLE) {
        /* inframe data not compressible: outframe will hold a copy */
        if (verbose >= TC_DEBUG) {
            tc_log_info(MOD_NAME, "encode_video: block contains"
                                  " incompressible data");
        }
        ac_memcpy(outframe->video_buf + TC_LZO_HDR_SIZE,
                  inframe->video_buf, out_len);
    } else {
        /* outframe data already in place */
        if (verbose >= TC_DEBUG) {
            tc_log_info(MOD_NAME, "encode_video: compressed %lu bytes"
                                  " into %lu bytes",
                                  (unsigned long)inframe->video_size,
                                  (unsigned long)out_len);
        }
    }

    /* only keyframes */
    outframe->video_len = out_len + TC_LZO_HDR_SIZE;
    outframe->attributes |= TC_FRAME_IS_KEYFRAME;

    return TC_EXPORT_OK;
}

/*************************************************************************/

static const int tc_lzo_codecs_in[] = {
    TC_CODEC_YUY2, TC_CODEC_RGB, TC_CODEC_YUV420P, TC_CODEC_ERROR
};

static const int tc_lzo_codecs_out[] = {
    TC_CODEC_LZO2,
    TC_CODEC_ERROR
};

static const TCModuleInfo tc_lzo_info = {
    .features    = TC_MODULE_FEATURE_ENCODE|TC_MODULE_FEATURE_VIDEO,
    .flags       = TC_MODULE_FLAG_RECONFIGURABLE,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = tc_lzo_codecs_in,
    .codecs_out  = tc_lzo_codecs_out,
};

static const TCModuleClass tc_lzo_class = {
    .info         = &tc_lzo_info,

    .init         = tc_lzo_init,
    .fini         = tc_lzo_fini,
    .configure    = tc_lzo_configure,
    .stop         = tc_lzo_stop,
    .inspect      = tc_lzo_inspect,

    .encode_video = tc_lzo_encode_video,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &tc_lzo_class;
}

