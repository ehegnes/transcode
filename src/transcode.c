/*
 *  transcode.c 
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

#include <ctype.h>
#include <math.h>

#include "transcode.h"
#include "decoder.h"
#include "encoder.h"
#include "dl_loader.h"
#include "framebuffer.h"
#include "counter.h"
#include "frame_threads.h"
#include "filter.h"
#include "probe.h"
#include "split.h"
#include "iodir.h"
#include "libxio/xio.h"

#ifdef HAVE_GETOPT_LONG_ONLY
#include <getopt.h>
#else 
#include "libsupport/getopt.h"
#endif

#include "usage.h"

/* imported from libtc
#define COL(x)  "\033[" #x ";1m"
char *RED    = COL(31);
char *GREEN  = COL(32);
char *YELLOW = COL(33);
char *BLUE   = COL(34);
char *WHITE  = COL(37);
char *GRAY   =  "\033[0m";
*/

// communicating with export modules to allow to set them defaults.
#include "probe_export.h"
unsigned int probe_export_attributes = 0;


/* ------------------------------------------------------------ 
 *
 * default options
 *
 * ------------------------------------------------------------*/


int pcmswap     = TC_FALSE;
int rgbswap     = TC_FALSE;
int rescale     = TC_FALSE;
int im_clip     = TC_FALSE;
int ex_clip     = TC_FALSE;
int pre_im_clip = TC_FALSE;
int post_ex_clip= TC_FALSE;
int flip        = TC_FALSE;
int mirror      = TC_FALSE;
int resize1     = TC_FALSE;
int resize2     = TC_FALSE;
int decolor     = TC_FALSE;
int zoom        = TC_FALSE;
int dgamma      = TC_FALSE;
int keepasr     = TC_FALSE;
int fast_resize = TC_FALSE;

// global information structure
static vob_t *vob;
int verbose = TC_INFO;

static int core_mode=TC_MODE_DEFAULT;

static int tcversion = 0;
static int sig_int   = 0;
static int sig_tstp  = 0;

static char *im_aud_mod = NULL, *im_vid_mod = NULL;
static char *ex_aud_mod = NULL, *ex_vid_mod = NULL;

static pthread_t thread_signal=(pthread_t)0, thread_server=(pthread_t)0, thread_socket=(pthread_t)0;
int tc_signal_thread     =  0;
sigset_t sigs_to_block;

// for initializing libavcodec
pthread_mutex_t init_avcodec_lock=PTHREAD_MUTEX_INITIALIZER;
// for initializing export_pvm
pthread_mutex_t s_channel_lock=PTHREAD_MUTEX_INITIALIZER;

void socket_thread(void); // socket.c

char *socket_file = NULL;
char *plugins_string = NULL;
size_t size_plugstr = 0;
char *tc_config_dir = NULL;
pid_t writepid = 0;
pthread_mutex_t writepid_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t writepid_cond = PTHREAD_COND_INITIALIZER;

char *video_ext = ".avi";
char *audio_ext = ".mp3";

//default
int tc_encode_stream = 0;
int tc_decode_stream = 0;

enum {
  ZOOM_FILTER = CHAR_MAX+1,
  CLUSTER_PERCENTAGE,
  CLUSTER_CHUNKS,
  EXPORT_ASR,
  IMPORT_ASR,
  EXPORT_FRC,
  EXPORT_FPS,
  HARD_FPS,
  DIVX_QUANT,
  DIVX_RC,
  IMPORT_V4L,
  RECORD_V4L,
  PULLDOWN,
  ANTIALIAS_PARA,
  MORE_HELP,
  KEEP_ASR,
  NO_AUDIO_ADJUST,
  NO_BITRESERVOIR,
  AV_FINE_MS,
  DURATION,
  NAV_SEEK,
  PSU_MODE,
  PSU_CHUNKS,
  NO_SPLIT,
  PRE_CLIP,
  POST_CLIP,
  A52_DRC_OFF,
  A52_DEMUX,
  A52_DOLBY_OFF,
  DIR_MODE,
  FRAME_INTERVAL,
  ENCODE_FIELDS,
  PRINT_STATUS,
  WRITE_PID,
  NICENESS,
  PROGRESS_OFF,
  DEBUG_MODE,
  ACCEL_MODE,
  TS_PID,
  AVI_LIMIT,
  SOCKET_FILE,
  DV_YUY2_MODE,
  LAME_PRESET,
  COLOR_LEVEL,
  VIDEO_MAX_BITRATE,
  AVI_COMMENTS,
  DIVX5_VBV_PROF,
  DIVX5_VBV,
  CONFIG_DIR,
  USE_YUV422,
  DVD_ACCESS_DELAY,
  EXTENSIONS,
  EX_PIXEL_ASPECT,
  EXPORT_PROF,
};

int print_counter_interval = 1;
int print_counter_cr = 0;
int color_level = 0;

//-------------------------------------------------------------
// core parameter

int tc_buffer_delay_dec  = -1;
int tc_buffer_delay_enc  = -1;
int tc_server_thread     =  0;
int tc_cluster_mode      =  0;
int tc_decoder_delay     =  0;
int tc_x_preview         =  0;
int tc_y_preview         =  0;
int tc_progress_meter    =  1;
int tc_accel             = AC_ALL;    //acceleration code
unsigned int tc_avi_limit = (unsigned int)-1;
pid_t tc_probe_pid       = 0;
int tc_frame_width_max   = 0;
int tc_frame_height_max  = 0;
int tc_niceness          = 0;

// for import_dvd
int tc_dvd_access_delay  = 3;

//-------------------------------------------------------------

pthread_t tc_pthread_main;

/* ------------------------------------------------------------ 
 *
 * print a usage/version message
 *
 * ------------------------------------------------------------*/

void version(void)
{
  /* id string */
  if(tcversion++) return;
  fprintf(stderr, "%s v%s (C) 2001-2003 Thomas Oestreich, 2003-2004 T. Bitterberg\n", PACKAGE, VERSION);
}


static void usage(int status)
{
  version();

  printf("\nUsage: transcode [options]\n");
  printf("\noptions:\n");

  //source
#ifdef HAVE_LIBDVDREAD
#ifdef NET_STREAM
  printf(" -i name             input file/directory/device/mountpoint/host name\n");
#else
  printf(" -i name             input file/directory/device/mountpoint name\n");
#endif
#else
#ifdef NET_STREAM 
  printf(" -i name             input file/directory/host name\n");
#else
  printf(" -i name             input file/directory name\n");
#endif
#endif
  printf(" -H n                auto-probe n MB of source (0=off) [1]\n");
  printf(" -p file             read audio stream from separate file [off]\n");
  printf(" -x vmod[,amod]      video[,audio] import modules [%s]\n", 
	 TC_DEFAULT_IMPORT_VIDEO);
  printf(" -a a[,v]            extract audio[,video] track [0,0]\n");
  printf("--dvd_access_delay N delay DVD access by N seconds [3]\n");
  printf("\n");

  //audio
  printf(" -e r[,b[,c]]        PCM audio stream parameter [%d,%d,%d]\n", RATE, BITS, CHANNELS);
  printf(" -E r[,b[,c]]        audio output samplerate, bits, channels [as input]\n"); 
  printf(" -n 0xnn             import audio format id [0x%x]\n", CODEC_AC3);
  printf(" -N 0xnn             export audio format id [0x%x]\n", CODEC_MP3);
  printf(" -b b[,v[,q[,m]]]    audio encoder bitrate kBits/s[,vbr[,quality[,mode]]] [%d,%d,%d,%d]\n", ABITRATE, AVBR, AQUALITY, AMODE);
  printf("--no_audio_adjust    disable audio frame sample adjustment [off]\n");
  printf("--no_bitreservoir    disable lame bitreservoir [off]\n");
  printf("--lame_preset name[,fast]  use lame preset with name. [off]\n");
  printf("\n");

  //video
  printf(" -g wxh              RGB video stream frame size [%dx%d]\n", PAL_W, PAL_H);
  printf("--import_asr C       set import display aspect ratio code C [auto]\n");
  printf("--export_asr C       set export display aspect ratio code C [as input]\n");
  printf("--export_par N,D     set export pixel aspect ratio [auto]\n");
  printf("--keep_asr           try to keep aspect ratio (only with -Z) [off]\n");
  printf(" -f rate[,frc]       input video frame rate[,frc] [%.3f,0] fps\n", PAL_FPS);
  printf("--export_fps f[,c]   output video frame rate[,code] [as input] [%0.3f,3]\n", PAL_FPS);
  printf("--export_frc F       set export frame rate code F [as input]\n");
  printf("--hard_fps           disable smooth dropping (for variable fps clips) [off]\n");
  printf("\n");

  //output
  printf(" -o file             output file name\n");
  printf(" -m file             write audio stream to separate file [off]\n");
  printf(" -y vmod[,amod]      video[,audio] export modules [%s]\n", 
	 TC_DEFAULT_EXPORT_VIDEO);
  printf(" -F codec            encoder parameter strings [module dependent]\n");
  printf(" --avi_limit N       split output AVI file after N MB [%d]\n", AVI_FILE_LIMIT);
  printf(" --avi_comments F    Read AVI header comments from file F (see transcode(1)) [off]\n");
  printf("\n");

  //audio effects
  printf(" -d                  swap bytes in audio stream [off]\n");
  printf(" -s g[,c[,f[,r]]]    increase volume by gain,[center,front,rear] [off,1,1,1]\n");
  printf("\n");

  //processing
  printf(" -u m[,n]            use m framebuffer[,n threads] for AV processing [%d,%d]\n", TC_FRAME_BUFFER, TC_FRAME_THREADS);
  printf(" -A                  use AC3 as internal audio codec [off]\n");
  printf(" -V                  use YV12/I420/YUV420 as internal video format [deprecated, default]\n");
  printf(" --use_rgb           use RGB as internal video format [off]\n");
  printf(" --yuv422            use YUV422 as internal video format [off]\n");
  printf(" -J f1[,f2[,...]]    apply external filter plugins [off]\n");
  printf(" -P flag             pass-through flag (0=off|1=V|2=A|3=A+V) [0]\n");
  printf("\n");

  //AV offset
  printf(" -D num              sync video start with audio frame num [0]\n");
  printf("--av_fine_ms t       AV fine-tuning shift t in millisecs [autodetect]\n");
  printf("\n");

  //sync modes
  printf(" -M mode             demuxer PES AV sync mode (0=off|1=PTS only|2=full) [1]\n");
  printf(" -O                  flush lame mp3 buffer on encoder stop [off]\n");
  printf("\n");

  //resizing
  printf(" -r n[,m]            reduce video height/width by n[,m] [off]\n");
  printf(" -B n[,m[,M]]        resize to height-n*M rows [,width-m*M] columns [off,32]\n");
  printf(" -X n[,m[,M]]        resize to height+n*M rows [,width+m*M] columns [off,32]\n");
  printf(" -Z wxh[,fast]       resize to w columns, h rows with filtering [off,notfast]\n");
  printf("--zoom_filter str    use filter string for video resampling -Z [Lanczos3]\n");
  printf("\n");

  //anti-alias
  printf(" -C mode             enable anti-aliasing mode (1-3) [off]\n");
  printf("--antialias_para w,b center pixel weight, xy-bias [%.3f,%.3f]\n", TC_DEFAULT_AAWEIGHT, TC_DEFAULT_AABIAS);
  printf("\n");

  //de-interlacing
  printf(" -I mode             enable de-interlacing mode (1-5) [off]\n");
  printf("\n");

  //video effects
  printf(" -K                  enable b/w mode [off]\n");
  printf(" -G val              gamma correction (0.0-10.0) [off]\n");
  printf(" -z                  flip video frame upside down [off]\n");
  printf(" -l                  mirror video frame [off]\n");
  printf(" -k                  swap red/blue (Cb/Cr) in video frame [off]\n");
  printf("\n");

  //clipping
  printf(" -j t[,l[,b[,r]]]    select frame region by clipping border [off]\n");
  printf(" -Y t[,l[,b[,r]]]    select (encoder) frame region by clipping border [off]\n");
  printf("--pre_clip t[,l[,b[,r]]]  select initial frame region by clipping border [off]\n");
  printf("--post_clip t[,l[,b[,r]]] select final frame region by clipping border [off]\n");
  printf("\n");

  //multi-pass
  printf(" -w b[,k[,c]]        encoder bitrate[,keyframes[,crispness]] [%d,%d,%d]\n", VBITRATE, VKEYFRAMES, VCRISPNESS);
  printf("--video_max_bitrate  maximum bitrate when encoding variable bitrate MPEG-2 streams [same as -w]\n");
  printf(" -R n[,f1[,f2]]      enable multi-pass encoding (0-3) [%d,divx4.log,pcm.log]\n", VMULTIPASS);
  printf(" -Q n[,m]            encoding[,decoding] quality (0=fastest-5=best) [%d,%d]\n", VQUALITY, VQUALITY);
  printf("--divx_quant min,max divx encoder min/max quantizer [%d,%d]\n", VMINQUANTIZER, VMAXQUANTIZER);
  printf("--divx_rc p,rp,rr    divx encoder rate control parameter [%d,%d,%d]\n", RC_PERIOD, RC_REACTION_PERIOD, RC_REACTION_RATIO);
  printf("--divx_vbv_prof N    divx5 encoder VBV profile (0=free-5=hiqhq) [%d]\n", DIVX5_VBV_PROFILE);
  printf("--divx_vbv br,sz,oc  divx5 encoder VBV params (bitrate,size,occup.) [%d,%d,%d]\n", 
      DIVX5_VBV_BITRATE, DIVX5_VBV_SIZE, DIVX5_VBV_OCCUPANCY);
  printf("\n");

  //range control
  printf(" -c f1-f2[,f3-f4]    encode only f1-f2[,f3-f4] (frames or HH:MM:SS) [all]\n");
  printf(" -t n,base           split output to base%s.avi with n frames [off]\n", "%03d");
  printf("--dir_mode base      process directory contents to base-%s.avi [off]\n", "%03d");
  printf("--frame_interval N   select only every Nth frame to be exported [1]\n");
  printf("\n");

  //DVD
#ifdef HAVE_LIBDVDREAD
  printf(" -U base             process DVD in chapter mode to base-ch%s.avi [off]\n", "%02d");
  printf(" -T t[,c[-d][,a]]    select DVD title[,chapter(s)[,angle]] [1,1,1]\n");
  printf("\n");
#endif

  //cluster mode
  printf(" -W n,m[,file]       autosplit and process part n of m (VOB only) [off]\n");
  printf("--cluster_percentage use percentage mode for cluster encoding -W [off]\n");
  printf("--cluster_chunks a-b process chunk range instead of selected chunk [off]\n");
  printf(" -S unit[,s1-s2]     process program stream unit[,s1-s2] sequences [0,all]\n");
  printf(" -L n                seek to VOB stream offset nx2kB [0]\n");
  printf("\n");

  //v4l
  printf("--import_v4l n[,id]  channel number and station number or name [0]\n");
  printf("\n");
  
  //mpeg
  printf("--pulldown           set MPEG 3:2 pulldown flags on export [off]\n");
  printf("--encode_fields      enable field based encoding (if supported) [off]\n");
  printf("\n");

  //psu mode
  printf("--nav_seek file      use VOB navigation file [off]\n");
  printf("--psu_mode           process VOB in PSU, -o is a filemask incl. %%d [off]\n");
  printf("--psu_chunks a-b     process only selected units a-b for PSU mode [all]\n");
  printf("--no_split           encode to single file in chapter/psu mode [off]\n");
  //  printf("--ts_pid 0xnn[,0xmm] transport stream pids [0,0]\n");
  printf("--ts_pid 0xnn        transport video stream pid [0]\n");
  printf("\n");

  //a52
  printf("--a52_drc_off        disable liba52 dynamic range compression [enabled]\n");
  printf("--a52_demux          demux AC3/A52 to separate channels [off]\n");
  printf("--a52_dolby_off      disable liba52 dolby surround [enabled]\n");
  printf("\n");

  //internal flags
  printf("--print_status N[,r] print status every N frames / use CR or NL [1,1]\n");
  printf("--progress_off       disable progress meter status line [off]\n");
  printf("--color N            level of color in transcodes output [1]\n");
  printf("--write_pid file     write pid of signal thread to \"file\" [off]\n");
  printf("--nice N             set niceness to N [off]\n");
#if defined(ARCH_X86) || defined(ARCH_X86_64)
  printf("--accel type[,...]   override CPU acceleration flags (for debugging)\n");
#endif
  printf("--socket file        socket file for run-time control [no file]\n");
  printf("--dv_yuy2_mode       libdv YUY2 mode (default is YV12) [off]\n");
  printf("--config_dir dir     Assume config files are in this dir [off]\n");
  printf("--ext vid,aud        Use these file extensions [%s,%s]\n", video_ext, audio_ext);
  printf("--export_prof S      Export profile {vcd, svcd, xvcd,  dvd}[-pal|-ntsc|-secam]\n");

  printf("\n");

  //help
  printf(" -q level            verbosity (0=quiet,1=info,2=debug) [%d]\n", TC_INFO);
  printf(" -h                  this usage message\n");
  printf(" -v                  print version\n");
//  printf("--more_help param   more help on named parameter\n");
  printf("\n");
  
  if (vob) free(vob);
  exit(status);
  
}

static void short_usage(int status)
{
  version();

  printf("\'transcode -h | more\' shows a list of available command line options.\n");
  if (vob) free(vob);
  exit(status);
  
}


static int source_check(char *import_file)
{
    // check for existent file, directory or host
    struct stat fbuf;
#ifdef NET_STREAM
    struct hostent *hp;
#endif

    if(import_file==NULL) { 
      tc_error("invalid filename \"%s\"", import_file);
      return(1);
    }

    if(import_file[0] == '!') return(0);
    
    if(xio_stat(import_file, &fbuf)==0) return(0);
    
#ifdef NET_STREAM    
    if((hp = gethostbyname(import_file)) != NULL) return(0);
    tc_error("invalid filename or host \"%s\"", import_file);
#else
    tc_error("invalid filename \"%s\"", import_file);
#endif
    return(1);
}


static void signal_thread(void)
{      
  
  int caught;
  char *signame = NULL;

  pthread_mutex_lock(&writepid_mutex);
  writepid = getpid();
  if (writepid > 0)
    pthread_cond_signal(&writepid_cond);
  pthread_mutex_unlock(&writepid_mutex);
  
  for (;;) {

    /* sigs_to_block were blocked in main() */    
    sigwait(&sigs_to_block, &caught);
#ifdef BROKEN_PTHREADS // Used to be MacOSX specific; kernel 2.6 as well?
    pthread_testcancel();
#endif
    
    switch (caught) {
    case SIGINT:  signame = "SIGINT"; break;
    case SIGTERM: signame = "SIGTERM"; break;
    case SIGPIPE: signame = "SIGPIPE"; break;
    }
    
    if (signame) {
      if(verbose & TC_INFO) fprintf(stderr, "\n[%s] (sighandler) %s received\n", PACKAGE, signame);

      sig_int=1;

      if(tc_probe_pid>0) kill(tc_probe_pid, SIGTERM);

      // import (termination signal)
      tc_import_stop_nolock();

      if(verbose & TC_DEBUG) fprintf(stderr, "[%s] (sighandler) import cancelation submitted\n", PACKAGE);

      // export (termination signal)
      tc_export_stop_nolock();

      if(verbose & TC_DEBUG) fprintf(stderr, "[%s] (sighandler) export cancelation submitted\n", PACKAGE);

      pthread_testcancel();

    }
  }
}

vob_t *tc_get_vob() {return(vob);}
 

/* ------------------------------------------------------------ 
 *
 * transcoder engine
 *
 * ------------------------------------------------------------*/

static int transcoder(int mode, vob_t *vob) 
{
    
    switch(mode) {
      
    case TC_ON:
      
      if(im_aud_mod && strcmp(im_aud_mod,"null") != 0) tc_decode_stream|=TC_AUDIO;
      if(im_vid_mod && strcmp(im_vid_mod,"null") != 0) tc_decode_stream|=TC_VIDEO;

      // load import modules and check capabilities
      if(import_init(vob, im_aud_mod, im_vid_mod)<0) {
	fprintf(stderr,"[%s] failed to init import modules\n", PACKAGE);
	return(-1);
      }  

      // load and initialize filter plugins
      plugin_init(vob);

      // initalize filter plugins
      filter_init();
      
      if(ex_aud_mod && strcmp(ex_aud_mod,"null") != 0) tc_encode_stream|=TC_AUDIO;
      if(ex_vid_mod && strcmp(ex_vid_mod,"null") != 0) tc_encode_stream|=TC_VIDEO;
      
      // load export modules and check capabilities
      if(export_init(vob, ex_aud_mod, ex_vid_mod)<0) {
      	tc_warn("failed to init export modules");
	return(-1);
      }  
      
      break;
      
    case TC_OFF:
      
      // unload import modules
      import_shutdown();

      // call all filter plug-ins closing routines
      filter_close();
      
      // and unload plugin
      plugin_close();
      
      // unload export modules
      export_shutdown();
      
      break;

    default:
      return(-1);
    }
    
    return(0);
}

// for atexit
static void safe_exit (void)
{
    void *thread_status;

    if (tc_signal_thread) {
       pthread_cancel(thread_signal);
#ifdef BROKEN_PTHREADS // Used to be MacOSX specific; kernel 2.6 as well?
       pthread_kill(thread_signal,SIGINT);
#endif
       pthread_join(thread_signal, &thread_status);
    }
}

/* ------------------------------------------------------------ 
 *
 * parse the command line and start main program loop
 *
 * ------------------------------------------------------------*/

