/*
 * filter_doublefps.c -- double frame rate by deinterlacing fields into frames
 * Written by Andrew Church <achurch@achurch.org>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#define MOD_NAME        "filter_doublefps.so"
#define MOD_VERSION     "v1.0 (2006-05-02)"
#define MOD_CAP         "double frame rate by deinterlacing fields into frames"
#define MOD_AUTHOR      "Andrew Church"

#include "transcode.h"
#include "filter.h"
#include "libtc/libtc.h"
#include "libtc/optstr.h"
#include "libtc/tcmodule-plugin.h"
#include "libtcvideo/tcvideo.h"
#include "aclib/ac.h"

/*************************************************************************/

typedef struct {
    int topfirst;           // Top field first?
    int hq;                 // High-quality mode
    TCVHandle tcvhandle;    // For tcv_zoom() when shifting
    int deinter_handle;     // For high-quality mode
    int saved_audio_len;    // Number of bytes of audio saved for second field
    uint8_t saved_audio[SIZE_PCM_FRAME];
    uint8_t saved_field[TC_MAX_V_FRAME_WIDTH*(TC_MAX_V_FRAME_HEIGHT/2)*3];
} PrivateData;

#define HQ_OFF          0   // HQ mode off
#define HQ_SHIFT_ONE    1   // Shift second frame to match first frame
#define HQ_SHIFT_BOTH   2   // Shift both frames half a line by zooming

/*************************************************************************/
/*************************************************************************/

/* Module interface routines and data. */

/*************************************************************************/

/**
 * doublefps_init:  Initialize this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int doublefps_init(TCModuleInstance *self)
{
    PrivateData *pd;
    vob_t *vob = tc_get_vob();

    if (!self) {
        tc_log_error(MOD_NAME, "init: self == NULL!");
        return -1;
    }

    self->userdata = pd = tc_malloc(sizeof(PrivateData));
    if (!pd) {
        tc_log_error(MOD_NAME, "init: out of memory!");
        return -1;
    }
    pd->topfirst = (vob->im_v_height == 480 ? 0 : 1);
    pd->hq = 0;

    pd->tcvhandle = tcv_init();
    if (!pd->tcvhandle) {
        tc_log_error(MOD_NAME, "init: tcv_init() failed");
        free(pd);
        self->userdata = NULL;
        return -1;
    }

    /* FIXME: we need a proper way for filters to tell the core that
     * they're changing the export parameters */
    vob->ex_v_height /= 2;
    if (vob->encode_fields == 1 || vob->encode_fields == 2) {
        pd->topfirst = (vob->encode_fields == 1) ? 1 : 0;
        if (vob->export_attributes & TC_EXPORT_ATTRIBUTE_FIELDS) {
            tc_log_warn(MOD_NAME, "Use \"-J doublefps=topfirst=%d\","
                        " not \"--encode_fields %c\"", pd->topfirst,
                        vob->encode_fields == 1 ? 't' : 'b');
        }
    }
    vob->encode_fields = 0;
    vob->export_attributes |= TC_EXPORT_ATTRIBUTE_FIELDS;
    if (!(vob->export_attributes
          & (TC_EXPORT_ATTRIBUTE_FPS | TC_EXPORT_ATTRIBUTE_FRC))
    ) {
        vob->ex_fps *= 2;
        switch (vob->ex_frc) {
            case  3: vob->ex_frc =  6; break;
            case  4: vob->ex_frc =  7; break;
            case  5: vob->ex_frc =  8; break;
            case 10: vob->ex_frc = 11; break;
            case 12: vob->ex_frc =  2; break;
            case 13: vob->ex_frc =  5; break;
            default: vob->ex_frc =  0; break;
        }
    }
    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }
    return 0;
}

/*************************************************************************/

/**
 * doublefps_fini:  Clean up after this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int doublefps_fini(TCModuleInstance *self)
{
    PrivateData *pd;

    if (!self) {
       return -1;
    }
    pd = self->userdata;

    if (pd->tcvhandle) {
        tcv_free(pd->tcvhandle);
        pd->tcvhandle = 0;
    }

    tc_free(self->userdata);
    self->userdata = NULL;
    return 0;
}

/*************************************************************************/

