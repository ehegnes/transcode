/*
 *  probe.h
 *
 *  Copyright (C) Thomas Östreich - June 2001
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

#ifndef _PROBE_H
#define _PROBE_H 

#include "import/magic.h"

void probe_source(int *flag, vob_t *vob, int range, char *vid, char *aud);
char *aformat2str(int f);
char *mformat2str(int f);
char *codec2str(int f);
char *asr2str(int c);
void server_thread(vob_t *vob);

static double frc_table[16] = {0,
			       NTSC_FILM, 24, 25, NTSC_VIDEO, 30, 50, 
			       (2*NTSC_VIDEO), 60,
			       1, 5, 10, 12, 15, 
			       0, 0};

#endif
