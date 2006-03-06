/*
 * tcexport.c - standalone encoder frontend for transcode
 * Copyright (C) Francesco Romani - February 2006
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "transcode.h"
#include "framebuffer.h"
#include "counter.h"
#include "probe.h"
#include "encoder.h"
#include "tcmodule-core.h"
#include "frc_table.h"

#include "rawsource.h"

#define EXE "tcexport"

#define STATUS_OK               0
#define STATUS_DONE             1
#define STATUS_BAD_PARAM        2
#define STATUS_IO_ERROR         4
#define STATUS_NO_MODULE        8
#define STATUS_MODULE_ERROR     16
#define STATUS_INTERNAL_ERROR   32
#define STATUS_PROBE_FAILED     64

#define VIDEO_LOG_FILE       "mpeg4.log"
#define AUDIO_LOG_FILE       "pcm.log"

typedef  struct tcencconf_ TCEncConf;

struct tcencconf_ {
    int dry_run;
    vob_t *vob;
    
    char vlogfile[TC_BUF_MIN];
    char alogfile[TC_BUF_MIN];
    
    char video_mod[TC_BUF_MIN];
    char audio_mod[TC_BUF_MIN];
    char mplex_mod[TC_BUF_MIN];
};
    
static vob_t vob = {
    .has_video = 1,
    .has_audio = 1,

    // some arbitrary values for the modules
    .fps = PAL_FPS,
    .ex_fps = PAL_FPS,
    .im_v_width = PAL_W,
    .ex_v_width = PAL_W,
    .im_v_height= PAL_H,
    .ex_v_height= PAL_H,

    .im_v_codec = CODEC_YUV,
    .im_a_codec = CODEC_PCM,

    .im_frc = 3,
    
    .a_rate = RATE,
    .a_chan = CHANNELS,
    .a_bits = BITS,
    .a_vbr = AVBR,

    .mod_path = MOD_PATH,
    
    .video_in_file = "/dev/zero",
    .audio_in_file = "/dev/zero",
    .video_out_file = "/dev/null",
    .audio_out_file = "/dev/null",
    .audiologfile = AUDIO_LOG_FILE,

    .mp3bitrate = ABITRATE,
    .mp3quality = AQUALITY,
    .mp3mode = AMODE,
    .mp3frequency = RATE,
    
    .divxlogfile = VIDEO_LOG_FILE,
    .divxmultipass = VMULTIPASS,
    .divxbitrate = VBITRATE,
    .divxkeyframes = VKEYFRAMES,
    .divxcrispness = VCRISPNESS,
};


void version(void)
{
    tc_log_info(EXE, "%s v%s (C) 2006 transcode team",
                EXE, VERSION);
}

static void usage(void)
{
    version();
    tc_log_info(EXE, "Usage: %s [options]", EXE);
    tc_log_msg(EXE, "\t -d verbosity      Verbosity mode [1 == TC_INFO]");
    tc_log_msg(EXE, "\t -D                dry run, only loads module (used"
                    " for testing)");
    tc_log_msg(EXE, "\t -m path           Use PATH as module path");
    tc_log_msg(EXE, "\t -b b[,v[,q[,m]]]  audio encoder bitrate kBits/s"
                    "[,vbr[,quality[,mode]]] [%i,%i,%i,%i]",
                    ABITRATE, AVBR, AQUALITY, AMODE);
    tc_log_msg(EXE, "\t -i file           video input file name");
    tc_log_msg(EXE, "\t -p file           audio input file name");
    tc_log_msg(EXE, "\t -o file           output file (base)name");
    tc_log_msg(EXE, "\t -y V,A,M          Video,Audio,Multiplexor export"
                    " modules [%s,%s,%s]", TC_DEFAULT_EXPORT_VIDEO,
                    TC_DEFAULT_EXPORT_AUDIO, TC_DEFAULT_EXPORT_MPLEX);
    tc_log_msg(EXE, "\t -w b[,k[,c]]      encoder"
                " bitrate[,keyframes[,crispness]] [%d,%d,%d]",
            VBITRATE, VKEYFRAMES, VCRISPNESS);
    tc_log_msg(EXE, "\t -R n[,f1[,f2]]    enable multi-pass encoding"
                " (0-3) [%d,mpeg4.log,pcm.log]", VMULTIPASS);
}

static void config_init(TCEncConf *conf, vob_t *vob)
{
    conf->dry_run = TC_FALSE;
    conf->vob = vob;

    strlcpy(conf->vlogfile, VIDEO_LOG_FILE, TC_BUF_MIN);
    strlcpy(conf->alogfile, AUDIO_LOG_FILE, TC_BUF_MIN);
   
    strlcpy(conf->video_mod, TC_DEFAULT_EXPORT_VIDEO, TC_BUF_MIN);
    strlcpy(conf->audio_mod, TC_DEFAULT_EXPORT_AUDIO, TC_BUF_MIN);
    strlcpy(conf->mplex_mod, TC_DEFAULT_EXPORT_MPLEX, TC_BUF_MIN);
}

static char *setup_mod_string(char *mod)
{
    size_t modlen = strlen(mod);
    char *sep = strchr(mod, '=');
    char *opts = NULL;
    
    if (modlen > 0 && sep != NULL) {
        size_t optslen;

        opts = sep + 1;
        optslen = strlen(opts);
        
        if (!optslen) {
            opts = NULL; /* no options or bad options given */
        }
        *sep = '\0'; /* mark end of module name */
    }
    return opts;
}
    
