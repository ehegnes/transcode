/*
 *  multiplex_raw.c - write a separate plain file for each stream
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

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#define MOD_NAME    "multiplex_raw.so"
#define MOD_VERSION "v0.0.3 (2006-03-06)"
#define MOD_CAP     "write each stream in a separate file"

#define RAW_VID_EXT "vid"
#define RAW_AUD_EXT "aud"

static const char *raw_help = ""
    "Overview:\n"
    "    this module simply write audio and video streams in\n"
    "    a separate plain file for each stream.\n"
    "Options:\n"
    "    help    produce module overview and options explanations\n";

typedef struct {
    int fd_aud;
    int fd_vid;
} RawPrivateData;

static const char *raw_inspect(TCModuleInstance *self,
                               const char *options)
{
    if (!self) {
        tc_log_error(MOD_NAME, "inspect: bad instance data reference");
        return NULL;
    }
    
    if (optstr_lookup(options, "help")) {
        return raw_help;
    }

    return "";
}

static int raw_configure(TCModuleInstance *self,
                         const char *options, vob_t *vob)
{
    char vid_name[PATH_MAX];
    char aud_name[PATH_MAX];
    RawPrivateData *pd = NULL;

    if (!self) {
        tc_log_error(MOD_NAME, "configure: bad instance data reference");
        return TC_EXPORT_ERROR;
    }
    pd = self->userdata;

    // XXX
    if (vob->audio_out_file == NULL
      || !strcmp(vob->audio_out_file, "/dev/null")) {
        /* use affine names */
        tc_snprintf(vid_name, PATH_MAX, "%s.%s",
                    vob->video_out_file, RAW_VID_EXT);
        tc_snprintf(aud_name, PATH_MAX, "%s.%s",
                    vob->video_out_file, RAW_AUD_EXT);
    } else {
        /* copy names verbatim */
        strlcpy(vid_name, vob->video_out_file, PATH_MAX);
        strlcpy(aud_name, vob->audio_out_file, PATH_MAX);
    }
    
    /* avoid fd loss in case of failed configuration */
    if (pd->fd_vid == -1) {
        pd->fd_vid = open(vid_name,
                          O_RDWR|O_CREAT|O_TRUNC,
                          S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        if (pd->fd_vid == -1) {
            tc_log_error(MOD_NAME, "failed to open video stream file");
            return TC_EXPORT_ERROR;
        }
    }

    /* avoid fd loss in case of failed configuration */
    if (pd->fd_aud == -1) {
        pd->fd_aud = open(aud_name,
                          O_RDWR|O_CREAT|O_TRUNC,
                          S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        if (pd->fd_aud == -1) {
            tc_log_error(MOD_NAME, "failed to open audio stream file");
            return TC_EXPORT_ERROR;
        }
    }
    if (vob->verbose >= TC_DEBUG) {
        tc_log_info(MOD_NAME, "video output: %s (%s)",
                    vid_name, (pd->fd_vid == -1) ?"FAILED" :"OK");
        tc_log_info(MOD_NAME, "audio output: %s (%s)",
                    aud_name, (pd->fd_aud == -1) ?"FAILED" :"OK");
    }
    return TC_EXPORT_OK;
}

static int raw_stop(TCModuleInstance *self)
{
    RawPrivateData *pd = NULL;
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
        pd->fd_vid = -1;
    }

    if (pd->fd_aud != -1) {
        aerr = close(pd->fd_aud);
        if (aerr) {
            tc_log_error(MOD_NAME, "closing audio file: %s",
                                   strerror(errno));
            return TC_EXPORT_ERROR;
        }
        pd->fd_aud = -1;
    }

    return 0;
}

static int raw_multiplex(TCModuleInstance *self,
                         vframe_list_t *vframe, aframe_list_t *aframe)
{
    ssize_t w_aud = 0, w_vid = 0;

    RawPrivateData *pd = NULL;

    if (!self) {
        tc_log_error(MOD_NAME, "multiplex: bad instance data reference");
        return TC_EXPORT_ERROR;
    }
    pd = self->userdata;

    if (vframe != NULL) {
        w_vid = tc_pwrite(pd->fd_vid, vframe->video_buf, vframe->video_size);
        if(w_vid < 0) {
            return TC_EXPORT_ERROR;
        }
    }

    if (aframe != NULL) {
        w_aud = tc_pwrite(pd->fd_aud, aframe->audio_buf, aframe->audio_size);
 		if (w_aud < 0) {
			return TC_EXPORT_ERROR;
		}
    }

    return (int)(w_vid + w_aud);
}

static int raw_init(TCModuleInstance *self)
{
    RawPrivateData *pd = NULL;
    if (!self) {
        tc_log_error(MOD_NAME, "init: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    pd = tc_malloc(sizeof(RawPrivateData));
    if (!pd) {
        return TC_EXPORT_ERROR;
    }

    pd->fd_aud = -1;
    pd->fd_vid = -1;

    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }

    self->userdata = pd;
    return 0;
}

static int raw_fini(TCModuleInstance *self)
{
    if (!self) {
        tc_log_error(MOD_NAME, "fini: bad instance data reference");
        return TC_EXPORT_ERROR;
    }

    raw_stop(self);

    tc_free(self->userdata);
    self->userdata = NULL;

    return 0;
}


/*************************************************************************/

static const int raw_codecs_in[] = { TC_CODEC_ANY, TC_CODEC_ERROR };

/* a multiplexor is at the end of pipeline */
static const int raw_codecs_out[] = { TC_CODEC_ERROR };

static const TCModuleInfo raw_info = {
    .features    = TC_MODULE_FEATURE_MULTIPLEX|TC_MODULE_FEATURE_VIDEO
                   |TC_MODULE_FEATURE_AUDIO,
    .flags       = TC_MODULE_FLAG_RECONFIGURABLE,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = raw_codecs_in,
    .codecs_out  = raw_codecs_out
};

static const TCModuleClass raw_class = {
    .info         = &raw_info,

    .init         = raw_init,
    .fini         = raw_fini,
    .configure    = raw_configure,
    .stop         = raw_stop,
    .inspect      = raw_inspect,

    .multiplex    = raw_multiplex,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &raw_class;
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

