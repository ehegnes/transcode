/*
 * encode_x264.c - encodes video using the x264 library
 * Written by Christian Bodenstedt, with changes for NMS by Andrew Church
 * 
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

/*
 * Many parts of this file are taken from FFMPEGs "libavcodec/x264.c",
 * which is licensed under LGPL. Other sources of information were
 * "export_ffmpeg.c", X264s "x264.c" and MPlayers "libmpcodecs/ve_x264.c"
 * (all licensed GPL afaik).
 */


/* TODO: 

- Various smaller things -> search for "TODO" and for "QUESTIONS" in
  the rest of the code

*/


#include "transcode.h"
#include "libtc/libtc.h"
#include "libtc/cfgfile.h"
#include "libtc/optstr.h"
#include "libtc/tcmodule-plugin.h"
#include "libtc/ratiocodes.h"

#include <x264.h>


#define MOD_NAME    "encode_x264.so"
#define MOD_VERSION "v0.2.0 (2006-10-09)"
#define MOD_CAP     "x264 encoder"


/* Module configuration file */
#define X264_CONFIG_FILE "x264.cfg"

/* Private data for this module */
typedef struct {
    int framenum;
    int interval;
    int width;
    int height;
    x264_param_t x264params;
    x264_t *enc;
} X264PrivateData;

/* Static structure to provide pointers for configuration entries */
static struct confdata_struct {
    x264_param_t x264params;
    /* No local parameters at the moment */
} confdata;

/*************************************************************************/

/* This array describes all option-names, pointers to where their
 * values are stored and the allowed ranges. It's needed to parse the
 * x264.cfg file using libtc. */

/* Use e.g. OPTION("overscan", vui.i_overscan) for x264params.vui.i_overscan */
#define OPTION(field,name,type,flag,low,high) \
    {name, &confdata.x264params.field, (type), (flag), (low), (high)},

/* Option to turn a flag on or off; the off version will have "no" prepended */
#define OPT_FLAG(field,name) \
    OPTION(field,      name, TCCONF_TYPE_FLAG, 0, 0, 1) \
    OPTION(field, "no" name, TCCONF_TYPE_FLAG, 0, 1, 0)
/* Integer option with range */
#define OPT_RANGE(field,name,low,high) \
    OPTION(field, name, TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, (low), (high))
/* Floating-point option */
#define OPT_FLOAT(field,name) \
    OPTION(field, name, TCCONF_TYPE_FLOAT, 0, 0, 0)
/* Floating-point option with range */
#define OPT_RANGF(field,name,low,high) \
    OPTION(field, name, TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, (low), (high))
/* String option */
#define OPT_STR(field,name) \
    OPTION(field, name, TCCONF_TYPE_STRING, 0, 0, 0)
/* Dummy entry that doesn't generate an option (placeholder) */
#define OPT_NONE(field) /*nothing*/

