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
#include "filter.h"
#include "libtc/tcmodule-core.h"
#include "libtc/cfgfile.h"
#include "libtc/libtc.h"
#include "libtc/tccodecs.h"

#include "rawsource.h"

#define EXE "tcexport"

enum {
    STATUS_OK = 0,
    STATUS_DONE,
    STATUS_BAD_PARAM,
    STATUS_IO_ERROR,
    STATUS_NO_MODULE,
    STATUS_MODULE_ERROR,
    STATUS_INTERNAL_ERROR,
    STATUS_PROBE_FAILED,
};

#define VIDEO_LOG_FILE       "mpeg4.log"
#define AUDIO_LOG_FILE       "pcm.log"

#define VIDEO_CODEC          "yuv420p"
#define AUDIO_CODEC          "pcm"

#define RANGE_STR_SEP        ","

typedef  struct tcencconf_ TCEncConf;

struct tcencconf_ {
    int dry_run; /* flag */
    vob_t *vob;

    char video_codec[TC_BUF_MIN];
    char audio_codec[TC_BUF_MIN];

    char vlogfile[TC_BUF_MIN];
    char alogfile[TC_BUF_MIN];

    char video_mod[TC_BUF_MIN];
    char audio_mod[TC_BUF_MIN];
    char mplex_mod[TC_BUF_MIN];

    char *range_str;
};

static vob_t vob = {
    .verbose = TC_INFO,

    .has_video = 1,
    .has_audio = 1,

    /* some sane settings, mostly identical to transcode's ones */
    .fps = PAL_FPS,
    .ex_fps = PAL_FPS,
    .im_v_width = PAL_W,
    .ex_v_width = PAL_W,
    .im_v_height= PAL_H,
    .ex_v_height= PAL_H,

    .im_v_codec = CODEC_YUV,
    .im_a_codec = CODEC_PCM,
    .ex_v_codec = CODEC_YUV,
    .ex_a_codec = CODEC_PCM,

    .im_frc = 3,
    .ex_frc = 3,

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

    .a_leap_frame = TC_LEAP_FRAME,
    .a_leap_bytes = 0,
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
    tc_log_msg(EXE, "\t -c f1-f2[,f3-f4]  encode only f1-f2[,f3-f4]"
                    " (frames or HH:MM:SS) [all]");
    tc_log_msg(EXE, "\t -b b[,v[,q[,m]]]  audio encoder bitrate kBits/s"
                    "[,vbr[,quality[,mode]]] [%i,%i,%i,%i]",
                    ABITRATE, AVBR, AQUALITY, AMODE);
    tc_log_msg(EXE, "\t -i file           video input file name");
    tc_log_msg(EXE, "\t -p file           audio input file name");
    tc_log_msg(EXE, "\t -o file           output file (base)name");
    tc_log_msg(EXE, "\t -N V,A            Video,Audio output format"
                    " (encoder) [%s,%s]", VIDEO_CODEC, AUDIO_CODEC);
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

    conf->range_str = NULL;

    strlcpy(conf->vlogfile, VIDEO_LOG_FILE, TC_BUF_MIN);
    strlcpy(conf->alogfile, AUDIO_LOG_FILE, TC_BUF_MIN);

    strlcpy(conf->video_mod, TC_DEFAULT_EXPORT_VIDEO, TC_BUF_MIN);
    strlcpy(conf->audio_mod, TC_DEFAULT_EXPORT_AUDIO, TC_BUF_MIN);
    strlcpy(conf->mplex_mod, TC_DEFAULT_EXPORT_MPLEX, TC_BUF_MIN);
}

/* split up module string (=options) to module name */
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

/* basic sanity check */
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
        ch = getopt(argc, argv, "b:c:Dd:hi:m:N:o:p:R:y:w:v?");
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
          case 'c':
            VALIDATE_OPTION;
            conf->range_str = optarg;
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
          case 'N':
            VALIDATE_OPTION;
	        n = sscanf(optarg,"%64[^,],%64s",
                       conf->video_codec, conf->audio_codec);
            if (n != 2) {
                tc_log_error(EXE, "invalid parameter for option -N"
                                  " (you must specify ALL parameters)");
                return STATUS_BAD_PARAM;
            }

            vob->ex_v_codec = tc_codec_from_string(conf->video_codec);
            vob->ex_a_codec = tc_codec_from_string(conf->audio_codec);

            if (vob->ex_v_codec == TC_CODEC_ERROR
             || vob->ex_a_codec == TC_CODEC_ERROR) {
                tc_log_error(EXE, "unknown A/V format");
                return STATUS_BAD_PARAM;
            }
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
    int leap_bytes1, leap_bytes2;

    /* update vob structure */
    /* assert(YUV420P source) */
    vob->im_v_size = (3 * vob->im_v_width * vob->im_v_height) / 2;
    /* borrowed from transcode.c */
    /* samples per audio frame */
    // fch = vob->a_rate/vob->ex_fps;
    /* 
     * XXX I still have to understand why we
     * doing like this in transcode.c, so I'll simplify things here
     */
    fch = vob->a_rate/vob->fps;
    /* bytes per audio frame */
    vob->im_a_size = (int)(fch * (vob->a_bits/8) * vob->a_chan);
    vob->im_a_size =  (vob->im_a_size>>2)<<2;

    fch *= (vob->a_bits/8) * vob->a_chan;

    leap_bytes1 = TC_LEAP_FRAME * (fch - vob->im_a_size);
    leap_bytes2 = - leap_bytes1 + TC_LEAP_FRAME * (vob->a_bits/8) * vob->a_chan;
    leap_bytes1 = (leap_bytes1 >>2)<<2;
    leap_bytes2 = (leap_bytes2 >>2)<<2;

    if (leap_bytes1 < leap_bytes2) {
    	vob->a_leap_bytes = leap_bytes1;
    } else {
	    vob->a_leap_bytes = -leap_bytes2;
    	vob->im_a_size += (vob->a_bits/8) * vob->a_chan;
    }
}

