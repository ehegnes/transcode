/*
 *  multiplex_avi.c - multiplex frames in an AVI file using avilib
 *  (C) Francesco Romani <fromani at gmail dot com> - December 2005
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "transcode.h"
#include "libtc/optstr.h"

#include "libtc/tcmodule-plugin.h"

#include "avilib/avilib.h"

#define MOD_NAME    "multiplex_avi.so"
#define MOD_VERSION "v0.0.1 (2005-12-23)"
#define MOD_CAP     "create an AVI stream using avilib"

static const char *avi_help = ""
    "Overview:\n"
    "\tthis module create an AVI stream using avilib.\n"
    "\tAVI streams produced by this module can have a\n"
    "\tmaximum of one audio and video track.\n"
    "\tYou can add more tracks with further processing.\n"
    "Options:\n"
    "\tHelp\tproduce module overview and options explanations\n";

typedef struct {
    avi_t *avifile;
    int force_kf;
    char conf_str[CONF_STR_SIZE];
} AVIPrivateData;

static const char *avi_configure(TCModuleInstance *self,
                                 const char *options)
{
    vob_t *vob = tc_get_vob();
    const char *fcc = tc_codec_to_string(vob->ex_v_codec);
    AVIPrivateData *pd = NULL;
     
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return NULL;
    }

    pd = self->userdata;
    
    if (optstr_lookup(options, "help")) {
        return avi_help;
    }

    pd->avifile = AVI_open_output_file(vob->video_out_file);
    if(!pd->avifile) {
	    AVI_print_error("avi open error");
        return NULL;
    }
    
    switch (vob->im_v_codec) {
        case CODEC_RGB: /* fallthrough */
        case CODEC_YUV: 
             pd->force_kf = 1;
        default:
             pd->force_kf = 0;
    }
    
	AVI_set_video(pd->avifile, vob->ex_v_width, vob->ex_v_height,
		          vob->ex_fps, fcc);

	AVI_set_audio_track(pd->avifile, vob->a_track);
/*    
    AVI_set_audio(pd->avifile, avi_aud_chan, avi_aud_rate,
                  avi_aud_bits, avi_aud_codec, avi_aud_bitrate);
*/                  
    AVI_set_audio_vbr(pd->avifile, vob->a_vbr);
    
    tc_snprintf(pd->conf_str, CONF_STR_SIZE, "help");
    return pd->conf_str;
}

static int avi_stop(TCModuleInstance *self) 
{
    AVIPrivateData *pd = NULL;
    
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    pd = self->userdata;

    if (pd->avifile) {
        AVI_close(pd->avifile);
        pd->avifile = NULL;
    }

    return TC_EXPORT_OK;
}

static int avi_multiplex(TCModuleInstance *self,
                         vframe_list_t *vframe, aframe_list_t *aframe)
{
    uint32_t size_before, size_after;
    int ret;
 
    AVIPrivateData *pd = NULL;
     
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    pd = self->userdata;
    size_before = AVI_bytes_written(pd->avifile);

    if (vframe != NULL) {
        int key = ((vframe->attributes & TC_FRAME_IS_KEYFRAME) 
                        || pd->force_kf) ?1 :0;

        ret = AVI_write_frame(pd->avifile, vframe->video_buf, 
                              vframe->video_size, key);

        if(ret < 0) {
            AVI_print_error("avi video write error");

            return TC_EXPORT_ERROR;
        }
    }

    if (aframe != NULL) {
 		ret = AVI_write_audio(pd->avifile, aframe->audio_buf, 
                              aframe->audio_size);
 		if (ret < 0) {
			AVI_print_error("AVI file audio write error");
			return TC_EXPORT_ERROR;
		}
    }
    
    size_after = AVI_bytes_written(pd->avifile);
    
    return (size_after - size_before);
}

static int avi_init(TCModuleInstance *self)
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }
    
    self->userdata = tc_malloc(sizeof(AVIPrivateData));
    if (!self->userdata) {
        return TC_EXPORT_ERROR;
    }
        
    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
        if (verbose >= TC_DEBUG) {
            tc_log_info(MOD_NAME, "max AVI-file size limit = %lu bytes",
                                  (unsigned long) AVI_max_size());
        }
    }
    
    return TC_EXPORT_OK;
}
 
static int avi_fini(TCModuleInstance *self)
{
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    avi_stop(self);

    tc_free(self->userdata);
    self->userdata = NULL;

    return TC_EXPORT_OK;
}

/*************************************************************************/

static const int avi_codecs_in[] = { 
    TC_CODEC_PCM, TC_CODEC_PCM, TC_CODEC_AC3, TC_CODEC_A52, TC_CODEC_MP3,
    TC_CODEC_YUV420P, TC_CODEC_DV, TC_CODEC_DIVX3, TC_CODEC_DIVX4, 
    TC_CODEC_DIVX5, TC_CODEC_XVID, TC_CODEC_MJPG, TC_CODEC_LZO1, 
    TC_CODEC_LZO2, TC_CODEC_RGB,
    TC_CODEC_ERROR 
};

/* a multiplexor is at the end of pipeline */
static const int avi_codecs_out[] = { TC_CODEC_ERROR };

static const TCModuleInfo avi_info = {
    .features    = TC_MODULE_FEATURE_MULTIPLEX|TC_MODULE_FEATURE_VIDEO
                   |TC_MODULE_FEATURE_AUDIO,
    .flags       = TC_MODULE_FLAG_RECONFIGURABLE,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = avi_codecs_in,
    .codecs_out  = avi_codecs_out
};

static const TCModuleClass avi_class = {
    .info         = &avi_info,

    .init         = avi_init,
    .fini         = avi_fini,
    .configure    = avi_configure,
    .stop         = avi_stop,
    
    .multiplex    = avi_multiplex,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &avi_class;
}