/**
 * doublefps_configure:  Configure this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int doublefps_configure(TCModuleInstance *self,
                               const char *options, vob_t *vob)
{
    PrivateData *pd;
    if (!self) {
       return -1;
    }
    pd = self->userdata;

    pd->topfirst = (vob->im_v_height == 480 ? 0 : 1);
    pd->hq = 0;
    if (options) {
        if (optstr_get(options, "shiftEven", "%d", &pd->topfirst) == 1) {
            tc_log_warn(MOD_NAME, "The \"shiftEven\" option name is obsolete;"
                        " please use \"topfirst\" instead.");
        }
        optstr_get(options, "topfirst", "%d", &pd->topfirst);
        optstr_get(options, "hq", "%d", &pd->hq);
    }
    return 0;
}

/*************************************************************************/

/**
 * doublefps_stop:  Reset this instance of the module.  See tcmodule-data.h
 * for function details.
 */

static int doublefps_stop(TCModuleInstance *self)
{
    return 0;
}

/*************************************************************************/

/**
 * doublefps_inspect:  Return the value of an option in this instance of
 * the module.  See tcmodule-data.h for function details.
 */

static const char *doublefps_inspect(TCModuleInstance *self, const char *param)
{
    PrivateData *pd;
    static char buf[TC_BUF_MAX];

    if (!self || !param)
       return NULL;
    pd = self->userdata;

    if (optstr_lookup(param, "help")) {
        return
"Overview:\n"
"\n"
"    Converts interlaced video into progressive video with half the\n"
"    original height and twice the speed (FPS), by converting each\n"
"    interlaced field to a separate frame.  Optionally allows the two\n"
"    fields to be shifted by half a pixel each to line them up correctly\n"
"    (at a significant expense of time).\n"
"\n"
"    When using this filter, make sure you specify \"--encode_fields p\"\n"
"    on the transcode command line, and do not use the \"-I\" option.\n"
"\n"
"Options available:\n"
"\n"
"    topfirst=0|1   Selects whether the top field is the first displayed.\n"
"                   Defaults to 0 (bottom-first) for 480-line video, 1\n"
"                   (top-first) otherwise.\n"
"\n"
"    hq=1           Selects high-quality mode.  This causes both fields to\n"
"                   be shifted half a pixel toward each other, removing the\n"
"                   \"jitter\" caused by the two fields being vertically\n"
"                   offset from each other and reducing encoding noise.\n"
"                   However, this carries a significant speed penalty;\n"
"                   in addition, the half-pixel shift can cause a minor\n"
"                   loss of detail.\n";
    }
    if (optstr_lookup(param, "topfirst")) {
        tc_snprintf(buf, sizeof(buf), "%d", pd->topfirst);
        return buf;
    }
    if (optstr_lookup(param, "hq")) {
        tc_snprintf(buf, sizeof(buf), "%d", pd->hq);
        return buf;
    }
    return "";
}

/*************************************************************************/

/**
 * doublefps_filter_video:  Perform the FPS-doubling operation on the video
 * stream.  See tcmodule-data.h for function details.
 */