static void setup_ex_params(vob_t *vob)
{
    /* common */
    vob->ex_fps = vob->fps;
    vob->ex_frc = vob->im_frc;
    /* video */
    vob->ex_v_width = vob->im_v_width;
    vob->ex_v_height = vob->im_v_height;
    vob->ex_v_size = vob->im_v_size;
    /* audio */
    vob->ex_a_size = vob->im_a_size;
    /* a_rate already correctly setup */
    vob->mp3frequency = vob->a_rate;
    vob->dm_bits = vob->a_bits;
    vob->dm_chan = vob->a_chan;
}

static int setup_ranges(TCEncConf *conf)
{
    vob_t *vob = conf->vob;
    int ret = 0;

    if (conf->range_str != NULL) {
        ret = parse_fc_time_string(conf->range_str, vob->fps,
                                   RANGE_STR_SEP, vob->verbose,
                                   &vob->ttime);
    } else {
        vob->ttime = new_fc_time();
        if (vob->ttime == NULL) {
            ret = -1;
        } else {
            vob->ttime->stf = TC_FRAME_FIRST;
            vob->ttime->etf = TC_FRAME_LAST;
            vob->ttime->vob_offset = 0;
            vob->ttime->next = NULL;
        }
    }
    return ret;
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
        tc_log_info(EXE, "A: adjustement     | %i@%i",
                         vob->a_leap_bytes, vob->a_leap_frame);
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

/* more symbols needed by modules ***************************************/

int probe_export_attributes = 0;
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
    /* needed by some modules */
    TCVHandle tcv_handle = tcv_init();
    TCFactory factory = NULL;

    TCEncConf config;

    ac_init(AC_ALL);
    tc_set_config_dir(NULL);
    config_init(&config, &vob);
    counter_on();

    ret = parse_options(argc, argv, &config);
    if (ret != STATUS_OK) {
        return (ret == STATUS_DONE) ?STATUS_OK :ret;
    }
    if (vob.ex_v_codec == TC_CODEC_ERROR
     || vob.ex_a_codec == TC_CODEC_ERROR) {
        tc_log_error(EXE, "bad export codec/format (use -N)");
        return STATUS_BAD_PARAM;
    }
    verbose = vob.verbose;
    ret = probe_source(vob.video_in_file, vob.audio_in_file,
                       1, 0, &vob);
    if (!ret) {
        return STATUS_PROBE_FAILED;
    }
    tc_adjust_frame_buffer(vob.im_v_width, vob.im_v_height);

    setup_im_size(&vob);
    setup_ex_params(&vob);
    ret = setup_ranges(&config);
    if (ret != 0) {
        tc_log_error(EXE, "error using -c option."
                          " Recheck your frame ranges!");
        return STATUS_BAD_PARAM;
    }
    print_summary(&config, verbose);

    factory = tc_new_module_factory(vob.mod_path, vob.verbose);
    EXIT_IF(!factory, "can't setup module factory", STATUS_MODULE_ERROR);

    /* open the A/V source */
    ret = tc_rawsource_open(&vob);
    EXIT_IF(ret != 2, "can't open input sources", STATUS_IO_ERROR);

    EXIT_IF(tc_rawsource_buffer == NULL, "can't get rawsource handle",
            STATUS_IO_ERROR);
    ret = export_init(tc_rawsource_buffer, factory);
    EXIT_IF(ret != 0, "can't setup export subsystem", STATUS_MODULE_ERROR);

    ret = export_setup(&vob,
                       config.audio_mod, config.video_mod, config.mplex_mod);
    EXIT_IF(ret != 0, "can't setup export modules", STATUS_MODULE_ERROR);

    if (!config.dry_run) {
        struct fc_time *tstart = NULL;
        int last_etf = 0;
        ret = encoder_init(&vob);
        EXIT_IF(ret != 0, "can't initialize encoder", STATUS_INTERNAL_ERROR);

        ret = encoder_open(&vob);
        EXIT_IF(ret != 0, "can't open encoder files", STATUS_IO_ERROR);

        /* first setup counter ranges */
        counter_reset_ranges();
    	for (tstart = vob.ttime; tstart != NULL; tstart = tstart->next) {
	        if (tstart->etf == TC_FRAME_LAST) {
                // variable length range, oh well
                counter_reset_ranges();
                break;
            }
            if (tstart->stf > last_etf) {
                counter_add_range(last_etf, tstart->stf-1, 0);
            }
            counter_add_range(tstart->stf, tstart->etf-1, 1);
            last_etf = tstart->etf;
    	}

        /* ok, now we can do the real (ranged) encoding */
        for (tstart = vob.ttime; tstart != NULL; tstart = tstart->next) {
            encoder_loop(&vob, tstart->stf, tstart->etf);
            printf("\n"); /* dont' mess (too much) counter output */
        }

        ret = encoder_stop();
        ret = encoder_close();
    }

    export_shutdown();

    ret = tc_rawsource_close();
    ret = tc_del_module_factory(factory);
    tcv_free(tcv_handle);
    free_fc_time(vob.ttime);

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
#include "avilib/static_wavlib.h"

/* vim: sw=4
 */


