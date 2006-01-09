/*
 *  encode_ffmpeg.c - ffmpeg/libavcodec interface module for transcode
 *  (C) Francesco Romani <fromani at gmail dot com> - January 2006
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include <math.h>

#include "transcode.h"
#include "framebuffer.h"
#include "filter.h"
#include "libtc/optstr.h"

#include "libtc/tcmodule-plugin.h"

#include "aclib/imgconvert.h"
/* FIXME: legacy from export_ffmpeg, still needed? */
#undef EMULATE_FAST_INT
#include <ffmpeg/avcodec.h>

#if !defined(INFINITY) && defined(HUGE_VAL)
#define INFINITY HUGE_VAL
#endif

#define MOD_NAME    "encode_ffmpeg.so"
#define MOD_VERSION "v0.0.1 (2006-01-01)"
#define MOD_CAP     "ffmpeg A/V encoder " LIBAVCODEC_IDENT

#define LINE_LEN  128

static const char *ffmpeg_help = ""
    "Overview:\n"
    "\tthis module provides access to ffmpeg codec library, libavcodec.\n"
    "\tlibavcodec can encode audio or video using a wide range of codecs,\n"
    "\tis fast and delivers high quality streams.\n"
    "Options:\n"
    "\thelp     tproduce module overview and options explanations\n"
    "\tlist     show all supported codecs\n"
    "\tvcodec   select video codec to use\n"
    "\tskipfile do not parse configuration file\n";

/*
 * FIXME:
 * - we can merge reset_module and set_lavc_defaults?
 *
 */

static const int ffmpeg_codecs_in[] = {
    TC_CODEC_RGB, TC_CODEC_YUV422P, TC_CODEC_YUV420P,
    TC_CODEC_ERROR
};

static const int ffmpeg_codecs_out[] = {
    TC_CODEC_MPEG1VIDEO, TC_CODEC_MPEG2VIDEO, TC_CODEC_DV, TC_CODEC_DIVX3,
    TC_CODEC_MP42, TC_CODEC_MPEG4,
    TC_CODEC_DIVX5, /* XXX */
    TC_CODEC_MJPG, TC_CODEC_RV10, TC_CODEC_WMV1, TC_CODEC_WMV2,
    TC_CODEC_HFYU, TC_CODEC_H263P, TC_CODEC_H263I, TC_CODEC_FFV1,
    TC_CODEC_ASV1, TC_CODEC_ASV2, TC_CODEC_H264,
    TC_CODEC_ERROR
};

#define CODECS_OUT_COUNT \
        (sizeof(ffmpeg_codecs_out) / sizeof(ffmpeg_codecs_out[0]))
#define DESC_LEN   (LINE_LEN * CODECS_OUT_COUNT)

extern char *tc_config_dir;

/*
 * libavcodec is not thread-safe. We must protect concurrent access to it.
 * this is visible (without the mutex of course) with
 * transcode .. -x ffmpeg -y ffmpeg -F mpeg4
 */
extern pthread_mutex_t init_avcodec_lock;

typedef struct {
    int multipass;
    int threads;
    
    uint8_t *tmp_buffer;
    uint8_t *yuv42xP_buffer;
    
    AVFrame *lavc_convert_frame;

    AVCodec *vid_codec;
    AVFrame *lavc_venc_frame;
    AVCodecContext *vid_ctx;
    
    int pix_fmt;
    
    FILE *stats_file;
    size_t size;
    int encoded_frames;
    int frames;
    int interlacing_active;
    int interlacing_top_first;
    int do_psnr;

    const char *codec_name;
    char *codecs_desc; /* rarely used, so dynamically created if needed */
    char conf_str[CONF_STR_SIZE]; /* will be always avalaible */
} FFmpegPrivateData;

/* 
 * while parsing the configuration file, sometimes isn't possible to
 * set directly a variable into lavc context, usually because such
 * value is a flag (and so destination value will be overwritten, NOT
 * OR-red as requested) or because value specified in file need
 * further manipulation (es: byte->bit conversion) before to be set.
 * So, such values are packed in the auxiliary configuration struct
 * below.
 */
typedef struct {
    int vrate_tolerance;
    int lmin;
    int lmax;
    int rc_min_rate;
    int rc_max_rate;
    int rc_buffer_size;
    int packet_size;
   
    /* flags */
    int v4mv_flag;
    int vdpart_flag;
    int gray_flag;
    int norm_aqp_flag;
    int psnr_flag;
    int qpel_flag;
    int trell_flag;
    int aic_flag;
    int umv_flag;
    int cbp_flag;
    int mv0_flag;
    int qp_rd_flag;
    int gmc_flag;
    int closed_gop_flag;
    int ss_flag;
    int alt_flag;
    int ilme_flag;
    int soff_flag;
} FFmpegConfig;