#define VALIDATE_OPTION \
        if (optarg[0] == '-') { \
            usage(); \
            return STATUS_BAD_PARAM; \
        }

static int parse_options(int argc, char** argv, TCEncConf *conf)
{
    int ch, n;
    vob_t *vob = conf->vob;
    
    if (argc == 1) {
        usage();
        return STATUS_BAD_PARAM;
    }

    while(1) {
        ch = getopt(argc, argv, "b:Dd:hi:m:o:p:R:y:w:v?");
        if (ch == -1) {
            break;
        }

        switch (ch) {
          case 'D':
            conf->dry_run = TC_TRUE;
            break; 
          case 'd':
            VALIDATE_OPTION;
            vob->verbose = atoi(optarg);
            break;
          case 'b':
            VALIDATE_OPTION;
            n = sscanf(optarg, "%i,%i,%f,%i",
                       &vob->mp3bitrate, &vob->a_vbr, &vob->mp3quality,
                       &vob->mp3mode);
            if (n < 0
              || vob->mp3bitrate < 0
              || vob->a_vbr < 0
              || vob->mp3quality < -1.00001
              || vob->mp3mode < 0) {
                tc_log_error(EXE, "invalid parameter for -b");
                return STATUS_BAD_PARAM;
            }
            break;
          case 'i':
            VALIDATE_OPTION;
            vob->video_in_file = optarg;
            break;
          case 'm':
            VALIDATE_OPTION;
            vob->mod_path = optarg;
            break;
          case 'p':
            VALIDATE_OPTION;
            vob->audio_in_file = optarg;
            break;
          case 'R':
            VALIDATE_OPTION;
            n = sscanf(optarg,"%d,%64[^,],%64s",
               &vob->divxmultipass, conf->vlogfile, conf->alogfile);

            if (n == 3) {
                vob->audiologfile = conf->alogfile;
                vob->divxlogfile = conf->vlogfile;
            } else if (n == 2) {
                vob->divxlogfile = conf->vlogfile;
            } else if (n != 1) {
                tc_log_error(EXE, "invalid parameter for option -R");
                return STATUS_BAD_PARAM;
            }

            if (vob->divxmultipass < 0 || vob->divxmultipass > 3) {
                tc_log_error(EXE, "invalid multi-pass in option -R");
                return STATUS_BAD_PARAM;
            }
            break;
          case 'o':
            VALIDATE_OPTION;
            vob->video_out_file = optarg;
            break;
          case 'w':
            VALIDATE_OPTION;
            sscanf(optarg,"%d,%d,%d",
                   &vob->divxbitrate, &vob->divxkeyframes,
                   &vob->divxcrispness);

            if (vob->divxcrispness < 0 || vob->divxcrispness > 100
              || vob->divxbitrate <= 0 || vob->divxkeyframes < 0) {
                tc_log_error(EXE, "invalid parameter for option -w");
                return STATUS_BAD_PARAM;
            }
            break;
          case 'y':
            VALIDATE_OPTION;
	        n = sscanf(optarg,"%64[^,],%64[^,],%64s",
                       conf->video_mod, conf->audio_mod, conf->mplex_mod);
            if (n != 3) {
                tc_log_error(EXE, "invalid parameter for option -y"
                                  " (you must specify ALL parameters)");
                return STATUS_BAD_PARAM;
            }

            vob->ex_v_string = setup_mod_string(conf->video_mod);
            vob->ex_a_string = setup_mod_string(conf->audio_mod);
            vob->ex_m_string = setup_mod_string(conf->mplex_mod);
            break;
          case 'v':
            version();
            return STATUS_DONE;
          case '?': /* fallthrough */
          case 'h': /* fallthrough */
          default:
            usage();
            return STATUS_DONE;
        }
    }
    return STATUS_OK;
}

