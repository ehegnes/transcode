/* ------------------------------------------------------------ 
 *
 * read ffmpeg configuration parameters from a file
 *
 * ------------------------------------------------------------*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#include "../libioaux/configs.h"

#include "ffmpeg_cfg.h"

/*
 * Default values as taken from MPlayer's libmpcodecs/ve_lavc.c
 */

/* video options */
//char *lavc_param_vcodec = "mpeg4";
//int lavc_param_vbitrate = -1;
int lavc_param_vrate_tolerance = 1000*8;
int lavc_param_mb_decision = 1; /* default is NOT realtime encoding */
int lavc_param_v4mv = 0;
int lavc_param_vme = 4;
//int lavc_param_vqscale = 0;
//int lavc_param_vqmin = 2;
//int lavc_param_vqmax = 31;
int lavc_param_mb_qmin = 2;
int lavc_param_mb_qmax = 31;
int lavc_param_lmin = 2;
int lavc_param_lmax = 31;
int lavc_param_vqdiff = 3;
float lavc_param_vqcompress = 0.5;
float lavc_param_vqblur = 0.5;
float lavc_param_vb_qfactor = 1.25;
float lavc_param_vb_qoffset = 1.25;
float lavc_param_vi_qfactor = 0.8;
float lavc_param_vi_qoffset = 0.0;
int lavc_param_vmax_b_frames = 0;
//int lavc_param_keyint = -1;
//int lavc_param_vpass = 0;
int lavc_param_vrc_strategy = 2;
int lavc_param_vb_strategy = 0;
int lavc_param_luma_elim_threshold = 0;
int lavc_param_chroma_elim_threshold = 0;
int lavc_param_packet_size= 0;
int lavc_param_strict= 0;
int lavc_param_data_partitioning= 0;
int lavc_param_gray=0;
float lavc_param_rc_qsquish=1.0;
float lavc_param_rc_qmod_amp=0;
int lavc_param_rc_qmod_freq=0;
char *lavc_param_rc_override_string=NULL;
char *lavc_param_rc_eq="tex^qComp";
int lavc_param_rc_buffer_size=0;
float lavc_param_rc_buffer_aggressivity=1.0;
int lavc_param_rc_max_rate=0;
int lavc_param_rc_min_rate=0;
float lavc_param_rc_initial_cplx=0.0;
int lavc_param_mpeg_quant=0;
int lavc_param_fdct=0;
int lavc_param_idct=0;
char* lavc_param_aspect=NULL;
int lavc_param_autoaspect=1; // FLAG
float lavc_param_lumi_masking= 0.0;
float lavc_param_dark_masking= 0.0;
float lavc_param_temporal_cplx_masking= 0.0;
float lavc_param_spatial_cplx_masking= 0.0;
float lavc_param_p_masking= 0.0;
int lavc_param_normalize_aqp= 0;
//int lavc_param_interlaced_dct= 0;
int lavc_param_prediction_method= FF_PRED_LEFT;
char *lavc_param_format="YV12";
int lavc_param_debug= 0;
int lavc_param_psnr= 0;
int lavc_param_me_pre_cmp= 0;
int lavc_param_me_cmp= 0;
int lavc_param_me_sub_cmp= 0;
int lavc_param_mb_cmp= 0;
int lavc_param_pre_dia_size= 0;
int lavc_param_dia_size= 0;
int lavc_param_qpel= 0;
int lavc_param_trell= 0;
int lavc_param_aic=0;
int lavc_param_umv=0;
int lavc_param_last_pred= 0;
int lavc_param_pre_me= 1;
int lavc_param_me_subpel_quality= 8;
int lavc_param_me_range=0;
int lavc_param_ibias=FF_DEFAULT_QUANT_BIAS;
int lavc_param_pbias=FF_DEFAULT_QUANT_BIAS;
int lavc_param_coder=0;
int lavc_param_context=0;
char *lavc_param_intra_matrix = NULL;
char *lavc_param_inter_matrix = NULL;
int lavc_param_cbp= 0;
int lavc_param_mv0= 0;
int lavc_param_noise_reduction= 0;
int lavc_param_qp_rd= 0;

