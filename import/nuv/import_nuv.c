/*
 * import_nuv.c -- NuppelVideo import module
 * Written by Andrew Church <achurch@achurch.org>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "transcode.h"
#include "libtc/libtc.h"
#include "libtc/optstr.h"
#include "libtc/tcmodule-plugin.h"
#include "aclib/ac.h"
#include "nuppelvideo.h"
#include "RTjpegN.h"
#include "libtc/tc_lzo.h"

#define MOD_NAME        "import_nuv.so"
#define MOD_VERSION     "v0.9 (2006-06-03)"
#define MOD_CAP         "Imports NuppelVideo streams"
#define MOD_AUTHOR      "Andrew Church"

/*************************************************************************/

/* Private data used by this module. */
typedef struct {
    int fd;              // File descriptor to read from
    int width, height;   // Video dimensions
    double fps;          // Nominal frames per second
    int framenum;        // Frame number of loaded frame
    int have_vframe;     // Do we have a video frame stored?
    double audiorate;    // Actual audio rate (from SA frame)
    double audiofrac;    // Saved fractional position (for resampling)
    uint32_t cdata[128]; // Compressor data (from DR frame)
    int dec_initted;     // Decompressor initted?

    // Previous video frame, for frame cloning
    uint8_t saved_vframe[TC_MAX_V_FRAME_WIDTH*TC_MAX_V_FRAME_HEIGHT*3];
    int saved_vframelen;
    uint8_t saved_vcomptype;
    struct rtframeheader framehdr;  // Next video frame header
} PrivateData;

/* NuppelVideo always uses 44100 sps */
#define NUV_ARATE   44100

/*************************************************************************/
/*************************************************************************/

/* Module interface routines and data. */

/*************************************************************************/

/**
 * nuv_init:  Initialize this instance of the module.  See tcmodule-data.h
 * for function details.
 */

static int nuv_init(TCModuleInstance *self)
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
    pd->fd = -1;
    pd->dec_initted = 0;

    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }

    return 0;
}

/*************************************************************************/

/**
 * nuv_fini:  Clean up after this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int nuv_fini(TCModuleInstance *self)
{
    PrivateData *pd;

    if (!self) {
       return -1;
    }
    pd = self->userdata;

    if (pd->fd) {
        close(pd->fd);
        pd->fd = -1;
    }

    tc_free(self->userdata);
    self->userdata = NULL;
    return 0;
}

/*************************************************************************/

/**
 * nuv_configure:  Configure this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int nuv_configure(TCModuleInstance *self,
                         const char *options, vob_t *vob)
{
    PrivateData *pd;
    struct rtfileheader hdr;
    const char *filename = vob->video_in_file;

    if (!self) {
       return -1;
    }
    pd = self->userdata;

    // FIXME: is this a good place for open()?  And how do we know which
    // file (video or audio) to open?
    pd->fd = open(filename, O_RDONLY);
    if (pd->fd < 0) {
        tc_log_error(MOD_NAME, "Unable to open %s: %s", filename,
                     strerror(errno));
        return 0;
    }
    if (read(pd->fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
        tc_log_error(MOD_NAME, "Unable to read file header from %s", filename);
        close(pd->fd);
        pd->fd = -1;
        return 0;
    }
    if (strcmp(hdr.finfo, "NuppelVideo") != 0) {
        tc_log_error(MOD_NAME, "Bad file header in %s", filename);
        close(pd->fd);
        pd->fd = -1;
        return 0;
    }
    if (strcmp(hdr.version, "0.05") != 0) {
        tc_log_error(MOD_NAME, "Bad format version in %s", filename);
        close(pd->fd);
        pd->fd = -1;
        return 0;
    }
    pd->width = hdr.width;
    pd->height = hdr.height;
    pd->fps = hdr.fps;
    pd->framenum = 0;
    pd->have_vframe = 0;
    pd->audiorate = NUV_ARATE;
    pd->audiofrac = 0;
    memset(pd->cdata, 0, sizeof(pd->cdata));
    pd->saved_vframelen = 0;
    pd->saved_vcomptype = 'N';  // black frame

    return 0;
}

/*************************************************************************/