static TCConfigEntry conf[] ={

    /* CPU flags */

    /* CPU acceleration flags (we leave the x264 default alone) */
    OPT_NONE (cpu)
    /* Divide each frame into multiple slices, encode in parallel */
    OPT_RANGE(i_threads,                  "threads",        1,     4)

    /* Video Properties */

    OPT_NONE (i_width)
    OPT_NONE (i_height)
    OPT_NONE (i_csp)  /* CSP of encoded bitstream, only i420 supported */
    /* H.264 level (1.0 ... 5.1) */
    OPT_RANGE(i_level_idc,                "level_idc",     10,    51)
    OPT_NONE (i_frame_total) /* number of frames to encode if known, else 0 */

    /* they will be reduced to be 0 < x <= 65535 and prime */
    OPT_NONE (vui.i_sar_height)
    OPT_NONE (vui.i_sar_width)

    /* 0=undef, 1=show, 2=crop */
    OPT_RANGE(vui.i_overscan,             "overscan",       0,     2)

    /* 0=component 1=PAL 2=NTSC 3=SECAM 4=Mac 5=undef */
    OPT_RANGE(vui.i_vidformat,            "vidformat",      0,     5)
    OPT_FLAG (vui.b_fullrange,            "fullrange")
    /* 1=bt709 2=undef 4=bt470m 5=bt470bg 6=smpte170m 7=smpte240m 8=film */
    OPT_RANGE(vui.i_colorprim,            "colorprim",      0,     8)
    /* 1..7 as above, 8=linear, 9=log100, 10=log316 */
    OPT_RANGE(vui.i_transfer,             "transfer",       0,    10)
    /* 0=GBR 1=bt709 2=undef 4=fcc 5=bt470bg 6=smpte170m 7=smpte240m 8=YCgCo */
    OPT_RANGE(vui.i_colmatrix,            "colmatrix",      0,     8)
    /* ??? */
    OPT_RANGE(vui.i_chroma_loc,           "chroma_loc",     0,     5)

    OPT_NONE (i_fps_num)
    OPT_NONE (i_fps_den)

    /* Bitstream parameters */

    /* Maximum number of reference frames */
    OPT_RANGE(i_frame_reference,          "frameref",       1,    16)
    /* Force an IDR keyframe at this interval */
    OPT_RANGE(i_keyint_max,               "keyint",         1,999999)
    OPT_RANGE(i_keyint_max,               "keyint_max",     1,999999)
    /* Scenecuts closer together than this are coded as I, not IDR. */
    OPT_RANGE(i_keyint_min,               "keyint_min",     1,999999)
    /* How aggressively to insert extra I frames */
    OPT_RANGE(i_scenecut_threshold,       "scenecut",      -1,   100)
    /* How many B-frames between 2 reference pictures */
    OPT_RANGE(i_bframe,                   "bframes",        0,    16)
    /* Use adaptive B-frame encoding */
    OPT_FLAG (b_bframe_adaptive,          "b_adapt")
    /* How often B-frames are used */
    OPT_RANGE(i_bframe_bias,              "b_bias",       -90,   100)
    /* Keep some B-frames as references */
    OPT_FLAG (b_bframe_pyramid,           "b_pyramid")

    /* Use deblocking filter */
    OPT_FLAG (b_deblocking_filter,        "deblock")
    /* [-6, 6] -6 light filter, 6 strong */
    OPT_RANGE(i_deblocking_filter_alphac0,"deblockalpha",  -6,     6)
    /* [-6, 6]  idem */
    OPT_RANGE(i_deblocking_filter_beta,   "deblockbeta",   -6,     6)

    /* Use context-adaptive binary arithmetic coding */
    OPT_FLAG (b_cabac,                    "cabac")
    /* Initial data for CABAC? */
    OPT_RANGE(i_cabac_init_idc,           "cabac_init_idc", 0,     2)

    /* Enable interlaced encoding (--encode_fields) */
    OPT_NONE (b_interlaced)

    /* Quantization matrix selection: 0=flat 1=JVT 2=custom */
    OPT_RANGE(i_cqm_preset,               "cqm",            0,     2)
    /* Custom quant matrix filename */
    OPT_STR  (psz_cqm_file,               "cqm_file")
    /* Quant matrix arrays set up by library */

    /* Logging */

    OPT_NONE (pf_log)
    OPT_NONE (p_log_private)
    OPT_NONE (i_log_level)
    OPT_NONE (b_visualize)

    /* Encoder analyser parameters */

    /* Partition selection (we always enable everything) */
    OPT_NONE (analyse.intra)
    OPT_NONE (analyse.inter)
    /* Allow integer 8x8 DCT transforms */
    OPT_FLAG (analyse.b_transform_8x8,    "8x8dct")
    /* Implicit weighting for B-frames */
    OPT_FLAG (analyse.b_weighted_bipred,  "weight_b")
    /* Spatial vs temporal MV prediction, 0=none 1=spatial 2=temporal 3=auto */
    OPT_RANGE(analyse.i_direct_mv_pred,   "direct_pred",    0,     3)
    /* Forbid 4x4 direct partitions (-1 = auto) */
    OPT_FLAG (analyse.i_direct_8x8_inference, "direct_8x8")
    /* QP difference between chroma and luma */
    OPT_RANGE(analyse.i_chroma_qp_offset, "chroma_qp_offset",-12, 12)

    /* Motion estimation algorithm to use (X264_ME_*) 0=dia 1=hex 2=umh 3=esa*/
    OPT_RANGE(analyse.i_me_method,        "me",             0,     3)
    /* Integer pixel motion estimation search range (from predicted MV) */
    OPT_RANGE(analyse.i_me_range,         "me_range",       4,    64)
    /* Maximum length of a MV (in pixels), 32-2048 or -1=auto */
    OPT_RANGE(analyse.i_mv_range,         "mv_range",      -1,  2048)
    /* Subpixel motion estimation quality: 1=fast, 7=best */
    OPT_RANGE(analyse.i_subpel_refine,    "subq",           1,     7)
    /* Bidirectional motion estimation */
    OPT_FLAG (analyse.b_bidir_me,         "bidir_me")
    /* Chroma ME for subpel and mode decision in P-frames */
    OPT_FLAG (analyse.b_chroma_me,        "chroma_me")
    /* RD based mode decision for B-frames */
    OPT_FLAG (analyse.b_bframe_rdo,       "brdo")
    /* Allow each MB partition in P-frames to have its own reference number */
    OPT_FLAG (analyse.b_mixed_references, "mixed_refs")
    /* Trellis RD quantization */
    OPT_RANGE(analyse.i_trellis,          "trellis",        0,     2)
    /* Early SKIP detection on P-frames */
    OPT_FLAG (analyse.b_fast_pskip,       "fast_pskip")
    /* Transform coefficient thresholding on P-frames */
    OPT_FLAG (analyse.b_dct_decimate,     "dct_decimate")
    /* Noise reduction */
    OPT_RANGE(analyse.i_noise_reduction,  "nr",             0, 65536)
    /* Compute PSNR stats, at the cost of a few % of CPU time */
    OPT_FLAG (analyse.b_psnr,             "psnr")
    /* Compute SSIM stats, at the cost of a few % of CPU time */
    OPT_FLAG (analyse.b_ssim,             "ssim")

    /* Rate control parameters */

    /* QP value for constant-quality encoding (to be a transcode option,
     * eventually--FIXME) */
    OPT_NONE (rc.i_qp_constant)
    /* Minimum allowed QP value */
    OPT_RANGE(rc.i_qp_min,                "qp_min",         0,    51)
    /* Maximum allowed QP value */
    OPT_RANGE(rc.i_qp_max,                "qp_max",         0,    51)
    /* Maximum QP difference between frames */
    OPT_RANGE(rc.i_qp_step,               "qp_step",        0,    50)
    /* Bitrate (transcode -w) */
    OPT_NONE (rc.i_bitrate)
    /* Nominal QP for 1-pass VBR */
    OPT_RANGF(rc.f_rf_constant,           "crf",            0,    51)
    /* Allowed variance from average bitrate */
    OPT_FLOAT(rc.f_rate_tolerance,        "ratetol")
    /* Maximum local bitrate (kbit/s) */
    OPT_RANGE(rc.i_vbv_max_bitrate,       "vbv_maxrate",    0,240000)
    /* Size of VBV buffer for CBR encoding */
    OPT_RANGE(rc.i_vbv_buffer_size,       "vbv_bufsize",    0,240000)
    /* Initial occupancy of VBV buffer */
    OPT_RANGF(rc.f_vbv_buffer_init,       "vbv_init",     0.0,   1.0)
    /* QP ratio between I and P frames */
    OPT_FLOAT(rc.f_ip_factor,             "ip_ratio")
    /* QP ratio between P and B frames */
    OPT_FLOAT(rc.f_pb_factor,             "pb_ratio")

    /* Rate control equation for 2-pass encoding (like FFmpeg) */
    OPT_STR  (rc.psz_rc_eq,               "rc_eq")
    /* Complexity blurring before QP compression */
    OPT_RANGF(rc.f_complexity_blur,       "cplx_blur",    0.0, 999.0)
    /* QP curve compression: 0.0 = constant bitrate, 1.0 = constant quality */
    OPT_RANGF(rc.f_qcompress,             "qcomp",        0.0,   1.0)
    /* QP blurring after compression */
    OPT_RANGF(rc.f_qblur,                 "qblur",        0.0,  99.0)
    /* Rate control override zones (not supported by transcode) */
    OPT_NONE (rc.zones)
    OPT_NONE (rc.i_zones)
    /* Alternate method of specifying zones */
    OPT_STR  (rc.psz_zones,               "zones")

    /* Other parameters */

    OPT_NONE (b_aud)
    OPT_NONE (b_repeat_headers)
    OPT_NONE (i_sps_id)

    {NULL}
};