/* helpers used by helpers :) */
static int have_out_codec(int codec);
static double psnr(double d); 
static const char *lavc_codec_name(const char *tc_name);
static char* describe_out_codecs(void);
static void setup_frc(FFmpegPrivateData *pd, int fr_code);
static void setup_ex_par(FFmpegPrivateData *pd, vob_t *vob);
static void setup_dar_sar(FFmpegPrivateData *pd, vob_t *vob);

/* main helpers */
static void reset_module(FFmpegPrivateData *pd);
static void set_lavc_defaults(FFmpegPrivateData *pd);
static void set_conf_defaults(FFmpegConfig *cfg);
static int parse_options(FFmpegPrivateData *pd,
                         const char *options, vob_t *vob);
static int read_config_file(FFmpegPrivateData *pd);
static void config_summary(FFmpegPrivateData *pd);

static int ffmpeg_configure(TCModuleInstance *self,
                            const char *options, vob_t *vob)
{
    FFmpegPrivateData *pd = NULL;
    int ret;
    
    if (!self || !options || !vob) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }
    pd = self->userdata;
  
    set_lavc_defaults(pd);

    ret = parse_options(pd, options, vob);
    if (ret == TC_EXPORT_ERROR) {
        tc_log_error(MOD_NAME, "failed to setup user configuration");
        return TC_EXPORT_ERROR;
    }
    if (!optstr_lookup(options, "skipfile")) {
        ret = read_config_file(pd);
        if (ret == TC_EXPORT_ERROR) {
            tc_log_error(MOD_NAME, "failed to setup configuration from file");
            return TC_EXPORT_ERROR;
        }
    }
    
    ret = avcodec_open(pd->vid_ctx, pd->vid_codec);
    if (ret < 0) {
        tc_log_error(MOD_NAME, "could not open FFMPEG codec");
        return TC_EXPORT_ERROR;
    }

    if (pd->vid_ctx->codec->encode == NULL) {
        tc_log_error(MOD_NAME, "could not open FFMPEG codec "
                     "(vid_ctx->codec->encode == NULL)");
        return TC_EXPORT_ERROR;
    }

    if ((pd->threads < 1) || (pd->threads > 7)) {
        tc_log_warn(MOD_NAME, "Thread count out of range "
                              "(should be [0-7]), reset to 1");
        pd->threads = 1;
    }

    tc_log_info(MOD_NAME, "Starting %d thread(s)", pd->threads);
    avcodec_thread_init(pd->vid_ctx, pd->threads);

    if (verbose) {
        config_summary(pd);
    }
    return TC_EXPORT_OK;
}

static int ffmpeg_stop(TCModuleInstance *self)
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    return TC_EXPORT_OK;
}


static int ffmpeg_init(TCModuleInstance *self)
{
    FFmpegPrivateData *pd = NULL;
    vob_t *vob = tc_get_vob();
    
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    pd = tc_malloc(sizeof(FFmpegPrivateData));
    if (!pd) {
        tc_log_error(MOD_NAME, "init: can't allocate FFmpeg private data");
        return TC_EXPORT_ERROR;
    }

    reset_module(pd);

    pthread_mutex_lock(&init_avcodec_lock);
    avcodec_init();
    avcodec_register_all();
    pthread_mutex_unlock(&init_avcodec_lock);

    pd->vid_ctx = avcodec_alloc_context();
    pd->lavc_venc_frame = avcodec_alloc_frame();

    if (!pd->vid_ctx || !pd->lavc_venc_frame) {
        fprintf(stderr, "[%s] Could not allocate enough memory.\n", MOD_NAME);
        return TC_EXPORT_ERROR;
    }

    self->userdata = pd;
    /* can't fail, here */
    ffmpeg_configure(self, "vcodec=mpeg4:skipfile", vob);

    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }
    return TC_EXPORT_OK;
    
init_failed:
    tc_free(pd);
    self->userdata = NULL; /* paranoia */
    return TC_EXPORT_ERROR;
}

static int ffmpeg_fini(TCModuleInstance *self)
{
    FFmpegPrivateData *pd = NULL;
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    ffmpeg_stop(self);
    
    pd = self->userdata;

    if (pd->lavc_venc_frame) {
      free(pd->lavc_venc_frame);
      pd->lavc_venc_frame = NULL;
    }

    if (pd->vid_ctx != NULL) {
      free(pd->vid_ctx);
      pd->vid_ctx = NULL;
    }

    if (pd->codecs_desc) {
        tc_free(pd->codecs_desc);
        pd->codecs_desc = NULL;
    }
    if (pd->codec_name) {
        tc_free(pd->codec_name);
        pd->codec_name = NULL;
    } 

    tc_free(self->userdata);
    self->userdata = NULL;
    return TC_EXPORT_OK
}


