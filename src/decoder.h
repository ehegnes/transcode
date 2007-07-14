/*
 *  decoder.h
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

#ifndef DECODER_H
#define DECODER_H

int tc_import_init(vob_t *vob, const char *a_mod, const char *v_mod);
void tc_import_shutdown(void);

void tc_import_audio_stop(void);
void tc_import_video_stop(void);

int tc_import_open(vob_t *vob);
int tc_import_close(void);

void tc_seq_import_threads_create(vob_t *vob);
void tc_seq_import_threads_cancel(void);

void tc_import_threads_create(vob_t *vob);
void tc_import_threads_cancel(void);

int tc_import_status(void);

int tc_import_audio_status(void);
int tc_import_video_status(void);

void tc_import_audio_notify(void);
void tc_import_video_notify(void);

void tc_import_stop_nolock(void);

#endif /* DECODER_H */
