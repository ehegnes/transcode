/*
 *  dvd_reader.h
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

#ifndef _DEV_READER_H
#define _DEV_READER_H

#include "transcode.h"

int dvd_init(char *dvd_path, int *arg_title, int verb);
int dvd_probe(int title, probe_info_t *info);
int dvd_query(int arg_title, int *arg_chapter, int *arg_angle);
int dvd_read(int arg_title, int arg_chapter, int arg_angle);
int dvd_stream(int arg_title,int arg_chapid);
int dvd_close(void);
int dvd_verify(char *path);

#endif
