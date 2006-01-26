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
#include "frc_table.h"
#include "framebuffer.h"
#include "filter.h"
#include "probe_export.h"
#include "libioaux/configs.h"
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

#define LINE_LEN 128
#define CODEC_NAME_LEN 16

#define VCODEC_DEFAULT "mpeg4"
#define ACODEC_DEFAULT "ac3"

#define INTRA_MATRIX 0
#define INTER_MATRIX 1

static const char *ffmpeg_help = ""
    "Overview:\n"
    "\tthis module provides access to ffmpeg codec library, libavcodec.\n"
    "\tlibavcodec can encode audio or video using a wide range of codecs,\n"
    "\tis fast and delivers high quality streams.\n"
    "Options:\n"
    "\thelp      produce module overview and options explanations\n"
    "\tlist      show all supported codecs\n"
    "\tvcodec    select video codec to use to encode video\n"
    "\tacodec    select video codec to use to encode audio\n"
    "\tnofilecfg do not parse configuration file\n";

/*
 * FIXME:
 * - codec naming issues
 * - aud buffer audit
 * - CODEC_* vs TC_CODEC_*
 * - restore missing features
 */

// XXX generalize
static const double asr_table[8] = {
    1.0,

    1.0,
    4.0/3.0,
    16.0/9.0,
    221.0/100.0,

    1.0,
    1.0,
    1.0
};

static const int par_table[8][2] = {
    { 1, 1 },
    
    { 1, 1 },
    { 1200, 1100 },
    { 1000, 1100 },
    { 1600, 1100 },
    { 4000, 3300 },

    { 1, 1 },
    { 1, 1 }
};
    
static const int ffmpeg_codecs_in[] = {
    TC_CODEC_RGB, TC_CODEC_YUV422P, TC_CODEC_YUV420P,
    TC_CODEC_PCM,
    TC_CODEC_ERROR
};

static const int ffmpeg_codecs_out[] = {
    TC_CODEC_MPEG1VIDEO, TC_CODEC_MPEG2VIDEO, TC_CODEC_MPEG4VIDEO,
    TC_CODEC_DV, TC_CODEC_DIVX3, TC_CODEC_MP42,
    TC_CODEC_MJPG, TC_CODEC_RV10, TC_CODEC_WMV1, TC_CODEC_WMV2,
    TC_CODEC_HUFFYUV, TC_CODEC_H263P, TC_CODEC_H263I, TC_CODEC_FFV1,
    TC_CODEC_ASV1, TC_CODEC_ASV2, TC_CODEC_H264,
    TC_CODEC_MP2, TC_CODEC_AC3,
    TC_CODEC_ERROR
};

#define IS_MJPEG(pd) (((pd)->tc_vcodec_id == TC_CODEC_MJPG) ?1 :0)
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

typedef struct ffmpegprivatedata_ FFmpegPrivateData;
typedef void (*prepare_encode_fn)(FFmpegPrivateData *pd,
                                  vframe_list_t *inframe,
                                  vframe_list_t *outframe);
/* FIXME: add comments */
struct ffmpegprivatedata_ {
    int multipass;
    int threads;
    
    /* FXIME: drop? */
    uint8_t *conv_buf;
    size_t conv_buf_size;

    uint8_t *aud_buf;
    size_t aud_buf_size;
    
    AVCodec *vid_codec;
    AVFrame *venc_frame;
    AVCodecContext *vid_ctx;
   
    AVCodec *aud_codec;
    AVCodecContext *aud_ctx;
    
    int pix_fmt;
    int aud_fmt;
    
    FILE *stats_file;
    size_t size;
    int encoded_frames;
    int frames;
    int interlacing_active;
    int interlacing_top_first;
    int do_psnr;

    int tc_vcodec_id;
    int tc_acodec_id;
    char vcodec_name[CODEC_NAME_LEN];
    char acodec_name[CODEC_NAME_LEN];
    
    char *codecs_desc; /* rarely used, so dynamically created if needed */
    char conf_str[CONF_STR_SIZE]; /* will be always avalaible */

    int levels_handle;
    prepare_encode_fn prepare_encode;
};

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
   
    char *intra_matrix_file;
    char *inter_matrix_file;
   
    char *rc_override_string;
    
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
    int trunc_flag;
} FFmpegConfig;


/* helpers used by helpers :) */
static int have_out_codec(int codec);
static double psnr(double d); 
static void log_psnr(FFmpegPrivateData *pd);
static void translate_codec_name(const char *user_name, char *lavc_name,
                                 size_t len);
static char* describe_out_codecs(void);
static void translate_settings(FFMpegPrivateData *pd, const vob_t *vob);
static int setup_codec(FFMpegPrivateData *pd, const char *options, int what);
static int setup_pix_fmt(FFmpegPrivateData *pd);
static void setup_ex_par(FFmpegPrivateData *pd, const vob_t *vob);
static void setup_dar_sar(FFmpegPrivateData *pd, const vob_t *vob);
static void setup_rc_override(FFmpegPrivateData *pd, FFmpegConfig *cfg);
static int setup_buffers(FFmpegPrivateData *pd, const vob_t *vob);


static void prepare_encode_yuv420(FFmpegPrivateData *pd,
                                  vframe_list_t *inframe,
                                  vframe_list_t *outframe);
static void prepare_encode_yuv422(FFmpegPrivateData *pd,
                                  vframe_list_t *inframe,
                                  vframe_list_t *outframe);
static void prepare_encode_rgb(FFmpegPrivateData *pd,
                                  vframe_list_t *inframe,
                                  vframe_list_t *outframe);

