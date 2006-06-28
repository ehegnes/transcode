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

- ATM only CBR encoding is supported. Mencoder doesn't set some
  switches when other modes than CBR are chosen. Have a look at this
  code, before adding other encoding-modes!
 
  MEncoder code:

  |  if(bitrate > 0) {
  |      if((vbv_maxrate > 0) != (vbv_bufsize > 0)) {
  |          mp_msg(MSGT_MENCODER, MSGL_ERR,
  |                 "VBV requires both vbv_maxrate and vbv_bufsize.\n");
  |          return 0;
  |      }
  |      mod->param.rc.b_cbr = 1;
  |      mod->param.rc.i_bitrate = bitrate;
  |      mod->param.rc.f_rate_tolerance = ratetol;
  |      mod->param.rc.i_vbv_max_bitrate = vbv_maxrate;
  |      mod->param.rc.i_vbv_buffer_size = vbv_bufsize;
  |      mod->param.rc.f_vbv_buffer_init = vbv_init;
  |  }

  QUESTION: May I set f_rate_tolerance, i_bitrate, etc. when in other
            mode than CBR?
*/


#include "transcode.h"
#include "libtc/cfgfile.h"
#include "libtc/optstr.h"
#include "libtc/tcmodule-plugin.h"
#include "libtc/ratiocodes.h"
#include <x264.h>


#define MOD_NAME    "encode_x264.so"
#define MOD_VERSION "v0.1 (2006-06-05)"
#define MOD_CAP     "x264 encoder"


/* Module configuration file */
#define X264_CONFIG_FILE "x264.cfg"

/* Buffer size like in x264.c */
#define BUFFER_SIZE 3000000

/* Private data for this module */
typedef struct {
    int framenum;
    int interval;
    int width, height;
    uint8_t buffer[BUFFER_SIZE];

    x264_param_t x264params;
    x264_t *enc;
} PrivateData;

/* Static structure to provide pointers for configuration entries */
static struct confdata_struct {
    x264_param_t x264params;

    /* configfile options that don't modify entries in x264_param_t directly */
    int turbo;     /* turbo mode for first pass like in mencoder*/
                   /* -> affects: i_frame_reference, i_subpel_refine,
                         analyse.inter, i_trellis, i_me_method,
                         b_transform_8x8, b_weighted_bipred */
    int me_method; /* motion estimation method (i_me_method) */
    int me_range;  /* TODO: what is it? */
} confdata;

/*************************************************************************/

/* This array describes all option-names, pointers to where their
 * values are stored and the allowed ranges. It's needet to parse the
 * x264.cfg file using transcodes libioaux.
 *
 * C++-style comments (//) contain the entries in x264_param_t that
 * can't be set directly through x264.cfg (ATM). Some of them are set from
 * transcodes CLI or autodetected values. The entries in the array are
 * in the same order as in the struct's definition in x264.h - so I
 * won't miss to integrate the missing ones later (at least I hope
 * so).
 */

