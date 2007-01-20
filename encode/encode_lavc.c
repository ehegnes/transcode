/*
 *  encode_tc_lavc.c - produce empty )as in zero-sized) A/V frames.
 *  (C) 2005/2006 Francesco Romani <fromani at gmail dot com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "transcode.h"
#include "framebuffer.h"
#include "filter.h"

#include "libtc/optstr.h"
#include "libtc/cfgfile.h"
#include "libtc/ratiocodes.h"
#include "libtc/tcframes.h"

#include "libtc/tcmodule-plugin.h"

#include <math.h>
#include <ffmpeg/avcodec.h>

#define MOD_NAME    "encode_lavc.so"
#define MOD_VERSION "v0.0.1 (2007-01-11)"
#define MOD_CAP     "libavcodec based encoder (" LIBAVCODEC_IDENT ")"

#define LAVC_CONFIG_FILE "lavc.cfg"
#define PSNR_LOG_FILE      "psnr.log"

static const char *tc_lavc_help = ""
    "Overview:\n"
    "    this module uses libavcodec to encode given raw frames in\n"
    "    an huge variety of compressed formats, both audio and video.\n"
    "Options:\n"
    "    help     produce module overview and options explanations\n"
    "    list     log out a list of supported A/V codecs\n"
    "    vcodec=X use video codec X for encoding [mandatory]\n";



typedef struct tclavcconfigdata_ TCLavcConfigData;
struct tclavcconfigdata_ {
    char vcodec_name[32];
    int thread_count;

    /* 
     * following options can't be sect directly on AVCodeContext,
     * we need some buffering and translation.
     */
    int vrate_tolerance;
    int rc_min_rate;
    int rc_max_rate;
    int rc_buffer_size;
    int lmin;
    int lmax;
    int me_method;

    /* same as above for flags */
    struct {
        uint32_t mv0;
        uint32_t cbp;
        uint32_t qpel;
        uint32_t alt;
        uint32_t vdpart;
        uint32_t naq;
        uint32_t ilme;
        uint32_t ildct;
        uint32_t aic;
        uint32_t aiv;
        uint32_t umv;
        uint32_t psnr;
        uint32_t trell;
        uint32_t gray;
        uint32_t v4mv;
        uint32_t closedgop;
    } flags;

    /* 
     * special flags: flags that triggers more than a setting
     * FIXME: not yet supported
     */
    int turbo_setup;
};

typedef struct tclavcprivatedata_ TCLavcPrivateData;

/* this is to reduce if()s in out encode_video() */
typedef void (*PreEncodeVideoFn)(struct tclavcprivatedata_ *pd,
                                 vframe_list_t *vframe);

struct tclavcprivatedata_ {
    TCCodecID tc_vcodec;
    TCCodecID tc_pix_fmt;

    AVFrame ff_venc_frame;
    AVCodecContext ff_vcontext;

    AVCodec *ff_vcodec;

    TCLavcConfigData confdata;

    struct {
        int active;
        int top_first;
    } interlacing;

    uint16_t inter_matrix[TC_MATRIX_SIZE];
    uint16_t intra_matrix[TC_MATRIX_SIZE];

    FILE *stats_file;
    FILE *psnr_file;

    vframe_list_t *vframe_buf;
    PreEncodeVideoFn pre_encode_video;
};

/*************************************************************************/

static const TCCodecID tc_lavc_codecs_in[] = {
    TC_CODEC_YUV420P, TC_CODEC_YUV422P, TC_CODEC_RGB,
    TC_CODEC_ERROR
};
static const TCCodecID tc_lavc_codecs_out[] = { 
    TC_CODEC_MPEG1VIDEO, TC_CODEC_MPEG2VIDEO, TC_CODEC_MPEG4VIDEO,
    TC_CODEC_H263I, TC_CODEC_H263P,
    TC_CODEC_H264,
    TC_CODEC_WMV1, TC_CODEC_WMV2,
    TC_CODEC_RV10,
    TC_CODEC_HUFFYUV, TC_CODEC_FFV1,
    TC_CODEC_DV,
    TC_CODEC_MJPEG, TC_CODEC_LJPEG,
    TC_CODEC_MP42, TC_CODEC_MP43,
    TC_CODEC_ERROR
};
static const TCFormatID tc_lavc_formats[] = { TC_FORMAT_ERROR };


/*************************************************************************/

/* 
 * following helper private functions adapt stuff and do proper
 * colorspace conversion, if needed, preparing data for
 * later real encoding
 */

static void pre_encode_video_yuv420p(TCLavcPrivateData *pd,
                                     vframe_list_t *vframe)
{
    avpicture_fill((AVPicture *)&pd->ff_venc_frame, vframe->video_buf,
                    PIX_FMT_YUV420P,
                    pd->ff_vcontext.width, pd->ff_vcontext.height);
}


static void pre_encode_video_yuv420p_huffyuv(TCLavcPrivateData *pd,
                                             vframe_list_t *vframe)
{
    uint8_t *src[3] = { NULL, NULL, NULL };

    YUV_INIT_PLANES(src, vframe->video_buf,
                    IMG_YUV_DEFAULT,
                    pd->ff_vcontext.width, pd->ff_vcontext.height);
    avpicture_fill((AVPicture *)&pd->ff_venc_frame, pd->vframe_buf->video_buf,
                   PIX_FMT_YUV422P,
                   pd->ff_vcontext.width, pd->ff_vcontext.height);
    /* FIXME: can't use tcv_convert (see decode_lavc.c) */
    ac_imgconvert(src, IMG_YUV_DEFAULT,
                  pd->ff_venc_frame.data, IMG_YUV422P,
                  pd->ff_vcontext.width, pd->ff_vcontext.height);
}