static const char *ffmpeg_inspect(TCModuleInstance *self,
                                const char *param)
{
    FFmpegPrivateData *pd = NULL;
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return NULL;
    }
    pd = self->userdata;

    if (optstr_lookup(param, "help")) {
        return ffmpeg_help;
    }

    if (optstr_lookup(param, "list")) {
        if (!pd->codecs_desc) {
            pd->codecs_desc = describe_out_codecs();
        }
        if (!pd->codecs_desc) {
            /* 
             * describe_out_codecs() failes:
             * temporary (we hope) error...
             */
            return "internal error";
        }
        return pd->codecs_desc;
    }

    if (optstr_lookup(param, "vcodec")) {
        /* direct answer */
        return pd->codec_name;
    }
    if (optstr_lookup(param, "skipfile")) {
        return "0";
    }
    
    tc_snprintf(pd->conf_str, CONF_STR_SIZE, "vcodec=%s",
                pd->codec_name); 
    return pd->conf_str;
}

static int ffmpeg_encode_video(TCModuleInstance *self,
                              vframe_list_t *inframe, vframe_list_t *outframe)
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    return TC_EXPORT_OK;
}

/*************************************************************************/

static const TCModuleInfo ffmpeg_info = {
    .features    = TC_MODULE_FEATURE_ENCODE|TC_MODULE_FEATURE_VIDEO,
    .flags       = TC_MODULE_FLAG_RECONFIGURABLE,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = ffmpeg_codecs_in,
    .codecs_out  = ffmpeg_codecs_out
};

static const TCModuleClass ffmpeg_class = {
    .info         = &ffmpeg_info,

    .init         = ffmpeg_init,
    .fini         = ffmpeg_fini,
    .configure    = ffmpeg_configure,
    .stop         = ffmpeg_stop,
    .inspect      = ffmpeg_inspect,

    .encode_video = ffmpeg_encode_video,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &ffmpeg_class;
}

/*************************************************************************
 * support functions: implementation                                     *
 *************************************************************************/

static int have_out_codec(int codec)
{
    int i = 0;
    while (ffmpeg_codecs_out[i] != TC_CODEC_ERROR) {
        if (codec == ffmpeg_codecs_out[i]) {
            return 1;
        }
    }
    return 0;
}
        
static double psnr(double d) 
{
    if (d == 0) {
        return INFINITY;
    }
    return -10.0 * log(d) / log(10);
}

static const char *lavc_codec_name(const char *tc_name)
{
    if (!strcasecmp(tc_name, "dv")) {
        return "dvvideo";
    }
    return tc_name;
}

static char* describe_out_codecs(void)
{
    char *buffer = tc_malloc(DESC_LEN);
    char *pc = buffer;
    size_t len = 0;
    int i = 0;

    if (!buffer) {
        return NULL;
    }

    /* adjust line pointers, don't consider last line (end terminator) */
    for (i = 0; ffmpeg_codecs_out[i] != TC_CODEC_ERROR; i++) {
        /* errors not fatal, just output truncated */
        tc_codec_description(ffmpeg_codecs_out[i], pc,
                             DESC_LEN - (pc - buffer));
        len = strlen(pc);
        pc[len] = '\n';
        pc += len + 1;
    }
    return buffer;
}

static void setup_frc(FFmpegPrivateData *pd, int fr_code)
{
    if (!pd || !pd->vid_ctx) {
        return;
    }
    
    switch (fr_code) {
      case 1: /* 23.976 */
        pd->vid_ctx->time_base.den = 24000;
        pd->vid_ctx->time_base.num = 1001;
        break;
      case 2: /* 24.000 */
        pd->vid_ctx->time_base.den = 24000;
        pd->vid_ctx->time_base.num = 1000;
        break;
      case 3: /* 25.000 */
        pd->vid_ctx->time_base.den = 25000;
        pd->vid_ctx->time_base.num = 1000;
        break;
      case 4: /* 29.970 */
        pd->vid_ctx->time_base.den = 30000;
        pd->vid_ctx->time_base.num = 1001;
        break;
      case 5: /* 30.000 */
        pd->vid_ctx->time_base.den = 30000;
        pd->vid_ctx->time_base.num = 1000;
        break;
      case 6: /* 50.000 */
        pd->vid_ctx->time_base.den = 50000;
        pd->vid_ctx->time_base.num = 1000;
        break;
      case 7: /* 59.940 */
        pd->vid_ctx->time_base.den = 60000;
        pd->vid_ctx->time_base.num = 1001;
        break;
      case 8: /* 60.000 */
        pd->vid_ctx->time_base.den = 60000;
        pd->vid_ctx->time_base.num = 1000;
        break;
      case 0: /* not set */
      default:
        /* FIXME
        if ((vob->ex_fps > 29) && (vob->ex_fps < 30)) {
            pd->vid_ctx->time_base.den = 30000;
            pd->vid_ctx->time_base.num = 1001;
        }
        */
        /* XXX */
        pd->vid_ctx->time_base.den = (int)(vob->ex_fps * 1000.0);
        pd->vid_ctx->time_base.num = 1000;
        break;
    }
}

