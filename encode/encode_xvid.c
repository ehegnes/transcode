/*****************************************************************************
 *  - XviD Transcode Export Module -
 *
 *  Copyright (C) 2001-2003 - Thomas Östreich
 *
 *  Author : Edouard Gomez <ed.gomez@free.fr>
 *
 *  Port to transcode 1.1.0+ Module System:
 *  Francesco Romani <fromani at gmail dot com> - December 2005
 *
 *  This file is part of transcode, a video stream processing tool
 *
 *  transcode is free software ; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation ; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY ; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program ; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 *****************************************************************************/

/*****************************************************************************
 * Includes
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#else
# ifdef SYSTEM_DARWIN
#  include "libdldarwin/dlfcn.h"
# endif
#endif

#ifndef SYS_BSD
# ifdef HAVE_MALLOC_H
# include <malloc.h>
# endif
#endif

#include "xvid4.h"

#include "libtcvideo/tcvideo.h"

#include "libioaux/configs.h"

#include "transcode.h"
#include "libtc/optstr.h"

#include "libtc/tcmodule-plugin.h"

/*****************************************************************************
 * Transcode module binding functions and strings
 ****************************************************************************/

#define MOD_NAME    "encode_xvid.so"
#define MOD_VERSION "v0.0.2 (2005-12-31)"
#define MOD_CAP     "XviD 1.x encoder"

/* XviD shared library name */
#define XVID_SHARED_LIB_BASE "libxvidcore"
#ifdef SYSTEM_DARWIN
#define XVID_SHARED_LIB_SUFX "dylib"
#else
#define XVID_SHARED_LIB_SUFX "so"
#endif
#define XVID_CONFIG_FILE "xvid.cfg"

static const char *xvid_help = ""
    "Overview:\n"
    "\tthis module encodes raw RGB/YUV video frames in MPEG4, using XviD.\n"
    "\tXviD is a high quality/performance ISO MPEG4 codec.\n"
    "Options:\n"
    "\thelp\tproduce module overview and options explanations\n";

/*****************************************************************************
 * Local data
 ****************************************************************************/

extern char* tc_config_dir;

/* Temporary audio/video buffer */

/*****************************************************************************
 * XviD symbols grouped in a nice struct.
 ****************************************************************************/

typedef int (*xvid_function_t)(void *handle, int opt, void *param1, void *param2);

typedef struct {
    void *so;
    xvid_function_t global;
    xvid_function_t encore;
    xvid_function_t plugin_onepass;
    xvid_function_t plugin_twopass1;
    xvid_function_t plugin_twopass2;
    xvid_function_t plugin_lumimasking;
} xvid_module_t;

static int load_xvid(xvid_module_t *xvid, const char *path);
static int unload_xvid(xvid_module_t *xvid);

/*****************************************************************************
 * Transcode module private data
 ****************************************************************************/

typedef struct {
    /* XviD lib functions */
    xvid_module_t xvid;

    /* Instance related global vars */
    void *instance;
    xvid_gbl_init_t   xvid_gbl_init;
    xvid_enc_create_t xvid_enc_create;
    xvid_enc_frame_t  xvid_enc_frame;

    /* This data must survive local block scope, so here it is */
    xvid_enc_plugin_t    plugins[7];
    xvid_enc_zone_t      zones[2];
    xvid_plugin_single_t onepass;
    xvid_plugin_2pass1_t pass1;
    xvid_plugin_2pass2_t pass2;

    /* Options from the config file */
    xvid_enc_create_t    cfg_create;
    xvid_enc_frame_t     cfg_frame;
    xvid_plugin_single_t cfg_onepass;
    xvid_plugin_2pass2_t cfg_pass2;
    char *cfg_intra_matrix_file;
    char *cfg_inter_matrix_file;
    char *cfg_quant_method;
    int cfg_packed;
    int cfg_closed_gop;
    int cfg_interlaced;
    int cfg_quarterpel;
    int cfg_gmc;
    int cfg_trellis;
    int cfg_cartoon;
    int cfg_hqacpred;
    int cfg_chromame;
    int cfg_vhq;
    int cfg_motion;
    int cfg_stats;
    int cfg_greyscale;
    int cfg_turbo;
    int cfg_full1pass;

    /* MPEG4 stream buffer */
    int   stream_size;
    uint8_t *stream;

    /* Stats accumulators */
    int frames;
    int64_t sse_y;
    int64_t sse_u;
    int64_t sse_v;

    /* Image format conversion handle */
    TCVHandle tcvhandle;
} XviDPrivateData;

static const char *errorstring(int err);
static void reset_module(XviDPrivateData *mod);
static void cleanup_module(XviDPrivateData *mod);
static void read_config_file(XviDPrivateData *mod);
static void dispatch_settings(XviDPrivateData *mod);
static void set_create_struct(XviDPrivateData *mod, vob_t *vob);
static void set_frame_struct(XviDPrivateData *mod,
                             vob_t *vob,
                             const vframe_list_t *inframe,
                             vframe_list_t *outframe);

/***************************************************************************/