static void pre_encode_video_yuv422p(TCLavcPrivateData *pd,
                                     vframe_list_t *vframe)
{
    uint8_t *src[3] = { NULL, NULL, NULL };

    YUV_INIT_PLANES(src, vframe->video_buf,
                    IMG_YUV422P,
                    pd->ff_vcontext.width, pd->ff_vcontext.height);
    avpicture_fill((AVPicture *)&pd->ff_venc_frame, pd->vframe_buf->video_buf,
                   PIX_FMT_YUV420P,
                   pd->ff_vcontext.width, pd->ff_vcontext.height);
    ac_imgconvert(src, IMG_YUV422P,
                  pd->ff_venc_frame.data, IMG_YUV420P,
                  pd->ff_vcontext.width, pd->ff_vcontext.height);
}


static void pre_encode_video_yuv422p_huffyuv(TCLavcPrivateData *pd,
                                             vframe_list_t *vframe)
{
    avpicture_fill((AVPicture *)&pd->ff_venc_frame, vframe->video_buf,
                   PIX_FMT_YUV422P,
                   pd->ff_vcontext.width, pd->ff_vcontext.height);

}


static void pre_encode_video_rgb24(TCLavcPrivateData *pd,
                                   vframe_list_t *vframe)
{
    avpicture_fill((AVPicture *)&pd->ff_venc_frame, pd->vframe_buf->video_buf,
                   PIX_FMT_YUV420P,
                   pd->ff_vcontext.width, pd->ff_vcontext.height);
    ac_imgconvert(&vframe->video_buf, IMG_RGB_DEFAULT,
                  pd->ff_venc_frame.data, IMG_YUV420P,
                  pd->ff_vcontext.width, pd->ff_vcontext.height);
}



/*************************************************************************/

/* more helpers */

#if !defined(INFINITY) && defined(HUGE_VAL)
#define INFINITY HUGE_VAL
#endif

static double psnr(double d) {
    if (d == 0) {
        return INFINITY;
    }
    return -10.0 * log(d) / log(10);
}

static const char* tc_lavc_list_codecs(void)
{
    /* XXX: I feel a bad taste */
    static char buf[TC_BUF_MAX];
    static int ready = TC_FALSE;

    if (!ready) {
        size_t used = 0;
        int i = 0;

        for (i = 0; tc_lavc_codecs_out[i] != TC_CODEC_ERROR; i++) {
            char sbuf[TC_BUF_MIN];
            size_t slen = 0;

            tc_snprintf(sbuf, sizeof(sbuf), "%15s: %s\n",
                        tc_codec_to_string(tc_lavc_codecs_out[i]),
                        tc_codec_to_comment(tc_lavc_codecs_out[i]));
            slen = strlen(sbuf);
            if (used + slen <= sizeof(buf)) {
                strlcpy(buf + used, sbuf, sizeof(buf) - used);
                used += slen;
                /* chomp final '\0' except for first round */
            } else {
                tc_log_error(MOD_NAME, "too much codecs! this should happen. "
                                       "Please file a bug report.");
                strlcpy(buf, "internal error", sizeof(buf));
            }
        }
        ready = TC_TRUE;
    }
    return buf;
}

static void tc_lavc_read_matrices(TCLavcPrivateData *pd,
                                    const char *intra_matrix_file,
                                    const char *inter_matrix_file)
{
    if (intra_matrix_file != NULL && strlen(intra_matrix_file) > 0) {
        /* looks like we've got something... */
        int ret = tc_read_matrix(intra_matrix_file, NULL, pd->inter_matrix);
        if (ret == 0) {
            /* ok, let's give this to lavc */
            pd->ff_vcontext.intra_matrix = pd->inter_matrix;
        } else {
            tc_log_warn(MOD_NAME, "error while reading intra matrix from"
                                  " %s", intra_matrix_file);
            pd->ff_vcontext.intra_matrix = NULL; /* paranoia */
        }
    }

    if (inter_matrix_file != NULL && strlen(inter_matrix_file) > 0) {
        /* looks like we've got something... */
        int ret = tc_read_matrix(inter_matrix_file, NULL, pd->inter_matrix);
        if (ret == 0) {
            /* ok, let's give this to lavc */
            pd->ff_vcontext.inter_matrix = pd->inter_matrix;
        } else {
            tc_log_warn(MOD_NAME, "error while reading inter matrix from"
                                  " %s", inter_matrix_file);
            pd->ff_vcontext.inter_matrix = NULL; /* paranoia */
        }
    }
}

static void tc_lavc_load_filters(TCLavcPrivateData *pd)
{
    if (pd->tc_vcodec == TC_CODEC_MJPEG || pd->tc_vcodec == TC_CODEC_LJPEG) {
        int handle;

        tc_log_info(MOD_NAME, "output is mjpeg or ljpeg, extending range from "
			      "YUV420P to YUVJ420P (full range)");

        handle = tc_filter_add("levels", "input=16-240");
        if (!handle) {
            tc_log_warn(MOD_NAME, "cannot load levels filter");
        }
    }
}

/*************************************************************************/

#define PSNR_REQUESTED(PD) ((PD)->confdata.flags.psnr)

/* PSNR-log stuff */
static int psnr_open(TCLavcPrivateData *pd)
{
    pd->psnr_file = NULL;

    pd->psnr_file = fopen(PSNR_LOG_FILE, "w");
    if (pd->psnr_file == NULL) {
        tc_log_warn(MOD_NAME, "can't open psnr log file '%s'",
                    PSNR_LOG_FILE);
        return TC_ERROR;
    }
    return TC_OK;
}

#define PFRAME(PD) ((PD)->ff_vcontext.coded_frame)

static int psnr_write(TCLavcPrivateData *pd, int size)
{
    if (pd->psnr_file != NULL) {
        const char pict_type[5] = { '?', 'I', 'P', 'B', 'S' };
        double f = pd->ff_vcontext.width * pd->ff_vcontext.height * 255.0 * 255.0;
        double err[3] = {
                PFRAME(pd)->error[0],
                PFRAME(pd)->error[1],
                PFRAME(pd)->error[2]
        };

        fprintf(pd->psnr_file, "%6d, %2d, %6d, %2.2f,"
                               " %2.2f, %2.2f, %2.2f %c\n",
                PFRAME(pd)->coded_picture_number, PFRAME(pd)->quality, size,
                psnr(err[0]     / f),
                psnr(err[1] * 4 / f), /* FIXME */
                psnr(err[2] * 4 / f), /* FIXME */
                psnr((err[0] + err[1] + err[2]) / (f * 1.5)),
                pict_type[PFRAME(pd)->pict_type]);
    }
    return TC_ERROR;
}