// XXX
static void setup_ex_par(FFmpegPrivateData *pd, vob_t *vob)
{
    if (!pd || !pd->vid_ctx || !vob) {
        return;
    }
    
    if (vob->ex_par > 0) {
        switch(vob->ex_par) {
          case 1:
            pd->vid_ctx->sample_aspect_ratio.num = 1;
            pd->vid_ctx->sample_aspect_ratio.den = 1;
            break;
          case 2:
            pd->vid_ctx->sample_aspect_ratio.num = 1200;
            pd->vid_ctx->sample_aspect_ratio.den = 1100;
            break;
          case 3:
            pd->vid_ctx->sample_aspect_ratio.num = 1000;
            pd->vid_ctx->sample_aspect_ratio.den = 1100;
            break;
          case 4:
            pd->vid_ctx->sample_aspect_ratio.num = 1600;
            pd->vid_ctx->sample_aspect_ratio.den = 1100;
            break;
          case 5:
            pd->vid_ctx->sample_aspect_ratio.num = 4000;
            pd->vid_ctx->sample_aspect_ratio.den = 3300;
            break;
          default:
            tc_log_warn(MOD_NAME, "unknown PAR code (not in [1..5]),"
                                  " defaulting to 1/1");
            pd->vid_ctx->sample_aspect_ratio.num = 1;
            pd->vid_ctx->sample_aspect_ratio.den = 1;
        }
    } else {
        if (vob->ex_par_width > 0 && vob->ex_par_height > 0) {
            pd->vid_ctx->sample_aspect_ratio.num = vob->ex_par_width;
            pd->vid_ctx->sample_aspect_ratio.den = vob->ex_par_height;
        } else {
            tc_log_warn(MOD_NAME, "bad PAR values (not [>0]/[>0]),"
                                  " defaulting to 1/1");
            pd->vid_ctx->sample_aspect_ratio.num = 1;
            pd->vid_ctx->sample_aspect_ratio.den = 1;
        }
    }
}

static void setup_dar_sar(FFmpegPrivateData *pd, vob_t *vob)
{
    double dar, sar;

    if (!pd || !pd->vid_ctx || !vob) {
        return;
    }

    if (vob->ex_asr > 0) {
                switch(vob->ex_asr) {
                case 1: dar = 1.0; break;
                case 2: dar = 4.0/3.0; break;
                case 3: dar = 16.0/9.0; break;
                case 4: dar = 221.0/100.0; break;
                default:
                    tc_log_warn(MOD_NAME, "Parameter value to --export_asr out of range (allowed: [1-4])");
		            return(TC_EXPORT_ERROR);

            tc_log_info(MOD_NAME, "Display aspect ratio calculated as %f", dar);
                sar = dar * ((double)vob->ex_v_height / (double)vob->ex_v_width);
                tc_log_info(MOD_NAME, "Sample aspect ratio calculated as %f", sar);
                vid_ctx->sample_aspect_ratio.num = (int)(sar * 1000);
                vid_ctx->sample_aspect_ratio.den = 1000;
            } else {
                tc_log_warn(MOD_NAME, "Parameter value to --export_asr out of range (allowed: [1-4])");
        		return(TC_EXPORT_ERROR);
	        }
        } else { /* user did not specify asr at all, assume no change */
            tc_log_info(MOD_NAME, "Set display aspect ratio to input");
            /*
             * sar = (4.0 * ((double)vob->ex_v_height) / (3.0 * (double)vob->ex_v_width));
             * vid_ctx->sample_aspect_ratio.num = (int)(sar * 1000);
             * vid_ctx->sample_aspect_ratio.den = 1000;
             */
            vid_ctx->sample_aspect_ratio.num = 1;
            vid_ctx->sample_aspect_ratio.den = 1;
        }
}


