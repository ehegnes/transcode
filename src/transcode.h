/*
 *  transcode.h
 *
 *  Copyright (C) Thomas Östreich - June 2001
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

#ifndef _TRANSCODE_H
#define _TRANSCODE_H

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include "../avilib/avilib.h"

#include "../libioaux/framecode.h"

#ifdef NET_STREAM
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#ifdef __bsdi__
typedef unsigned int uint32_t;
#endif

#include "tc_defaults.h"
#include "tc_functions.h"

/* ----------------------------
 *
 * global information structure
 *
 * ----------------------------*/

typedef struct _transfer_t {

  int   flag;
  FILE   *fd;
  
  int   size;

  char *buffer;
  char *buffer2;

  int attributes;

} transfer_t;

typedef struct _vob_t {
  
  // import info

  char *vmod_probed;
  char *amod_probed;
  char *vmod_probed_xml;       // type of content of xml file
  char *amod_probed_xml;       // type of content of xml file

  
  int verbose;
  
  char *video_in_file;       // video source
  char *audio_in_file;       // audio source
  
  char *nav_seek_file;        // seek/index information

  int in_flag;

  int has_audio;           // stream has probably no audio 
  int has_audio_track;     // stream has probably not requested track
  int has_video;           // stream has probably no video
 
  int lang_code;            // track language
  
  int a_track;
  int v_track;
  int s_track;              // subtitle track id
  
  int sync;                 // audio frame offset for synchronization
  int sync_ms;              // fine tuning audio/video offset

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

  char *vob_info_file;

  double pts_start;

  double psu_offset;         // psu offset to pass to extsub

  int demuxer;

  long format_flag;          //NTSC=1,PAL=0
  long codec_flag;           //MPEG version ...

  int quality;

  int af6_mode;
  
  // audio stream parameter

  int a_stream_bitrate;     //source stream bitrate
  
  int a_chan;
  int a_bits;
  int a_rate;

  int a_padrate;            // zero padding rate

  int im_a_size;            // import total bytes per audio frame 
  int ex_a_size;            // export total bytes per audio frame 

  int im_a_codec;           // true frame buffer audio codec
  int fixme_a_codec;        // true frame buffer audio codec
  
  int a_leap_frame;
  int a_leap_bytes;

  int a_vbr;                // lame vbr switch

  int a52_mode;

  int dm_bits;
  int dm_chan;

  // video stream parameter

  int v_stream_bitrate;     //source stream bitrate
  
  double fps;               // defaults to PAL: 25 fps
  int im_frc;               // import frame rate code
  double ex_fps;            // defaults to PAL: 25 fps
  int ex_frc;               // export frame rate code
  int hard_fps_flag;        // if this is set, disable smooth drop in demuxer

  int pulldown;             // set 3:2 pulldown flags on MPEG export

  int im_v_height;          // import picture height
  int im_v_width;           // import picture width
  int im_v_size;            // total number of bytes per frame

  int v_bpp;                // defaults to BPP
  
  int im_asr;               // import aspect ratio code  
  int ex_asr;               // export aspect ratio code  
  int attributes;           // more video frame attributes 

  int im_v_codec;           // true frame buffer video codec

  int encode_fields;        // flag

  int dv_yuy2_mode;            // DV decoding option (0.6.5, ThOe)

  // audio frame manipulation info

  int core_a_format;      // internal audio data format

  double volume;          // audio amplitude rescale parameter
  double ac3_gain[3];     // audio amplitude rescale parameter for ac3
  int clip_count;         // total number of bytes clipped after 
                          // volume adjustment 

  // video frame manipulation info

  int core_v_format;      // internal video data format  

  int ex_v_width;         // export picture width
  int ex_v_height;        // export picture height
  int ex_v_size;          // total number of bytes per frame
  
  int reduce_h;             // reductionfactor for frame size 2^reduce
  int reduce_w;             // reductionfactor for frame size 2^reduce

  int resize1_mult;       // row resp. colums multiplier
  int vert_resize1;        // resize height: h -> h - vert_resize*resize1_mult rows
  int hori_resize1;        // resize width:  w -> w - hori_resize*resize1_mult coloums

  
  int resize2_mult;        // row resp. colums multiplier
  int vert_resize2;        // resize height: h -> h + vert_resize*resize2_mult rows
  int hori_resize2;        // resize width:  w -> w + hori_resize*resize2_mult coloums

  int zoom_width;         // zoom width
  int zoom_height;        // zoom height

  double (*zoom_filter)(double);
  double zoom_support;

  int antialias;          // not yet implemented
  int deinterlace;        // not yet implemented
  int decolor;

  double aa_weight;       // anti-alias center pixel weight
  double aa_bias;         // anti-alias horizontal/vertical bias
   
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

  // export info

  char *video_out_file;
  char *audio_out_file;

  avi_t *avifile_in;
  avi_t *avifile_out;
  int avi_comment_fd; // text file to read avi header comments from

  int out_flag;

  // encoding parameter
  
  int divxbitrate;
  int divxkeyframes;
  int divxquality;
  int divxcrispness;
  int divxmultipass;
  int video_max_bitrate;
  char *divxlogfile;

  int min_quantizer;
  int max_quantizer;

  int rc_period;
  int rc_reaction_period;
  int rc_reaction_ratio;

  int divx5_vbv_prof; // profile
  // Video Bitrate Verifier contraints (override profile)
  int divx5_vbv_bitrate;
  int divx5_vbv_size;
  int divx5_vbv_occupancy;

  int mp3bitrate;
  int mp3frequency;
  float mp3quality;         //0=best (very slow).  9=worst (default=5)
  int mp3mode;              //0=joint-stereo, 1=full-stereo, 2=mono

  int bitreservoir;
  char *lame_preset;

  char *audiologfile;
  
  int ex_a_codec;         // audio codec for export module
  int ex_v_codec;         // video codec fcc for export module
  
  char *ex_v_fcc;        //video fourcc string
  char *ex_a_fcc;        //audio fourcc string/identifier
  char *ex_profile_name; //user profile name
  
  int pass_flag;
  int lame_flush;
  
  char *mod_path;

  struct fc_time *ttime; // for framecode parsing, pointer to first struct
  int  ttime_current;    // current time element, needed?

  unsigned int frame_interval; //select every `frame_interval` frames only

  int   chanid;
  char *station_id;

  char *im_v_string;  // pass parameters to import video modules
  char *im_a_string;  // pass parameters to import audio modules
  char *ex_v_string;  // pass parameters to export video modules
  char *ex_a_string;  // pass parameters to export audio modules

  int accel;

  int video_frames_delay; // delay audio by N frames, (video is late)
  float m2v_requant;    // Requantize Factor for mpeg2 video streams
  
} vob_t;

