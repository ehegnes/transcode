/*
 * rawsource.c - (almost) raw source reader interface for encoder
 *               expect WAV audio and YUV4MPEG2 video
 * Copyright (C) Francesco Romani - February 2006
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "framebuffer.h"
#include "dl_loader.h"
#include "rawsource.h"
#include "libtc/libtc.h"
#include "wavlib.h"

#define RAWSOURCE_IM_MOD    "yuv4mpeg"

extern int errno;

static int rawsource_read_video(TCEncoderBuffer *buf, vob_t *vob);
static int rawsource_read_audio(TCEncoderBuffer *buf, vob_t *vob);
static int rawsource_have_data(TCEncoderBuffer *buf);
static void rawsource_dummy(TCEncoderBuffer *buf);


typedef struct tcrawsource_ {
    void *im_handle;
    transfer_t im_para;

    int eof_flag;
    int sources;

    vframe_list_t *vframe;
    aframe_list_t *aframe;
} TCFileSource;

static TCFileSource rawsource = {
    .im_handle = NULL,

    .eof_flag = TC_FALSE,
    .sources = 0,
    
    .vframe = NULL,
    .aframe = NULL,
};

static TCEncoderBuffer rawsource_buffer = {
    .frame_id = 0,
    .frame_num = 0,

    .vptr = NULL,
    .aptr = NULL,

    .acquire_video_frame = rawsource_read_video,
    .acquire_audio_frame = rawsource_read_audio,
    .dispose_video_frame = rawsource_dummy,
    .dispose_audio_frame = rawsource_dummy,

    .have_data = rawsource_have_data,
};

static int rawsource_read_video(TCEncoderBuffer *buf, vob_t *vob)
{
    int ret;

    if (!buf) {
        /* paranoia */
        return -1;
    }
    if (vob->im_v_size > rawsource.vframe->video_size) {
        /* paranoia */
        tc_log_warn(__FILE__, "video buffer too small"
                              " (this should'nt happen)");
        return -1;
    }
    if (buf->frame_id >= buf->frame_num) {
        /* readed all required frames */
        tc_log_info(__FILE__, "readed %i frames, exiting",
                    buf->frame_num);
        return -1;
    }
    
    rawsource.im_para.buffer  = rawsource.vframe->video_buf;
    rawsource.im_para.buffer2 = NULL;
    rawsource.im_para.size    = vob->im_v_size;
    rawsource.im_para.flag    = TC_VIDEO;

    ret = tcv_import(TC_IMPORT_DECODE, &rawsource.im_para, vob);
    if (ret != TC_IMPORT_OK) {
        /* read failed */
        rawsource.eof_flag = TC_TRUE;
        return -1;
    }
    rawsource.vframe->video_size = rawsource.im_para.size;
    rawsource.vframe->attributes = rawsource.im_para.attributes;

    rawsource_buffer.vptr = rawsource.vframe;
    rawsource_buffer.frame_id++;
    return 0;
}

static int rawsource_read_audio(TCEncoderBuffer *buf, vob_t *vob)
{
    int ret = 0;

    if (!buf) {
        /* paranoia */
        return -1;
    }
    if (vob->im_a_size > rawsource.aframe->audio_size) {
        /* paranoia */
        tc_log_warn(__FILE__, "audio buffer too small"
                              " (this should'nt happen)");
        return -1;
    }

    rawsource.im_para.buffer  = rawsource.aframe->audio_buf;
    rawsource.im_para.buffer2 = NULL;
    rawsource.im_para.size    = vob->im_a_size;
    rawsource.im_para.flag    = TC_AUDIO;

    ret = tca_import(TC_IMPORT_DECODE, &rawsource.im_para, vob);
    if (ret != TC_IMPORT_OK) {
        /* read failed */
        rawsource.eof_flag = TC_TRUE;
        return -1;
    }
    rawsource.aframe->audio_size = rawsource.im_para.size;
    rawsource.aframe->attributes = rawsource.im_para.attributes;

    rawsource_buffer.aptr = rawsource.aframe;
    return 0;
}