/* main helpers */
static int startup_libavcodec(FFmpegPrivateData *pd);
static int setup_multipass(FFmpegPrivateData *pd, const vob_t *vob);
static void reset_module(FFmpegPrivateData *pd);
static void set_lavc_defaults(FFmpegPrivateData *pd);
static void set_conf_defaults(FFmpegConfig *cfg);
static int parse_options(FFmpegPrivateData *pd,
                         const char *options, const vob_t *vob);
static int read_config_file(FFmpegPrivateData *pd);
static void config_summary(FFmpegPrivateData *pd, const vob_t *vob);


#define RETURN_IF_ERROR(ret, msg) \
    if (ret == TC_EXPORT_ERROR) { \
        tc_log_error(MOD_NAME, msg); \
        return TC_EXPORT_ERROR; \
    }

#define RETURN_IF_VERIFIED(condition, msg) \
    if (condition) { \
        tc_log_error(MOD_NAME, msg); \
        return TC_EXPORT_ERROR; \
    }

static int ffmpeg_configure(TCModuleInstance *self,
                            const char *options, vob_t *vob)
{
    FFmpegPrivateData *pd = NULL;
    int ret;
   
    RETURN_IF_VERIFIED((!self || !options || !vob), 
                        "configure: bad instance data reference");
    pd = self->userdata;
  
    set_lavc_defaults(pd);

    ret = parse_options(pd, options, vob);
    RETURN_IF_ERROR(ret, "failed to setup user configuration");
    
    ret = setup_buffers(pd, vob);
    RETURN_IF_ERROR(ret, "can't allocate internal buffers.");
    
    if (!optstr_lookup(options, "nofilecfg")) {
        ret = read_config_file(pd);
        RETURN_IF_ERROR(ret, "failed to read configuration from file");
    }
    
    ret = setup_multipass(pd, vob);
    RETURN_IF_ERROR(ret, "failed to setup multipass settings");
    
    ret = startup_libavcodec(pd);
    RETURN_IF_ERROR(ret, "failed to startup libavcodec");
    
    /* propagate extradata */
    self->extradata = pd->vid_ctx->extradata;
    self->extradata_size = pd->vid_ctx->extradata_size;
    
    if (verbose) {
        config_summary(pd, vob);
    }
    return TC_EXPORT_OK;
}

/* DO NOT FREE EXTRADATA! Is handled by libavcodec! */
static int ffmpeg_stop(TCModuleInstance *self)
{
    FFmpegPrivateData *pd = NULL;
    
    RETURN_IF_VERIFIED((!self), "stop: bad instance data reference");
    
    pd = self->userdata;

    log_psnr(pd);
    
    if (pd->conv_buf != NULL) {
        tc_free(pd->conv_buf);
        pd->conv_buf = NULL;
        pd->conv_buf_size = 0;
    }
    /* video */
    if (pd->vid_codec != NULL) {
        avcodec_close(pd->vid_ctx);
        pd->vid_codec = NULL;
    }
    /* audio */
    if (pd->aud_codec != NULL) {
        avcodec_close(pd->aud_ctx);
        pd->aud_codec = NULL;
    }

    if (pd->vid_ctx != NULL) {
        if (pd->vid_ctx->rc_override != NULL) {
            tc_free(pd->vid_ctx->rc_override);
            pd->vid_ctx->rc_override = NULL;
        }
        if (pd->vid_ctx->intra_matrix != NULL) {
            tc_free(pd->vid_ctx->intra_matrix);
            pd->vid_ctx->intra_matrix = NULL;
        }
        if (pd->vid_ctx->inter_matrix != NULL) {
            tc_free(pd->vid_ctx->inter_matrix);
            pd->vid_ctx->inter_matrix = NULL;
        }
    }

    if (pd->stats_file) {
        fclose(pd->stats_file);
        pd->stats_file = NULL;
    }
    
    reset_module(pd);
    return TC_EXPORT_OK;
}


static int ffmpeg_init(TCModuleInstance *self)
{
    FFmpegPrivateData *pd = NULL;
   
    RETURN_IF_VERIFIED((!self), "init: bad instance data reference");
    
    pd = tc_malloc(sizeof(FFmpegPrivateData));
    
    RETURN_IF_VERIFIED((!pd), "init: can't allocate FFmpeg private data");

    reset_module(pd);

    pthread_mutex_lock(&init_avcodec_lock);
    avcodec_init();
    avcodec_register_all();
    pthread_mutex_unlock(&init_avcodec_lock);

    pd->vid_ctx = avcodec_alloc_context();
    pd->aud_ctx = avcodec_alloc_context();
    pd->venc_frame = avcodec_alloc_frame();

    if ((!pd->vid_ctx || !pd->aud_ctx) || !pd->venc_frame) {
        fprintf(stderr, "[%s] could not allocate enough memory.\n", MOD_NAME);
        return TC_EXPORT_ERROR;
    }

    self->userdata = pd;

    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }
    return TC_EXPORT_OK;
}

static int ffmpeg_fini(TCModuleInstance *self)
{
    FFmpegPrivateData *pd = NULL;

    RETURN_IF_VERIFIED((!self), "fini: bad instance data reference");

    ffmpeg_stop(self);
    
    pd = self->userdata;

    if (pd->venc_frame) {
      free(pd->venc_frame);
      pd->venc_frame = NULL;
    }

    if (pd->vid_ctx != NULL) {
      tc_free(pd->vid_ctx);
      pd->vid_ctx = NULL;
    }

    if (pd->codecs_desc) {
        tc_free(pd->codecs_desc);
        pd->codecs_desc = NULL;
    }

    /* DO NOT FREE EXTRADATA! Is handled by libavcodec! */
    tc_free(self->userdata);
    self->userdata = NULL;
    return TC_EXPORT_OK;
}


