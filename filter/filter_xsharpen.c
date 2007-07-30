/*
    filter_xsharpen.c -- VirtualDub's XSharpen Filter

    Copyright (C) 1999-2000 Donald A. Graft
      modified 2002 by Tilmann Bitterberg for use with transcode
      modified 2007 by Branko Kokanovic <branko.kokanovic at gmail dot com> to use NMS
    This file is part of transcode, a video stream processing tool

    Xsharpen Filter for VirtualDub -- sharpen by mapping pixels
    to the closest of window max or min.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    The author can be contacted at:
    Donald Graft
    http://sauron.mordor.net/dgraft/
    neuron2@home.com.
*/

#define MOD_NAME    "filter_xharpen.so"
#define MOD_VERSION "(1.0b3) (2007-07-29)"
#define MOD_CAP     "VirtualDub's XSharpen Filter"
#define MOD_AUTHOR  "Donald Graft, Tilmann Bitterberg"
#define MOD_FEATURES \
    TC_MODULE_FEATURE_FILTER|TC_MODULE_FEATURE_VIDEO
#define MOD_FLAGS \
    TC_MODULE_FLAG_RECONFIGURABLE

/* -------------------------------------------------
 *
 * mandatory include files
 *
 *-------------------------------------------------*/

#include "transcode.h"
#include "filter.h"
#include "libtc/libtc.h"
#include "libtc/optstr.h"
#include "libtc/tcmodule-plugin.h"
#include "aclib/imgconvert.h"


/* vdub compat */
typedef unsigned int	Pixel;
typedef unsigned int	Pixel32;
typedef unsigned char	Pixel8;
typedef int		PixCoord;
typedef	int		PixDim;
typedef	int		PixOffset;

static const char xsharpen_help[] = ""
    "Overview\n"
    "   This filter performs a subtle but useful sharpening effect. The\n"
    "   result is a sharpening effect that not only avoids amplifying\n"
    "   noise, but also tends to reduce it. A welcome side effect is that\n"
    "   files processed with this filter tend to compress to smaller files.\n"
    "\n"
    "Options\n"
    "   Strength 'strength' (0-255) [200]\n"
    "   When this value is 255, mapped pixels are not blended with the\n"
    "   original pixel values, so a full-strength effect is obtained. As\n"
    "   the value is reduced, each mapped pixel is blended with more of the\n"
    "   original pixel. At a value of 0, the original pixels are passed\n"
    "   through and there is no sharpening effect.\n"
    "\n"
    "   Threshold 'threshold' (0-255) [255]\n"
    "   This value determines how close a pixel must be to the brightest or\n"
    "   dimmest pixel to be mapped. If a pixel is more than threshold away\n"
    "   from the brightest or dimmest pixel, it is not mapped.  Thus, as\n"
    "   the threshold is reduced, pixels in the mid range start to be\n"
    "   spared.\n";

/*************************************************************************/

typedef struct MyFilterData {
    Pixel32		*convertFrameIn;
    Pixel32		*convertFrameOut;
    int		strength;
    int		strengthInv;
    int		threshold;
    int		srcPitch;
    int		dstPitch;
    int codec;
    TCVHandle	tcvhandle;
    char conf_str[TC_BUF_MIN];
} MyFilterData;

/*************************************************************************/

/* Module interface routines and data. */

/*************************************************************************/

/**
 * xsharpen_init:  Initialize this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int xsharpen_init(TCModuleInstance *self, uint32_t features)
{
    MyFilterData *mfd = NULL;

    TC_MODULE_SELF_CHECK(self, "init");
    TC_MODULE_INIT_CHECK(self, MOD_FEATURES, features);

    mfd = tc_malloc(sizeof(MyFilterData));
    if (!mfd) {
        tc_log_error(MOD_NAME, "init: out of memory!");
        return TC_ERROR;
    }
    self->userdata = mfd;

    /* initialize data */
    mfd->strength       = 200; /* 255 is too much */
    mfd->strengthInv    = 255 - mfd->strength;
    mfd->threshold      = 255;
    mfd->srcPitch       = 0;
    mfd->dstPitch       = 0;
    mfd->convertFrameIn = NULL;
    mfd->convertFrameOut= NULL;

    return TC_OK;
}

/*************************************************************************/

