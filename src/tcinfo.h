/*
 * tcinfo.h - definitions of info_t and decode_t
 * Written by Andrew Church <achurch@achurch.org>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#ifndef TCINFO_H
#define TCINFO_H

#ifndef PROBE_H
# include "probe.h"  // for ProbeInfo
#endif

/*************************************************************************/

typedef struct _info_t {

    int fd_in;          // Input stream file descriptor
    int fd_out;         // Output stream file descriptor

    long magic;         // Specifies file magic for extract thread
    int track;          // Track to extract
    long stype;         // Specifies stream type for extract thread
    long codec;         // Specifies codec for extract thread
    int verbose;        // Verbosity

    int dvd_title;
    int dvd_chapter;
    int dvd_angle;

    int vob_offset;

    int ps_unit;
    int ps_seq1;
    int ps_seq2;

    int ts_pid;

    int seek_allowed;

    int demux;
    int select;         // Selected packet payload type
    int subid;          // Selected packet substream ID
    int keep_seq;       // Do not drop first sequence (cluster mode)

    double fps;

    int fd_log;

    const char *name;   // Source name as supplied with -i option
    const char *nav_seek_file; // Seek/index file

    int probe;          // Flag for probe only mode
    int factor;         // Amount of file to probe, in MB

    ProbeInfo *probe_info;

    int quality;
    int error;

    long frame_limit[3];
    int hard_fps_flag;  // If this is set, disable demuxer smooth drop

} info_t;

typedef struct {
    int fd_in;          // Input stream file descriptor
    int fd_out;         // Output stream file descriptor
    double ac3_gain[3];
    long frame_limit[3];
    int dv_yuy2_mode;
    int padrate;        // Zero padding rate
    long magic;         // Specifies file magic
    long stype;         // Specifies stream type
    long codec;         // Specifies codec
    int verbose;        // Verbosity
    int quality;
    const char *name;   // Source name as supplied with -i option
    int width;
    int height;
    int a52_mode;
    long format;        // Specifies raw stream format for output
    int select;         // Selected packet payload type
} decode_t;

/*************************************************************************/

#endif  // TCINFO_H

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
