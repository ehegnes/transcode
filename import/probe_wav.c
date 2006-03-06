/*
 *  probe_wav.c
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

#include "transcode.h"
#include "tcinfo.h"
#include "ioaux.h"
#include "tc.h"
#include "wavlib.h"

void probe_wav(info_t *ipipe)
{
    WAVError err = 0;
    WAV wav = NULL;
    int chans = 0;

    wav = wav_fdopen(ipipe->fd_in, WAV_READ, &err);
    if (!wav) {
        fprintf(stderr, "(%s) %s\n", __FILE__, wav_strerror(err));
        ipipe->error = 1;
        return;
    }

    chans = wav_get_channels(wav);
    ipipe->probe_info->track[0].chan = chans;
    ipipe->probe_info->track[0].samplerate = wav_get_rate(wav);
    ipipe->probe_info->track[0].bits = wav_get_bits(wav);
    ipipe->probe_info->track[0].bitrate = wav_get_bitrate(wav);
    ipipe->probe_info->track[0].format = 0x1; /* XXX */

    ipipe->probe_info->magic = TC_MAGIC_WAV;
    ipipe->probe_info->codec = TC_CODEC_PCM;

    if (ipipe->probe_info->track[0].chan > 0) {
        ipipe->probe_info->num_tracks = 1;
    }
    return;
}