static int parse_options(FFmpegPrivateData *pd, const char *options, vob_t *vob)
{
    char user_codec[21] = { 'm', 'p', 'e', 'g', '4', '\0' }; // XXX
    const char *codec_name = user_codec;
    int tc_codec_id;
    int ret;
    
    pd->vid_ctx->bit_rate = vob->divxbitrate * 1000;
    pd->vid_ctx->width = vob->ex_v_width;
    pd->vid_ctx->height = vob->ex_v_height;
    pd->vid_ctx->qmin = vob->min_quantizer;
    pd->vid_ctx->qmax = vob->max_quantizer;
    setup_frc(pd, vob->ex_frc); 

    pd->pix_fmt = vob->im_v_codec;
    if (pd->pix_fmt != CODEC_RGB && pd->pix_fmt != CODEC_YUV
     && pd->pix_fmt != CODEC_YUV422) {
        tc_log_error(MOD_NAME, "Unknown color space %d.", pix_fmt);
        return TC_EXPORT_ERROR;
    }

    switch(vob->encode_fields) {
    case 1:
        pd->interlacing_active = 1;
        pd->interlacing_top_first = 1;
        break;
    case 2:
        pd->interlacing_active = 1;
        pd->interlacing_top_first = 0;
        break;
    default: /* progressive / unknown */
        pd->interlacing_active = 0;
        pd->interlacing_top_first = 0;
        break;
    }

    pd->vid_ctx->flags |= interlacing_active ?
        CODEC_FLAG_INTERLACED_DCT : 0;
    pd->vid_ctx->flags |= interlacing_active ?
        CODEC_FLAG_INTERLACED_ME : 0;

    ret = optstr_get(options, "vcodec", "%20[^:]", user_codec);
    if (ret > 0) {
        tc_strstrip(user_codec);
        codec_name = lavc_codec_name(user_codec);
    }    
    
    tc_codec_id = tc_codec_from_string(codec_name);
    if (!have_out_codec(pd->tc_codec_id)) {
        tc_log_error(MOD_NAME, "unknown '%s' codec", user_codec);
        return TC_EXPORT_ERROR;
    }

    if (tc_codec_id == TC_CODEC_MJPG && pd->levels_handle == -1) {
        tc_log_info(MOD_NAME, "output is mjpeg, extending range from "
		                      "YUV420P to YUVJ420P (full range)");

        pd->levels_handle = plugin_get_handle("levels=input=16-240");
        if(pd->levels_handle == -1) {
            tc_log_warn(MOD_NAME, "cannot load levels filter");
        }
    }

    pd->vid_codec = avcodec_find_encoder_by_name(codec_name);
    if (!vid_codec) {
        tc_log_error(MOD_NAME, "Could not find a FFMPEG codec for '%s'",
                     user_codec);
        return TC_EXPORT_ERROR;
    } else {
        if (verbose) {
            tc_log_info(MOD_NAME, "Using FFMPEG codec '%s'", user_codec);
        }
    }

    if (probe_export_attributes & TC_PROBE_NO_EXPORT_GOP) {
        pd->vid_ctx->gop_size = vob->divxkeyframes;
    } else {
        if (tc_codec_id == TC_CODEC_MPEG1VIDEO
         || tc_codec_id == TC_CODEC_MPEG2VIDEO) {
            vid_ctx->gop_size = 15; /* conservative default for mpeg1/2 svcd/dvd */
        } else {
            vid_ctx->gop_size = 250; /* reasonable default for mpeg4 (and others) */
        }
    }

    if (probe_export_attributes & TC_PROBE_NO_EXPORT_PAR) {
        /* export_par explicitely set by user */
        setup_ex_par(pd, vob);
    } else {
        if (probe_export_attributes & TC_PROBE_NO_EXPORT_ASR) {
            /* export_asr explicitely set by user */
            setup_dar_sar(pd, vob);
        }
    }

    pd->codec_name = tc_strdup(codec_name);
    return TC_EXPORT_OK;
}

static void reset_module(FFmpegPrivateData *pd)
{
    if (!pd) {
        return;
    }
   
    pd->levels_handle = -1;
    pd->threads = 1;
    pd->multipass = 0;
    pd->tmp_buffer = NULL
    pd->yuv42xP_buffer = NULL;
    pd->lavc_convert_frame = NULL;
    pd->vid_codec = NULL;
    pd->lavc_venc_frame = NULL;
    pd->vid_ctx = NULL;
    pd->pix_fmt = CODEC_YUV;
    pd->stats_file = NULL;
    pd->size = 0;
    pd->encoded_frames = 0;
    pd->frames = 0;
    pd->interlacing_active = 0;
    pd->interlacing_top_first = 0;
    pd->do_psnr = 0;
    pd->codec_name = "mpeg4";
    pd->codecs_desc = NULL;
    pd->conf_str[0] = '\0';
}

static void set_conf_defaults(FFmpegConfig *cfg)
{
    if (!cfg) {
        return;
    }
    memset(cfg, 0, sizeof(FFmpegConfig));

    cfg->vrate_tolerance = 1000*8;
    cfg->lmin = 2;
    cfg->lmax = 31;
}