int main(int argc, char *argv[]) {

#ifdef HAVE_LIBXML2
    FILE *p_fd_tcxmlcheck;
    int s_tcxmlcheck_resize;
    char *p_tcxmlcheck_buffer;
#endif
    // v4l capture
    int chanid = -1;
    char station_id[TC_BUF_MIN];

    char 
      *audio_in_file=NULL, *audio_out_file=NULL,
      *video_in_file=NULL, *video_out_file=NULL,
      *nav_seek_file=NULL;

    int n=0, ch1, ch2, fa, fb, hh, mm, ss;
    
    int psu_frame_threshold=12; //psu with less/equal frames are skipped.

    int 
	no_vin_codec=1, no_ain_codec=1,
	no_v_out_codec=1, no_a_out_codec=1;
    
    int 
	frame_a=TC_FRAME_FIRST,   // defaults to all frames
	frame_b=TC_FRAME_LAST,
	frame_asec=0, frame_bsec=0,
	splitavi_frames=0,
	psu_mode=TC_FALSE;

    char 
      base[TC_BUF_MIN], buf[TC_BUF_MAX], 
      *chbase=NULL, *psubase=NULL, *dirbase=NULL, 
      abuf1[TC_BUF_MIN], vbuf1[TC_BUF_MIN], 
      abuf2[TC_BUF_MIN], vbuf2[TC_BUF_MIN];

#if defined(ARCH_X86) || defined(ARCH_X86_64)
    char
      *accel=NULL;
#endif
    
    char 
      vlogfile[TC_BUF_MIN], alogfile[TC_BUF_MIN], vob_logfile[TC_BUF_MIN];

    char *aux_str;
    char **endptr=&aux_str;

    char *dir_name, *dir_fname;
    int dir_fcnt=0, dir_audio=0;

    transfer_t export_para;
    
    double fch, asr;
    int leap_bytes1, leap_bytes2;

    int preset_flag=0, auto_probe=1, seek_range=1;

    void *thread_status;

    int option_index = 0;

    char *zoom_filter="Lanczos3";

    struct fc_time *tstart = NULL;
    char *fc_ttime_separator = ",";
    char *fc_ttime_string = NULL;
    int sync_seconds=0;

    int no_audio_adjust=TC_FALSE, no_split=TC_FALSE;

    long sret;  /* used for string function return values */

    static struct option long_options[] =
    {
      {"version", no_argument, NULL, 'v'},
      {"help", no_argument, NULL, 'h'},
      {"resize_up", required_argument, NULL, 'X'},
      {"resize_down", required_argument, NULL, 'B'},
      {"input_file", required_argument, NULL, 'i'},
      {"output_file", required_argument, NULL, 'o'},
      {"probe", optional_argument, NULL, 'H'},
      {"astream_file_in", required_argument, NULL, 'p'},
      {"extract_track", required_argument, NULL, 'a'},
      {"dvd_title", required_argument, NULL, 'T'},
      {"vob_seek", required_argument, NULL, 'L'},
      {"pcm_astream", required_argument, NULL, 'e'},
      {"frame_size", required_argument, NULL, 'g'},
      {"buffer", required_argument, NULL, 'u'},
      {"import_mods", required_argument, NULL, 'x'},
      {"astream_file_out", required_argument, NULL, 'm'},
      {"export_mods", required_argument, NULL, 'y'},
      {"audio_swap_bytes", no_argument, NULL, 'd'},
      {"increase_volume", required_argument, NULL, 's'},
      {"use_ac3", no_argument, NULL, 'A'},
      {"use_yuv", no_argument, NULL, 'V'},
      {"use_rgb", no_argument, NULL, '1'},
      {"external_filter", required_argument, NULL, 'J'},
      {"pass-through", required_argument, NULL, 'P'},
      {"av_sync_offset", required_argument, NULL, 'D'},
      {"demux_mode", required_argument, NULL, 'M'},
      {"flush_lame_buffer", no_argument, NULL, 'O'},
      {"av_rate", required_argument, NULL, 'f'},
      {"flip", no_argument, NULL, 'z'},
      {"mirror", no_argument, NULL, 'l'},
      {"video_swap_bytes", no_argument, NULL, 'k'},
      {"reduce_height_width", required_argument, NULL, 'r'},
      {"resize", required_argument, NULL, 'Z'},
      {"anti-alias", required_argument, NULL, 'C'},
      {"de-interlace", required_argument, NULL, 'I'},
      {"bw", no_argument, NULL, 'K'},
      {"gamma", required_argument, NULL, 'G'},
      {"pre_clipping", required_argument, NULL, 'j'},
      {"post_clipping", required_argument, NULL, 'Y'},
      {"video_bitrate", required_argument, NULL, 'w'},
      {"multi-pass", required_argument, NULL, 'R'},
      {"quality", required_argument, NULL, 'Q'},
      {"audio_bitrate", required_argument, NULL, 'b'},
      {"import_audio_format", required_argument, NULL, 'n'},
      {"export_audio_format", required_argument, NULL, 'N'},
      {"re-sample", required_argument, NULL, 'E'},
      {"codec", required_argument, NULL, 'F'},
      {"encode_frames", required_argument, NULL, 'c'},
      {"split_output", required_argument, NULL, 't'},
      {"chapter_mode", required_argument, NULL, 'U'},
      {"auto_split", required_argument, NULL, 'W'},
      {"program_stream", required_argument, NULL, 'S'},
      {"verbosity", required_argument, NULL, 'q'},

      //add new long options here:

      {"zoom_filter", required_argument, NULL, ZOOM_FILTER},
      {"cluster_percentage", no_argument, NULL, CLUSTER_PERCENTAGE},
      {"cluster_chunks", required_argument, NULL, CLUSTER_CHUNKS},
      {"export_asr", required_argument, NULL, EXPORT_ASR},
      {"import_asr", required_argument, NULL, IMPORT_ASR},
      {"export_frc", required_argument, NULL, EXPORT_FRC},
      {"export_fps", required_argument, NULL, EXPORT_FPS},
      {"divx_quant", required_argument, NULL, DIVX_QUANT},
      {"divx_rc", required_argument, NULL, DIVX_RC},
      {"import_v4l", required_argument, NULL, IMPORT_V4L},
      {"record_v4l", required_argument, NULL, RECORD_V4L},
      {"pulldown", no_argument, NULL, PULLDOWN},
      {"antialias_para", required_argument, NULL, ANTIALIAS_PARA},
      {"more_help", required_argument, NULL, MORE_HELP},
      {"keep_asr", no_argument, NULL, KEEP_ASR},
      {"no_audio_adjust", no_argument, NULL, NO_AUDIO_ADJUST},
      {"no_bitreservoir", no_argument, NULL, NO_BITRESERVOIR},
      {"av_fine_ms", required_argument, NULL, AV_FINE_MS},
      {"duration", required_argument, NULL, DURATION},
      {"nav_seek", required_argument, NULL, NAV_SEEK},
      {"psu_mode", no_argument, NULL, PSU_MODE},
      {"psu_chunks", required_argument, NULL, PSU_CHUNKS},
      {"no_split", no_argument, NULL, NO_SPLIT},
      {"pre_clip", required_argument, NULL, PRE_CLIP},
      {"post_clip", required_argument, NULL, POST_CLIP},
      {"a52_drc_off", no_argument, NULL, A52_DRC_OFF},
      {"a52_demux", no_argument, NULL, A52_DEMUX},
      {"a52_dolby_off", no_argument, NULL, A52_DOLBY_OFF},
      {"dir_mode", required_argument, NULL, DIR_MODE},
      {"frame_interval", required_argument, NULL, FRAME_INTERVAL},
      {"encode_fields", required_argument, NULL, ENCODE_FIELDS},
      {"print_status", required_argument, NULL, PRINT_STATUS},
      {"write_pid", required_argument, NULL, WRITE_PID},
      {"nice", required_argument, NULL, NICENESS},
      {"progress_off", no_argument, NULL, PROGRESS_OFF},
      {"debug_mode", no_argument, NULL, DEBUG_MODE},
      {"accel_mode", required_argument, NULL, ACCEL_MODE},
      {"avi_limit", required_argument, NULL, AVI_LIMIT},
      {"ts_pid", required_argument, NULL, TS_PID},
      {"socket", required_argument, NULL, SOCKET_FILE},
      {"dv_yuy2_mode", no_argument, NULL, DV_YUY2_MODE},
      {"lame_preset", required_argument, NULL, LAME_PRESET},
      {"color", required_argument, NULL, COLOR_LEVEL},
      {"colour", required_argument, NULL, COLOR_LEVEL},
      {"video_max_bitrate", required_argument, NULL, VIDEO_MAX_BITRATE},
      {"avi_comments", required_argument, NULL, AVI_COMMENTS},
      {"divx_vbv_prof", required_argument, NULL, DIVX5_VBV_PROF},
      {"divx_vbv", required_argument, NULL, DIVX5_VBV},
      {"hard_fps", no_argument, NULL, HARD_FPS},
      {"config_dir", required_argument, NULL, CONFIG_DIR},
      {"yuv422", no_argument, NULL, USE_YUV422},
      {"dvd_access_delay", required_argument, NULL, DVD_ACCESS_DELAY},
      {"ext", required_argument, NULL, EXTENSIONS},
      {"export_par", required_argument, NULL, EX_PIXEL_ASPECT},
      {"export_prof", required_argument, NULL, EXPORT_PROF},
      {0,0,0,0}
    };
    
    if(argc==1) short_usage(EXIT_FAILURE);

    // if we're sending output to a terminal default to printing CR after every
    // status update instead of LF.
    if (isatty(fileno(stdout))) {
      print_counter_cr = 1;
    } else {
      print_counter_cr = 0;
    }

    // don't do colors if writing to a file
    if (isatty(STDOUT_FILENO)==0 || isatty(STDERR_FILENO)==0) {
      color_level = 0;
      RED    = "";
      GREEN  = "";
      YELLOW = "";
      WHITE  = "";
      GRAY   = "";
      BLUE   = "";
    }

    //main thread id
    tc_pthread_main=pthread_self();
    
    //allocate vob structure
    vob = (vob_t *) malloc(sizeof(vob_t));
    
    /* ------------------------------------------------------------ 
     *
     *  (I) set transcode defaults: 
     *
     * ------------------------------------------------------------*/

    vob->divxbitrate      = VBITRATE;
    vob->video_max_bitrate= 0;           /* 0 = set by encoder */
    vob->divxkeyframes    = VKEYFRAMES;
    vob->divxquality      = VQUALITY;
    vob->divxmultipass    = VMULTIPASS;
    vob->divxcrispness    = VCRISPNESS;
    vob->m2v_requant      = M2V_REQUANT_FACTOR;

    vob->min_quantizer    = VMINQUANTIZER;
    vob->max_quantizer    = VMAXQUANTIZER; 
    
    vob->rc_period          = RC_PERIOD;
    vob->rc_reaction_period = RC_REACTION_PERIOD;
    vob->rc_reaction_ratio  = RC_REACTION_RATIO;

    vob->divx5_vbv_prof      = DIVX5_VBV_PROFILE;
    vob->divx5_vbv_bitrate   = DIVX5_VBV_BITRATE;
    vob->divx5_vbv_size      = DIVX5_VBV_SIZE;
    vob->divx5_vbv_occupancy = DIVX5_VBV_OCCUPANCY;

    vob->mp3bitrate       = ABITRATE;
    vob->mp3frequency     = 0;
    vob->mp3quality       = AQUALITY;
    vob->mp3mode          = AMODE;
    vob->a_rate           = RATE;
    vob->a_stream_bitrate = 0;
    vob->a_bits           = BITS;
    vob->a_chan           = CHANNELS;
    vob->a_padrate        = 0;
    
    vob->dm_bits          = 0;
    vob->dm_chan          = 0;

    vob->im_a_size        = SIZE_PCM_FRAME;
    vob->im_v_width       = PAL_W;
    vob->im_v_height      = PAL_H;
    vob->im_v_size        = SIZE_RGB_FRAME;
    vob->ex_a_size        = SIZE_PCM_FRAME;
    vob->ex_v_width       = PAL_W;
    vob->ex_v_height      = PAL_H;
    vob->ex_v_size        = SIZE_RGB_FRAME;
    vob->v_bpp            = BPP;
    vob->a_track          = 0;
    vob->v_track          = 0;
    vob->volume           = 0;
    vob->ac3_gain[0] = vob->ac3_gain[1] = vob->ac3_gain[2] = 1.0;
    vob->audio_out_file   = "/dev/null";
    vob->video_out_file   = "/dev/null";
    vob->avifile_in       = NULL;
    vob->avifile_out      = NULL;
    vob->avi_comment_fd   = -1;
    vob->nav_seek_file    = NULL;
    vob->out_flag         = 0;
    vob->audio_in_file    = "/dev/zero";
    vob->video_in_file    = "/dev/zero";
    vob->in_flag          = 0;
    vob->clip_count       = 0;
    vob->ex_a_codec       = CODEC_MP3;  //or fall back to module default 
    vob->ex_v_codec       = CODEC_NULL; //determined by type of export module
    vob->ex_v_fcc         = NULL;
    vob->ex_a_fcc         = NULL;
    vob->ex_profile_name  = NULL;
    vob->fps              = PAL_FPS;
    vob->ex_fps           = 0;
    vob->im_frc           = 0;
    vob->ex_frc           = 0;
    vob->pulldown         = 0;
    vob->im_clip_top      = 0;
    vob->im_clip_bottom   = 0;
    vob->im_clip_left     = 0;
    vob->im_clip_right    = 0;
    vob->ex_clip_top      = 0;
    vob->ex_clip_bottom   = 0;
    vob->ex_clip_left     = 0;
    vob->ex_clip_right    = 0;
    vob->resize1_mult     = 32;    
    vob->vert_resize1     = 0;
    vob->hori_resize1     = 0;
    vob->resize2_mult     = 32;
    vob->vert_resize2     = 0;
    vob->hori_resize2     = 0;
    vob->sync             = 0;
    vob->sync_ms          = 0;
    vob->dvd_title        = 1;
    vob->dvd_chapter1     = 1;
    vob->dvd_chapter2     = -1;
    vob->dvd_max_chapters =-1;
    vob->dvd_angle        = 1;
    vob->pass_flag        = 0;
    vob->verbose          = TC_QUIET; 
    vob->antialias        = 0;
    vob->deinterlace      = 0;
    vob->decolor          = 0;
    vob->im_a_codec       = CODEC_PCM; //PCM audio frames requested
    // vob->im_v_codec       = CODEC_RGB; //RGB video frames requested
    vob->im_v_codec       = CODEC_YUV;
    vob->core_a_format    = CODEC_PCM; //PCM 
    vob->core_v_format    = CODEC_RGB; //RGB  // never used, EMS
    vob->mod_path         = MOD_PATH;
    vob->audiologfile     = NULL;
    vob->divxlogfile      = NULL;
    vob->ps_unit          = 0;
    vob->ps_seq1          = 0;
    vob->ps_seq2          = TC_FRAME_LAST;
    vob->a_leap_frame     = TC_LEAP_FRAME;
    vob->a_leap_bytes     = 0;
    vob->demuxer          = -1;
    vob->fixme_a_codec    = CODEC_AC3;    //FIXME
    vob->gamma            = 0.0;
    vob->lame_flush       = 0;
    vob->has_video        = 1;
    vob->has_audio        = 1;
    vob->has_audio_track  = 1;
    vob->lang_code        = 0;
    vob->format_flag      = 0;
    vob->codec_flag       = 0;
    vob->im_asr           = 0;
    vob->im_par           = 0;
    vob->im_par_width     = 0;
    vob->im_par_height    = 0;
    vob->ex_asr           = -1;
    vob->ex_par           = 0;
    vob->ex_par_width     = 0;
    vob->ex_par_height    = 0;
    vob->quality          = VQUALITY;
    vob->amod_probed      = NULL;
    vob->vmod_probed      = NULL;
    vob->amod_probed_xml  = NULL;
    vob->vmod_probed_xml  = NULL;
    vob->af6_mode         = 0;
    vob->a_vbr            = 0;
    vob->pts_start        = 0.0f;
    vob->vob_offset       = 0;
    vob->vob_chunk        = 0;
    vob->vob_chunk_max    = 0;
    vob->vob_chunk_num1   = 0;
    vob->vob_chunk_num2   = 0;
    vob->vob_psu_num1     = 0;
    vob->vob_psu_num2     = INT_MAX;
    vob->vob_info_file    = NULL;
    vob->vob_percentage   = 0;
    vob->im_a_string      = NULL;
    vob->im_v_string      = NULL;
    vob->ex_a_string      = NULL;
    vob->ex_v_string      = NULL;

    vob->reduce_h         = 1;
    vob->reduce_w         = 1;

    //-Z
    vob->zoom_width       = 0;
    vob->zoom_height      = 0;
    vob->zoom_filter      = Lanczos3_filter;
    vob->zoom_support     = Lanczos3_support;

    vob->frame_interval   = 1; // write every frame 
    
    // v4l capture
    vob->chanid           = chanid;
    vob->station_id       = NULL;

    //anti-alias
    vob->aa_weight        = TC_DEFAULT_AAWEIGHT;
    vob->aa_bias          = TC_DEFAULT_AABIAS;

    vob->a52_mode         = 0;
    vob->encode_fields    = 0;

    vob->ttime            = NULL;
    vob->ttime_current    = 0;

    vob->accel            = ac_cpuinfo();

    vob->psu_offset       = 0.0f;
    vob->bitreservoir     = TC_TRUE;
    vob->lame_preset      = NULL;

    vob->ts_pid1          = 0x0;
    vob->ts_pid2          = 0x0;

    vob->video_frames_delay = 0;

    vob->dv_yuy2_mode     = 0;
    vob->hard_fps_flag    = 0;
    vob->mpeg_profile     = PROF_NONE;

    // prepare for SIGINT to catch
    
    //signal(SIGINT, SIG_IGN);
    
    sigemptyset(&sigs_to_block);
    
    sigaddset(&sigs_to_block,  SIGINT);
    sigaddset(&sigs_to_block,  SIGTERM);
    // enabling this breaks the import_vob module.
    //sigaddset(&sigs_to_block,  SIGPIPE);
    
    pthread_sigmask(SIG_BLOCK, &sigs_to_block, NULL);
    
    // start the signal handler thread     
    if(pthread_create(&thread_signal, NULL, (void *) signal_thread, NULL)!=0)
      tc_error("failed to start signal handler thread");
    tc_signal_thread=1;

    /* writepid is set in signal_thread */
    pthread_mutex_lock(&writepid_mutex);
    while (writepid == 0)
      pthread_cond_wait(&writepid_cond, &writepid_mutex);
    pthread_mutex_unlock(&writepid_mutex);

    // close all threads at exit
    atexit(safe_exit);
    
    /* ------------------------------------------------------------ 
     *
     * (II) parse command line
     *
     * ------------------------------------------------------------*/

    while ((ch1 = getopt_long_only(argc, argv, "n:m:y:h?u:i:o:a:t:p:f:zdkr:j:w:b:c:x:s:e:g:q:vlD:AV1B:Z:C:I:KP:T:U:L:Q:R:J:F:E:S:M:Y:G:OX:H:N:W:", long_options, &option_index)) != -1) {

	switch (ch1) {
	  
	case 'T': 
	  
	  if (4 == sscanf(optarg,"%d,%d-%d,%d", &vob->dvd_title, &vob->dvd_chapter1, &vob->dvd_chapter2, &vob->dvd_angle));
  	  else if (3 == sscanf(optarg,"%d,%d-%d", &vob->dvd_title, &vob->dvd_chapter1, &vob->dvd_chapter2));
	  else {
	      n = sscanf(optarg,"%d,%d,%d", &vob->dvd_title, &vob->dvd_chapter1, &vob->dvd_angle);
	      if(n<0 || n>3) tc_error("invalid parameter for option -T\n");
	  }
	  if(vob->dvd_chapter2!=-1 && vob->dvd_chapter2 < vob->dvd_chapter1) {
	      tc_error("invalid parameter for option -T\n");
	  }

	  if(verbose & TC_DEBUG) fprintf(stderr, "T=%d title=%d ch1=%d ch2=%d angle=%d\n", n, vob->dvd_title, vob->dvd_chapter1, vob->dvd_chapter2, vob->dvd_angle);
	  break;
	  
	case 't':
	    
	  if(optarg[0]=='-') usage(EXIT_FAILURE);

	  n = sscanf(optarg,"%d,%s", &splitavi_frames, base);

	  if (!n) tc_error("invalid parameter for option -t");
	  
	  if(splitavi_frames <= 0) tc_error("invalid parameter for option -t");
	  
	  core_mode=TC_MODE_AVI_SPLIT;

	  break;
	  
	case 'W':
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  
	  n=sscanf(optarg,"%d,%d,%s", &vob->vob_chunk, &vob->vob_chunk_max, vob_logfile); 
	  
	  if(n==3) vob->vob_info_file=vob_logfile;
	  
	  tc_cluster_mode = TC_ON;

	  break;
	  
	case 's': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  
	  if ((n = sscanf(optarg,"%lf,%lf,%lf,%lf", &vob->volume, &vob->ac3_gain[0], &vob->ac3_gain[1], &vob->ac3_gain[2]))<0) usage(EXIT_FAILURE);
	  
	  if(vob->volume<0) tc_error("invalid parameter for option -s");
	  
	  for(; n < 4; ++n)
	    vob->ac3_gain[n - 1] = 1.0;
	  
	  break;
	  
	case 'L': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  vob->vob_offset = atoi(optarg);
	  
	  if(vob->vob_offset<0) tc_error("invalid parameter for option -L");
	  
	  break;
	  
	case 'O':
	  
	  vob->lame_flush=TC_ON;
	  break;
	  
	case 'H':
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  seek_range = atoi(optarg);
	  if(seek_range==0) auto_probe=0;
	  
	  break;
	  
	case 'v': 
	  
	  version();
	  exit(0);
	  break;
	  
	case 'z': 
	  
	  flip = TC_TRUE;
	  break;
	  
	  
	case 'K': 
	  
	  decolor = TC_TRUE;
	  vob->decolor = TC_TRUE;
	  break;
	  
	case 'M': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  vob->demuxer = atoi(optarg);
	  
	  preset_flag |= TC_PROBE_NO_DEMUX;
	  
	  //only 0-4 allowed:
	  if(vob->demuxer>4) tc_error("invalid parameter for option -M");

	  break;
	  
	case 'G': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  vob->gamma = atof(optarg);
	  
	  if(vob->gamma<0.0) tc_error("invalid parameter for option -G");
	  
	  dgamma = TC_TRUE;
	  
	  break;
	  
	case 'A': 
	  vob->im_a_codec=CODEC_AC3;	
	  break;
	  
	case 'V': 
	  fprintf(stderr, "*** WARNING: The option -V is deprecated. ***\n"
	      "*** Transcode internal frame handling is now in YV12 / YUV420 ***\n"
	      "*** format by default because most codecs can only handle this format, ***\n"
	      "*** otherwise leading to unnecessary time and quality wasting conversions. ***\n"
	      "*** If you want to have to \"old\" behaviour (RGB24 as internal format), ***\n"
	      "*** then please use the new -1/--use_rgb option ***\n");
	  // vob->im_v_codec=CODEC_YUV;
	  break;

	case '1': 
	  vob->im_v_codec = CODEC_RGB;
	  break;
	  
	case 'l': 
	  
	  mirror = TC_TRUE;
	  break;
	  
	case 'C': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  vob->antialias  = atoi(optarg);
	  
	  break;
	  
	case 'Q': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  
	  n = sscanf(optarg,"%d,%d", &vob->divxquality, &vob->quality);
	  
	  if(n<0 || vob->divxquality<0 || vob->quality<0) tc_error("invalid parameter for option -Q");
	  
	  break;
	  
	case 'E': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);

	  n = sscanf(optarg,"%d,%d,%d", &vob->mp3frequency, &vob->dm_bits, &vob->dm_chan);
	  
	  if(n < 0 || vob->mp3frequency < 0)  
	    tc_error("invalid parameter for option -E");
	  
	  if(n>2 && (vob->dm_chan < 0 || vob->dm_chan > 6)) tc_error("invalid parameter for option -E");
	  
	  if(n>1 && vob->dm_bits != 0 && vob->dm_bits != 8 && vob->dm_bits != 16 && vob->dm_bits != 24) tc_error("invalid parameter for option -E");

	  switch (n) {
	    case 3: probe_export_attributes |= TC_PROBE_NO_EXPORT_ACHANS;
	    case 2: probe_export_attributes |= TC_PROBE_NO_EXPORT_ABITS;
	    case 1: probe_export_attributes |= TC_PROBE_NO_EXPORT_ARATE;
	  }

	  
	  break;
	  
	case 'R': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  
	  n = sscanf(optarg,"%d,%64[^,],%s", &vob->divxmultipass, vlogfile, alogfile);
	  
	  switch (n) {
	    
	  case 3:
	    vob->audiologfile = alogfile;
	  case 2:
	    vob->divxlogfile = vlogfile;
	    vob->audiologfile = "pcm.log";
	    break;
	  case 1:
	    vob->divxlogfile="divx4.log";
	    break;
	  default:
	    tc_error("invalid parameter for option -R");
	  }
	  
	  if(vob->divxmultipass<0)  
	    tc_error("invalid multi-pass parameter for option -R");
	  
	  break;
	  
	case 'P': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  vob->pass_flag  = atoi(optarg);
	  
	  if(vob->pass_flag < 0 || vob->pass_flag>3)  
	    tc_error("invalid parameter for option -P");
	  
	  break;
	  
	case 'I': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  vob->deinterlace = atoi(optarg);
	  
	  if(vob->deinterlace < 0)
	    tc_error("invalid parameter for option -I");
	  
	  break;
	  
	case 'U': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  
	  chbase = optarg;
	  
	  core_mode=TC_MODE_DVD_CHAPTER;
	  break;
	  
	case 'h': 
	  
	  usage(EXIT_SUCCESS);
	  break;
	  
	case 'q': 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  verbose = atoi(optarg);
	  
	  if(verbose) verbose |= TC_INFO;
	  
	  vob->verbose = verbose;
	  
	  break;
	  
	case 'b': 
	  
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  
	  n = sscanf(optarg,"%d,%d,%f,%d", &vob->mp3bitrate, &vob->a_vbr, &vob->mp3quality, &vob->mp3mode);
	  probe_export_attributes |= TC_PROBE_NO_EXPORT_ABITRATE;

	if(n<0 || vob->mp3bitrate < 0|| vob->a_vbr<0 || vob->mp3quality<-1.00001 || vob->mp3mode<0) 
	  tc_error("invalid bitrate for option -b");
	
	break;

      case 'f': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	
	n = sscanf(optarg,"%lf,%d", &vob->fps, &vob->im_frc);

	if(n==2) vob->fps=MIN_FPS; //will be overwritten later

	if(vob->fps < MIN_FPS || n < 0) tc_error("invalid frame rate for option -f");
	
	preset_flag |= TC_PROBE_NO_FPS;
	preset_flag |= TC_PROBE_NO_FRC;
	
	if(n==2) {
	  if(vob->im_frc < 0 || vob->im_frc > 15) tc_error("invalid frame rate code for option -f");
	  
	  vob->fps = frc_table[vob->im_frc];
	  
	  preset_flag |= TC_PROBE_NO_FRC;
	}
	
	break;

      case 'n': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	vob->fixme_a_codec = strtol(optarg, endptr, 16);

	if(vob->fixme_a_codec < 0) tc_error("invalid parameter for option -n");

	preset_flag |= TC_PROBE_NO_ACODEC;

	break;

      case 'N': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	vob->ex_a_codec = strtol(optarg, endptr, 16);

	if(vob->ex_a_codec < 0) tc_error("invalid parameter for option -N");
	probe_export_attributes |= TC_PROBE_NO_EXPORT_ACODEC;

	break;
	
      case 'w': 
	{	
	float ratefact = 1.0f;

	n = sscanf(optarg,"%f,%d,%d", &ratefact, &vob->divxkeyframes, &vob->divxcrispness);

	switch (n) {
	  
	  // allow divxkeyframes=-1
	case 3:
	  if(vob->divxcrispness < 0 || vob->divxcrispness >100 ) 
	    tc_error("invalid crispness parameter for option -w");
	case 2:
	  probe_export_attributes |= TC_PROBE_NO_EXPORT_GOP;
	case 1:

	  vob->divxbitrate = (int)ratefact;
	  vob->m2v_requant =      ratefact;
	  probe_export_attributes |= TC_PROBE_NO_EXPORT_VBITRATE;
	
	  if(!vob->divxbitrate)
	    tc_error("invalid bitrate parameter for option -w");
	  
	  break;
	default:
	  tc_error("invalid divx parameter for option -w");
	}
	
	}
	break;

      case VIDEO_MAX_BITRATE:
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	n = sscanf(optarg, "%d", &vob->video_max_bitrate);
	if(!n)
	  tc_error("invalid parameter for option --video_max_bitrate");
	break;
	
      case 'r': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);

	if ((n = sscanf(optarg,"%d,%d", &vob->reduce_h, &vob->reduce_w))<0) usage(EXIT_FAILURE);
	
	if(n==1) vob->reduce_w=vob->reduce_h;

	if(vob->reduce_h > 0 || vob->reduce_w > 0) {
	  rescale = TC_TRUE;
	} else
	  tc_error("invalid rescale factor for option -r");

	break;
	
      case 'c': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	fc_ttime_string = optarg;

	break;

      case 'Y': 
	
	if ((n = sscanf(optarg,"%d,%d,%d,%d", &vob->ex_clip_top, &vob->ex_clip_left, &vob->ex_clip_bottom, &vob->ex_clip_right))<0) usage(EXIT_FAILURE);
	
	//symmetrical clipping for only 1-3 arguments
	if(n==1 || n==2) vob->ex_clip_bottom=vob->ex_clip_top;
	if(n==2 || n==3) vob->ex_clip_right=vob->ex_clip_left;
	
	ex_clip=TC_TRUE;
	
	break;
	
	case 'j': 
	  
	  if ((n = sscanf(optarg,"%d,%d,%d,%d", &vob->im_clip_top, &vob->im_clip_left, &vob->im_clip_bottom, &vob->im_clip_right))<0) usage(EXIT_FAILURE);
	  
	  //symmetrical clipping for only 1-3 arguments
	  if(n==1 || n==2) vob->im_clip_bottom=vob->im_clip_top;
	  if(n==2 || n==3) vob->im_clip_right=vob->im_clip_left;
	  
	  im_clip=TC_TRUE;
	  
	  break;
	  
	case 'S': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);

	if((n = sscanf(optarg,"%d,%d-%d", &vob->ps_unit, &vob->ps_seq1, &vob->ps_seq2))<0) usage(EXIT_FAILURE);

	if(vob->ps_unit <0 || vob->ps_seq1 <0 || vob->ps_seq2 <0 || vob->ps_seq1 >= vob->ps_seq2)
	    tc_error("invalid parameter for option -S");

	preset_flag |= TC_PROBE_NO_SEEK;

	break;
	
      case 'g': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	if (2 != sscanf(optarg,"%dx%d", &vob->im_v_width, &vob->im_v_height)) 
	  tc_error("invalid video parameter for option -g");

	// import frame size
	if(vob->im_v_width > TC_MAX_V_FRAME_WIDTH || vob->im_v_height > TC_MAX_V_FRAME_HEIGHT || vob->im_v_width<0 || vob->im_v_height <0)
	  tc_error("invalid video parameter for option -g");
	
	preset_flag |= TC_PROBE_NO_FRAMESIZE;
	
	break;


      case 'J':

	  if(optarg[0]=='-') usage(EXIT_FAILURE);

	  if (!plugins_string) {
            size_plugstr = strlen(optarg) + 2;
	    if (NULL == (plugins_string = (char *)malloc(size_plugstr)))
	      tc_error("Malloc failed for filter string");

	    memset(plugins_string, '\0', size_plugstr);
	    sret = strlcpy(plugins_string, optarg, size_plugstr);
            tc_test_string(__FILE__, __LINE__, size_plugstr, sret, errno);
	  } else {
	    char *curpos;
            size_plugstr = strlen(optarg) + 2 + strlen(plugins_string) + 2;
	    if (NULL == (plugins_string = (char *)realloc(plugins_string, 
								size_plugstr)))
	      tc_error("Realloc failed for filter string");

	    curpos = plugins_string+strlen(plugins_string);
	    *(curpos) = ',';
	    *(curpos+1) = '\0';
	    sret = strlcat(plugins_string, optarg, size_plugstr);
	    tc_test_string(__FILE__, __LINE__, size_plugstr, sret, errno);
	    //fprintf(stderr, "\nFILTER 2 = (%s) (%s)\n", plugins_string, optarg);
	  }

	  break;

	
      case 'y': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	
	if ((n = sscanf(optarg,"%64[^,],%s", vbuf2, abuf2))<=0) tc_error("invalid parameter for option -y");
	
	if(n==1) {
	  ex_aud_mod = vbuf2;
	  ex_vid_mod = vbuf2;
	  no_v_out_codec=0;
	  probe_export_attributes |= TC_PROBE_NO_EXPORT_AMODULE;
	}
	
	if(n==2) {
	  ex_aud_mod = abuf2;
	  ex_vid_mod = vbuf2;
	  no_v_out_codec=no_a_out_codec=0;
	  probe_export_attributes |= (TC_PROBE_NO_EXPORT_AMODULE|TC_PROBE_NO_EXPORT_VMODULE);
	}

	if(strlen(ex_aud_mod)!=0 && strchr(ex_aud_mod,'=') && n==2) {
	  char *t = strchr(optarg, ',');
	  vob->ex_a_string=strchr(t, '=')+1;
	  if (vob->ex_a_string[0] == '\0')
	    tc_error("invalid option string for audio export module");

	  t = strchr(ex_aud_mod, '=');
	  *t = '\0'; 
	}

	if(strlen(ex_vid_mod)!=0 && strchr(ex_vid_mod,'=')) {
	  char *t = strchr(ex_vid_mod, '=');
	  *t = '\0'; // terminate export module

	  vob->ex_v_string=strchr(optarg, '=')+1;
	  if (vob->ex_v_string[0] == '\0' || vob->ex_v_string[0] == ',')
	    tc_error("invalid option string for video export module");

	  t = strchr(optarg, ',');
	  if (t && *t) *t = '\0';
	}
	
	if(n>2) tc_error("invalid parameter for option -y");
	
	break;
	
	
      case 'e': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	
	n = sscanf(optarg,"%d,%d,%d", &vob->a_rate, 
		   &vob->a_bits, &vob->a_chan);
	
	switch (n) {
	  
	case 3:
	  if(!(vob->a_chan == 0 || vob->a_chan == 1 || vob->a_chan == 2 || vob->a_chan == 6))
	    tc_error("invalid pcm parameter 'channels' for option -e");
	  preset_flag |= TC_PROBE_NO_CHAN;
	  
	case 2:
	  if(!(vob->a_bits == 16 || vob->a_bits == 8))
	    tc_error("invalid pcm parameter 'bits' for option -e");
	  preset_flag |= TC_PROBE_NO_BITS;
	case 1:
	  if(vob->a_rate > RATE || vob->a_rate <= 0)
	    tc_error("invalid pcm parameter 'rate' for option -e");
	  preset_flag |= TC_PROBE_NO_RATE;
	  break;
	default:
	  tc_error("invalid pcm parameter set for option -e");
	}
	
	break;
	
      case 'x':
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	
	if ((n = sscanf(optarg,"%64[^,],%s", vbuf1, abuf1))<=0) tc_error("invalid parameter for option -x");
	
	if(n==1) {
	  im_aud_mod = vbuf1;
	  im_vid_mod = vbuf1;
	  no_vin_codec=0;
	}
	
	if(n==2) {
	  im_aud_mod = abuf1;
	  im_vid_mod = vbuf1;
	  no_vin_codec=no_ain_codec=0;
	}
	
	if(n>2) tc_error("invalid parameter for option -x");

	//check for avifile special mode
	if(strlen(im_vid_mod)!=0 && strcmp(im_vid_mod,"af6")==0 && no_ain_codec) vob->af6_mode=1;


	if(strlen(im_aud_mod)!=0 && strchr(im_aud_mod,'=') && n==2) {
	  char *t = strchr(optarg, ',');
	  vob->im_a_string=strchr(t, '=')+1;
	  if (vob->im_a_string[0] == '\0')
	    tc_error("invalid option string for audio import module");

	  t = strchr(im_aud_mod, '=');
	  *t = '\0'; 
	}

	if(strlen(im_vid_mod)!=0 && strchr(im_vid_mod,'=')) {
	  char *t = strchr(im_vid_mod, '=');
	  *t = '\0'; // terminate import module

	  vob->im_v_string=strchr(optarg, '=')+1;
	  if (vob->im_v_string[0] == '\0' || vob->im_v_string[0] == ',')
	    tc_error("invalid option string for video import module");

	  t = strchr(optarg, ',');
	  if (t && *t) *t = '\0';
	}

	// "auto" is just a placeholder */
	if(strlen(im_vid_mod)!=0 && strcmp(im_vid_mod,"auto")==0) {
	  im_vid_mod = NULL;
	  no_vin_codec=1;
	}
	if(strlen(im_aud_mod)!=0 && strcmp(im_aud_mod,"auto")==0) {
	  im_aud_mod = NULL;
	  no_ain_codec=1;
	}

	break;	
	
	
      case 'u':
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);

	if ((n = sscanf(optarg,"%d,%d,%d,%d", &max_frame_buffer, &max_frame_threads, &tc_buffer_delay_dec, &tc_buffer_delay_enc))<=0) tc_error("invalid parameter for option -u");
	
	if(max_frame_buffer < 0 || max_frame_threads < 0 || max_frame_threads > TC_FRAME_THREADS_MAX) tc_error("invalid parameter for option -u");

	preset_flag |= TC_PROBE_NO_BUFFER;
	
	break;
	
      case 'B': 

	if(optarg[0]=='-') usage(EXIT_FAILURE);

        if ((n = sscanf(optarg,"%d,%d,%d", &vob->vert_resize1, &vob->hori_resize1, &vob->resize1_mult))<=0) tc_error("invalid parameter for option -B");	
	if(n==1) vob->hori_resize1=0;
	resize1=TC_TRUE;

	break;

      case 'X': 

	if(optarg[0]=='-') usage(EXIT_FAILURE);

        if ((n = sscanf(optarg,"%d,%d,%d", &vob->vert_resize2, &vob->hori_resize2, &vob->resize2_mult))<=0) tc_error("invalid parameter for option -X");	
	if(n==1) vob->hori_resize2=0;
	resize2=TC_TRUE;

	break;
	
      case 'Z': 

	if(optarg[0]=='-') usage(EXIT_FAILURE);

	{ 
	char *c = strchr(optarg, 'x');

	vob->zoom_width  = 0;
	vob->zoom_height = 0;

	if (isdigit(*optarg) || *optarg == 'x') {

	  if (c && *c && *(c+1)) vob->zoom_height = atoi (c+1);
	  if (*optarg != 'x')    vob->zoom_width = atoi(optarg);
	  zoom = TC_TRUE;

	} else if (*optarg == 'f') {
	  fast_resize = TC_TRUE;
	} else {
	  tc_error("invalid setting for option -Z");
	}

	if(vob->zoom_width > TC_MAX_V_FRAME_WIDTH) 
	  tc_error("invalid width for option -Z");
	if(vob->zoom_height >TC_MAX_V_FRAME_HEIGHT) 
	  tc_error("invalid height for option -Z");

	c = strchr(optarg, ',');
	if (c && *c && *(c+1) && !strncmp(c+1, "fast", 1)) fast_resize = TC_TRUE;

	}

	break;
	
      case 'k': 
	
	rgbswap=TC_TRUE;
	break;
	
      case 'd': 
	
	pcmswap=TC_TRUE;
	break;
	
      case 'a': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	
	if ((n = sscanf(optarg,"%d,%d", &vob->a_track, &vob->v_track))<=0) tc_error("invalid parameter for option -a");
	
	preset_flag |= TC_PROBE_NO_TRACK;
	
	break;
	
      case 'i':
	
	//if(optarg[0]=='-') usage(EXIT_FAILURE);
	
	video_in_file=optarg;
	vob->video_in_file = optarg;

	if(source_check(video_in_file)) exit(1);
	break;
	
      case 'p':
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	
	audio_in_file = optarg;
	vob->audio_in_file = optarg;
	
	if(source_check(audio_in_file)) exit(1);
	
	vob->in_flag = 1; 
	break;


      case 'F':
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	
        {
          char *p2 = strchr(optarg,',');
          if(p2 != NULL) {
            *p2 = '\0';
            vob->ex_a_fcc = p2+1;
	    {
	      char *p3 = strchr(vob->ex_a_fcc,',');
	      if(p3 != NULL) {
		*p3 = '\0';
		vob->ex_profile_name = p3+1;
	      }
	    }
          }
          vob->ex_v_fcc = optarg;
        }
	break;
	probe_export_attributes |= TC_PROBE_NO_EXPORT_VCODEC;
	
      case 'o': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);

	video_out_file = optarg;
	vob->video_out_file = optarg;
	break;

      case 'm':

	if(optarg[0]=='-') usage(EXIT_FAILURE);
	audio_out_file = optarg;
	vob->audio_out_file = optarg;
	
	vob->out_flag = 1; 
	break;

      case 'D':

	if (1 != sscanf(optarg,"%d", &vob->sync)) 
	  tc_error("invalid parameter for option -D");

	sync_seconds = vob->sync; // save for later;
	preset_flag |= TC_PROBE_NO_AVSHIFT;

	break;
	
	case ZOOM_FILTER:

	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  
	  if(optarg && strlen(optarg) > 0) {
	    
	    if(strcasecmp(optarg,"bell")==0) {
	      vob->zoom_filter=Bell_filter;
	      vob->zoom_support=Bell_support; 
	      zoom_filter="Bell";
	      break;
	    }
	    
	    if(strcasecmp(optarg,"box")==0) {
	      vob->zoom_filter=Box_filter;
	      vob->zoom_support=Box_support; 
	      zoom_filter="Box";
	      break;
	    }
	    
	    //default
	    if(strncasecmp(optarg,"lanczos3",1)==0) {
	      vob->zoom_filter=Lanczos3_filter;
	      vob->zoom_support=Lanczos3_support; 
	      zoom_filter="Lanczos3";
	      break;
	    }
	    
	    if(strncasecmp(optarg,"mitchell",1)==0) {
	      vob->zoom_filter=Mitchell_filter;
	      vob->zoom_support=Mitchell_support; 
	      zoom_filter="Mitchell";
	      break;
	    }

	    if(strncasecmp(optarg,"hermite",1)==0) {
	      vob->zoom_filter=Hermite_filter;
	      vob->zoom_support=Hermite_support; 
	      zoom_filter="Hermite";
	      break;
	    }

	    if(strncasecmp(optarg,"B_spline",1)==0) {
	      vob->zoom_filter=B_spline_filter;
	      vob->zoom_support=B_spline_support; 
	      zoom_filter="B_spline";
	      break;
	    }
	    
	    if(strncasecmp(optarg,"triangle",1)==0) {
	      vob->zoom_filter=Triangle_filter;
	      vob->zoom_support=Triangle_support; 
	      zoom_filter="Triangle";
	      break;
	    }
	    tc_error("invalid argument for --zoom_filter option\nmethod: L[anczos] M[itchell] T[riangle] H[ermite] B[_spline] bell box");
	    tc_error("invalid filter selection for --zoom_filter");
	    
	  } else  tc_error("invalid parameter for option --zoom_filter");
	  
	  break;

	case USE_YUV422:
	  vob->im_v_codec = CODEC_YUV422;
	  break;

	case CLUSTER_PERCENTAGE:
	  vob->vob_percentage=1;
	  break;

	case CLUSTER_CHUNKS:
	    if(optarg[0]=='-') usage(EXIT_FAILURE);
	    
	    if (2 != sscanf(optarg,"%d-%d", &vob->vob_chunk_num1, &vob->vob_chunk_num2)) 
		tc_error("invalid parameter for option --cluster_chunks");	  
	  
	    if(vob->vob_chunk_num1 < 0 || vob->vob_chunk_num2 <=0 || vob->vob_chunk_num1 >= vob->vob_chunk_num2) tc_error("invalid parameter selection for option --cluster_chunks");
	  break;

	case EXPORT_ASR:

	    if(optarg[0]=='-') usage(EXIT_FAILURE);
	    vob->ex_asr=atoi(optarg);

	    if(vob->ex_asr < 0) tc_error("invalid parameter for option --export_asr");

	    probe_export_attributes |= TC_PROBE_NO_EXPORT_ASR;
	    
	    break;

	case IMPORT_ASR:

	    if(optarg[0]=='-') usage(EXIT_FAILURE);
	    vob->im_asr=atoi(optarg);

	    if(vob->im_asr < 0) tc_error("invalid parameter for option --import_asr");

	    preset_flag |= TC_PROBE_NO_IMASR;
	    
	    break;

	case EXPORT_FPS:

	    if(optarg[0]=='-') usage(EXIT_FAILURE);
	    n = sscanf(optarg,"%lf,%d", &vob->ex_fps, &vob->ex_frc);

	    if(n==1) vob->ex_frc = 0;
	    if(n==2) vob->ex_fps=MIN_FPS; //will be overwritten later

	    if(vob->ex_fps < MIN_FPS || n < 0) tc_error("invalid parameter for option --export_fps");
	    probe_export_attributes |= TC_PROBE_NO_EXPORT_FPS;
	
	    if(n==2) {
	      if(vob->ex_frc < 0 || vob->ex_frc > 15) tc_error("invalid frame rate code for option --export_fps");
	  
	      probe_export_attributes |= TC_PROBE_NO_EXPORT_FRC;

	      vob->ex_fps = frc_table[vob->ex_frc];
	    }
	
	    break;
	  
	case EXPORT_FRC:

	    if(optarg[0]=='-') usage(EXIT_FAILURE);
	    vob->ex_frc=atoi(optarg);

	    if(vob->ex_frc<0) tc_error("invalid parameter for option --export_frc");
	    probe_export_attributes |= TC_PROBE_NO_EXPORT_FRC;
	    
	    break;
	  
	case DIVX_QUANT:
	    if(optarg[0]=='-') usage(EXIT_FAILURE);

	    if (sscanf(optarg,"%d,%d", &vob->min_quantizer, &vob->max_quantizer)<0) tc_error("invalid parameter for option --divx_quant");

	    break;

	case DIVX_RC:
	    if(optarg[0]=='-') usage(EXIT_FAILURE);

	    if (sscanf(optarg,"%d,%d,%d", &vob->rc_period, &vob->rc_reaction_period, &vob->rc_reaction_ratio )<0) tc_error("invalid parameter for option --divx_rc");

	    break;

	case DIVX5_VBV_PROF:
	    if(optarg[0]=='-') usage(EXIT_FAILURE);

	    if (sscanf(optarg,"%d", &vob->divx5_vbv_prof)<0) 
	      tc_error("invalid parameter for option --divx_vbv_prof");
	    if (vob->divx5_vbv_prof <0 || vob->divx5_vbv_prof >4)
	      tc_error("invalid value (%d) for option --divx_vbv_prof (0-4)", vob->divx5_vbv_prof);

	    break;

	case DIVX5_VBV:
	    if(optarg[0]=='-') usage(EXIT_FAILURE);

	    if (sscanf(optarg,"%d,%d,%d", &vob->divx5_vbv_bitrate, 
		                          &vob->divx5_vbv_size, 
					  &vob->divx5_vbv_occupancy )<0) 
	      tc_error("invalid parameters for option --divx_vbv");

	    break;

	case IMPORT_V4L:
	  if( ( n = sscanf( optarg, "%d,%s", &chanid, station_id ) ) == 0 )
	    tc_error( "invalid parameter for option --import_v4l" );
	  
	  vob->chanid = chanid;
	  
	  if( n > 1 )
	    vob->station_id = station_id;
	  
	  break;


	case RECORD_V4L:
	  tc_error ("--record_v4l is deprecated, please use -c 0:0:s1-0:0:s2");

	  if((n = sscanf( optarg, "%d-%d", &frame_asec, &frame_bsec) ) != 2 )
	    tc_error( "invalid parameter for option --record_v4l" );
	  
	  if(frame_bsec<=frame_asec) tc_error( "invalid parameter for option --record_v4l" );
	  
	  break;
	  
	case ANTIALIAS_PARA:
	  if((n = sscanf( optarg, "%lf,%lf", &vob->aa_weight, &vob->aa_bias)) == 0 ) tc_error( "invalid parameter for option --antialias_para");
	  
	  if(vob->aa_weight<0.0f || vob->aa_weight>1.0f) tc_error( "invalid weight parameter w for option --antialias_para (0.0<w<1.0)");
	  if(vob->aa_bias<0.0f || vob->aa_bias>1.0f) tc_error( "invalid bias parameter b for option --antialias_para (0.0<b<1.0)");

	  break;
	  
	case CONFIG_DIR:
	  tc_config_dir = optarg;
	  break;

	case PULLDOWN:
	  vob->pulldown = 1;
	  break;

	case KEEP_ASR:
	  keepasr = TC_TRUE;
	  break;

	case ENCODE_FIELDS:
	  if(optarg[0]=='-') {
	    tc_error("option --encode_fields requires an argument (one of p, t, b or u)");
	  }
	  switch (optarg[0]) {
	    case 'p':
	      vob->encode_fields = 0; break;
	    case 't':
	      vob->encode_fields = 1; break;
	    case 'b':
	      vob->encode_fields = 2; break;
	    case 'u':
	      vob->encode_fields = 3; break;
	    default:
	      tc_error("option --encode_fields argument must be one of p, t, b or u");
	      break;
	  }
	  probe_export_attributes |= TC_PROBE_NO_EXPORT_FIELDS;
	  
	  break;

	case DVD_ACCESS_DELAY:
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  tc_dvd_access_delay = atoi(optarg);
	  break;

	case HARD_FPS:
	  vob->hard_fps_flag = TC_TRUE;
	  break;

	case DV_YUY2_MODE:
	  vob->dv_yuy2_mode = TC_TRUE;
	  break;

	case TS_PID:

	  if(optarg[0]=='-') usage(EXIT_FAILURE);

	  vob->ts_pid1 = strtol(optarg, endptr, 16);
	  vob->ts_pid2 = vob->ts_pid1;
	  break;

	case NICENESS:
	  
	  tc_niceness = atoi(optarg);

	  break;

	case WRITE_PID:
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  
	  if (optarg) {
	    FILE *f = fopen(optarg,"w");
	    if (f) {
	      fprintf (f, "%d\n", writepid);
	      fclose (f);
	    }
	  }
	  break;
	  
	case NO_AUDIO_ADJUST:
	  no_audio_adjust=TC_TRUE;
	  break;

	case NO_BITRESERVOIR:
	  vob->bitreservoir=TC_FALSE;
	  break;

	case LAME_PRESET:
	  if (optarg && strlen(optarg)) {
	    vob->lame_preset = optarg;
	  } else {
	    tc_error ("--lame_preset: invalid preset\n");
	  }

	  
	  break;

	case AV_FINE_MS:
	  vob->sync_ms=atoi(optarg);
	  preset_flag |= TC_PROBE_NO_AV_FINE;
	  break;
	  
	case MORE_HELP:
	  printf( "more help for " );
	  
	  if( strncmp( optarg, "import_v4l", TC_BUF_MIN ) == 0 ) {
	    printf( "import_v4l\n" );
	    import_v4l_usage();
	  }

 	  if( strncmp( optarg, "duration", TC_BUF_MIN ) == 0 ) {
	    printf( "duration\n" );
	    duration_usage();
	  }

	  printf( "none\n" );
	  usage(EXIT_FAILURE);
	  
	  break;

	case DURATION:
	  tc_error ("--duration is deprecated, please use -c 0-hh:mm:ss");

	  if( ( n = sscanf( optarg, "%d:%d:%d", &hh, &mm, &ss ) ) == 0 ) usage(EXIT_FAILURE);
	  
	  frame_a = 0;
	  
	  switch( n ) {
	  case 1:  // record for hh seconds
	    frame_b = vob->fps * hh;
	    break;
	  case 2:  // record for hh minutes and mm seconds
	    frame_b = vob->fps * 60 * hh + vob->fps * mm;
	    break;
	  case 3:  // record for hh hours, mm minutes and ss seconds
	    frame_b = vob->fps * 3600 * hh + vob->fps * 60 * mm + vob->fps * ss;
	    break;
	  }

	  if(frame_b-1 < frame_a) tc_error("invalid frame range for option --duration");

	  counter_set_range(frame_a, frame_b);
	  
	  break;
	  
	case NAV_SEEK:
	  vob->nav_seek_file = nav_seek_file = optarg;
	  break;
	  
	case NO_SPLIT:
	  no_split=TC_TRUE;
	  break;

	case A52_DRC_OFF:
	  vob->a52_mode |=TC_A52_DRC_OFF;
	  break;

	case A52_DOLBY_OFF:
	  vob->a52_mode |=TC_A52_DOLBY_OFF;
	  break;

	case A52_DEMUX:
	  vob->a52_mode |=TC_A52_DEMUX;
	  break;
	
	case PSU_MODE: 
	  psu_mode = TC_TRUE;
	  core_mode=TC_MODE_PSU;
	  tc_cluster_mode = TC_ON;
	  break;

	case FRAME_INTERVAL:
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  if((n = sscanf( optarg, "%u", &vob->frame_interval) ) < 0) 
	    usage(EXIT_FAILURE);

	  break;
	  
	case DIR_MODE: 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  
	  dirbase = optarg;

	  core_mode=TC_MODE_DIRECTORY;
	  
	  break;

