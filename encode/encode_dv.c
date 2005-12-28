/*
 *  encode_dv.c - encode a DV video stream using libdv
 *  (C) Francesco Romani <fromani at gmail dot com> - December 2005
 *  Based on code
 *  Copyright (C) Thomas Östreich et Al. - June 2001
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <libdv/dv.h>

#include "transcode.h"
#include "aclib/imgconvert.h"
#include "libtc/optstr.h"

#include "libtc/tcmodule-plugin.h"

#define MOD_NAME    "encode_dv.so"
#define MOD_VERSION "v0.0.1 (2005-12-26)"
#define MOD_CAP     "Digital Video encoder"

static const char *tcdv_help = ""
    "Overview:\n"
    "\tthis module is capable to encode raw RGB/YUV video frames\n"
    "\tinto a DV encoded stream using libdv.\n"
    "Options:\n"
    "\tHelp\tproduce module overview and options explanations\n";


#include <stdio.h>
#include <stdlib.h>
#include <libdv/dv.h>
#include "transcode.h"
#include "aclib/imgconvert.h"

typedef struct {
    int frame_size;
    int is_yuv;

    int dv_yuy2_mode;

    dv_encoder_t *dvenc;
    uint8_t *conv_buf;
} DVPrivateData;

static int tcdv_init(TCModuleInstance *self)
{
    DVPrivateData *pd = NULL;
    vob_t *vob = tc_get_vob();
    
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    pd = tc_malloc(sizeof(DVPrivateData));
    if (!pd) {
        tc_log_error(MOD_NAME, "init: can't allocate private data");
        return TC_EXPORT_ERROR;
    }

    pd->dvenc = dv_encoder_new(FALSE, FALSE, FALSE);
    if (!pd->dvenc) {
        tc_log_error(MOD_NAME, "init: can't allocate encoder data");
        goto failed_alloc_dvenc;
    }
    
    if(vob->dv_yuy2_mode) {
        pd->conv_buf = tc_bufalloc(PAL_W * PAL_H * 2); /* max framne size */
        if (!pd->conv_buf) {
            tc_log_error(MOD_NAME, "init: can't allocate private buffer");
            goto failed_alloc_privbuf;
        }
        pd->dv_yuy2_mode = 1;
    } else {
        pd->conv_buf = NULL;
        pd->dv_yuy2_mode = 0;
    }

    pd->frame_size = 0;
    pd->is_yuv = -1; /* invalid value */
    
    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }
    self->userdata = pd;
    
    return TC_EXPORT_OK;

failed_alloc_privbuf:
    dv_encoder_free(pd->dvenc);
failed_alloc_dvenc:    
    tc_free(pd);    
    return TC_EXPORT_ERROR;
}

static int tcdv_fini(TCModuleInstance *self)
{
    DVPrivateData *pd = NULL;
    
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }
    pd = self->userdata;

    if (pd->conv_buf != NULL) {
        tc_free(pd->conv_buf);
    }
    dv_encoder_free(pd->dvenc);
    tc_free(pd);

    self->userdata = NULL;
    return 0;
}

static const char *tcdv_configure(TCModuleInstance *self,
                                 const char *options)
{
    DVPrivateData *pd = NULL;
    vob_t *vob = tc_get_vob();
    
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return NULL;
    }

    pd = self->userdata;
    
    if (optstr_lookup(options, "help")) {
        return tcdv_help;
    }

    switch (vob->im_v_codec) {
      case CODEC_RGB:
        pd->is_yuv = 0;
        break;
      case CODEC_YUV:
        pd->is_yuv = 1;
        break;
      default:
        tc_log_error(MOD_NAME, "codec not supported");
        return NULL;
    }

    // for reading
    pd->frame_size = (vob->ex_v_height==PAL_H) 
                        ?TC_FRAME_DV_PAL :TC_FRAME_DV_NTSC;

    pd->dvenc->isPAL = (vob->ex_v_height == PAL_H) ?1 :0;
    pd->dvenc->is16x9 = FALSE;
    pd->dvenc->vlc_encode_passes = 3;
    pd->dvenc->static_qno = 0;
    pd->dvenc->force_dct = DV_DCT_AUTO;

    return "";
}


/* ------------------------------------------------------------
 *
 * encode and export
 *
 * ------------------------------------------------------------*/

#define DV_INIT_PLANES(pixels, buf, w, h) \
    pixels[0] = (buf); \
    pixels[1] = pixels[0] + (w * h); \
   	pixels[2] = pixels[1] + ((w / 2) * (h / 2));

static int tcdv_encode_video(TCModuleInstance *self,
                             vframe_list_t *inframe, vframe_list_t *outframe)
{
    DVPrivateData *pd = NULL;
    uint8_t *pixels[3];
    time_t now = time(NULL);
    int w, h;

    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    pd = self->userdata;
    w = (pd->dvenc->isPAL) ?PAL_W :NTSC_W;
    h = (pd->dvenc->isPAL) ?PAL_H :NTSC_H;
    
    DV_INIT_PLANES(pixels, inframe->video_buf, w, h);

    if (pd->dv_yuy2_mode) {
        uint8_t *conv_pixels[3];
        DV_INIT_PLANES(conv_pixels, pd->conv_buf, w, h);
        
        ac_imgconvert(pixels, IMG_YUV420P, conv_pixels, IMG_YUY2,
		              PAL_W, (pd->dvenc->isPAL)? PAL_H : NTSC_H);

        /* adjust main pointers */
        DV_INIT_PLANES(pixels, pd->conv_buf, w, h);
    }

    dv_encode_full_frame(pd->dvenc, pixels,
                         (pd->is_yuv) ?e_dv_color_yuv :e_dv_color_rgb,
                         outframe->video_buf);
    outframe->video_size = pd->frame_size;

    dv_encode_metadata(outframe->video_buf, pd->dvenc->isPAL,
                       pd->dvenc->is16x9, &now, 0);
    dv_encode_timecode(outframe->video_buf, pd->dvenc->isPAL, 0);

    /* only keyframes */
    outframe->attributes |= TC_FRAME_IS_KEYFRAME;

    return TC_EXPORT_OK;
}

static int tcdv_stop(TCModuleInstance *self) 
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    /* we don't need to do anything here */
    
    return 0;
}

/*************************************************************************/

static const int tcdv_codecs_in[] = { 
    TC_CODEC_YUY2, TC_CODEC_RGB, TC_CODEC_YUV420P, 
    TC_CODEC_ERROR
};

static const int tcdv_codecs_out[] = { 
    TC_CODEC_DV,
    TC_CODEC_ERROR
};

static const TCModuleInfo tcdv_info = {
    .features    = TC_MODULE_FEATURE_ENCODE|TC_MODULE_FEATURE_VIDEO
                   |TC_MODULE_FEATURE_AUDIO,
    .flags       = TC_MODULE_FLAG_RECONFIGURABLE,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = tcdv_codecs_in,
    .codecs_out  = tcdv_codecs_out
};

static const TCModuleClass tcdv_class = {
    .info         = &tcdv_info,

    .init         = tcdv_init,
    .fini         = tcdv_fini,
    .configure    = tcdv_configure,
    .stop         = tcdv_stop,
    
    .encode_video = tcdv_encode_video
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &tcdv_class;
}