static TCConfigEntry conf[] ={

//  /* CPU flags */
//  unsigned int cpu;
    /* divide each frame into multiple slices, encode in parallel */
    {"threads",            &confdata.x264params.i_threads, TCCONF_TYPE_INT,
     TCCONF_FLAG_RANGE, 1, 4},
//
//  /* Video Properties */
//  int         i_width;
//  int         i_height;
//  int         i_csp;  /* CSP of encoded bitstream, only i420 supported */
    /* ??? */
    {"level_idc",         &confdata.x264params.i_level_idc,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 10, 51},
//  int         i_frame_total; /* number of frames to encode if known, else 0 */
//
//  struct
//  {
//      /* they will be reduced to be 0 < x <= 65535 and prime */
//      int         i_sar_height;
//      int         i_sar_width;
//
//      int         i_overscan;    /* 0=undef, 1=no overscan, 2=overscan */
//
//      /* see h264 annex E for the values of the following */
//      int         i_vidformat;
//      int         b_fullrange;
//      int         i_colorprim;
//      int         i_transfer;
//      int         i_colmatrix;
//      int         i_chroma_loc;    /* both top & bottom */
//  } vui;
//
//  int         i_fps_num;
//  int         i_fps_den;
//
//  /* Bitstream parameters */
    /* Maximum number of reference frames */
    {"frameref",          &confdata.x264params.i_frame_reference,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 1, 16},
    /* Force an IDR keyframe at this interval */
    {"keyint",            &confdata.x264params.i_keyint_max,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 24000000},
    /* Scenecuts closer together than this are coded as I, not IDR. */
    {"keyint_min",        &confdata.x264params.i_keyint_min,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 24000000},
    /* how aggressively to insert extra I frames */
    {"scenecut",          &confdata.x264params.i_scenecut_threshold,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, -1, 100},
    /* how many b-frame between 2 references pictures */
    {"bframes",           &confdata.x264params.i_bframe,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 16},
    /* ??? */
    {"b_adapt",           &confdata.x264params.b_bframe_adaptive,
     TCCONF_TYPE_FLAG, 0, 0, 1},
    /* ??? */
    {"nob_adapt",         &confdata.x264params.b_bframe_adaptive,
     TCCONF_TYPE_FLAG, 0, 1, 0},
    /* ??? */
    {"b_bias",            &confdata.x264params.i_bframe_bias,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, -100, 100},
    /* Keep some B-frames as references */
    {"b_pyramid",         &confdata.x264params.b_bframe_pyramid,
     TCCONF_TYPE_FLAG, 0, 0, 1},
    /* Keep some B-frames as references */
    {"nob_pyramid",       &confdata.x264params.b_bframe_pyramid,
     TCCONF_TYPE_FLAG, 0, 1, 0},

    /* ??? */
    {"deblock",           &confdata.x264params.b_deblocking_filter,
     TCCONF_TYPE_FLAG, 0, 0, 1},
    /* ??? */
    {"nodeblock",         &confdata.x264params.b_deblocking_filter,
     TCCONF_TYPE_FLAG, 0, 1, 0},
    /* [-6, 6] -6 light filter, 6 strong */
    {"deblockalpha",      &confdata.x264params.i_deblocking_filter_alphac0,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, -6, 6},
    /* [-6, 6]  idem */
    {"deblockbeta",       &confdata.x264params.i_deblocking_filter_beta,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, -6, 6},

    /* ??? */
    {"cabac",             &confdata.x264params.b_cabac,
     TCCONF_TYPE_FLAG, 0, 0, 1},
    /* ??? */
    {"nocabac",           &confdata.x264params.b_cabac,
     TCCONF_TYPE_FLAG, 0, 1, 0},
//  int         i_cabac_init_idc;
//
//  int         i_cqm_preset;
//  char        *psz_cqm_file;      /* JM format */
//  uint8_t     cqm_4iy[16];        /* used only if i_cqm_preset == X264_CQM_CUSTOM */
//  uint8_t     cqm_4ic[16];
//  uint8_t     cqm_4py[16];
//  uint8_t     cqm_4pc[16];
//  uint8_t     cqm_8iy[64];
//  uint8_t     cqm_8py[64];
//
//  /* Log */
//  void        (*pf_log)( void *, int i_level, const char *psz, va_list );
//  void        *p_log_private;
//  int         i_log_level;
//  int         b_visualize;
//
//  /* Encoder analyser parameters */
//  struct
//  {
    /* ??? */
    {"8x8dct",            &confdata.x264params.analyse.b_transform_8x8,
     TCCONF_TYPE_FLAG, 0, 0, 1},
    /* ??? */
    {"no8x8dct",          &confdata.x264params.analyse.b_transform_8x8,
     TCCONF_TYPE_FLAG, 0, 1, 0},
    /* implicit weighting for B-frames */
    {"weight_b",          &confdata.x264params.analyse.b_weighted_bipred,
     TCCONF_TYPE_FLAG, 0, 0, 1},
    /* implicit weighting for B-frames */
    {"noweight_b",        &confdata.x264params.analyse.b_weighted_bipred,
     TCCONF_TYPE_FLAG, 0, 1, 0},
    /* spatial vs temporal mv prediction */
    {"direct_pred",       &confdata.x264params.analyse.i_direct_mv_pred,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 3},
    /* ??? */
    {"chroma_qp_offset",  &confdata.x264params.analyse.i_chroma_qp_offset,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, -12, 12},

    /* motion estimation algorithm to use (X264_ME_*) */
    {"me",                &confdata.me_method,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 1, 4},
    /* integer pixel motion estimation search range (from predicted mv) */
    {"me_range",          &confdata.x264params.analyse.i_me_range,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 4, 64},
//      int          i_mv_range; /* maximum length of a mv (in pixels) */
    /* subpixel motion estimation quality */
    {"subq",              &confdata.x264params.analyse.i_subpel_refine,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 1, 7},
    /* chroma ME for subpel and mode decision in P-frames */
    {"chroma_me",         &confdata.x264params.analyse.b_chroma_me,
     TCCONF_TYPE_FLAG, 0, 0, 1},
    /* chroma ME for subpel and mode decision in P-frames */
    {"nochroma_me",       &confdata.x264params.analyse.b_chroma_me,
     TCCONF_TYPE_FLAG, 0, 1, 0},
    /* RD based mode decision for B-frames */
    {"brdo",              &confdata.x264params.analyse.b_bframe_rdo,
     TCCONF_TYPE_FLAG, 0, 0, 1},
    /* RD based mode decision for B-frames */
    {"nobrdo",            &confdata.x264params.analyse.b_bframe_rdo,
     TCCONF_TYPE_FLAG, 0, 1, 0},
    /* allow each mb partition in P-frames to have it's own reference number */
    {"mixed_refs",        &confdata.x264params.analyse.b_mixed_references,
     TCCONF_TYPE_FLAG, 0, 0, 1},
    /* allow each mb partition in P-frames to have it's own reference number */
    {"nomixed_refs",      &confdata.x264params.analyse.b_mixed_references,
     TCCONF_TYPE_FLAG, 0, 1, 0},
    /* trellis RD quantization */
    {"trellis",           &confdata.x264params.analyse.i_trellis,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 2},
//      int          b_fast_pskip; /* early SKIP detection on P-frames */
//      int          i_noise_reduction; /* adaptive pseudo-deadzone */
//
    /* Do we compute PSNR stats (save a few % of cpu) */
    {"psnr",              &confdata.x264params.analyse.b_psnr,
     TCCONF_TYPE_FLAG, 0, 0, 1},
    /* Do we compute PSNR stats (save a few % of cpu) */
    {"nopsnr",            &confdata.x264params.analyse.b_psnr,
     TCCONF_TYPE_FLAG, 0, 1, 0},
//  } analyse;
//
//  /* Rate control parameters */
//  struct
//  {
    /* 0-51 */
    {"qp_constant",       &confdata.x264params.rc.i_qp_constant,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 51},
    /* 0-51 */
    {"qp",                &confdata.x264params.rc.i_qp_constant,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 51},
    /* min allowed QP value */
    {"qp_min",            &confdata.x264params.rc.i_qp_min,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 51},
    /* max allowed QP value */
    {"qp_max",            &confdata.x264params.rc.i_qp_max,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 51},
    /* max QP step between frames */
    {"qp_step",           &confdata.x264params.rc.i_qp_step,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 50},
//
//      int         b_cbr;          /* use bitrate instead of CQP */
//      int         i_bitrate;
    /* 1pass VBR, nominal QP */
    {"crf",               &confdata.x264params.rc.i_rf_constant,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 1, 50},
    /* ??? */
    {"ratetol",           &confdata.x264params.rc.f_rate_tolerance,
     TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, 0.1, 100.0},
    /* ??? */
    {"vbv_maxrate",       &confdata.x264params.rc.i_vbv_max_bitrate,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 24000000},
    /* ??? */
    {"vbv_bufsize",       &confdata.x264params.rc.i_vbv_buffer_size,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 24000000},
    /* ??? */
    {"vbv_init",          &confdata.x264params.rc.f_vbv_buffer_init,
     TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, 0.0, 1.0},
    /* ??? */
    {"ip_factor",         &confdata.x264params.rc.f_ip_factor,
     TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, -10.0, 10.0},
    /* ??? */
    {"pb_factor",         &confdata.x264params.rc.f_pb_factor,
     TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, -10.0, 10.0},

    /* 2pass params (same as ffmpeg ones) */
    /* 2 pass rate control equation */
    {"rc_eq",             &confdata.x264params.rc.psz_rc_eq,
     TCCONF_TYPE_STRING},
    /* 0.0 => cbr, 1.0 => constant qp */
    {"qcomp",             &confdata.x264params.rc.f_qcompress,
     TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, 0.0, 1.0},
    /* temporally blur quants */
    {"qblur",             &confdata.x264params.rc.f_qblur,
     TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, 0.0, 99.0},
    /* temporally blur complexity */
    {"cplx_blur",         &confdata.x264params.rc.f_complexity_blur,
     TCCONF_TYPE_FLOAT, TCCONF_FLAG_RANGE, 0.0, 999.0},
//      x264_zone_t *zones;         /* ratecontrol overrides */
//      int         i_zones;        /* sumber of zone_t's */
    /* alternate method of specifying zones */
    {"zones",             &confdata.x264params.rc.psz_zones,
     TCCONF_TYPE_STRING},
    {"turbo",             &confdata.turbo,
     TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 2},
//  } rc;
//
//  int b_aud;                  /* generate access unit delimiters */
//  int b_repeat_headers;       /* put SPS/PPS before each keyframe */
//} x264_param_t;
    {NULL}
};