static int doublefps_filter_video(TCModuleInstance *self, vframe_list_t *frame)
{
    PrivateData *pd;
    uint8_t *oldbuf[3], *newbuf[3], *savebuf[3];
    int w, h, hUV;
    TCVDeinterlaceMode dropfirst, dropsecond;

    if (!self || !frame) {
        tc_log_error(MOD_NAME, "filter_video: %s == NULL!",
                     !self ? "self" : "frame");
        return -1;
    }
    pd = self->userdata;
    w = frame->v_width;
    h = frame->v_height;
    hUV = (frame->v_codec == CODEC_YUV422) ? h : h/2;

    /* If this is the second field, the new frame is already saved in our
     * private data structure, so copy it over and return. */
    if (frame->attributes & TC_FRAME_WAS_CLONED) {
        ac_memcpy(frame->video_buf, pd->saved_field, w*h + (w/2)*hUV*2);
        frame->attributes &= ~TC_FRAME_IS_INTERLACED;
        return 0;
    }

    /* The remainder is processing for the first field. */

    if (pd->topfirst) {
        dropfirst  = TCV_DEINTERLACE_DROP_FIELD_BOTTOM;
        dropsecond = TCV_DEINTERLACE_DROP_FIELD_TOP;
    } else {
        dropfirst  = TCV_DEINTERLACE_DROP_FIELD_TOP;
        dropsecond = TCV_DEINTERLACE_DROP_FIELD_BOTTOM;
    }

    oldbuf[0]  = frame->video_buf;
    oldbuf[1]  = oldbuf[0] + w * h;
    oldbuf[2]  = oldbuf[1] + (w/2) * hUV;
    newbuf[0]  = frame->video_buf_Y[frame->free];
    newbuf[1]  = newbuf[0] + w * (h/2);
    newbuf[2]  = newbuf[1] + (w/2) * (hUV/2);
    savebuf[0] = pd->saved_field;
    savebuf[1] = savebuf[0] + w * (h/2);
    savebuf[2] = savebuf[1] + (w/2) * (hUV/2);

    /* Deinterlace the fields into separate frames, and save the second
     * frame for the next time we're called. */
    if (!tcv_deinterlace(pd->tcvhandle, oldbuf[0], newbuf[0], w, h, 1,
                         dropfirst)
     || !tcv_deinterlace(pd->tcvhandle, oldbuf[1], newbuf[1], w/2, hUV, 1,
                         dropfirst)
     || !tcv_deinterlace(pd->tcvhandle, oldbuf[2], newbuf[2], w/2, hUV, 1,
                         dropfirst)
     || !tcv_deinterlace(pd->tcvhandle, oldbuf[0], savebuf[0], w, h, 1,
                         dropsecond)
     || !tcv_deinterlace(pd->tcvhandle, oldbuf[1], savebuf[1], w/2, hUV, 1,
                         dropsecond)
     || !tcv_deinterlace(pd->tcvhandle, oldbuf[2], savebuf[2], w/2, hUV, 1,
                         dropsecond)
    ) {
        tc_log_warn(MOD_NAME, "tcv_deinterlace() failed!");
        return -1;
    }

    if (pd->hq == 1) {
        /* Zoom out to full (original) height, then drop the opposite field
         * to make the frames line up properly.  Use oldbuf as a temporary
         * buffer, since it's no longer needed. */
        /* Note that we need a separate set of pointers for newbuf[] in the
         * second set of zooms, because the current set assumes half height. */
        char *newbuf2[3];
        newbuf2[0] = newbuf[0];
        newbuf2[1] = newbuf2[0] + w * h;
        newbuf2[2] = newbuf2[1] + (w/2) * hUV;
        if (!tcv_zoom(pd->tcvhandle, newbuf[0], oldbuf[0], w, h/2, 1,
                      w, h, tc_get_vob()->zoom_filter)
         || !tcv_zoom(pd->tcvhandle, newbuf[1], oldbuf[1], w/2, hUV/2, 1,
                      w/2, hUV, tc_get_vob()->zoom_filter)
         || !tcv_zoom(pd->tcvhandle, newbuf[2], oldbuf[2], w/2, hUV/2, 1,
                      w/2, hUV, tc_get_vob()->zoom_filter)
         || !tcv_zoom(pd->tcvhandle, savebuf[0], newbuf2[0], w, h/2, 1,
                      w, h, tc_get_vob()->zoom_filter)
         || !tcv_zoom(pd->tcvhandle, savebuf[1], newbuf2[1], w/2, hUV/2, 1,
                      w/2, hUV, tc_get_vob()->zoom_filter)
         || !tcv_zoom(pd->tcvhandle, savebuf[2], newbuf2[2], w/2, hUV/2, 1,
                      w/2, hUV, tc_get_vob()->zoom_filter)
        ) {
            tc_log_warn(MOD_NAME, "tcv_zoom() failed!");
            return -1;
        }
        if (!tcv_deinterlace(pd->tcvhandle, newbuf2[0], savebuf[0],
                             w, h, 1, dropfirst)
         || !tcv_deinterlace(pd->tcvhandle, newbuf2[1], savebuf[1],
                             w/2, hUV, 1, dropfirst)
         || !tcv_deinterlace(pd->tcvhandle, newbuf2[2], savebuf[2],
                             w/2, hUV, 1, dropfirst)
         || !tcv_deinterlace(pd->tcvhandle, oldbuf[0], newbuf[0],
                             w, h, 1, dropsecond)
         || !tcv_deinterlace(pd->tcvhandle, oldbuf[1], newbuf[1],
                             w/2, hUV, 1, dropsecond)
         || !tcv_deinterlace(pd->tcvhandle, oldbuf[2], newbuf[2],
                             w/2, hUV, 1, dropsecond)
        ) {
            tc_log_warn(MOD_NAME, "tcv_deinterlace() failed!");
            return -1;
        }
    }  // if (pd->hq)

    frame->attributes |= TC_FRAME_IS_CLONED;
    frame->attributes &= ~TC_FRAME_IS_INTERLACED;
    frame->v_height /= 2;
    frame->video_buf = newbuf[0];
    frame->free = (frame->free==0) ? 1 : 0;
    return 0;
}