static int xvid_configure(TCModuleInstance *self,
                          const char *options,
                          vob_t *vob)
{
    int ret;    
    XviDPrivateData *pd = NULL;

    if (!self || !vob) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }
    pd = self->userdata;

    /* Load the config file settings */
    read_config_file(pd);

    /* Dispatch settings to xvid structures that hold the config ready to
     * be copied to encoder structures */
    dispatch_settings(pd);

    /* Init the xvidcore lib */
    memset(&pd->xvid_gbl_init, 0, sizeof(xvid_gbl_init_t));
    pd->xvid_gbl_init.version = XVID_VERSION;
    
    ret = pd->xvid.global(NULL, XVID_GBL_INIT, &pd->xvid_gbl_init, NULL);
    if (ret < 0) {
        tc_log_error(MOD_NAME, "Library initialization failed");
        return TC_EXPORT_ERROR;
    }

    /* Combine both the config settings with the transcode direct options
     * into the final xvid_enc_create_t struct */
    set_create_struct(pd, vob);
    ret = pd->xvid.encore(NULL, XVID_ENC_CREATE, &pd->xvid_enc_create, NULL);

    if (ret < 0) {
        tc_log_error(MOD_NAME, "Encoder initialization failed");
        return TC_EXPORT_ERROR;
    }

    /* Attach returned instance */
    pd->instance = pd->xvid_enc_create.handle;

    return TC_EXPORT_OK;
}


static int xvid_init(TCModuleInstance *self)
{
    int ret;
    XviDPrivateData *pd = NULL;
    vob_t *vob = tc_get_vob();
    
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }
 
    /* Check frame dimensions */
    if (vob->ex_v_width % 2 || vob->ex_v_height % 2) {
        tc_log_warn(MOD_NAME, "Only even dimensions allowed (%dx%d)",
                              vob->ex_v_width, vob->ex_v_height);
        return TC_EXPORT_ERROR;
    }

    pd = tc_malloc(sizeof(XviDPrivateData));
    if (!pd) {
        tc_log_error(MOD_NAME, "init: can't allocate XviD private data");
        return TC_EXPORT_ERROR;
    }

    /* Buffer allocation
     * We allocate width*height*bpp/8 to "receive" the compressed stream
     * I don't think the codec will ever return more than that. It's and
     * encoder, so if it fails delivering smaller frames than original
     * ones, something really odd occurs somewhere and i prefer the
     * application crash */
    if (vob->im_v_codec == CODEC_RGB || vob->im_v_codec == CODEC_YUV422) {
        pd->tcvhandle = tcv_init();
        if (!pd->tcvhandle) {
            tc_log_warn(MOD_NAME, "tcv_init failed");
            goto init_failed;
        }
    }
    
    reset_module(pd);

    /* Load the codec */
    ret = load_xvid(&pd->xvid, vob->mod_path);
    if (ret < 0) {
        goto init_failed;
        return TC_EXPORT_ERROR;
    }

    self->userdata = pd;
    /* can't fail, here */
    xvid_configure(self, "", vob);

    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }
    return TC_EXPORT_OK;

init_failed:
    tc_free(pd);
    self->userdata = NULL; /* paranoia */
    return TC_EXPORT_ERROR;
} 

static const char *xvid_inspect(TCModuleInstance *self,
                                const char *param)
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return NULL;
    }

    if (optstr_lookup(param, "help")) {
        return xvid_help;
    }

    return "";
}
    
static int xvid_encode_video(TCModuleInstance *self,
                             vframe_list_t *inframe, vframe_list_t *outframe)
{
    int bytes;
    xvid_enc_stats_t xvid_enc_stats;
    xvid_module_t *xvid = NULL;
    vob_t *vob = tc_get_vob();
    XviDPrivateData *pd = NULL;

    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    pd = self->userdata;
    xvid = &pd->xvid;
    
    /* Video encoding */

    if(vob->im_v_codec == CODEC_YUV422) {
        /* Convert to UYVY */
        tcv_convert(pd->tcvhandle, inframe->video_buf, vob->ex_v_width,
                    vob->ex_v_height, IMG_YUV422P, IMG_UYVY);
    } else if (vob->im_v_codec == CODEC_RGB) {
        /* Convert to BGR (why isn't RGB supported??) */
        tcv_convert(pd->tcvhandle, inframe->video_buf, vob->ex_v_width,
                    vob->ex_v_height, IMG_RGB24, IMG_BGR24);
    }

    /* Init the stat structure */
    memset(&xvid_enc_stats, 0, sizeof(xvid_enc_stats_t));
    xvid_enc_stats.version = XVID_VERSION;

    /* Combine both the config settings with the transcode direct options
     * into the final xvid_enc_frame_t struct */
    set_frame_struct(pd, vob, inframe, outframe);

    bytes = xvid->encore(pd->instance, XVID_ENC_ENCODE,
                         &pd->xvid_enc_frame, &xvid_enc_stats);

    /* Error handling */
    if (bytes < 0) {
        tc_log_error(MOD_NAME, "xvidcore returned a \"%s\" error",
                               errorstring(bytes));
        return TC_EXPORT_ERROR;
    }
    outframe->video_size = bytes;

    /* Extract stats info */
    if (xvid_enc_stats.type>0 && pd->cfg_stats) {
        pd->frames++;
        pd->sse_y += xvid_enc_stats.sse_y;
        pd->sse_u += xvid_enc_stats.sse_u;
        pd->sse_v += xvid_enc_stats.sse_v;
    }

    /* XviD Core rame buffering handling
    * We must make sure audio A/V is still good and does not run away */
    if (bytes == 0) {
    /* XXX */
        extern pthread_mutex_t delay_video_frames_lock;
        extern int video_frames_delay;
        pthread_mutex_lock(&delay_video_frames_lock);
        video_frames_delay++;
        pthread_mutex_unlock(&delay_video_frames_lock);
        outframe->video_size = 0; /* paranoia */
        return TC_EXPORT_OK;
    }

    if (pd->xvid_enc_frame.out_flags & XVID_KEYFRAME) {
        outframe->attributes |= TC_FRAME_IS_KEYFRAME;
    }

    return TC_EXPORT_OK;
}