/* -----------------------------------
 *
 * source probe information structure
 *
 * -----------------------------------*/


typedef struct pcm_s {
  
  int samplerate;
  int chan;
  int bits;
  int bitrate;
  int padrate;         // byterate for zero padding
  
  int format;
  int lang;
  
  int attribute;       // 0=subtitle,1=AC3,2=PCM 
  int tid;             // logical track id, in case of gaps
  
  double pts_start;

} pcm_t;


typedef struct probe_info_s {
  
  int width;              //frame size parameter
  int height;             //frame size parameter
  
  double fps;             //encoder fps
  
  long codec;             //video codec
  long magic;             //file type/magic
  long magic_xml;         // specifies type/magic of the file content in xml file

  int asr;                //aspect ratio code
  int frc;                //frame cate code

  int attributes;         //video attributes
  
  int num_tracks;         //number of audio tracks
  
  pcm_t track[TC_MAX_AUD_TRACKS]; //probe for TC_MAX_AUD_TRACKS tracks

  long frames;            //total frames
  long time;              //total time in secs

  int unit_cnt;           //detected presentation units
  double pts_start;       //video PTS start

  long bitrate;           //video stream bitrate

  int ext_attributes[4];  //reserved for MPEG

  int is_video;            //NTSC flag

} probe_info_t;

/* -----------------------------------
 *
 * import threads parameter structure
 *
 * -----------------------------------*/

typedef struct _info_t {
  
  int fd_in;      // input stream file descriptor
  int fd_out;     // output stream file descriptor
  
  long magic;     // specifies file magic for extract thread
  int track;      // extract this track
  
  int width;      // logical stream parameter
  int height;     // logical stream parameter

  int padrate;    // zero padding rate
  
  long stype;     // specifies stream type for extract thread
  long codec;     // specifies codec for decoder thread
  
  long format;    // specifies raw stream format for decoder output
  int verbose;    // verbosity

  int dvd_title;
  int dvd_chapter;
  int dvd_angle;

  int vob_offset;

  int a52_mode;
 
  int ps_unit;
  int ps_seq1;
  int ps_seq2;

  int ts_pid;

  int seek_allowed;
  
  int demux;      // demux or debug
  int select;     // selected packet payload type
  int subid;      // selected packet substream id
  int keep_seq;   // do not drop firsat sequence (cluster mode)

  double fps;

  int fd_log;

  char *name;     // source name as supplied with -i option
  char *nav_seek_file; // seek/index file

  int probe;      // flag for probe only mode
  int factor;     // amount of MB to probe

  probe_info_t *probe_info;    

  int quality;
  int error;

  double ac3_gain[3];
  long frame_limit[3];

  int dv_yuy2_mode;
  int hard_fps_flag;    // if this is set, disable smooth drop in demuxer

} info_t;