static void setup_im_size(vob_t *vob)
{
    double fch;

    /* update vob structure */
    /* assert(YUV420P source) */
    vob->im_v_size = (3 * vob->im_v_width * vob->im_v_height) / 2;
    vob->ex_v_size = vob->im_v_size;
    /* borrowed from transcode.c */
    /* samples per audio frame */
    fch = vob->a_rate/vob->ex_fps;
    /* bytes per audio frame */
    vob->im_a_size = (int)(fch * (vob->a_bits/8) * vob->a_chan);
    vob->im_a_size =  (vob->im_a_size>>2)<<2;
    vob->ex_a_size = vob->im_a_size;
}

#define MOD_OPTS(opts) (((opts)) ?((opts)) :"none")
static void print_summary(TCEncConf *conf, int verbose)
{
    vob_t *vob = conf->vob;
   
    version();
    if (verbose >= TC_INFO) {
        tc_log_info(EXE, "M: destination     | %s", vob->video_out_file);
        tc_log_info(EXE, "E: bitrate (A,V)   | %i,%i kbps",
                    vob->divxbitrate, vob->mp3bitrate);
        tc_log_info(EXE, "E: logfile (A,V)   | %s,%s",
                    vob->divxlogfile, vob->audiologfile); 
        tc_log_info(EXE, "V: encoder         | %s (options=%s)",
                    conf->video_mod, MOD_OPTS(vob->ex_v_string));
        tc_log_info(EXE, "A: encoder         | %s (options=%s)",
                    conf->audio_mod, MOD_OPTS(vob->ex_a_string));
        tc_log_info(EXE, "M: format          | %s (options=%s)",
                    conf->mplex_mod, MOD_OPTS(vob->ex_m_string));
        tc_log_info(EXE, "M: fps             | %.3f", vob->fps);
        tc_log_info(EXE, "V: picture size    | %ix%i",
                    vob->im_v_width, vob->im_v_height);
        tc_log_info(EXE, "V: bytes per frame | %i", vob->im_v_size);
        tc_log_info(EXE, "V: pass            | %i", vob->divxmultipass);
        tc_log_info(EXE, "A: rate,chans,bits | %i,%i,%i",
                    vob->a_rate, vob->a_chan, vob->a_bits);
        tc_log_info(EXE, "A: bytes per frame | %i", vob->im_a_size);
    }
}
#undef MOD_OPTS

/* dependencies. Yeah, this sucks badly **********************************/

vob_t *tc_get_vob()
{
    return &vob;
}

int plugin_get_handle(char *name)
{
    /* do nothing */
    return 0;
}

void tc_pause(void)
{
    /* do nothing */
    return;
}