#define SSE2PSNR(sse, width, height) \
((!(sse)) ? (99.0f) : (48.131f - 10*(float)log10((float)(sse)/((float)((width)*(height))))))

static int xvid_stop(TCModuleInstance *self)
{
    int ret;
    xvid_module_t *xvid = NULL;
    XviDPrivateData *pd = NULL;

    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    pd = self->userdata;
    xvid = &pd->xvid;

    /* ToDo: Can we flush the last frames here ? */

    /* Destroy the encoder instance */
    ret = xvid->encore(pd->instance, XVID_ENC_DESTROY, NULL, NULL);
    if (ret < 0) {
        tc_log_warn(MOD_NAME, "Encoder instance releasing failed");
        return TC_EXPORT_ERROR;
    }

    /* Print stats before resting the complete module structure */
    if (pd->cfg_stats) {
        if(pd->frames > 0) {
            pd->sse_y /= pd->frames;
            pd->sse_u /= pd->frames;
            pd->sse_v /= pd->frames;
        } else {
            pd->sse_y = 0;
            pd->sse_u = 0;
            pd->sse_v = 0;
        }

        tc_log_info(MOD_NAME,
            "psnr y = %.2f dB, "
            "psnr u = %.2f dB, "
            "psnr v = %.2f dB",
            SSE2PSNR(pd->sse_y,
                 pd->xvid_enc_create.width,
                 pd->xvid_enc_create.height),
            SSE2PSNR(pd->sse_u,
                 pd->xvid_enc_create.width/2,
                 pd->xvid_enc_create.height/2),
            SSE2PSNR(pd->sse_v,
                 pd->xvid_enc_create.width/2,
                 pd->xvid_enc_create.height/2));
    }
    return TC_EXPORT_OK;
}
#undef SSE2PSNR

static int xvid_fini(TCModuleInstance *self)
{
    xvid_module_t *xvid = NULL;
    XviDPrivateData *pd = NULL;
    
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }
    xvid_stop(self);
    
    pd = self->userdata;
    xvid = &pd->xvid;

    /* Unload the shared symbols/lib */
    unload_xvid(xvid);

    /* Free all dynamic memory allocated in the module structure */
    cleanup_module(pd);

    /* This is the last function according to the transcode API
     * this should be safe to reset the module structure */
    reset_module(pd);

    tc_free(self->userdata);
    self->userdata = NULL;
    
    return TC_EXPORT_OK;
}

/*************************************************************************/

static const int xvid_codecs_in[] = { 
    TC_CODEC_RGB, TC_CODEC_YUV422P, TC_CODEC_YUV420P,
    TC_CODEC_ERROR
};

/* a encodeor is at the end of pipeline */
static const int xvid_codecs_out[] = {
    TC_CODEC_XVID,
    TC_CODEC_ERROR
};

static const TCModuleInfo xvid_info = {
    .features    = TC_MODULE_FEATURE_ENCODE|TC_MODULE_FEATURE_VIDEO
                   |TC_MODULE_FEATURE_AUDIO,
    .flags       = TC_MODULE_FLAG_RECONFIGURABLE,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = xvid_codecs_in,
    .codecs_out  = xvid_codecs_out
};

static const TCModuleClass xvid_class = {
    .info         = &xvid_info,

    .init         = xvid_init,
    .fini         = xvid_fini,
    .configure    = xvid_configure,
    .stop         = xvid_stop,
    .inspect      = xvid_inspect,
    
    .encode_video = xvid_encode_video
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &xvid_class;
}


/*****************************************************************************
 * Transcode module helper functions
 ****************************************************************************/

static void reset_module(XviDPrivateData *mod)
{
    /* Default options */
    mod->cfg_packed = 0;
    mod->cfg_closed_gop = 1;
    mod->cfg_interlaced = 0;
    mod->cfg_quarterpel = 0;
    mod->cfg_gmc = 0;
    mod->cfg_trellis = 0;
    mod->cfg_cartoon = 0;
    mod->cfg_hqacpred = 1;
    mod->cfg_chromame = 1;
    mod->cfg_vhq = 1;
    mod->cfg_motion = 6;
    mod->cfg_turbo = 0;
    mod->cfg_full1pass = 0;
    mod->cfg_stats = 0;
    mod->cfg_greyscale = 0;
    mod->cfg_quant_method = "h263";
    mod->cfg_create.max_bframes = 1;
    mod->cfg_create.bquant_ratio = 150;
    mod->cfg_create.bquant_offset = 100;

    return;
}