/*************************************************************************/
/*************************************************************************/

/**
 * x264_log:  Logging routine for x264 library.
 *
 * Parameters:
 *     userdata: Unused.
 *        level: x264 log level (X264_LOG_*).
 *       format: Log message format string.
 *         args: Log message format arguments.
 * Return value:
 *     None.
 */

static void x264_log(void *userdata, int level, const char *format,
                     va_list args)
{
    TCLogLevel tclevel;
    char buf[1000];

    if (!format)
        return;
    switch (level) {
      case X264_LOG_ERROR:
        tclevel = TC_LOG_ERR;
        break;
      case X264_LOG_WARNING:
        tclevel = TC_LOG_WARN;
        break;
      case X264_LOG_INFO:
        if (!(verbose & TC_INFO))
            return;
        tclevel = TC_LOG_INFO;
        break;
      case X264_LOG_DEBUG:
        if (!(verbose & TC_DEBUG))
            return;
        tclevel = TC_LOG_MSG;
        break;
      default:
        return;
    }
    tc_vsnprintf(buf, sizeof(buf), format, args);
    buf[strcspn(buf,"\r\n")] = 0;  /* delete trailing newline */
    tc_log(tclevel, MOD_NAME, "%s", buf);
}

/*************************************************************************/

/**
 * vob_get_sample_aspect_ratio:  Set $sar_num and $sar_den to the sample
 * aspect ratio (also called pixel aspect ratio) described by $vob->ex_par,
 * $vob->ex_par_width, $vob->ex_par_height and vob->ex_asr.
 *
 * This function might return quite high values in $sar_num and
 * $sar_den. Depending on what codec these parameters are given to,
 * eventually a common factor should be reduced first. In case of x264
 * this is not needed, since it's done in x264's code.
 *
 * QUESTION: should this be the job of an export-module?
 *
 * Parameters:
 *     sar_num: integer to store SAR-numerator in.
 *     sar_den: integer to store SAR-denominator in.
 *
 * Returns:
 *     0 on success, nonzero otherwise (this means bad parameters).
 */

