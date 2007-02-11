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

#ifndef _DECODER_H
#define _DECODER_H

// import API
int import_init(vob_t *vob, char *a_mod, char *v_mod);
void import_shutdown(void);

void aimport_stop(void);
void vimport_stop(void);

void aimport_start(void);
void vimport_start(void);

int import_open(vob_t *vob);
int import_close(void);

void import_threads_create(vob_t *vob);
void import_threads_cancel(void);

int import_status(void);

int aimport_status(void);
int vimport_status(void);


#endif