#undef PFRAME

static int psnr_close(TCLavcPrivateData *pd)
{
    if (pd->psnr_file != NULL) {
        if (fclose(pd->psnr_file) != 0) {
            return TC_ERROR;
        }
    }
    return TC_OK;
}

static void psnr_print(TCLavcPrivateData *pd)
{
    double f = pd->ff_vcontext.width * pd->ff_vcontext.height * 255.0 * 255.0;

    f *= pd->ff_vcontext.coded_frame->coded_picture_number;

#define ERROR pd->ff_vcontext.error
    tc_log_info(MOD_NAME, "PSNR: Y:%2.2f, Cb:%2.2f, Cr:%2.2f, All:%2.2f",
                psnr(ERROR[0] / f),
                /* FIXME: this is correct if pix_fmt != YUV420P */
                psnr(ERROR[1] * 4 / f), 
                psnr(ERROR[2] * 4 / f), 
                psnr((ERROR[0] + ERROR[1] + ERROR[2]) / (f * 1.5)));
#undef ERROR
}


/*************************************************************************/

/* 
 * configure() helpers, libavcodec allow very detailed
 * configuration step
 */

/* FIXME: move to TC_CODEC_* colorspaces */
static int tc_lavc_set_pix_fmt(TCLavcPrivateData *pd, const vob_t *vob)
{
    switch (vob->im_v_codec) {
      case CODEC_YUV:
        if (pd->tc_vcodec == TC_CODEC_HUFFYUV) {
            pd->tc_pix_fmt = TC_CODEC_YUV422P;
            pd->ff_vcontext.pix_fmt = PIX_FMT_YUV422P;
            pd->pre_encode_video = pre_encode_video_yuv420p_huffyuv;
        } else {
            pd->tc_pix_fmt = TC_CODEC_YUV420P;
            pd->pre_encode_video = pre_encode_video_yuv420p;
        }
        break;
      case CODEC_YUV422:
        pd->tc_pix_fmt = TC_CODEC_YUV422P;
        pd->ff_vcontext.pix_fmt = (pd->tc_vcodec == TC_CODEC_MJPEG) 
                                   ? PIX_FMT_YUVJ422P
                                   : PIX_FMT_YUV422P;
        if (pd->tc_vcodec == TC_CODEC_HUFFYUV) {
            pd->pre_encode_video = pre_encode_video_yuv422p_huffyuv;
        } else {
            pd->pre_encode_video = pre_encode_video_yuv422p;
        }
        break;
      case CODEC_RGB:
        pd->tc_pix_fmt = TC_CODEC_RGB;
        pd->ff_vcontext.pix_fmt = (pd->tc_vcodec == TC_CODEC_HUFFYUV)
                                        ? PIX_FMT_YUV422P
                                        : (pd->tc_vcodec == TC_CODEC_MJPEG) 
                                           ? PIX_FMT_YUVJ420P
                                           : PIX_FMT_YUV420P;
        pd->pre_encode_video = pre_encode_video_rgb24;
        break;
      default:
        tc_log_warn(MOD_NAME, "Unknown pixel format %i", vob->im_v_codec);
        return TC_EXPORT_ERROR;
    }

    tc_log_info(MOD_NAME, "internal pixel format: %s",
                tc_codec_to_string(pd->tc_pix_fmt));
    return TC_OK;
}


#define CAN_DO_MULTIPASS(FLAG) do { \
    if (!(FLAG)) { \
        tc_log_error(MOD_NAME, "This codec does not support multipass " \
                     "encoding."); \
        return TC_ERROR; \
    } \
} while (0)
 

static int tc_lavc_init_multipass(TCLavcPrivateData *pd, const vob_t *vob)
{
    int multipass_flag = tc_codec_is_multipass(pd->tc_vcodec);
    pd->stats_file = NULL;
    size_t fsize = 0;

    switch (vob->divxmultipass) {
      case 1:
        CAN_DO_MULTIPASS(multipass_flag);
        pd->ff_vcontext.flags |= CODEC_FLAG_PASS1;
        pd->stats_file = fopen(vob->divxlogfile, "w");
        if (pd->stats_file == NULL) {
            tc_log_error(MOD_NAME, "could not create 2pass log file"
                         " \"%s\".", vob->divxlogfile);
            return TC_ERROR;
        }
        break;
      case 2:
        CAN_DO_MULTIPASS(multipass_flag);
        pd->ff_vcontext.flags |= CODEC_FLAG_PASS2;
        pd->stats_file = fopen(vob->divxlogfile, "r");
        if (pd->stats_file == NULL){
            tc_log_error(MOD_NAME, "could not open 2pass log file \"%s\""
                         " for reading.", vob->divxlogfile);
            return TC_ERROR;
        }
        /* FIXME: we're optimistic here, don't we? */
        fseek(pd->stats_file, 0, SEEK_END);
        fsize = ftell(pd->stats_file);
        fseek(pd->stats_file, 0, SEEK_SET);

        pd->ff_vcontext.stats_in = tc_malloc(fsize + 1);
        if (pd->ff_vcontext.stats_in == NULL) {
            tc_log_error(MOD_NAME, "can't get memory for multipass log");
            fclose(pd->stats_file);
            return TC_ERROR;
        }

        if (fread(pd->ff_vcontext.stats_in, fsize, 1, pd->stats_file) < 1) {
            tc_log_error(MOD_NAME, "Could not read the complete 2pass log"
                         " file \"%s\".", vob->divxlogfile);
            return TC_ERROR;
        }
        pd->ff_vcontext.stats_in[fsize] = 0; /* paranoia */
        fclose(pd->stats_file);
        break;
      case 3:
        /* fixed qscale :p */
        pd->ff_vcontext.flags |= CODEC_FLAG_QSCALE;
        pd->ff_venc_frame.quality = vob->divxbitrate;
        break;
    }
    return TC_OK;
}