#if defined(ARCH_X86) || defined(ARCH_X86_64)
	case ACCEL_MODE:
	  accel = optarg;
	  if(accel == NULL) usage(EXIT_FAILURE);
	  if(accel[0] == '-') usage(EXIT_FAILURE);

	  tc_accel = 0;
	  while (accel) {
	    char *comma = strchr(accel, ',');
	    if(comma)
	      *comma++ = 0;
	    if(strcasecmp(accel, "C") == 0)  // dummy for "no accel"
	      tc_accel |= 0;
#ifdef ARCH_X86
	    else if(strcasecmp(accel, "asm"     ) == 0)
	      tc_accel |= AC_IA32ASM;
#endif
#ifdef ARCH_X86_64
	    else if(strcasecmp(accel, "asm"     ) == 0)
	      tc_accel |= AC_AMD64ASM;
#endif
	    else if(strcasecmp(accel, "mmx"     ) == 0)
	      tc_accel |= AC_MMX;
	    else if(strcasecmp(accel, "mmxext"  ) == 0)
	      tc_accel |= AC_MMXEXT;
	    else if(strcasecmp(accel, "3dnow"   ) == 0)
	      tc_accel |= AC_3DNOW;
	    else if(strcasecmp(accel, "3dnowext") == 0)
	      tc_accel |= AC_3DNOWEXT;
	    else if(strcasecmp(accel, "sse"     ) == 0)
	      tc_accel |= AC_SSE;
	    else if(strcasecmp(accel, "sse2"    ) == 0)
	      tc_accel |= AC_SSE2;
	    else if(strcasecmp(accel, "sse3"    ) == 0)
	      tc_accel |= AC_SSE3;
	    else {
	      fprintf(stderr, "bad --accel type, valid types: C asm mmx mmxext 3dnow 3dnowext sse sse2 sse3\n\n");
	      short_usage(EXIT_FAILURE);
	    }
	    accel = comma;
	  }
	  break;
