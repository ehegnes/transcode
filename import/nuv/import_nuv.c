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

#define MOD_NAME        "import_nuv.so"
#define MOD_VERSION     "v0.2 (2006-05-31)"
#define MOD_CAP         "Imports NuppelVideo streams"
#define MOD_AUTHOR      "Andrew Church"

/*************************************************************************/

//#include "rtjpeg_vid_plugin.h"
static int rtjpeg_vid_open(const char *filename);
static void *rtjpeg_vid_get_frame(int frame, int *timecode_p, int one,
                                  void *audiobuf, int *audiolen_p);
static int rtjpeg_vid_end_of_video(void);
static void rtjpeg_vid_close(void);
static int rtjpeg_vid_video_width = 0;
static int rtjpeg_vid_video_height = 0;

#include "rtjpeg_aud_plugin.h"

/*************************************************************************/

/* Old module support only, for the moment */

#define MOD_CODEC   "(video) YUV | (audio) PCM"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_YUV | TC_CAP_PCM;

#define MOD_PRE nuv
#include "import_def.h"

static int videoframe = 0;
static int audioframe = 0;
static uint8_t *audiosave = NULL;  // Buffer for audio data
static int audiosave_size = 0;     // Allocated size of buffer
static int audiosave_len = 0;      // Bytes of buffer actually used

/*************************************************************************/

/* Open stream. */

MOD_open
{
    if (param->flag == TC_VIDEO) {
        if (!rtjpeg_vid_open(vob->video_in_file))
            return TC_IMPORT_ERROR;
        param->fd = NULL;
        videoframe = 0;
        return 0;
    } else if (param->flag == TC_AUDIO) {
        rtjpeg_aud_open(vob->audio_in_file);
        rtjpeg_aud_resample = 1;
        param->fd = NULL;
        audioframe = 0;
        audiosave = NULL;
        audiosave_size = 0;
        audiosave_len = 0;
        return 0;
    } else {
        return TC_IMPORT_ERROR;
    }
}

/*************************************************************************/

/* Decode stream. */

MOD_decode
{
    void *videobuf = NULL;
    uint8_t *audiobuf = NULL;
    int audiolen = 0;
    int timecode = 0;

    if (param->flag == TC_VIDEO) {

        if (rtjpeg_vid_end_of_video())
            return TC_IMPORT_ERROR;
        if (verbose & TC_DEBUG)
            tc_log_msg(MOD_NAME, "vid: get frame %d", videoframe);
        videobuf = rtjpeg_vid_get_frame(videoframe, &timecode, 1,
                                        &audiobuf, &audiolen);
        if (!videobuf) {
            if (verbose & TC_DEBUG)
                tc_log_msg(MOD_NAME, "video buffer empty");
            return TC_IMPORT_ERROR;
        }
        param->size = (rtjpeg_vid_video_width * rtjpeg_vid_video_height)
            + ((rtjpeg_vid_video_width/2) * (rtjpeg_vid_video_height/2)) * 2;
        ac_memcpy(param->buffer, videobuf, param->size);
        videoframe++;
        return 0;

    } else if (param->flag == TC_AUDIO) {

        /* Nuppelvideo doesn't seem to keep a consistent audio frame size,
         * so we read in enough audio data to fill the caller's frame and
         * save the rest in the `audiosave' buffer for the next frame. */
        while (audiosave_len < param->size) {
            if (rtjpeg_aud_end_of_video())
                return TC_IMPORT_ERROR;
            if (verbose & TC_DEBUG)
                tc_log_msg(MOD_NAME, "aud: get frame %d", audioframe);
            videobuf = rtjpeg_aud_get_frame(audioframe, &timecode, 0,
                                            &audiobuf, &audiolen);
            if (!audiobuf) {
                if (verbose & TC_DEBUG)
                    tc_log_msg(MOD_NAME, "audio buffer empty");
                return TC_IMPORT_ERROR;
            }
            if (audiosave_len + audiolen > audiosave_size) {
                audiosave_size = audiosave_len + audiolen;
                audiosave = realloc(audiosave, audiosave_size);
                if (!audiosave) {
                    tc_log_error(MOD_NAME, "No memory for audio buffer!");
                    return TC_IMPORT_ERROR;
                }
            }
            ac_memcpy(audiosave + audiosave_len, audiobuf, audiolen);
            audiosave_len += audiolen;
            audioframe++;
        }

        /* Give the caller the number of bytes it expects, and leave the
         * rest buffered.*/
        ac_memcpy(param->buffer, audiosave, param->size);
        audiosave_len -= param->size;
        if (audiosave_len > 0) {
            /* This is safe even if it overlaps--ac_memcpy() is guaranteed
             * to be ascending */
            ac_memcpy(audiosave, audiosave + param->size, audiosave_len);
        }

        return 0;

    } else {

        param->size = 0;
        return TC_IMPORT_ERROR;

    }
}