static int rawsource_have_data(TCEncoderBuffer *buf)
{
    if (!buf) {
        return -1;
    }
    if (rawsource.eof_flag == TC_TRUE) {
        return 0;
    }
    return 1;
}

static void rawsource_dummy(TCEncoderBuffer *buf)
{
    return;
}

TCEncoderBuffer *tc_rawsource_buffer(vob_t *vob, int frame_num)
{
    if (!vob) {
        return NULL;
    }

    rawsource.vframe = vframe_new(vob->im_v_width, vob->im_v_height);
    if (!rawsource.vframe) {
        tc_log_warn(__FILE__, "can't allocate video frame buffer");
        return NULL;
    }
    rawsource.aframe = aframe_new();
    if (!rawsource.aframe) {
        vframe_del(rawsource.vframe);
        tc_log_warn(__FILE__, "can't allocate audio frame buffer");
        return NULL;
    }
    
    rawsource_buffer.frame_num = frame_num;

    return &rawsource_buffer;
}

int tc_rawsource_open(vob_t *vob)
{
    int ret = 0;
    int num_sources = 0;

	rawsource.im_handle = load_module(RAWSOURCE_IM_MOD, TC_IMPORT|TC_AUDIO|TC_VIDEO);
	if (!rawsource.im_handle) {
        return -1;
    }

    /* hello, module! */
	memset(&rawsource.im_para, 0, sizeof(transfer_t));
	rawsource.im_para.flag = vob->verbose;
	tca_import(TC_IMPORT_NAME, &rawsource.im_para, NULL);

    memset(&rawsource.im_para, 0, sizeof(transfer_t));
	rawsource.im_para.flag = vob->verbose;
	tcv_import(TC_IMPORT_NAME, &rawsource.im_para, NULL);

    /* open sources */
	memset(&rawsource.im_para, 0, sizeof(transfer_t));
    rawsource.im_para.flag = TC_AUDIO;
    ret = tca_import(TC_IMPORT_OPEN, &rawsource.im_para, vob);
    if (TC_IMPORT_OK == ret) {
        num_sources++;
        rawsource.sources |= TC_AUDIO;
    }

	memset(&rawsource.im_para, 0, sizeof(transfer_t));
    rawsource.im_para.flag = TC_VIDEO;
    ret = tcv_import(TC_IMPORT_OPEN, &rawsource.im_para, vob);
    if (TC_IMPORT_OK == ret) {
        num_sources++;
        rawsource.sources |= TC_VIDEO;
    }

    return num_sources;
}

static void tc_rawsource_free(void)
{
    if (rawsource.vframe != NULL) {
        vframe_del(rawsource.vframe);
        rawsource.vframe = NULL;
    }
    if (rawsource.aframe != NULL) {
        aframe_del(rawsource.aframe);
        rawsource.aframe = NULL;
    }
}

/* errors not fatal, but notified */
int tc_rawsource_close(void)
{
    tc_rawsource_free();

    if (rawsource.im_handle != NULL) {
        int ret = 0;

	    memset(&rawsource.im_para, 0, sizeof(transfer_t));
        rawsource.im_para.flag = TC_VIDEO;
        ret = tcv_import(TC_IMPORT_CLOSE, &rawsource.im_para, NULL);
        if(ret != TC_IMPORT_OK) {
            tc_log_warn(__FILE__, "video import module error: CLOSE failed");
        } else {
            rawsource.sources &= ~TC_VIDEO;
        }

        memset(&rawsource.im_para, 0, sizeof(transfer_t));
        rawsource.im_para.flag = TC_AUDIO;
        ret = tca_import(TC_IMPORT_CLOSE, &rawsource.im_para, NULL);
        if(ret != TC_IMPORT_OK) {
            tc_log_warn(__FILE__, "audio import module error: CLOSE failed");
        } else {
            rawsource.sources &= ~TC_AUDIO;
        }

        if (!rawsource.sources) {
            unload_module(rawsource.im_handle);
            rawsource.im_handle = NULL;
        }
    }
    return 0;
}