#else
	case ACCEL_MODE: 
	  // not supported
	  break;
#endif

	case AVI_LIMIT:
	  if(optarg[0]=='-') usage(EXIT_FAILURE);

	  tc_avi_limit=atoi(optarg);

	  if (tc_avi_limit <= 0) {
	    tc_avi_limit = (unsigned int)-1;
	  }

	  break;

	case AVI_COMMENTS:
	  if(optarg[0]=='-') usage(EXIT_FAILURE);

	  if ( (vob->avi_comment_fd = xio_open(optarg, O_RDONLY)) == -1)
	    tc_error ("Cannot open comment file \"%s\"", optarg);
	    

	    break;


	case DEBUG_MODE: 

	  core_mode=TC_MODE_DEBUG;

	  break;
	  
	case PSU_CHUNKS: 
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  
	  if((n = sscanf( optarg, "%d-%d,%d", &vob->vob_psu_num1, &vob->vob_psu_num2, &psu_frame_threshold) ) < 0) usage(EXIT_FAILURE);

	  break;

	case PRE_CLIP: 
	  
	  if ((n = sscanf(optarg,"%d,%d,%d,%d", &vob->pre_im_clip_top, &vob->pre_im_clip_left, &vob->pre_im_clip_bottom, &vob->pre_im_clip_right))<0) usage(EXIT_FAILURE);
	  
	  //symmetrical clipping for only 1-3 arguments
	  if(n==1 || n==2) vob->pre_im_clip_bottom=vob->pre_im_clip_top;
	  if(n==2 || n==3) vob->pre_im_clip_right=vob->pre_im_clip_left;
	  
	  pre_im_clip=TC_TRUE;
	  
	  break;

	case POST_CLIP: 
	  
	  if ((n = sscanf(optarg,"%d,%d,%d,%d", &vob->post_ex_clip_top, &vob->post_ex_clip_left, &vob->post_ex_clip_bottom, &vob->post_ex_clip_right))<0) usage(EXIT_FAILURE);
	  
	  //symmetrical clipping for only 1-3 arguments
	  if(n==1 || n==2) vob->post_ex_clip_bottom=vob->post_ex_clip_top;
	  if(n==2 || n==3) vob->post_ex_clip_right=vob->post_ex_clip_left;
	  
	  post_ex_clip=TC_TRUE;
	  
	  break;
	  
	case PRINT_STATUS:
	  if( ( n = sscanf( optarg, "%d,%d", &print_counter_interval, &print_counter_cr ) ) == 0 )
	    tc_error( "invalid parameter for option --print_status" );
	  
	  break;

	case COLOR_LEVEL:
	  if( ( n = sscanf( optarg, "%d", &color_level) ) == 0 )
	    tc_error( "invalid parameter for option --color_level" );

	  // --color
	  if (isatty(STDOUT_FILENO)==0 || isatty(STDERR_FILENO)==0 || color_level == 0) {
	    RED    = "";
	    GREEN  = "";
	    YELLOW = "";
	    WHITE  = "";
	    GRAY   = "";
	    BLUE   = "";
	  }
	  
	  break;
	case PROGRESS_OFF:
	  tc_progress_meter = TC_OFF;
	  break;

	case SOCKET_FILE:
	  socket_file = optarg;
	  break;

	case EX_PIXEL_ASPECT: 
	  if(optarg[0]=='-') usage(EXIT_FAILURE);

	  n = sscanf(optarg, "%d,%d", &vob->ex_par_width, &vob->ex_par_height);
	  if (n == 1) {
	    vob->ex_par = vob->ex_par_width;
	    vob->ex_par_width = vob->ex_par_height = 0;
	    if (vob->ex_par < 0 || vob->ex_par > 5) {
	      tc_error("--ex_par must be between 0 and 5");
	    }
	    
	    switch (vob->ex_par) {
	      case 1: vob->ex_par_width =  1; vob->ex_par_height =  1; break;
	      case 2: vob->ex_par_width = 12; vob->ex_par_height = 11; break;
	      case 3: vob->ex_par_width = 10; vob->ex_par_height = 11; break;
	      case 4: vob->ex_par_width = 16; vob->ex_par_height = 11; break;
	      case 5: vob->ex_par_width = 40; vob->ex_par_height = 33; break;
	      default: case 0: vob->ex_par_width = 0; vob->ex_par_height = 1; break;
	    }

	  } else if (n == 2) {
	    vob->ex_par = 0;
	  } else {
	    tc_error("invalid argument for --ex_par");
	  }

	  probe_export_attributes |= TC_PROBE_NO_EXPORT_PAR;
	  break;

	case EXTENSIONS:
	  if(optarg[0]=='-') usage(EXIT_FAILURE);

	  {
	    char *c;
	    
	    c = strchr (optarg, ',');
	    if (!c) { // no amod
	      video_ext = optarg;
	      probe_export_attributes |= TC_PROBE_NO_EXPORT_VEXT;
	    } else if (c == optarg) { // only amod
	      audio_ext = c+1;
	      probe_export_attributes |= TC_PROBE_NO_EXPORT_AEXT;
	    } else { // both
	      *c = '\0';
	      video_ext = optarg;
	      audio_ext = c+1;
	      probe_export_attributes |= TC_PROBE_NO_EXPORT_AEXT;
	      probe_export_attributes |= TC_PROBE_NO_EXPORT_VEXT;
	    }
	    if ( !strcmp(video_ext, "none")) video_ext = "";
	    if ( !strcmp(video_ext, "null")) video_ext = "";
	    if ( !strcmp(audio_ext, "none")) audio_ext = "";
	    if ( !strcmp(audio_ext, "null")) audio_ext = "";
	    
	  }
	  break;

	case EXPORT_PROF:
	  if(optarg[0]=='-') usage(EXIT_FAILURE);

	if          (strncasecmp(optarg, "vcd", 3) == 0) {
	  vob->mpeg_profile = VCD; // need to guess later if pal or ntsc
	  if        (strncasecmp(optarg, "vcd-pal", 7) == 0) {
	    vob->mpeg_profile = VCD_PAL;
	  } else if (strncasecmp(optarg, "vcd-ntsc", 8) == 0) {
	    vob->mpeg_profile = VCD_NTSC;
	  } else if (strncasecmp(optarg, "vcd-secam", 9) == 0) {
	    vob->mpeg_profile = VCD_PAL;
	  }
	} else  if  (strncasecmp(optarg, "svcd", 4) == 0) {
	  vob->mpeg_profile = SVCD;
	  if        (strncasecmp(optarg, "svcd-pal", 8) == 0) {
	    vob->mpeg_profile = SVCD_PAL;
	  } else if (strncasecmp(optarg, "svcd-ntsc", 9) == 0) {
	    vob->mpeg_profile = SVCD_NTSC;
	  } else if (strncasecmp(optarg, "svcd-secam", 10) == 0) {
	    vob->mpeg_profile = SVCD_PAL;
	  }
	} else  if  (strncasecmp(optarg, "dvd", 3) == 0) {
	  vob->mpeg_profile = DVD;
	  if        (strncasecmp(optarg, "dvd-pal", 7) == 0) {
	    vob->mpeg_profile = DVD_PAL;
	  } else if (strncasecmp(optarg, "dvd-ntsc", 8) == 0) {
	    vob->mpeg_profile = DVD_NTSC;
	  } else if (strncasecmp(optarg, "dvd-secam", 9) == 0) {
	    vob->mpeg_profile = DVD_PAL;
	  }
	} else  if  (strncasecmp(optarg, "xvcd", 4) == 0) {
	  vob->mpeg_profile = XVCD;
	  if        (strncasecmp(optarg, "xvcd-pal", 8) == 0) {
	    vob->mpeg_profile = XVCD_PAL;
	  } else if (strncasecmp(optarg, "xvcd-ntsc", 9) == 0) {
	    vob->mpeg_profile = XVCD_NTSC;
	  } else if (strncasecmp(optarg, "xvcd-secam", 10) == 0) {
	    vob->mpeg_profile = XVCD_PAL;
	  }
	} else {
	  tc_error("invalid setting for option --export_prof");
	}
			

	  break;

	default:
	  short_usage(EXIT_FAILURE);
	  break;
	}
    }

#if !defined (__APPLE__)
    if(optind < argc) {
      tc_warn("unused command line parameter detected (%d/%d)", optind, argc);
      
      for(n=optind; n<argc; ++n) tc_warn("argc[%d]=%s (unused)", n, argv[n]);
      
      if(optind==1) short_usage(EXIT_SUCCESS);
      
    }
#endif

    if ( psu_mode ) {
      
      if(video_out_file==NULL) tc_error("please specify output file name for psu mode");

      if(!strchr(video_out_file, '%') && !no_split) {
	char *suffix = strrchr(video_out_file, '.');
	if(suffix && 0 == strcmp(".avi", suffix)) {
	  *suffix = '\0';
	}
	psubase = malloc(PATH_MAX);
	snprintf(psubase, PATH_MAX, "%s-psu%%02d%s", video_out_file, video_ext);
      } else {
	psubase = video_out_file;
      }
    }

      /* ------------------------------------------------------------ 
       *
       * (III) auto probe properties of input stream
       *
       * ------------------------------------------------------------*/

    // user doesn't want to start at all;-(
    if(sig_int) goto summary;

    // display program version
    if(verbose) version();

    if (tc_niceness) {
      if (nice(tc_niceness)<0) {
	tc_warn("setting nice to %d failed", tc_niceness);
      }
    }

    // this will determine most source parameter
    if(auto_probe) {

      // interface to "tcprobe"
      probe_source(&preset_flag, vob, seek_range, video_in_file, audio_in_file);

      if(verbose) {
	
	printf("[%s] %s %s (%s%s%s)\n", PACKAGE, "auto-probing source", 
	    ((video_in_file==NULL)? audio_in_file:video_in_file), 
	    ((preset_flag == TC_PROBE_ERROR)?RED:GREEN),
	    ((preset_flag == TC_PROBE_ERROR)?"failed":"ok"), GRAY);
	
	printf("[%s] V: %-16s | %s %s (V=%s|A=%s)\n", PACKAGE, "import format", codec2str(vob->codec_flag), mformat2str(vob->format_flag), 
	       ((no_vin_codec==0)?im_vid_mod:vob->vmod_probed),
	       ((no_ain_codec==0)?im_aud_mod:vob->amod_probed));
      }
    }