#undef CAN_DO_MULTIPASS

static void tc_lavc_fini_multipass(TCLavcPrivateData *pd)
{
    if (pd->ff_vcontext.stats_in != NULL) {
        tc_free(pd->ff_vcontext.stats_in);
        pd->ff_vcontext.stats_in = NULL;
    }
    if (pd->stats_file != NULL) {
        fclose(pd->stats_file); /* XXX */
        pd->stats_file = NULL;
    }
}

static void tc_lavc_init_rc_override(TCLavcPrivateData *pd, const char *str)
{
    int i = 0;

    if (str != NULL && strlen(str) > 0) {
        const char *p = str;

        for (i = 0; p != NULL; i++) {
            int start, end, q;
            int e = sscanf(p, "%i,%i,%i", &start, &end, &q);

            if (e != 3) {
                tc_log_warn(MOD_NAME, "Error parsing rc_override (ignored)");
                return;
            }
            pd->ff_vcontext.rc_override = 
                tc_realloc(pd->ff_vcontext.rc_override,
                           sizeof(RcOverride) * (i + 1));
            pd->ff_vcontext.rc_override[i].start_frame = start;
            pd->ff_vcontext.rc_override[i].end_frame   = end;
            if (q > 0) {
                pd->ff_vcontext.rc_override[i].qscale         = q;
                pd->ff_vcontext.rc_override[i].quality_factor = 1.0;
            } else {
                pd->ff_vcontext.rc_override[i].qscale         = 0;
                pd->ff_vcontext.rc_override[i].quality_factor = -q / 100.0;
            }
            p = strchr(p, '/');
            if (p != NULL) {
                p++;
            }
        }
    }
    pd->ff_vcontext.rc_override_count = i;
}


static void tc_lavc_fini_rc_override(TCLavcPrivateData *pd)
{
    if (pd->ff_vcontext.rc_override != NULL) {
        tc_free(pd->ff_vcontext.rc_override);
        pd->ff_vcontext.rc_override = NULL;
    }
}

static int tc_lavc_init_buf(TCLavcPrivateData *pd, const vob_t *vob)
{
    if (pd->tc_pix_fmt != TC_CODEC_YUV420P) { /*yuv420p it's out default */
        pd->vframe_buf = tc_new_video_frame(vob->im_v_width, vob->im_v_height,
                                            pd->tc_pix_fmt, TC_TRUE);
        if (pd->vframe_buf == NULL) {
            tc_log_warn(MOD_NAME, "unable to allocate internal vframe buffer");
            return TC_ERROR;
        }
    }
    return TC_OK;
}

#define tc_lavc_fini_buf(PD) do { \
    if ((PD) != NULL && (PD)->vframe_buf != NULL) { \
        tc_del_video_frame((PD)->vframe_buf); \
    } \
} while (0)

static int tc_lavc_settings_from_vob(TCLavcPrivateData *pd, const vob_t *vob)
{
    int ret = 0;

    pd->ff_vcontext.bit_rate = vob->divxbitrate * 1000;
    pd->ff_vcontext.width    = vob->ex_v_width;
    pd->ff_vcontext.height   = vob->ex_v_height;
    pd->ff_vcontext.qmin     = vob->min_quantizer;
    pd->ff_vcontext.qmax     = vob->max_quantizer;

    if (vob->export_attributes & TC_EXPORT_ATTRIBUTE_GOP) {
        pd->ff_vcontext.gop_size = vob->divxkeyframes;
    } else {
        if (pd->tc_vcodec == TC_CODEC_MPEG1VIDEO
         || pd->tc_vcodec == TC_CODEC_MPEG2VIDEO) {
            pd->ff_vcontext.gop_size = 15; /* conservative default for mpeg1/2 svcd/dvd */
        } else {
            pd->ff_vcontext.gop_size = 250; /* reasonable default for mpeg4 (and others) */
        }
    }

    ret = tc_find_best_aspect_ratio(vob,
                                    &pd->ff_vcontext.sample_aspect_ratio.num,
                                    &pd->ff_vcontext.sample_aspect_ratio.den);
    if (ret != TC_OK) {
        tc_log_error(MOD_NAME, "unable to find sane value for SAR");
        return TC_ERROR;
    }
    ret = tc_frc_code_to_ratio(vob->ex_frc,
                               &pd->ff_vcontext.time_base.den,
                               &pd->ff_vcontext.time_base.num);
                               /* watch out here */
    if (ret == TC_NULL_MATCH) {
        /* legacy */
        if ((vob->ex_fps > 29) && (vob->ex_fps < 30)) {
            pd->ff_vcontext.time_base.den = 30000;
            pd->ff_vcontext.time_base.num = 1001;
        } else {
            pd->ff_vcontext.time_base.den = (int)(vob->ex_fps * 1000.0);
            pd->ff_vcontext.time_base.num = 1000;
        }
    }

    switch(vob->encode_fields) {
      case 1:
        pd->interlacing.active    = 1;
        pd->interlacing.top_first = 1;
        break;
      case 2:
        pd->interlacing.active    = 1;
        pd->interlacing.top_first = 0;
        break;
      default: /* progressive / unknown */
        pd->interlacing.active    = 0;
        pd->interlacing.top_first = 0;
        break;
    }

    ret = tc_lavc_set_pix_fmt(pd, vob);
    if (ret != TC_OK) {
        return ret;
    }
    return tc_lavc_init_multipass(pd, vob);
}

#define PCTX(field) &(pd->ff_vcontext.field)
#define PAUX(field) &(pd->confdata.field)

/*
 * setup sane values for auxiliary config, and setup *transcode's*
 * AVCodecContext default settings.
 */
