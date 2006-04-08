/*
 *  encoder.h
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *  Updated and partially rewritten by
 *  Francesco Romani - January 2006
 *
 *  This file is part of transcode, a video stream  processing tool
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

#ifndef _ENCODER_H
#define _ENCODER_H 

#include "transcode.h"
#include "framebuffer.h"

#include "tcmodule-core.h"
#include "encoder-common.h"

typedef struct tcencoderbuffer_ TCEncoderBuffer;
struct tcencoderbuffer_ {
    int frame_id; /* current frame identifier (both for A and V, yet) */

    vframe_list_t *vptr; /* current video frame */
    aframe_list_t *aptr; /* current audio frame */

    int (*acquire_video_frame)(TCEncoderBuffer *buf, vob_t *vob);
    int (*acquire_audio_frame)(TCEncoderBuffer *buf, vob_t *vob);
    void (*dispose_video_frame)(TCEncoderBuffer *buf);
    void (*dispose_audio_frame)(TCEncoderBuffer *buf);

    /* !0 if there is more frames avalaible, 0 otherwise */
    int (*have_data)(TCEncoderBuffer *buf);
};

int export_init(TCEncoderBuffer *buffer, TCFactory factory);
int export_setup(vob_t *vob,
		 const char *a_mod, const char *v_mod, const char *m_mod);
void export_shutdown(void);

int encoder_init(vob_t *vob);
void encoder_loop(vob_t *vob, int frame_first, int frame_last);
int encoder_stop(void);
int encoder_open(vob_t *vob);
int encoder_close(void);

#endif