#ifdef HAVE_LIBXML2
#define TCXML_MAX_BUFF 1024
    if (vob->vmod_probed_xml && strstr(vob->vmod_probed_xml,"xml") != NULL)
    {
	if (vob->video_in_file && strstr(vob->video_in_file,"/dev/zero") ==NULL)
	{
	      	p_tcxmlcheck_buffer=(char *)calloc(TCXML_MAX_BUFF,1);
		if ((snprintf(p_tcxmlcheck_buffer, TCXML_MAX_BUFF, "tcxmlcheck -i \"%s\" -S -B -V",vob->video_in_file))<0)
		{
		  perror("command buffer overflow");
		  exit(1);
		}
		if (verbose & TC_DEBUG) printf("XML check out video: %s\n", p_tcxmlcheck_buffer);
    		if((p_fd_tcxmlcheck = popen(p_tcxmlcheck_buffer, "w"))== NULL)
		{
		  fprintf(stderr,"[%s] Error opening pipe\n",PACKAGE);
		  exit(1);
		}
		if ((fwrite((char *)vob,sizeof(vob_t), 1, p_fd_tcxmlcheck))!=1)
		{
		  fprintf(stderr,"[%s] Error writing data to tcxmlcheck\n",PACKAGE);
		  exit(1);
		}
		pclose (p_fd_tcxmlcheck);
		memset(p_tcxmlcheck_buffer, 0 ,TCXML_MAX_BUFF);
		if ((snprintf(p_tcxmlcheck_buffer, TCXML_MAX_BUFF, "tcxmlcheck -i \"%s\" -B -V",vob->video_in_file))<0)
		{
		  perror("command buffer overflow");
		  exit(1);
		}
		if (verbose & TC_DEBUG) printf("XML check in video: %s\n", p_tcxmlcheck_buffer);
    		if((p_fd_tcxmlcheck = popen(p_tcxmlcheck_buffer, "r"))== NULL)
		{
		  fprintf(stderr,"[%s] Error opening pipe\n",PACKAGE);
		  exit(1);
		}
		if ((read(fileno(p_fd_tcxmlcheck),(char *)vob,sizeof(vob_t)))!=sizeof(vob_t))
		{
		  fprintf(stderr,"[%s] Error reading data from tcxmlcheck\n",PACKAGE);
		  exit(1);
		}
		if ((read(fileno(p_fd_tcxmlcheck),&s_tcxmlcheck_resize,sizeof(int)))!=sizeof(int))
		{
		  fprintf(stderr,"[%s] Error reading data from tcxmlcheck 2\n",PACKAGE);
		  exit(1);
		}
		pclose(p_fd_tcxmlcheck);
		if (s_tcxmlcheck_resize == 2)	//if the xml force the resize i need to disable the parameter passed from command line
		{
			resize1=TC_FALSE;	
			resize2=TC_FALSE;
			zoom=TC_FALSE;
			vob->resize1_mult     = 32;
			vob->vert_resize1     = 0;
			vob->hori_resize1     = 0;
			vob->resize2_mult     = 32;
			vob->vert_resize2     = 0;
			vob->hori_resize2     = 0;
			vob->zoom_width       = 0;
			vob->zoom_height      = 0;
			vob->zoom_filter      = Lanczos3_filter;
			vob->zoom_support     = Lanczos3_support;
		}
		free(p_tcxmlcheck_buffer);
    	}
    }
    if (vob->amod_probed_xml && strstr(vob->amod_probed_xml,"xml") != NULL)
    {
	if (vob->audio_in_file && strstr(vob->audio_in_file,"/dev/zero") ==NULL)
	{
      		p_tcxmlcheck_buffer=(char *)calloc(TCXML_MAX_BUFF,1);
		if ((snprintf(p_tcxmlcheck_buffer, TCXML_MAX_BUFF, "tcxmlcheck -p %s -S -B -A",vob->audio_in_file))<0)
		{
	  		perror("command buffer overflow");
	  		exit(1);
		}
		if (verbose & TC_DEBUG) printf("XML check out audio: %s\n", p_tcxmlcheck_buffer);
	    	if((p_fd_tcxmlcheck = popen(p_tcxmlcheck_buffer, "w"))== NULL)
		{
		  fprintf(stderr,"[%s] Error opening pipe\n",PACKAGE);
		  exit(1);
		}
		if ((write(fileno(p_fd_tcxmlcheck),(char *)vob,sizeof(vob_t)))!=sizeof(vob_t))
		{
		  fprintf(stderr,"[%s] Error writing data to stdout\n",PACKAGE);
		  exit(1);
		}
		pclose(p_fd_tcxmlcheck);
		memset(p_tcxmlcheck_buffer,0 ,TCXML_MAX_BUFF);
		if ((snprintf(p_tcxmlcheck_buffer, TCXML_MAX_BUFF, "tcxmlcheck -p %s -B -A",vob->audio_in_file))<0)
		{
		  perror("command buffer overflow");
		  exit(1);
		}
		if (verbose & TC_DEBUG) printf("XML check in audio: %s\n", p_tcxmlcheck_buffer);
    		if((p_fd_tcxmlcheck = popen(p_tcxmlcheck_buffer, "r"))== NULL)
		{
		  fprintf(stderr,"[%s] Error opening pipe\n",PACKAGE);
		  exit(1);
		}
		if ((read(fileno(p_fd_tcxmlcheck),(char *)vob,sizeof(vob_t)))!=sizeof(vob_t))
		{
		  fprintf(stderr,"[%s] Error reading data to stdout\n",PACKAGE);
		  exit(1);
		}
		if ((read(fileno(p_fd_tcxmlcheck),&s_tcxmlcheck_resize,sizeof(int)))!=sizeof(int))
		{
		  fprintf(stderr,"[%s] Error reading data to stdout\n",PACKAGE);
		  exit(1);
		}
		pclose(p_fd_tcxmlcheck);
		free(p_tcxmlcheck_buffer);
	}
    }