/**
 * xsharpen_fini:  Clean up after this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int xsharpen_fini(TCModuleInstance *self)
{
    MyFilterData *mfd = NULL;

    TC_MODULE_SELF_CHECK(self, "fini");

    mfd = self->userdata;

    if (mfd->convertFrameIn)
        tc_free(mfd->convertFrameIn);
    mfd->convertFrameIn = NULL;

    if (mfd->convertFrameOut)
        tc_free(mfd->convertFrameOut);
    mfd->convertFrameOut = NULL;

    if (mfd->tcvhandle){
        tcv_free(mfd->tcvhandle);
    }
    mfd->tcvhandle = 0;

    tc_free(self->userdata);
    self->userdata = NULL;

    return TC_OK;
}

/*************************************************************************/

/**
 * xsharpen_configure:  Configure this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int xsharpen_configure(TCModuleInstance *self,
                          const char *options, vob_t *vob)
{
    MyFilterData *mfd = NULL;
	int width, height;

    TC_MODULE_SELF_CHECK(self, "configure");

    mfd = self->userdata;

	if(vob == NULL){
        return TC_ERROR;
    }

	height = vob->ex_v_height;
	width  = vob->ex_v_width;

    /* setup defaults */
    mfd->codec          = vob->im_v_codec;
    mfd->strength       = 200; /* 255 is too much */
    mfd->strengthInv    = 255 - mfd->strength;
    mfd->threshold      = 255;
    mfd->srcPitch       = 0;
    mfd->dstPitch       = 0;

    if (options) {
        if (verbose >= TC_STATS) {
            tc_log_info(MOD_NAME, "options=%s", options);
        }
        optstr_get (options, "strength",  "%d", &mfd->strength);
        optstr_get (options, "threshold", "%d", &mfd->threshold);
    }
    mfd->strengthInv    = 255 - mfd->strength;

    if (verbose > TC_INFO) {
        tc_log_info (MOD_NAME, " XSharpen Filter Settings (%dx%d):", width,height);
        tc_log_info (MOD_NAME, "          strength = %d", mfd->strength);
        tc_log_info (MOD_NAME, "         threshold = %d", mfd->threshold);
    }

    /* fetch memory */
    mfd->convertFrameIn = tc_malloc (width*height*sizeof(Pixel32));
    if (!mfd->convertFrameIn) {
        tc_log_error(MOD_NAME, "No memory at %d!", __LINE__);
        return TC_ERROR;
    }
    memset(mfd->convertFrameIn, 0, width*height*sizeof(Pixel32));

    mfd->convertFrameOut = tc_malloc (width*height*sizeof(Pixel32));
    if (!mfd->convertFrameOut) {
        tc_log_error(MOD_NAME, "No memory at %d!", __LINE__);
        return TC_ERROR;
    }
    memset(mfd->convertFrameOut, 0, width*height*sizeof(Pixel32));

    mfd->tcvhandle = tcv_init();

    /* filter init ok */
    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }

    return TC_OK;
}

/*************************************************************************/

/**
 * xsharpen_stop:  Reset this instance of the module.  See tcmodule-data.h
 * for function details.
 */

static int xsharpen_stop(TCModuleInstance *self)
{
    TC_MODULE_SELF_CHECK(self, "stop");
    return TC_OK;
}

/*************************************************************************/

/**
 * xsharpen_inspect:  Return the value of an option in this instance of
 * the module.  See tcmodule-data.h for function details.
 */

static int xsharpen_inspect(TCModuleInstance *self,
                        const char *param, const char **value)
{
    MyFilterData *mfd = NULL;

    TC_MODULE_SELF_CHECK(self, "inspect");
    TC_MODULE_SELF_CHECK(param, "inspect");
    TC_MODULE_SELF_CHECK(value, "inspect");

    mfd = self->userdata;

    if (optstr_lookup(param, "help")) {
        *value = xsharpen_help;
    }
    if (optstr_lookup(param, "strength")) {
        tc_snprintf(mfd->conf_str, sizeof(mfd->conf_str),"ypos=%d",mfd->strength);
        *value = mfd->conf_str;
    }
    if (optstr_lookup(param, "threshold")) {
        tc_snprintf(mfd->conf_str, sizeof(mfd->conf_str),"ypos=%d",mfd->threshold);
        *value = mfd->conf_str;
    }

    return TC_OK;
}

/*************************************************************************/

/**
 * xsharpen_filter_video:  show something on given frame of the video
 * stream.  See tcmodule-data.h for function details.
 */