/*************************************************************************/

/* Close stream. */

MOD_close
{
    if (param->flag == TC_VIDEO) {
        rtjpeg_vid_close();
        return 0;
    } else if (param->flag == TC_AUDIO) {
        rtjpeg_aud_close();
        free(audiosave);
        audiosave = NULL;
        audiosave_size = 0;
        audiosave_len = 0;
        return 0;
    } else {
        return TC_IMPORT_ERROR;
    }
}

/*************************************************************************/
/*************************************************************************/

/* Temp rtjpeg_vid replacement--should eventually be restructured */

#include "nuppelvideo.h"
#include "RTjpegN.h"
#include "libtc/tc_lzo.h"

static int rtjpeg_vid_file = -1;
static int rtjpeg_vid_eof = 0;
static int rtjpeg_vid_framesize = 0;
static double rtjpeg_vid_fps = 0;
static int rtjpeg_vid_framenum = 0;

/*************************************************************************/

/* Open the file and parse its header; return nonzero on success, zero on
 * error. */

static int rtjpeg_vid_open(const char *filename)
{
    struct rtfileheader hdr;

    rtjpeg_vid_file = open(filename, O_RDONLY);
    if (rtjpeg_vid_file < 0) {
        tc_log_error(MOD_NAME, "Unable to open %s: %s", filename,
                     strerror(errno));
        return 0;
    }
    if (read(rtjpeg_vid_file, &hdr, sizeof(hdr)) != sizeof(hdr)) {
        tc_log_error(MOD_NAME, "Unable to read file header from %s", filename);
        close(rtjpeg_vid_file);
        rtjpeg_vid_file = -1;
        return 0;
    }
    if (strcmp(hdr.finfo, "NuppelVideo") != 0) {
        tc_log_error(MOD_NAME, "Bad file header in %s", filename);
        close(rtjpeg_vid_file);
        rtjpeg_vid_file = -1;
        return 0;
    }
    if (strcmp(hdr.version, "0.05") != 0) {
        tc_log_error(MOD_NAME, "Bad format version in %s", filename);
        close(rtjpeg_vid_file);
        rtjpeg_vid_file = -1;
        return 0;
    }

    rtjpeg_vid_video_width = hdr.width;
    rtjpeg_vid_video_height = hdr.height;
    rtjpeg_vid_framesize = hdr.width * hdr.height
                         + (hdr.width/2) * (hdr.height/2) * 2;
    rtjpeg_vid_fps = hdr.fps;
    rtjpeg_vid_framenum = 0;
    rtjpeg_vid_eof = 0;

    return 1;
}

/*************************************************************************/

/* Read a frame from the file.  We ignore the `frame' parameter and just
 * read the next frame (that's all transcode wants, anyway). */

