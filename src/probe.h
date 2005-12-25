/*
 * probe.h - declarations for input file probing
 * Written by Andrew Church <achurch@achurch.org>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#ifndef PROBE_H
#define PROBE_H

/* External interface */
int probe_source(const char *vid_file, const char *aud_file, int range,
                 int flags, vob_t *vob);

/* Flags for probe_source(), indicating which parameters were specified by
 * the user and shouldn't be overwritten */
#define TC_PROBE_NO_FRAMESIZE   1
#define TC_PROBE_NO_FPS         2
#define TC_PROBE_NO_DEMUX       4
#define TC_PROBE_NO_RATE        8
#define TC_PROBE_NO_CHAN       16
#define TC_PROBE_NO_BITS       32
#define TC_PROBE_NO_SEEK       64
#define TC_PROBE_NO_TRACK     128
#define TC_PROBE_NO_BUFFER    256
//#define TC_PROBE_NO_FRC       512  // unused
#define TC_PROBE_NO_ACODEC   1024
#define TC_PROBE_NO_AVSHIFT  2048
#define TC_PROBE_NO_AV_FINE  4096
#define TC_PROBE_NO_IMASR    8192
#define TC_PROBE_NO_BUILTIN 16384  // external probe (mplayer)

/* Auxiliary info routines */
const char *codec2str(int flag);
const char *aformat2str(int flag);
const char *mformat2str(int flag);
const char *asr2str(int flag);

/* info_server.c */
void server_thread(vob_t *vob);

#endif  // PROBE_H

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
