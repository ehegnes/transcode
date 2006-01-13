/*
 * decode_dv.c - Digital Video decoding routines, using libdv
 * Written by Andrew Church <achurch@achurch.org>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "transcode.h"
#include "tcinfo.h"
#include "aclib/imgconvert.h"
#include "ioaux.h"  /* for import_exit() prototype */
#include "tc.h"  /* for function prototypes */

#ifdef HAVE_LIBDV
# include <libdv/dv.h>
#endif

#define DV_FRAME_SIZE_525_60    120000
#define DV_FRAME_SIZE_625_50    144000

/* Redefine libdv constants to match our naming style */
#define DV_SYSTEM_525_60        e_dv_system_525_60
#define DV_SYSTEM_625_50        e_dv_system_625_50
#define DV_COLOR_YUV            e_dv_color_yuv
#define DV_COLOR_RGB            e_dv_color_rgb

/*************************************************************************/

/**
 * check_yuy2:  Internal routine to check whether libdv returns YUY2 or
 * YV12 data for PAL DV frames.
 *
 * Parameters: None.
 * Return value:  1 if the decoded video data is in YUY2 format,
 *                0 if the decoded video data is in YV12 format,
 *               -1 if the decoded video data format is unknown.
 * Preconditions: None.
 */

static int check_yuy2(void)
{
    /* Not yet implemented */
    return -1;
}

/*************************************************************************/

/**
 * decode_dv:  DV decoding loop, called from tcdecode.  Reads raw DV frames
 * from stdin and writes the decoded video or audio data to stdout.
 *
 * Parameters: decode: Pointer to decoding parameter structure.
 * Return value: None.
 * Preconditions: None.
 */