static int vob_get_sample_aspect_ratio(const vob_t *vob,
                                       int *sar_num, int *sar_den)
{
    int num, den;

    if (!vob || !sar_num || !sar_den)
        return TC_ERROR;

    /* Aspect Ratio Calculations (modified code from export_ffmpeg.c) */
    if (vob->export_attributes & TC_EXPORT_ATTRIBUTE_PAR) {
        if (vob->ex_par > 0) {
            /* 
             * vob->ex_par MUST be guarantee to be in a sane range
             * by core (transcode/tcexport 
             */
            tc_par_code_to_ratio(vob->ex_par, &num, &den);
        } else {
            /* same as above */
            num = vob->ex_par_width;
            den = vob->ex_par_height;
        }
        tc_log_info(MOD_NAME, "DAR value ratio calculated as"
                              " %f = %d/%d",
                              (double)num/(double)den, num, den);
    } else {
        if (vob->export_attributes & TC_EXPORT_ATTRIBUTE_ASR) {
            /* same as above for PAR stuff */
            tc_asr_code_to_ratio(vob->ex_asr, &num, &den);
            tc_log_info(MOD_NAME, "Display aspect ratio calculated as"
                                  " %f = %d/%d",
                                 (double)num/(double)den, num, den);

            /* ffmpeg FIXME:
             * This original code might lead to rounding/truncating errors
             * and maybe produces too high values for "den" and
             * "num" for -y ffmpeg -F mpeg4
             *
             * sar = dar * ((double)vob->ex_v_height / (double)vob->ex_v_width);
             * lavc_venc_context->sample_aspect_ratio.num = (int)(sar * 1000);
             * lavc_venc_context->sample_aspect_ratio.den = 1000;
             */

             num *= vob->ex_v_height;
             den *= vob->ex_v_width;
             /* I don't need to reduce since x264 does it itself :-) */
             tc_log_info(MOD_NAME, "Sample aspect ratio calculated as"
                                   " %f = %d/%d",
                                   (double)num/(double)den, num, den);

        } else { /* user did not specify asr at all, assume no change */
            tc_log_info(MOD_NAME, "Set display aspect ratio to input");
            num = 1;
            den = 1;
        }
    }

    *sar_num = num;
    *sar_den = den;
    return TC_OK;
}

/*************************************************************************/