/*************************************************************************/

/**
 * doublefps_filter_audio:  Perform the FPS-doubling operation on the audio
 * stream.  See tcmodule-data.h for function details.
 */

static int doublefps_filter_audio(TCModuleInstance *self, aframe_list_t *frame)
{
    PrivateData *pd;

    if (!self || !frame) {
        tc_log_error(MOD_NAME, "filter_audio: %s == NULL!",
                     !self ? "self" : "frame");
        return -1;
    }
    pd = self->userdata;

    if (!(frame->attributes & TC_FRAME_WAS_CLONED)) {
        /* First field */
        int bps = frame->a_chan * frame->a_bits / 8;
        int nsamples = frame->audio_size / bps;
        int nsamples_first = (nsamples+1) / 2;  // put odd sample in 1st frame
        int nsamples_second = nsamples - nsamples_first;

        frame->attributes |= TC_FRAME_IS_CLONED;
        frame->audio_size = nsamples_first * bps;
        pd->saved_audio_len = nsamples_second * bps;
        if (pd->saved_audio_len > 0) {
            ac_memcpy(pd->saved_audio, frame->audio_buf + frame->audio_size,
                      pd->saved_audio_len);
        }
    } else {
        /* Second frame */
        frame->audio_size = pd->saved_audio_len;
        if (pd->saved_audio_len > 0)
            ac_memcpy(frame->audio_buf, pd->saved_audio, pd->saved_audio_len);
    }

    return 0;
}

/*************************************************************************/

static const int doublefps_codecs_in[] =
    { TC_CODEC_YUV420P, TC_CODEC_YUV422P, TC_CODEC_PCM, TC_CODEC_ERROR };
static const int doublefps_codecs_out[] =
    { TC_CODEC_YUV420P, TC_CODEC_YUV422P, TC_CODEC_PCM, TC_CODEC_ERROR };

static const TCModuleInfo doublefps_info = {
    .features    = TC_MODULE_FEATURE_FILTER
                 | TC_MODULE_FEATURE_VIDEO
                 | TC_MODULE_FEATURE_AUDIO,
    .flags       = TC_MODULE_FLAG_RECONFIGURABLE,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = doublefps_codecs_in,
    .codecs_out  = doublefps_codecs_out
};

static const TCModuleClass doublefps_class = {
    .info         = &doublefps_info,

    .init         = doublefps_init,
    .fini         = doublefps_fini,
    .configure    = doublefps_configure,
    .stop         = doublefps_stop,
    .inspect      = doublefps_inspect,

    .filter_video = doublefps_filter_video,
    /* We have to handle the audio too! */
    .filter_audio = doublefps_filter_audio,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &doublefps_class;
}

/*************************************************************************/
/*************************************************************************/

/* Old-fashioned module interface. */

static TCModuleInstance mod;

/*************************************************************************/

int tc_filter(frame_list_t *frame, char *options)
{
    if (frame->tag & TC_FILTER_INIT) {
        if (doublefps_init(&mod) < 0)
            return -1;
        return doublefps_configure(&mod, options, tc_get_vob());

    } else if (frame->tag & TC_FILTER_GET_CONFIG) {
        tc_snprintf(options, ARG_CONFIG_LEN, "%s",
                    doublefps_inspect(&mod, "help"));
        return 0;

    } else if (frame->tag & TC_POST_S_PROCESS) {
        if (frame->tag & TC_VIDEO)
            return doublefps_filter_video(&mod, (vframe_list_t *)frame);
        else if (frame->tag & TC_AUDIO)
            return doublefps_filter_audio(&mod, (aframe_list_t *)frame);
        else
            return 0;

    } else if (frame->tag & TC_FILTER_CLOSE) {
        return doublefps_fini(&mod);

    }

    return 0;
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
