/*
 *  export_ffmpeg.c
 *    based heavily on mplayers ve_lavc.c
 *
 *  Copyright (C) Moritz Bunkus - October 2002
 *    UpToDate by Tilmann Bitterberg - July 2003
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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <time.h>

#include "transcode.h"
#include "avilib.h"
#include "aud_aux.h"
#include "vid_aux.h"
#include "../ffmpeg/libavcodec/avcodec.h"

#if !defined(INFINITY) && defined(HUGE_VAL)
#define INFINITY HUGE_VAL
#endif

#if LIBAVCODEC_BUILD < 4641
#error we dont support libavcodec prior to build 4641, get the latest libavcodec CVS
#endif

#if LIBAVCODEC_BUILD < 4659
#warning your version of libavcodec is old, u might want to get a newer one
#endif

#if LIBAVCODEC_BUILD < 4645
#define AVFrame AVVideoFrame
#define coded_frame coded_picture
#endif

#define MOD_NAME    "export_ffmpeg.so"
#define MOD_VERSION "v0.3.8 (2003-10-11)"
#define MOD_CODEC   "(video) " LIBAVCODEC_IDENT \
                    " | (audio) MPEG/AC3/PCM"
#define MOD_PRE ffmpeg

#include "export_def.h"
#include "ffmpeg_cfg.h"

extern char *tc_config_dir;

// libavcodec is not thread-safe. We must protect concurrent access to it.
// this is visible (without the mutex of course) with 
// transcode .. -x ffmpeg -y ffmpeg -F mpeg4

extern pthread_mutex_t init_avcodec_lock;

static int verbose_flag    = TC_QUIET;
static int capability_flag = TC_CAP_YUV|TC_CAP_RGB|TC_CAP_PCM|TC_CAP_AC3|
                             TC_CAP_AUD|TC_CAP_YUV422;

struct ffmpeg_codec {
  char *name;
  char *fourCC;
  char *comments;
  int   multipass;
};

struct ffmpeg_codec ffmpeg_codecs[] = {
  {"mpeg4", "DIVX", "MPEG4 compliant video", 1},
  {"msmpeg4", "div3", "old DivX3 compatible (aka MSMPEG4v3)", 1},
  {"msmpeg4v2", "MP42", "old DivX3 compatible (older version)", 1},
  {"mjpeg", "MJPG", "Motion JPEG", 0},
  {"mpeg1video", "mpg1", "MPEG1 compliant video", 1},
  {"mpeg1", "mpg1", "MPEG1 compliant video (alias of above)", 1},
  {"mpeg2video", "mpg2", "MPEG2 compliant video", 1},
  {"mpeg2", "mpg2", "MPEG2 compliant video (alias of above)", 1},
  {"h263", "h263", "H263", 0},
  {"h263p", "h263", "H263 plus", 1},
  {"wmv1", "WMV1", "Windows Media Video v1", 1},
  {"wmv2", "WMV2", "Windows Media Video v2", 1},
  {"rv10", "RV10", "old RealVideo codec", 1},
  {"huffyuv", "HFYU", "Lossless HUFFYUV codec", 1},
  {"dvvideo", "DVSD", "Digital Video", 0},
  {NULL, NULL, NULL, 0}};


static uint8_t             *tmp_buffer = NULL;
static AVFrame             *lavc_convert_frame = NULL;

static AVCodec             *lavc_venc_codec = NULL;
static AVFrame             *lavc_venc_frame = NULL;
static AVCodecContext      *lavc_venc_context;
static avi_t               *avifile = NULL;
static int                  pix_fmt;
static FILE                *stats_file = NULL;
static size_t               size;
static int                  encoded_frames = 0;
static int                  frames = 0;
static struct ffmpeg_codec *codec;
static int                  is_mpegvideo = 0;
static int                  is_huffyuv = 0;
static FILE                *mpeg1fd = NULL;

// We can't declare lavc_param_psnr static so save it to this variable
static int                  do_psnr = 0;

// make a planar version
static void uyvyto422p(char *dest, char *input, int width, int height) 
{

    int i,j;
    char *y, *u, *v;

    y = dest;
    v = dest+width*height;
    u = dest+width*height*3/2;
    
    for (i=0; i<height; i++) {
      for (j=0; j<width*2; j++) {
	
	/* UYVY.  The byte order is CbY'CrY' */
	input++;
	//*u++ = *input++;
	*y++ = *input++;
	input++;
	//*v++ = *input++;
	*y++ = *input++;
      }
    }
    return;
}

