/*
 * rawsource.h - (almost) raw source reader interface for encoder
 *               expect WAV audio and YUV4MPEG2 video
 * (C) 2006-2007 - Francesco Romani <fromani at gmail dot com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License version 2.  See the file COPYING for details.
 */

#ifndef _FILE_SOURCE_H
#define _FILE_SOURCE_H

#include "transcode.h"
#include "encoder.h"

extern TCEncoderBuffer *tc_rawsource_buffer;

int tc_rawsource_open(vob_t *vob);
int tc_rawsource_close(void);

#endif /* _FILE_SOURCE_H */