static void *rtjpeg_vid_get_frame(int frame, int *timecode_p, int one,
                                  void *audiobuf, int *audiolen_p)
{
    /* Ideally the buffer pointer should be passed in */
    static char videobuf[TC_MAX_V_FRAME_WIDTH*TC_MAX_V_FRAME_HEIGHT*3];
    static struct rtframeheader hdr;  // may be saved until next call
    uint8_t *encoded_frame;
    double timestamp;


    // init the buffer to black in case we skip frames to start with (HACK)
    static int cleared_videobuf = 0;
    if (!cleared_videobuf) {
        memset(videobuf, 0, rtjpeg_vid_video_width * rtjpeg_vid_video_height);
        memset(videobuf + rtjpeg_vid_video_width*rtjpeg_vid_video_height, 128,
               (rtjpeg_vid_video_width/2) * (rtjpeg_vid_video_height/2) * 2);
        cleared_videobuf = 1;
    }

    if (rtjpeg_vid_file == -1 || rtjpeg_vid_eof)
        return NULL;

    /* Loop reading packets until we find a video packet (don't do
     * anything if we have a frame waiting to be read) */

    while (hdr.frametype != 'V') {

        if (read(rtjpeg_vid_file, &hdr, sizeof(hdr)) != sizeof(hdr)) {
            rtjpeg_vid_eof = 1;
            return NULL;
        }

        /* Check for a compressor data packet */
        if (hdr.frametype == 'D' && hdr.comptype == 'R') {
            /* Data for RTjpeg decompressor */
            uint32_t table[128];
            if (hdr.packetlength < sizeof(table)) {
                tc_log_warn(MOD_NAME, "Short compressor data packet");
                rtjpeg_vid_eof = 1;
                return NULL;
            }
            if (read(rtjpeg_vid_file, table, sizeof(table)) != sizeof(table)) {
                tc_log_warn(MOD_NAME,
                            "File truncated in compressor data packet");
                rtjpeg_vid_eof = 1;
                return NULL;
            }
            RTjpeg_init_decompress((unsigned long *)table,
                                   rtjpeg_vid_video_width,
                                   rtjpeg_vid_video_height);
            hdr.packetlength -= sizeof(table);
            /* Fall through to packet-skipping code below */
        }

        /* Skip anything that's not a video packet (including any trailing
         * data not read from a compressor data packet).  Note that
         * seekpoints ('R') packets don't have a valid length, but they
         * also don't have any data, so we just ignore them. */
        if (hdr.frametype != 'V' && hdr.frametype != 'R') {
            while (hdr.packetlength > 0) {
                char buf[0x1000];
                int toread = hdr.packetlength;
                if (toread > sizeof(buf))
                    toread = sizeof(buf);
                if (read(rtjpeg_vid_file, buf, toread) != toread) {
                    tc_log_warn(MOD_NAME, "File truncated in skipped packet");
                    rtjpeg_vid_eof = 1;
                    return NULL;
                }
                hdr.packetlength -= toread;
            }
        }

    }  // while (hdr.frametype != 'V')

    /* Now we have a video packet.  Check the timecode (cloning as needed),
     * then (possibly) read it in and decode it. */
    if (verbose & TC_DEBUG)
        tc_log_msg(MOD_NAME,"<<< frame=%d timecode=%d >>>",frame,hdr.timecode);

    timestamp = hdr.timecode / 1000.0;
    if (timestamp > ((rtjpeg_vid_framenum+0.5) / rtjpeg_vid_fps)) {
        tc_log_warn(MOD_NAME, "Dropped frame(s) or bad A/V sync, cloning"
                    " last frame");
        rtjpeg_vid_framenum++;
        return videobuf;
    }

    if (hdr.packetlength > 0) {
        encoded_frame = malloc(hdr.packetlength);
        if (!encoded_frame) {
            tc_log_error(MOD_NAME, "No memory for encoded frame!");
            rtjpeg_vid_eof = 1;
            return NULL;
        }
        if (read(rtjpeg_vid_file, encoded_frame, hdr.packetlength) 
            != hdr.packetlength
        ) {
            tc_log_warn(MOD_NAME, "File truncated in video packet");
            free(encoded_frame);
            rtjpeg_vid_eof = 1;
            return NULL;
        }
    } else {
        encoded_frame = NULL;
    }

    if (hdr.comptype == '2' || hdr.comptype == '3') {
        /* Undo LZO compression */
        uint8_t *decompressed_frame;
        lzo_uint len;
        decompressed_frame = malloc(rtjpeg_vid_framesize);
        if (!decompressed_frame) {
            tc_log_error(MOD_NAME, "No memory for decompressed frame!");
            free(encoded_frame);
            rtjpeg_vid_eof = 1;
            return NULL;
        }
        if (lzo1x_decompress(encoded_frame, hdr.packetlength,
                             decompressed_frame, &len, NULL) == LZO_E_OK) {
            free(encoded_frame);
            encoded_frame = decompressed_frame;
        } else {
            free(decompressed_frame);
            tc_log_warn(MOD_NAME, "Unable to decompress video frame");
            /* And try it as raw, just like rtjpeg_vid_plugin */
        }
        /* Convert 2 -> 1, 3 -> 0 */
        hdr.comptype ^= 3;
    }

    switch (hdr.comptype) {

      case '0':  // Uncompressed YUV
        if (hdr.packetlength > rtjpeg_vid_framesize)
            hdr.packetlength = rtjpeg_vid_framesize;
        ac_memcpy(videobuf, encoded_frame, hdr.packetlength);
        break;

      case '1':  // RTjpeg-compressed data
        RTjpeg_decompressYUV420((__s8 *)encoded_frame, videobuf);
        break;

      case 'N':  // Black frame
        break;

      case 'L':  // Repeat last frame--leave videobuf alone
        break;

      default:
        tc_log_warn(MOD_NAME, "Unknown video compression type %c (%02X)",
                    hdr.comptype>=' ' && hdr.comptype<='=' ? hdr.comptype : '?',
                    hdr.comptype);
        break;  // Repeat last frame

    }  // switch (hdr.comptype)

    rtjpeg_vid_framenum++;
    free(encoded_frame);
    hdr.frametype = 0;
    return videobuf;
}

/*************************************************************************/

/* Return whether the end of file has been reached. */

static int rtjpeg_vid_end_of_video(void)
{
    return rtjpeg_vid_eof;
}

/*************************************************************************/

/* Close the file. */

static void rtjpeg_vid_close(void)
{
    if (rtjpeg_vid_file != -1) {
        close(rtjpeg_vid_file);
        rtjpeg_vid_file = -1;
    }
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
