/*
 *  decoder.h
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  This file is part of transcode, a linux video stream processing tool
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

void import_init(vob_t *vob, char *a_mod, char *v_mod);
void aimport_thread(vob_t *vob);
void vimport_thread(vob_t *vob);
void aimport_stop();
void vimport_stop();
void import_close();
void import_cancel();

void decoder_init(vob_t *vob);
int decoder_stop(int what);

void export_close();

int import_status();
int aimport_status();
int vimport_status();

extern int max_frame_buffer;
extern int max_frame_threads;

#endif