static int xsharpen_filter_video(TCModuleInstance *self, vframe_list_t *frame)
{
    MyFilterData *mfd = NULL;
    int post = 0;

    TC_MODULE_SELF_CHECK(self, "filer_video");
    TC_MODULE_SELF_CHECK(frame, "filer_video");

    mfd = self->userdata;
    post = (frame->tag & TC_POST_M_PROCESS);

    if(post && !(frame->attributes & TC_FRAME_IS_SKIPPED)) {
        if (mfd->codec == CODEC_RGB) {
            const PixDim    width  = frame->v_width;
            const PixDim    height = frame->v_height;
            Pixel32         *src, *dst;
            int             x, y;
            int             r, g, b, R, G, B;
            Pixel32         p, min=1000, max=-1;
            int             luma, lumac, lumamax, lumamin, mindiff, maxdiff;
            const int       srcpitch = frame->v_width*sizeof(Pixel32);
            const int       dstpitch = frame->v_width*sizeof(Pixel32);

            Pixel32 * dst_buf;
            Pixel32 * src_buf;

            tcv_convert(mfd->tcvhandle, frame->video_buf,
                (uint8_t *)mfd->convertFrameIn, frame->v_width, frame->v_height,
                IMG_RGB24, IMG_BGRA32);

            src_buf = mfd->convertFrameIn;
            dst_buf = mfd->convertFrameOut;

            /* First copy through the four border lines. */
            src	= src_buf;
            dst	= dst_buf;
            for (x = 0; x < width; x++){
                dst[x] = src[x];
            }
            src	= (Pixel *)((char *)src_buf + (height - 1) * srcpitch);
            dst	= (Pixel *)((char *)dst_buf + (height - 1) * dstpitch);
            for (x = 0; x < width; x++){
                dst[x] = src[x];
            }
            src	= src_buf;
            dst	= dst_buf;
            for (y = 0; y < height; y++){
                dst[0] = src[0];
                dst[width-1] = src[width-1];
                src	= (Pixel *)((char *)src + srcpitch);
                dst	= (Pixel *)((char *)dst + dstpitch);
            }

            /* Now calculate and store the pixel luminances for the remaining pixels. */
            src	= src_buf;
            for (y = 0; y < height; y++){
                for (x = 0; x < width; x++){
                    r = (src[x] >> 16) & 0xff;
                    g = (src[x] >> 8) & 0xff;
                    b = src[x] & 0xff;
                    luma = (55 * r + 182 * g + 19 * b) >> 8;
                    src[x] &= 0x00ffffff;
                    src[x] |= (luma << 24);
                }
                src	= (Pixel *)((char *)src + srcpitch);
            }

            /* Finally run the 3x3 rank-order sharpening kernel over the pixels. */
            src = (Pixel *)((char *)src_buf + srcpitch);
            dst = (Pixel *)((char *)dst_buf + dstpitch);
            for (y = 1; y < height - 1; y++){
                for (x = 1; x < width - 1; x++){
                    /* Find the brightest and dimmest pixels in the 3x3 window
                    surrounding the current pixel. */
                    lumamax = -1;
                    lumamin = 1000;

                    p = ((Pixel32 *)((char *)src - srcpitch))[x-1];
                    luma = p >> 24;
                    if (luma > lumamax){
                        lumamax = luma;
                        max = p;
                    }
                    if (luma < lumamin){
                        lumamin = luma;
                        min = p;
                    }

                    p = ((Pixel32 *)((char *)src - srcpitch))[x];
                    luma = p >> 24;
                    if (luma > lumamax){
                        lumamax = luma;
                        max = p;
                    }
                    if (luma < lumamin){
                        lumamin = luma;
                        min = p;
                    }

                    p = ((Pixel32 *)((char *)src - srcpitch))[x+1];
                    luma = p >> 24;
                    if (luma > lumamax){
                        lumamax = luma;
                        max = p;
                    }
                    if (luma < lumamin){
                        lumamin = luma;
                        min = p;
                    }

                    p = src[x-1];
                    luma = p >> 24;
                    if (luma > lumamax){
                        lumamax = luma;
                        max = p;
                    }
                    if (luma < lumamin){
                        lumamin = luma;
                        min = p;
                    }

                    p = src[x];
                    lumac = luma = p >> 24;
                    if (luma > lumamax){
                        lumamax = luma;
                        max = p;
                    }
                    if (luma < lumamin){
                        lumamin = luma;
                        min = p;
                    }

                    p = src[x+1];
                    luma = p >> 24;
                    if (luma > lumamax){
                        lumamax = luma;
                        max = p;
                    }
                    if (luma < lumamin){
                        lumamin = luma;
                        min = p;
                    }

                    p = ((Pixel32 *)((char *)src + srcpitch))[x-1];
                    luma = p >> 24;
                    if (luma > lumamax){
                        lumamax = luma;
                        max = p;
                    }
                    if (luma < lumamin){
                        lumamin = luma;
                        min = p;
                    }

                    p = ((Pixel32 *)((char *)src + srcpitch))[x];
                    luma = p >> 24;
                    if (luma > lumamax){
                        lumamax = luma;
                        max = p;
                    }
                    if (luma < lumamin){
                        lumamin = luma;
                        min = p;
                    }

                    p = ((Pixel32 *)((char *)src + srcpitch))[x+1];
                    luma = p >> 24;
                    if (luma > lumamax){
                        lumamax = luma;
                        max = p;
                    }
                    if (luma < lumamin){
                        lumamin = luma;
                        min = p;
                    }

                    /* Determine whether the current pixel is closer to the
                       brightest or the dimmest pixel. Then compare the current
                       pixel to that closest pixel. If the difference is within
                       threshold, map the current pixel to the closest pixel;
                       otherwise pass it through. */
                    p = -1;
                    if (mfd->strength != 0){
                        mindiff = lumac - lumamin;
                        maxdiff = lumamax - lumac;
                        if (mindiff > maxdiff){
                            if (maxdiff < mfd->threshold){
                                p = max;
                            }
                        }else{
                            if (mindiff < mfd->threshold){
                                p = min;
                            }
                        }
                    }
                    if (p == -1){
                        dst[x] = src[x];
                    }else{
                        R = (src[x] >> 16) & 0xff;
                        G = (src[x] >> 8) & 0xff;
                        B = src[x] & 0xff;
                        r = (p >> 16) & 0xff;
                        g = (p >> 8) & 0xff;
                        b = p & 0xff;
                        r = (mfd->strength * r + mfd->strengthInv * R) / 255;
                        g = (mfd->strength * g + mfd->strengthInv * G) / 255;
                        b = (mfd->strength * b + mfd->strengthInv * B) / 255;
                        dst[x] = (r << 16) | (g << 8) | b;
                    }
                }
                src	= (Pixel *)((char *)src + srcpitch);
                dst	= (Pixel *)((char *)dst + dstpitch);
            }

            tcv_convert(mfd->tcvhandle, (uint8_t *)mfd->convertFrameOut,
                frame->video_buf, frame->v_width, frame->v_height,
                IMG_BGRA32, IMG_RGB24);

            return TC_OK;
        }
        if (mfd->codec == CODEC_YUV) {
            const PixDim       width = frame->v_width;
            const PixDim       height = frame->v_height;
            char               *src, *dst;
            int                x, y;
            int        	   luma, lumac, lumamax, lumamin;
            int 		   p, mindiff, maxdiff;
            const int	   srcpitch = frame->v_width;
            const int	   dstpitch = frame->v_width;

            char * src_buf = frame->video_buf;
            static char * dst_buf = NULL;

            if (!dst_buf){
                dst_buf =  tc_malloc (width*height*3/2);
            }

            /* First copy through the four border lines. */
            /* first */
            src	= src_buf;
            dst	= dst_buf;
            ac_memcpy (dst, src, width);

            /* last */
            src     = src_buf+srcpitch*(height-1);
            dst     = dst_buf+dstpitch*(height-1);
            ac_memcpy (dst, src, width);

            /* copy Cb and Cr */
            ac_memcpy (dst_buf+dstpitch*height, src_buf+srcpitch*height, width*height>>1);

            src	= src_buf;
            dst	= dst_buf;
            for (y = 0; y < height; y++){
                *dst = *src;
                *(dst+width-1) = *(src+width-1);
                dst += dstpitch;
                src += srcpitch;
            }

            src = src_buf+srcpitch;
            dst = dst_buf+dstpitch;

            /* Finally run the 3x3 rank-order sharpening kernel over the pixels. */
            for (y = 1; y < height - 1; y++){
                for (x = 1; x < width - 1; x++){
                    /* Find the brightest and dimmest pixels in the 3x3 window
                       surrounding the current pixel. */
                    lumamax = -1000;
                    lumamin = 1000;

                    luma = (src - srcpitch)[x-1] &0xff;
                    if (luma > lumamax)
                        lumamax = luma;
                    if (luma < lumamin)
                        lumamin = luma;

                    luma = (src - srcpitch)[x] &0xff;
                    if (luma > lumamax)
                        lumamax = luma;
                    if (luma < lumamin)
                        lumamin = luma;
                    luma = (src - srcpitch)[x+1] &0xff;
                    if (luma > lumamax)
                        lumamax = luma;
                    if (luma < lumamin)
                        lumamin = luma;

                    luma = src[x-1] &0xff;
                    if (luma > lumamax)
                        lumamax = luma;
                    if (luma < lumamin)
                        lumamin = luma;
                    luma = src[x] &0xff;
                    lumac = luma;
                    if (luma > lumamax)
                        lumamax = luma;
                    if (luma < lumamin)
                        lumamin = luma;

                    luma = src[x+1] &0xff;
                    if (luma > lumamax)
                        lumamax = luma;
                    if (luma < lumamin)
                        lumamin = luma;

                    luma = (src + srcpitch)[x-1] &0xff;
                    if (luma > lumamax)
                        lumamax = luma;
                    if (luma < lumamin)
                        lumamin = luma;

                    luma = (src + srcpitch)[x] &0xff;
                    if (luma > lumamax)
                        lumamax = luma;
                    if (luma < lumamin)
                        lumamin = luma;

                    luma = (src + srcpitch)[x+1] &0xff;
                    if (luma > lumamax)
                        lumamax = luma;
                    if (luma < lumamin)
                        lumamin = luma;

                    /* Determine whether the current pixel is closer to the
                       brightest or the dimmest pixel. Then compare the current
                       pixel to that closest pixel. If the difference is within
                       threshold, map the current pixel to the closest pixel;
                       otherwise pass it through. */

                    p = -1;
                    if (mfd->strength != 0){
                        mindiff = lumac   - lumamin;
                        maxdiff = lumamax - lumac;
                        if (mindiff > maxdiff){
                            if (maxdiff < mfd->threshold)
                            p = lumamax&0xff;
                        }else{
                            if (mindiff < mfd->threshold)
                            p = lumamin&0xff;
                        }
                    }
                    if (p == -1){
                        dst[x] = src[x];
                    }else{
                        int t;
                        lumac = src[x] &0xff;
                        t = ((mfd->strength*p + mfd->strengthInv*lumac)/255) & 0xff;
                        if (t>240) t = 240;
                        if (t<16)  t = 16;
                        dst[x] = t&0xff;
                    }
                }
                src += srcpitch;
                dst += dstpitch;
            }

            ac_memcpy (frame->video_buf, dst_buf, width*height*3/2);
            return TC_OK;
        }
    }
    return TC_OK;
}

