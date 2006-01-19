/*
 *  transcode.h
 *
 *  Copyright (C) Thomas Oestreich - June 2001
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

#ifndef _TRANSCODE_H
#define _TRANSCODE_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_ERRNO_H
# include <errno.h>
#else
extern int errno;
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef NET_STREAM
# ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
# endif
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
#endif

#include "avilib/avilib.h"
#include "aclib/ac.h"
#include "libioaux/framecode.h"
#include "libtcvideo/tcvideo.h"


#ifdef __bsdi__
typedef unsigned int uint32_t;
#endif

#include "tc_defaults.h"
#include "libtc/libtc.h"

/*************************************************************************/

/* ----------------------------
 *
 * MPEG profiles for setting
 * sensible defaults
 *
 * ----------------------------*/

typedef enum {
    PROF_NONE = 0,
    VCD,
    VCD_PAL,
    VCD_NTSC,
    SVCD,
    SVCD_PAL,
    SVCD_NTSC,
    XVCD,
    XVCD_PAL,
    XVCD_NTSC,
    DVD,
    DVD_PAL,
    DVD_NTSC
} mpeg_profile_t;


/* ----------------------------
 *
 * Global information structure
 *
 * ----------------------------*/

typedef struct _transfer_t {
    int flag;
    FILE *fd;
    int size;
    uint8_t *buffer;
    uint8_t *buffer2;
    int attributes;
} transfer_t;

typedef struct _vob_t {

    // import info

    const char *vmod_probed;
    const char *amod_probed;
    const char *vmod_probed_xml; // Modules for reading XML data
    const char *amod_probed_xml;

    int verbose;

    const char *video_in_file;  // Video source file
    const char *audio_in_file;  // Audio source file

    const char *nav_seek_file;  // Seek/index information

    int in_flag;

    int has_audio;              // Does the stream have audio?
    int has_audio_track;        // Does the requested audio track exist?
    int has_video;              // Does the stream have video?

    int lang_code;              // Language of audio track

    int a_track;                // Audio track ID
    int v_track;                // Video track ID
    int s_track;                // Subtitle track ID

    int sync;                   // Frame offset for audio/video synchronization
    int sync_ms;                // Fine-tuning for audio/video synchronization

    int dvd_title;
    int dvd_chapter1;
    int dvd_chapter2;
    int dvd_max_chapters;
    int dvd_angle;

    int ps_unit;
    int ps_seq1;
    int ps_seq2;

    int ts_pid1;
    int ts_pid2;

    int vob_offset;
    int vob_chunk;
    int vob_chunk_num1;
    int vob_chunk_num2;
    int vob_chunk_max;
    int vob_percentage;

    int vob_psu_num1;
    int vob_psu_num2;

    const char *vob_info_file;

    double pts_start;

    double psu_offset;          // PSU offset to pass to extsub

    int demuxer;

    long v_format_flag;         // Video stream format
    long v_codec_flag;          // Video codec
    long a_format_flag;         // Audio stream format
    long a_codec_flag;          // Audio codec

    int quality;

    // Audio stream parameters

    int a_stream_bitrate;       // Source stream bitrate

    int a_chan;
    int a_bits;
    int a_rate;

    int a_padrate;              // Zero padding rate

    int im_a_size;              // Import total bytes per audio frame
    int ex_a_size;              // Export total bytes per audio frame

    int im_a_codec;             // True frame buffer audio codec

    int a_leap_frame;
    int a_leap_bytes;

    int a_vbr;                  // LAME VBR switch

    int a52_mode;

    int dm_bits;
    int dm_chan;

    // Video stream parameters

    int v_stream_bitrate;       // Source stream bitrate

    double fps;                 // Import frame rate (default 25 fps)
    int im_frc;                 // Import frame rate code
    double ex_fps;              // Export frame rate (default 25 fps)
    int ex_frc;                 // Export frame rate code
    int hard_fps_flag;          // If this is set, disable demuxer smooth drop

    int pulldown;               // Set 3:2 pulldown flags on MPEG export

    int im_v_height;            // Import picture height
    int im_v_width;             // Import picture width
    int im_v_size;              // Total number of bytes per frame

    int im_asr;                 // Import aspect ratio code
    int im_par;                 // Import pixel aspect (code)
    int im_par_width;           // Import pixel aspect width
    int im_par_height;          // Import pixel aspect height
    int ex_asr;                 // Export aspect ratio code
    int ex_par;                 // Export pixel aspect (code)
    int ex_par_width;           // Export pixel aspect width
    int ex_par_height;          // Export pixel aspect height

    int attributes;             // More video frame attributes

    int im_v_codec;             // True frame buffer video codec

    int encode_fields;          // Interlaced field handling flag

    int dv_yuy2_mode;           // Decode DV video in YUY2 mode?

    // Audio frame manipulation info

    double volume;              // Audio amplitude rescale parameter
    double ac3_gain[3];         // Audio amplitude rescale parameter for ac3
    int clip_count;             // # of bytes clipped after volume adjustment

    // Video frame manipulation info

    int ex_v_width;             // Export picture width
    int ex_v_height;            // Export picture height
    int ex_v_size;              // Total number of bytes per frame

    int reduce_h;               // Reduction factor for frame height
    int reduce_w;               // Reduction factor for frame width

    int resize1_mult;           // Multiplier for {vert,hori}_resize1
    int vert_resize1;           // Height resize amount (shrink)
    int hori_resize1;           // Width resize amount (shrink)

    int resize2_mult;           // Multiplier for {vert,hori}_resize2
    int vert_resize2;           // Height resize amount (expand)
    int hori_resize2;           // Width resize amount (expand)

    int zoom_width;             // Zoom width
    int zoom_height;            // Zoom height

    TCVZoomFilter zoom_filter;

    int antialias;
    int deinterlace;
    int decolor;

    double aa_weight;           // Antialiasing center pixel weight
    double aa_bias;             // Antialiasing horizontal/vertical bias

    double gamma;

    int ex_clip_top;
    int ex_clip_bottom;
    int ex_clip_left;
    int ex_clip_right;

    int im_clip_top;
    int im_clip_bottom;
    int im_clip_left;
    int im_clip_right;

    int post_ex_clip_top;
    int post_ex_clip_bottom;
    int post_ex_clip_left;
    int post_ex_clip_right;

    int pre_im_clip_top;
    int pre_im_clip_bottom;
    int pre_im_clip_left;
    int pre_im_clip_right;

    // Export info

    const char *video_out_file;
    const char *audio_out_file;

    avi_t *avifile_in;
    avi_t *avifile_out;
    int avi_comment_fd;         // Text file to read AVI header comments from

    int out_flag;

    // Encoding parameters

    int divxbitrate;
    int divxkeyframes;
    int divxquality;
    int divxcrispness;
    int divxmultipass;
    int video_max_bitrate;
    const char *divxlogfile;

    int min_quantizer;
    int max_quantizer;

    int rc_period;
    int rc_reaction_period;
    int rc_reaction_ratio;

    int divx5_vbv_prof;         // Profile number
    int divx5_vbv_bitrate;      // Video Bitrate Verifier constraint overrides
    int divx5_vbv_size;
    int divx5_vbv_occupancy;

    int mp3bitrate;
    int mp3frequency;
    float mp3quality;           // 0=best (very slow), 9=worst (default=5)
    int mp3mode;                // 0=joint-stereo, 1=full-stereo, 2=mono

    int bitreservoir;
    const char *lame_preset;

    const char *audiologfile;

    int ex_a_codec;             // Audio codec for export module
    int ex_v_codec;             // Video codec for export module

    const char *ex_v_fcc;       // Video fourcc string
    const char *ex_a_fcc;       // Audio fourcc string/identifier
    const char *ex_profile_name; // User profile name

    int pass_flag;
    int lame_flush;

    const char *mod_path;

    struct fc_time *ttime;      // For framecode parsing (list of structs)

    unsigned int frame_interval; // Select every `frame_interval' frames only

    int chanid;
    const char *station_id;

    char *im_v_string;          // Extra options for import video module
    char *im_a_string;          // Extra options for import audio module
    char *ex_v_string;          // Extra options for export video module
    char *ex_a_string;          // Extra options for export audio module

    float m2v_requant;          // Requantize factor for mpeg2 video streams

    mpeg_profile_t mpeg_profile;

} vob_t;


