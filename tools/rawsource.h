/*
 * rawsource.h - (almost) raw source reader interface for encoder
 *               expect WAV audio and YUV4MPEG2 video
 * Copyright (C) Francesco Romani - February 2006
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#ifndef _FILE_SOURCE_H
#define _FILE_SOURCE_H

#include "transcode.h"
#include "encoder.h"

TCEncoderBuffer *tc_rawsource_buffer(vob_t *vob);

int tc_rawsource_open(vob_t *vob);
int tc_rawsource_close(void);

#endif /* _FILE_SOURCE_H */