static const char *ffmpeg_inspect(TCModuleInstance *self,
                                const char *param)
{
    FFmpegPrivateData *pd = NULL;
    if (!self) {
        tc_log_error(MOD_NAME, "inspect: bad instance data reference");
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
             * describe_out_codecs() failed:
             * temporary (we hope) error...
             */
            return "internal error";
        }
        return pd->codecs_desc;
    }

    if (optstr_lookup(param, "vcodec")) {
        /* direct answer */
        return pd->vcodec_name;
    }
    if (optstr_lookup(param, "acodec")) {
        /* direct answer */
        return pd->acodec_name;
    }
    if (optstr_lookup(param, "nofilecfg")) {
        return "0";
    }
    
    tc_snprintf(pd->conf_str, CONF_STR_SIZE, "vcodec=%s:acodec=%s",
                pd->vcodec_name, pd->acodec_name); 
    return pd->conf_str;
}

static int ffmpeg_encode_video(TCModuleInstance *self,
                              vframe_list_t *inframe, vframe_list_t *outframe)
{
    int out_size;
    FFmpegPrivateData *pd = NULL;

    RETURN_IF_VERIFIED((!self), "encode_video: bad instance data reference");
    
    pd = self->userdata;

    pd->venc_frame->interlaced_frame = pd->interlacing_active;
    pd->venc_frame->top_field_first = pd->interlacing_top_first;

    pd->prepare_encode(pd, inframe, outframe);

    pthread_mutex_lock(&init_avcodec_lock);
    out_size = avcodec_encode_video(pd->vid_ctx, outframe->video_buf,
                                    outframe->video_size, pd->venc_frame);
    pthread_mutex_unlock(&init_avcodec_lock);

    if (out_size < 0) {
        tc_log_error(MOD_NAME, "encoder error: size (%d)", out_size);
        return TC_EXPORT_ERROR;
    }

    outframe->video_size = out_size; // XXX: correct?
    if (pd->vid_ctx->coded_frame->key_frame) {
        outframe->attributes |= TC_FRAME_IS_KEYFRAME;
    }

    /* store stats if there are any */
    if (pd->vid_ctx->stats_out && pd->stats_file) {
        fprintf(pd->stats_file, "%s", pd->vid_ctx->stats_out);
    }

    return TC_EXPORT_OK;
}

static int ffmpeg_encode_audio(TCModuleInstance *self,
                               aframe_list_t *inframe, aframe_list_t *outframe)
{
    int out_size;
    FFmpegPrivateData *pd = NULL;

    RETURN_IF_VERIFIED((!self), "encode_audio: bad instance data reference");
    
    pd = self->userdata;

    return TC_EXPORT_OK;
}


/*************************************************************************/

static const TCModuleInfo ffmpeg_info = {
    .features    = TC_MODULE_FEATURE_ENCODE|TC_MODULE_FEATURE_VIDEO
                   |TC_MODULE_FEATURE_AUDIO,
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
    .encode_audio = ffmpeg_encode_audio,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &ffmpeg_class;
}

/*************************************************************************
 * support functions: implementation                                     *
 *************************************************************************/

static void translate_codec_name(const char *user_name, char *lavc_name,
                                 size_t len)
{
    const char *name = user_name;
    if (!strcmp(user_name, "dv")) {
        name = "dvvideo";
    }
    strlcpy(lavc_name, name, len);
}

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