//char *lavc_param_acodec = "mp2";
//int lavc_param_atag = 0;
//int lavc_param_abitrate = 224;




struct config lavcopts_conf[]={
//    {"acodec", &lavc_param_acodec, CONF_TYPE_STRING, 0, 0, 0, NULL},
//    {"abitrate", &lavc_param_abitrate, CONF_TYPE_INT, CONF_RANGE, 1, 1000, NULL},
//    {"atag", &lavc_param_atag, CONF_TYPE_INT, CONF_RANGE, 0, 0xffff, NULL},
//    {"vcodec", &lavc_param_vcodec, CONF_TYPE_STRING, 0, 0, 0, NULL},
//    {"vbitrate", &lavc_param_vbitrate, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
    {"vratetol", &lavc_param_vrate_tolerance, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
    {"vhq", &lavc_param_mb_decision, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"mbd", &lavc_param_mb_decision, CONF_TYPE_INT, CONF_RANGE, 0, 9, NULL},
    {"v4mv", &lavc_param_v4mv, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"vme", &lavc_param_vme, CONF_TYPE_INT, CONF_RANGE, 0, 5, NULL},
//    {"vqscale", &lavc_param_vqscale, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
//    {"vqmin", &lavc_param_vqmin, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
//    {"vqmax", &lavc_param_vqmax, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
    {"mbqmin", &lavc_param_mb_qmin, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
    {"mbqmax", &lavc_param_mb_qmax, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
    {"lmin", &lavc_param_lmin, CONF_TYPE_FLOAT, CONF_RANGE, 0.01, 255.0, NULL},
    {"lmax", &lavc_param_lmax, CONF_TYPE_FLOAT, CONF_RANGE, 0.01, 255.0, NULL},
    {"vqdiff", &lavc_param_vqdiff, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
    {"vqcomp", &lavc_param_vqcompress, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 1.0, NULL},
    {"vqblur", &lavc_param_vqblur, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 1.0, NULL},
    {"vb_qfactor", &lavc_param_vb_qfactor, CONF_TYPE_FLOAT, CONF_RANGE, -31.0, 31.0, NULL},
    {"vmax_b_frames", &lavc_param_vmax_b_frames, CONF_TYPE_INT, CONF_RANGE, 0, FF_MAX_B_FRAMES, NULL},
//    {"vpass", &lavc_param_vpass, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
    {"vrc_strategy", &lavc_param_vrc_strategy, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
    {"vb_strategy", &lavc_param_vb_strategy, CONF_TYPE_INT, CONF_RANGE, 0, 10, NULL},
    {"vb_qoffset", &lavc_param_vb_qoffset, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 31.0, NULL},
    {"vlelim", &lavc_param_luma_elim_threshold, CONF_TYPE_INT, CONF_RANGE, -99, 99, NULL},
    {"vcelim", &lavc_param_chroma_elim_threshold, CONF_TYPE_INT, CONF_RANGE, -99, 99, NULL},
    {"vpsize", &lavc_param_packet_size, CONF_TYPE_INT, CONF_RANGE, 0, 100000000, NULL},
    {"vstrict", &lavc_param_strict, CONF_TYPE_INT, CONF_RANGE, -99, 99, NULL},
    {"vdpart", &lavc_param_data_partitioning, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PART, NULL},
//    {"keyint", &lavc_param_keyint, CONF_TYPE_INT, 0, 0, 0, NULL},
    {"gray", &lavc_param_gray, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PART, NULL},
    {"mpeg_quant", &lavc_param_mpeg_quant, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"vi_qfactor", &lavc_param_vi_qfactor, CONF_TYPE_FLOAT, CONF_RANGE, -31.0, 31.0, NULL},
    {"vi_qoffset", &lavc_param_vi_qoffset, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 31.0, NULL},
    {"vqsquish", &lavc_param_rc_qsquish, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 99.0, NULL},
    {"vqmod_amp", &lavc_param_rc_qmod_amp, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 99.0, NULL},
    {"vqmod_freq", &lavc_param_rc_qmod_freq, CONF_TYPE_INT, 0, 0, 0, NULL},
    {"vrc_eq", &lavc_param_rc_eq, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"vrc_override", &lavc_param_rc_override_string, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"vrc_maxrate", &lavc_param_rc_max_rate, CONF_TYPE_INT, CONF_RANGE, 0, 24000000, NULL},
    {"vrc_minrate", &lavc_param_rc_min_rate, CONF_TYPE_INT, CONF_RANGE, 0, 24000000, NULL},
    {"vrc_buf_size", &lavc_param_rc_buffer_size, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
    {"vrc_buf_aggressivity", &lavc_param_rc_buffer_aggressivity, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 99.0, NULL},
    {"vrc_init_cplx", &lavc_param_rc_initial_cplx, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 9999999.0, NULL},
    {"vfdct", &lavc_param_fdct, CONF_TYPE_INT, CONF_RANGE, 0, 10, NULL},
    {"aspect", &lavc_param_aspect, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"autoaspect", &lavc_param_autoaspect, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"lumi_mask", &lavc_param_lumi_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
    {"tcplx_mask", &lavc_param_temporal_cplx_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
    {"scplx_mask", &lavc_param_spatial_cplx_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
    {"p_mask", &lavc_param_p_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
    {"naq", &lavc_param_normalize_aqp, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"dark_mask", &lavc_param_dark_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
    //{"ildct", &lavc_param_interlaced_dct, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"idct", &lavc_param_idct, CONF_TYPE_INT, CONF_RANGE, 0, 20, NULL},
    {"pred", &lavc_param_prediction_method, CONF_TYPE_INT, CONF_RANGE, 0, 20, NULL},
    {"format", &lavc_param_format, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"debug", &lavc_param_debug, CONF_TYPE_INT, CONF_RANGE, 0, 100000000, NULL},
    {"psnr", &lavc_param_psnr, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PSNR, NULL},
    {"precmp", &lavc_param_me_pre_cmp, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
    {"cmp", &lavc_param_me_cmp, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
    {"subcmp", &lavc_param_me_sub_cmp, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
    {"mbcmp", &lavc_param_mb_cmp, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
    {"predia", &lavc_param_pre_dia_size, CONF_TYPE_INT, CONF_RANGE, -2000, 2000, NULL},
    {"dia", &lavc_param_dia_size, CONF_TYPE_INT, CONF_RANGE, -2000, 2000, NULL},
    {"qpel", &lavc_param_qpel, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_QPEL, NULL},
    {"trell", &lavc_param_trell, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_TRELLIS_QUANT, NULL},
    {"last_pred", &lavc_param_last_pred, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
    {"preme", &lavc_param_pre_me, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
    {"subq", &lavc_param_me_subpel_quality, CONF_TYPE_INT, CONF_RANGE, 0, 8, NULL},
    {"me_range", &lavc_param_me_range, CONF_TYPE_INT, CONF_RANGE, 0, 16000, NULL},
    {"aic", &lavc_param_aic, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_H263P_AIC, NULL},
    {"umv", &lavc_param_umv, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_H263P_UMV, NULL},
    {"ibias", &lavc_param_ibias, CONF_TYPE_INT, CONF_RANGE, -512, 512, NULL},
    {"pbias", &lavc_param_pbias, CONF_TYPE_INT, CONF_RANGE, -512, 512, NULL},
    {"coder", &lavc_param_coder, CONF_TYPE_INT, CONF_RANGE, 0, 10, NULL},
    {"context", &lavc_param_context, CONF_TYPE_INT, CONF_RANGE, 0, 10, NULL},
    {"intra_matrix", &lavc_param_intra_matrix, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"inter_matrix", &lavc_param_inter_matrix, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"cbp", &lavc_param_cbp, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_CBP_RD, NULL},
    {"mv0", &lavc_param_mv0, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_MV0, NULL},
    {"nr", &lavc_param_noise_reduction, CONF_TYPE_INT, CONF_RANGE, 0, 1000000, NULL},
    {"qprd", &lavc_param_qp_rd, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_QP_RD, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

