/*
 *  encoder.h
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  This file is part of transcode, a linux video stream  processing tool
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

#include "filter.h"

#ifndef _ENCODER_H
#define _ENCODER_H 

void export_init(vob_t *vob_ptr, char *a_mod, char *v_mod);

int encoder_init(transfer_t *export_para, vob_t *vob);
void encoder(vob_t *vob_ptr, int frame_a, int frame_b);
int encoder_stop(transfer_t *export_para);
int encoder_open(transfer_t *export_para, vob_t *vob);
int encoder_close(transfer_t *export_para);

int export_status();

#endif