static void set_lavc_defaults(FFMpegPrivateData *pd)
{
    AVCodecContext *ff_ctx = NULL; /* shortcut */
    if (!pd || !pd->vid_ctx) {
        /* can't happen */
        tc_log_error(MOD_NAME, "BAD ffmpeg context when setting defaults");
        return;
    }
    ff_ctx = pd->vid_ctx;

    ff_ctx->flags = 0;
    ff_ctx->mpeg_quant = 0;
    ff_ctx->vrate_tolerance = 1000*8;
    ff_ctx->mb_decision = 0;
    ff_ctx->me_method = 4;
    ff_ctx->mb_qmin = 2;
    ff_ctx->mb_qmax = 31;
    ff_ctx->lmin = 2;
    ff_ctx->lmax = 31;
    ff_ctx->vqdiff = 3;
    ff_ctx->vqcompress = 0.5;
    ff_ctx->vqblur = 0.5;
    ff_ctx->max_b_frames = 0;
    ff_ctx->b_quant_factor = 1.25;
    ff_ctx->b_frame_strategy = 0;
    ff_ctx->b_quant_offset = 1.25;
    ff_ctx->rc_strategy = 2;
    ff_ctx->luma_elim_threshold = 0;
    ff_ctx->chroma_elim_threshold = 0;
    ff_ctx->packet_size = 0;
    ff_ctx->strict_std_compliance = 0;
    ff_ctx->vi_qfactor = 0.8;
    ff_ctx->vi_qoffset = 0.0;
    ff_ctx->rc_qsquish = 1.0;
    ff_ctx->rc_qmod_amp = 0.0;
    ff_ctx->rc_qmod_freq = 0;
    ff_ctx->rc_override_string = NULL;
    ff_ctx->lavc_param_rc_eq = "tex^qComp";
    ff_ctx->rc_buffer_size = 0;
    ff_ctx->rc_buffer_aggressivity = 1.0;
    ff_ctx->rc_max_rate = 0;
    ff_ctx->rc_min_rate = 0;
    ff_ctx->rc_initial_cplx = 0.0;
    ff_ctx->mpeg_quant = 0;
    ff_ctx->fdct = 0;
    ff_ctx->idct = 0;
    ff_ctx->lumi_masking = 0.0;
    ff_ctx->dark_masking = 0.0;
    ff_ctx->temporal_cplx_masking = 0.0;
    ff_ctx->spatial_cplx_masking = 0.0;
    ff_ctx->p_masking = 0.0;
    ff_ctx->prediction_method= FF_PRED_LEFT;
    ff_ctx->debug = 0;
    ff_ctx->me_pre_cmp = 0;
    ff_ctx->me_cmp = 0;
    ff_ctx->me_sub_cmp = 0;
    ff_ctx->mb_cmp = 0;
    ff_ctx->ildct_cmp = FF_CMP_VSAD;
    ff_ctx->pre_dia_size= 0;
    ff_ctx->dia_size= 0;
    ff_ctx->last_predictor_count = 0;
    ff_ctx->pre_me = 1;
    ff_ctx->me_subpel_quality = 8;
    ff_ctx->me_range = 0;
    ff_ctx->ibias = FF_DEFAULT_QUANT_BIAS;
    ff_ctx->pbias = FF_DEFAULT_QUANT_BIAS;
    ff_ctx->coder_type = 0;
    ff_ctx->context_model = 0;
    ff_ctx->intra_matrix = NULL;
    ff_ctx->inter_matrix = NULL;
    ff_ctx->noise_reduction = 0;
    ff_ctx->inter_threshold = 0;
    ff_ctx->scenechange_threshold= 0;
    ff-ctx->threads = 1;
    ff_ctx->intra_dc_precision = 0;
    ff_ctx->top= -1;
}