// Subsample UYVY to YV12/I420
static void uyvytoyv12(char *dest, char *input, int width, int height) 
{

    int i,j,w2;
    char *y, *u, *v;

    w2 = width/2;

    //I420
    y = dest;
    v = dest+width*height;
    u = dest+width*height*5/4;
    
    for (i=0; i<height; i+=2) {
      for (j=0; j<w2; j++) {
	
	/* UYVY.  The byte order is CbY'CrY' */
	*u++ = *input++;
	*y++ = *input++;
	*v++ = *input++;
	*y++ = *input++;
      }

      //down sampling
      u -= w2;
      v -= w2;
      
      /* average every second line for U and V */
      for (j=0; j<w2; j++) {
	  int un = *u & 0xff;
	  int vn = *v & 0xff; 

	  un += *input++ & 0xff;
	  *u++ = un>>1;

	  *y++ = *input++;

	  vn += *input++ & 0xff;
	  *v++ = vn>>1;

	  *y++ = *input++;
      }
    }
}

static struct ffmpeg_codec *find_ffmpeg_codec(char *name) {
  int i;
  
  i = 0;
  while (ffmpeg_codecs[i].name != NULL) {
    if (!strcasecmp(name, ffmpeg_codecs[i].name))
      return &ffmpeg_codecs[i];
    i++;
  }
  
  return NULL;
}

static void strip(char *s) {
  char *start;

  if (s == NULL)
    return;
      
  start = s;
  while ((*start != 0) && isspace(*start))
    start++;
  
  memmove(s, start, strlen(start) + 1);
  if (strlen(s) == 0)
    return;
  
  start = &s[strlen(s) - 1];
  while ((start != s) && isspace(*start)) {
    *start = 0;
    start--;
  }
}

static double psnr(double d){
    if(d==0) return INFINITY;
    return -10.0*log(d)/log(10);
}