static void cleanup_module(XviDPrivateData *mod)
{

    /* Free tcvideo handle */
    tcv_free(mod->tcvhandle);
    mod->tcvhandle = 0;

    /* Release stream buffer memory */
    if(mod->stream) {
        free(mod->stream);
        mod->stream = NULL;
    }

    /* Release the matrix file name string */
    if(mod->cfg_inter_matrix_file) {
        free(mod->cfg_inter_matrix_file);
        mod->cfg_inter_matrix_file = NULL;
    }

    /* Release the matrix definition */
    if(mod->cfg_frame.quant_inter_matrix) {
        free(mod->cfg_frame.quant_inter_matrix);
        mod->cfg_frame.quant_inter_matrix = NULL;
    }

    /* Release the matrix file name string */
    if(mod->cfg_intra_matrix_file) {
        free(mod->cfg_intra_matrix_file);
        mod->cfg_intra_matrix_file = NULL;
    }

    /* Release the matrix definition */
    if(mod->cfg_frame.quant_intra_matrix) {
        free(mod->cfg_frame.quant_intra_matrix);
        mod->cfg_frame.quant_intra_matrix = NULL;
    }

    return;
}

/*****************************************************************************
 * Configuration functions
 *
 * They fill the .cfg_xxx members of the module structure.
 *  - read_config_file reads the values from the config file and sets .cfg_xxx
 *    members of the module structure.
 *  - dispatch_settings uses the values retrieved by read_config_file and
 *    turns them into XviD settings in the cfg_xxx xvid structure available
 *    in the module structure.
 *  - set_create_struct sets a xvid_enc_create structure according to the
 *    settings generated by the two previous functions calls.
 *  - set_frame_struct same as above for a xvid_enc_frame_t struct.
 ****************************************************************************/

#define INTRA_MATRIX    0
#define INTER_MATRIX    1

static void load_matrix(XviDPrivateData *mod, int type) 
{
    xvid_enc_frame_t *frame = &mod->cfg_frame;
    const char *filename = (type == INTER_MATRIX)
                                    ?mod->cfg_inter_matrix_file
                                    :mod->cfg_intra_matrix_file;
    uint8_t *matrix = NULL;

    if (!filename) {
        return;
    }

    matrix = tc_malloc(TC_MATRIX_SIZE);
    if (matrix != NULL) {
        int ret =  tc_read_matrix(filename, matrix, NULL);
        
        if (ret == 0) {
            tc_log_info(MOD_NAME, "Loaded %s matrix (switching to "
                                  "mpeg quantization type)",
                                  (type == INTER_MATRIX) ?"Inter" :"Intra");
                //print_matrix(matrix, NULL);
                //free(mod->cfg_quant_method);
                mod->cfg_quant_method = "mpeg";
        } else {
            tc_free(matrix);
            matrix = NULL;
        }
    }

    if (type == INTER_MATRIX) {
        frame->quant_inter_matrix = matrix;
    } else {
        frame->quant_intra_matrix = matrix;
    }
}

