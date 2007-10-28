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

#ifndef FRAME_THREADS_H
#define FRAME_THREADS_H

#include "transcode.h"

void tc_frame_threads_init(vob_t *vob, int vworkers, int aworkers);
void tc_frame_threads_close(void);

void tc_frame_threads_notify_audio(int broadcast);
void tc_frame_threads_notify_video(int broadcast);

int tc_frame_threads_have_video_workers(void);
int tc_frame_threads_have_audio_workers(void);

#endif /* FRAME_THREADS_H */