/* ------------------------------------------------------------ 
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init {
  char *p;
  int   i;
  
  if (param->flag == TC_VIDEO) {
    // Check if the user used '-F codecname' and abort if not.
    strip(vob->ex_v_fcc);
    if ((vob->ex_v_fcc == NULL) || (strlen(vob->ex_v_fcc) == 0)) {
      fprintf(stderr, "[%s] You must chose a codec by supplying '-F "
              "<codecname>'. A list of supported codecs can be obtained with "
              "'-F list'.\n", MOD_NAME);
      return TC_EXPORT_ERROR;
    }
    if (!strcasecmp(vob->ex_v_fcc, "list")) {
      i = 0;
      fprintf(stderr, "[%s] List of known and supported codecs:\n"
              "[%s] Name       fourCC multipass comments\n"
              "[%s] ---------- ------ --------- ---------------------------"
              "--------\n", MOD_NAME, MOD_NAME, MOD_NAME);
      while (ffmpeg_codecs[i].name != NULL) {
        fprintf(stderr, "[%s] %-10s  %s     %3s    %s\n", MOD_NAME,
                ffmpeg_codecs[i].name, ffmpeg_codecs[i].fourCC, 
                ffmpeg_codecs[i].multipass ? "yes" : "no", 
                ffmpeg_codecs[i].comments);
        i++;
      }
      
      return TC_EXPORT_ERROR;
    }

    if (!strcmp (vob->ex_v_fcc, "mpeg1")) vob->ex_v_fcc = "mpeg1video";
    if (!strcmp (vob->ex_v_fcc, "mpeg2")) vob->ex_v_fcc = "mpeg2video";
    if (!strcmp (vob->ex_v_fcc, "dv")) vob->ex_v_fcc = "dvvideo";
    
    codec = find_ffmpeg_codec(vob->ex_v_fcc);
    if (codec == NULL) {
      fprintf(stderr, "[%s] Unknown codec '%s'.\n", MOD_NAME, vob->ex_v_fcc);
      return TC_EXPORT_ERROR;
    }

    if (!strcmp (vob->ex_v_fcc, "mpeg1video") ||
        !strcmp (vob->ex_v_fcc, "mpeg1")) is_mpegvideo = 1;

    if (!strcmp (vob->ex_v_fcc, "mpeg2video") ||
        !strcmp (vob->ex_v_fcc, "mpeg2")) is_mpegvideo = 2;

    // doesn't work
    //if (!strcmp (vob->ex_v_fcc, "huffyuv")) is_huffyuv = 1;

    pthread_mutex_lock(&init_avcodec_lock);
      avcodec_init();
      avcodec_register_all();
    pthread_mutex_unlock(&init_avcodec_lock);
    
    //-- get it --
    lavc_venc_codec = avcodec_find_encoder_by_name(codec->name);
    if (!lavc_venc_codec) {
      fprintf(stderr, "[%s] Could not find a FFMPEG codec for '%s'.\n",
              MOD_NAME, codec->name);
      return TC_EXPORT_ERROR; 
    }
    fprintf(stderr, "[%s] Using FFMPEG codec '%s' (FourCC '%s', %s).\n",
            MOD_NAME, codec->name, codec->fourCC, codec->comments);

    lavc_venc_context = avcodec_alloc_context();
    lavc_venc_frame   = avcodec_alloc_frame();

    lavc_convert_frame= avcodec_alloc_frame();
    size = avpicture_get_size(PIX_FMT_RGB24, vob->ex_v_width, vob->ex_v_height);
    tmp_buffer = malloc(size);

     
    if (lavc_venc_context == NULL || !tmp_buffer || !lavc_convert_frame) {
      fprintf(stderr, "[%s] Could not allocate enough memory.\n", MOD_NAME);
      return TC_EXPORT_ERROR;
    }
    
    if (vob->im_v_codec == CODEC_YUV)
      pix_fmt = PIX_FMT_YUV420P;
    else if (vob->im_v_codec == CODEC_RGB) {
      pix_fmt = PIX_FMT_RGB24;
    } else if (vob->im_v_codec == CODEC_YUV422) {
      pix_fmt = PIX_FMT_YUV422;
    } else {
      fprintf(stderr, "[%s] Unknown color space %d.\n", MOD_NAME,
              vob->im_v_codec);
      return TC_EXPORT_ERROR;
    }

    lavc_venc_context->width              = vob->ex_v_width;
    lavc_venc_context->height             = vob->ex_v_height;
    lavc_venc_context->bit_rate           = vob->divxbitrate * 1000;
    lavc_venc_context->qmin               = vob->min_quantizer;
    lavc_venc_context->qmax               = vob->max_quantizer;

    switch (vob->ex_frc) {
	case 1: // 23.976
	    lavc_venc_context->frame_rate      = 24000;
	    lavc_venc_context->frame_rate_base = 1001;
	    break;
	case 2: // 24.000
	    lavc_venc_context->frame_rate      = 24000;
	    lavc_venc_context->frame_rate_base = 1000;
	    break;
	case 3: // 25.000
	    lavc_venc_context->frame_rate      = 25000;
	    lavc_venc_context->frame_rate_base = 1000;
	    break;
	case 4: // 29.970
	    lavc_venc_context->frame_rate      = 30000;
	    lavc_venc_context->frame_rate_base = 1001;
	    break;
	case 5: // 30.000
	    lavc_venc_context->frame_rate      = 30000;
	    lavc_venc_context->frame_rate_base = 1000;
	    break;
	case 6: // 50.000
	    lavc_venc_context->frame_rate      = 50000;
	    lavc_venc_context->frame_rate_base = 1000;
	    break;
	case 7: // 59.940
	    lavc_venc_context->frame_rate      = 60000;
	    lavc_venc_context->frame_rate_base = 1001;
	    break;
	case 8: // 60.000
	    lavc_venc_context->frame_rate      = 60000;
	    lavc_venc_context->frame_rate_base = 1000;
	    break;
	case 0: // not set
	default:
	    lavc_venc_context->frame_rate      = (int)(vob->ex_fps*1000.0);
	    lavc_venc_context->frame_rate_base = 1000;
	    break;
    }

    /* keyframe interval */
    if (vob->divxkeyframes >= 0) /* != -1 */
      lavc_venc_context->gop_size = vob->divxkeyframes;
    else
      lavc_venc_context->gop_size = 250; /* default */

    if (is_mpegvideo && vob->divxkeyframes == 250) {
	// set a sensible gop_size
	lavc_venc_context->gop_size = 12;
	fprintf(stderr, "[%s] setting gop_size to 12 for mpeg1/2-video\n", MOD_NAME);
    }

	
    module_read_config(codec->name, MOD_NAME, "ffmpeg", lavcopts_conf, tc_config_dir);
    if (verbose_flag & TC_DEBUG) {
      fprintf(stderr, "[%s] Using the following FFMPEG parameters:\n",
              MOD_NAME);
      module_print_config("", lavcopts_conf);
    }
    
    //if (lavc_param_vhq) lavc_venc_context->flags |= CODEC_FLAG_HQ;

    lavc_venc_context->bit_rate_tolerance = lavc_param_vrate_tolerance * 1000;
    lavc_venc_context->max_qdiff          = lavc_param_vqdiff;
    lavc_venc_context->qcompress          = lavc_param_vqcompress;
    lavc_venc_context->qblur              = lavc_param_vqblur;
    lavc_venc_context->max_b_frames       = lavc_param_vmax_b_frames;
    lavc_venc_context->b_quant_factor     = lavc_param_vb_qfactor;
    lavc_venc_context->rc_strategy        = lavc_param_vrc_strategy;
    lavc_venc_context->b_frame_strategy   = lavc_param_vb_strategy;
    lavc_venc_context->b_quant_offset     = lavc_param_vb_qoffset;
    lavc_venc_context->luma_elim_threshold= lavc_param_luma_elim_threshold;
    lavc_venc_context->chroma_elim_threshold= lavc_param_chroma_elim_threshold;
    lavc_venc_context->rtp_payload_size   = lavc_param_packet_size;
    if (lavc_param_packet_size)
      lavc_venc_context->rtp_mode         = 1;
    lavc_venc_context->strict_std_compliance= lavc_param_strict;
    lavc_venc_context->i_quant_factor     = lavc_param_vi_qfactor;
    lavc_venc_context->i_quant_offset     = lavc_param_vi_qoffset;
    lavc_venc_context->rc_qsquish         = lavc_param_rc_qsquish;
    lavc_venc_context->rc_qmod_amp        = lavc_param_rc_qmod_amp;
    lavc_venc_context->rc_qmod_freq       = lavc_param_rc_qmod_freq;
    lavc_venc_context->rc_eq              = lavc_param_rc_eq;
    lavc_venc_context->rc_max_rate        = lavc_param_rc_max_rate * 1000;
    lavc_venc_context->rc_min_rate        = lavc_param_rc_min_rate * 1000;
    lavc_venc_context->rc_buffer_size     = lavc_param_rc_buffer_size * 1000;
    lavc_venc_context->rc_buffer_aggressivity= lavc_param_rc_buffer_aggressivity;
    lavc_venc_context->rc_initial_cplx    = lavc_param_rc_initial_cplx;
    lavc_venc_context->debug              = lavc_param_debug;
    lavc_venc_context->last_predictor_count= lavc_param_last_pred;
    lavc_venc_context->pre_me             = lavc_param_pre_me;
    lavc_venc_context->me_pre_cmp         = lavc_param_me_pre_cmp;
    lavc_venc_context->pre_dia_size       = lavc_param_pre_dia_size;
    lavc_venc_context->me_subpel_quality  = lavc_param_me_subpel_quality;
    lavc_venc_context->me_range           = lavc_param_me_range;
    lavc_venc_context->intra_quant_bias   = lavc_param_ibias;
    lavc_venc_context->inter_quant_bias   = lavc_param_pbias;
    lavc_venc_context->coder_type         = lavc_param_coder;
    lavc_venc_context->context_model      = lavc_param_context;

    if (lavc_param_intra_matrix)
    {
	char *tmp;

	lavc_venc_context->intra_matrix =
	    malloc(sizeof(*lavc_venc_context->intra_matrix)*64);

	i = 0;
	while ((tmp = strsep(&lavc_param_intra_matrix, ",")) && (i < 64))
	{
	    if (!tmp || (tmp && !strlen(tmp)))
		break;
	    lavc_venc_context->intra_matrix[i++] = atoi(tmp);
	}
	
	if (i != 64)
	{
	    free(lavc_venc_context->intra_matrix);
	    lavc_venc_context->intra_matrix = NULL;
	}
	else
	    fprintf(stderr, "[%s] Using user specified intra matrix\n", MOD_NAME);
    }
    if (lavc_param_inter_matrix)
    {
	char *tmp;

	lavc_venc_context->inter_matrix =
	    malloc(sizeof(*lavc_venc_context->inter_matrix)*64);

	i = 0;
	while ((tmp = strsep(&lavc_param_inter_matrix, ",")) && (i < 64))
	{
	    if (!tmp || (tmp && !strlen(tmp)))
		break;
	    lavc_venc_context->inter_matrix[i++] = atoi(tmp);
	}
	
	if (i != 64)
	{
	    free(lavc_venc_context->inter_matrix);
	    lavc_venc_context->inter_matrix = NULL;
	}
	else
	    fprintf(stderr, "[%s] Using user specified intra matrix\n", MOD_NAME);
    }

    p = lavc_param_rc_override_string;
    for (i = 0; p; i++) {
      int start, end, q;
      int e = sscanf(p, "%d,%d,%d", &start, &end, &q);
      
      if (e != 3) {
        fprintf(stderr, "[%s] Error parsing vrc_override.\n", MOD_NAME);
        return TC_EXPORT_ERROR;
      }
      lavc_venc_context->rc_override =
          realloc(lavc_venc_context->rc_override, sizeof(RcOverride) * (i + 1));
      lavc_venc_context->rc_override[i].start_frame = start;
      lavc_venc_context->rc_override[i].end_frame   = end;
      if (q > 0) {
        lavc_venc_context->rc_override[i].qscale         = q;
        lavc_venc_context->rc_override[i].quality_factor = 1.0;
      }
      else {
        lavc_venc_context->rc_override[i].qscale         = 0;
        lavc_venc_context->rc_override[i].quality_factor = -q / 100.0;
      }
      p = strchr(p, '/');
      if (p)
        p++;
    }
    lavc_venc_context->rc_override_count     = i;
    lavc_venc_context->mpeg_quant            = lavc_param_mpeg_quant;
    lavc_venc_context->dct_algo              = lavc_param_fdct;
    lavc_venc_context->idct_algo             = lavc_param_idct;
    lavc_venc_context->lumi_masking          = lavc_param_lumi_masking;
    lavc_venc_context->temporal_cplx_masking = lavc_param_temporal_cplx_masking;
    lavc_venc_context->spatial_cplx_masking  = lavc_param_spatial_cplx_masking;
    lavc_venc_context->p_masking             = lavc_param_p_masking;
    lavc_venc_context->dark_masking          = lavc_param_dark_masking;

    if (lavc_param_aspect != NULL)
    {
	int par_width, par_height, e;
	float ratio=0;
	e = sscanf (lavc_param_aspect, "%d/%d", &par_width, &par_height);
	if(e==2) {
            if(par_height)
                ratio= (float)par_width / (float)par_height;
        } else {
	    e= sscanf (lavc_param_aspect, "%f", &ratio);
	}

	if (e && ratio > 0.1 && ratio < 10.0) {
	    lavc_venc_context->aspect_ratio= ratio;
	} else {
	    fprintf(stderr, "[%s] Unsupported aspect ration %s.\n", MOD_NAME,
		    lavc_param_aspect);
	    return TC_EXPORT_ERROR; 
	}
    }
    else if (lavc_param_autoaspect)
	lavc_venc_context->aspect_ratio = (float)vob->ex_v_width/vob->ex_v_height;

    lavc_venc_context->me_cmp     = lavc_param_me_cmp;
    lavc_venc_context->me_sub_cmp = lavc_param_me_sub_cmp;
    lavc_venc_context->mb_cmp     = lavc_param_mb_cmp;
    lavc_venc_context->dia_size   = lavc_param_dia_size;
    lavc_venc_context->flags |= lavc_param_qpel;
    lavc_venc_context->flags |= lavc_param_trell;
    lavc_venc_context->flags |= lavc_param_aic;
    lavc_venc_context->flags |= lavc_param_umv;
    lavc_venc_context->flags |= lavc_param_v4mv ? CODEC_FLAG_4MV : 0;
    lavc_venc_context->flags |= lavc_param_data_partitioning;
    lavc_venc_context->flags |= lavc_param_cbp;
    lavc_venc_context->flags |= lavc_param_mv0;

    if (lavc_param_gray)
      lavc_venc_context->flags |= CODEC_FLAG_GRAY;
    if (lavc_param_normalize_aqp)
      lavc_venc_context->flags |= CODEC_FLAG_NORMALIZE_AQP;
    if (lavc_param_interlaced_dct)
      lavc_venc_context->flags |= CODEC_FLAG_INTERLACED_DCT;

    lavc_venc_context->flags|= lavc_param_psnr;
    do_psnr = lavc_param_psnr;

    lavc_venc_context->prediction_method= lavc_param_prediction_method;

    // this changed to an int
    if(!strcasecmp(lavc_param_format, "YV12"))
        lavc_venc_context->pix_fmt= PIX_FMT_YUV420P;
    else if(!strcasecmp(lavc_param_format, "422P"))
        lavc_venc_context->pix_fmt= PIX_FMT_YUV422P;
    else if(!strcasecmp(lavc_param_format, "444P"))
        lavc_venc_context->pix_fmt= PIX_FMT_YUV444P;
    else if(!strcasecmp(lavc_param_format, "411P"))
        lavc_venc_context->pix_fmt= PIX_FMT_YUV411P;
    else if(!strcasecmp(lavc_param_format, "YVU9"))
        lavc_venc_context->pix_fmt= PIX_FMT_YUV410P;
    else if(!strcasecmp(lavc_param_format, "UYVY"))
        lavc_venc_context->pix_fmt= PIX_FMT_YUV422;
    else if(!strcasecmp(lavc_param_format, "BGR32"))
        lavc_venc_context->pix_fmt= PIX_FMT_RGBA32;
    else{
        fprintf(stderr, "%s is not a supported format\n", lavc_param_format);
        return TC_IMPORT_ERROR;
    }

    if (is_huffyuv) {
        lavc_venc_context->pix_fmt= PIX_FMT_YUV422P;
    }

    switch (vob->divxmultipass) {
      case 1:
        if (!codec->multipass) {
          fprintf(stderr, "[%s] This codec does not support multipass "
                  "encoding.\n", MOD_NAME);
          return TC_EXPORT_ERROR;
        }
        lavc_venc_context->flags |= CODEC_FLAG_PASS1; 
        stats_file = fopen(vob->divxlogfile, "w");
        if (stats_file == NULL){
          fprintf(stderr, "[%s] Could not create 2pass log file \"%s\".\n",
                  MOD_NAME, vob->divxlogfile);
          return TC_EXPORT_ERROR;
        }
        break;
      case 2:
        if (!codec->multipass) {
          fprintf(stderr, "[%s] This codec does not support multipass "
                  "encoding.\n", MOD_NAME);
          return TC_EXPORT_ERROR;
        }
        lavc_venc_context->flags |= CODEC_FLAG_PASS2; 
        stats_file= fopen(vob->divxlogfile, "r");
        if (stats_file==NULL){
          fprintf(stderr, "[%s] Could not open 2pass log file \"%s\" for "
                  "reading.\n", MOD_NAME, vob->divxlogfile);
          return TC_EXPORT_ERROR;
        }
        fseek(stats_file, 0, SEEK_END);
        size = ftell(stats_file);
        fseek(stats_file, 0, SEEK_SET);

	// count the lines of the file to not encode to much
	{ 
	    char lbuf[255];
	    while (fgets (lbuf, 255, stats_file))
		encoded_frames++;
	}
	
        fseek(stats_file, 0, SEEK_SET);

        lavc_venc_context->stats_in= malloc(size + 1);
        lavc_venc_context->stats_in[size] = 0;

        if (fread(lavc_venc_context->stats_in, size, 1, stats_file) < 1){
          fprintf(stderr, "[%s] Could not read the complete 2pass log file "
                  "\"%s\".\n", MOD_NAME, vob->divxlogfile);
          return TC_EXPORT_ERROR;
        }        
        break;
      case 3:
        /* fixed qscale :p */
        lavc_venc_context->flags   |= CODEC_FLAG_QSCALE;
        lavc_venc_frame->quality  = vob->min_quantizer;
        break;
    }

    lavc_venc_context->me_method = ME_ZERO + lavc_param_vme;


    //-- open codec --
    //----------------
    if (avcodec_open(lavc_venc_context, lavc_venc_codec) < 0) {
      fprintf(stderr, "[%s] could not open FFMPEG/MPEG4 codec\n", MOD_NAME);
      return TC_EXPORT_ERROR; 
    }

    if (lavc_venc_context->codec->encode == NULL) {
      fprintf(stderr, "[%s] could not open FFMPEG/MPEG4 codec "
              "(lavc_venc_context->codec->encode == NULL)\n", MOD_NAME);
      return TC_EXPORT_ERROR; 
    }
    
    /* free second pass buffer, its not needed anymore */
    if (lavc_venc_context->stats_in)
      free(lavc_venc_context->stats_in);
    lavc_venc_context->stats_in = NULL;
    
    if (verbose_flag & TC_DEBUG) {
     //-- GMO start -- 
      if (vob->divxmultipass == 3) { 
        fprintf(stderr, "[%s]    single-pass session: 3 (VBR)\n", MOD_NAME);
        fprintf(stderr, "[%s]          VBR-quantizer: %d\n", MOD_NAME,
                vob->divxbitrate);
      } else {
        fprintf(stderr, "[%s]     multi-pass session: %d\n", MOD_NAME,
                vob->divxmultipass);
        fprintf(stderr, "[%s]      bitrate [kBits/s]: %d\n", MOD_NAME,
                lavc_venc_context->bit_rate/1000);
      }
  
      //-- GMO end --

      fprintf(stderr, "[%s]              crispness: %d\n", MOD_NAME,
              vob->divxcrispness);
      fprintf(stderr, "[%s]  max keyframe interval: %d\n", MOD_NAME,
              vob->divxkeyframes);
      fprintf(stderr, "[%s]             frame rate: %.2f\n", MOD_NAME,
              vob->ex_fps);
      fprintf(stderr, "[%s]            color space: %s\n", MOD_NAME,
              (vob->im_v_codec==CODEC_RGB) ? "RGB24":"YV12");
      fprintf(stderr, "[%s]             quantizers: %d/%d\n", MOD_NAME,
              lavc_venc_context->qmin, lavc_venc_context->qmax);
    }

    return 0;
  }

  if (param->flag == TC_AUDIO)
    return audio_init(vob, verbose_flag);
  
  // invalid flag
  return TC_EXPORT_ERROR;
}