static void read_config_file(XviDPrivateData *mod)
{
    xvid_plugin_single_t *onepass = &mod->cfg_onepass;
    xvid_plugin_2pass2_t *pass2   = &mod->cfg_pass2;
    xvid_enc_create_t    *create  = &mod->cfg_create;
    xvid_enc_frame_t     *frame   = &mod->cfg_frame;

    struct config xvid_config[] = {
            /* Section [features] */
            {"features", "Feature settings", CONF_TYPE_SECTION, 0, 0, 0, NULL},
            {"quant_type", &mod->cfg_quant_method, CONF_TYPE_STRING, 0, 0, 0, NULL},
            {"motion", &mod->cfg_motion, CONF_TYPE_INT, CONF_RANGE, 0, 6, NULL},
            {"chromame", &mod->cfg_chromame, CONF_TYPE_FLAG, 0, 0, 1, NULL},
            {"vhq", &mod->cfg_vhq, CONF_TYPE_INT, CONF_RANGE, 0, 4, NULL},
            {"max_bframes", &create->max_bframes, CONF_TYPE_INT, CONF_RANGE, 0, 20, NULL},
            {"bquant_ratio", &create->bquant_ratio, CONF_TYPE_INT, CONF_RANGE, 0, 200, NULL},
            {"bquant_offset", &create->bquant_offset, CONF_TYPE_INT, CONF_RANGE, 0, 200, NULL},
            {"bframe_threshold", &frame->bframe_threshold, CONF_TYPE_INT, CONF_RANGE, -255, 255, NULL},
            {"quarterpel", &mod->cfg_quarterpel, CONF_TYPE_FLAG, 0, 0, 1, NULL},
            {"gmc", &mod->cfg_gmc, CONF_TYPE_FLAG, 0, 0, 1, NULL},
            {"trellis", &mod->cfg_trellis, CONF_TYPE_FLAG, 0, 0, 1, NULL},
            {"packed", &mod->cfg_packed, CONF_TYPE_FLAG, 0, 0, 1, NULL},
            {"closed_gop", &mod->cfg_closed_gop, CONF_TYPE_FLAG, 0, 0, 1, NULL},
            {"interlaced", &mod->cfg_interlaced, CONF_TYPE_FLAG, 0, 0, 1, NULL},
            {"cartoon", &mod->cfg_cartoon, CONF_TYPE_FLAG, 0, 0, 1, NULL},
            {"hqacpred", &mod->cfg_hqacpred, CONF_TYPE_FLAG, 0, 0, 1, NULL},
            {"frame_drop_ratio", &create->frame_drop_ratio, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
            {"stats", &mod->cfg_stats, CONF_TYPE_FLAG, 0, 0, 1, NULL},
            {"greyscale", &mod->cfg_greyscale, CONF_TYPE_FLAG, 0, 0, 1, NULL},
            {"turbo", &mod->cfg_turbo, CONF_TYPE_FLAG, 0, 0, 1, NULL},
            {"full1pass", &mod->cfg_full1pass, CONF_TYPE_FLAG, 0, 0, 1, NULL},

            /* section [quantizer] */
            {"quantizer", "Quantizer settings", CONF_TYPE_SECTION, 0, 0, 0, NULL},
            {"min_iquant", &create->min_quant[0], CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
            {"max_iquant", &create->max_quant[0], CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
            {"min_pquant", &create->min_quant[1], CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
            {"max_pquant", &create->max_quant[1], CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
            {"min_bquant", &create->min_quant[2], CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
            {"max_bquant", &create->max_quant[2], CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
            {"quant_intra_matrix", &mod->cfg_intra_matrix_file, CONF_TYPE_STRING, 0, 0, 100, NULL},
            {"quant_inter_matrix", &mod->cfg_inter_matrix_file, CONF_TYPE_STRING, 0, 0, 100, NULL},

            /* section [cbr] */
            {"cbr", "CBR settings", CONF_TYPE_SECTION, 0, 0, 0, NULL},
            {"reaction_delay_factor", &onepass->reaction_delay_factor, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
            {"averaging_period", &onepass->averaging_period, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},
            {"buffer", &onepass->buffer, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},

            /* section [cbr] */
            {"vbr", "VBR settings", CONF_TYPE_SECTION, 0, 0, 0, NULL},
            {"keyframe_boost", &pass2->keyframe_boost, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
            {"curve_compression_high", &pass2->curve_compression_high, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
            {"curve_compression_low", &pass2->curve_compression_low, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
            {"overflow_control_strength", &pass2->overflow_control_strength, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
            {"max_overflow_improvement", &pass2->max_overflow_improvement, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
            {"max_overflow_degradation", &pass2->max_overflow_degradation, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
            {"kfreduction", &pass2->kfreduction, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
            {"kfthreshold", &pass2->kfthreshold, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},
            {"container_frame_overhead", &pass2->container_frame_overhead, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},

            /* End of the config file */
            {NULL, 0, 0, 0, 0, 0, NULL}
    };

    /* Read the values */
    module_read_config(NULL, MOD_NAME, "xvid", xvid_config, tc_config_dir);

    /* Print the values */
    if (verbose & TC_DEBUG) {
        module_print_config("["MOD_NAME"] ", xvid_config);
    }
        
    return;
}

static void dispatch_settings(XviDPrivateData *mod)
{

    xvid_enc_create_t *create = &mod->cfg_create;
    xvid_enc_frame_t  *frame  = &mod->cfg_frame;

    const int motion_presets[7] =
        {
            0,
            0,
            0,
            0,
            XVID_ME_HALFPELREFINE16,
            XVID_ME_HALFPELREFINE16 | XVID_ME_ADVANCEDDIAMOND16,
            XVID_ME_HALFPELREFINE16 | XVID_ME_EXTSEARCH16 |
            XVID_ME_HALFPELREFINE8  | XVID_ME_USESQUARES16
        };


    /* Dispatch all settings having an impact on the "create" structure */
    create->global = 0;

    if (mod->cfg_packed) {
        create->global |= XVID_GLOBAL_PACKED;
    }
    if (mod->cfg_closed_gop) {
        create->global |= XVID_GLOBAL_CLOSED_GOP;
    }
    if (mod->cfg_stats) {
        create->global |= XVID_GLOBAL_EXTRASTATS_ENABLE;
    }
    /* Dispatch all settings having an impact on the "frame" structure */
    frame->vol_flags = 0;
    frame->vop_flags = 0;
    frame->motion    = 0;

    frame->vop_flags |= XVID_VOP_HALFPEL;
    frame->motion    |= motion_presets[mod->cfg_motion];

    if (mod->cfg_stats) {
        frame->vol_flags |= XVID_VOL_EXTRASTATS;
    }
    if (mod->cfg_greyscale) {
        frame->vop_flags |= XVID_VOP_GREYSCALE;
    }
    if (mod->cfg_cartoon) {
        frame->vop_flags |= XVID_VOP_CARTOON;
        frame->motion |= XVID_ME_DETECT_STATIC_MOTION;
    }

    load_matrix(mod, INTRA_MATRIX);
    load_matrix(mod, INTER_MATRIX);
    
    if (!strcasecmp(mod->cfg_quant_method, "mpeg")) {
        frame->vol_flags |= XVID_VOL_MPEGQUANT;
    }
    if (mod->cfg_quarterpel) {
        frame->vol_flags |= XVID_VOL_QUARTERPEL;
        frame->motion    |= XVID_ME_QUARTERPELREFINE16;
        frame->motion    |= XVID_ME_QUARTERPELREFINE8;
    }
    if (mod->cfg_gmc) {
        frame->vol_flags |= XVID_VOL_GMC;
        frame->motion    |= XVID_ME_GME_REFINE;
    }
    if (mod->cfg_interlaced) {
        frame->vol_flags |= XVID_VOL_INTERLACING;
    }
    if (mod->cfg_trellis) {
        frame->vop_flags |= XVID_VOP_TRELLISQUANT;
    }
    if (mod->cfg_hqacpred) {
        frame->vop_flags |= XVID_VOP_HQACPRED;
    }
    if (mod->cfg_motion > 4) {
        frame->vop_flags |= XVID_VOP_INTER4V;
    }
    if (mod->cfg_chromame) {
        frame->motion |= XVID_ME_CHROMA_PVOP;
        frame->motion |= XVID_ME_CHROMA_BVOP;
    }
    if (mod->cfg_vhq >= 1) {
        frame->vop_flags |= XVID_VOP_MODEDECISION_RD;
    }
    if (mod->cfg_vhq >= 2) {
        frame->motion |= XVID_ME_HALFPELREFINE16_RD;
        frame->motion |= XVID_ME_QUARTERPELREFINE16_RD;
    }
    if (mod->cfg_vhq >= 3) {
        frame->motion |= XVID_ME_HALFPELREFINE8_RD;
        frame->motion |= XVID_ME_QUARTERPELREFINE8_RD;
        frame->motion |= XVID_ME_CHECKPREDICTION_RD;
    }
    if (mod->cfg_vhq >= 4) {
        frame->motion |= XVID_ME_EXTSEARCH_RD;
    }
    if (mod->cfg_turbo) {
        frame->motion |= XVID_ME_FASTREFINE16;
        frame->motion |= XVID_ME_FASTREFINE8;
        frame->motion |= XVID_ME_SKIP_DELTASEARCH;
        frame->motion |= XVID_ME_FAST_MODEINTERPOLATE;
        frame->motion |= XVID_ME_BFRAME_EARLYSTOP;
    }

    /* motion level == 0 means no motion search which is equivalent to
     * intra coding only */
    if (mod->cfg_motion == 0) {
        frame->type = XVID_TYPE_IVOP;
    } else {
        frame->type = XVID_TYPE_AUTO;
    }

    return;
}

static void set_create_struct(XviDPrivateData *mod, vob_t *vob)
{
    xvid_enc_create_t *x    = &mod->xvid_enc_create;
    xvid_enc_create_t *xcfg = &mod->cfg_create;
    xvid_module_t *xvid     = &mod->xvid;

    memset(x, 0, sizeof(xvid_enc_create_t));
    x->version = XVID_VERSION;

    /* Global encoder options */
    x->global = xcfg->global;

    /* Width and Height */
    x->width  = vob->ex_v_width;
    x->height = vob->ex_v_height;

    /* Max keyframe interval */
    x->max_key_interval = vob->divxkeyframes;

    /* FPS : we take care of non integer values */
    if ((vob->ex_fps - (int)vob->ex_fps) == 0) {
        x->fincr = 1;
        x->fbase = (int)vob->ex_fps;
    } else {
        x->fincr = 1001;
        x->fbase = (int)(1001 * vob->ex_fps);
    }

    /* BFrames settings */
    x->max_bframes   = xcfg->max_bframes;
    x->bquant_ratio  = xcfg->bquant_ratio;
    x->bquant_offset = xcfg->bquant_offset;

    /* Frame dropping factor */
    x->frame_drop_ratio = xcfg->frame_drop_ratio;

    /* Quantizers */
    x->min_quant[0] = xcfg->min_quant[0];
    x->min_quant[1] = xcfg->min_quant[1];
    x->min_quant[2] = xcfg->min_quant[2];
    x->max_quant[0] = xcfg->max_quant[0];
    x->max_quant[1] = xcfg->max_quant[1];
    x->max_quant[2] = xcfg->max_quant[2];

    /* Encodings zones
     * ToDo?: Allow zones definitions */
    memset(mod->zones, 0, sizeof(mod->zones));
    x->zones     = mod->zones;

    if (1 == vob->divxmultipass && mod->cfg_full1pass)
    {
        x->zones[0].frame = 0;
        x->zones[0].mode = XVID_ZONE_QUANT;
        x->zones[0].increment = 200;
        x->zones[0].base = 100;
        x->num_zones = 1;
    } else {
        x->num_zones = 0;
    }

    /* Plugins */
    memset(mod->plugins, 0, sizeof(mod->plugins));
    x->plugins     = mod->plugins;
    x->num_plugins = 0;

    /* Initialize rate controller plugin */

    /* This is the first pass of a Two pass process */
    if (vob->divxmultipass == 1) {
        xvid_plugin_2pass1_t *pass1 = &mod->pass1;

        if (xvid->plugin_twopass1 == NULL) {
            tc_log_warn(MOD_NAME, "Two Pass #1 bitrate controller plugin not available");
            return;
        }

        memset(pass1, 0, sizeof(xvid_plugin_2pass1_t));
        pass1->version  = XVID_VERSION;
        pass1->filename = vob->divxlogfile;

        x->plugins[x->num_plugins].func  = xvid->plugin_twopass1;
        x->plugins[x->num_plugins].param = pass1;
        x->num_plugins++;
    }

    /* This is the second pass of a Two pass process */
    if (vob->divxmultipass == 2) {
        xvid_plugin_2pass2_t *pass2 = &mod->pass2;
        xvid_plugin_2pass2_t *pass2cfg = &mod->cfg_pass2;

        if (xvid->plugin_twopass2 == NULL) {
            tc_log_warn(MOD_NAME, "Two Pass #2 bitrate controller plugin not available");
            return;
        }

        memset(pass2, 0, sizeof(xvid_plugin_2pass2_t));
        pass2->version  = XVID_VERSION;
        pass2->filename = vob->divxlogfile;

        /* Apply config file settings if any, or all 0s which lets XviD
         * apply its defaults */
        pass2->keyframe_boost = pass2cfg->keyframe_boost;
        pass2->curve_compression_high = pass2cfg->curve_compression_high;
        pass2->curve_compression_low = pass2cfg->curve_compression_low;
        pass2->overflow_control_strength = pass2cfg->overflow_control_strength;
        pass2->max_overflow_improvement = pass2cfg->max_overflow_improvement;
        pass2->max_overflow_degradation = pass2cfg->max_overflow_degradation;
        pass2->kfreduction = pass2cfg->kfreduction;
        pass2->kfthreshold = pass2cfg->kfthreshold;
        pass2->container_frame_overhead = pass2cfg->container_frame_overhead;

        /* Positive bitrate values are bitrates as usual but if the
         * value is negative it is considered as being a total size
         * to reach (in kilobytes) */
        if (vob->divxbitrate > 0) {
            pass2->bitrate  = vob->divxbitrate*1000;
        } else {
            pass2->bitrate  = vob->divxbitrate;
        }
        x->plugins[x->num_plugins].func  = xvid->plugin_twopass2;
        x->plugins[x->num_plugins].param = pass2;
        x->num_plugins++;
    }

    /* This is a single pass encoding: either a CBR pass or a constant
     * quantizer pass */
    if (vob->divxmultipass == 0  || vob->divxmultipass == 3) {
        xvid_plugin_single_t *onepass = &mod->onepass;
        xvid_plugin_single_t *cfgonepass = &mod->cfg_onepass;

        if (xvid->plugin_onepass == NULL) {
            tc_log_warn(MOD_NAME, "One Pass bitrate controller plugin not available");
            return;
        }

        memset(onepass, 0, sizeof(xvid_plugin_single_t));
        onepass->version = XVID_VERSION;
        onepass->bitrate = vob->divxbitrate*1000;

        /* Apply config file settings if any, or all 0s which lets XviD
         * apply its defaults */
        onepass->reaction_delay_factor = cfgonepass->reaction_delay_factor;
        onepass->averaging_period = cfgonepass->averaging_period;
        onepass->buffer = cfgonepass->buffer;

        /* Quantizer mode uses the same plugin, we have only to define
         * a constant quantizer zone beginning at frame 0 */
        if (vob->divxmultipass == 3) {
            x->zones[x->num_zones].mode      = XVID_ZONE_QUANT;
            x->zones[x->num_zones].frame     = 1;
            x->zones[x->num_zones].increment = vob->divxbitrate;
            x->zones[x->num_zones].base      = 1;
            x->num_zones++;
        }


        x->plugins[x->num_plugins].func  = xvid->plugin_onepass;
        x->plugins[x->num_plugins].param = onepass;
        x->num_plugins++;
    }

    return;
}

static void set_frame_struct(XviDPrivateData *mod, vob_t *vob,
                             const vframe_list_t *inframe, vframe_list_t *outframe)
{
    xvid_enc_frame_t *x    = &mod->xvid_enc_frame;
    xvid_enc_frame_t *xcfg = &mod->cfg_frame;

    memset(x, 0, sizeof(xvid_enc_frame_t));
    x->version = XVID_VERSION;

    /* Bind output buffer */
    x->bitstream = outframe->video_buf;
    x->length    = outframe->video_size;

    /* Bind source frame */
    x->input.plane[0] = inframe->video_buf;
    if (vob->im_v_codec == CODEC_RGB) {
        x->input.csp       = XVID_CSP_BGR;
        x->input.stride[0] = vob->ex_v_width*3;
    } else if (vob->im_v_codec == CODEC_YUV422) {
        x->input.csp       = XVID_CSP_UYVY;
        x->input.stride[0] = vob->ex_v_width*2;
    } else {
        x->input.csp       = XVID_CSP_I420;
        x->input.stride[0] = vob->ex_v_width;
    }

    /* Set up core's VOL level features */
    x->vol_flags = xcfg->vol_flags;

    /* Set up core's VOP level features */
    x->vop_flags = xcfg->vop_flags;

    /* Frame type -- let core decide for us */
    x->type = xcfg->type;

    /* Force the right quantizer -- It is internally managed by RC
     * plugins */
    x->quant = 0;

    /* Set up motion estimation flags */
    x->motion = xcfg->motion;

    /* We don't use special matrices */
    x->quant_intra_matrix = xcfg->quant_intra_matrix;
    x->quant_inter_matrix = xcfg->quant_inter_matrix;

    /* pixel aspect ratio
     * transcode.c uses 0 for EXT instead of 15 */
    if ((vob->ex_par==0) &&
        (vob->ex_par_width==1) && (vob->ex_par_height==1)) {
        vob->ex_par = 1;
    }
    x->par = (vob->ex_par==0)? XVID_PAR_EXT: vob->ex_par;
    x->par_width = vob->ex_par_width;
    x->par_height = vob->ex_par_height;

    return;
}

/*****************************************************************************
 * Returns an error string corresponding to the XviD err code
 ****************************************************************************/

static const char *errorstring(int err)
{
    char *error;
    switch(err) {
    case XVID_ERR_FAIL:
        error = "General fault";
        break;
    case XVID_ERR_MEMORY:
        error =  "Memory allocation error";
        break;
    case XVID_ERR_FORMAT:
        error =  "File format error";
        break;
    case XVID_ERR_VERSION:
        error =  "Structure version not supported";
        break;
    case XVID_ERR_END:
        error =  "End of stream reached";
        break;
    default:
        error = "Unknown";
    }

    return (const char *)error;
}

/*****************************************************************************
 * Un/Loading XviD shared lib and symbols
 ****************************************************************************/

static int load_xvid(xvid_module_t *xvid, const char *path)
{
    const char *error;
    char soname[4][4096];
    int i;

    /* Reset pointers */
    memset(xvid, 0, sizeof(xvid[0]));

    /* First we build all sonames we will try to load */
#ifdef SYSTEM_DARWIN
    tc_snprintf(soname[0], 4095, "%s/%s.%d.%s", path, XVID_SHARED_LIB_BASE,
                XVID_API_MAJOR(XVID_API), XVID_SHARED_LIB_SUFX);
#else
    tc_snprintf(soname[0], 4095, "%s/%s.%s.%d", path, XVID_SHARED_LIB_BASE,
                XVID_SHARED_LIB_SUFX, XVID_API_MAJOR(XVID_API));
#endif
#ifdef SYSTEM_DARWIN
    tc_snprintf(soname[1], 4095, "%s.%d.%s", XVID_SHARED_LIB_BASE,
                XVID_API_MAJOR(XVID_API), XVID_SHARED_LIB_SUFX);
#else
    tc_snprintf(soname[1], 4095, "%s.%s.%d", XVID_SHARED_LIB_BASE,
                XVID_SHARED_LIB_SUFX, XVID_API_MAJOR(XVID_API));
#endif
    tc_snprintf(soname[2], 4095, "%s/%s.%s", path, XVID_SHARED_LIB_BASE,
                XVID_SHARED_LIB_SUFX);
    tc_snprintf(soname[3], 4095, "%s.%s", XVID_SHARED_LIB_BASE,
                XVID_SHARED_LIB_SUFX);

    /* Let's try each shared lib until success */
    for (i = 0; i < 4; i++) {
        if (verbose & TC_DEBUG) {
            tc_log_info(MOD_NAME, "Trying to load shared lib %s",
                soname[i]);
        }
        /* Try loading the shared lib */
        xvid->so = dlopen(soname[i], RTLD_GLOBAL| RTLD_LAZY);

        /* Test wether loading succeeded */
        if (xvid->so != NULL) {
            break;
        }
    }

    /* None of the modules were available */
    if (xvid->so == NULL) {
        tc_log_warn(MOD_NAME, "No libxvidcore API4 found");
        return -1;
    }

    if (verbose & TC_DEBUG) {
        tc_log_info(MOD_NAME, "Loaded %s", soname[i]);
    }
    
    /* Next step is to load xvidcore symbols
     *
     * Some of them are mandatory, others like plugins can be safely
     * ignored if they are not available, this will just restrict user
     * available functionnality -- Up to the upper layer to handle these
     * functionnality restrictions */

    /* Mandatory symbol */
    xvid->global = dlsym(xvid->so, "xvid_global");

    error = dlerror();
    if (xvid->global == NULL && error != NULL) {
        tc_log_warn(MOD_NAME, "Error loading symbol (%s)", error);
        tc_log_warn(MOD_NAME, "Library \"%s\" looks like an old "
                      "version of libxvidcore", soname[i]);
        tc_log_warn(MOD_NAME, "You cannot use this module with this"
                      " lib; maybe -y xvid2 works");
        return -1;
    }

    /* Mandatory symbol */
    xvid->encore = dlsym(xvid->so, "xvid_encore");

    error = dlerror();
    if (xvid->encore == NULL && error != NULL) {
        tc_log_warn(MOD_NAME, "Error loading symbol (%s)", error);
        return -1;
    }

    /* Optional plugin symbols */
    xvid->plugin_onepass     = dlsym(xvid->so, "xvid_plugin_single");
    xvid->plugin_twopass1    = dlsym(xvid->so, "xvid_plugin_2pass1");
    xvid->plugin_twopass2    = dlsym(xvid->so, "xvid_plugin_2pass2");
    xvid->plugin_lumimasking = dlsym(xvid->so, "xvid_plugin_lumimasking");

    return 0;
}

static int unload_xvid(xvid_module_t *xvid)
{
    if (xvid->so != NULL) {
        dlclose(xvid->so);
        memset(xvid, 0, sizeof(xvid[0]));
    }

    return 0;
}

/*
 * Please do not modify the tag line.
 * (still useful? -- fromani 20051231)
 *
 * arch-tag: 16c618a5-6cda-4c95-a418-602fc4837824 export_xvid module
 */