static void tc_lavc_config_defaults(TCLavcPrivateData *pd)
{
    /* first of all reinitialize lavc data */
    avcodec_get_context_defaults(&pd->ff_vcontext);

    pd->confdata.vcodec_name[0] = '\0';
    pd->confdata.thread_count = 1;

    pd->confdata.vrate_tolerance = 8 * 1000;
    pd->confdata.rc_min_rate     = 0;
    pd->confdata.rc_max_rate     = 0;
    pd->confdata.rc_buffer_size  = 0;
    pd->confdata.lmin            = 2;
    pd->confdata.lmax            = 31;
    pd->confdata.me_method       = ME_EPZS;

    memset(&pd->confdata.flags, 0, sizeof(pd->confdata.flags));
    pd->confdata.turbo_setup = 0;

    /* 
     * context *transcode* (not libavcodec) defaults
     */
    pd->ff_vcontext.mb_qmin                 = 2;
    pd->ff_vcontext.mb_qmax                 = 31;
    pd->ff_vcontext.max_qdiff               = 3;
    pd->ff_vcontext.max_b_frames            = 0;
    pd->ff_vcontext.me_range                = 0;
    pd->ff_vcontext.mb_decision             = 0;
    pd->ff_vcontext.scenechange_threshold   = 0;
    pd->ff_vcontext.scenechange_factor      = 1;
    pd->ff_vcontext.b_frame_strategy        = 0;
    pd->ff_vcontext.b_sensitivity           = 40;
    pd->ff_vcontext.brd_scale               = 0;
    pd->ff_vcontext.bidir_refine            = 0;
    pd->ff_vcontext.rc_strategy             = 2;
    pd->ff_vcontext.b_quant_factor          = 1.25;
    pd->ff_vcontext.i_quant_factor          = 0.8;
    pd->ff_vcontext.b_quant_offset          = 1.25;
    pd->ff_vcontext.i_quant_offset          = 0.0;
    pd->ff_vcontext.qblur                   = 0.5;
    pd->ff_vcontext.qcompress               = 0.5;
    pd->ff_vcontext.mpeg_quant              = 0;
    pd->ff_vcontext.rc_initial_cplx         = 0.0;
    pd->ff_vcontext.rc_qsquish              = 1.0;
    pd->ff_vcontext.luma_elim_threshold     = 0;
    pd->ff_vcontext.chroma_elim_threshold   = 0;
    pd->ff_vcontext.strict_std_compliance   = 0;
    pd->ff_vcontext.dct_algo                = FF_DCT_AUTO;
    pd->ff_vcontext.idct_algo               = FF_IDCT_AUTO;
    pd->ff_vcontext.lumi_masking            = 0.0;
    pd->ff_vcontext.dark_masking            = 0.0;
    pd->ff_vcontext.temporal_cplx_masking   = 0.0;
    pd->ff_vcontext.spatial_cplx_masking    = 0.0;
    pd->ff_vcontext.p_masking               = 0.0;
    pd->ff_vcontext.border_masking          = 0.0;
    pd->ff_vcontext.me_pre_cmp              = 0;
    pd->ff_vcontext.me_cmp                  = 0;
    pd->ff_vcontext.me_sub_cmp              = 0;
    pd->ff_vcontext.ildct_cmp               = FF_CMP_SAD;
    pd->ff_vcontext.pre_dia_size            = 0;
    pd->ff_vcontext.dia_size                = 0;
    pd->ff_vcontext.mv0_threshold           = 256;
    pd->ff_vcontext.last_predictor_count    = 0;
    pd->ff_vcontext.pre_me                  = 1;
    pd->ff_vcontext.me_subpel_quality       = 8;
    pd->ff_vcontext.refs                    = 1;
    pd->ff_vcontext.intra_quant_bias        = FF_DEFAULT_QUANT_BIAS;
    pd->ff_vcontext.inter_quant_bias        = FF_DEFAULT_QUANT_BIAS;
    pd->ff_vcontext.noise_reduction         = 0;
    pd->ff_vcontext.quantizer_noise_shaping = 0;
    pd->ff_vcontext.flags                   = 0;
}


/* FIXME: it is too nasty? */
#define SET_FLAG(pd, field) (pd)->ff_vcontext.flags |= (pd)->confdata.flags.field

/* 
 * translate auxiliary configuration into context values;
 * also does some consistency verifications
 */
static void tc_lavc_dispatch_settings(TCLavcPrivateData *pd)
{
    /* some translation... */
    pd->ff_vcontext.bit_rate_tolerance = pd->confdata.vrate_tolerance * 1000;
    pd->ff_vcontext.rc_min_rate = pd->confdata.rc_min_rate * 1000;
    pd->ff_vcontext.rc_max_rate = pd->confdata.rc_max_rate * 1000;
    pd->ff_vcontext.rc_buffer_size = pd->confdata.rc_buffer_size * 1024;
    pd->ff_vcontext.lmin = (int)(FF_QP2LAMBDA * pd->confdata.lmin + 0.5);
    pd->ff_vcontext.lmax = (int)(FF_QP2LAMBDA * pd->confdata.lmax + 0.5);
    pd->ff_vcontext.me_method = ME_ZERO + pd->confdata.me_method;

    pd->ff_vcontext.flags = 0;
    SET_FLAG(pd, mv0);
    SET_FLAG(pd, cbp);
    SET_FLAG(pd, qpel);
    SET_FLAG(pd, alt);
    SET_FLAG(pd, vdpart);
    SET_FLAG(pd, naq);
    SET_FLAG(pd, ilme);
    SET_FLAG(pd, ildct);
    SET_FLAG(pd, aic);
    SET_FLAG(pd, aiv);
    SET_FLAG(pd, umv);
    SET_FLAG(pd, psnr);
    SET_FLAG(pd, trell);
    SET_FLAG(pd, gray);
    SET_FLAG(pd, v4mv);
    SET_FLAG(pd, closedgop);

    /* FIXME: coherency check */
    if (pd->ff_vcontext.rtp_payload_size > 0) {
        pd->ff_vcontext.rtp_mode = 1;
    }
    if (pd->confdata.flags.closedgop) {
        pd->ff_vcontext.scenechange_threshold = 1000000;
    }
    if (pd->interlacing.active) {
        /* enforce interlacing */
        pd->ff_vcontext.flags |= CODEC_FLAG_INTERLACED_DCT;
        pd->ff_vcontext.flags |= CODEC_FLAG_INTERLACED_ME;
    }
}