/* ------------------------------------------------------------ 
 *
 * open outputfile
 *
 * ------------------------------------------------------------*/

MOD_open
{

  // open output file
  
  /* Open file */
  if ( (param->flag == TC_VIDEO && !is_mpegvideo) || (param->flag == TC_AUDIO)) {
    if (vob->avifile_out==NULL) {

      vob->avifile_out = AVI_open_output_file(vob->video_out_file);

      if ((vob->avifile_out) == NULL) {
	AVI_print_error("avi open error");
	return TC_EXPORT_ERROR;
      }

    }
  }
    
  /* Save locally */
  avifile = vob->avifile_out;

  
  if (param->flag == TC_VIDEO) {
    
    // video
    if (is_mpegvideo) {
      char *buf = malloc (strlen (vob->video_out_file)+1+4);

      if (is_mpegvideo==1 && strcmp(vob->video_out_file, "/dev/null") != 0) {
	  sprintf(buf, "%s.m1v", vob->video_out_file);
	  mpeg1fd = fopen ( buf, "wb" );
      } else if (is_mpegvideo==2 && strcmp(vob->video_out_file, "/dev/null") != 0) {
	  sprintf(buf, "%s.m2v", vob->video_out_file);
	  mpeg1fd = fopen ( buf, "wb" );
      } else {
	  mpeg1fd = fopen ( vob->video_out_file, "wb" );
      }

      if (!mpeg1fd) {
	fprintf(stderr, "Can not open |%s|\n", buf); 
	return TC_EXPORT_ERROR;
      }
      free (buf);

    } else {
      AVI_set_video(avifile, vob->ex_v_width, vob->ex_v_height, vob->ex_fps,
                    codec->fourCC);

      if (vob->avi_comment_fd>0)
	  AVI_set_comment_fd(vob->avifile_out, vob->avi_comment_fd);
    }
    
    return 0;
  }
  
  
  if (param->flag == TC_AUDIO)
    return audio_open(vob, vob->avifile_out);
  
  // invalid flag
  return TC_EXPORT_ERROR;
}   