#endif 
    /* ------------------------------------------------------------ 
     *
     * (IV) autosplit stream for cluster processing
     *
     * currently, only VOB streams are supported
     *
     * ------------------------------------------------------------*/

    // get -c from string
    if (fc_ttime_string) {
      if( parse_fc_time_string( fc_ttime_string, vob->fps, 
	    fc_ttime_separator, (verbose>1?1:0), &vob->ttime ) == -1 )
	usage(EXIT_FAILURE);

      frame_a = vob->ttime->stf;
      frame_b = vob->ttime->etf;
      vob->ttime->vob_offset = 0;

      tstart=vob->ttime;
      counter_set_range(frame_a, frame_b);
      counter_on(); //activate
    } else {
      vob->ttime = new_fc_time();
      frame_a = vob->ttime->stf = TC_FRAME_FIRST;
      frame_b = vob->ttime->etf = TC_FRAME_LAST;
      vob->ttime->vob_offset = 0;
      tstart = vob->ttime;
      tstart->next = NULL;
    }

    // determine -S,-c,-L option parameter for distributed processing
    if(nav_seek_file) {
      FILE *fp;
      struct fc_time *tmptime;
      char buf[80];
      int line_count;
      int flag = 0;
      int is_aviindex = 0;
      
      if(vob->vob_offset) {
	tc_warn("-L and --nav_seek are incompatible.");
      }
      
      if(NULL == (fp = fopen(nav_seek_file, "r"))) {
	perror(nav_seek_file);
	exit(EXIT_FAILURE);
      }

      tmptime = vob->ttime;
      line_count = 0;

      // check if this is an AVIIDX1 file
      if (fgets(buf, sizeof(buf), fp)) {
	if(strncasecmp(buf, "AVIIDX1", 7) == 0) is_aviindex=1;
	fseek(fp, 0, SEEK_SET);
      } else {
	tc_error("An error happend while reading the nav_seek file");
      }

      if (!is_aviindex) {
      while(tmptime){
        flag=0;
        if(verbose & TC_DEBUG) printf("searching %s for frame %d\n", nav_seek_file, tmptime->stf);
        for(; fgets(buf, sizeof(buf), fp); line_count++) {
  	  int L, new_frame_a;
    	
	  if(2 == sscanf(buf, "%*d %*d %*d %*d %d %d ", &L, &new_frame_a)) {
	      if(line_count == tmptime->stf) {
		  int len = tmptime->etf - tmptime->stf;
		  if(verbose & TC_DEBUG) printf("%s: -c %d-%d -> -L %d -c %d-%d\n", nav_seek_file, tmptime->stf, tmptime->etf, L, new_frame_a, new_frame_a+len);
		  tmptime->stf = frame_a = new_frame_a;
		  tmptime->etf = frame_b = new_frame_a + len;
		  tmptime->vob_offset = L;
		  flag=1;
		  ++line_count;
		  break;
	      }
  	  }
        }
	tmptime = tmptime->next;
      }
      } else { // is_aviindex==1

      fgets(buf, sizeof(buf), fp); // magic
      fgets(buf, sizeof(buf), fp); // comment

      while(tmptime){
	int new_frame_a, type, key;
	long chunk, chunkptype, last_keyframe=0;
	off_t pos, len;
	char tag[4];
	double ms=0.0;
        flag=0;
	  
        if(verbose & TC_DEBUG) printf("searching %s for frame %d\n", nav_seek_file, tmptime->stf);
        for(; fgets(buf, sizeof(buf), fp); line_count++) {
    	
	  // TAG TYPE CHUNK CHUNK/TYPE POS LEN KEY MS
	  if(sscanf(buf, "%s %d %ld %ld %lld %lld %d %lf", 
		      tag, &type, &chunk, &chunkptype, &pos, &len, &key, &ms))
	  {
	    if (type!=1) continue;
	    if(key) {
	      //printf("type (%d) key (%d) chunkptype(%ld)\n", type, key, chunkptype);
	      last_keyframe = chunkptype;
	    }
	    if(chunkptype == tmptime->stf) {
	      int lenf = tmptime->etf - tmptime->stf;
	      new_frame_a = chunkptype - last_keyframe;
	      
	      // If we are doing pass-through, we cannot skip frames, but only start
	      // passthrough on a keyframe boundary. At least, we respect the
	      // last frame the user whishes.
	      if(vob->pass_flag & TC_VIDEO) {
		new_frame_a = 0;
		lenf += (chunkptype - last_keyframe);
	      }

	      if(verbose & TC_DEBUG) 
		printf("%s: -c %d-%d -> -L %ld -c %d-%d\n", 
		    nav_seek_file, tmptime->stf, tmptime->etf, last_keyframe, new_frame_a, new_frame_a+lenf);
	      tmptime->stf = frame_a = new_frame_a;
	      tmptime->etf = frame_b = new_frame_a + lenf;
	      tmptime->vob_offset = last_keyframe;
	      flag=1;
	      ++line_count;
	      break;
	    }
	  }
	}
	tmptime = tmptime->next;
      }

      }
      fclose(fp);

      if(!flag) {
	  //frame not found
	  printf("%s: frame %d out of range (%d frames found)\n", nav_seek_file, frame_a, line_count);
	  tc_error("invalid option parameter for -c / --nav_seek\n");
      }
    }
    
    if(vob->vob_chunk_max) {
      
      int this_unit=-1;
      
      //overwrite tcprobe's unit preset:
      if(preset_flag & TC_PROBE_NO_SEEK) this_unit=vob->ps_unit;
      
      if(split_stream(vob, vob->vob_info_file, this_unit, &frame_a, &frame_b, 1)<0) tc_error("cluster mode option -W error");
      
      counter_set_range(frame_a, frame_b);
    }
    
    /* ------------------------------------------------------------ 
     *
     * some sanity checks for command line parameters
     *
     * ------------------------------------------------------------*/

    // -M

    if(vob->demuxer!=-1 && (verbose & TC_INFO)) {

      switch(vob->demuxer) {
	
      case 0:
	printf("[%s] V: %-16s | %s\n", PACKAGE, "AV demux/sync", "(0) sync AV at PTS start - demuxer disabled");
	break;

      case 1:
	printf("[%s] V: %-16s | %s\n", PACKAGE, "AV demux/sync", "(1) sync AV at initial MPEG sequence"); 
	break;
	
      case 2:
	printf("[%s] V: %-16s | %s\n", PACKAGE, "AV demux/sync", "(2) initial MPEG sequence / enforce frame rate");
	break;
	
      case 3:
	printf("[%s] V: %-16s | %s\n", PACKAGE, "AV demux/sync", "(3) sync AV at initial PTS"); 
	break;

      case 4:
	printf("[%s] V: %-16s | %s\n", PACKAGE, "AV demux/sync", "(4) initial PTS / enforce frame rate");
	break;
      }
    } else vob->demuxer=1;
    
    
    // -P

    if(vob->pass_flag & TC_VIDEO) {
      
      vob->im_v_codec = (vob->im_v_codec==CODEC_YUV) ? CODEC_RAW_YUV:CODEC_RAW;
      vob->ex_v_codec=CODEC_RAW;
      
      //suggestion:
      if(no_v_out_codec) ex_vid_mod="raw";
      no_v_out_codec=0;

      if(no_a_out_codec) ex_aud_mod="raw";
      no_a_out_codec=0;
      
      if(verbose & TC_INFO) printf("[%s] V: %-16s | yes\n", PACKAGE, "pass-through");
    }


    // -x
    
    if(no_vin_codec && video_in_file!=NULL && vob->vmod_probed==NULL) 
	tc_warn("no option -x found, option -i ignored, reading from \"/dev/zero\"");
    
    
    //overwrite results of autoprobing if modules are provided
    if(no_vin_codec && vob->vmod_probed!=NULL) {
        im_vid_mod=vob->vmod_probed_xml;                //need to load the correct module if the input file type is xml
    }

    if(no_ain_codec && vob->amod_probed!=NULL) {
        im_aud_mod=vob->amod_probed_xml;                //need to load the correct module if the input file type is xml
    }

    // make zero frame size default for no video
    if(im_vid_mod != NULL && strcmp(im_vid_mod, "null")==0) vob->im_v_width=vob->im_v_height=0;
    
    //initial aspect ratio
    asr = (double) vob->im_v_width/vob->im_v_height;

    // -g

    // import size
    // force to even for YUV mode
    if(vob->im_v_codec == CODEC_YUV || vob->im_v_codec == CODEC_YUV422) {
	if(vob->im_v_width%2 != 0) {
	    tc_warn("frame width must be even in YUV/YUV422 mode");
	    vob->im_v_width--;
	}
	if(vob->im_v_codec == CODEC_YUV && vob->im_v_height%2 != 0) {
	    tc_warn("frame height must be even in YUV mode");
	    vob->im_v_height--;
	}
    }
    if(verbose & TC_INFO) {
      (vob->im_v_width && vob->im_v_height) ?
	printf("[%s] V: %-16s | %03dx%03d  %4.2f:1  %s\n", PACKAGE, "import frame", vob->im_v_width, vob->im_v_height, asr, asr2str(vob->im_asr)):printf("[%s] V: %-16s | disabled\n", PACKAGE, "import frame");
    }
    
    // init frame size with cmd line frame size
    vob->ex_v_height = vob->im_v_height;
    vob->ex_v_width  = vob->im_v_width;
    
    // import bytes per frame (RGB 24bits)
    vob->im_v_size   = vob->im_v_height * vob->im_v_width * vob->v_bpp/8;
    // export bytes per frame (RGB 24bits)
    vob->ex_v_size   = vob->im_v_size;

    //2003-01-13 
    tc_adjust_frame_buffer(vob->ex_v_height, vob->ex_v_width);

    // calc clip settings for encoding to mpeg (vcd,svcd,xvcd,dvd)
    // --export_prof {vcd,vcd-pal,vcd-ntsc,svcd,svcd-pal,svcd-ntsc,dvd,dvd-pal,dvd-ntsc}
   
    if (vob->mpeg_profile != PROF_NONE) {
      typedef struct ratio_t { int t, b; } ratio_t;
      ratio_t asrs[] = { {1, 1}, {1, 1}, {4, 3}, {16, 9}, {221, 100}, {250, 100}, {125, 100}};
      ratio_t imasr = asrs[0];
      ratio_t exasr = asrs[0];

      int impal = 0;
      int pre_clip;

      //if(vob->im_v_codec == CODEC_RGB)
	// vob->im_v_codec = CODEC_YUV; // mpeg is always YUV // will this always do?

      // Make an educated guess if this is pal or ntsc
      switch (vob->mpeg_profile) {
      case VCD:
      case SVCD:
      case XVCD:
      case DVD:
          if (vob->im_v_height == 288 || vob->im_v_height == 576) impal = 1;
          if ((int)vob->fps == 25 || vob->im_frc == 3) impal = 1;
          break;
      case VCD_PAL:
      case SVCD_PAL:
      case XVCD_PAL:
      case DVD_PAL:
          impal = 1;
          break;
      default:
          break;
      }

      // choose height dependent on pal or NTSC.
      switch (vob->mpeg_profile) {
	case VCD_PAL: case VCD_NTSC:
	case VCD: vob->zoom_height = impal?288:240;
		  break;

	case SVCD_PAL: case SVCD_NTSC: case SVCD:
	case XVCD_PAL: case XVCD_NTSC: case XVCD:
	case DVD_PAL: case DVD_NTSC:
	case DVD: vob->zoom_height = impal?576:480;
		  break;

	default:
		  break;
      }

      // choose width if not set by user.
      switch (vob->mpeg_profile) {
	case VCD_PAL: case VCD_NTSC: case VCD: 
	  if (!vob->zoom_width) vob->zoom_width = 352;
	  vob->ex_asr = 2;
	  break;
	case SVCD_PAL: case SVCD_NTSC: case SVCD:
	case XVCD_PAL: case XVCD_NTSC: case XVCD:
	  if (!vob->zoom_width) vob->zoom_width = 480;
	  vob->ex_asr = 2;
	  break;
	case DVD_PAL: case DVD_NTSC: case DVD:
	  if (!vob->zoom_width) vob->zoom_width = 720;
	  if (vob->ex_asr <= 0) vob->ex_asr = 2; // assume 4:3
		  break;
	default:
		  break;
      }


      // an input file without any aspect ratio setting (an AVI maybe?)
      // so make a guess.

      if (vob->im_asr == 0) {

	int i, mini=0;
	ratio_t *r = &asrs[1];
	double diffs[6];
	double mindiff = 2.0;

	memset (diffs, 0, sizeof(diffs));
	
	for (i=0; i<6; i++) {
	  diffs[i] = (double)(r->b*vob->im_v_width) / (double)(r->t*vob->im_v_height);
	  r++;
	}

	// look for the diff which is closest to 1.0

	for (i=0; i<6; i++) {
	  double a = fabs(1.0 - diffs[i]);
	  if (a < mindiff) {
	    mindiff = a;
	    mini = i+1;
	  }
	}
	vob->im_asr = mini;
      }

      imasr = asrs[vob->im_asr];
      exasr = asrs[vob->ex_asr];

      pre_clip = vob->im_v_height - (vob->im_v_height * imasr.t * exasr.b ) / (imasr.b * exasr.t );

      if(pre_im_clip == TC_FALSE) {

	if (pre_clip%2 != 0) {
	  vob->pre_im_clip_top = pre_clip/2+1;
	  vob->pre_im_clip_bottom = pre_clip/2-1;
	} else {
	  vob->pre_im_clip_bottom = vob->pre_im_clip_top = pre_clip/2;
	}
	if (vob->pre_im_clip_top%2 != 0 || vob->pre_im_clip_bottom%2 != 0) {
	  vob->pre_im_clip_top--;
	  vob->pre_im_clip_bottom++;
	}
      }

      //FIXME hack, kludge, etc. EMS
      if( (vob->im_v_height != vob->zoom_height) || 
	  ((vob->im_v_width != vob->zoom_width) && (vob->ex_v_width != 704)))
	zoom = TC_TRUE;
      else 
	zoom = TC_FALSE;

      if (pre_clip) pre_im_clip = TC_TRUE;

      printf("XXX: zoom=%s pre_clip=%s\n", zoom?"yes":"no", pre_im_clip?"yes":"no");

      // shall we really go this far?
      // If yes, there can be much more settings adjusted.
      if (ex_vid_mod == NULL || !strcmp(ex_vid_mod, "mpeg2enc")) {
#ifdef HAVE_MJPEGTOOLS
	if(!ex_aud_mod)
	    ex_aud_mod = "mp2enc";
	no_v_out_codec=0;
	ex_vid_mod = "mpeg2enc";
	//FIXME this should be in export_mpeg2enc.c
	if(!vob->ex_v_fcc) {
	  switch (vob->mpeg_profile) {
	    case VCD_PAL: case VCD_NTSC: case VCD: 
	      vob->ex_v_fcc = "1";
	      break;
	    case SVCD_PAL: case SVCD_NTSC: case SVCD:
		case XVCD_PAL: case XVCD_NTSC: case XVCD:
	      vob->ex_v_fcc = "4";
	      break;
	    case DVD_PAL: case DVD_NTSC: case DVD:
	      vob->ex_v_fcc = "8";
	      break;
	    default: break;
	  }
	}
#endif
      } else if(!strcmp(ex_vid_mod, "ffmpeg")) {

	if(!ex_aud_mod)
	  ex_aud_mod = "ffmpeg";

	switch(vob->mpeg_profile)
	{
	  case(VCD):		vob->ex_v_fcc = "vcd";		break;
	  case(VCD_PAL):	vob->ex_v_fcc = "vcd-pal";	break;
	  case(VCD_NTSC):	vob->ex_v_fcc = "vcd-ntsc";	break;
	  case(SVCD):		vob->ex_v_fcc = "svcd";		break;
	  case(SVCD_PAL):	vob->ex_v_fcc = "svcd-pal";	break;
	  case(SVCD_NTSC):	vob->ex_v_fcc = "svcd-ntsc";break;
	  case(XVCD):		vob->ex_v_fcc = "xvcd";		break;
	  case(XVCD_PAL):	vob->ex_v_fcc = "xvcd-pal";	break;
	  case(XVCD_NTSC):	vob->ex_v_fcc = "xvcd-ntsc";break;
	  case(DVD):		vob->ex_v_fcc = "dvd";		break;
	  case(DVD_PAL):	vob->ex_v_fcc = "dvd-pal";	break;
	  case(DVD_NTSC):	vob->ex_v_fcc = "dvd-ntsc";	break;
	  case(PROF_NONE):					break;
	}
      } // ffmpeg

      if (ex_aud_mod == NULL) {
#ifdef HAVE_MJPEGTOOLS
	  no_a_out_codec=0;
	  ex_aud_mod = "mp2enc";
#endif
	}
    } // mpeg_profile != PROF_NONE


    // --PRE_CLIP
    
    if(pre_im_clip) {
      
      // force to even for YUV mode
      if(vob->im_v_codec == CODEC_YUV || vob->im_v_codec == CODEC_YUV422) {
	if(vob->pre_im_clip_left%2 != 0) {
	    tc_warn("left/right pre_clip must be even in YUV/YUV422 mode");
	    vob->pre_im_clip_left--;
	}
	if(vob->pre_im_clip_right%2 != 0) {
	    tc_warn("left/right pre_clip must be even in YUV/YUV422 mode");
	    vob->pre_im_clip_right--;
	}
	if(vob->im_v_codec == CODEC_YUV && vob->pre_im_clip_top%2 != 0) {
	    tc_warn("top/bottom pre_clip must be even in YUV mode");
	    vob->pre_im_clip_top--;
	}
	if(vob->im_v_codec == CODEC_YUV && vob->pre_im_clip_bottom%2 != 0) {
	    tc_warn("top/bottom pre_clip must be even in YUV mode");
	    vob->pre_im_clip_bottom--;
	}
      }

      //check against import parameter, this is pre processing!
      
      if(vob->ex_v_height - vob->pre_im_clip_top - vob->pre_im_clip_bottom <= 0 ||
	 vob->ex_v_height - vob->pre_im_clip_top - vob->pre_im_clip_bottom > TC_MAX_V_FRAME_HEIGHT) tc_error("invalid top/bottom clip parameter for option --pre_clip");
      
      if( vob->ex_v_width - vob->pre_im_clip_left - vob->pre_im_clip_right <= 0 ||    
	  vob->ex_v_width - vob->pre_im_clip_left - vob->pre_im_clip_right > TC_MAX_V_FRAME_WIDTH) tc_error("invalid left/right clip parameter for option --pre_clip");
      
      vob->ex_v_height -= (vob->pre_im_clip_top + vob->pre_im_clip_bottom);
      vob->ex_v_width  -= (vob->pre_im_clip_left + vob->pre_im_clip_right);
      
      if(verbose & TC_INFO) printf("[%s] V: %-16s | %03dx%03d (%d,%d,%d,%d)\n", PACKAGE, 
	  "pre clip frame", vob->ex_v_width, vob->ex_v_height,
	  vob->pre_im_clip_top, vob->pre_im_clip_left, 
	  vob->pre_im_clip_bottom, vob->pre_im_clip_right);
    
      //2003-01-13 
      tc_adjust_frame_buffer(vob->ex_v_height, vob->ex_v_width);
    }


    // -j
    
    if(im_clip) {
      
      // force to even for YUV mode
      if(vob->im_v_codec == CODEC_YUV || vob->im_v_codec == CODEC_YUV422) {
	if(vob->im_clip_left%2 != 0) {
	    tc_warn("left/right clip must be even in YUV/YUV422 mode");
	    vob->im_clip_left--;
	}
	if(vob->im_clip_right%2 != 0) {
	    tc_warn("left/right clip must be even in YUV/YUV422 mode");
	    vob->im_clip_right--;
	}
	if(vob->im_v_codec == CODEC_YUV && vob->im_clip_top%2 != 0) {
	    tc_warn("top/bottom clip must be even in YUV mode");
	    vob->im_clip_top--;
	}
	if(vob->im_v_codec == CODEC_YUV && vob->im_clip_bottom%2 != 0) {
	    tc_warn("top/bottom clip must be even in YUV mode");
	    vob->im_clip_bottom--;
	}
      }

      if(vob->ex_v_height - vob->im_clip_top - vob->im_clip_bottom <= 0 ||
	 vob->ex_v_height - vob->im_clip_top - vob->im_clip_bottom > TC_MAX_V_FRAME_HEIGHT) tc_error("invalid top/bottom clip parameter for option -j");
      
      if( vob->ex_v_width - vob->im_clip_left - vob->im_clip_right <= 0 ||    
	  vob->ex_v_width - vob->im_clip_left - vob->im_clip_right > TC_MAX_V_FRAME_WIDTH) tc_error("invalid left/right clip parameter for option -j");

      vob->ex_v_height -= (vob->im_clip_top + vob->im_clip_bottom);
      vob->ex_v_width  -= (vob->im_clip_left + vob->im_clip_right);
      
      if(verbose & TC_INFO) printf("[%s] V: %-16s | %03dx%03d\n", PACKAGE, "clip frame (<-)", vob->ex_v_width, vob->ex_v_height);
    
      //2003-01-13 
      tc_adjust_frame_buffer(vob->ex_v_height, vob->ex_v_width);
    }
    
    
    // -I

    if(verbose & TC_INFO) {
      
      switch(vob->deinterlace) {
	
      case 0:
	break;
	
      case 1:
	printf("[%s] V: %-16s | (mode=1) interpolate scanlines (fast)\n", PACKAGE, "de-interlace");
	break;
	
      case 2:
	printf("[%s] V: %-16s | (mode=2) handled by encoder (if available)\n", PACKAGE, "de-interlace");
	break;
	
      case 3:
	printf("[%s] V: %-16s | (mode=3) zoom to full frame (slow)\n", PACKAGE, "de-interlace");
	break;
	
      case 4:
	printf("[%s] V: %-16s | (mode=4) drop field / half height (fast)\n", PACKAGE, "de-interlace");
	break;
	
      case 5:
	printf("[%s] V: %-16s | (mode=5) interpolate scanlines / blend frames\n", PACKAGE, "de-interlace");
	break;

      default:
	tc_error("invalid parameter for option -I");
	break;
      }
    }
    
    if(vob->deinterlace==4) vob->ex_v_height /= 2;
 
    // Calculate the missing w or h based on the ASR
    if (zoom && (vob->zoom_width == 0 || vob->zoom_height == 0)) {

      typedef struct ratio_t { int t, b; } ratio_t;
      enum missing_t { NONE, CALC_W, CALC_H, ALL } missing = ALL;
      ratio_t asrs[] = { {1, 1}, {1, 1}, {4, 3}, {16, 9}, {221, 100} };
      ratio_t asr = asrs[0];
      float oldr;

      // check if we have at least on width or height
      if (vob->zoom_width==0 && vob->zoom_height==0) missing = ALL;
      else if (vob->zoom_width==0 && vob->zoom_height>0) missing = CALC_W;
      else if (vob->zoom_width>0 && vob->zoom_height==0) missing = CALC_H;
      else if (vob->zoom_width>0 && vob->zoom_height>0) missing = NONE;

      // try import
      if (vob->im_asr>0 && vob->im_asr<5) asr = asrs[vob->im_asr];
      // try the export aspectratio
      else if (vob->ex_asr>0 && vob->ex_asr<5) asr = asrs[vob->ex_asr];

      switch (missing) {
	case ALL: 
	  tc_error("Neither zoom width nor height set, can't guess anything"); 
	case CALC_W: 
	  vob->zoom_width = vob->zoom_height * asr.t; vob->zoom_width /= asr.b;
	  break;
	case CALC_H: 
	  vob->zoom_height = vob->zoom_width * asr.b; vob->zoom_height /= asr.t;
	  break;
	case NONE: default:
	  /* can't happen */
	  break;
      }

      // for error printout
      oldr = (float)vob->zoom_width/(float)vob->zoom_height;

      // align
      if (vob->zoom_height%8 != 0) vob->zoom_height += 8-(vob->zoom_height%8);
      if (vob->zoom_width%8 != 0) vob->zoom_width += 8-(vob->zoom_width%8);
      //printf("New %%8s %d X %d\n", vob->zoom_width, vob->zoom_height);
      oldr = ((float)vob->zoom_width/(float)vob->zoom_height-oldr)*100.0;
      oldr = oldr<0?-oldr:oldr;

      printf("[%s] V: %-16s | %03dx%03d  %4.2f:1 error %.2f%%\n", 
	  PACKAGE, "auto resize", vob->zoom_width, vob->zoom_height,
	  (float)vob->zoom_width/(float)vob->zoom_height, oldr);

    }

    // -Z ...,fast
    if (fast_resize) {

      int nw, nh, ow, oh;
      int Bw, Bh, Xw, Xh;
      int dw, dh, M=0;

      ow = vob->ex_v_width;
      oh = vob->ex_v_height;
      nw = vob->zoom_width;
      nh = vob->zoom_height;

      // -B: new = old - n*M
      // (-1)*n = (new - old)/M 

      dw = nw - ow;
      if (dw%8 == 0) M = 8;
      if (!M) fast_resize = 0;

      M = 0;
      dh = nh - oh;
      if (dh%8 == 0) M = 8;
      if (!M) fast_resize = 0; 

      if (fast_resize) {

	Bw = dw/M;
	Bh = dh/M;

	if        (Bw < 0 && Bh < 0) {
	  resize1 = TC_TRUE;
	  resize2 = TC_FALSE;
	  Xh = Xw = 0;
	  Bw = -Bw;
	  Bh = -Bh;
	} else if (Bw < 0 && Bh >= 0) {
	  resize1 = TC_TRUE;
	  resize2 = TC_TRUE;
	  Bw = -Bw;
	  Xh = Bh;
	  Xw = Bh = 0;
	} else if (Bw >= 0 && Bh < 0) {
	  resize1 = TC_TRUE;
	  resize2 = TC_TRUE;
	  Bh = -Bh;
	  Xw = Bw;
	  Bw = Xh = 0;
	} else {
	  resize1 = TC_FALSE;
	  resize2 = TC_TRUE;
	  Xw = Bw;
	  Xh = Bh;
	  Bh = Bw = 0;
	}

	vob->resize1_mult = 8;
	vob->resize2_mult = 8;
	vob->vert_resize1 = Bh;
	vob->hori_resize1 = Bw;
	vob->vert_resize2 = Xh;
	vob->hori_resize2 = Xw;
	vob->zoom_width   = 0;
	vob->zoom_height  = 0;

	if (Bw == 0 && Bh == 0) resize1 = TC_FALSE;
	if (Xw == 0 && Xh == 0) resize2 = TC_FALSE;

	if(verbose & TC_INFO)
	printf("[%s] V: %-16s | Using -B %d,%d,8 -X %d,%d,8\n", 
	    PACKAGE, "fast resize", Bh, Bw, Xh, Xw);

	zoom = TC_FALSE;

      } else {
	if(verbose & TC_INFO)
	printf("[%s] V: %-16s | requested but can't be used (W or H mod 8 != 0)\n", 
	    PACKAGE, "fast resize");
      }

    }
    
    // -X

    if(resize2) {
        
        if(vob->resize2_mult % 8 != 0)
            tc_error("resize multiplier for option -X is not a multiple of 8");

        // works only for frame dimension beeing an integral multiple of vob->resize2_mult:


        if(vob->vert_resize2 && (vob->vert_resize2 * vob->resize2_mult + vob->ex_v_height) % vob->resize2_mult != 0)
            tc_error("invalid frame height for option -X, check also option -j");

        if(vob->hori_resize2 && (vob->hori_resize2 * vob->resize2_mult + vob->ex_v_width) % vob->resize2_mult != 0) 
            tc_error("invalid frame width for option -X, check also option -j");
	
        vob->ex_v_height += (vob->vert_resize2 * vob->resize2_mult);
        vob->ex_v_width += (vob->hori_resize2 * vob->resize2_mult);

	//check2:

	if(vob->ex_v_height > TC_MAX_V_FRAME_HEIGHT || vob->ex_v_width >TC_MAX_V_FRAME_WIDTH)
	    tc_error("invalid resize parameter for option -X");

	if(vob->vert_resize2 <0 || vob->hori_resize2 < 0)
	    tc_error("invalid resize parameter for option -X");

	//new aspect ratio:
        asr *= (double) vob->ex_v_width * (vob->ex_v_height - vob->vert_resize2*vob->resize2_mult)/((vob->ex_v_width - vob->hori_resize2*vob->resize2_mult) * vob->ex_v_height);

        vob->vert_resize2 *= (vob->resize2_mult/8);
        vob->hori_resize2 *= (vob->resize2_mult/8);

	if(verbose & TC_INFO && vob->ex_v_height>0) printf("[%s] V: %-16s | %03dx%03d  %4.2f:1 (-X)\n", PACKAGE, "new aspect ratio", vob->ex_v_width, vob->ex_v_height, asr);

      //2003-01-13 
      tc_adjust_frame_buffer(vob->ex_v_height, vob->ex_v_width);
    } 

    
    // -B

    if(resize1) {


        if(vob->resize1_mult % 8 != 0)
            tc_error("resize multiplier for option -B is not a multiple of 8");

        // works only for frame dimension beeing an integral multiple of vob->resize1_mult:

        if(vob->vert_resize1 && (vob->ex_v_height - vob->vert_resize1*vob->resize1_mult) % vob->resize1_mult != 0)
            tc_error("invalid frame height for option -B, check also option -j");
	
        if(vob->hori_resize1 && (vob->ex_v_width - vob->hori_resize1*vob->resize1_mult) % vob->resize1_mult != 0)
	    tc_error("invalid frame width for option -B, check also option -j");
	
        vob->ex_v_height -= (vob->vert_resize1 * vob->resize1_mult);
        vob->ex_v_width -= (vob->hori_resize1 * vob->resize1_mult);

	//check:

	if(vob->vert_resize1 < 0 || vob->hori_resize1 < 0)
	    tc_error("invalid resize parameter for option -B");

	//new aspect ratio:
        asr *= (double) vob->ex_v_width * (vob->ex_v_height + vob->vert_resize1*vob->resize1_mult)/((vob->ex_v_width + vob->hori_resize1*vob->resize1_mult) * vob->ex_v_height);
	
        vob->vert_resize1 *= (vob->resize1_mult/8);
        vob->hori_resize1 *= (vob->resize1_mult/8);

	if(verbose & TC_INFO && vob->ex_v_height>0) printf("[%s] V: %-16s | %03dx%03d  %4.2f:1 (-B)\n", PACKAGE, "new aspect ratio", vob->ex_v_width, vob->ex_v_height, asr);

      //2003-01-13 
      tc_adjust_frame_buffer(vob->ex_v_height, vob->ex_v_width);
    } 


    // -Z

    if(zoom) {

	//new aspect ratio:
	asr *= (double) vob->zoom_width*vob->ex_v_height/(vob->ex_v_width * vob->zoom_height);

        vob->ex_v_width  = vob->zoom_width;
        vob->ex_v_height = vob->zoom_height;

        if(verbose & TC_INFO && vob->ex_v_height>0 ) printf("[%s] V: %-16s | %03dx%03d  %4.2f:1 (%s)\n", PACKAGE, "zoom", vob->ex_v_width, vob->ex_v_height, asr, zoom_filter);

      //2003-01-13 
      tc_adjust_frame_buffer(vob->ex_v_height, vob->ex_v_width);
    }


    // -Y

    if(ex_clip) {

      // force to even for YUV mode
      if(vob->im_v_codec == CODEC_YUV || vob->im_v_codec == CODEC_YUV422) {
	if(vob->ex_clip_left%2 != 0) {
	    tc_warn("left/right clip must be even in YUV/YUV422 mode");
	    vob->ex_clip_left--;
	}
	if(vob->ex_clip_right%2 != 0) {
	    tc_warn("left/right clip must be even in YUV/YUV422 mode");
	    vob->ex_clip_right--;
	}
	if(vob->im_v_codec == CODEC_YUV && vob->ex_clip_top%2 != 0) {
	    tc_warn("top/bottom clip must be even in YUV mode");
	    vob->ex_clip_top--;
	}
	if(vob->im_v_codec == CODEC_YUV && vob->ex_clip_bottom%2 != 0) {
	    tc_warn("top/bottom clip must be even in YUV mode");
	    vob->ex_clip_bottom--;
	}
      }

      //check against export parameter, this is post processing!
	
	if(vob->ex_v_height - vob->ex_clip_top - vob->ex_clip_bottom <= 0 ||
	   vob->ex_v_height - vob->ex_clip_top - vob->ex_clip_bottom > TC_MAX_V_FRAME_HEIGHT) tc_error("invalid top/bottom clip parameter for option -Y");
	
	if(vob->ex_v_width - vob->ex_clip_left - vob->ex_clip_right <= 0 ||
	   vob->ex_v_width - vob->ex_clip_left - vob->ex_clip_right > TC_MAX_V_FRAME_WIDTH) tc_error("invalid left/right clip parameter for option -Y");
	
      vob->ex_v_height -= (vob->ex_clip_top + vob->ex_clip_bottom);
      vob->ex_v_width -= (vob->ex_clip_left + vob->ex_clip_right);

      if(verbose & TC_INFO) printf("[%s] V: %-16s | %03dx%03d\n", PACKAGE, "clip frame (->)", vob->ex_v_width, vob->ex_v_height);

      //2003-01-13 
      tc_adjust_frame_buffer(vob->ex_v_height, vob->ex_v_width);
    }
    
    // -r
  
    if(rescale) {
	
      vob->ex_v_height /= vob->reduce_h;
      vob->ex_v_width /= vob->reduce_w;

      //new aspect ratio:
      asr *= (double)vob->ex_v_width/vob->ex_v_height*(vob->reduce_h*vob->ex_v_height)/(vob->reduce_w*vob->ex_v_width);
      if(verbose & TC_INFO) printf("[%s] V: %-16s | %03dx%03d  %4.2f:1 (-r)\n", PACKAGE, "rescale frame", vob->ex_v_width, vob->ex_v_height,asr);

      // sanity check for YUV
      if(vob->im_v_codec == CODEC_YUV || vob->im_v_codec == CODEC_YUV422) {
	if(vob->ex_v_width%2 != 0 || (vob->im_v_codec == CODEC_YUV && vob->ex_v_height%2 != 0)) {
	    tc_error("rescaled width/height must be even for YUV mode, try --use_rgb");
	}
      }
    
      //2003-01-13 
      tc_adjust_frame_buffer(vob->ex_v_height, vob->ex_v_width);
    } 

    // --keep_asr

    if (keepasr) {
      int clip, zoomto;
      double asr_out = (double)vob->ex_v_width/(double)vob->ex_v_height;
      double asr_in  = (double)vob->im_v_width/(double)vob->im_v_height;
      double delta   = 0.01;
      double asr_cor = 1.0;


      if (vob->im_asr) {
	switch (vob->im_asr) {
	  case 1:
	    asr_cor = (1.0);
	    break;
	  case 2:
	    asr_cor = (4.0/3.0);
	    break;
	  case 3:
	    asr_cor = (16.0/9.0);
	    break;
	  case 4:
	    asr_cor = (2.21);
	    break;
	}
      }

      if (!zoom) tc_error ("keep_asr only works with -Z");

      if (asr_in-delta < asr_out && asr_out < asr_in+delta)
	  tc_error ("Aspect ratios are too similar, don't use --keep_asr ");

      if (asr_in > asr_out) {
	  /* adjust height */
	  int clipV = (vob->im_clip_top +vob->im_clip_bottom);
	  int clipH = (vob->im_clip_left+vob->im_clip_right);
	  int clip1 = 0;
	  int clip2 = 0;
	  zoomto = (int)((double)(vob->ex_v_width) / 
		        ( ((double)(vob->im_v_width -clipH) / (vob->im_v_width/asr_cor/vob->im_v_height) )/ 
		         (double)(vob->im_v_height-clipV))+.5);


	  clip = vob->ex_v_height - zoomto;
	  /*
	  printf("clip %d, zoomto %d cor %f ducor %f imw %d imh %d\n", 
	      clip, zoomto, asr_cor, vob->im_v_width/asr_cor/vob->im_v_height, vob->im_v_width, vob->im_v_height);
	      */

	  if (zoomto%2 != 0) (clip>0?zoomto--:zoomto++);
	  clip = vob->ex_v_height - zoomto;
	  clip /= 2;
	  clip1 = clip2 = clip;

	  if (clip&1) { clip1--; clip2++; }
	  ex_clip = TC_TRUE;
	  vob->ex_clip_top = -clip1;
	  vob->ex_clip_bottom = -clip2;

	  vob->zoom_height = zoomto;

      } else {
	  /* adjust width */
	  int clipV = (vob->im_clip_top +vob->im_clip_bottom);
	  int clipH = (vob->im_clip_left+vob->im_clip_right);
	  int clip1 = 0;
	  int clip2 = 0;
	  zoomto = (int)((double)vob->ex_v_height * (
		        ( ((double)(vob->im_v_width-clipH)) / (vob->im_v_width/asr_cor/vob->im_v_height) ) / 
		           (double)(vob->im_v_height-clipV)) +.5);

	  clip = vob->ex_v_width - zoomto;

	  /*
	      */

	  if (zoomto%2 != 0) (clip>0?zoomto--:zoomto++);
	  clip = vob->ex_v_width - zoomto;
	  clip /= 2;
	  clip1 = clip2 = clip;

	  if (clip&1) { clip1--; clip2++; }
	  ex_clip = TC_TRUE;
	  vob->ex_clip_left = -clip1;
	  vob->ex_clip_right = -clip2;

	  vob->zoom_width = zoomto;
      }
      
      if(vob->ex_v_height - vob->ex_clip_top - vob->ex_clip_bottom <= 0)
	tc_error("invalid top/bottom clip parameter calculated from --keep_asr");
      
      if(vob->ex_v_width - vob->ex_clip_left - vob->ex_clip_right <= 0)
	tc_error("invalid left/right clip parameter calculated from --keep_asr");
      
      if(verbose & TC_INFO) printf("[%s] V: %-16s | yes (%d,%d,%d,%d)\n", PACKAGE, "keep aspect",
	    vob->ex_clip_top, vob->ex_clip_left, vob->ex_clip_bottom, vob->ex_clip_right);
    }
    
    // -z

    if(flip && verbose & TC_INFO) 
      printf("[%s] V: %-16s | yes\n", PACKAGE, "flip frame");
    
    // -l

    if(mirror && verbose & TC_INFO) 
      printf("[%s] V: %-16s | yes\n", PACKAGE, "mirror frame");
    
    // -k

    if(rgbswap && verbose & TC_INFO) 
      printf("[%s] V: %-16s | yes\n", PACKAGE, "rgb2bgr");

    // -K

    if(decolor && verbose & TC_INFO) 
      printf("[%s] V: %-16s | yes\n", PACKAGE, "b/w reduction");

    // -G

    if(dgamma && verbose & TC_INFO) 
      printf("[%s] V: %-16s | %.3f\n", PACKAGE, "gamma correction", vob->gamma);

    // number of bits/pixel
    //
    // Christoph Lampert writes in transcode-users/2002-July/003670.html
    //          B*1000            B*1000*asr
    //  bpp =  --------;   W^2 = ------------
    //          W*H*F             bpp * F
    // If this number is less than 0.15, you will
    // most likely see visual artefacts (e.g. in high motion scenes). If you 
    // reach 0.2 or more, the visual quality normally is rather good. 
    // For my tests, this corresponded roughly to a fixed quantizer of 4, 
    // which is not brilliant, but okay.

    if(vob->divxbitrate > 0 && vob->divxmultipass != 3 && verbose & TC_INFO) {
      double div = vob->ex_v_width * vob->ex_v_height * vob->fps;
      double bpp = vob->divxbitrate * 1000;
      char *judge;

      if (div < 1.0)
	bpp = 0.0;
      else
	bpp /= div;

      if (bpp <= 0.0)
	judge = " (unknown)";
      else if (bpp > 0.0  && bpp <= 0.15)
	judge = " (low)";
      else 
	judge = "";

      printf("[%s] V: %-16s | %.3f%s\n", 
	  PACKAGE, "bits/pixel", bpp, judge);
    }


    // -C

    if(vob->antialias<0) 
      tc_error("invalid parameter for option -C");
    else
      if((verbose & TC_INFO) && vob->antialias) {

	switch(vob->antialias) {
	  
	case 1:
	  printf("[%s] V: %-16s | (mode=%d|%.2f|%.2f) de-interlace effects only\n", PACKAGE, "anti-alias", vob->antialias, vob->aa_weight, vob->aa_bias);
	  break;
	case 2:
	  printf("[%s] V: %-16s | (mode=%d|%.2f|%.2f) resize effects only\n", PACKAGE, "anti-alias", vob->antialias, vob->aa_weight, vob->aa_bias);
	  break;
	case 3:
	  printf("[%s] V: %-16s | (mode=%d|%.2f|%.2f) process full frame (slow)\n", PACKAGE, "anti-alias", vob->antialias, vob->aa_weight, vob->aa_bias);
	  break;
	default:
	  break;
	}
      }

    //set preview frame size before post-processing

    tc_x_preview = vob->ex_v_width;
    tc_y_preview = vob->ex_v_height;
    
    // --POST_CLIP
    
    if(post_ex_clip) {
      
      // force to even for YUV mode
      if(vob->im_v_codec == CODEC_YUV || vob->im_v_codec == CODEC_YUV422) {
	if(vob->post_ex_clip_left%2 != 0) {
	    tc_warn("left/right post_clip must be even in YUV/YUV422 mode");
	    vob->post_ex_clip_left--;
	}
	if(vob->post_ex_clip_right%2 != 0) {
	    tc_warn("left/right post_clip must be even in YUV/YUV422 mode");
	    vob->post_ex_clip_right--;
	}
	if(vob->im_v_codec == CODEC_YUV && vob->post_ex_clip_top%2 != 0) {
	    tc_warn("top/bottom post_clip must be even in YUV mode");
	    vob->post_ex_clip_top--;
	}
	if(vob->im_v_codec == CODEC_YUV && vob->post_ex_clip_bottom%2 != 0) {
	    tc_warn("top/bottom post_clip must be even in YUV mode");
	    vob->post_ex_clip_bottom--;
	}
      }

      //check against export parameter, this is post processing!
      
      if(vob->ex_v_height - vob->post_ex_clip_top - vob->post_ex_clip_bottom <= 0 ||
	 vob->ex_v_height - vob->post_ex_clip_top - vob->post_ex_clip_bottom > TC_MAX_V_FRAME_HEIGHT) tc_error("invalid top/bottom clip parameter for option --post_clip");
      
      if(vob->ex_v_width - vob->post_ex_clip_left - vob->post_ex_clip_right <= 0 ||
	 vob->ex_v_width - vob->post_ex_clip_left - vob->post_ex_clip_right > TC_MAX_V_FRAME_WIDTH) tc_error("invalid left/right clip parameter for option --post_clip");

      vob->ex_v_height -= (vob->post_ex_clip_top + vob->post_ex_clip_bottom);
      vob->ex_v_width -= (vob->post_ex_clip_left + vob->post_ex_clip_right);
      
      if(verbose & TC_INFO) printf("[%s] V: %-16s | %03dx%03d\n", PACKAGE, "post clip frame", vob->ex_v_width, vob->ex_v_height);

      //2003-01-13 
      tc_adjust_frame_buffer(vob->ex_v_height, vob->ex_v_width);
    }
    
    
    // -W

    if(vob->vob_percentage) {
      if(vob->vob_chunk < 0 || vob->vob_chunk < 0) tc_error("invalid parameter for option -W");
    } else {
      if(vob->vob_chunk < 0 || vob->vob_chunk > vob->vob_chunk_max) tc_error("invalid parameter for option -W");
    }

    // -f

    if(verbose & TC_INFO) 
      printf("[%s] V: %-16s | %.3f,%d\n", PACKAGE, "decoding fps,frc", vob->fps, vob->im_frc);
   
    // -R
    
    if(vob->divxmultipass && verbose & TC_INFO) {
      
      switch(vob->divxmultipass) {
	
      case 1:
	printf("[%s] V: %-16s | (mode=%d) %s %s\n", PACKAGE, "multi-pass", vob->divxmultipass, "writing data (pass 1) to", vob->divxlogfile);
	break;
	
      case 2:
	printf("[%s] V: %-16s | (mode=%d) %s %s\n", PACKAGE, "multi-pass", vob->divxmultipass, "reading data (pass2) from", vob->divxlogfile);
	break;
	
      case 3:
	if(vob->divxbitrate > VMAXQUANTIZER) vob->divxbitrate = VQUANTIZER;
	printf("[%s] V: %-16s | (mode=%d) %s (quant=%d)\n", PACKAGE, "single-pass", vob->divxmultipass, "constant quantizer/quality", vob->divxbitrate);
	break;
      }
    }
    
    
    // export frame size final check

    if(vob->ex_v_height < 0 || vob->ex_v_width < 0) {
      tc_warn("invalid export frame combination %dx%d", vob->ex_v_width, vob->ex_v_height);
      tc_error("invalid frame processing requested");
    }    

    // -V / --yuv422
    
    if(vob->im_v_codec==CODEC_YUV) {
      vob->ex_v_size = (3*vob->ex_v_height * vob->ex_v_width)>>1;
      vob->im_v_size = (3*vob->im_v_height * vob->im_v_width)>>1;
      if(verbose & TC_INFO) printf("[%s] V: %-16s | I420\n", PACKAGE, "YCbCr");
    } else if (vob->im_v_codec==CODEC_YUV422) {
      vob->ex_v_size = (2*vob->ex_v_height * vob->ex_v_width);
      vob->im_v_size = (2*vob->im_v_height * vob->im_v_width);
      if(verbose & TC_INFO) printf("[%s] V: %-16s | YUV422 (4:2:2)\n", PACKAGE, "YCbCr");
    } else
      vob->ex_v_size = vob->ex_v_height * vob->ex_v_width * vob->v_bpp>>3;

    // -p
    
    // video/audio from same source?
    if(audio_in_file==NULL) vob->audio_in_file=vob->video_in_file;
    
    // -m

    // different audio/video output files not yet supported
    if(audio_out_file==NULL) vob->audio_out_file=vob->video_out_file;
    
    // -n

