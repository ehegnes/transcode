/*
 *  import_ffmpeg.c
 *
 *  Copyright (C) Moritz Bunkus - October 2002
 *
 *  This file is part of transcode, a video stream processing tool
 *
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

#define MOD_NAME    "import_ffmpeg.so"
#define MOD_VERSION "v0.2.0 (2006-09-15)"
#define MOD_CODEC   "(video) libavformat/libavcodec"

#include "transcode.h"
#include "libtc/libtc.h"
#include "filter.h"
#include "libtc/tcframes.h"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_YUV | TC_CAP_RGB;

#define MOD_PRE ffmpeg
#include "import_def.h"

#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>

#include "aclib/imgconvert.h"

/*
 * libavcodec is not thread-safe. We must protect concurrent access to it.
 * this is visible (without the mutex of course) with
 * transcode .. -x ffmpeg -y ffmpeg -F mpeg4
 */


typedef void (*AdaptImageFn)(uint8_t *src_planes[3], uint8_t *frame,
                             AVCodecContext *lavc_dec_context, AVFrame *picture);

static AdaptImageFn        img_adaptor = NULL;
static int                 levels_handle = -1;
static AVFormatContext    *lavf_dmx_context = NULL;
static AVCodecContext     *lavc_dec_context = NULL;
static AVCodec            *lavc_dec_codec = NULL;
static int                 src_fmt;
static int                 dst_fmt;
static int                 streamid = -1;
static size_t              frame_size = 0;
static uint8_t            *frame = NULL;


static inline void enable_levels_filter(void)
{
    tc_log_info(MOD_NAME, "input is mjpeg, reducing range from YUVJ420P to YUV420P");
    levels_handle = tc_filter_add("levels", "output=16-240:pre=1");
    if (!levels_handle) {
        tc_log_warn(MOD_NAME, "cannot load levels filter");
    }
}

#define ALLOC_MEM_AREA(PTR, SIZE) do { \
    if ((PTR) == NULL) { \
        (PTR) = tc_bufalloc((SIZE)); \
    } \
    if ((PTR) == NULL) { \
        tc_log_perror(MOD_NAME, "out of memory"); \
        return TC_IMPORT_ERROR; \
    } \
} while (0)

/*************************************************************************/
/*
 * Image adaptors helper routines
 */

/*
 * yeah following adaptors can be further factored out.
 * Can't do this now, unfortunately. -- fromani
 */

static void adapt_image_yuv420p(uint8_t *src_planes[3], uint8_t *frame,
                                AVCodecContext *lavc_dec_context, AVFrame *picture)
{
    // Remove "dead space" at right edge of planes, if any
    if (picture->linesize[0] != lavc_dec_context->width) {
        int y;
        for (y = 0; y < lavc_dec_context->height; y++) {
            ac_memcpy(src_planes[0] + y*lavc_dec_context->width,
                      picture->data[0] + y*picture->linesize[0],
                      lavc_dec_context->width);
        }
        for (y = 0; y < lavc_dec_context->height / 2; y++) {
            ac_memcpy(src_planes[1] + y*(lavc_dec_context->width/2),
                      picture->data[1] + y*picture->linesize[1],
                	  lavc_dec_context->width/2);
            ac_memcpy(src_planes[2] + y*(lavc_dec_context->width/2),
			          picture->data[2] + y*picture->linesize[2],
                      lavc_dec_context->width/2);
        }
    } else {
        ac_memcpy(src_planes[0], picture->data[0],
                  lavc_dec_context->width * lavc_dec_context->height);
        ac_memcpy(src_planes[1], picture->data[1],
                  (lavc_dec_context->width/2)*(lavc_dec_context->height/2));
        ac_memcpy(src_planes[2], picture->data[2],
		          (lavc_dec_context->width/2)*(lavc_dec_context->height/2));
    }
} 


static void adapt_image_yuv411p(uint8_t *src_planes[3], uint8_t *frame,
                                AVCodecContext *lavc_dec_context, AVFrame *picture)
{
    if (picture->linesize[0] != lavc_dec_context->width) {
        int y;
        for (y = 0; y < lavc_dec_context->height; y++) {
            ac_memcpy(src_planes[0] + y*lavc_dec_context->width,
			          picture->data[0] + y*picture->linesize[0],
                      lavc_dec_context->width);
            ac_memcpy(src_planes[1] + y*(lavc_dec_context->width/4),
	    	          picture->data[1] + y*picture->linesize[1],
                      lavc_dec_context->width/4);
            ac_memcpy(src_planes[2] + y*(lavc_dec_context->width/4),
			          picture->data[2] + y*picture->linesize[2],
                      lavc_dec_context->width/4);
        }
    } else {
        ac_memcpy(src_planes[0], picture->data[0],
                  lavc_dec_context->width * lavc_dec_context->height);
        ac_memcpy(src_planes[1], picture->data[1],
		          (lavc_dec_context->width/4) * lavc_dec_context->height);
        ac_memcpy(src_planes[2], picture->data[2],
		          (lavc_dec_context->width/4) * lavc_dec_context->height);
    }
}