/* ------------------------------------------------------------ 
 *
 * encode and export
 *
 * ------------------------------------------------------------*/

MOD_encode
{
  
  int out_size;
  int buf_size=SIZE_RGB_FRAME;
  const char pict_type_char[5]= {'?', 'I', 'P', 'B', 'S'};
  
  if (param->flag == TC_VIDEO) { 

    ++frames;

    if (encoded_frames && frames > encoded_frames)
	return TC_EXPORT_ERROR;

    if (pix_fmt == PIX_FMT_YUV420P) {
      lavc_venc_context->pix_fmt     = PIX_FMT_YUV420P;
      
#if 0
      avpicture_fill((AVPicture *)lavc_venc_frame, param->buffer,
	  PIX_FMT_YUV420P, lavc_venc_context->width, lavc_venc_context->height);

#else
      lavc_venc_frame->linesize[0] = lavc_venc_context->width;     
      lavc_venc_frame->linesize[1] = lavc_venc_context->width / 2;
      lavc_venc_frame->linesize[2] = lavc_venc_context->width / 2;

      lavc_venc_frame->data[0]     = param->buffer;
      lavc_venc_frame->data[2]     = param->buffer +
        lavc_venc_context->width * lavc_venc_context->height;
      lavc_venc_frame->data[1]     = param->buffer +
        (lavc_venc_context->width * lavc_venc_context->height*5)/4;
#endif
    } else if (pix_fmt == PIX_FMT_YUV422) {

      if (is_huffyuv) {

	lavc_venc_context->pix_fmt     = PIX_FMT_YUV422P;
	uyvyto422p(tmp_buffer, param->buffer, lavc_venc_context->width, lavc_venc_context->height);

	avpicture_fill((AVPicture *)lavc_venc_frame, tmp_buffer,
	    PIX_FMT_YUV422P, lavc_venc_context->width, lavc_venc_context->height);

	printf ("%d %d %d %p %p %p\n", 
		lavc_venc_frame->linesize[0], 
		lavc_venc_frame->linesize[1], 
		lavc_venc_frame->linesize[2],
		lavc_venc_frame->data[0],
		lavc_venc_frame->data[1],
		lavc_venc_frame->data[2]
		);

      } else {

	lavc_venc_context->pix_fmt     = PIX_FMT_YUV420P;
	uyvytoyv12(tmp_buffer, param->buffer, lavc_venc_context->width, lavc_venc_context->height);

	lavc_venc_frame->linesize[0] = lavc_venc_context->width;     
	lavc_venc_frame->linesize[1] = lavc_venc_context->width / 2;
	lavc_venc_frame->linesize[2] = lavc_venc_context->width / 2;

	lavc_venc_frame->data[0]     = tmp_buffer;
	lavc_venc_frame->data[2]     = tmp_buffer +
	  lavc_venc_context->width * lavc_venc_context->height;
	lavc_venc_frame->data[1]     = tmp_buffer +
	  (lavc_venc_context->width * lavc_venc_context->height*5)/4;
      }



    } else if (pix_fmt == PIX_FMT_RGB24) {

      avpicture_fill((AVPicture *)lavc_convert_frame, param->buffer,
	  PIX_FMT_RGB24, lavc_venc_context->width, lavc_venc_context->height);

      avpicture_fill((AVPicture *)lavc_venc_frame, tmp_buffer,
	  PIX_FMT_YUV420P, lavc_venc_context->width, lavc_venc_context->height);

      lavc_venc_context->pix_fmt     = PIX_FMT_YUV420P;
      img_convert((AVPicture *)lavc_venc_frame, PIX_FMT_YUV420P,
	          (AVPicture *)lavc_convert_frame, PIX_FMT_RGB24, 
		  lavc_venc_context->width, lavc_venc_context->height);

    } else {
      fprintf(stderr, "[%s] Unknown pixel format %d.\n", MOD_NAME,
              lavc_venc_context->pix_fmt);
      return TC_EXPORT_ERROR;
    }


    pthread_mutex_lock(&init_avcodec_lock);
    out_size = avcodec_encode_video(lavc_venc_context,
                                    (unsigned char *) tmp_buffer, buf_size,
                                    lavc_venc_frame);
    pthread_mutex_unlock(&init_avcodec_lock);
  
    if (out_size < 0) {
      fprintf(stderr, "[%s] encoder error: size (%d)\n", MOD_NAME, out_size);
      return TC_EXPORT_ERROR; 
    }
    if (verbose & TC_STATS) {
      fprintf(stderr, "[%s] encoder: size of encoded (%d)\n", MOD_NAME, out_size);
    }

    //0.6.2: switch outfile on "r/R" and -J pv
    //0.6.2: enforce auto-split at 2G (or user value) for normal AVI files
    if (!is_mpegvideo) {
      if((uint32_t)(AVI_bytes_written(avifile)+out_size+16+8)>>20 >= tc_avi_limit) tc_outstream_rotate_request();
    
      if (lavc_venc_context->coded_frame->key_frame) tc_outstream_rotate();
    
      if (AVI_write_frame(avifile, tmp_buffer, out_size,
                       lavc_venc_context->coded_frame->key_frame? 1 : 0) < 0) {
	AVI_print_error("avi video write error");
      
	return TC_EXPORT_ERROR; 
      }
    } else { // mpegvideo
      if ( (out_size >0) && (fwrite (tmp_buffer, out_size, 1, mpeg1fd) <= 0) ) {
	fprintf(stderr, "[%s] encoder error write failed size (%d)\n", MOD_NAME, out_size);
	//return TC_EXPORT_ERROR; 
      }
    }

    /* store psnr / pict size / type / qscale */
    if(do_psnr){
        static FILE *fvstats=NULL;
        char filename[20];
        double f= lavc_venc_context->width*lavc_venc_context->height*255.0*255.0;

        if(!fvstats) {
            time_t today2;
            struct tm *today;
            today2 = time(NULL);
            today = localtime(&today2);
            sprintf(filename, "psnr_%02d%02d%02d.log", today->tm_hour,
                today->tm_min, today->tm_sec);
            fvstats = fopen(filename,"w");
            if(!fvstats) {
                perror("fopen");
                lavc_param_psnr=0; // disable block
		do_psnr = 0;
                /*exit(1);*/
            }
        }

        fprintf(fvstats, "%6d, %2.2f, %6d, %2.2f, %2.2f, %2.2f, %2.2f %c\n",
            lavc_venc_context->coded_frame->coded_picture_number,
            lavc_venc_context->coded_frame->quality,
            out_size,
            psnr(lavc_venc_context->coded_frame->error[0]/f),
            psnr(lavc_venc_context->coded_frame->error[1]*4/f),
            psnr(lavc_venc_context->coded_frame->error[2]*4/f),
            psnr((lavc_venc_context->coded_frame->error[0]+lavc_venc_context->coded_frame->error[1]+lavc_venc_context->coded_frame->error[2])/(f*1.5)),
            pict_type_char[lavc_venc_context->coded_frame->pict_type]
            );
    }
    
    /* store stats if there are any */
    if (lavc_venc_context->stats_out && stats_file) 
      fprintf(stats_file, "%s", lavc_venc_context->stats_out);

    return 0;
  }
  
  if (param->flag == TC_AUDIO)
    return audio_encode(param->buffer, param->size, avifile);
  
  // invalid flag
  return TC_EXPORT_ERROR;
}