#ifdef USE_LIBA52_DECODER 
    //switch codec ids
    if(vob->fixme_a_codec==CODEC_AC3) vob->fixme_a_codec=CODEC_A52;
    else if(vob->fixme_a_codec==CODEC_A52) vob->fixme_a_codec=CODEC_AC3;
#endif

    if(no_ain_codec==1 && vob->has_audio==0 && 
       vob->fixme_a_codec==CODEC_AC3) {
      
      if (vob->amod_probed==NULL || strcmp(vob->amod_probed,"null")==0) {
	
	if(verbose & TC_DEBUG) printf("[%s] problems detecting audio format - using 'null' module\n", PACKAGE);
	vob->fixme_a_codec=0;
      }
    }
    
    if(preset_flag & TC_PROBE_NO_TRACK) {
      //tracks specified by user
    } else {
      
      if(!vob->has_audio_track && vob->has_audio) {
	tc_warn("requested audio track %d not found - using 'null' module", vob->a_track);
	vob->fixme_a_codec=0;
      }
    }
    
    //audio import disabled
    
    if(vob->fixme_a_codec==0) {
      if(verbose & TC_INFO) printf("[%s] A: %-16s | disabled\n", PACKAGE, "import");
      im_aud_mod="null";
    } else {
      //audio format, if probed sucessfully
      if(verbose & TC_INFO) 
	(vob->a_stream_bitrate) ?
	  printf("[%s] A: %-16s | 0x%-5x %-12s [%4d,%2d,%1d] %4d kbps\n", PACKAGE, "import format", vob->fixme_a_codec, aformat2str(vob->fixme_a_codec), vob->a_rate, vob->a_bits, vob->a_chan, vob->a_stream_bitrate):
	printf("[%s] A: %-16s | 0x%-5x %-12s [%4d,%2d,%1d]\n", PACKAGE, "import format", vob->fixme_a_codec, aformat2str(vob->fixme_a_codec), vob->a_rate, vob->a_bits, vob->a_chan);
    }

    if(vob->im_a_codec==CODEC_PCM && vob->a_chan > 2 && !(vob->pass_flag & TC_AUDIO)) {
      // Input is more than 2 channels (i.e. 5.1 AC3) but PCM internal
      // representation can't handle that, adjust the channel count to reflect
      // what modules will actually have presented to them.

      if(verbose & TC_INFO) 
	printf("[%s] A: %-16s | %d channels -> %d channels\n", PACKAGE, "downmix", vob->a_chan, 2);
      vob->a_chan = 2;
    }

    if(vob->ex_a_codec==0 || vob->fixme_a_codec==0 || ex_aud_mod == NULL || strcmp(ex_aud_mod, "null")==0) {
      if(verbose & TC_INFO) printf("[%s] A: %-16s | disabled\n", PACKAGE, "export");
      ex_aud_mod="null";
    } else {
      
      //audio format
      
      if(ex_aud_mod && strlen(ex_aud_mod) != 0) {
	if (strcmp(ex_aud_mod, "mpeg")==0) vob->ex_a_codec=CODEC_MP2;
	if (strcmp(ex_aud_mod, "mp2enc")==0) vob->ex_a_codec=CODEC_MP2;
	if (strcmp(ex_aud_mod, "mp1e")==0) vob->ex_a_codec=CODEC_MP2;
      }
      
      // calc export bitrate
      switch (vob->ex_a_codec) {
      case 0x1: // PCM
	vob->mp3bitrate = ((vob->mp3frequency>0)? vob->mp3frequency:vob->a_rate) *
	                  ((vob->dm_bits>0)?vob->dm_bits:vob->a_bits) *
			  ((vob->dm_chan>0)?vob->dm_chan:vob->a_chan) / 1000;
	break;
      case 0x2000: // PCM
	if (vob->im_a_codec == CODEC_AC3) {
	  vob->mp3bitrate = vob->a_stream_bitrate;
	}
	break;
      }

      if(verbose & TC_INFO) {
	if(vob->pass_flag & TC_AUDIO)
	  printf("[%s] A: %-16s | 0x%-5x %-12s [%4d,%2d,%1d] %4d kbps\n", PACKAGE, "export format", vob->im_a_codec, aformat2str(vob->im_a_codec),
		 vob->a_rate, vob->a_bits, vob->a_chan, vob->a_stream_bitrate);
	else
	  printf("[%s] A: %-16s | 0x%-5x %-12s [%4d,%2d,%1d] %4d kbps\n", PACKAGE, "export format", vob->ex_a_codec, aformat2str(vob->ex_a_codec),
		 ((vob->mp3frequency>0)? vob->mp3frequency:vob->a_rate), 
		 ((vob->dm_bits>0)?vob->dm_bits:vob->a_bits), 
		 ((vob->dm_chan>0)?vob->dm_chan:vob->a_chan), 
		 vob->mp3bitrate);
      }
    }
    
    // Do not run out of audio-data
    // import_ac3 now correctly probes the channels of the ac3 stream
    // (previous versions always returned "2"). This breakes transcode
    // when doing -A --tibit
    if (vob->im_a_codec == CODEC_AC3)
      vob->a_chan = vob->a_chan>2?2:vob->a_chan;

    // -f and --export_fps/export_frc
    //
    // set import/export frc/fps
    //printf("XXX: 1 | %f,%d %f,%c\n", vob->fps, vob->im_frc, vob->ex_fps, vob->ex_frc);
    if (vob->im_frc == 0) 
      vob->im_frc = tc_guess_frc(vob->fps);
    //printf("XXX: 2 | %f,%d %f,%c\n", vob->fps, vob->im_frc, vob->ex_fps, vob->ex_frc);

    // ex_fps given, but not ex_frc
    if (vob->ex_frc == 0 && (vob->ex_fps != 0.0))
      vob->ex_frc = tc_guess_frc(vob->ex_fps);
    //printf("XXX: 3 | %f,%d %f,%c\n", vob->fps, vob->im_frc, vob->ex_fps, vob->ex_frc);

    if (vob->ex_frc == 0 && vob->im_frc != 0)
      vob->ex_frc = vob->im_frc;
    //printf("XXX: 4 | %f,%d %f,%c\n", vob->fps, vob->im_frc, vob->ex_fps, vob->ex_frc);

    // ex_frc always overwrites ex_fps
    if (vob->ex_frc > 0) {
      vob->ex_fps  = frc_table[vob->ex_frc];
    }

    //printf("XXX: 4 | %f,%d %f,%c\n", vob->fps, vob->im_frc, vob->ex_fps, vob->ex_frc);

    if (vob->im_frc <= 0 && vob->ex_frc <= 0 && vob->ex_fps == 0)
      vob->ex_fps = vob->fps;
    //printf("XXX: 4 | %f,%d %f,%c\n", vob->fps, vob->im_frc, vob->ex_fps, vob->ex_frc);

    if (vob->im_frc == -1) vob->im_frc = 0;
    if (vob->ex_frc == -1) vob->ex_frc = 0;
    /*
    */

    // --export_fps

    if(verbose & TC_INFO) 
      printf("[%s] V: %-16s | %.3f,%d\n", PACKAGE, "encoding fps,frc", vob->ex_fps, vob->ex_frc);
    

    //printf("FPS: %f,%d | %f,%d\n", vob->fps, vob->im_frc, vob->ex_fps, vob->ex_frc);
    
    // --a52_demux

    if((vob->a52_mode & TC_A52_DEMUX) && (verbose & TC_INFO))
      printf("[%s] A: %-16s | %s\n", PACKAGE, "A52 demuxing", "(yes) 3 front, 2 rear, 1 LFE (5.1)");
    
    //audio language, if probed sucessfully
    if(vob->lang_code > 0 && (verbose & TC_INFO)) 
      printf("[%s] A: %-16s | %c%c\n", PACKAGE, "language", vob->lang_code>>8, vob->lang_code & 0xff);
    
    // recalculate audio bytes per frame since video frames per second 
    // may have changed
    
    // samples per audio frame
    fch = vob->a_rate/vob->ex_fps;

    // bytes per audio frame
    vob->im_a_size = (int)(fch * (vob->a_bits/8) * vob->a_chan);
    vob->im_a_size =  (vob->im_a_size>>2)<<2;

    // rest:
    fch *= (vob->a_bits/8) * vob->a_chan;

    leap_bytes1 = TC_LEAP_FRAME * (fch - vob->im_a_size);
    leap_bytes2 = - leap_bytes1 + TC_LEAP_FRAME * (vob->a_bits/8) * vob->a_chan;
    leap_bytes1 = (leap_bytes1 >>2)<<2;
    leap_bytes2 = (leap_bytes2 >>2)<<2;

    if(leap_bytes1<leap_bytes2) {
	vob->a_leap_bytes = leap_bytes1;
    } else {
	vob->a_leap_bytes = -leap_bytes2;
	vob->im_a_size += (vob->a_bits/8) * vob->a_chan;
    }

    // final size in bytes
    vob->ex_a_size = vob->im_a_size;

    if(verbose & TC_INFO) printf("[%s] A: %-16s | %d (%.6f)\n", PACKAGE, "bytes per frame", vob->im_a_size, fch);

    if(no_audio_adjust) {
      vob->a_leap_bytes=0;
      
      if(verbose & TC_INFO) printf("[%s] A: %-16s | disabled\n", PACKAGE, "adjustment");
      
    } else 
      if(verbose & TC_INFO) printf("[%s] A: %-16s | %d@%d\n", PACKAGE, "adjustment", vob->a_leap_bytes, vob->a_leap_frame);
    
    // -s

    if(vob->volume > 0 && vob->a_chan != 2) {
      //tc_error("option -s not yet implemented for mono streams");
    }

    if(vob->volume > 0 && (verbose & TC_INFO)) printf("[%s] A: %-16s | %5.3f\n", PACKAGE, "rescale stream", vob->volume);

    // -D

    if(vob->sync_ms >= (int) (1000.0/vob->ex_fps) 
       || vob->sync_ms <= - (int) (1000.0/vob->ex_fps)) {
      vob->sync     = (int) (vob->sync_ms/1000.0*vob->ex_fps);
      vob->sync_ms -= vob->sync * (int) (1000.0/vob->ex_fps);
    }

    if((vob->sync || vob->sync_ms) &&(verbose & TC_INFO)) printf("[%s] A: %-16s | %d ms [ %d (A) | %d ms ]\n", PACKAGE, "AV shift", vob->sync * (int) (1000.0/vob->ex_fps) + vob->sync_ms, vob->sync, vob->sync_ms);
    
    // -d

    if(pcmswap) if(verbose & TC_INFO) printf("[%s] A: %-16s | yes\n", PACKAGE, "swap bytes");
    
    // -E
    
    //set export parameter to input parameter, if no re-sampling is requested
    if(vob->dm_chan==0) vob->dm_chan=vob->a_chan;
    if(vob->dm_bits==0) vob->dm_bits=vob->a_bits;

    // -P

    if(vob->pass_flag & TC_AUDIO) {
      vob->im_a_codec=CODEC_RAW;
      vob->ex_a_codec=CODEC_RAW;
      //suggestion:
      if(no_a_out_codec) ex_aud_mod="raw";
      no_a_out_codec=0;

      if(verbose & TC_INFO) printf("[%s] A: %-16s | yes\n", PACKAGE, "pass-through");
    }

    // -m
    
    // different audio/video output files need two export modules
    if(no_a_out_codec==0 && vob->audio_out_file==NULL &&strcmp(ex_vid_mod,ex_aud_mod) !=0) tc_error("different audio/export modules require use of option -m");
    
    
    // --record_v4l

    if(frame_asec > 0 || frame_bsec > 0) {
	if(frame_asec > 0 ) frame_a = (int) (frame_asec*vob->fps);
	if(frame_bsec > 0 ) frame_b = (int) (frame_bsec*vob->fps);
	counter_set_range(frame_a, frame_b);
    }

    // --accel

#if defined(ARCH_X86) || defined(ARCH_X86_64)
    if(verbose & TC_INFO) printf("[%s] V: IA32/AMD64 accel | %s \n", PACKAGE, ac_flagstotext(tc_accel & ac_cpuinfo()));