static void adapt_image_yuv422p(uint8_t *src_planes[3], uint8_t *frame,
                                AVCodecContext *lavc_dec_context, AVFrame *picture)
{
    if (picture->linesize[0] != lavc_dec_context->width) {
        int y;
        for (y = 0; y < lavc_dec_context->height; y++) {
            ac_memcpy(src_planes[0] + y*lavc_dec_context->width,
			          picture->data[0] + y*picture->linesize[0],
                      lavc_dec_context->width);
            ac_memcpy(src_planes[1] + y*(lavc_dec_context->width/2),
	    	          picture->data[1] + y*picture->linesize[1],
                      lavc_dec_context->width/2);
            ac_memcpy(src_planes[2] + y*(lavc_dec_context->width/2),
			          picture->data[2] + y*picture->linesize[2],
                      lavc_dec_context->width/2);
        }
    } else {
        ac_memcpy(src_planes[0], picture->data[0],
                  lavc_dec_context->width * lavc_dec_context->height);
        ac_memcpy(src_planes[1], picture->data[1],
		         (lavc_dec_context->width/2) * lavc_dec_context->height);
        ac_memcpy(src_planes[2], picture->data[2],
		          (lavc_dec_context->width/2) * lavc_dec_context->height);
    }
}


static void adapt_image_yuv444p(uint8_t *src_planes[3], uint8_t *frame,
                                AVCodecContext *lavc_dec_context, AVFrame *picture)
{
    if (picture->linesize[0] != lavc_dec_context->width) {
	    int y;
        for (y = 0; y < lavc_dec_context->height; y++) {
            ac_memcpy(picture->data[0] + y*lavc_dec_context->width,
		              picture->data[0] + y*picture->linesize[0],
                      lavc_dec_context->width);
            ac_memcpy(picture->data[1] + y*lavc_dec_context->width,
	    	          picture->data[1] + y*picture->linesize[1],
                      lavc_dec_context->width);
            ac_memcpy(picture->data[2] + y*lavc_dec_context->width,
			          picture->data[2] + y*picture->linesize[2],
                      lavc_dec_context->width);
        }
    } else {
        ac_memcpy(src_planes[0], picture->data[0],
		          lavc_dec_context->width * lavc_dec_context->height);
        ac_memcpy(src_planes[1], picture->data[1],
		          lavc_dec_context->width * lavc_dec_context->height);
        ac_memcpy(src_planes[2], picture->data[2],
	              lavc_dec_context->width * lavc_dec_context->height);
    }
}