typedef struct subtitle_header_s {

    unsigned int header_length;
    unsigned int header_version;
    unsigned int payload_length;

    unsigned int lpts;
    double rpts;

    unsigned int discont_ctr;

} subtitle_header_t;

/*************************************************************************/

// Module functions

int tc_import(int opt, void *para1, void *para2);
int tc_export(int opt, void *para1, void *para2);

// Some functions exported by transcode

void tc_import_stop_nolock(void);
void tc_export_stop_nolock(void);
vob_t *tc_get_vob(void);

void tc_outstream_rotate(void);
void tc_outstream_rotate_request(void);
void tc_pause(void);
void tc_pause_request(void);

void tc_adjust_frame_buffer(int height, int width);

extern int verbose;
extern int pcmswap;
extern int rescale;
extern int im_clip;
extern int ex_clip;
extern int pre_im_clip;
extern int post_ex_clip;
extern int flip;
extern int mirror;
extern int rgbswap;
extern int resize1;
extern int resize2;
extern int decolor;
extern int zoom;
extern int dgamma;

// Core parameters

extern int tc_buffer_delay_dec;
extern int tc_buffer_delay_enc;
extern int tc_encode_stream;
extern int tc_decode_stream;
extern int tc_cluster_mode;
extern int tc_decoder_delay;
extern int tc_x_preview;
extern int tc_y_preview;
extern int tc_progress_meter;
extern int tc_progress_rate;
extern pthread_t tc_pthread_main;
extern int tc_accel;
extern unsigned int tc_avi_limit;
extern int tc_frame_width_max;
extern int tc_frame_height_max;
extern pid_t tc_probe_pid;

// Various constants

#define TC_EXPORT_NAME     10
#define TC_EXPORT_OPEN     11
#define TC_EXPORT_INIT     12
#define TC_EXPORT_ENCODE   13
#define TC_EXPORT_CLOSE    14
#define TC_EXPORT_STOP     15

#define TC_EXPORT_ERROR    -1
#define TC_EXPORT_UNKNOWN   1
#define TC_EXPORT_OK        0

#define TC_IMPORT_NAME     20
#define TC_IMPORT_OPEN     21
#define TC_IMPORT_DECODE   22
#define TC_IMPORT_CLOSE    23

#define TC_IMPORT_UNKNOWN   1
#define TC_IMPORT_ERROR    -1
#define TC_IMPORT_OK        0

#define TC_CAP_PCM          1
#define TC_CAP_RGB          2
#define TC_CAP_AC3          4
#define TC_CAP_YUV          8
#define TC_CAP_AUD         16
#define TC_CAP_VID         32
#define TC_CAP_MP3         64
#define TC_CAP_YUY2       128
#define TC_CAP_DV         256
#define TC_CAP_YUV422     512

#define TC_MODE_DEFAULT            0
#define TC_MODE_AVI_SPLIT          1
#define TC_MODE_DVD_CHAPTER        2
#define TC_MODE_PSU                4
#define TC_MODE_DIRECTORY         16
#define TC_MODE_DEBUG             32

/*************************************************************************/

#endif  // _TRANSCODE_H

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
