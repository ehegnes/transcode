#ifndef __FFMPEG_CFG_H
#define __FFMPEG_CFG_H

#include "../libioaux/configs.h"
#include "../ffmpeg/libavcodec/avcodec.h"

#define FFMPEG_CONFIG_FILE "ffmpeg.cfg"

#define CONF_TYPE_FLAG		0
#define CONF_TYPE_INT		1
#define CONF_TYPE_FLOAT		2
#define CONF_TYPE_STRING	3

#define CONF_MIN		(1<<0)
#define CONF_MAX		(1<<1)
#define CONF_RANGE		(CONF_MIN|CONF_MAX)

struct config {
  char *name;
  void *p;
  unsigned int type, flags;
  float min, max;
  void *dummy;
};

//extern int lavc_param_vbitrate;
extern int lavc_param_vrate_tolerance;
extern int lavc_param_vhq; /* default is NOT realtime encoding! */
extern int lavc_param_v4mv;
extern int lavc_param_vme;
//extern int lavc_param_vqscale;
//extern int lavc_param_vqmin;
//extern int lavc_param_vqmax;
extern int lavc_param_vqdiff;
extern float lavc_param_vqcompress;
extern float lavc_param_vqblur;
extern float lavc_param_vb_qfactor;
extern float lavc_param_vb_qoffset;
extern float lavc_param_vi_qfactor;
extern float lavc_param_vi_qoffset;
extern int lavc_param_vmax_b_frames;
//extern int lavc_param_keyint;
//extern int lavc_param_vpass;
extern int lavc_param_vrc_strategy;
extern int lavc_param_vb_strategy;
extern int lavc_param_luma_elim_threshold;
extern int lavc_param_chroma_elim_threshold;
extern int lavc_param_packet_size;
extern int lavc_param_strict;
extern int lavc_param_data_partitioning;
extern int lavc_param_gray;
extern float lavc_param_rc_qsquish;
extern float lavc_param_rc_qmod_amp;
extern int lavc_param_rc_qmod_freq;
extern char *lavc_param_rc_override_string;
extern char *lavc_param_rc_eq;
extern int lavc_param_rc_buffer_size;
extern float lavc_param_rc_buffer_aggressivity;
extern int lavc_param_rc_max_rate;
extern int lavc_param_rc_min_rate;
extern float lavc_param_rc_initial_cplx;
extern int lavc_param_mpeg_quant;
extern int lavc_param_fdct;
extern int lavc_param_idct;
extern float lavc_param_aspect;
extern float lavc_param_lumi_masking;
extern float lavc_param_dark_masking;
extern float lavc_param_temporal_cplx_masking;
extern float lavc_param_spatial_cplx_masking;
extern float lavc_param_p_masking;
extern int lavc_param_normalize_aqp;
extern int lavc_param_interlaced_dct;

extern struct config lavcopts_conf[];

int ffmpeg_read_config(char *section, char *prefix, struct config *conf);
int ffmpeg_read_values(CF_ROOT_TYPE *p_root, CF_SECTION_TYPE *p_section,
                       char *prefix, struct config *conf);
int ffmpeg_print_config(char *prefix, struct config *conf);

#endif