static void log_psnr(FFmpegPrivateData *pd)
{
    if(pd != NULL && pd->do_psnr) {
        double f = pd->vid_ctx->width * pd->vid_ctx->height * 255.0 * 255.0;

        f *= pd->vid_ctx->coded_frame->coded_picture_number;

        tc_log_info(MOD_NAME, "PSNR: Y:%2.2f, Cb:%2.2f, Cr:%2.2f, All:%2.2f",
                              psnr(pd->vid_ctx->error[0]/f),
                              psnr(pd->vid_ctx->error[1]*4/f),
                              psnr(pd->vid_ctx->error[2]*4/f),
                              psnr((pd->vid_ctx->error[0]
                                  + pd->vid_ctx->error[1]
                                  + pd->vid_ctx->error[2])
                                  / (f * 1.5)));
    }
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

/*
 * I use avpicture_fill instead of YUV_INIT_PLANES because the former
 * also set linesize[] and friends in AVFrame data
 */
static void prepare_encode_yuv420(FFmpegPrivateData *pd,
                                  vframe_list_t *inframe,
                                  vframe_list_t *outframe)
{
    if (pd->tc_vcodec_id == TC_CODEC_HUFFYUV) {
        uint8_t *src[3];
        YUV_INIT_PLANES(src, inframe->video_buf, IMG_YUV_DEFAULT,
                        pd->vid_ctx->width, pd->vid_ctx->height);
        avpicture_fill((AVPicture*)pd->venc_frame, pd->conv_buf,
                       PIX_FMT_YUV422P,
                       pd->vid_ctx->width, pd->vid_ctx->height);
        ac_imgconvert(src, IMG_YUV_DEFAULT, pd->venc_frame->data,
                      IMG_YUV422P,
                      pd->vid_ctx->width, pd->vid_ctx->height);
    } else {
        avpicture_fill((AVPicture*)pd->venc_frame, inframe->video_buf,
                       PIX_FMT_YUV420P,
                       pd->vid_ctx->width, pd->vid_ctx->height);
    }
}

static void prepare_encode_yuv422(FFmpegPrivateData *pd,
                                  vframe_list_t *inframe,
                                  vframe_list_t *outframe)
{
    // XXX
    if (pd->tc_vcodec_id == TC_CODEC_HUFFYUV) {
        YUV_INIT_PLANES(pd->venc_frame->data, inframe->video_buf,
                        IMG_YUV422P,
                        pd->vid_ctx->width, pd->vid_ctx->height);
    } else {
        uint8_t *src[3];
        YUV_INIT_PLANES(src, inframe->video_buf,
                        IMG_YUV422P,
                        pd->vid_ctx->width, pd->vid_ctx->height);
        avpicture_fill((AVPicture *)pd->venc_frame, pd->conv_buf,
                        PIX_FMT_YUV420P,
                        pd->vid_ctx->width, pd->vid_ctx->height);
        ac_imgconvert(src, IMG_YUV422P, pd->venc_frame->data,
                      IMG_YUV420P,
                      pd->vid_ctx->width, pd->vid_ctx->height);
    }
}

// XXX
static void prepare_encode_rgb(FFmpegPrivateData *pd,
                               vframe_list_t *inframe,
                               vframe_list_t *outframe)
{
    uint8_t *src[3];
    YUV_INIT_PLANES(src, inframe->video_buf,
                    IMG_RGB_DEFAULT,
                    pd->vid_ctx->width, pd->vid_ctx->height);
    avpicture_fill((AVPicture *)pd->venc_frame, pd->conv_buf,
                   PIX_FMT_YUV420P,
                   pd->vid_ctx->width, pd->vid_ctx->height);
    ac_imgconvert(src, IMG_RGB_DEFAULT, pd->venc_frame->data,
                  IMG_YUV420P,
                  pd->vid_ctx->width, pd->vid_ctx->height);
}

static int startup_libavcodec(FFmpegPrivateData *pd)
{
    int ret = 0;
    
    ret = avcodec_open(pd->vid_ctx, pd->vid_codec);
    RETURN_IF_VERIFIED((ret < 0), "startup: could not open FFMPEG video codec");
    
    ret = avcodec_open(pd->aud_ctx, pd->aud_codec);
    RETURN_IF_VERIFIED((ret < 0), "startup: could not open FFMPEG audio codec");

    if ((pd->threads < 1) || (pd->threads > 7)) {
        tc_log_warn(MOD_NAME, "startup: thread count out of range"
                              " (should be [0-7]), reset to 1");
        pd->threads = 1;
    }

    /* XXX: right place? I'm not convinced */
    /* free second pass buffer, its not needed anymore */
    if (pd->vid_ctx->stats_in != NULL) {
        tc_free(pd->vid_ctx->stats_in);
        pd->vid_ctx->stats_in = NULL;
    }

    return TC_EXPORT_OK;
}

// XXX
static void setup_ex_par(FFmpegPrivateData *pd, const vob_t *vob)
{
    int code;
    if (!pd || !pd->vid_ctx || !vob) {
        tc_log_error(MOD_NAME, "setup_ex_par: bad instance data reference");
        return;
    }
    
    if (vob->ex_par > 0) {
        if (vob->ex_par >= 1 && vob->ex_par <= 5) {
            code = vob->ex_par;
        } else {
            tc_log_warn(MOD_NAME, "unknown PAR code (not in [1..5]),"
                                  " defaulting to 1/1");
            code = 1;
        }
        pd->vid_ctx->sample_aspect_ratio.num = par_table[code][0];
        pd->vid_ctx->sample_aspect_ratio.den = par_table[code][1];
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

// XXX
static void setup_dar_sar(FFmpegPrivateData *pd, const vob_t *vob)
{
    int asr_code;
    double dar, sar;

    if (!pd || !pd->vid_ctx || !vob) {
        tc_log_error(MOD_NAME, "setup_dar_sar: bad instance data reference");
        return;
    }

    if (vob->ex_asr > 0) {
        if (vob->ex_asr >= 1 && vob->ex_asr <= 4) {
            asr_code = vob->ex_asr;
        } else {
            tc_log_warn(MOD_NAME, "Bad parameter value for ASR (not in [1-4]),"
                                  " resetting DAR to 1/1");
            asr_code = 0;
        }
        dar = asr_table[asr_code];

        sar = dar * ((double)vob->ex_v_height / (double)vob->ex_v_width);
        tc_log_info(MOD_NAME, "Display aspect ratio calculated as %f", dar);
        tc_log_info(MOD_NAME, "Sample aspect ratio calculated as %f", sar);
        pd->vid_ctx->sample_aspect_ratio.num = (int)(sar * 1000);
        pd->vid_ctx->sample_aspect_ratio.den = 1000;
    } else {
        tc_log_info(MOD_NAME, "Set DAR to input (1/1)");
        pd->vid_ctx->sample_aspect_ratio.num = 1;
        pd->vid_ctx->sample_aspect_ratio.den = 1;
    }
}


static int setup_pix_fmt(FFmpegPrivateData *pd)
{
    if (pd->tc_vcodec_id == TC_CODEC_HUFFYUV) {
        pd->vid_ctx->pix_fmt = PIX_FMT_YUV422P;
    } else {
        switch (pd->pix_fmt) {
          case CODEC_YUV:
            pd->prepare_encode = prepare_encode_yuv420;
            pd->vid_ctx->pix_fmt = (IS_MJPEG(pd)
                                    ?PIX_FMT_YUVJ420P :PIX_FMT_YUV420P);
            break;
          case CODEC_YUV422:
            pd->prepare_encode = prepare_encode_yuv422;
            pd->vid_ctx->pix_fmt = (IS_MJPEG(pd)
                                    ?PIX_FMT_YUVJ422P :PIX_FMT_YUV422P);
            break;
          case CODEC_RGB:
            pd->prepare_encode = prepare_encode_rgb;
            pd->vid_ctx->pix_fmt = (IS_MJPEG(pd)
                                    ?PIX_FMT_YUVJ420P :PIX_FMT_YUV420P);
            break;
          default:
            tc_log_warn(MOD_NAME, "Unknown pixel format %d.", pd->pix_fmt);
            return -1;
        }
    }
    return 0;
}

static uint16_t *load_matrix(const char *filename, int type)
{        
    uint16_t *matrix = NULL;
    
    if (filename != NULL) {
        matrix = tc_malloc(sizeof(uint16_t) * TC_MATRIX_SIZE);
        int ret = tc_read_matrix(filename, NULL, matrix);
        
        if (ret == 0) {
            tc_log_info(MOD_NAME, "using user specified %s matrix",
                        (type == INTER_MATRIX) ?"inter" :"intra");
        } else {
            tc_free(matrix);
            matrix = NULL;
        }
    }
    return matrix;
}

static void config_summary(FFmpegPrivateData *pd, const vob_t *vob)
{
    if (vob->divxmultipass == 3) {
        tc_log_info(MOD_NAME, "    single-pass session: 3 (VBR)");
        tc_log_info(MOD_NAME, "          VBR-quantizer: %d",
                              vob->divxbitrate);
    } else {
        tc_log_info(MOD_NAME, "     multi-pass session: %d",
                              vob->divxmultipass);
        tc_log_info(MOD_NAME, "      bitrate [kBits/s]: %d",
                              pd->vid_ctx->bit_rate/1000);
    }

    tc_log_info(MOD_NAME, "  max keyframe interval: %d\n",
                          vob->divxkeyframes);
    tc_log_info(MOD_NAME, "             frame rate: %.2f\n",
                          vob->ex_fps);
    tc_log_info(MOD_NAME, "            color space: %s\n",
                         (pd->pix_fmt == CODEC_RGB) ?"RGB24":
                        ((pd->pix_fmt == CODEC_YUV) ?"YUV420P" :"YUV422"));
    tc_log_info(MOD_NAME, "             quantizers: %d/%d\n",
                          pd->vid_ctx->qmin, pd->vid_ctx->qmax);
}

static void translate_settings(FFMpegPrivateData *pd, const vob_t *vob)
{
    if ((!pd || !pd->vid_ctx) || !vob) {
        tc_log_error(MOD_NAME, "translate_settings: "
                               "bad instance data reference");
        return;
    }
  
    pd->aud_fmt = vob->ex_a_codec;
    pd->aud_ctx->bit_rate = vob->mp3bitrate * 1000;
    pd->aud_ctx->channels = vob->dm_chan;
    pd->aud_ctx->sample_rate = vob->a_rate;
    
    pd->pix_fmt = vob->im_v_codec;
    pd->vid_ctx->bit_rate = vob->divxbitrate * 1000;
    pd->vid_ctx->width = vob->ex_v_width;
    pd->vid_ctx->height = vob->ex_v_height;
    pd->vid_ctx->qmin = vob->min_quantizer;
    pd->vid_ctx->qmax = vob->max_quantizer;
   
    if (vob->ex_frc >= 1 && vob->ex_frc <= 8) {
        pd->vid_ctx->time_base.num = frc_values[vob->ex_frc][0];
        pd->vid_ctx->time_base.den = frc_values[vob->ex_frc][1];
    } else {
        /* XXX */
        pd->vid_ctx->time_base.den = (int)(vob->ex_fps * 1000.0);
        pd->vid_ctx->time_base.num = 1000;
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

    pd->vid_ctx->flags |= pd->interlacing_active ?
                            CODEC_FLAG_INTERLACED_DCT :0;
    pd->vid_ctx->flags |= pd->interlacing_active ?
                            CODEC_FLAG_INTERLACED_ME :0;
}

#define WANT_VIDEO (what == TC_VIDEO)
#define WANT_AUDIO (what == TC_AUDIO)

static int setup_codec(FFMpegPrivateData *pd, const char *options, int what)
{
    int ret;
    char user_codec[CODEC_NAME_LEN];
    char *codec_name = WANT_VIDEO ?pd->vcodec_name :pd->acodec_name;
    int *codec_id = WANT_VIDEO ?&(pd->tc_vcodec_id) :&(pd->tc_acodec_id);
    AVCodec **codec = WANT_VIDEO ?&(pd->vid_codec) :&(pd->aud_codec);
    
    ret = optstr_get(options, WANT_VIDEO ?"vcodec" :"acodec", "%20[^:]",
                     user_codec);
    if (ret > 0) {
        tc_strstrip(user_codec);
        translate_codec_name(user_codec, codec_name, CODEC_NAME_LEN);
    }    
    
    *codec_id = tc_codec_from_string(codec_name);
    if (!have_out_codec(*codec_id)) {
        tc_log_error(MOD_NAME, "unknown %s codec: '%s'",
                     WANT_VIDEO ?"video" :"audio", user_codec);
        return TC_EXPORT_ERROR;
    }
    *codec = avcodec_find_encoder_by_name(codec_name);
    if (*codec) {
        tc_log_error(MOD_NAME, "could not find a FFMPEG %s codec for '%s'",
                     WANT_VIDEO ?"video" :"audio", user_codec);
        return TC_EXPORT_ERROR;
    } else {
        if (verbose) {
            tc_log_info(MOD_NAME, "using FFMPEG %s codec '%s'",
                        WANT_VIDEO ?"video" :"audio", user_codec);
        }
    }
    return TC_EXPORT_OK;
}

#undef WANT_VIDEO
#undef WANT_AUDIO

static int parse_options(FFmpegPrivateData *pd, const char *options,
                         const vob_t *vob)
{
    int ret;

    translate_settings(pd, vob);

    if ((setup_codec(pd, options, 1) != TC_EXPORT_OK)
     || (setup_codec(pd, options, 0) != TC_EXPORT_OK)) {
        /* error message already print'd out by setup_codec */
        return TC_EXPORT_ERROR;
    }

    ret = setup_pix_fmt(pd); 
    if (ret != 0) {
        tc_log_error(MOD_NAME, "unable to setup for color space %d.",
                     pd->pix_fmt);
        return TC_EXPORT_ERROR;
    }
    if (IS_MJPEG(pd) && pd->levels_handle == -1) {
        tc_log_info(MOD_NAME, "output is mjpeg, extending range from "
                              "YUV420P to YUVJ420P (full range)");

        pd->levels_handle = plugin_get_handle("levels=input=16-240");
        if(pd->levels_handle == -1) {
            tc_log_warn(MOD_NAME, "cannot load levels filter");
        }
    }

    if (probe_export_attributes & TC_PROBE_NO_EXPORT_GOP) {
        pd->vid_ctx->gop_size = vob->divxkeyframes;
    } else {
        if (pd->tc_vcodec_id == TC_CODEC_MPEG1VIDEO
         || pd->tc_vcodec_id == TC_CODEC_MPEG2VIDEO) {
            pd->vid_ctx->gop_size = 15;
            /* conservative default for mpeg1/2 svcd/dvd */
        } else {
            pd->vid_ctx->gop_size = 250;
            /* reasonable default for mpeg4 (and others) */
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

    return TC_EXPORT_OK;
}

static int setup_multipass(FFmpegPrivateData *pd, const vob_t *vob)
{
    size_t fsize;
    
    if ((!pd || !pd->vid_ctx) || !vob) {
        tc_log_warn(MOD_NAME, "multipass: bad references for parsing");
        return TC_EXPORT_ERROR;
    }
    if ((vob->divxmultipass == 1 || vob->divxmultipass == 2)
      && !pd->multipass) {
        tc_log_warn(MOD_NAME, "this codec does not support multipass "
                              "encoding.");
        return TC_EXPORT_ERROR;
    }

    switch (vob->divxmultipass) {
      case 1:
        pd->vid_ctx->flags |= CODEC_FLAG_PASS1;
        pd->stats_file = fopen(vob->divxlogfile, "w");
        if (!pd->stats_file) {
          tc_log_warn(MOD_NAME, "could not open 2pass log file \"%s\" for "
                                "writing.", vob->divxlogfile);
          return TC_EXPORT_ERROR;
        }
        break;
      case 2:
        pd->vid_ctx->flags |= CODEC_FLAG_PASS2;
        pd->stats_file = fopen(vob->divxlogfile, "r");
        if (!pd->stats_file) {
          tc_log_warn(MOD_NAME, "could not open 2pass log file \"%s\" for "
                                "reading.", vob->divxlogfile);
          return TC_EXPORT_ERROR;
        }
        fseek(pd->stats_file, 0, SEEK_END);
        fsize = ftell(pd->stats_file);
        fseek(pd->stats_file, 0, SEEK_SET);

        pd->vid_ctx->stats_in = tc_malloc(fsize + 1);
        pd->vid_ctx->stats_in[fsize] = 0;

        if (fread(pd->vid_ctx->stats_in, fsize, 1, pd->stats_file) < 1){
          tc_log_warn(MOD_NAME, "could not read the complete 2pass log file "
                                "\"%s\".", vob->divxlogfile);
          return TC_EXPORT_ERROR;
        }
        break;
      case 3:
        /* fixed qscale :p */
        pd->vid_ctx->flags |= CODEC_FLAG_QSCALE;
        pd->venc_frame->quality = vob->divxbitrate;
        break;
    }
    
    return TC_EXPORT_OK;
}

static int setup_buffers(FFmpegPrivateData *pd, const vob_t *vob)
{
    if (!pd || !vob) {
        tc_log_error(MOD_NAME, "conversion_buffer: bad instance"
                               " data reference");
        return TC_EXPORT_ERROR;
    }
 
    pd->aud_buf_size = (vob->dm_bits/8) * vob->dm_chan * pd->aud_ctx->frame_size;
    pd->aud_buf = tc_malloc(pd->aud_buf_size);
    if (!pd->conv_buf) {
        tc_log_error(MOD_NAME, "audio_buffer: can't allocate data");
        return TC_EXPORT_ERROR;
    }
    
    if ((pd->pix_fmt == CODEC_YUV422 || pd->pix_fmt == CODEC_RGB)
      || IS_MJPEG(pd)) {
        pd->conv_buf_size = avpicture_get_size(PIX_FMT_RGB24,
                                               vob->ex_v_width,
                                               vob->ex_v_height);
        pd->conv_buf = tc_malloc(pd->conv_buf_size);
        if (!pd->conv_buf) {
            tc_log_error(MOD_NAME, "conversion_buffer: can't allocate data");
            return TC_EXPORT_ERROR;
        }
    } else {
        pd->conv_buf = NULL;
        pd->conv_buf_size = 0;
    }
    return TC_EXPORT_OK;
}

static void setup_rc_override(FFmpegPrivateData *pd, FFmpegConfig *cfg)
{
    int i = 0;
    int error = 0;
    const char *p = NULL;
    AVCodecContext *ff_ctx = NULL; /* shortcut */
    
    if ((pd != NULL && pd->vid_ctx != NULL)
      && cfg != NULL) {
        ff_ctx = pd->vid_ctx;
        p = cfg->rc_override_string;
    
        for (i = 0; p != NULL; i++) {
            int start, end, q;
            int e = sscanf(p, "%d,%d,%d", &start, &end, &q);

            if (e != 3) {
                tc_log_warn(MOD_NAME, "error parsing rc_override string");
                error = 1;
                break;
            }
            ff_ctx->rc_override =
                realloc(ff_ctx->rc_override, sizeof(RcOverride) * (i + 1));
            if (!pd->vid_ctx->rc_override) {
                tc_log_warn(MOD_NAME, "can't reallocate "
                                      "rc_override structure");
                error = 1;
                break;
            }
        
            ff_ctx->rc_override[i].start_frame = start;
            ff_ctx->rc_override[i].end_frame   = end;
            if (q > 0) {
                ff_ctx->rc_override[i].qscale = q;
                ff_ctx->rc_override[i].quality_factor = 1.0;
            } else {
                ff_ctx->rc_override[i].qscale = 0;
                ff_ctx->rc_override[i].quality_factor = -q / 100.0;
            }
            p = strchr(p, '/');
            if (p != NULL) { 
                p++;
            }
        }
    } else {
        tc_log_warn(MOD_NAME, "rc_override: bad references for parsing");
        error = 1;
    }
    
    if (error) {
        if (ff_ctx->rc_override != NULL) {
            tc_free(ff_ctx->rc_override);
            ff_ctx->rc_override = NULL;
        }
        ff_ctx->rc_override_count = 0;
    } else {
        ff_ctx->rc_override_count = i;
    }
}

static void reset_module(FFmpegPrivateData *pd)
{
    if (!pd) {
        tc_log_error(MOD_NAME, "reset_module: bad instance data reference");
        return;
    }
   
    pd->levels_handle = -1;
    pd->threads = 1;
    pd->multipass = 0;
    pd->conv_buf = NULL;
    pd->conv_buf_size = 0;
    pd->vid_codec = NULL;
    pd->aud_codec = NULL;
    pd->venc_frame = NULL;
    pd->vid_ctx = NULL;
    pd->aud_ctx = NULL;
    pd->pix_fmt = CODEC_YUV;
    pd->stats_file = NULL;
    pd->size = 0;
    pd->encoded_frames = 0;
    pd->frames = 0;
    pd->interlacing_active = 0;
    pd->interlacing_top_first = 0;
    pd->do_psnr = 0;
    pd->codecs_desc = NULL;
    pd->conf_str[0] = '\0';
    
    strlcpy(pd->vcodec_name, VCODEC_DEFAULT, CODEC_NAME_LEN);
    strlcpy(pd->acodec_name, ACODEC_DEFAULT, CODEC_NAME_LEN);
}

static void set_conf_defaults(FFmpegConfig *cfg)
{
    if (!cfg) {
        tc_log_error(MOD_NAME, "config_defaults: bad instance "
                               "data reference");
        return;
    }
    memset(cfg, 0, sizeof(FFmpegConfig));

    cfg->vrate_tolerance = 1000*8;
    cfg->lmin = 2;
    cfg->lmax = 31;
    
    cfg->inter_matrix_file = NULL;
    cfg->intra_matrix_file = NULL;
    cfg->rc_override_string = NULL;
}

static void set_lavc_defaults(FFmpegPrivateData *pd)
{
    AVCodecContext *ff_ctx = NULL; /* shortcut */
    if (!pd || !pd->vid_ctx) {
        /* can't happen */
        tc_log_error(MOD_NAME, "libavcodec_defaults: bad instance "
                               "data reference");
        return;
    }
    ff_ctx = pd->vid_ctx;

    ff_ctx->flags = 0;
    ff_ctx->mpeg_quant = 0;
    ff_ctx->bit_rate_tolerance = 1000*8;
    ff_ctx->mb_decision = 0;
    ff_ctx->me_method = 4;
    ff_ctx->mb_qmin = 2;
    ff_ctx->mb_qmax = 31;
    ff_ctx->lmin = 2;
    ff_ctx->lmax = 31;
    ff_ctx->max_qdiff = 3;
    ff_ctx->qcompress = 0.5;
    ff_ctx->qblur = 0.5;
    ff_ctx->max_b_frames = 0;
    ff_ctx->b_quant_factor = 1.25;
    ff_ctx->b_frame_strategy = 0;
    ff_ctx->b_quant_offset = 1.25;
    ff_ctx->rc_strategy = 2;
    ff_ctx->luma_elim_threshold = 0;
    ff_ctx->chroma_elim_threshold = 0;
    ff_ctx->rtp_payload_size = 0;
    ff_ctx->strict_std_compliance = 0;
    ff_ctx->i_quant_factor = 0.8;
    ff_ctx->i_quant_offset = 0.0;
    ff_ctx->rc_qsquish = 1.0;
    ff_ctx->rc_qmod_amp = 0.0;
    ff_ctx->rc_qmod_freq = 0;
    ff_ctx->rc_eq = "tex^qComp";
    ff_ctx->rc_buffer_size = 0;
    ff_ctx->rc_buffer_aggressivity = 1.0;
    ff_ctx->rc_max_rate = 0;
    ff_ctx->rc_min_rate = 0;
    ff_ctx->rc_initial_cplx = 0.0;
    ff_ctx->mpeg_quant = 0;
    ff_ctx->dct_algo = 0;
    ff_ctx->idct_algo = 0;
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
    ff_ctx->intra_quant_bias = FF_DEFAULT_QUANT_BIAS;
    ff_ctx->inter_quant_bias = FF_DEFAULT_QUANT_BIAS;
    ff_ctx->coder_type = 0;
    ff_ctx->context_model = 0;
    ff_ctx->intra_matrix = NULL;
    ff_ctx->inter_matrix = NULL;
    ff_ctx->noise_reduction = 0;
    ff_ctx->inter_threshold = 0;
    ff_ctx->scenechange_threshold= 0;
    ff_ctx->thread_count = 1;
    ff_ctx->intra_dc_precision = 0;
}

static int read_config_file(FFmpegPrivateData *pd)
{
    AVCodecContext *ff_ctx = pd->vid_ctx; /* shortcut */
    FFmpegConfig aux_cfg;
   
    struct config ffmpeg_config[] = {
        {"v4mv", &(aux_cfg.v4mv_flag), CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_4MV, 0},
        {"vdpart", &(aux_cfg.vdpart_flag), CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PART, NULL},
        {"gray", &(aux_cfg.gray_flag), CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PART, NULL},
        {"naq", &(aux_cfg.norm_aqp_flag), CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"qpel", &(aux_cfg.qpel_flag), CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_QPEL, NULL},
        {"trell", &(aux_cfg.trell_flag), CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_TRELLIS_QUANT, NULL},
        {"aic", &(aux_cfg.aic_flag), CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_H263P_AIC, NULL},
        {"umv", &(aux_cfg.umv_flag), CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_H263P_UMV, NULL},
        {"cbp", &(aux_cfg.cbp_flag), CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_CBP_RD, NULL},
        {"mv0", &(aux_cfg.mv0_flag), CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_MV0, NULL},
        {"qprd", &(aux_cfg.qp_rd_flag), CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_QP_RD, NULL},
        {"gmc", &(aux_cfg.gmc_flag), CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_GMC, NULL},
        {"trunc", &(aux_cfg.trunc_flag), CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_TRUNCATED, NULL},
        {"closedgop", &(aux_cfg.closed_gop_flag), CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_CLOSED_GOP, NULL},
        {"ss", &(aux_cfg.ss_flag), CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_H263P_SLICE_STRUCT, NULL},
        {"alt", &(aux_cfg.alt_flag), CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_ALT_SCAN, NULL},
        {"ilme", &(aux_cfg.ilme_flag), CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_INTERLACED_ME, NULL},
        {"svcd_sof", &(aux_cfg.soff_flag), CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_SVCD_SCAN_OFFSET, 0},
        {"psnr", &(aux_cfg.psnr_flag), CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PSNR, NULL}, // XXX
        
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
        {"vi_qfactor", &ff_ctx->i_quant_factor, CONF_TYPE_FLOAT, CONF_RANGE, -31.0, 31.0, NULL},
        {"vi_qoffset", &ff_ctx->i_quant_offset, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 31.0, NULL},
        {"vqsquish", &ff_ctx->rc_qsquish, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 99.0, NULL},
        {"vqmod_amp", &ff_ctx->rc_qmod_amp, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 99.0, NULL},
        {"vqmod_freq", &ff_ctx->rc_qmod_freq, CONF_TYPE_INT, 0, 0, 0, NULL},
        {"vrc_eq", &ff_ctx->rc_eq, CONF_TYPE_STRING, 0, 0, 0, NULL},
        {"vrc_override", &aux_cfg.rc_override_string, CONF_TYPE_STRING, 0, 0, 0, NULL},
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
        {"intra_matrix_file", &aux_cfg.intra_matrix_file, CONF_TYPE_STRING, 0, 0, 0, NULL},
        {"inter_matrix_file", &aux_cfg.inter_matrix_file, CONF_TYPE_STRING, 0, 0, 0, NULL},
        {"nr", &ff_ctx->noise_reduction, CONF_TYPE_INT, CONF_RANGE, 0, 1000000, NULL},
        {"sc_threshold", &ff_ctx->scenechange_threshold, CONF_TYPE_INT, CONF_RANGE, -1000000, 1000000, NULL},
        {"inter_threshold", &ff_ctx->inter_threshold, CONF_TYPE_INT, CONF_RANGE, -1000000, 1000000, NULL},
        {"intra_dc_precision", &ff_ctx->intra_dc_precision, CONF_TYPE_INT, CONF_RANGE, 0, 16, NULL},
        
       {"threads", &pd->threads, CONF_TYPE_INT, CONF_RANGE, 1, 7, NULL},
        {NULL, NULL, 0, 0, 0, 0, NULL},
    };        

    set_conf_defaults(&aux_cfg);
    
    module_read_config((char*)pd->vcodec_name, MOD_NAME, "ffmpeg", ffmpeg_config, tc_config_dir);
    if (verbose & TC_DEBUG) {
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
 
    ff_ctx->rtp_payload_size = aux_cfg.packet_size;
    if (aux_cfg.packet_size > 0) {
        ff_ctx->rtp_mode = 1;
    }
    pd->do_psnr = aux_cfg.psnr_flag;
    
    ff_ctx->intra_matrix = load_matrix(aux_cfg.intra_matrix_file,
                                       INTRA_MATRIX);
    ff_ctx->inter_matrix = load_matrix(aux_cfg.inter_matrix_file,
                                       INTER_MATRIX);
    
    setup_rc_override(pd, &aux_cfg);
    
    return TC_EXPORT_OK;
}