#undef SET_FLAG



/* FIXME: I'm a bit worried about heavy stack usage of this function... */
static int tc_lavc_read_config(TCLavcPrivateData *pd,
                                  const char *options)
{
    char intra_matrix_file[PATH_MAX] = { '\0' };
    char inter_matrix_file[PATH_MAX] = { '\0' };
    char rc_override_buf[TC_BUF_MIN] = { '\0' }; /* XXX */
    /* 
     * Please note that option names are INTENTIONALLY identical/similar
     * to mplayer/mencoder ones
     */
    TCConfigEntry lavc_conf[] = {
        { "threads", PAUX(thread_count), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 1, 7 },
        //  { "vcodec", PAUX(vcodec_name), TCCONF_TYPE_STRING, 0, 0, 0 },
        //  need special handling
        //  { "keyint", PCTX(gop_size), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 1, 1000 },
        //  handled by transcode core
        //  { "vbitrate", PCTX(bit_rate), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, INT_MAX },
        //  handled by transcode core
        //  { "vqmin", PCTX(qmin), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 1, 60 },
        //  handled by transcode core
        //  { "vqmax", PCTX(qmax), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 1, 60 },
        //  handled by transcode core
        { "mbqmin", PCTX(mb_qmin), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 1, 60 },
        { "mbqmax", PCTX(mb_qmax), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 1, 60 },
        { "lmin", PAUX(lmin), TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, 0.01, 255.0 },
        { "lmax", PAUX(lmax), TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, 0.01, 255.0 },
        { "vqdiff", PCTX(max_qdiff), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 1, 31 },
        { "vmax_b_frames", PCTX(max_b_frames), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, FF_MAX_B_FRAMES },
        { "vme", PAUX(me_method), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 16, },
        { "me_range", PCTX(me_range), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 16000 },
        { "mbd", PCTX(mb_decision), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 3 },
        { "sc_threshold", PCTX(scenechange_threshold), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, -1000000, 1000000 },
        { "sc_factor", PCTX(scenechange_factor), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 1, 16 },
        { "vb_strategy", PCTX(b_frame_strategy), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 10 },
        { "b_sensitivity", PCTX(b_sensitivity), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 1, 100 },
        { "brd_scale", PCTX(brd_scale), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 10 },
        { "bidir_refine", PCTX(bidir_refine), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 4 },
        //  { "aspect",     },
        //  handled by transcode core
        { "vratetol", PAUX(vrate_tolerance), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 4, 24000000 },
        { "vrc_maxrate", PAUX(rc_max_rate), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 24000000 },
        { "vrc_minrate", PAUX(rc_min_rate), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 24000000 },
        { "vrc_buf_size", PAUX(rc_buffer_size), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 4, 24000000 },
        { "vrc_strategy", PCTX(rc_strategy), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 2 },
        { "vb_qfactor", PCTX(b_quant_factor), TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, -31.0, 31.0 },
        { "vi_qfactor", PCTX(i_quant_factor), TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, -31.0, 31.0 },
        { "vb_qoffset", PCTX(b_quant_offset), TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, 0.0, 31.0 },
        { "vi_qoffset", PCTX(i_quant_offset), TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, 0.0, 31.0 },
        { "vqblur", PCTX(qblur), TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, 0.0, 1.0 },
        { "vqcomp", PCTX(qcompress), TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, 0.0, 1.0 },
        { "mpeg_quant", PCTX(mpeg_quant), TCCONF_TYPE_FLAG, 0, 0, 1 },
        //  { "vrc_eq",     }, // not yet supported
        { "vrc_override", rc_override_buf, TCCONF_TYPE_STRING, 0, 0, 0 },
        { "vrc_init_cplx", PCTX(rc_initial_cplx), TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, 0.0, 9999999.0 },
        //  { "vrc_init_occupancy",   }, // not yet supported
        { "vqsquish", PCTX(rc_qsquish), TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, 0.0, 99.0 },
        { "vlelim", PCTX(luma_elim_threshold), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, -99, 99 },
        { "vcelim", PCTX(chroma_elim_threshold), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, -99, 99 },
        { "vstrict", PCTX(strict_std_compliance), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, -99, 99 },
        { "vpsize", PCTX(rtp_payload_size), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 100000000 },
        { "dct", PCTX(dct_algo), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 10 },
        { "idct", PCTX(idct_algo), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 20 },
        { "lumi_mask", PCTX(lumi_masking), TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, 0.0, 1.0 },
        { "dark_mask", PCTX(dark_masking), TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, 0.0, 1.0 },
        { "tcplx_mask", PCTX(temporal_cplx_masking), TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, 0.0, 1.0 },
        { "scplx_mask", PCTX(spatial_cplx_masking), TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, 0.0, 1.0 },
        { "p_mask", PCTX(p_masking), TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, 0.0, 1.0 },
        { "border_mask", PCTX(border_masking), TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, 0.0, 1.0 },
        { "pred", PCTX(prediction_method), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 4 },
        { "precmp", PCTX(me_pre_cmp), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 2000 },
        { "cmp", PCTX(me_cmp), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 2000 },
        { "subcmp", PCTX(me_sub_cmp), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 2000 },
        { "ildctcmp", PCTX(ildct_cmp), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 2000 },
        { "predia", PCTX(pre_dia_size), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, -2000, 2000 },
        { "dia", PCTX(dia_size), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, -2000, 2000 },
        { "mv0_threshold", PCTX(mv0_threshold), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 1000 },
        { "last_pred", PCTX(last_predictor_count), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 2000 },
        { "pre_me", PCTX(pre_me), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 2000},
        { "subq", PCTX(me_subpel_quality), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 8 },
        { "refs", PCTX(refs), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 1, 8 },
        { "ibias", PCTX(intra_quant_bias), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, -512, 512 },
        { "pbias", PCTX(inter_quant_bias), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, -512, 512 },
        { "nr", PCTX(noise_reduction), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 1000000},
        { "qns", PCTX(quantizer_noise_shaping), TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 3 },
        { "inter_matrix_file", inter_matrix_file, TCCONF_TYPE_STRING, 0, 0, 0 },
        { "intra_matrix_file", intra_matrix_file, TCCONF_TYPE_STRING, 0, 0, 0 },
    
        { "mv0", PAUX(flags.mv0), TCCONF_TYPE_FLAG, 0, 0, CODEC_FLAG_MV0 },
        { "cbp", PAUX(flags.cbp), TCCONF_TYPE_FLAG, 0, 0, CODEC_FLAG_CBP_RD },
        { "qpel", PAUX(flags.qpel), TCCONF_TYPE_FLAG, 0, 0, CODEC_FLAG_QPEL },
        { "alt", PAUX(flags.alt), TCCONF_TYPE_FLAG, 0, 0, CODEC_FLAG_ALT_SCAN },
        { "ilme", PAUX(flags.ilme), TCCONF_TYPE_FLAG, 0, 0, CODEC_FLAG_INTERLACED_ME },
        { "ildct", PAUX(flags.ildct), TCCONF_TYPE_FLAG, 0, 0, CODEC_FLAG_INTERLACED_DCT },
        { "naq", PAUX(flags.naq), TCCONF_TYPE_FLAG, 0, 0, CODEC_FLAG_NORMALIZE_AQP },
        { "vdpart", PAUX(flags.vdpart), TCCONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PART },
        { "aic", PAUX(flags.aic), TCCONF_TYPE_FLAG, 0, 0, CODEC_FLAG_H263P_AIC },
        { "aiv", PAUX(flags.aiv), TCCONF_TYPE_FLAG, 0, 0, CODEC_FLAG_H263P_AIV },
        { "umv", PAUX(flags.umv), TCCONF_TYPE_FLAG, 0, 0, CODEC_FLAG_H263P_UMV },
        { "psnr", PAUX(flags.psnr), TCCONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PSNR },
        { "trell", PAUX(flags.trell), TCCONF_TYPE_FLAG, 0, 0, CODEC_FLAG_TRELLIS_QUANT },
        { "gray", PAUX(flags.gray), TCCONF_TYPE_FLAG, 0, 0, CODEC_FLAG_GRAY },
        { "v4mv", PAUX(flags.v4mv), TCCONF_TYPE_FLAG, 0, 0, CODEC_FLAG_4MV },
        { "closedgop", PAUX(flags.closedgop), TCCONF_TYPE_FLAG, 0, 0, CODEC_FLAG_CLOSED_GOP },
    
        //  { "turbo", PAUX(turbo_setup), TCCONF_TYPE_FLAG, 0, 0, 1 }, // not yet  supported
        /* End of the config file */
        { NULL, 0, 0, 0, 0, 0 }
    };
    /* first of all, get mandatory codec name and bail out if not found. */
    int ret = optstr_get(options, "vcodec", "%15s", pd->confdata.vcodec_name);
    pd->confdata.vcodec_name[15] = '\0'; /* paranoia */
    if (ret != 1) {
        tc_log_error(MOD_NAME, "missing mandatory vcodec option");
        return TC_ERROR;
    }

    module_read_config(LAVC_CONFIG_FILE, pd->confdata.vcodec_name,
                       lavc_conf, MOD_NAME);

    if (options && strlen(options) > 0) {
        size_t i = 0, n = 0;
        char **opts = tc_strsplit(options, ':', &n);

        if (opts == NULL) {
            tc_log_error(MOD_NAME, "can't split option string");
            return TC_ERROR;
        }
        for (i = 0; i < n; i++) {
            if (!module_read_config_line(opts[i], lavc_conf, MOD_NAME)) {
                tc_log_error(MOD_NAME, "error parsing module options (%s)",
                             opts[i]);
                tc_strfreev(opts);
                return TC_ERROR;
            }
        }
        tc_strfreev(opts);
    }

    /* gracefully go ahead if no matrices are given */
    tc_lavc_read_matrices(pd, intra_matrix_file, inter_matrix_file);
    /* gracefully go ahead if no matrices are given */
    tc_lavc_init_rc_override(pd, rc_override_buf);

    if (verbose >= TC_DEBUG) {
        module_print_config(lavc_conf, MOD_NAME);
    }
    /* only now we can do this safely */
    tc_lavc_dispatch_settings(pd);

    return TC_OK;
}