/*************************************************************************/

static const TCCodecID xsharpen_codecs_in[] = { 
    TC_CODEC_RGB, TC_CODEC_YUV420P, TC_CODEC_ERROR
};
static const TCCodecID xsharpen_codecs_out[] = {
    TC_CODEC_RGB, TC_CODEC_YUV420P, TC_CODEC_ERROR
};
static const TCFormatID xsharpen_formats[] = {
    TC_FORMAT_ERROR
};

static const TCModuleInfo xsharpen_info = {
    .features    = MOD_FEATURES,
    .flags       = MOD_FLAGS,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = xsharpen_codecs_in,
    .codecs_out  = xsharpen_codecs_out,
    .formats_in  = xsharpen_formats,
    .formats_out = xsharpen_formats
};

static const TCModuleClass xsharpen_class = {
    .info         = &xsharpen_info,

    .init         = xsharpen_init,
    .fini         = xsharpen_fini,
    .configure    = xsharpen_configure,
    .stop         = xsharpen_stop,
    .inspect      = xsharpen_inspect,

    .filter_video = xsharpen_filter_video
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &xsharpen_class;
}

/*************************************************************************/

static int xsharpen_get_config(TCModuleInstance *self, char *options)
{
    MyFilterData *mfd = NULL;
    char buf[TC_BUF_MIN];

    TC_MODULE_SELF_CHECK(self, "get_config");

    mfd = self->userdata;

    optstr_filter_desc(options, MOD_NAME, MOD_CAP, MOD_VERSION,
                       MOD_AUTHOR, "VRYO", "1");

    /* can be omitted */
    optstr_param (options, "help", "VirtualDub's XSharpen Filter", "", "0");

    tc_snprintf (buf, sizeof(buf), "%d", mfd->strength);
    optstr_param (options, "strength", "How much  of the effect", "%d", buf, "0", "255");

    tc_snprintf (buf, sizeof(buf), "%d", mfd->threshold);
    optstr_param (options, "threshold",
        "How close a pixel must be to the brightest or dimmest pixel to be mapped",
        "%d", buf, "0", "255");

    return TC_OK;
}


static int xsharpen_process(TCModuleInstance *self, 
                            frame_list_t *frame)
{
    TC_MODULE_SELF_CHECK(self, "process");

    if (frame->tag & TC_VIDEO) {
        return xsharpen_filter_video(self, (vframe_list_t*)frame);
    }
    return TC_OK;
}

/*************************************************************************/

/* Old-fashioned module interface. */

TC_FILTER_OLDINTERFACE(xsharpen)

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
