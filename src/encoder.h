/*
 *  encoder.h
 *
 *  Copyright (C) Thomas Östreich - June 2001
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
#include "filter.h"

long tc_get_frames_dropped(void);
long tc_get_frames_skipped(void);
long tc_get_frames_encoded(void);
long tc_get_frames_cloned(void);
void tc_update_frames_dropped(long cc);
void tc_update_frames_skipped(long cc);
void tc_update_frames_encoded(long cc);
void tc_update_frames_cloned(long cc);

int export_init(vob_t *vob, char *a_mod, char *v_mod);
void export_shutdown(void);

int encoder_init(vob_t *vob);
void encoder(vob_t *vob, int frame_first, int frame_last);
int encoder_stop(void);
int encoder_open(vob_t *vob);
int encoder_close(void);

#endif