/* Some more prepared options (for experiments only) */
/*     {"devel", "option names like in x264.h's x264_param_t", TCCONF_TYPE_SECTION, 0, 0, 0}, */
/*     {"i_vidformat",       &confdata.x264params.vui.i_vidformat,             TCCONF_TYPE_INT,     TCCONF_FLAG_RANGE, -127, 127}, /\* ??? *\/ */
/*     {"b_fullrange",       &confdata.x264params.vui.b_fullrange,             TCCONF_TYPE_INT,     TCCONF_FLAG_RANGE, -127, 127}, /\* ??? *\/ */
/*     {"i_colorprim",       &confdata.x264params.vui.i_colorprim,             TCCONF_TYPE_INT,     TCCONF_FLAG_RANGE, -127, 127}, /\* ??? *\/ */
/*     {"i_transfer",        &confdata.x264params.vui.i_transfer,              TCCONF_TYPE_INT,     TCCONF_FLAG_RANGE, -127, 127}, /\* ??? *\/ */
/*     {"i_colmatrix",       &confdata.x264params.vui.i_colmatrix,             TCCONF_TYPE_INT,     TCCONF_FLAG_RANGE, -127, 127}, /\* ??? *\/ */
/*     {"i_chroma_loc",      &confdata.x264params.vui.i_chroma_loc,            TCCONF_TYPE_INT,     TCCONF_FLAG_RANGE, -127, 127}, /\* both top & bottom *\/ */
/*     {"i_overscan",        &confdata.x264params.vui.i_overscan,              TCCONF_TYPE_INT,     TCCONF_FLAG_RANGE,    0,   2}, /\* 0=undef, 1=no overscan, 2=overscan *\/ */
/*     {"i_cabac_init_idc",  &confdata.x264params.i_cabac_init_idc,            TCCONF_TYPE_INT,     TCCONF_FLAG_RANGE, -127, 127}, /\* ??? *\/ */