/* ------------------------------------------------------------ 
 *
 * stop encoder
 *
 * ------------------------------------------------------------*/

MOD_stop 
{
  
  if (param->flag == TC_VIDEO) {

    if(do_psnr){
        double f= lavc_venc_context->width*lavc_venc_context->height*255.0*255.0;
        
        f*= lavc_venc_context->coded_frame->coded_picture_number;
        
        fprintf(stderr, "PSNR: Y:%2.2f, Cb:%2.2f, Cr:%2.2f, All:%2.2f\n",
            psnr(lavc_venc_context->error[0]/f),
            psnr(lavc_venc_context->error[1]*4/f),
            psnr(lavc_venc_context->error[2]*4/f),
            psnr((lavc_venc_context->error[0]+lavc_venc_context->error[1]+lavc_venc_context->error[2])/(f*1.5))
            );
    }

    if (lavc_venc_frame) {
      free(lavc_venc_frame);
      lavc_venc_frame = NULL;
    }

    //-- release encoder --
    if (lavc_venc_codec) {
      avcodec_close(lavc_venc_context);
      lavc_venc_codec = NULL;
    }

    if (stats_file) {
      fclose(stats_file);
      stats_file = NULL;
    }
    
    if (lavc_venc_context != NULL) {    
      if (lavc_venc_context->rc_override) {
        free(lavc_venc_context->rc_override);
        lavc_venc_context->rc_override = NULL;
      }
      free(lavc_venc_context);
      lavc_venc_context = NULL;
    }
    return 0;
  }
  
  if (param->flag == TC_AUDIO)
    return audio_stop();
  
  return TC_EXPORT_ERROR;
}

/* ------------------------------------------------------------ 
 *
 * close outputfiles
 *
 * ------------------------------------------------------------*/

MOD_close
{  

  vob_t *vob = tc_get_vob();

  if (param->flag == TC_AUDIO)
    return audio_close();

  if (vob->avifile_out!=NULL) {
    AVI_close(vob->avifile_out);
    vob->avifile_out=NULL;
    return 0;
  }
  
  if (is_mpegvideo) {
    if (mpeg1fd) {
      fclose (mpeg1fd);
      mpeg1fd = NULL;
      return 0;
    }
  }

  return TC_EXPORT_ERROR;
  
}