/**
 * x264params_set_multipass:  Does all settings related to multipass.
 *
 * Parameters:
 *              pass: 0 = single pass
 *                    1 = 1st pass
 *                    2 = 2nd pass
 *                    3 = Nth pass, must also be used for 2nd pass
 *                                  of multipass encoding.
 *     statsfilename: where to read and write multipass stat data.
 * Return value:
 *     always 0.
 * Preconditions:
 *     params != NULL
 *     pass == 0 || statsfilename != NULL
 */

static int x264params_set_multipass(x264_param_t *params,
                                    int pass, const char *statsfilename)
{
    /* Drop the const and hope that x264 treats it as const anyway */
    params->rc.psz_stat_in  = (char *)statsfilename;
    params->rc.psz_stat_out = (char *)statsfilename;

    switch (pass) {
      default:
        params->rc.b_stat_write = 0;
        params->rc.b_stat_read = 0;
        break;
      case 1:
        params->rc.b_stat_write = 1;
        params->rc.b_stat_read = 0;
        break;
      case 2:
        params->rc.b_stat_write = 0;
        params->rc.b_stat_read = 1;
        break;
      case 3:
        params->rc.b_stat_write = 1;
        params->rc.b_stat_read = 1;
        break;
    }
    return TC_OK;
}

/*************************************************************************/

/**
 * Checks or corrects some strange combinations of settings done in
 * x264params.
 *
 * Parameters:
 *     params: x264_param_t structure to check
 * Return value:
 *     0 on success, nonzero otherwise.
 */

static int x264params_check(x264_param_t *params)
{
    /* don't know if these checks are really needed, but they won't hurt */
    if (params->rc.i_qp_min > params->rc.i_qp_constant) {
        params->rc.i_qp_min = params->rc.i_qp_constant;
    }
    if (params->rc.i_qp_max < params->rc.i_qp_constant) {
        params->rc.i_qp_max = params->rc.i_qp_constant;
    }

    if (params->rc.i_rc_method == X264_RC_ABR) {
        if ((params->rc.i_vbv_max_bitrate > 0)
            != (params->rc.i_vbv_buffer_size > 0)
        ) {
            tc_log_error(MOD_NAME,
                         "VBV requires both vbv_maxrate and vbv_bufsize.");
            return TC_ERROR;
        }
    }
    return TC_OK;
}

/*************************************************************************/

/**
 * x264params_set_by_vob:  Handle transcode CLI and tc-autodetection
 * dependent entries in x264_param_t.
 *
 * This method copies various values from transcodes vob_t structure to
 * x264 $params. That means all settings that can be done through
 * transcodes CLI or autodetection are applied to x264s $params here
 * (and I hope nowhere else).
 *
 * Parameters:
 *     params: x264_param_t structure to apply changes to
 *        vob: transcodes vob_t structure to copy values from
 * Return value:
 *     0 on success, nonzero otherwise.
 * Preconditions:
 *     params != NULL
 *     vob != NULL
 */

static int x264params_set_by_vob(x264_param_t *params, const vob_t *vob)
{
    /* Set video/bitstream parameters */

    params->i_width = vob->ex_v_width;
    params->i_height = vob->ex_v_height;
    params->b_interlaced = (vob->encode_fields==1 || vob->encode_fields==2);

    /* TODO: allow other modes than cbr */
    params->rc.i_rc_method = X264_RC_ABR; /* use bitrate instead of CQP */
    params->rc.i_bitrate = vob->divxbitrate; /* what a name */

    if (TC_NULL_MATCH == tc_frc_code_to_ratio(vob->ex_frc,
                                              &params->i_fps_num,
                                              &params->i_fps_den)
    ) {
        if (vob->ex_fps > 29.9 && vob->ex_fps < 30) {
            params->i_fps_num = 30000;
            params->i_fps_den = 1001;
        } else if (vob->ex_fps > 23.9 && vob->ex_fps < 24) {
            params->i_fps_num = 24000;
            params->i_fps_den = 1001;
        } else if (vob->ex_fps > 59.9 && vob->ex_fps < 60) {
            params->i_fps_num = 60000;
            params->i_fps_den = 1001;
        } else {
            params->i_fps_num = vob->ex_fps * 1000;
            params->i_fps_den = 1000;
        }
    }

    if (0 != vob_get_sample_aspect_ratio(vob,
                                         &params->vui.i_sar_width,
                                         &params->vui.i_sar_height)
    ) {
        return TC_ERROR;
    }

    /* Set logging function and acceleration flags */
    params->pf_log = x264_log;
    params->p_log_private = NULL;
    params->cpu = 0;
    if (tc_accel & AC_MMX)      params->cpu |= X264_CPU_MMX;
    if (tc_accel & AC_MMXEXT)   params->cpu |= X264_CPU_MMXEXT;
    if (tc_accel & AC_SSE)      params->cpu |= X264_CPU_SSE;
    if (tc_accel & AC_SSE2)     params->cpu |= X264_CPU_SSE2;
    if (tc_accel & AC_3DNOW)    params->cpu |= X264_CPU_3DNOW;
    if (tc_accel & AC_3DNOWEXT) params->cpu |= X264_CPU_3DNOWEXT;

    return TC_OK;
}