void decode_dv(decode_t *decode)
{
#ifdef HAVE_LIBDV

    dv_decoder_t *decoder;
    /* FIXME: should this buffer be static instead (to reduce stack usage)? */
    uint8_t framebuf[DV_FRAME_SIZE_625_50];    /* Buffer for input frame */
    uint8_t *video[3];                         /* Decoded video data */
    uint8_t *video_conv_buf[3];                /* For YUY2/420P conversion */
    ImageFormat srcfmt = 0, destfmt = 0;       /* For YUY2/420P conversion */
    dv_color_space_t colorspace = 0;           /* For dv_decode_full_frame() */
    int linesize[3], planesize[3];             /* video[] line/plane size */
    int video_conv_linesize[3];                /* Same, for video_conv_buf[] */
    int16_t audio_in[4][DV_AUDIO_MAX_SAMPLES]; /* Decoded audio data */
    int16_t *audio_inptr[4];                   /* List of pointers for same */
    int16_t audio_out[4*DV_AUDIO_MAX_SAMPLES]; /* Interleaved audio data */
    int yuy2_mode;                             /* libdv YUY2/YV12 selector */
    int error = 0;                             /* Set to signal an error */
    int ispal;                                 /* Is it a 625/50 stream? */
    int nread;                                 /* Result of tc_pread() */


    /**** Sanity check and variable setup ****/

    if (!decode) {
        tc_log_error(__FILE__, "Invalid parameter to decode_dv()");
        import_exit(1);
        return;
    }
    audio_inptr[0] = audio_in[0];
    audio_inptr[1] = audio_in[1];
    audio_inptr[2] = audio_in[2];
    audio_inptr[3] = audio_in[3];

    /**** Initialize libdv decoder ****/

    decoder = dv_decoder_new(1, 0, 0);
    if (!decoder) {
        tc_log_error(__FILE__, "Unable to initialize DV decoder");
        import_exit(1);
        return;
    }
    switch (decode->quality) {
        case 1:  decoder->quality = DV_QUALITY_FASTEST;                 break;
        case 2:  decoder->quality = DV_QUALITY_AC_1;                    break;
        case 3:  decoder->quality = DV_QUALITY_AC_2;                    break;
        case 4:  decoder->quality = DV_QUALITY_AC_1 | DV_QUALITY_COLOR; break;
        case 5:
        default: decoder->quality = DV_QUALITY_BEST;                    break;
    }

    /**** Set up image formats and line size per plane (*16) ****/

    switch (decode->format) {
      case TC_CODEC_YUV420P:
        srcfmt = IMG_UNKNOWN;  /* Source format not yet known */
        destfmt = IMG_YUV420P;
        colorspace = DV_COLOR_YUV;
        linesize[0] = 16;
        linesize[1] = linesize[2] = 8;
        break;
      case TC_CODEC_YUY2:
        srcfmt = IMG_UNKNOWN;  /* Source format not yet known */
        destfmt = IMG_YUY2;
        colorspace = DV_COLOR_YUV;
        linesize[0] = 32;
        linesize[1] = linesize[2] = 0;
        break;
      case TC_CODEC_RGB:
        srcfmt = destfmt = IMG_RGB24;
        colorspace = DV_COLOR_RGB;
        linesize[0] = 48;
        linesize[1] = linesize[2] = 0;
        break;
      case TC_CODEC_PCM:
        linesize[0] = linesize[1] = linesize[2] = 0;  // what video? ;)
        break;
      default:
        tc_log_error(__FILE__, "Invalid output format (%08X)", decode->format);
        import_exit(1);
        return;
    }

    /**** Read the first frame and analyze the frame header.  We start ****
     **** by reading in enough data for an NTSC frame; if it turns out ****
     **** to be PAL instead, we read the rest of it once we find that  ****
     **** out.                                                         ****/

    nread = tc_pread(decode->fd_in, framebuf, DV_FRAME_SIZE_525_60);
    if (nread != DV_FRAME_SIZE_525_60) {
        tc_log_error(__FILE__, "No DV frames found!");
        import_exit(1);
        return;
    }
    if (dv_parse_header(decoder, framebuf) < 0) {
        tc_log_error(__FILE__, "Unable to parse frame header!");
        import_exit(1);
        return;
    }
    if (decoder->system == DV_SYSTEM_525_60) {
        ispal = 0;
    } else if (decoder->system == DV_SYSTEM_625_50) {
        ispal = 1;
    } else {
        tc_log_error(__FILE__, "Unknown or invalid DV frame type!");
        import_exit(1);
        return;
    }
    if (ispal) {
        nread = tc_pread(decode->fd_in, framebuf + DV_FRAME_SIZE_525_60,
                         DV_FRAME_SIZE_625_50 - DV_FRAME_SIZE_525_60);
        if (nread != DV_FRAME_SIZE_625_50 - DV_FRAME_SIZE_525_60) {
            tc_log_error(__FILE__, "No DV frames found!");
            import_exit(1);
            return;
        }
    }

    /**** Print a stream information summary ****/

    if (decode->format == TC_CODEC_PCM) {
        tc_log_info(__FILE__, "audio: %d Hz, %d channels",
                    decoder->audio->frequency, decoder->audio->num_channels);
    } else {
        tc_log_info(__FILE__, "%s video: %dx%d framesize=%lu sampling=%d",
                    ispal ? "PAL" : "NTSC", decoder->width,
                    decoder->height, (unsigned long)decoder->frame_size,
                    decoder->sampling);
    }

    /**** Allocate video buffers ****/

    /* Plane 0: packed RGB, packed YUV, or planar Y */
    linesize[0] = (linesize[0] * decoder->width) / 16;
    planesize[0] = linesize[0] * decoder->height;
    if (planesize[0])
        video[0] = tc_bufalloc(planesize[0]);
    else
        video[0] = NULL;

    /* Plane 1: planar U (half height) */
    linesize[1] = (linesize[1] * decoder->width) / 16;
    planesize[1] = linesize[1] * (decoder->height/2);
    if (planesize[1])
        video[1] = tc_bufalloc(planesize[1]);
    else
        video[1] = NULL;

    /* Plane 2: planar V (half height) */
    linesize[2] = (linesize[2] * decoder->width) / 16;
    planesize[2] = linesize[2] * (decoder->height/2);
    if (planesize[2])
        video[2] = tc_bufalloc(planesize[2]);
    else
        video[2] = NULL;

    /* Conversion buffer (allocate just in case) */
    video_conv_buf[0] = tc_bufalloc(decoder->width * decoder->height * 2);
    video_conv_buf[1] = video_conv_buf[0] + decoder->width * decoder->height;
    video_conv_buf[2] = video_conv_buf[1] + (decoder->width/2)*decoder->height;

    /* Make sure we got everything we need */
    if ((planesize[0] && !video[0])
     || (planesize[1] && !video[1])
     || (planesize[2] && !video[2])
     || !video_conv_buf[0]
    ) {
        tc_log_error(__FILE__, "No memory for video buffers!");
        tc_buffree(video_conv_buf[0]);
        tc_buffree(video[2]);
        tc_buffree(video[1]);
        tc_buffree(video[0]);
        import_exit(1);
        return;
    }

    /**** Determine whether YUV data is YUY2 or YV12 ****/

    if (ispal) {
        int result = check_yuy2();
        if (result == 1)
            yuy2_mode = 1;
        else if (result == 0)
            yuy2_mode = 0;
        else /* we don't know, let the user decide */
            yuy2_mode = decode->dv_yuy2_mode;
    } else {
        /* NTSC is always returned in YUY2 format */
        yuy2_mode = 1;
    }
    if (srcfmt == IMG_UNKNOWN) {
        if (yuy2_mode) {
            srcfmt = IMG_YUY2;
            video_conv_linesize[0] = decoder->width * 2;
            video_conv_linesize[1] = video_conv_linesize[2] = 0;
        } else {
            srcfmt = IMG_YUV420P;
            video_conv_linesize[0] = decoder->width;
            video_conv_linesize[1] = video_conv_linesize[2] = decoder->width/2;
        }
    }

    /**** Decoding loop ****/

    for (;;) {
        int toread;

        /* Process the data as requested */

        switch (decode->format) {

          case TC_CODEC_YUV420P:
          case TC_CODEC_YUY2:
          case TC_CODEC_RGB:
            if (srcfmt == destfmt) {
                dv_decode_full_frame(decoder, framebuf, colorspace, video,
                                     linesize);
            } else {
                dv_decode_full_frame(decoder, framebuf, colorspace,
                                     video_conv_buf, video_conv_linesize);
                if (!ac_imgconvert(video_conv_buf, srcfmt, video, destfmt,
                                   decoder->width, decoder->height)) {
                    tc_log_error(__FILE__, "Image format conversion failed!");
                    error = 1;
                    goto done;
                }
            }
            if ((planesize[0] && tc_pwrite(decode->fd_out, video[0],
                                           planesize[0]) != planesize[0])
             || (planesize[1] && tc_pwrite(decode->fd_out, video[1],
                                           planesize[1]) != planesize[1])
             || (planesize[2] && tc_pwrite(decode->fd_out, video[2],
                                           planesize[2]) != planesize[2])
            ) {
                tc_log_error(__FILE__, "Write failed: %s", strerror(errno));
                error = 1;
                goto done;
            }
            break;

          case TC_CODEC_PCM: {
            int i, ch;
            int16_t *outptr = audio_out;
            dv_decode_full_audio(decoder, framebuf, audio_inptr);
            for (i = 0; i < decoder->audio->samples_this_frame; i++) {
                for (ch = 0; ch < decoder->audio->num_channels; ch++) {
                    *outptr++ = audio_in[ch][i];
                }
            }
            i = decoder->audio->samples_this_frame
                * decoder->audio->num_channels * 2;
            if (tc_pwrite(decode->fd_out, (void *)audio_out, i) != i) {
                tc_log_error(__FILE__, "Write failed: %s", strerror(errno));
                error = 1;
                goto done;
            }
            break;
          }  /* case TC_CODEC_PCM */

        }  /* switch (decode->format) */

        /* Read in the next frame, if any, and parse the header */

      retry:
        toread = ispal ? DV_FRAME_SIZE_625_50 : DV_FRAME_SIZE_525_60;
        nread = tc_pread(decode->fd_in, framebuf, toread);
        if (nread != toread) {
            if (verbose & TC_DEBUG)
                tc_log_info(__FILE__, "End of stream reached.");
            break;
        }
        if (dv_parse_header(decoder, framebuf) < 0) {
            tc_log_warn(__FILE__, "Unable to parse frame header, skipping...");
            goto retry;
        }
        /* Sanity check: make sure it's the same video system */
        if (decoder->system != (ispal ? DV_SYSTEM_625_50 : DV_SYSTEM_525_60)) {
            tc_log_error(__FILE__, "Video system (NTSC/PAL) changed"
                         " midstream!  Aborting.");
            error = 1;
            break;
        }

    }  /* end decoding loop */

    /**** All done ****/

  done:
    tc_buffree(video_conv_buf[0]);
    tc_buffree(video[2]);
    tc_buffree(video[1]);
    tc_buffree(video[0]);
    dv_decoder_free(decoder);

    import_exit(error ? 1 : 0);

#else  /* !HAVE_LIBDV */

    tc_log_error(__FILE__, "No support for Digital Video configured - exit.");
    import_exit(1);

#endif
}