#undef PCTX
#undef PAUX

static int tc_lavc_write_logs(TCLavcPrivateData *pd, int size)
{
    /* store stats if there are any */
    if (pd->ff_vcontext.stats_out != NULL && pd->stats_file != NULL) {
        int ret = fprintf(pd->stats_file, "%s",
                          pd->ff_vcontext.stats_out);
        if (ret < 0) {
            tc_log_warn(MOD_NAME, "error while writing multipass log file");
            return TC_ERROR;
        }
    }

    if (PSNR_REQUESTED(pd)) {
        /* errors not fatal, they can be ignored */
        psnr_write(pd, size);
    }
        
    return TC_OK;
}

/*************************************************************************/

static int tc_lavc_init(TCModuleInstance *self)
{
    TCLavcPrivateData *pd = NULL;

    TC_MODULE_SELF_CHECK(self, "init");

    pd = tc_malloc(sizeof(TCLavcPrivateData));
    if (pd == NULL) {
        tc_log_error(MOD_NAME, "unable to allocate private data");
        return TC_ERROR;
    }

    pd->psnr_file = NULL;
    pd->stats_file = NULL;

    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }
    self->userdata = pd;

    return TC_OK;
}

static int tc_lavc_fini(TCModuleInstance *self)
{
    TC_MODULE_SELF_CHECK(self, "fini");

    tc_free(self->userdata);
    self->userdata = NULL;

    return TC_OK;
}

