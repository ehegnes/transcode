/*
 * audio_trans.c - audio frame transformation routines
 * Written by Andrew Church <achurch@achurch.org>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "transcode.h"
#include "framebuffer.h"
#include "audio_trans.h"
#include "libtcaudio/tcaudio.h"

/*************************************************************************/

/* Handle for calling tcaudio functions. */
static TCAHandle handle = 0;

/*************************************************************************/
/*************************************************************************/

/**
 * do_process_audio:  Perform actual audio processing.
 *
 * Parameters:
 *     vob: Global data pointer.
 *     ptr: Pointer to audio frame buffer.
 * Return value:
 *     1 on success, 0 on failure.
 */

static int do_process_audio(vob_t *vob, aframe_list_t *ptr)
{
    int srcfmt, nsamples;

    /* First convert audio to destination format (also handles -d) */
    if (vob->a_bits == 8) {
        srcfmt = TCA_U8;
        nsamples = ptr->audio_size;
    } else if (vob->a_bits == 16) {
        srcfmt = pcmswap ? TCA_S16BE : TCA_S16LE;
        nsamples = ptr->audio_size / 2;
    } else {
        tc_log_error(__FILE__, "Sorry, source audio format not supported");
        return 0;
    }
    tca_convert_from(handle, ptr->audio_buf, nsamples, srcfmt);

    /* Convert between stereo and mono */
    if (vob->a_chan == 1 && vob->dm_chan == 2) {
        tca_mono_to_stereo(handle, ptr->audio_buf, nsamples);
        nsamples *= 2;
    } else if (vob->a_chan == 2 && vob->dm_chan == 1) {
        nsamples /= 2;
        tca_stereo_to_mono(handle, ptr->audio_buf, nsamples);
    }

    /* Update audio buffer size */
    ptr->audio_size = nsamples * (vob->dm_bits/8);

    /* -s: Amplify volume */
    if (vob->volume > 0) {
        int nclip = 0;
        tca_amplify(handle, ptr->audio_buf, nsamples, vob->volume, &nclip);
        vob->clip_count += nclip;
    }

    /* --av_fine_ms: Shift audio */
    if (vob->sync_ms != 0) {
        /* Note that we adjust based on the source rate */
        int newsamps = (-vob->sync_ms * vob->a_rate / 1000) * vob->dm_chan;
        if (newsamps > 0) {
            memmove((uint8_t *)ptr->audio_buf + newsamps, ptr->audio_buf,
                    ptr->audio_size);
            memset(ptr->audio_buf, 0, newsamps * (vob->dm_bits/8));
        } else {
            memmove(ptr->audio_buf, (uint8_t *)ptr->audio_buf - newsamps,
                    ptr->audio_size + (newsamps * (vob->dm_bits/8)));
        }
        nsamples += newsamps;
        ptr->audio_size += newsamps * (vob->dm_bits/8);
        if (verbose & TC_DEBUG) {
            tc_log_info(__FILE__, "adjusted %d PCM samples (%d ms)",
                        -newsamps, vob->sync_ms);
        }
        /* Only do it once */
        vob->sync_ms = 0;
    }

    /* All done */
    return 1;
}

/*************************************************************************/

/**
 * process_aud_frame:  Main audio frame processing routine.
 *
 * Parameters:
 *     vob: Global data pointer.
 *     ptr: Pointer to audio frame buffer.
 * Return value:
 *     0 on success, -1 on failure.
 */

int process_aud_frame(vob_t *vob, aframe_list_t *ptr)
{
    /* Check parameter validity */
    if (!vob || !ptr)
        return -1;

    /* Allocate tcaudio handle if necessary */
    if (!handle) {
        AudioFormat format;
        if (vob->dm_bits == 8) {
            format = TCA_U8;
        } else if (vob->dm_bits == 16) {
            format = TCA_S16LE;
        } else {
            tc_log_error(__FILE__, "Sorry, output audio format not supported");
            return -1;
        }
        handle = tca_init(format);
        if (!handle) {
            tc_log_error(__FILE__, "tca_init() failed!");
            return -1;
        }
    }

    /* Check for pass-through mode */
    if (vob->pass_flag & TC_AUDIO)
        return 0;

    /* Check audio format */
    if (vob->im_a_codec != CODEC_PCM) {
        tc_log_error(__FILE__,
                     "Sorry, only PCM audio is supported for processing");
        return -1;
    }

    /* Actually perform processing */
    return do_process_audio(vob, ptr) ? 0 : -1;
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