/*     {"i_cqm_preset",      &confdata.x264params.i_cqm_preset,                TCCONF_TYPE_INT,     TCCONF_FLAG_RANGE, 0, 1}, /\* ??? *\/ */
/*     {"i_mv_range",        &confdata.x264params.analyse.i_mv_range,          TCCONF_TYPE_INT,     TCCONF_FLAG_RANGE, -127, 127}, /\* maximum length of a mv (in pixels) *\/ */
/*     {"b_cbr",             &confdata.x264params.rc.b_cbr,                    TCCONF_TYPE_FLAG,    0,             0,   1}, /\* use bitrate instead of CQP *\/ */
/*     {"i_bitrate",         &confdata.x264params.rc.i_bitrate,                TCCONF_TYPE_INT,     TCCONF_FLAG_RANGE, 0, MAX_INT}, /\* ??? *\/ */
/*     {"b_aud",             &confdata.x264params.b_aud,                       TCCONF_TYPE_FLAG,    0,             0,   1}, /\* generate access unit delimiters *\/ */
/*     {"intra",             &confdata.x264params.analyse.intra,               TCCONF_TYPE_INT,     TCCONF_FLAG_RANGE,    0,         255}, /\* intra partitions (unsigned int)*\/ */
/*     {"inter",             &confdata.x264params.analyse.inter,               TCCONF_TYPE_INT,     TCCONF_FLAG_RANGE,    0,         255}, /\* inter partitions (unsigned int)*\/ */

/*************************************************************************/
/*************************************************************************/

/* Some helper functions first */

static int min(int a, int b)
{
    if (a < b) return a; else return b;
}