pthread_mutex_t abuffer_im_fill_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t abuffer_im_fill_ctr = 0;
pthread_mutex_t abuffer_ex_fill_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t abuffer_ex_fill_ctr = 0;
pthread_mutex_t abuffer_xx_fill_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t abuffer_xx_fill_ctr = 0;
pthread_mutex_t vbuffer_im_fill_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t vbuffer_im_fill_ctr = 0;
pthread_mutex_t vbuffer_ex_fill_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t vbuffer_ex_fill_ctr = 0;
pthread_mutex_t vbuffer_xx_fill_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t vbuffer_xx_fill_ctr = 0;

int tc_progress_meter = 1;
int tc_progress_rate = 1;

/* more symbols nbeeded by modules ***************************************/

pthread_mutex_t init_avcodec_lock = PTHREAD_MUTEX_INITIALIZER;
int probe_export_attributes = 0;
const char *tc_config_dir = NULL;
int verbose  = TC_INFO;
int tc_accel = -1;    /* acceleration code */
int tc_cluster_mode = 0;
pid_t tc_probe_pid = 0;

#define EXIT_IF(cond, msg, status) \
    if((cond)) { \
        tc_log_error(EXE, msg); \
        return status; \
    }

int main(int argc, char *argv[])
{
    int ret = 0;
    int status = STATUS_OK;
    TCEncoderBuffer *source = NULL;
    /* needed by some modules */
    TCVHandle tcv_handle = tcv_init();
    TCFactory factory = NULL;

    TCEncConf config;
    
    ac_init(AC_ALL);
    config_init(&config, &vob);
    counter_on();
    
    ret = parse_options(argc, argv, &config);
    if (ret != STATUS_OK) {
        return (ret == STATUS_DONE) ?STATUS_OK :ret;
    }
    verbose = vob.verbose;
    ret = probe_source(vob.video_in_file, vob.audio_in_file,
                       1, 0, &vob);
    if (!ret) {
        return STATUS_PROBE_FAILED;
    }
    tc_adjust_frame_buffer(vob.im_v_width, vob.im_v_height);
    
    setup_im_size(&vob);
    print_summary(&config, verbose);
    
    factory = tc_new_module_factory(vob.mod_path, vob.verbose);
    EXIT_IF(!factory, "can't setup module factory", STATUS_MODULE_ERROR);

    /* open the A/V source */
    ret = tc_rawsource_open(&vob);
    EXIT_IF(ret != 2, "can't open input sources", STATUS_IO_ERROR);
    
    source = tc_rawsource_buffer(&vob, TC_FRAME_LAST);
    EXIT_IF(!source, "can't get rawsource handle", STATUS_IO_ERROR);
    
    ret = export_init(source, factory);
    EXIT_IF(ret != 0, "can't setup export subsystem", STATUS_MODULE_ERROR);
    
    ret = export_setup(config.audio_mod, config.video_mod, config.mplex_mod);
    EXIT_IF(ret != 0, "can't setup export modules", STATUS_MODULE_ERROR);

    if (!config.dry_run) {
        ret = encoder_init(&vob);
        EXIT_IF(ret != 0, "can't initialize encoder", STATUS_INTERNAL_ERROR);
    
        ret = encoder_open(&vob);
        EXIT_IF(ret != 0, "can't open encoder files", STATUS_IO_ERROR);
   
        encoder(&vob, TC_FRAME_FIRST, TC_FRAME_LAST);
        printf("\n"); /* dont' mess (too much) counter output */

        ret = encoder_stop();
        ret = encoder_close();
    }
        
    export_shutdown();
    
    ret = tc_rawsource_close();
    ret = tc_del_module_factory(factory);
    tcv_free(tcv_handle);

    if(verbose >= TC_INFO) {
        long drop = - tc_get_frames_dropped();

        tc_log_info(EXE, "encoded %ld frames (%ld dropped, %ld cloned),"
                        " clip length %6.2f s",
                        tc_get_frames_encoded(),
                        drop,
                        tc_get_frames_cloned(),
                        tc_get_frames_encoded()/vob.fps);
    }
    return status;
}

#include "libtc/static_optstr.h"
#include "avilib/static_avilib.h"

/* vim: sw=4
 */
