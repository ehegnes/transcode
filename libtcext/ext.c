/*
 *  ext.c - glue code for interfacing transcode with external libraries.
 *  Written by Thomas Oestreich, Francesco Romani, Andrew Church, and others
 *
 *  This file is part of transcode, a video stream processing tool.
 *  transcode is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  transcode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pthread.h>

#include "libtcutil/tcutil.h"
#include "libtc/libtc.h"
#include "aclib/ac.h"

#include "tc_ogg.h"

/*************************************************************************/
/* OGG support                                                           */
/*************************************************************************/

/* watch out the 'n' here */
#ifndef TC_ENCODER
void tc_ogg_del_packet(ogg_packet *op);
void tc_ogg_del_extradata(OGGExtraData *oxd);
int tc_ogg_dup_packet(ogg_packet *dst, const ogg_packet *src);
#endif /* TC_ENCODER */

void tc_ogg_del_packet(ogg_packet *op)
{
    tc_free(op->packet);
    memset(op, 0, sizeof(*op));
}

void tc_ogg_del_extradata(OGGExtraData *oxd)
{
    oxd->magic = TC_CODEC_ERROR;
    tc_ogg_del_packet(&oxd->header);
    tc_ogg_del_packet(&oxd->comment);
    tc_ogg_del_packet(&oxd->code);
}

int tc_ogg_dup_packet(ogg_packet *dst, const ogg_packet *src)
{
    int ret = TC_ERROR;

    ac_memcpy(dst, src, sizeof(ogg_packet));
    dst->packet = tc_malloc(src->bytes);
    if (dst->packet) {
        ac_memcpy(dst->packet, src->packet, src->bytes);
        ret = TC_OK;
    }
    return ret;
}


/*************************************************************************/
/* libav* support                                                        */
/*************************************************************************/


pthread_mutex_t tc_libavcodec_mutex = PTHREAD_MUTEX_INITIALIZER;

/*************************************************************************/
/* GraphicsMagick support                                                */
/*************************************************************************/

pthread_mutex_t tc_magick_mutex = PTHREAD_MUTEX_INITIALIZER;
static int magick_usecount = 0;

int tc_magick_init(TCMagickContext *ctx, const char *format)
{
    int ret = TC_OK;
    pthread_mutex_lock(&magick_mutex);

    if (magick_usecount == 0) {
        InitializeMagick(""); /* FIXME */
    }
    magick_usecount++;
    pthread_mutex_unlock(&magick_mutex);

    GetExceptionInfo(&ctx->exception_info);
    ctx->image_info = CloneImageInfo(NULL);

    /* FIXME */

    return ret;
}

int tc_magick_fini(TCMagickContext *ctx)
{
    int ret = TC_OK;
    if (ctx->image != NULL) {
        DestroyImage(ctx->image);
        DestroyImageInfo(ctx->image_info);
    }
    DestroyExceptionInfo(&ctx->exception_info);

    pthread_mutex_lock(&magick_mutex);
    magick_usecount--;
    if (magick_usecount == 0) {
        DestroyMagick();
    }
    pthread_mutex_unlock(&magick_mutex);

    return ret;
}

/* FIXME */
int tc_magick_encode_RGBin(TCMagickContext *ctx,
                           int width, int height, const uint8_t *data)
{
    int ret = TC_OK;

    /* FIXME */
    ctx->image = ConstituteImage(width, height,
                                 "RGB", CharPixel, data,
                                 &ctx->exception_info);
    /* FIXME */
    return ret;
}

int tc_magick_encode_filein(TCMagickContext *ctx, const char *filename)
{
    strlcpy(ctx->image_info->filename, filename, MaxTextExtent);
    ctx->image = ReadImage(ctx->image_info, &ctx->exception_info);

    if (ctx->image == NULL) {
        return TC_ERROR;
    }
    return TC_OK;
}

int tc_magick_encode_frameout(TCMagickContext *ctx, TCFrameVideo *frame)
{
    return TC_ERROR;
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