static int read_config_file(FFmpegPrivateData *pd)
{
    AVCodecContext *ff_ctx = pd->vid_ctx; /* shortcut */
    FFmpegConfig aux_cfg;
    
    static struct config ffmpeg_config[] = {
        {"v4mv", &aux_cfg.v4mv_flag, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_4MV, 0},
        {"vdpart", &aux_cfg.vdpart_flag, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PART, NULL},
        {"gray", &aux_cfg.gray_flag, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PART, NULL},
        {"naq", &aux_cfg.norm_aqp_flag, CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"qpel", &aux_cfg.qpel_flag, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_QPEL, NULL},
        {"trell", &aux_cfg.trell_flag, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_TRELLIS_QUANT, NULL},
        {"aic", &aux_cfg.aic_flag, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_H263P_AIC, NULL},
        {"umv", &aux_cfg.umv_flag, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_H263P_UMV, NULL},
        {"cbp", &aux_cfg.cbp_flag, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_CBP_RD, NULL},
        {"mv0", &aux_cfg.mv0_flag, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_MV0, NULL},
        {"qprd", &aux_cfg.qp_rd_flag, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_QP_RD, NULL},
        {"gmc", &aux_cfg.gmc_flag, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_GMC, NULL},
        {"trunc", &aux_cfg.trunc_flag, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_TRUNCATED, NULL},
        {"closedgop", &aux_cfg.closed_gop_flag, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_CLOSED_GOP, NULL},
        {"ss", &aux_cfg.ss_flag, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_H263P_SLICE_STRUCT, NULL},
        {"alt", &aux_cfg.alt_flag, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_ALT_SCAN, NULL},
        {"ilme", &aux_cfg.ilme_flag, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_INTERLACED_ME, NULL}
        {"svcd_sof", &aux_cfg.soff_flag, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_SVCD_SCAN_OFFSET, 0},
        {"psnr", &aux_cfg.psnr_flag, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PSNR, NULL}, // XXX
        
        {"mpeg_quant", &ff_ctx->mpeg_quant, CONF_TYPE_FLAG, 0, 0, 1, NULL}, /* boolean, not flag */
        {"vratetol", &aux_cfg.vrate_tolerance, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
        {"mbd", &ff_ctx->mb_decision, CONF_TYPE_INT, CONF_RANGE, 0, 9, NULL},
        {"vme", &ff_ctx->me_method, CONF_TYPE_INT, CONF_RANGE, 1, 9, NULL}, // XXX
        {"mbqmin", &ff_ctx->mb_qmin, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
        {"mbqmax", &ff_ctx->mb_qmax, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL },
        {"lmin", &aux_cfg.lmin, CONF_TYPE_FLOAT, CONF_RANGE, 0.01, 255.0, NULL}, // XXX
        {"lmax", &aux_cfg.lmax, CONF_TYPE_FLOAT, CONF_RANGE, 0.01, 255.0, NULL}, // XXX
        {"vqdiff", &ff_ctx->max_qdiff, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
        {"vqcomp", &ff_ctx->qcompress, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 1.0, NULL},
        {"vqblur", &ff_ctx->qblur, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 1.0, NULL},
        {"vb_qfactor", &ff_ctx->b_quant_factor, CONF_TYPE_FLOAT, CONF_RANGE, -31.0, 31.0, NULL},
        {"vmax_b_frames", &ff_ctx->max_b_frames, CONF_TYPE_INT, CONF_RANGE, 0, FF_MAX_B_FRAMES, NULL},
        {"vrc_strategy", &ff_ctx->rc_strategy, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
        {"vb_strategy", &ff_ctx->b_frame_strategy, CONF_TYPE_INT, CONF_RANGE, 0, 10, NULL},
        {"vb_qoffset", &ff_ctx->b_quant_offset, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 31.0, NULL},
        {"vlelim", &ff_ctx->luma_elim_threshold, CONF_TYPE_INT, CONF_RANGE, -99, 99, NULL},
        {"vcelim", &ff_ctx->chroma_elim_threshold, CONF_TYPE_INT, CONF_RANGE, -99, 99, NULL},
        {"vpsize", &aux_cfg.packet_size, CONF_TYPE_INT, CONF_RANGE, 0, 100000000, NULL},
        {"vstrict", &ff_ctx->strict_std_compliance, CONF_TYPE_INT, CONF_RANGE, -99, 99, NULL },
        {"vi_qfactor", &ff_ctx->vi_qfactor, CONF_TYPE_FLOAT, CONF_RANGE, -31.0, 31.0, NULL},
        {"vi_qoffset", &ff_ctx->vi_qoffset, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 31.0, NULL},
        {"vqsquish", &ff_ctx->rc_qsquish, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 99.0, NULL},
        {"vqmod_amp", &ff_ctx->rc_qmod_amp, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 99.0, NULL},
        {"vqmod_freq", &ff_ctx->rc_qmod_freq, CONF_TYPE_INT, 0, 0, 0, NULL},
        // XXX
        // {"vrc_eq", &ff_ctx->rc_eq, CONF_TYPE_STRING, 0, 0, 0, NULL},
        // {"vrc_override", &lavc_param_rc_override_string, CONF_TYPE_STRING, 0, 0, 0, NULL},
        // XXX
        {"vrc_maxrate", &aux_cfg.rc_max_rate, CONF_TYPE_INT, CONF_RANGE, 0, 24000000, NULL},
        {"vrc_minrate", &aux_cfg.rc_min_rate, CONF_TYPE_INT, CONF_RANGE, 0, 24000000, NULL},
        {"vrc_buf_size", &aux_cfg.rc_buffer_size, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
        {"vrc_buf_aggressivity", &ff_ctx->rc_buffer_aggressivity, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 99.0, NULL},
        {"vrc_init_cplx", &ff_ctx->rc_initial_cplx, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 9999999.0, NULL},
        {"idct", &ff_ctx->idct_algo, CONF_TYPE_INT, CONF_RANGE, 0, 20, NULL},
        {"vfdct", &ff_ctx->dct_algo, CONF_TYPE_INT, CONF_RANGE, 0, 10, NULL},
        {"lumi_mask", &ff_ctx->lumi_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
        {"tcplx_mask", &ff_ctx->temporal_cplx_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
        {"scplx_mask", &ff_ctx->spatial_cplx_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
        {"p_mask", &ff_ctx->p_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
        {"dark_mask", &ff_ctx->dark_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
        {"pred", &ff_ctx->prediction_method, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
        {"debug", &ff_ctx->debug, CONF_TYPE_INT, CONF_RANGE, 0, 100000000, NULL}, // XXX
        {"precmp", &ff_ctx->me_pre_cmp, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
        {"cmp", &ff_ctx->me_cmp, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
        {"subcmp", &ff_ctx->me_sub_cmp, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
        {"mbcmp", &ff_ctx->mb_cmp, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
        {"ildctcmp", &ff_ctx->ildct_cmp, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
        {"predia", &ff_ctx->pre_dia_size, CONF_TYPE_INT, CONF_RANGE, -2000, 2000, NULL},
        {"dia", &ff_ctx->dia_size, CONF_TYPE_INT, CONF_RANGE, -2000, 2000, NULL},
        {"last_pred", &ff_ctx->last_predictor_count, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
        {"preme", &ff_ctx->pre_me, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
        {"subq", &ff_ctx->me_subpel_quality, CONF_TYPE_INT, CONF_RANGE, 0, 8, NULL},
        {"me_range", &ff_ctx->me_range, CONF_TYPE_INT, CONF_RANGE, 0, 16000, NULL},
        {"ibias", &ff_ctx->intra_quant_bias, CONF_TYPE_INT, CONF_RANGE, -512, 512, NULL},
        {"pbias", &ff_ctx->inter_quant_bias, CONF_TYPE_INT, CONF_RANGE, -512, 512, NULL},
        {"coder", &ff_ctx->coder_type, CONF_TYPE_INT, CONF_RANGE, 0, 10, NULL},
        {"context", &ff_ctx->context_model, CONF_TYPE_INT, CONF_RANGE, 0, 10, NULL},
        // XXX
        //{"intra_matrix", &lavc_param_intra_matrix, CONF_TYPE_STRING, 0, 0, 0, NULL},
        //{"inter_matrix", &lavc_param_inter_matrix, CONF_TYPE_STRING, 0, 0, 0, NULL},
        // XXX
        {"nr", &ff_ctx->noise_reduction, CONF_TYPE_INT, CONF_RANGE, 0, 1000000, NULL},
        {"sc_threshold", &ff_ctx->scenechange_threshold, CONF_TYPE_INT, CONF_RANGE, -1000000, 1000000, NULL},
        {"inter_threshold", &ff_ctx->inter_threshold, CONF_TYPE_INT, CONF_RANGE, -1000000, 1000000, NULL},
        {"intra_dc_precision", &ff_ctx->intra_dc_precision, CONF_TYPE_INT, CONF_RANGE, 0, 16, NULL},
        
        {"threads", &pd->threads, CONF_TYPE_INT, CONF_RANGE, 1, 7, NULL}, // XXX
        {NULL, NULL, 0, 0, 0, 0, NULL},
    };        

    module_read_config(pd->codec_name, MOD_NAME, "ffmpeg", ffmpeg_config, tc_config_dir);
    if (verbose_flag & TC_DEBUG) {
        tc_log_info(MOD_NAME, "Using the following FFMPEG parameters:");
        module_print_config("["MOD_NAME"] ", ffmpeg_config);
    }
    
    /* setup indirect values */
    ff_ctx->bit_rate_tolerance = aux_cfg.vrate_tolerance * 1000;
    ff_ctx->lmin= (int)(FF_QP2LAMBDA * aux_cfg.lmin + 0.5);
    ff_ctx->lmax= (int)(FF_QP2LAMBDA * aux_cfg.lmax + 0.5);
    
    ff_ctx->rc_max_rate = aux_cfg.rc_max_rate * 1000;
    ff_ctx->rc_min_rate = aux_cfg.rc_min_rate * 1000;
    ff_ctx->rc_buffer_size = aux_cfg.rc_buffer_size * 1024;
    
    ff_ctx->flags |= aux_cfg.v4mv_flag;
    ff_ctx->flags |= aux_cfg.vdpart_flag;
    ff_ctx->flags |= aux_cfg.psnr_flag;
    ff_ctx->flags |= aux_cfg.qpel_flag;
    ff_ctx->flags |= aux_cfg.trell_flag;
    ff_ctx->flags |= aux_cfg.aic_flag;
    ff_ctx->flags |= aux_cfg.umv_flag;
    ff_ctx->flags |= aux_cfg.cbp_flag;
    ff_ctx->flags |= aux_cfg.mv0_flag;
    ff_ctx->flags |= aux_cfg.qp_rd_flag;
    ff_ctx->flags |= aux_cfg.gmc_flag;
    ff_ctx->flags |= aux_cfg.trunc_flag;
    ff_ctx->flags |= aux_cfg.ss_flag;
    ff_ctx->flags |= aux_cfg.alt_flag;
    ff_ctx->flags |= aux_cfg.ilme_flag;
    ff_ctx->flags |= aux_cfg.soff_flag;
            
    /* special flags (XXX) */
    if (aux_cfg.gray_flag) {
        ff_ctx->flags |= CODEC_FLAG_GRAY;
    }
    if (aux_cfg.norm_aqp_flag) {
        ff_ctx->flags |= CODEC_FLAG_NORMALIZE_AQP;
    }
    if (aux_cfg.closed_gop_flag) {
        ff_ctx->flags |= CODEC_FLAG_CLOSED_GOP;
        /* closedgop requires scene detection to be disabled separately */
        ff_ctx->scenechange_threshold = 1000000000;
    }
 
    ff_ctx->->rtp_payload_size = aux_cfg.packet_size;
    if (aux_cfg.packet_size > 0) {
        ff_ctx->rtp_mode = 1;
    }
    pd->do_psnr = aux_cfg.psnr_flag;
                
    return TC_EXPORT_OK;
}
