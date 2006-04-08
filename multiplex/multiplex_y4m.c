/*
 *  multiplex_y4m.c - pack a yuv420p stream in YUV4MPEG2 format
 *                    and/or a pcm stream in WAVE format
 *  (C) Francesco Romani <fromani at gmail dot com> - March 2006
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "config.h"

#include "transcode.h"
#include "libtc/optstr.h"

#include "libtc/tcmodule-plugin.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#if defined(HAVE_MJPEGTOOLS_INC)
#include "yuv4mpeg.h"
#include "mpegconsts.h"
#else
#include "mjpegtools/yuv4mpeg.h"
#include "mjpegtools/mpegconsts.h"
#endif

#include "wavlib.h"

#define MOD_NAME    "multiplex_y4m.so"
#define MOD_VERSION "v0.0.1 (2006-03-22)"
#define MOD_CAP     "write YUV4MPEG2 video and WAVE audio streams"

#define YW_VID_EXT "y4m"
#define YW_AUD_EXT "wav"

/* 
 * 'yw_' prefix is used internally to avoid any name clash
 * with mjpegtools's y4m_* routines
 */


/* XXX */
static const char *yw_help = ""
    "Overview:\n"
    "\tthis module writes a yuv420p video stream using YUV4MPEG2 format"
    "\tand/or a pcm stream using WAVE format.\n"
    "Options:\n"
    "\thelp\tproduce module overview and options explanations\n";

typedef struct {
    int fd_vid;
    WAV wav;

    y4m_frame_info_t frameinfo;
    y4m_stream_info_t streaminfo;

    int width;
    int height;
    
} YWPrivateData;


/* XXX: this one should go into libtc ASAP */
static void asr_code_to_ratio(int asr, y4m_ratio_t *r)
{
    switch (asr) {
      case 2:
        r->n = 4;
        r->d = 3;
        break;
      case 3: 
        r->n = 16;
        r->d = 9;
        break;
      case 4:
        r->n = 221;
        r->d = 100;
        break;
      case 1:
        r->n = 1;
        r->d = 1;
        break;
      case 0: /* fallthrough */
      default:
        r->n = 0;
        r->d = 0;
        break;
    }
}


static const char *yw_inspect(TCModuleInstance *self,
                               const char *options)
{
    if (!self) {
        tc_log_error(MOD_NAME, "inspect: bad instance data reference");
        return NULL;
    }
    
    if (optstr_lookup(options, "help")) {
        return yw_help;
    }

    return "";
}