/*************************************************************************/
/*************************************************************************/

/* Module interface routines and data. */

/*************************************************************************/

/**
 * x264_init:  Initialize this instance of the module.  See tcmodule-data.h
 * for function details.
 */

static int x264_init(TCModuleInstance *self)
{
    X264PrivateData *pd = NULL;

    TC_MODULE_SELF_CHECK(self, "init");

    pd = tc_malloc(sizeof(X264PrivateData));
    if (!pd) {
        tc_log_error(MOD_NAME, "init: out of memory!");
        return TC_ERROR;
    }
    pd->framenum = 0;
    pd->enc = NULL;

    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }
    self->userdata = pd;

    return TC_OK;
}

/*************************************************************************/

/**
 * x264_fini:  Clean up after this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int x264_fini(TCModuleInstance *self)
{
    X264PrivateData *pd = NULL;

    TC_MODULE_SELF_CHECK(self, "fini");

    pd = self->userdata;

    if (pd->enc) {
        x264_encoder_close(pd->enc);
        pd->enc = NULL;
    }

    tc_free(self->userdata);
    self->userdata = NULL;
    return TC_OK;
}

/*************************************************************************/

/**
 * x264_configure:  Configure this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int x264_configure(TCModuleInstance *self,
                         const char *options, vob_t *vob)
{
    X264PrivateData *pd = NULL;
    char *s;

    TC_MODULE_SELF_CHECK(self, "configure");

    pd = self->userdata;

    /* Initialize parameter block */
    memset(&confdata, 0, sizeof(confdata));
    x264_param_default(&confdata.x264params);

    /* Parameters not (yet) settable via options: */
    confdata.x264params.analyse.intra = ~0;
    confdata.x264params.analyse.inter = ~0;

    /* Read settings from configuration file */
    module_read_config(X264_CONFIG_FILE, NULL, conf, MOD_NAME);

    /* Parse options given in -y option string (format:
     * "name1=value1:name2=value2:...") */
    for (s = (vob->ex_v_string ? strtok(vob->ex_v_string,":") : NULL);
         s != NULL;
         s = strtok(NULL,":")
    ) {
        if (!module_read_config_line(s, conf, MOD_NAME)) {
            tc_log_error(MOD_NAME, "Error parsing module options");
            return TC_ERROR;
        }
    }

    /* Apply extra settings to $x264params */
    if (0 != x264params_set_multipass(&confdata.x264params, vob->divxmultipass,
                                      vob->divxlogfile)
    ) {
        tc_log_error(MOD_NAME, "Failed to apply multipass settings.");
        return TC_ERROR;
    }

    /* Copy parameter block to module private data */
    ac_memcpy(&pd->x264params, &confdata.x264params, sizeof(pd->x264params));

    /* Apply transcode CLI and autodetected values from $vob to
     * $x264params. This is done as the last step to make transcode CLI
     * override any settings done before. */
    if (0 != x264params_set_by_vob(&pd->x264params, vob)) {
        tc_log_error(MOD_NAME, "Failed to evaluate vob_t values.");
        return TC_ERROR;
    }

    /* Test if the set parameters fit together. */
    if (0 != x264params_check(&pd->x264params)) {
        return TC_ERROR;
    }

    /* Now we've set all parameters gathered from transcode and the config
     * file to $x264params. Let's give some status report and finally open
     * the encoder. */
    if (verbose >= TC_DEBUG) {
        module_print_config(conf, MOD_NAME);
    }

    pd->enc = x264_encoder_open(&pd->x264params);
    if (!pd->enc) {
        tc_log_error(MOD_NAME, "x264_encoder_open() returned NULL - sorry.");
        return TC_ERROR;
    }

    return TC_OK;
}