typedef struct subtitle_header_s {

  unsigned int header_length;   
  unsigned int header_version;
  unsigned int payload_length;
  
  unsigned int lpts;
  double rpts;

  unsigned int discont_ctr;

} subtitle_header_t;

// some functions exported by transcode

void tc_progress(char *string);
void tc_import_stop_nolock(void);
void tc_export_stop_nolock(void);
/* these function are in tc_functions.{ch}
void tc_error(char *fmt, ...);
void tc_info(char *fmt, ...);
void tc_warn(char *fmt, ...);
*/
vob_t *tc_get_vob(void); 
long tc_get_frames_encoded(void);
long tc_get_frames_dropped(void);
long tc_get_frames_cloned(void);
void tc_update_frames_dropped(long cc);
void tc_update_frames_skipped(long cc);
void tc_update_frames_encoded(long cc);
void tc_update_frames_cloned(long cc);
void tc_set_force_exit(void);
int tc_get_force_exit(void);

void tc_outstream_rotate();
void tc_outstream_rotate_request();

void tc_adjust_frame_buffer(int height, int width);

#define Malloc(p,n,typ)  do if( !(p = (typ *) malloc ( sizeof(typ) * (n) )))\
{\
    perror(__FILE__); \
    fprintf(stderr, "in line %d: \n", __LINE__);\
    fflush(stderr);\
    exit(1);\
}while(0)

#define pow2(b) (((b)==0) ? 1 : 1<<(b))

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
extern int print_counter_interval;
extern int print_counter_cr;

// core parameter
extern int tc_buffer_delay_dec;
extern int tc_buffer_delay_enc;
extern int tc_encode_stream;
extern int tc_decode_stream;
extern int tc_cluster_mode;
extern int tc_decoder_delay;
extern int tc_x_preview;
extern int tc_y_preview;
extern int tc_progress_meter;
extern pthread_t tc_pthread_main;
extern int tc_accel;
extern int tc_avi_limit;
extern int tc_frame_width_max;
extern int tc_frame_height_max;
extern pid_t tc_probe_pid;

# define TC_EXPORT_NAME     10
# define TC_EXPORT_OPEN     11
# define TC_EXPORT_INIT     12
# define TC_EXPORT_ENCODE   13
# define TC_EXPORT_CLOSE    14
# define TC_EXPORT_STOP     15

# define TC_EXPORT_ERROR    -1
# define TC_EXPORT_UNKNOWN   1
# define TC_EXPORT_OK        0

# define TC_IMPORT_NAME     20
# define TC_IMPORT_OPEN     21
# define TC_IMPORT_DECODE   22
# define TC_IMPORT_CLOSE    23

# define TC_IMPORT_UNKNOWN   1
# define TC_IMPORT_ERROR    -1
# define TC_IMPORT_OK        0

# define TC_CAP_PCM          1
# define TC_CAP_RGB          2
# define TC_CAP_AC3          4
# define TC_CAP_YUV          8
# define TC_CAP_AUD         16
# define TC_CAP_VID         32
# define TC_CAP_MP3         64
# define TC_CAP_YUY2       128
# define TC_CAP_DV         256

# define TC_MODE_DEFAULT            0
# define TC_MODE_AVI_SPLIT          1
# define TC_MODE_DVD_CHAPTER        2
# define TC_MODE_PSU                4
# define TC_MODE_DIRECTORY         16
# define TC_MODE_DEBUG             32

#define DD {printf("(%s) CHECK @ line (%d)\n", __FILE__, __LINE__); sleep(1);}

#define tc_pthread_mutex_lock(S) {fprintf(stderr, "(%s@%d) (%ld) lock....\n", __FILE__, __LINE__, pthread_self()); pthread_mutex_lock(S);}
#define tc_pthread_mutex_unlock(S) {fprintf(stderr, "(%s@%d)  (%ld) ...unlock\n", __FILE__, __LINE__, pthread_self()); pthread_mutex_unlock(S);}
  
#define tc_pthread_cond_wait(A,B) {fprintf(stderr, "(%s@%d) (%ld) condwait\n", __FILE__, __LINE__, pthread_self()); pthread_cond_wait(A,B);}
  
#define tc_pthread_mutex_trylock(S);{fprintf(stderr, "(%s@%d) (%ld) try lock\n", __FILE__, __LINE__, pthread_self()); pthread_mutex_trylock(S);}

#endif