static int yw_open_video(YWPrivateData *pd, const char *filename,
                         vob_t *vob)
{
    int asr, ret;
    y4m_ratio_t framerate;
    y4m_ratio_t asr_rate;

    /* avoid fd loss in case of failed configuration */
    if (pd->fd_vid == -1) {
        pd->fd_vid = open(filename,
                          O_RDWR|O_CREAT|O_TRUNC|O_LARGEFILE,
                          S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        if (pd->fd_vid == -1) {
            tc_log_error(MOD_NAME, "failed to open video stream file '%s'"
                                   " (reason: %s)", filename,
                                   strerror(errno));
            return TC_EXPORT_ERROR;
        }
    }
    y4m_init_stream_info(&(pd->streaminfo));

    //note: this is the real framerate of the raw stream
    framerate = (vob->ex_frc == 0) ?mpeg_conform_framerate(vob->ex_fps)
                                   :mpeg_framerate(vob->ex_frc);
    if (framerate.n == 0 && framerate.d == 0) {
    	framerate.n = vob->ex_fps * 1000;
	    framerate.d = 1000;
    }
    
    asr = (vob->ex_asr < 0) ?vob->im_asr :vob->ex_asr;
    asr_code_to_ratio(asr, &asr_rate);

    y4m_init_stream_info(&(pd->streaminfo));
    y4m_si_set_framerate(&(pd->streaminfo), framerate);
    y4m_si_set_interlace(&(pd->streaminfo), vob->encode_fields);
    /* XXX */
    y4m_si_set_sampleaspect(&(pd->streaminfo),
                            y4m_guess_sar(pd->width,
                                          pd->height,
                                          asr_rate));
    /* XXX */
    y4m_si_set_height(&(pd->streaminfo), pd->height);
    y4m_si_set_width(&(pd->streaminfo), pd->width);
    /* Y4M_CHROMA_420JPEG     4:2:0, H/V centered, for JPEG/MPEG-1 */
    /* Y4M_CHROMA_420MPEG2   4:2:0, H cosited, for MPEG-2         */
    /* Y4M_CHROMA_420PALDV   4:2:0, alternating Cb/Cr, for PAL-DV */
    y4m_si_set_chroma(&(pd->streaminfo), Y4M_CHROMA_420JPEG); // XXX
    
    ret = y4m_write_stream_header(pd->fd_vid, &(pd->streaminfo));
    if (ret != Y4M_OK) {
        tc_log_warn(MOD_NAME, "failed to write video YUV4MPEG2 header: %s",
                              y4m_strerr(ret));
        return TC_EXPORT_ERROR;
    }
    return TC_EXPORT_OK;
}

static int yw_open_audio(YWPrivateData *pd, const char *filename,
                         vob_t *vob)
{
    WAVError err;
    int rate;

    pd->wav = wav_open(filename, WAV_WRITE, &err);
    if (!pd->wav) {
        tc_log_error(MOD_NAME, "failed to open audio stream file '%s'"
                               " (reason: %s)", filename,
                               wav_strerror(err));
        return TC_EXPORT_ERROR;
    }

    rate = (vob->mp3frequency != 0) ?vob->mp3frequency :vob->a_rate;
    wav_set_bits(pd->wav, vob->dm_bits);
    wav_set_rate(pd->wav, rate);
    wav_set_bitrate(pd->wav, vob->dm_chan * rate * vob->dm_bits/8);
    wav_set_channels(pd->wav, vob->dm_chan);

    return TC_EXPORT_OK;
}

static int yw_configure(TCModuleInstance *self,
                         const char *options, vob_t *vob)
{
    char vid_name[PATH_MAX];
    char aud_name[PATH_MAX];
    YWPrivateData *pd = NULL;
    int ret;

    if (!self) {
        tc_log_error(MOD_NAME, "configure: bad instance data reference");
        return TC_EXPORT_ERROR;
    }
    pd = self->userdata;

    if (vob->audio_out_file == NULL
      || !strcmp(vob->audio_out_file, "/dev/null")) {
        /* use affine names */
        tc_snprintf(vid_name, PATH_MAX, "%s.%s",
                    vob->video_out_file, YW_VID_EXT);
        tc_snprintf(aud_name, PATH_MAX, "%s.%s",
                    vob->video_out_file, YW_AUD_EXT);
    } else {
        /* copy names verbatim */
        strlcpy(vid_name, vob->video_out_file, PATH_MAX);
        strlcpy(aud_name, vob->audio_out_file, PATH_MAX);
    }
    
    pd->width = vob->ex_v_width;
    pd->height = vob->ex_v_height;
    
    ret = yw_open_video(pd, vid_name, vob);
    if (ret != TC_EXPORT_OK) {
        return ret;
    }
    ret = yw_open_audio(pd, aud_name, vob);
    if (ret != TC_EXPORT_OK) {
        return ret;
    }
    if (vob->verbose >= TC_DEBUG) {
        tc_log_info(MOD_NAME, "video output: %s (%s)",
                    vid_name, (pd->fd_vid == -1) ?"FAILED" :"OK");
        tc_log_info(MOD_NAME, "audio output: %s (%s)",
                    aud_name, (pd->wav == NULL) ?"FAILED" :"OK");
    }
    return TC_EXPORT_OK;
}

static int yw_stop(TCModuleInstance *self)
{
    YWPrivateData *pd = NULL;
    int verr, aerr;

    if (!self) {
        tc_log_error(MOD_NAME, "stop: bad instance data reference");
        return TC_EXPORT_ERROR;
    }
    pd = self->userdata;

    if (pd->fd_vid != -1) {
        verr = close(pd->fd_vid);
        if (verr) {
            tc_log_error(MOD_NAME, "closing video file: %s",
                                   strerror(errno));
            return TC_EXPORT_ERROR;
        }
        y4m_fini_frame_info(&pd->frameinfo);
        y4m_fini_stream_info(&(pd->streaminfo));
   
        pd->fd_vid = -1;
    }

    if (pd->wav != NULL) {
        aerr = wav_close(pd->wav);
        if (aerr != 0) {
            tc_log_error(MOD_NAME, "closing audio file: %s",
                                   wav_strerror(wav_last_error(pd->wav)));
            return TC_EXPORT_ERROR;
        }
        pd->wav = NULL;
    }

    return TC_EXPORT_OK;
}

static int yw_multiplex(TCModuleInstance *self,
                         vframe_list_t *vframe, aframe_list_t *aframe)
{
    ssize_t w_aud = 0, w_vid = 0;

    YWPrivateData *pd = NULL;

    if (!self) {
        tc_log_error(MOD_NAME, "multiplex: bad instance data reference");
        return TC_EXPORT_ERROR;
    }
    pd = self->userdata;

    if (vframe != NULL) {
        uint8_t *planes[3];
        int ret = 0;
        y4m_init_frame_info(&pd->frameinfo);
        YUV_INIT_PLANES(planes, vframe->video_buf, IMG_YUV420P,
                        pd->width, pd->height);
        
        ret = y4m_write_frame(pd->fd_vid, &(pd->streaminfo),
                                 &pd->frameinfo, planes);
        if (ret != Y4M_OK) {
            tc_log_warn(MOD_NAME, "error while writing video frame: %s",
                                  y4m_strerr(ret));
            return TC_EXPORT_ERROR;
        }
        w_vid = vframe->video_size;
    }

    if (aframe != NULL) {
        w_aud = wav_write_data(pd->wav, aframe->audio_buf, aframe->audio_size);
        if (w_aud != aframe->audio_size) {
            tc_log_warn(MOD_NAME, "error while writing audio frame: %s",
                                  wav_strerror(wav_last_error(pd->wav)));
            return TC_EXPORT_ERROR;
        }
    }

    return (int)(w_vid + w_aud);
}

static int yw_init(TCModuleInstance *self)
{
    YWPrivateData *pd = NULL;
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    pd = tc_malloc(sizeof(YWPrivateData));
    if (!pd) {
        return TC_EXPORT_ERROR;
    }

    pd->width = 0;
    pd->height = 0;
    pd->fd_vid = -1;
    pd->wav = NULL;
    y4m_init_stream_info(&(pd->streaminfo));
    /* frameinfo will be initialized at each multiplex call  */

    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }

    self->userdata = pd;
    return 0;
}

static int yw_fini(TCModuleInstance *self)
{
    if (!self) {
        tc_log_error(MOD_NAME, "fini: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    yw_stop(self);

    tc_free(self->userdata);
    self->userdata = NULL;

    return 0;
}


/*************************************************************************/

static const int yw_codecs_in[] = { TC_CODEC_YUV420P, TC_CODEC_PCM,
                                     TC_CODEC_ERROR };

/* a multiplexor is at the end of pipeline */
static const int yw_codecs_out[] = { TC_CODEC_ERROR };

static const TCModuleInfo yw_info = {
    .features    = TC_MODULE_FEATURE_MULTIPLEX|TC_MODULE_FEATURE_VIDEO
                   |TC_MODULE_FEATURE_AUDIO,
    .flags       = TC_MODULE_FLAG_RECONFIGURABLE,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = yw_codecs_in,
    .codecs_out  = yw_codecs_out
};

static const TCModuleClass yw_class = {
    .info         = &yw_info,

    .init         = yw_init,
    .fini         = yw_fini,
    .configure    = yw_configure,
    .stop         = yw_stop,
    .inspect      = yw_inspect,

    .multiplex    = yw_multiplex,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &yw_class;
}