MOD_open 
{
    int ret, i = 0;

    if (param->flag == TC_VIDEO) {
        TC_LOCK_LIBAVCODEC;
        av_register_all();
        avcodec_init();
        avcodec_register_all();

        ret = av_open_input_file(&lavf_dmx_context, vob->video_in_file,
                                 NULL, 0, NULL);
        TC_UNLOCK_LIBAVCODEC;

        if (ret != 0) {
            tc_log_error(MOD_NAME, "unable to open '%s'"
                                   " (libavformat failure)",
                         vob->video_in_file);
            return TC_IMPORT_ERROR;
        }

        ret = av_find_stream_info(lavf_dmx_context);
        if (ret < 0) {
            tc_log_error(MOD_NAME, "unable to fetch informations from '%s'"
                                   " (libavformat failure)",
                         vob->video_in_file);
            return TC_IMPORT_ERROR;
        }

        if (verbose >= TC_STATS) {
            dump_format(lavf_dmx_context, 0, vob->video_in_file, 0);
        }

        for (i = 0; i < lavf_dmx_context->nb_streams; i++) {
            if (lavf_dmx_context->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO) {
                streamid = i;
                break;
            }
        }
        if (streamid != -1) {
            if (verbose >= TC_DEBUG) {
                tc_log_info(MOD_NAME, "using stream #%i for video",
                            streamid);
            }
        } else {
            tc_log_error(MOD_NAME, "video stream not found in '%s'",
                         vob->video_in_file);
            return TC_IMPORT_ERROR;
        }
 
        lavc_dec_context = lavf_dmx_context->streams[streamid]->codec;

        if (lavc_dec_context->width != vob->im_v_width
         || lavc_dec_context->height != vob->im_v_height) {
            tc_log_error(MOD_NAME, "frame dimension mismatch:"
                                   " probing=%ix%i, opening=%ix%i",
                         vob->im_v_width, vob->im_v_height,
                         lavc_dec_context->width, lavc_dec_context->height);
            return TC_IMPORT_ERROR;
        }

        lavc_dec_codec = avcodec_find_decoder(lavc_dec_context->codec_id);
        if (lavc_dec_codec == NULL) {
            tc_log_warn(MOD_NAME, "No codec found for the ID '0x%X'.",
                        lavc_dec_context->codec_id);
            return TC_IMPORT_ERROR;
        }

        if (lavc_dec_codec->capabilities & CODEC_CAP_TRUNCATED) {
            lavc_dec_context->flags |= CODEC_FLAG_TRUNCATED;
        }
        if (vob->decolor) {
            lavc_dec_context->flags |= CODEC_FLAG_GRAY;
        }
        lavc_dec_context->error_resilience = 2;
        lavc_dec_context->error_concealment = 3;
        lavc_dec_context->workaround_bugs = FF_BUG_AUTODETECT;

        TC_LOCK_LIBAVCODEC;
        ret = avcodec_open(lavc_dec_context, lavc_dec_codec);
        TC_UNLOCK_LIBAVCODEC;
        if (ret < 0) {
            tc_log_error(MOD_NAME, "Could not initialize the '%s' codec.",
                         lavc_dec_codec->name);
            return TC_IMPORT_ERROR;
        }

        frame_size = tc_video_frame_size(vob->im_v_width, vob->im_v_height,
                                         vob->im_v_codec);

        ALLOC_MEM_AREA(frame, frame_size);

        /* translate src_fmt */
        dst_fmt = (vob->im_v_codec == CODEC_YUV) ?IMG_YUV_DEFAULT :IMG_RGB_DEFAULT;
        switch (lavc_dec_context->pix_fmt) {
          case PIX_FMT_YUVJ420P:
          case PIX_FMT_YUV420P:
            src_fmt = IMG_YUV420P;
            img_adaptor = adapt_image_yuv420p;
            break;

          case PIX_FMT_YUV411P:
            src_fmt = IMG_YUV411P;
            img_adaptor = adapt_image_yuv411p;
            break;

          case PIX_FMT_YUVJ422P:
          case PIX_FMT_YUV422P:
            src_fmt = IMG_YUV422P;
            img_adaptor = adapt_image_yuv422p;
            break;

          case PIX_FMT_YUVJ444P:
          case PIX_FMT_YUV444P:
            src_fmt = IMG_YUV444P;
            img_adaptor = adapt_image_yuv444p;
            break;

          default:
        	tc_log_error(MOD_NAME, "Unsupported decoded frame format: %d",
		                 lavc_dec_context->pix_fmt);
            return TC_IMPORT_ERROR;
        }

        param->fd = NULL;
        return TC_IMPORT_OK;
    }

    return TC_IMPORT_ERROR;
}

#undef ALLOC_MEM_AREA



MOD_decode 
{
    int got_picture, ret, status;
    uint8_t *src_planes[3];
    uint8_t *dst_planes[3];
    AVPacket packet;
    AVFrame picture;

    if (param->flag == TC_VIDEO) {
        av_init_packet(&packet);

        /* XXX */
        while ((ret = av_read_frame(lavf_dmx_context, &packet)) >= 0) {
            if (packet.stream_index != streamid) {
                continue;
            }

            TC_LOCK_LIBAVCODEC;
            status = avcodec_decode_video(lavc_dec_context, &picture,
                                          &got_picture,
                                          packet.data, packet.size);
            TC_UNLOCK_LIBAVCODEC;
            /* TODO: check status code */

            if (got_picture) {
                YUV_INIT_PLANES(src_planes, frame, src_fmt,
                                lavc_dec_context->width,
                                lavc_dec_context->height);
                YUV_INIT_PLANES(dst_planes, param->buffer, dst_fmt,
                                lavc_dec_context->width,
                                lavc_dec_context->height);

                img_adaptor(src_planes, frame,
                            lavc_dec_context, &picture);

                ac_imgconvert(src_planes, src_fmt, dst_planes, dst_fmt,
                              lavc_dec_context->width,
                              lavc_dec_context->height);
                param->size = frame_size;
                return TC_IMPORT_OK;
            } else {
                /* unlikely */
                if (verbose >= TC_STATS) {
                    tc_log_info(MOD_NAME, "need more data after succesfull"
                                          " av_read_frame");
                }
            }
        }
        if (verbose >= TC_DEBUG) {
            tc_log_info(MOD_NAME, "reading frame failed (return value=%i)",
                                  ret);
        }
        return TC_IMPORT_ERROR;
    }
    return TC_IMPORT_ERROR;
}



MOD_close 
{
    if (param->flag == TC_VIDEO) {
        if (lavc_dec_context != NULL) {
            avcodec_flush_buffers(lavc_dec_context);

            avcodec_close(lavc_dec_context);
            lavc_dec_context = NULL;
        }

        if (lavf_dmx_context != NULL) {
            av_close_input_file(lavf_dmx_context);
            lavf_dmx_context = NULL;
        }

        return TC_IMPORT_OK;
    }

    return TC_IMPORT_ERROR;
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