/**
 * nuv_stop:  Reset this instance of the module.  See tcmodule-data.h for
 * function details.
 */

static int nuv_stop(TCModuleInstance *self)
{
    PrivateData *pd;

    if (!self) {
       return -1;
    }
    pd = self->userdata;

    if (pd->fd >= 0) {
        close(pd->fd);
        pd->fd = -1;
    }
    pd->dec_initted = 0;

    return 0;
}

/*************************************************************************/

/**
 * nuv_inspect:  Return the value of an option in this instance of the
 * module.  See tcmodule-data.h for function details.
 */

static int nuv_inspect(TCModuleInstance *self,
                       const char *param, const char **value)
{
    PrivateData *pd;
    static char buf[TC_BUF_MAX];

    if (!self || !param)
       return 0;
    pd = self->userdata;

    if (optstr_lookup(param, "help")) {
        tc_snprintf(buf, sizeof(buf),
                    "Overview:\n"
                    "    Decodes NuppelVideo streams.\n"
                    "Options available: None.\n");
       *value = buf;
    }
    return TC_IMPORT_OK;
}

/*************************************************************************/

/**
 * nuv_demultiplex:  Demultiplex a frame of data.  See tcmodule-data.h for
 * function details.
 */

static int nuv_demultiplex(TCModuleInstance *self,
                           vframe_list_t *vframe, aframe_list_t *aframe)
{
    PrivateData *pd;
    double timestamp;
    uint8_t *audiobuf = NULL;
    int audiolen = 0;

    if (!self) {
        tc_log_error(MOD_NAME, "demultiplex: self == NULL!");
        return -1;
    }
    pd = self->userdata;
    if (pd->fd < 0) {
        tc_log_error(MOD_NAME, "demultiplex: no file opened!");
        return -1;
    }

    /* Loop reading packets until we have a video frame. */

    while (!pd->have_vframe) {
        struct rtframeheader hdr;

        if (read(pd->fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
            if (verbose & TC_DEBUG)
                tc_log_info(MOD_NAME, "End of file reached");
            tc_free(audiobuf);
            nuv_stop(self);
            return -1;
        }

        /* Check for a compressor data (DR) packet */
        if (hdr.frametype == 'D' && hdr.comptype == 'R') {
            if (hdr.packetlength < sizeof(pd->cdata)) {
                tc_log_warn(MOD_NAME, "Short compressor data packet");
                tc_free(audiobuf);
                nuv_stop(self);
                return -1;
            }
            if (read(pd->fd, pd->cdata, sizeof(pd->cdata))
                != sizeof(pd->cdata)
            ) {
                tc_log_warn(MOD_NAME,
                            "File truncated in compressor data packet");
                tc_free(audiobuf);
                nuv_stop(self);
                return -1;
            }
            hdr.packetlength -= sizeof(pd->cdata);
        }

        /* Check for an audio sync (SA) packet */
        if (hdr.frametype == 'S' && hdr.comptype == 'A') {
            pd->audiorate = (double)hdr.timecode / 100;
        }

        /* Check for a seekpoint (R) packet, and set its length to zero
         * (seekpoint packets don't have a valid length field) */
        if (hdr.frametype == 'R') {
            hdr.packetlength = 0;
        }

        /* Check for and read an audio (A) packet */
        if (hdr.frametype == 'A' && hdr.packetlength > 0) {
            if (hdr.comptype != '0') {
                tc_log_warn(MOD_NAME, "Unsupported audio compression %c",
                            hdr.comptype);
                tc_free(audiobuf);
                nuv_stop(self);
                return -1;
            }
            audiobuf = tc_realloc(audiobuf, audiolen + hdr.packetlength);
            if (!audiobuf) {
                tc_log_error(MOD_NAME, "No memory for audio!");
                nuv_stop(self);
                return -1;
            }
            if (read(pd->fd, audiobuf+audiolen, hdr.packetlength)
                != hdr.packetlength
            ) {
                tc_log_warn(MOD_NAME, "File truncated in audio packet");
                tc_free(audiobuf);
                nuv_stop(self);
                return -1;
            }
            audiolen += hdr.packetlength;
            hdr.packetlength = 0;
        }

        /* Check for a video (V) packet */
        if (hdr.frametype == 'V') {
            memcpy(&pd->framehdr, &hdr, sizeof(hdr));
            pd->have_vframe = 1;
            /* We read the data later, so avoid skipping it now */
            hdr.packetlength = 0;
        }

        /* Skip anything that's not processed above */
        while (hdr.packetlength > 0) {
            char buf[0x1000];
            int toread = hdr.packetlength;
            if (toread > sizeof(buf))
                toread = sizeof(buf);
            if (read(pd->fd, buf, toread) != toread) {
                tc_log_warn(MOD_NAME, "File truncated in skipped packet");
                tc_free(audiobuf);
                nuv_stop(self);
                return -1;
            }
            hdr.packetlength -= toread;
        }

    }  // while (!pd->have_vframe)

    /* Now we have a video packet.  First take care of audio processing. */
    if (aframe) {
        if (pd->audiorate == NUV_ARATE) {
            /* No resampling needed, just copy */
            ac_memcpy(aframe->audio_buf, audiobuf, audiolen);
            aframe->audio_size = audiolen;
        } else {
            /* Resample data to NUV_ARATE samples per second */
            int16_t *in = (int16_t *)audiobuf;
            int16_t *out = (int16_t *)aframe->audio_buf;
            int inpos = 0, outpos = 0;

            while (pd->audiofrac >= 1 && inpos < audiolen/2) {
                inpos += 2;
                pd->audiofrac -= 1;
            }
            while (inpos < audiolen/2) {
                out[outpos  ] = in[ inpos     ] * (1-pd->audiofrac)
                              + in[(inpos+2)  ] *    pd->audiofrac;
                out[outpos+1] = in[ inpos   +1] * (1-pd->audiofrac)
                              + in[(inpos+2)+1] *    pd->audiofrac;
                pd->audiofrac += pd->audiorate / NUV_ARATE;
                while (pd->audiofrac >= 1 && inpos < audiolen/2) {
                    inpos += 2;
                    pd->audiofrac -= 1;
                }
                outpos += 2;
            }
            aframe->audio_size = outpos*2;
        }
        aframe->a_rate = NUV_ARATE;
        aframe->a_bits = 16;
        aframe->a_chan = 2;
        tc_free(audiobuf);
        audiobuf = NULL;
    }

    /* Check the timecode on the video frame and read or clone as needed. */
    timestamp = (double)pd->framehdr.timecode / 1000;
    if (verbose & TC_DEBUG) {
        tc_log_msg(MOD_NAME,"<<< frame=%d[%.3f] timestamp=%.3f >>>",
                   pd->framenum, pd->framenum/pd->fps, timestamp);
    }
    if (timestamp < (pd->framenum+0.5)/pd->fps) {
        if (pd->framehdr.comptype != 'L') {  // 'L'ast frame: keep saved data
            if (pd->framehdr.packetlength > 0) {
                if (read(pd->fd, pd->saved_vframe, pd->framehdr.packetlength) 
                    != pd->framehdr.packetlength
                    ) {
                    tc_log_warn(MOD_NAME, "File truncated in video packet");
                    nuv_stop(self);
                    return -1;
                }
            }
            pd->saved_vframelen = pd->framehdr.packetlength;
            pd->saved_vcomptype = pd->framehdr.comptype;
        }
        pd->have_vframe = 0;
    } else {
        if (verbose & TC_DEBUG) {
            tc_log_warn(MOD_NAME, "(frame %d) Dropped frame(s) or bad A/V"
                        " sync, cloning last frame", pd->framenum);
        }
    }

    /* Copy the video frame to the destination buffer and return. */
    if (vframe) {
        vframe->video_buf[0] = pd->width>>8;
        vframe->video_buf[1] = pd->width;
        vframe->video_buf[2] = pd->height>>8;
        vframe->video_buf[3] = pd->height;
        vframe->video_buf[4] = pd->saved_vcomptype;
        ac_memcpy(vframe->video_buf+5, pd->cdata, sizeof(pd->cdata));
        ac_memcpy(vframe->video_buf+5+sizeof(pd->cdata), pd->saved_vframe,
                  pd->saved_vframelen);
        vframe->video_size = pd->saved_vframelen+5+sizeof(pd->cdata);
        vframe->v_codec = TC_CODEC_NUV;
    }
    pd->framenum++;
    return 0;
}

/*************************************************************************/

/**
 * nuv_decode_video:  Decode a frame of data.  See tcmodule-data.h for
 * function details.
 */

static int nuv_decode_video(TCModuleInstance *self,
                            vframe_list_t *inframe, vframe_list_t *outframe)
{
    PrivateData *pd;
    uint8_t comptype, *encoded_frame;
    int in_framesize, out_framesize;
    int free_frame = 0;  // set to 1 if we need to free the decompress buffer

    if (!self || !inframe || !outframe) {
        tc_log_error(MOD_NAME, "decode_video: NULL parameter(s)!");
        return -1;
    }
    pd = self->userdata;

    if (!pd->dec_initted) {
        pd->width  = inframe->video_buf[0]<<8 | inframe->video_buf[1];
        pd->height = inframe->video_buf[2]<<8 | inframe->video_buf[3];
        RTjpeg_init_decompress((unsigned long *)(inframe->video_buf+5),
                               pd->width, pd->height);
        pd->dec_initted = 1;
    }

    comptype = inframe->video_buf[4];
    encoded_frame = inframe->video_buf+5+sizeof(pd->cdata);
    in_framesize = inframe->video_size-5-sizeof(pd->cdata);
    out_framesize = pd->width*pd->height + (pd->width/2)*(pd->height/2)*2;

    if (comptype == '2' || comptype == '3') {
        /* Undo LZO compression */
        uint8_t *decompressed_frame;
        lzo_uint len;
        decompressed_frame = tc_malloc(out_framesize);
        if (!decompressed_frame) {
            tc_log_error(MOD_NAME, "No memory for decompressed frame!");
            return -1;
        }
        if (lzo1x_decompress(encoded_frame, in_framesize,
                             decompressed_frame, &len, NULL) == LZO_E_OK) {
            encoded_frame = decompressed_frame;
            in_framesize = len;
            free_frame = 1;
        } else {
            tc_log_warn(MOD_NAME, "Unable to decompress video frame");
            tc_free(decompressed_frame);
            /* And try it as raw, just like rtjpeg_vid_plugin */
        }
        /* Convert 2 -> 1, 3 -> 0 */
        comptype ^= 3;
    }

    switch (comptype) {

      case '0':  // Uncompressed YUV
        if (in_framesize > out_framesize)
            in_framesize = out_framesize;
        ac_memcpy(outframe->video_buf, encoded_frame, in_framesize);
        break;

      case '1':  // RTjpeg-compressed data
        RTjpeg_decompressYUV420((__s8 *)encoded_frame, outframe->video_buf);
        break;

      case 'N':  // Black frame
        memset(outframe->video_buf, 0, pd->width*pd->height);
        memset(outframe->video_buf + pd->width*pd->height, 128,
               (pd->width/2)*(pd->height/2)*2);
        break;

      case 'L':  // Repeat last frame--leave videobuf alone
        // Should have been handled by demux!
        tc_log_warn(MOD_NAME, "BUG: 'L' frame not handled!");
        break;

      default:
        tc_log_warn(MOD_NAME, "Unknown video compression type %c (%02X)",
                    comptype >= ' ' && comptype <= '=' ? comptype : '?',
                    comptype);
        break;

    }  // switch (comptype)

    if (free_frame)
        tc_free(encoded_frame);
    outframe->video_size = out_framesize;
    return 0;
}

/*************************************************************************/

static const TCCodecID nuv_codecs_in[] = { TC_CODEC_NUV, TC_CODEC_ERROR };
static const TCCodecID nuv_codecs_out[] = { TC_CODEC_YUV420P, TC_CODEC_ERROR };

static const TCFormatID nuv_formats_in[] = { TC_FORMAT_NUV, TC_FORMAT_ERROR };
static const TCFormatID nuv_formats_out[] = { TC_FORMAT_ERROR };

static const TCModuleInfo nuv_info = {
    .features    = TC_MODULE_FEATURE_DEMULTIPLEX
                 | TC_MODULE_FEATURE_DECODE
                 | TC_MODULE_FEATURE_VIDEO,
    .flags       = TC_MODULE_FLAG_RECONFIGURABLE,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = nuv_codecs_in,
    .codecs_out  = nuv_codecs_out,
    .formats_in  = nuv_formats_in,
    .formats_out = nuv_formats_out
};

static const TCModuleClass nuv_class = {
    .info         = &nuv_info,

    .init         = nuv_init,
    .fini         = nuv_fini,
    .configure    = nuv_configure,
    .stop         = nuv_stop,
    .inspect      = nuv_inspect,

    .decode_video = nuv_decode_video,
    .demultiplex  = nuv_demultiplex,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &nuv_class;
}

/*************************************************************************/
/*************************************************************************/

/* Old-fashioned module interface. */

#define MOD_CODEC   "(video) YUV | (audio) PCM"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_YUV | TC_CAP_PCM;

#define MOD_PRE nuv
#include "import_def.h"

static TCModuleInstance mod_video, mod_audio;

/*************************************************************************/

/* Open stream. */

MOD_open
{
    TCModuleInstance *mod = NULL;

    if (param->flag == TC_VIDEO) {
        mod = &mod_video;
    } else if (param->flag == TC_AUDIO) {
        mod = &mod_audio;
    } else {
        return -1;
    }

    if (nuv_init(mod) < 0)
        return -1;
    if (nuv_configure(mod, "", vob) < 0) {
        nuv_fini(mod);
        return -1;
    }

    param->fd = NULL;  /* we handle the reading ourselves */
    return 0;
}

/*************************************************************************/

/* Close stream. */

MOD_close
{
    TCModuleInstance *mod = NULL;

    if (param->flag == TC_VIDEO) {
        mod = &mod_video;
    } else if (param->flag == TC_AUDIO) {
        mod = &mod_audio;
    } else {
        return -1;
    }
    return nuv_fini(mod);
}

/*************************************************************************/

/* Decode stream. */

MOD_decode
{
    TCModuleInstance *mod = NULL;
    PrivateData *pd = NULL;

    if (param->flag == TC_VIDEO) {
        mod = &mod_video;
    } else if (param->flag == TC_AUDIO) {
        mod = &mod_audio;
    } else {
        return -1;
    }
    pd = mod->userdata;

    if (pd->fd < 0) {
        tc_log_error(MOD_NAME, "No file open in decode!");
        return -1;
    }

    if (param->flag == TC_VIDEO) {
        vframe_list_t vframe1, vframe2;
        static uint8_t tempvbuf[TC_MAX_V_FRAME_WIDTH*TC_MAX_V_FRAME_HEIGHT*3];
        vframe1.video_buf = tempvbuf;
        vframe2.video_buf = param->buffer;
        if (param->attributes & TC_FRAME_IS_OUT_OF_RANGE) {
            if (nuv_demultiplex(mod, &vframe2, NULL) < 0)
                return -1;
        } else {
            if (nuv_demultiplex(mod, &vframe1, NULL) < 0)
                return -1;
            if (nuv_decode_video(mod, &vframe1, &vframe2) < 0)
                return -1;
        }
        param->size = vframe2.video_size;
    } else if (param->flag == TC_AUDIO) {
        aframe_list_t aframe;
        aframe.audio_buf = param->buffer;
        if (nuv_demultiplex(mod, NULL, &aframe) < 0)
            return -1;
        param->size = aframe.audio_size;
    }

    return 0;
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
