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

//int lavc_param_vbitrate = -1;
int lavc_param_vrate_tolerance = 1000*8;
int lavc_param_vhq = 1; /* default is NOT realtime encoding! */
int lavc_param_v4mv = 0;
int lavc_param_vme = 4;
//int lavc_param_vqscale = 0;
//int lavc_param_vqmin = 2;
//int lavc_param_vqmax = 31;
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
float lavc_param_rc_initial_cplx=0;
int lavc_param_mpeg_quant=0;
int lavc_param_fdct=0;
int lavc_param_idct=0;
float lavc_param_aspect=0.0;
float lavc_param_lumi_masking= 0.0;
float lavc_param_dark_masking= 0.0;
float lavc_param_temporal_cplx_masking= 0.0;
float lavc_param_spatial_cplx_masking= 0.0;
float lavc_param_p_masking= 0.0;
int lavc_param_normalize_aqp= 0;
int lavc_param_interlaced_dct= 0;

struct config lavcopts_conf[]={
//  {"vcodec", &lavc_param_vcodec, CONF_TYPE_STRING, 0, 0, 0, NULL},
//  {"vbitrate", &lavc_param_vbitrate, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
  {"vratetol", &lavc_param_vrate_tolerance, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
  {"vhq", &lavc_param_vhq, CONF_TYPE_FLAG, 0, 0, 1, NULL},
  {"v4mv", &lavc_param_v4mv, CONF_TYPE_FLAG, 0, 0, 1, NULL},
  {"vme", &lavc_param_vme, CONF_TYPE_INT, CONF_RANGE, 0, 5, NULL},
//  {"vqscale", &lavc_param_vqscale, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
//  {"vqmin", &lavc_param_vqmin, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
//  {"vqmax", &lavc_param_vqmax, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
  {"vqdiff", &lavc_param_vqdiff, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
  {"vqcomp", &lavc_param_vqcompress, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 1.0, NULL},
  {"vqblur", &lavc_param_vqblur, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 1.0, NULL},
  {"vb_qfactor", &lavc_param_vb_qfactor, CONF_TYPE_FLOAT, CONF_RANGE, -31.0, 31.0, NULL},
  {"vmax_b_frames", &lavc_param_vmax_b_frames, CONF_TYPE_INT, CONF_RANGE, 0, FF_MAX_B_FRAMES, NULL},
//  {"vpass", &lavc_param_vpass, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
  {"vrc_strategy", &lavc_param_vrc_strategy, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
  {"vb_strategy", &lavc_param_vb_strategy, CONF_TYPE_INT, CONF_RANGE, 0, 1, NULL},
  {"vb_qoffset", &lavc_param_vb_qoffset, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 31.0, NULL},
  {"vlelim", &lavc_param_luma_elim_threshold, CONF_TYPE_INT, CONF_RANGE, -99, 99, NULL},
  {"vcelim", &lavc_param_chroma_elim_threshold, CONF_TYPE_INT, CONF_RANGE, -99, 99, NULL},
  {"vpsize", &lavc_param_packet_size, CONF_TYPE_INT, CONF_RANGE, 0, 100000000, NULL},
  {"vstrict", &lavc_param_strict, CONF_TYPE_FLAG, 0, 0, 1, NULL},
  {"vdpart", &lavc_param_data_partitioning, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PART, NULL},
//  {"keyint", &lavc_param_keyint, CONF_TYPE_INT, 0, 0, 0, NULL},
  {"gray", &lavc_param_gray, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PART, NULL},
  {"mpeg_quant", &lavc_param_mpeg_quant, CONF_TYPE_FLAG, 0, 0, 1, NULL},
  {"vi_qfactor", &lavc_param_vi_qfactor, CONF_TYPE_FLOAT, CONF_RANGE, -31.0, 31.0, NULL},
  {"vi_qoffset", &lavc_param_vi_qoffset, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 31.0, NULL},
  {"vqsquish", &lavc_param_rc_qsquish, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 99.0, NULL},
  {"vqmod_amp", &lavc_param_rc_qmod_amp, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 99.0, NULL},
  {"vqmod_freq", &lavc_param_rc_qmod_freq, CONF_TYPE_INT, 0, 0, 0, NULL},
  {"vrc_eq", &lavc_param_rc_eq, CONF_TYPE_STRING, 0, 0, 0, NULL},
  {"vrc_override", &lavc_param_rc_override_string, CONF_TYPE_STRING, 0, 0, 0, NULL},
  {"vrc_maxrate", &lavc_param_rc_max_rate, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
  {"vrc_minrate", &lavc_param_rc_min_rate, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
  {"vrc_buf_size", &lavc_param_rc_min_rate, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
  {"vrc_buf_aggressivity", &lavc_param_rc_buffer_aggressivity, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 99.0, NULL},
  {"vrc_init_cplx", &lavc_param_rc_initial_cplx, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 9999999.0, NULL},
  {"vfdct", &lavc_param_fdct, CONF_TYPE_INT, CONF_RANGE, 0, 10, NULL},
  {"aspect", &lavc_param_aspect, CONF_TYPE_FLOAT, CONF_RANGE, 0.2, 3.0, NULL},
  {"lumi_mask", &lavc_param_lumi_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
  {"tcplx_mask", &lavc_param_temporal_cplx_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
  {"scplx_mask", &lavc_param_spatial_cplx_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
  {"p_mask", &lavc_param_p_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
  {"naq", &lavc_param_normalize_aqp, CONF_TYPE_FLAG, 0, 0, 1, NULL},
  {"dark_mask", &lavc_param_dark_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
  {"ildct", &lavc_param_interlaced_dct, CONF_TYPE_FLAG, 0, 0, 1, NULL},
  {"idct", &lavc_param_idct, CONF_TYPE_INT, CONF_RANGE, 0, 20, NULL},
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

int ffmpeg_read_config(char *section, char *prefix, struct config *conf) {
  CF_ROOT_TYPE    *p_root;
  CF_SECTION_TYPE *p_section;
  struct stat      statfile;
  char             buffer[1024];
  
// search for the config file called ffmpeg.cfg
  if (stat(FFMPEG_CONFIG_FILE, &statfile) != 0) {
    char *home = getenv("HOME");
    
    if (home != NULL) {
      snprintf(buffer, 1023, "%s/.%s", home, FFMPEG_CONFIG_FILE);
      if (stat(buffer, &statfile) != 0) {
        fprintf(stderr, "[%s] Neither './%s' nor '~/.%s' found. Falling back "
                "to hardcoded defaults.\n", prefix, FFMPEG_CONFIG_FILE,
                FFMPEG_CONFIG_FILE);
        return 0;
      }
    } else
      return 0;
  } else
    strcpy(buffer, FFMPEG_CONFIG_FILE);
  
  if (!S_ISREG(statfile.st_mode)) {
    fprintf(stderr, "[%s] '%s' is not a regular file. Falling back to hardcoded"
            " defaults.\n", prefix, buffer);
    return 0;
  }

  p_root = cf_read(buffer);
  if (p_root == NULL) {
    fprintf(stderr, "[%s] Error reading configuration file '%s'. Falling back "
            "to hardcoded defaults.\n", prefix, buffer);
    return 0;
  }

  p_section = cf_get_section(p_root);
  while (p_section != NULL) {
    if (!strcmp(p_section->name, section)) {
      ffmpeg_read_values(p_root, p_section, prefix, conf);
      CF_FREE_ROOT(p_root);
      return 1;
    }
    p_section = cf_get_next_section(p_root, p_section);
  }
  
  CF_FREE_ROOT(p_root);
  
  fprintf(stderr, "[%s] No section named '%s' found in '%s'. Falling "
          "back to hardcoded defaults.\n", prefix, section,
          FFMPEG_CONFIG_FILE);

  return 0;
}

int ffmpeg_read_values(CF_ROOT_TYPE *p_root, CF_SECTION_TYPE *p_section,
                        char *prefix, struct config *conf) {
  CF_KEYVALUE_TYPE *kv;
  struct config    *cur_config;
  char             *value, *error;
  int               i;
  float             f;

  cur_config = conf;

  while (cur_config->name != NULL) {
    value = cf_get_named_section_value_of_key(p_root, p_section->name,
                                              cur_config->name);
    if (value != NULL) {
      errno = 0;
      switch (cur_config->type) {
        case CONF_TYPE_INT:
          i = strtol(value, &error, 10);
          if ((errno != 0) || (i == LONG_MIN) || (i == LONG_MAX) ||
              ((error != NULL) && (*error != 0)))
            fprintf(stderr, "[%s] Option '%s' must be an integer.\n",
                    prefix, cur_config->name);
          else if ((cur_config->flags & CONF_MIN) && (i < cur_config->min))
            fprintf(stderr, "[%s] Option '%s' has a value that is too low "
                    "(%d < %d).\n", prefix, cur_config->name, i,
                    (int)cur_config->min);
          else if ((cur_config->flags & CONF_MAX) && (i > cur_config->max))
            fprintf(stderr, "[%s] Option '%s' has a value that is too high "
                    "(%d > %d).\n", prefix, cur_config->name, i,
                    (int)cur_config->max);
          else
            *((int *)cur_config->p) = i;
          break;
        case CONF_TYPE_FLAG:
          i = atoi(value);
          if (errno != 0)
            fprintf(stderr, "[%s] Option '%s' is a flag. The only values "
                    "allowed for it are '0' and '1'.\n", prefix,
                    cur_config->name);
          else if ((i != 1) && (i != 0))
            fprintf(stderr, "[%s] Option '%s' is a flag. The only values "
                    "allowed for it are '0' and '1'.\n", prefix,
                    cur_config->name);
          else
            *((int *)cur_config->p) = i;
          break;
        case CONF_TYPE_FLOAT:
          f = strtod(value, NULL);
          if (errno != 0)
            fprintf(stderr, "[%s] Option '%s' must be a float.\n",
                    prefix, cur_config->name);
          else if ((cur_config->flags & CONF_MIN) && (f < cur_config->min))
            fprintf(stderr, "[%s] Option '%s' has a value that is too low "
                    "(%f < %f).\n", prefix, cur_config->name, f,
                    cur_config->min);
          else if ((cur_config->flags & CONF_MAX) && (f > cur_config->max))
            fprintf(stderr, "[%s] Option '%s' has a value that is too high "
                    "(%f > %f).\n", prefix, cur_config->name, f,
                    cur_config->max);
          else
            *((float *)cur_config->p) = f;
          break;
        case CONF_TYPE_STRING:
          *((char **)cur_config->p) = strdup(value);
          break;
        default:
          fprintf(stderr, "[%s] Unsupported config type '%d' for '%s'.\n",
                  prefix, cur_config->type, cur_config->name);
      }
    }
  
    cur_config++;
  }
  
  kv = cf_get_named_section_keyvalue(p_root, p_section->name);
  while (kv != NULL) {
    cur_config = conf;
    i = 0;
    while (cur_config->name != NULL) {
      if (!strcmp(kv->key, cur_config->name)) {
        i = 1;
        break;
      }
      cur_config++;
    }
    if (!i)
      fprintf(stderr, "[%s] Key '%s' is not a valid option.\n", prefix,
              kv->key);
    kv = cf_get_named_section_next_keyvalue(p_root, p_section->name, kv);
  }
  
  return 0;
}

int ffmpeg_print_config(char *prefix, struct config *conf) {
  struct config *cur_config;
  char          *s;
  
  cur_config = conf;
  
  while (cur_config->name != NULL) {
    switch (cur_config->type) {
      case CONF_TYPE_INT:
        fprintf(stderr, "%s%s = %d\n", prefix, cur_config->name, 
                *((int *)cur_config->p));
        break;
      case CONF_TYPE_FLAG:
        fprintf(stderr, "%s%s = %d\n", prefix, cur_config->name, 
                *((int *)cur_config->p) ? 1 : 0);
        break;
      case CONF_TYPE_FLOAT:
        fprintf(stderr, "%s%s = %f\n", prefix, cur_config->name, 
                *((float *)cur_config->p));
        break;
      case CONF_TYPE_STRING:
        s = *((char **)cur_config->p);
        fprintf(stderr, "%s%s%s = %s\n", prefix,
                s == NULL ? "#" : (*s == 0 ? "#" : ""),
                cur_config->name,
                s == NULL ? "" : s);
        break;
      default:
        fprintf(stderr, "%s#%s = <UNSUPPORTED FORMAT>\n", prefix,
                cur_config->name);
    }
    cur_config++;
  }
  
  return 0;
}
