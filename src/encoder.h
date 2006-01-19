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

/* 
 * this structure will hold *private data* needed by encoder
 * there is no need to export it to client code
 * fromani -- 20051111
 */
typedef struct tcencoderdata_ TCEncoderData;
struct tcencoderdata_ {
	/* references to current frames */
	vframe_list_t *vptr;
	aframe_list_t *aptr;

	/* (video) frame identifier */
	int fid;

	/* flags */
	int error_flag;
	int fill_flag;

	/* frame boundaries */
	int frame_first;
	int frame_last;
	/* needed by encoder_skip */
	int saved_frame_last;
};

#define TC_ENCODER_DATA_INIT(data) \
	do { \
		(data)->vptr = NULL; \
		(data)->aptr = NULL; \
		(data)->fid = 0; \
		(data)->error_flag = 0; \
		(data)->fill_flag = 0; \
	} while(0)

long tc_get_frames_dropped(void);
long tc_get_frames_skipped(void);
long tc_get_frames_encoded(void);
long tc_get_frames_cloned(void);
void tc_update_frames_dropped(long cc);
void tc_update_frames_skipped(long cc);
void tc_update_frames_encoded(long cc);
void tc_update_frames_cloned(long cc);
void tc_set_force_exit(void);
int tc_get_force_exit(void);

int export_init(vob_t *vob_ptr, char *a_mod, char *v_mod);

#ifdef ENCODER_EXPORT
int encoder_acquire_vframe(TCEncoderData *data, vob_t *vob);
int encoder_acquire_aframe(TCEncoderData *data, vob_t *vob);

int encoder_export(TCEncoderData *data, vob_t *vob);
void encoder_skip(TCEncoderData *data);

void encoder_dispose_vframe(TCEncoderData *data);
void encoder_dispose_aframe(TCEncoderData *data);
#endif

int encoder_init(vob_t *vob);
void encoder(vob_t *vob, int frame_first, int frame_last);
int encoder_stop(void);
int encoder_open(vob_t *vob);
int encoder_close(void);

void export_shutdown(void);

#endif
