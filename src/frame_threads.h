/*
 *  frame_threads.h
 *
 *  Copyright (C) Thomas Oestreich - June 2001
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

#include <pthread.h>

#ifndef _FRAME_THREADS_H
#define _FRAME_THREADS_H

void frame_threads_init(vob_t *vob, int vworkers, int aworkers);
void process_vframe(vob_t *vob);
void process_aframe(vob_t *vob);
void frame_threads_close(void);
void frame_threads_notify_audio(int broadcast);
void frame_threads_notify_video(int broadcast);

int frame_threads_have_video_workers(void);
int frame_threads_have_audio_workers(void);

extern pthread_mutex_t vbuffer_im_fill_lock;
extern uint32_t vbuffer_im_fill_ctr;

extern pthread_mutex_t vbuffer_xx_fill_lock;
extern uint32_t vbuffer_xx_fill_ctr;

extern pthread_mutex_t vbuffer_ex_fill_lock;
extern uint32_t vbuffer_ex_fill_ctr;

extern pthread_mutex_t abuffer_im_fill_lock;
extern uint32_t abuffer_im_fill_ctr;

extern pthread_mutex_t abuffer_xx_fill_lock;
extern uint32_t abuffer_xx_fill_ctr;

extern pthread_mutex_t abuffer_ex_fill_lock;
extern uint32_t abuffer_ex_fill_ctr;

/*
 * tc_flush_{audio,video}_counters:
 *      reset to zero frame counters that accounts
 *      amount of frames in various stage of processing.
 *      Threas safe
 *
 * Parameters:
 *      None
 * Return Value:
 *      None
 * Side effects:
 *      Use internal locking, so it's thread safe/
 */
void tc_flush_audio_counters(void);
void tc_flush_video_counters(void);

#endif