/*************************************************************************/

/**
 * x264_stop:  Reset this instance of the module.  See tcmodule-data.h for
 * function details.
 */

static int x264_stop(TCModuleInstance *self)
{
    X264PrivateData *pd = NULL;

    TC_MODULE_SELF_CHECK(self, "stop");

    pd = self->userdata;

    if (pd->enc) {
        x264_encoder_close(pd->enc);
        pd->enc = NULL;
    }

    return TC_OK;
}

/*************************************************************************/

/**
 * x264_inspect:  Return the value of an option in this instance of the
 * module.  See tcmodule-data.h for function details.
 */

static int x264_inspect(TCModuleInstance *self,
                        const char *param, const char **value)
{
    X264PrivateData *pd = NULL;
    static char buf[TC_BUF_MAX];

    if (!self || !param || !value) {
        return TC_ERROR;
    }
    pd = self->userdata;

    if (optstr_lookup(param, "help")) {
        tc_snprintf(buf, sizeof(buf),
"Overview:\n"
"    Encodes video in h.264 format using the x264 library.\n"
"Options available:\n"
"    All options in x264.cfg can be specified on the command line\n"
"    using the format: -y x264=name1=value1:name2=value2:...\n");
        *value = buf;
    }
    /* FIXME: go through the option list to find a match to param */

    return TC_OK;
}

/*************************************************************************/

/**
 * x264_encode_video:  Decode a frame of data.  See tcmodule-data.h for
 * function details.
 */

static int x264_encode_video(TCModuleInstance *self,
                            vframe_list_t *inframe, vframe_list_t *outframe)
{
    X264PrivateData *pd;
    x264_nal_t *nal;
    int nnal, i;
    x264_picture_t pic, pic_out;

    if (!self) {
        return TC_OK;
    }
    pd = self->userdata;

    pd->framenum++;

    pic.img.i_csp = X264_CSP_I420;
    pic.img.i_plane = 3;

    pic.img.plane[0] = inframe->video_buf;
    pic.img.i_stride[0] = inframe->v_width;

    pic.img.plane[1] = pic.img.plane[0] + inframe->v_width*inframe->v_height;
    pic.img.i_stride[1] = inframe->v_width / 2;

    pic.img.plane[2] = pic.img.plane[1]
                     + (inframe->v_width/2)*(inframe->v_height/2);
    pic.img.i_stride[2] = inframe->v_width / 2;


    pic.i_type = X264_TYPE_AUTO;
    pic.i_qpplus1 = 0;
    /* QUESTION: Is this pts-handling ok? I don't have a clue how
     * PTS/DTS handling works. Does it matter, when no muxing is
     * done? */
    pic.i_pts = (int64_t) pd->framenum * pd->x264params.i_fps_den;

    if (x264_encoder_encode(pd->enc, &nal, &nnal, &pic, &pic_out) != 0) {
        return TC_ERROR;
    }

    outframe->video_len = 0;
    for (i = 0; i < nnal; i++) {
        int size, ret;

        size = outframe->video_size - outframe->video_len;
        if (size <= 0) {
            tc_log_error(MOD_NAME, "output buffer overflow");
            return TC_ERROR;
        }
        ret = x264_nal_encode(outframe->video_buf + outframe->video_len,
                              &size, 1, &nal[i]);
        if (ret < 0 || size > outframe->video_size - outframe->video_len) {
            tc_log_error(MOD_NAME, "output buffer overflow");
            break;
        }
        outframe->video_len += size;
    }

    return TC_OK;
}

/*************************************************************************/

static const int x264_codecs_in[] = { TC_CODEC_YUV420P, TC_CODEC_ERROR };
static const int x264_codecs_out[] = { TC_CODEC_H264, TC_CODEC_ERROR };

static const TCModuleInfo x264_info = {
    .features    = TC_MODULE_FEATURE_ENCODE
                 | TC_MODULE_FEATURE_VIDEO,
    .flags       = TC_MODULE_FLAG_RECONFIGURABLE,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = x264_codecs_in,
    .codecs_out  = x264_codecs_out
};

static const TCModuleClass x264_class = {
    .info         = &x264_info,

    .init         = x264_init,
    .fini         = x264_fini,
    .configure    = x264_configure,
    .stop         = x264_stop,
    .inspect      = x264_inspect,

    .encode_video = x264_encode_video,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &x264_class;
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

