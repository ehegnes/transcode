/*
 *  probe_export.h 
 *
 *  Copyright (C) Tilmann Bitterberg - October 2003
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

#ifndef PROBE_EXPORT_H
#define PROBE_EXPORT_H

// add flags here
// possible flags are bitrate, etc
// if flag is set, use the extensions provided by the user.
// otherwise use the ones the export modules suggests.

#define TC_PROBE_NO_EXPORT_VEXT   1
#define TC_PROBE_NO_EXPORT_AEXT   2

extern unsigned int probe_export_attributes;

extern char *audio_ext;
extern char *video_ext;


#endif
