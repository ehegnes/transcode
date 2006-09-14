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

/*************************************************************************/

/* Structures to hold probed data */

typedef struct {
    int samplerate;
    int chan;
    int bits;
    int bitrate;
    int padrate;        // Padding byterate
    int format;
    int lang;
    int attribute;      // 0=subtitle, 1=AC3, 2=PCM
    int tid;            // Logical track id, in case of gaps
    double pts_start;
} ProbeTrackInfo;


typedef struct {

    int width;          // Frame width
    int height;         // Frame height

    double fps;         // Encoder fps

    long codec;         // Video codec
    long magic;         // File type/magic
    long magic_xml;     // Type/magic of content in XML file

    int asr;            // Aspect ratio code
    int frc;            // Frame cate code

    int par_width;      // Pixel aspect (== sample aspect ratio)
    int par_height;

    int attributes;     // Video attributes

    int num_tracks;     // Number of audio tracks

    ProbeTrackInfo track[TC_MAX_AUD_TRACKS];

    long frames;        // Total frames
    long time;          // Total time in secs

    int unit_cnt;       // Detected presentation units
    double pts_start;   // Video PTS start

    long bitrate;       // Video stream bitrate

    int ext_attributes[4]; // Reserved for MPEG

    int is_video;       // NTSC flag

} ProbeInfo;

/*************************************************************************/

/* External interface */
int probe_source(const char *vid_file, const char *aud_file, int range,
                 int flags, vob_t *vob);
int probe_source_xml(vob_t *vob, int which);

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

/* `which' value for probe_xml() */
#define PROBE_XML_VIDEO  0
#define PROBE_XML_AUDIO  1

/* Auxiliary info routines */
const char *codec2str(int flag);
const char *aformat2str(int flag);
const char *mformat2str(int flag);
const char *asr2str(int flag);

/* info_server.c */
void server_thread(vob_t *vob);

/*************************************************************************/

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
