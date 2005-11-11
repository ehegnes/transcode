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
typedef struct tcencoderdata_ TcEncoderData;
struct tcencoderdata_ {
	/* references to current frames */
	vframe_list_t *vptr;
	aframe_list_t *aptr;

	/* (video) frame identifier */
	int fid;

	/* flags */
	int exit_on_encoder_error;
	int fill_flag;

	/* frame boundaries */
	int frame_a;
	int frame_b;
	/* needed by encoder_skip */
	int last_frame_b;
	
	/* used for communications with modules */
	transfer_t export_para;
};

#define TC_ENCODER_DATA_INIT(data) \
	do { \
		(data)->vptr = NULL; \
		(data)->aptr = NULL; \
		(data)->fid = 0; \
		(data)->exit_on_encoder_error = 0; \
		(data)->fill_flag = 0; \
	} while(0)

int export_init(vob_t *vob_ptr, char *a_mod, char *v_mod);

#ifdef ENCODER_EXPORT
int encoder_wait_vframe(TcEncoderData *data);
int encoder_wait_aframe(TcEncoderData *data);
int encoder_acquire_vframe(TcEncoderData *data, vob_t *vob);
int encoder_acquire_aframe(TcEncoderData *data, vob_t *vob);

int encoder_export(TcEncoderData *data, vob_t *vob);
void encoder_skip(TcEncoderData *data);

void encoder_dispose_vframe(TcEncoderData *data);
void encoder_dispose_aframe(TcEncoderData *data);
#endif

int encoder_init(transfer_t *export_para, vob_t *vob);
void encoder(vob_t *vob_ptr, int frame_a, int frame_b);
int encoder_stop(transfer_t *export_para);
int encoder_open(transfer_t *export_para, vob_t *vob);
int encoder_close(transfer_t *export_para);

int export_status(void);
void export_shutdown(void);

#endif