#define ABORT_IF_NOT_OK(RET) do { \
    if ((RET) != TC_OK) { \
        goto failed; \
    } \
} while (0)


static int tc_lavc_configure(TCModuleInstance *self,
                               const char *options, vob_t *vob)
{
    TCLavcPrivateData *pd = NULL;
    int ret = TC_OK;

    TC_MODULE_SELF_CHECK(self, "configure");
    TC_MODULE_SELF_CHECK(options, "configure"); /* paranoia */

    pd = self->userdata;

    TC_LOCK_LIBAVCODEC;
    avcodec_init();
    avcodec_register_all();
    TC_UNLOCK_LIBAVCODEC;

    avcodec_get_frame_defaults(&pd->ff_venc_frame);
    /*
     * auxiliary config data needs to be blanked too
     * before any other operation
     */
    tc_lavc_config_defaults(pd);

    ret = tc_lavc_settings_from_vob(pd, vob);
    ABORT_IF_NOT_OK(ret);

    /* calling WARNING: order matters here */
    ret = tc_lavc_init_buf(pd, vob);
    ABORT_IF_NOT_OK(ret);

    ret = tc_lavc_read_config(pd, options);
    ABORT_IF_NOT_OK(ret);

    tc_lavc_load_filters(pd);

    if (verbose) {
        tc_log_info(MOD_NAME, "using %i thread%s",
                    pd->confdata.thread_count,
                    (pd->confdata.thread_count > 1) ?"s" :"");
    }
    avcodec_thread_init(&pd->ff_vcontext, pd->confdata.thread_count);

    pd->ff_vcodec = avcodec_find_encoder_by_name(pd->confdata.vcodec_name);
    if (pd->ff_vcodec == NULL) {
        tc_log_error(MOD_NAME, "unable to find a codec for `%s'",
                     pd->confdata.vcodec_name);
        goto failed;
    }

    TC_LOCK_LIBAVCODEC;
    ret = avcodec_open(&pd->ff_vcontext, pd->ff_vcodec);
    TC_UNLOCK_LIBAVCODEC;

    if (ret < 0) {
        tc_log_error(MOD_NAME, "avcodec_open() failed");
        goto failed;
    }
    /* finally, pass up the extradata, if any */
    self->extradata      = pd->ff_vcontext.extradata;
    self->extradata_size = pd->ff_vcontext.extradata_size;

    if (PSNR_REQUESTED(pd)) {
        /* errors already logged, and they can be ignored */
        psnr_open(pd);
        pd->confdata.flags.psnr = 0; /* no longer requested :^) */
    }
    return TC_OK;

failed:
    tc_lavc_fini_buf(pd);
    return TC_ERROR;
}

#undef ABORT_IF_NOT_OK


static int tc_lavc_inspect(TCModuleInstance *self,
                        const char *param, const char **value)
{
    TC_MODULE_SELF_CHECK(self, "inspect");
    TC_MODULE_SELF_CHECK(value, "inspect");

    if (optstr_lookup(param, "help")) {
        *value = tc_lavc_help;
    }

    if (optstr_lookup(param, "vcodec")) {
        *value = "must be selected by user\n";
    }

    if (optstr_lookup(param, "list")) {
        *value = tc_lavc_list_codecs();
    }
    return TC_OK;
}

static int tc_lavc_stop(TCModuleInstance *self)
{
    TCLavcPrivateData *pd = NULL;

    TC_MODULE_SELF_CHECK(self, "stop");

    pd = self->userdata;

    tc_lavc_fini_buf(pd);

    if (PSNR_REQUESTED(pd)) {
        psnr_print(pd);
        psnr_close(pd);
    }

    tc_lavc_fini_rc_override(pd);
    /* ok, now really start the real teardown */
    tc_lavc_fini_multipass(pd);

    if (pd->ff_vcodec != NULL) {
        avcodec_close(&pd->ff_vcontext);
        pd->ff_vcodec = NULL;
    }

    return TC_OK;
}


static int tc_lavc_encode_video(TCModuleInstance *self,
                                  vframe_list_t *inframe,
                                  vframe_list_t *outframe)
{
    TCLavcPrivateData *pd = NULL;

    TC_MODULE_SELF_CHECK(self, "encode_video");

    pd = self->userdata;

    pd->ff_venc_frame.interlaced_frame = pd->interlacing.active;
    pd->ff_venc_frame.top_field_first  = pd->interlacing.top_first;

    pd->pre_encode_video(pd, inframe); 

    TC_LOCK_LIBAVCODEC;
    outframe->video_len = avcodec_encode_video(&pd->ff_vcontext,
                                               outframe->video_buf,
                                               inframe->video_size,
                                               &pd->ff_venc_frame);
    TC_UNLOCK_LIBAVCODEC;

    if (outframe->video_len < 0) {
        tc_log_warn(MOD_NAME, "encoder error: size (%i)",
                    outframe->video_len);
        return TC_EXPORT_ERROR;
    }

    return tc_lavc_write_logs(pd, outframe->video_len);
}


/*************************************************************************/

static const TCModuleInfo tc_lavc_info = {
    .features    = TC_MODULE_FEATURE_ENCODE|TC_MODULE_FEATURE_VIDEO,
    .flags       = TC_MODULE_FLAG_RECONFIGURABLE,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = tc_lavc_codecs_in,
    .codecs_out  = tc_lavc_codecs_out,
    .formats_in  = tc_lavc_formats,
    .formats_out = tc_lavc_formats
};

static const TCModuleClass tc_lavc_class = {
    .info         = &tc_lavc_info,

    .init         = tc_lavc_init,
    .fini         = tc_lavc_fini,
    .configure    = tc_lavc_configure,
    .stop         = tc_lavc_stop,
    .inspect      = tc_lavc_inspect,

    .encode_video = tc_lavc_encode_video,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &tc_lavc_class;
}

/*************************************************************************/

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