#endif

    ac_init(tc_accel);

    // more checks with warnings
    
    if(verbose & TC_INFO) {
      
      // -i
      
      if(video_in_file==NULL) tc_warn("no option -i found, reading from \"%s\"", vob->video_in_file);
      
      // -o
      
      if(video_out_file == NULL && audio_out_file == NULL && core_mode == TC_MODE_DEFAULT)
	tc_warn("no option -o found, encoded frames send to \"%s\"", vob->video_out_file);
      
      // -y
      
      if(core_mode == TC_MODE_DEFAULT && video_out_file != NULL && no_v_out_codec)
	tc_warn("no option -y found, option -o ignored, writing to \"/dev/null\"");
      
      if(core_mode == TC_MODE_AVI_SPLIT && no_v_out_codec)
	tc_warn("no option -y found, option -t ignored, writing to \"/dev/null\"");

      if( vob->im_v_codec==CODEC_YUV && (vob->im_clip_left%2!=0 ||
	    vob->im_clip_right%2 || vob->im_clip_top%2!=0 || vob->im_clip_bottom%2!=0))
	tc_warn ("Odd import clipping paramter(s) detected, may cause distortion");

      if( vob->im_v_codec==CODEC_YUV && (vob->ex_clip_left%2!=0 ||
	    vob->ex_clip_right%2 || vob->ex_clip_top%2!=0 || vob->ex_clip_bottom%2!=0))
	tc_warn ("Odd export clipping paramter(s) detected, may cause distortion");
      

    }

    if(ex_aud_mod && strlen(ex_aud_mod) != 0 && strcmp(ex_aud_mod, "net")==0) 
      tc_server_thread=1;
    if(ex_vid_mod && strlen(ex_vid_mod) != 0 && strcmp(ex_vid_mod, "net")==0) 
      tc_server_thread=1;

    // -u 
    
    if(tc_buffer_delay_dec==-1) //adjust core parameter 
      tc_buffer_delay_dec = (vob->pass_flag & TC_VIDEO || ex_vid_mod==NULL || strcmp(ex_vid_mod, "null")==0) ? TC_DELAY_MIN:TC_DELAY_MAX;

    if(tc_buffer_delay_enc==-1) //adjust core parameter 
      tc_buffer_delay_enc = (vob->pass_flag & TC_VIDEO || ex_vid_mod==NULL || strcmp(ex_vid_mod, "null")==0) ? TC_DELAY_MIN:TC_DELAY_MAX;

    if(verbose & TC_DEBUG) printf("[%s] encoder delay = decode=%d encode=%d usec\n", PACKAGE, tc_buffer_delay_dec, tc_buffer_delay_enc);    

    if (socket_file) {
      if(pthread_create(&thread_socket, NULL, (void *) socket_thread, NULL)!=0)
	tc_error("failed to start socket handler thread");
    }


    if(core_mode == TC_MODE_AVI_SPLIT && !strlen(base) && !video_out_file)
      tc_error("no option -o found, no base for -t given, so what?");

    /* ------------------------------------------------------------- 
     *
     * OK, so far, now start the support threads, setup buffers, ...
     *
     * ------------------------------------------------------------- */ 
    
    // start the signal vob info structure server thread, if requested     
    if(tc_server_thread) {
      if(pthread_create(&thread_server, NULL, (void *) server_thread, vob)!=0)
	tc_error("failed to start server thread");
    }

    //this will speed up in pass-through mode
    if(vob->pass_flag && !(preset_flag & TC_PROBE_NO_BUFFER)) max_frame_buffer=50;

    if(verbose & TC_INFO) printf("[%s] V: video buffer     | %d @ %dx%d\n", PACKAGE, max_frame_buffer, tc_frame_width_max, tc_frame_height_max);    
    
#ifdef STATBUFFER
    // allocate buffer
    if(verbose & TC_DEBUG) printf("[%s] allocating %d framebuffer (static)\n", PACKAGE, max_frame_buffer);
    
    if(vframe_alloc(max_frame_buffer)<0) tc_error("static framebuffer allocation failed");
    if(aframe_alloc(max_frame_buffer)<0) tc_error("static framebuffer allocation failed");

#else
    if(verbose & TC_DEBUG) printf("[%s] %d framebuffer (dynamical) requested\n", PACKAGE, max_frame_buffer);
#endif

    // load import/export modules and filters plugins
    if(transcoder(TC_ON, vob)<0) tc_error("plug-in initialization failed");
    
    // start frame processing threads
    frame_threads_init(vob, max_frame_threads, max_frame_threads);

    /* ------------------------------------------------------------ 
     *
     * transcoder core modes
     *
     * ------------------------------------------------------------*/  
    
    switch(core_mode) {
      
    case TC_MODE_DEFAULT:
      
      /* ------------------------------------------------------------- 
       *
       * single file continuous or interval mode
       *
       * ------------------------------------------------------------*/  

      // init decoder and open the source     
      if (0 != vob->ttime->vob_offset){
	vob->vob_offset = vob->ttime->vob_offset;
      }
      if(import_open(vob)<0) tc_error("failed to open input source");

      // start the AV import threads that load the frames into transcode
      // this must be called after import_open
      import_threads_create(vob);
      
      // init encoder
      if(encoder_init(&export_para, vob)<0) tc_error("failed to init encoder");
      
      // open output files 
      if(encoder_open(&export_para, vob)<0) tc_error("failed to open output"); 

      // get start interval
      tstart = vob->ttime;

      while (tstart) {
	
        if (tstart->etf != TC_FRAME_LAST) {
          counter_set_range(tstart->stf, tstart->etf);
        }
        // main encoding loop, return when done with all frames
	// cluster mode will automagically determine frame range
        (tc_cluster_mode) ? encoder(vob, frame_a, frame_b):encoder(vob, tstart->stf, tstart->etf);

	// check for user cancelation request
	if (sig_int || sig_tstp) break;
	
        // next range
        tstart = tstart->next;
	// see if we're using vob_offset
	if ((tstart != NULL) && (tstart->vob_offset != 0)){
	  tc_decoder_delay=3;
	  import_threads_cancel();
	  import_close();
	  aframe_flush();
	  vframe_flush();
	  vob->vob_offset = tstart->vob_offset;
	  vob->sync = sync_seconds;
	  if(import_open(vob)<0) tc_error("failed to open input source");
	  import_threads_create(vob);
	}
      }

      // close output files
      encoder_close(&export_para);
      
      // stop encoder
      encoder_stop(&export_para);

      // cancel import threads
      import_threads_cancel(); 

      // stop decoder and close the source     
      import_close();

      break;
      
    case TC_MODE_AVI_SPLIT:
      

      /* ------------------------------------------------------------ 
       *
       * split output AVI file
       *
       * ------------------------------------------------------------*/  

      // init decoder and open the source     
      if(import_open(vob)<0) tc_error("failed to open input source");

      // start the AV import threads that load the frames into transcode
      import_threads_create(vob);
      
      // encoder init
      if(encoder_init(&export_para, vob)<0) tc_error("failed to init encoder");
      
      // need to loop for this option
      
      ch1 = 0;
      
      do { 

	if (!base || !strlen(base))
	  strlcpy(base, vob->video_out_file, TC_BUF_MIN);
	
	// create new filename 
	snprintf(buf, sizeof(buf), "%s%03d%s", base, ch1++, video_ext);
	
	// update vob structure
	vob->video_out_file = buf;
	vob->audio_out_file = buf;

	// open output
	if(encoder_open(&export_para, vob)<0) 
	  tc_error("failed to open output");      
	
	fa = frame_a;
	fb = frame_a + splitavi_frames;
	
	encoder(vob, fa, ((fb > frame_b) ? frame_b : fb));

	// close output
	encoder_close(&export_para);	
	
	// restart
	frame_a += splitavi_frames;
	if(frame_a >= frame_b) break;
	
	if(verbose & TC_DEBUG) fprintf(stderr, "(%s) import status=%d\n", __FILE__, import_status());

	// check for user cancelation request
	if(sig_int || sig_tstp) break;
	
      } while(import_status());
      
      encoder_stop(&export_para);

      // cancel import threads
      import_threads_cancel(); 

      // stop decoder and close the source     
      import_close();
      
      break;

    case TC_MODE_PSU:
      
      /* --------------------------------------------------------------- 
       *
       * VOB PSU mode: transcode and split based on program stream units
       *
       * --------------------------------------------------------------*/  
      
      // encoder init
      if(encoder_init(&export_para, vob)<0) 
	tc_error("failed to init encoder");      
      
      // open output
      if(no_split) {
	
	vob->video_out_file = psubase;
	
	if(encoder_open(&export_para, vob)<0) 
	  tc_error("failed to open output");      
      }

      // 1 sec delay after decoder closing
      tc_decoder_delay=3;
      
      // need to loop for this option
      ch1 = vob->vob_psu_num1;

      // enable counter
      counter_on();

      for(;;) {
	
	int ret;

	memset(buf, 0, sizeof buf);
	if(!no_split) {
	  // create new filename 
	  snprintf(buf, sizeof(buf), psubase, ch1);
	  
	  // update vob structure
	  vob->video_out_file = buf;

	  if(verbose & TC_INFO) printf("[%s] using output filename %s\n", PACKAGE, vob->video_out_file);
	}
	
	// get seek/frame information for next PSU
	// need to process whole PSU
	vob->vob_chunk=0;
	vob->vob_chunk_max=1;
	
	ret=split_stream(vob, nav_seek_file, ch1, &fa, &fb, 0);

	if(verbose & TC_DEBUG) printf("(%s) processing PSU %d, -L %d -c %d-%d %s (ret=%d)\n", __FILE__, ch1, vob->vob_offset, fa, fb, buf, ret);

	// exit condition
	if(ret<0 || ch1 == vob->vob_psu_num2) break;

	//do not process units with a small frame number, assume it is junk
	if((fb-fa) > psu_frame_threshold) {
	  
	  // start new decoding session with updated vob structure
	  // this starts the full decoder setup, including the threads
	  if(import_open(vob)<0) tc_error("failed to open input source");
	  
	  // start the AV import threads that load the frames into transcode
	  import_threads_create(vob);
	  
	  // set range for ETA
	  counter_set_range(fa, fb);
	  
	  // open new output file
	  if(!no_split) {
	    if(encoder_open(&export_para, vob)<0) 
	      tc_error("failed to open output");      
	  }
	  
	  // core 
	  // we try to encode more frames and let the decoder safely
	  // drain the queue to avoid threads not stopping
	  
	  encoder(vob, fa, TC_FRAME_LAST);
	  
	  // close output file
	  if(!no_split) {
	    if(encoder_close(&export_para)<0)
	      tc_warn("failed to close encoder - non fatal");
	  } else printf("\n");
	  
	  //debugging code since PSU mode still alpha code
	  vframe_fill_print(0);
	  aframe_fill_print(0);
	  
	  // cancel import threads
	  import_threads_cancel(); 

	  // stop decoder and close the source     
	  import_close();
	  
	  // flush all buffers before we proceed to next PSU
	  aframe_flush();
	  vframe_flush();

	  vob->psu_offset += (double) (fb-fa);
	  
	} else printf("skipping PSU %d with %d frame(s)\n", ch1, fb-fa);
	
	++ch1;

	if (sig_int || sig_tstp) break;

      }//next PSU
      
      // close output
      if(no_split) {
	if(encoder_close(&export_para)<0)
	  tc_warn("failed to close encoder - non fatal");
      }
      
      encoder_stop(&export_para);
      
      break;


    case TC_MODE_DIRECTORY:
      
      /* --------------------------------------------------------------- 
       *
       * internal directory mode (needs single import directory)
       *
       * --------------------------------------------------------------*/  
      
      // 1 sec delay after decoder closing
      tc_decoder_delay=1;

      if(strncmp(vob->video_in_file, "/dev/zero", 9)==0) dir_audio=1;
      
      dir_name = (dir_audio) ? vob->audio_in_file : vob->video_in_file;
      dir_fcnt = 0;
      
      if((tc_open_directory(dir_name))<0) { 
	tc_error("unable to open directory \"%s\"", dir_name);
	exit(1);
      }
      
      while((dir_fname=tc_scan_directory(dir_name))!=NULL) {
	if(verbose & TC_DEBUG) printf("(%d) %s\n", dir_fcnt, dir_fname);
	++dir_fcnt;
      }
      
      printf("(%s) processing %d file(s) in directory %s\n", __FILE__, dir_fcnt, dir_name);
      
      tc_close_directory();
      
      if(dir_fcnt==0) tc_error("no valid input files found");
      dir_fcnt=0;
      
      if((tc_open_directory(dir_name))<0) { 
	tc_error("unable to open directory \"%s\"", dir_name);
	exit(1);
      }
      
      if((tc_sortbuf_directory(dir_name))<0) { 
	tc_error("unable to sort directory entries \"%s\"", dir_name);
	exit(1);
      }
      
      // encoder init
      if(encoder_init(&export_para, vob)<0) 
	tc_error("failed to init encoder");      
      
      // open output
      if(no_split) {
	
	// create single output filename 
	snprintf(buf, sizeof(buf), "%s%s", dirbase, video_ext);
	
	// update vob structure
	if(dir_audio) {

	  switch(vob->ex_a_codec) {
	    
	  case CODEC_MP3:
	    snprintf(buf, sizeof(buf), "%s-%03d%s", dirbase, dir_fcnt, audio_ext);
	    break;
	  }

	  vob->audio_out_file = buf;
	  
	} else { 
	  vob->video_out_file = buf;
	}
	
	if(encoder_open(&export_para, vob)<0) 
	  tc_error("failed to open output");      
      }

      // need to loop with directory content for this option
      
      while((dir_fname=tc_scan_directory(dir_name))!=NULL) {
	
	// update vob structure
	if(dir_audio) {
	  vob->audio_in_file = dir_fname;
	} else { 
	  vob->video_in_file = dir_fname;
	  vob->audio_in_file = dir_fname;
	}

	if(!no_split) {
	  // create new filename 
	  snprintf(buf, sizeof(buf), "%s-%03d%s", dirbase, dir_fcnt, video_ext);
	  
	  // update vob structure
	  if(dir_audio) {
	    
	    switch(vob->ex_a_codec) {
	      
	    case CODEC_MP3:
	      snprintf(buf, sizeof(buf), "%s-%03d%s", dirbase, dir_fcnt, audio_ext);
	      break;
	    }

	    vob->audio_out_file = buf;
	    vob->out_flag=1;
	  } else { 
	    vob->video_out_file = buf;
	  }
	}

	// start new decoding session with updated vob structure
	if(import_open(vob)<0) tc_error("failed to open input source");

	// start the AV import threads that load the frames into transcode
	import_threads_create(vob);

	// open output
	if(!no_split) {
	  if(encoder_open(&export_para, vob)<0) 
	    tc_error("failed to open output");      
	}

	// get start interval
	tstart = vob->ttime;

	while (tstart) {
	  
	  if (tstart->etf != TC_FRAME_LAST) {
	    counter_set_range(tstart->stf, tstart->etf);
	  }
	  
	  // main encoding loop, return when done with all frames
	  encoder(vob, tstart->stf, tstart->etf);
	  
	  // check for user cancelation request
	  if (sig_int || sig_tstp) break;
	  
	  // next range
	  tstart = tstart->next;
	}
	
	// close output
	if(!no_split) {
	  if(encoder_close(&export_para)<0)
	    tc_warn("failed to close encoder - non fatal");
	}

	// cancel import threads
	import_threads_cancel(); 

	// stop decoder and close the source     
	import_close();
	
	// flush all buffers before we proceed to next file
	aframe_flush();
        vframe_flush();
	
	++dir_fcnt;
	
	if (sig_int || sig_tstp) break;
	
      }//next directory entry
      
      tc_close_directory();
      tc_freebuf_directory();
      
      // close output
      if(no_split) {
	if(encoder_close(&export_para)<0)
	  tc_warn("failed to close encoder - non fatal"); 
      }
      
      encoder_stop(&export_para);
      
      break;

      
    case TC_MODE_DVD_CHAPTER:
      
#ifdef HAVE_LIBDVDREAD
      
      
      /* ------------------------------------------------------------ 
       *
       * DVD chapter mode
       *
       * ------------------------------------------------------------*/   
      
      // the import module probes for the number of chapter of 
      // given DVD title
      
      // encoder init
      if(encoder_init(&export_para, vob)<0) 
	tc_error("failed to init encoder");      

      // open output
      if(no_split) {
	
	// create new filename 
	snprintf(buf, sizeof(buf), "%s%s", chbase, video_ext);
	
	// update vob structure
	vob->video_out_file = buf;
	vob->audio_out_file = buf;
	
	if(encoder_open(&export_para, vob)<0) 
	  tc_error("failed to open output");      
      }

      // 1 sec delay after decoder closing
      tc_decoder_delay=1;

      // loop each chapter
      ch1=vob->dvd_chapter1;
      ch2=vob->dvd_chapter2;
      
      //ch=-1 is allowed but makes no sense
      if(ch1<0) ch1=1;

      //frame range selection finally works
      (frame_b < INT_MAX) ? counter_set_range(frame_a, frame_b) : counter_off(); 

      for(;;) {
	
	vob->dvd_chapter1 = ch1;
	vob->dvd_chapter2 =  -1;
	
	if(!no_split) {
	  // create new filename 
	  snprintf(buf, sizeof(buf), "%s-ch%02d%s", chbase, ch1, video_ext);
	  
	  // update vob structure
	  vob->video_out_file = buf;
	  vob->audio_out_file = buf;
	}
	
	// start decoding with updated vob structure
	if(import_open(vob)<0) tc_error("failed to open input source");
	
	// start the AV import threads that load the frames into transcode
	import_threads_create(vob);
	
	if(verbose & TC_DEBUG)
	  fprintf(stderr, "%d chapters for title %d detected\n", vob->dvd_max_chapters, vob->dvd_title);
	
	
	// encode
	if(!no_split) {
	  if(encoder_open(&export_para, vob)<0) 
	    tc_error("failed to init encoder");      
	}

	// main encoding loop, selecting an interval won't work
	encoder(vob, frame_a, frame_b);

	if(!no_split) {
	  if(encoder_close(&export_para)<0)
	    tc_warn("failed to close encoder - non fatal"); 
	}
	
	// cancel import threads
	import_threads_cancel(); 
	
	// stop decoder and close the source     
	import_close();
	
	// flush all buffers before we proceed
	aframe_flush();
	vframe_flush();
	
	//exit, i) if import module could not determine max_chapters
	//      ii) all chapters are done
	//      iii) someone hit ^C
	
	if(vob->dvd_max_chapters==-1 || ch1==vob->dvd_max_chapters || sig_int || sig_tstp || ch1 == ch2) break;
	ch1++;
      }
      
      if(no_split) {
	if(encoder_close(&export_para)<0)
	  tc_warn("failed to close encoder - non fatal"); 
      }
      
      encoder_stop(&export_para);
      
#endif
      break;

    case TC_MODE_DEBUG:
      
      printf("[%s] debug \"core\" mode", PACKAGE);

      break;

    default:
      //should not get here:
      tc_error("internal error");      
    }      
    
    /* ------------------------------------------------------------ 
     *
     * shutdown transcode, all cores modes end up here, core modes
     * must take care of proper import/export API shutdown. 
     *
     * 1) stop and cancel frame processing threads
     * 2) unload all external modules
     * 3) cancel internal signal/server thread
     *
     * ------------------------------------------------------------*/  

    // shutdown
    if(verbose & TC_INFO) { printf("\nclean up |"); fflush(stdout); }

    // stop and cancel frame processing threads
    frame_threads_close(); 
    if(verbose & TC_INFO) { printf(" frame threads |"); fflush(stdout); }

    // unload all external modules
    transcoder(TC_OFF, NULL);
    if(verbose & TC_INFO) { printf(" unload modules |");fflush(stdout); }

    // cancel no longer used internal signal handler threads
    if (tc_signal_thread) {
      if(verbose & TC_INFO) { printf(" cancel signal |");fflush(stdout); }
      if (thread_signal) {
	pthread_cancel(thread_signal);
        pthread_kill(thread_signal,SIGINT);
#ifdef BROKEN_PTHREADS // Used to be MacOSX specific; kernel 2.6 as well?
#endif
	pthread_join(thread_signal, &thread_status);
      }
      thread_signal=(pthread_t)0;
      tc_signal_thread=0;
    }
    
    // cancel optional server thread
    if(tc_server_thread) {
      pthread_cancel(thread_server);
      pthread_join(thread_server, &thread_status);
      tc_server_thread=0;
    }
    
    if(verbose & TC_INFO) { printf(" internal threads |");fflush(stdout); }

    // all done
    if(verbose & TC_INFO) printf(" done\n");

 summary:

    // print a summary
    if((verbose & TC_INFO) && vob->clip_count) 
      fprintf(stderr, "[%s] clipped %d audio samples\n", PACKAGE, vob->clip_count/2);

    if(verbose & TC_INFO) {
      
      long drop = - tc_get_frames_dropped();
      
      fprintf(stderr, "[%s] encoded %ld frames (%ld dropped, %ld cloned), clip length %6.2f s\n", 
	      PACKAGE, tc_get_frames_encoded(), drop, tc_get_frames_cloned(), tc_get_frames_encoded()/vob->fps);
    }

#ifdef STATBUFFER
    // free buffers
    vframe_free();
    aframe_free();
    if(verbose & TC_DEBUG) fprintf(stderr, "[%s] buffer released\n", PACKAGE);
#endif

    if (vob) free(vob);

    //exit at last
    if (sig_int || sig_tstp)
      exit(127);
    else
      exit(0);
}

// this Code below here _never_ gets called.
// it is just there to trick the linker to not remove
// unneeded object files from a .a file.

#include "avilib/avilib.h"
void dummy_avilib(void);
void dummy_avilib(void) {
  avi_t *file;
  file = AVI_open_input_file((char *)NULL, 1);
  AVI_close(file);
  AVI_dump((char *)NULL, 0);
  AVI_info(file);
}

#include "libioaux/configs.h"
void dummy_libioaux(void);
void dummy_libioaux(void) {
  module_read_config(NULL, NULL, NULL, NULL, NULL);
  append_fc_time( NULL, NULL);
}

/* vim: sw=2 ts=8 
 */