/*************************************************************************/

/**
 * probe_dv:  Probe the input stream (on stdin) and set stream parameters
 * as appropriate.
 *
 * Parameters: info: Pointer to stream parameter structure.
 * Return value: None.
 * Preconditions: None.
 */

void probe_dv(info_t *info)
{
#ifdef HAVE_LIBDV
    dv_decoder_t *decoder;
    uint8_t framebuf[DV_FRAME_SIZE_525_60];  // FIXME: too big for the stack?
    int nread, ispal;
#endif

    if (!info) {
        tc_log_error(__FILE__, "Invalid parameter to probe_dv()");
        return;
    }

#ifdef HAVE_LIBDV

    /* Read in an NTSC frame; for probing, this is enough for PAL as well */
    nread = tc_pread(info->fd_in, framebuf, sizeof(framebuf));
    if (nread < sizeof(framebuf)) {
        tc_log_error(__FILE__, "Stream too short");
        info->error = 1;
        return;
    }

    /* Initialize libdv decoder */
    decoder = dv_decoder_new(1, 0, 0);
    if (!decoder) {
        tc_log_error(__FILE__, "Unable to initialize DV decoder");
        info->error = 1;
        return;
    }

    /* Parse frame header and check video system type */
    if (dv_parse_header(decoder, framebuf) < 0) {
        tc_log_error(__FILE__, "No valid DV frame found");
        info->error = 1;
        return;
    }
    if (decoder->system == DV_SYSTEM_525_60) {
        ispal = 0;
    } else if (decoder->system == DV_SYSTEM_625_50) {
        ispal = 1;
    } else {
        tc_log_error(__FILE__, "Unknown or invalid DV frame type");
        info->error = 1;
        return;
    }

    /* Set stream parameters */

    info->probe_info->magic  = ispal ? TC_MAGIC_PAL : TC_MAGIC_NTSC;

    info->probe_info->width  = decoder->width;
    info->probe_info->height = decoder->height;
    info->probe_info->fps    = ispal ? PAL_FPS : NTSC_VIDEO;
    info->probe_info->frc    = ispal ? 3 : 4;
    info->probe_info->asr    = dv_format_wide(decoder)   ? 3 :
                               dv_format_normal(decoder) ? 2 : 0;

    info->probe_info->track[0].samplerate = decoder->audio->frequency;
    info->probe_info->track[0].chan       = decoder->audio->num_channels;
    info->probe_info->track[0].bits       = 16;
    info->probe_info->track[0].format     = CODEC_PCM;
    info->probe_info->track[0].bitrate    =
        (decoder->audio->frequency * decoder->audio->num_channels * 16) / 1000;
    info->probe_info->num_tracks          = 1;

    /* All done */
    dv_decoder_free(decoder);

#endif  /* HAVE_LIBDV */

    /* Even without libdv, we know this much... */
    info->probe_info->codec = TC_CODEC_DV;

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
