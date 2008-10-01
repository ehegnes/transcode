/*
 * synchronizer.h -- transcode A/V synchronization code - interface
 * (C) 2008 - Francesco Romani <fromani at gmail dot com>
 *
 * This file is part of transcode, a video stream processing tool.
 *
 * transcode is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * transcode is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SYNCHRONIZER_H
#define SYNCHRONIZER_H


#include "transcode.h"
#include "libtc/tcframes.h"

typedef enum tcsyncmethodid_ TCSyncMethodID;
enum tcsyncmethodid_ {
    TC_SYNC_NULL = -1,
    TC_SYNC_NONE = 0,
    TC_SYNC_ADJUST_FRAMES,
};

typedef int (*TCFillFrameVideo)(void *ctx, TCFrameVideo *vf);
typedef int (*TCFillFrameAudio)(void *ctx, TCFrameAudio *af);

int tc_sync_init(vob_t *vob, TCSyncMethodID method, int master);
int tc_sync_fini(void);

int tc_sync_get_video_frame(TCFrameVideo *vf, TCFillFrameVideo filler, void *ctx);
int tc_sync_get_audio_frame(TCFrameAudio *af, TCFillFrameAudio filler, void *ctx);

#endif /* SYNCHRONIZER_H */