static int max(int a, int b)
{
    if (a > b) return a; else return b;
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
 *     0 on success, nonzero otherwise.
 */

static int vob_get_sample_aspect_ratio(const vob_t *vob,
                                       int *sar_num, int *sar_den)
{
    int num, den;

    /* TODO: change error-handling */

    /* QUESTION: Do I really need to check, if $vob->ex_par,
         $vob->ex_par_width, $vob->ex_par_height and $vob->ex_asr are
         in a sane range? Aren't these checks performed by
         core-transcode somewhere?
    */
    if (!vob || !sar_num || !sar_den)
        return -1;

    /* Aspect Ratio Calculations (modified code from export_ffmpeg.c) */
    if (vob->export_attributes & TC_EXPORT_ATTRIBUTE_PAR) {
        if (vob->ex_par > 0) {
            switch(vob->ex_par) {
              case 1:
                num = 1;
                den = 1;
                break;
              case 2:
                num = 1200;
                den = 1100;
                break;
              case 3:
                num = 1000;
                den = 1100;
                break;
              case 4:
                num = 1600;
                den = 1100;
                break;
              case 5:
                num = 4000;
                den = 3300;
                break;
              default:
                tc_log_error(MOD_NAME, "Parameter value for --export_par"
                             " out of range (allowed: [1-5])");
                return -1;
            }
        } else {
            if (vob->ex_par_width > 0 && vob->ex_par_height > 0) {
                num = vob->ex_par_width;
                den = vob->ex_par_height;
            } else {
                tc_log_error(MOD_NAME, "Parameter values for --export_par"
                             " parameter out of range (allowed: [>0]/[>0])");
                return -1;
            }
        }
    } else {
        if (vob->export_attributes & TC_EXPORT_ATTRIBUTE_ASR) {
            if (vob->ex_asr > 0) {
                switch(vob->ex_asr) {
                  case 1: num =   1; den =   1; break;
                  case 2: num =   4; den =   3; break;
                  case 3: num =  16; den =   9; break;
                  case 4: num = 221; den = 100; break;
                  default:
                    tc_log_error(MOD_NAME, "Parameter value to --export_asr"
                                 " out of range (allowed: [1-4])");
                    return -1;
                }

                tc_log_info(MOD_NAME, "Display aspect ratio calculated as"
                            " %f = %d/%d", (double)num/(double)den, num, den);

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
                            " %f = %d/%d", (double)num/(double)den, num, den);

            } else {
                tc_log_error(MOD_NAME, "Parameter value to --export_asr"
                             " out of range (allowed: [1-4])");
                return -1;
            }
        } else { /* user did not specify asr at all, assume no change */
            tc_log_info(MOD_NAME, "Set display aspect ratio to input");
            num = 1;
            den = 1;
        }
    }

    *sar_num = num;
    *sar_den = den;

    return 0;
}

/*************************************************************************/

/**
 * x264params_set_multipass:  Does all settings related to multipass with
 * respect to turbo-mode for the first pass - this code is borrowed from
 * mencoder.
 *
 * This function encapsulates any code related to multipass encoding.
 *
 * TODO: check for NULL-pointer.
 *
 * Parameters:
 *              pass: 0 = single pass
 *                    1 = 1st pass
 *                    2 = 2nd pass
 *                    3 = Nth pass, must also be used for 2nd pass
 *                                  of multipass encoding.
 *
 *             turbo: 0   = high quality first pass encoding
 *                    1,2 = boost speed in first pass.
 *
 *     statsfilename: where to read and write multipass stat data.
 * Return value:
 *     always 0.
 * Preconditions:
 *     in x264 $params the following entries must be set before:
 *         i_frame_reference,
 *         i_subpel_refine,
 *         analyse.inter.
 */

static int x264params_set_multipass(x264_param_t *params,
                                    int pass, int turbo,
                                    const char *statsfilename)
{
    /* Drop the const and hope that x264 treats it as const anyway */
    params->rc.psz_stat_in  = (char *)statsfilename;
    params->rc.psz_stat_out = (char *)statsfilename;

    /* Multipass handling code, borrowed from MPlayer  */
    switch (pass) {
      case 0:
        params->rc.b_stat_write = 0;
        params->rc.b_stat_read = 0;
        break;
      case 1:
        /* MP Adjust or disable some flags to gain speed in the first pass */
        if (turbo == 1) {
            params->i_frame_reference = (params->i_frame_reference + 1) / 2;
            params->analyse.i_subpel_refine =
                max(min(3, params->analyse.i_subpel_refine - 1), 1);
            params->analyse.inter &= ~X264_ANALYSE_PSUB8x8;
            params->analyse.inter &= ~X264_ANALYSE_BSUB16x16;
            params->analyse.i_trellis = 0;
        } else if (turbo == 2) {
            params->i_frame_reference = 1;
            params->analyse.i_subpel_refine = 1;
            params->analyse.i_me_method = X264_ME_DIA;
            params->analyse.inter = 0;
            params->analyse.b_transform_8x8 = 0;
            params->analyse.b_weighted_bipred = 0;
            params->analyse.i_trellis = 0;
        }
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
    return 0;
}

/*************************************************************************/

/**
 * x264params_configur:e  Apply cfg-file options from $mencopts to
 * $x264params - this code is transcode-independent.
 *
 * Entries in $mencopts require additional checks before they can be
 * applied to $x264params.
 * All entries in $mencopts should be independent from transcode. So lets
 * apply them separate from transcode dependent settings (CLI-options).
 *
 * TODO: check NULL-pointers.
 *
 * Parameters:
 *     mpstat_filename: filename for multi-pass statistics
 *                pass: pass number
 * Return value:
 *     0 on success, nonzero otherwise.
 */

static int x264params_configure(const char *mpstat_filename, int pass)
{
    switch (confdata.me_method) {
        case 1: confdata.x264params.analyse.i_me_method = X264_ME_DIA; break;
        case 2: confdata.x264params.analyse.i_me_method = X264_ME_HEX; break;
        case 3: confdata.x264params.analyse.i_me_method = X264_ME_UMH; break;
        case 4: confdata.x264params.analyse.i_me_method = X264_ME_ESA; break;
    }

    /* turbo mode might change $i_me_method, which influences $i_me_range */
    if (0 != x264params_set_multipass(&confdata.x264params, pass,
                                      confdata.turbo, mpstat_filename)
    ) {
        fprintf(stderr, "Failed to apply multipass settings.\n");
        return -1;
    }

    /* don't know if there's a need for this check, but mencoder does so */
    if (confdata.x264params.analyse.i_me_method >= X264_ME_UMH
     && confdata.me_range != 0
    ) {
        confdata.x264params.analyse.i_me_range = confdata.me_range;
    }

    return 0;
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

    if (params->rc.b_cbr == 1) {
        if ((params->rc.i_vbv_max_bitrate > 0)
            != (params->rc.i_vbv_buffer_size > 0)
        ) {
            tc_log_error(MOD_NAME,
                         "VBV requires both vbv_maxrate and vbv_bufsize.");
            return -1;
        }
    }
    return 0;
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
 * TODO: check for NULL-pointers.
 *
 * Parameters:
 *     params: x264_param_t structure to apply changes to
 *        vob: transcodes vob_t structure to copy values from
 * Return value:
 *     0 on success, nonzero otherwise.
 */

static int x264params_set_by_vob(x264_param_t *params, const vob_t *vob)
{
    params->i_width = vob->ex_v_width;
    params->i_height = vob->ex_v_height;

    /* TODO: allow other modes than cbr */
    params->rc.b_cbr = 1; /* use bitrate instead of CQP */
    params->rc.i_bitrate = vob->divxbitrate; /* what a name */

    if (TC_NULL_MATCH == tc_frc_code_to_ratio(vob->ex_frc,
                                              &params->i_fps_num,
                                              &params->i_fps_den)
    ) {
        if (vob->ex_fps > 29 && vob->ex_fps < 30) {
            params->i_fps_num = 30000;
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
        return -1;
    }

    /* TODO: change x264's logging function (code in comments down
                                             there is from lavc) */
    /*         params->pf_log = X264_log; */
    /*         params->p_log_private = avctx;  */

    return 0;
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
    PrivateData *pd;

    if (!self) {
        tc_log_error(MOD_NAME, "init: self == NULL!");
        return -1;
    }

    self->userdata = pd = tc_malloc(sizeof(PrivateData));
    if (!pd) {
        tc_log_error(MOD_NAME, "init: out of memory!");
        return -1;
    }
    pd->framenum = 0;
    pd->enc = NULL;

    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }

    return 0;
}

/*************************************************************************/

/**
 * x264_fini:  Clean up after this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int x264_fini(TCModuleInstance *self)
{
    PrivateData *pd;

    if (!self) {
        return -1;
    }
    pd = self->userdata;

    if (pd->enc) {
        x264_encoder_close(pd->enc);
        pd->enc = NULL;
    }

    tc_free(self->userdata);
    self->userdata = NULL;
    return 0;
}

/*************************************************************************/

/**
 * x264_configure:  Configure this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int x264_configure(TCModuleInstance *self,
                         const char *options, vob_t *vob)
{
    PrivateData *pd;

    if (!self) {
        return -1;
    }
    pd = self->userdata;

    /* Initialize parameter block */
    memset(&confdata, 0, sizeof(confdata));
    x264_param_default(&confdata.x264params);

    /* Read settings from configuration file */
    module_read_config(X264_CONFIG_FILE, NULL, conf, MOD_NAME);

    /* Apply extra settings to $x264params */
    if (0 != x264params_configure(vob->divxlogfile, vob->divxmultipass)) {
        tc_log_error(MOD_NAME, "Failed to apply settings to $x264params.");
        return -1;
    }

    /* Copy parameter block to module private data */
    ac_memcpy(&pd->x264params, &confdata.x264params, sizeof(pd->x264params));

    /* Apply transcode CLI and autodetected values from $vob to
     * $x264params. This is done as the last step to make transcode CLI
     * override any settings done before. */
    if (0 != x264params_set_by_vob(&pd->x264params, vob)) {
        fprintf(stderr, "Failed to evaluate vob_t values.\n");
        return -1;
    }

    /* Test if the set parameters fit together. */
    if (0 != x264params_check(&pd->x264params)) {
        return -1;
    }

    /* Now we've set all parameters gathered from transcode and the config
     * file to $x264params. Let's give some status report and finally open
     * the encoder. */
    if (verbose & TC_DEBUG) {
        module_print_config(conf, MOD_NAME);
    }

    pd->enc = x264_encoder_open(&pd->x264params);
    if (!pd->enc) {
        tc_log_error(MOD_NAME, "x264_encoder_open() returned NULL - sorry.");
        return -1;
    }

    return 0;
}

/*************************************************************************/

/**
 * x264_stop:  Reset this instance of the module.  See tcmodule-data.h for
 * function details.
 */

static int x264_stop(TCModuleInstance *self)
{
    PrivateData *pd;

    if (!self) {
        return -1;
    }
    pd = self->userdata;

    if (pd->enc) {
        x264_encoder_close(pd->enc);
        pd->enc = NULL;
    }

    return 0;
}

/*************************************************************************/

/**
 * x264_inspect:  Return the value of an option in this instance of the
 * module.  See tcmodule-data.h for function details.
 */

static const char *x264_inspect(TCModuleInstance *self,
                               const char *param)
{
    PrivateData *pd;
    static char buf[TC_BUF_MAX];

    if (!self || !param)
       return NULL;
    pd = self->userdata;

    if (optstr_lookup(param, "help")) {
        tc_snprintf(buf, sizeof(buf),
                "Overview:\n"
                "    Encodes video in h.264 format using the x264 library.\n"
                "Options available:\n"
                "    [todo]\n");
        return buf;
    }

    return "";
}

/*************************************************************************/

/**
 * x264_encode_video:  Decode a frame of data.  See tcmodule-data.h for
 * function details.
 */

static int x264_encode_video(TCModuleInstance *self,
                            vframe_list_t *inframe, vframe_list_t *outframe)
{
    PrivateData *pd;
    x264_nal_t *nal;
    int nnal, i;
    x264_picture_t pic, pic_out;

    if (!self) {
        return -1;
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

    // tc_log_msg(MOD_NAME, "starting encoder_encode(frame %d)", framenum);
    if (x264_encoder_encode(pd->enc, &nal, &nnal, &pic, &pic_out) != 0)
        return -1;

    // tc_log_msg("saving %d NAL(s)", nnal);
    /* modified code from x264.c down there (IIRC). */
    outframe->video_len = 0;
    for (i = 0; i < nnal; i++) {
        int size, ret;

        size = outframe->video_size - outframe->video_len;
        ret = x264_nal_encode(outframe->video_buf + outframe->video_len,
                              &size, 1, &nal[i]);
        if (ret < 0) {
            tc_log_warn(MOD_NAME, "output buffer overflow");
            break;
        }
        outframe->video_len += size;
    }

    return 0;
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
