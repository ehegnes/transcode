/*
 * probe.c - probe input file for parameters
 * Written by Andrew Church <achurch@achurch.org>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "transcode.h"
#include "probe.h"
#include "libtc/ratiocodes.h"
#include "import/magic.h"

/*************************************************************************/

/* Handy macro to check whether the probe flags allow setting of a
 * particular field (pass the flag name without TC_PROBE_NO_): */
#define MAY_SET(flagname)  (!(flags & TC_PROBE_NO_##flagname))

/* Internal routine declarations: */

static int do_probe(const char *file, const char *nav_seek_file, int title,
		    int range, int mplayer_flag, int verbose_flag,
		    ProbeInfo *info_ret);
static void probe_to_vob(ProbeInfo *vinfo, ProbeInfo *ainfo, int flags,
                         vob_t *vob);
static void select_modules(int flags, vob_t *vob);

/*************************************************************************/
/*************************************************************************/

/* External interfaces */

/*************************************************************************/

/**
 * probe_source:  Probe the given input file(s) and store the results in
 * the global data structure.
 *
 * Parameters: vid_file: Video file name, or NULL if none.
 *             aud_file: Audio file name, or NULL if none.
 *                range: Amount of input files to probe, in MB.
 *                flags: Flags indicating which global parameters should be
 *                       left alone (TC_PROBE_NO_xxx flags).
 *                  vob: Pointer to global data structure.
 * Return value: Nonzero on success, zero on error.
 * Preconditions: vob != NULL
 */

int probe_source(const char *vid_file, const char *aud_file, int range,
                 int flags, vob_t *vob)
{
    ProbeInfo vinfo, ainfo;  // video and audio info structures

    /* Probe the video file, if present */
    if (vid_file) {
        if (!do_probe(vid_file, vob->nav_seek_file, vob->dvd_title, range,
                      (flags & TC_PROBE_NO_BUILTIN),
                      (verbose >= TC_DEBUG) ? verbose : 0, &vinfo)
        ) {
            if (verbose & TC_DEBUG) {
                tc_log_warn(PACKAGE, "(%s) failed to probe video source",
                            __FILE__);
            }
            return 0;
        }
    } else {
        vob->has_video = 0;
    }

    /* Probe the audio file, if present */
    if (aud_file) {
        if (!do_probe(aud_file, vob->nav_seek_file, vob->dvd_title, range,
                      (flags & TC_PROBE_NO_BUILTIN),
                      (verbose >= TC_DEBUG) ? verbose : 0, &ainfo)
        ) {
            if (verbose & TC_DEBUG) {
                tc_log_warn(PACKAGE, "(%s) failed to probe audio source",
                            __FILE__);
            }
            return 0;
        }
    }  /* else it might be contained in the video file */

    /* Set global parameters based on probed data */
    probe_to_vob(vid_file ? &vinfo : NULL, aud_file ? &ainfo : NULL,
                 flags, vob);
    if (verbose & TC_DEBUG) {
        tc_log_info(PACKAGE, "(%s) V magic=0x%lx, A magic=0x%lx,"
                    " V codec=0x%lx, A codec=0x%lx", __FILE__,
                    vob->v_format_flag, vob->a_format_flag,
                    vob->v_codec_flag, vob->a_codec_flag);
        tc_log_info(PACKAGE, "(%s) V magic=%s, A magic=%s, V codec=%s,"
                    " A codec=%s", __FILE__,
                    mformat2str(vob->v_format_flag),
                    mformat2str(vob->a_format_flag),
                    codec2str(vob->v_codec_flag),
                    aformat2str(vob->a_codec_flag));
    }

    /* Select appropriate import modules */
    select_modules(flags, vob);

    /* All done, return success */
    return 1;
}

/*************************************************************************/

/**
 * codec2str, aformat2str, mformat2str, asr2str:  Return a descriptive
 * string for the given codec, audio format, video format, or aspect ratio
 * flag, respectively.
 *
 * Parameters: flag: Flag to return string for.
 * Return value: String describing `flag'.
 * Preconditions: None.
 */

const char *codec2str(int flag)
{
    switch (flag) {
        case TC_CODEC_MPEG2:   return "MPEG-2";
        case TC_CODEC_MJPG:    return "MJPEG";
        case TC_CODEC_MPG1:    return "mpg1";
        case TC_CODEC_LZO1:    return "LZO1";
        case TC_CODEC_RV10:    return "RV10 Real Video";
        case TC_CODEC_DIVX3:   return "DivX;-)";
        case TC_CODEC_MP42:    return "MSMPEG4v2";
        case TC_CODEC_MP43:    return "MSMPEG4v3";
        case TC_CODEC_DIVX4:   return "DivX4";
        case TC_CODEC_DIVX5:   return "DivX5";
        case TC_CODEC_XVID:    return "XviD";
        case TC_CODEC_MPEG1:   return "MPEG-1";
        case TC_CODEC_MPEG:    return "MPEG";
        case TC_CODEC_DV:      return "DV Digital Video";
        case TC_CODEC_YUV420P: return "YUV420P/YV12";
        case TC_CODEC_YUV2:    return "YUV2";
        case TC_CODEC_NUV:     return "RTjpeg";
        case TC_CODEC_RGB:     return "RGB/BGR";
        case TC_CODEC_PCM:     return "PCM";
    }
    return "unknown";
}


const char *aformat2str(int flag)
{
    switch (flag) {
        case CODEC_AC3:    return "AC3";
        case CODEC_A52:    return "AC3/A52";
        case CODEC_MP3:    return "MPEG layer-3";
        case CODEC_MP2:    return "MPEG layer-2";
        case CODEC_PCM:    return "PCM";
        case CODEC_LPCM:   return "LPCM";
        case CODEC_VORBIS: return "Ogg Vorbis";
        case CODEC_VAG:    return "PS-VAG";
    }
    return "unknown";
}


const char *mformat2str(int flag)
{
    switch (flag) {
        case TC_MAGIC_PAL:       return "PAL";
        case TC_MAGIC_NTSC:      return "NTSC";
        case TC_MAGIC_TS:        return "MPEG transport stream";
        case TC_MAGIC_YUV4MPEG:  return "YUV4MPEG";
        case TC_MAGIC_SOCKET:    return "network stream";
        case TC_MAGIC_NUV:       return "NuppelVideo";
        case TC_MAGIC_DVD_PAL:   return "DVD PAL";
        case TC_MAGIC_DVD_NTSC:  return "DVD NTSC";
        case TC_MAGIC_AVI:       return "RIFF data, AVI";
        case TC_MAGIC_MOV:       return "QuickTime";
        case TC_MAGIC_XML:       return "XML file";
        case TC_MAGIC_TIFF1:     return "TIFF image";
        case TC_MAGIC_TIFF2:     return "TIFF image";
        case TC_MAGIC_JPEG:      return "JPEG image";
        case TC_MAGIC_BMP:       return "BMP image";
        case TC_MAGIC_PNG:       return "PNG image";
        case TC_MAGIC_GIF:       return "GIF image";
        case TC_MAGIC_PPM:       return "PPM image";
        case TC_MAGIC_PGM:       return "PGM image";
        case TC_MAGIC_CDXA:      return "RIFF data, CDXA";
        case TC_MAGIC_AC3:       return "AC3";
        case TC_MAGIC_MP3:       return "MP3";
        case TC_MAGIC_MP2:       return "MP2";
        case TC_MAGIC_OGG:       return "OGG stream";
        case TC_MAGIC_WAV:       return "RIFF data, WAVE";
        case TC_MAGIC_V4L_VIDEO: return "V4L";
        case TC_MAGIC_V4L_AUDIO: return "V4L";
    }
    return "";
}


const char *asr2str(int flag)
{
    switch (flag) {
        case  1: return "encoded @ 1:1";
        case  2: return "encoded @ 4:3";
        case  3: return "encoded @ 16:9";
        case  4: return "encoded @ 2.21:1";
        case  8: return "encoded @ 4:3";
        case 12: return "encoded @ 4:3";
    }
    return "";
}

/*************************************************************************/
/*************************************************************************/

/* Internal routines */

/*************************************************************************/

/**
 * do_probe:  Perform the actual probing of the source file.
 *
 * Parameters:          file: Filename to probe.
 *             nav_seek_file: Navigation file for `file', or NULL if none.
 *                     title: Title to probe for DVD probing.
 *                     range: Amount of file to probe, in MB.
 *              mplayer_flag: If nonzero, use mplayer to probe file.
 *              verbose_flag: Verbosity flag to pass to tcprobe.
 *                  info_ret: Structure to be filled in with probed data.
 * Return value: Nonzero on success, zero on failure.
 * Preconditions: file != NULL
 */

static int do_probe(const char *file, const char *nav_seek_file, int title,
		    int range, int mplayer_flag, int verbose_flag,
		    ProbeInfo *info_ret)
{
    char cmdbuf[PATH_MAX+1000];
    FILE *pipe;

    if (mplayer_flag) {
	if (tc_snprintf(cmdbuf, sizeof(cmdbuf),
			"tcprobe -B -M -i \"%s\" -d %d",
			file, verbose_flag) < 0)
	    return 0;
    } else {
	if (tc_snprintf(cmdbuf, sizeof(cmdbuf),
			"tcprobe -B -i \"%s\" -T %d -H %d -d %d",
			file, title, range, verbose_flag) < 0)
	    return 0;
        if (nav_seek_file
         && tc_snprintf(cmdbuf+strlen(cmdbuf), sizeof(cmdbuf)-strlen(cmdbuf),
                        " -f \"%s\"", nav_seek_file) < 0)
            return 0;
    }
    pipe = popen(cmdbuf, "r");
    if (!pipe)
	return 0;
    if (fread(&tc_probe_pid, sizeof(pid_t), 1, pipe) != 1) {
        pclose(pipe);
	return 0;
    }
    if (fread(info_ret, sizeof(*info_ret), 1, pipe) != 1) {
        pclose(pipe);
	return 0;
    }
    pclose(pipe);
    return 1;
}

/*************************************************************************/

/**
 * probe_to_vob:  Use the results of probing the input files to set global
 * parameters.
 *
 * Parameters: vinfo: Pointer to probe results for video file, or NULL if
 *                    no video file.
 *             ainfo: Pointer to probe results for audio file, or NULL if
 *                    no audio file.
 *             flags: TC_PROBE_NO_xxx flags.
 *               vob: Pointer to global data structure.
 * Return value: None.
 * Preconditions: vob != NULL
 */

static void probe_to_vob(ProbeInfo *vinfo, ProbeInfo *ainfo, int flags,
                         vob_t *vob)
{
    int track;  // user-selected audio track, sanity-checked

    track = vob->a_track;
    if (track < 0 || track >= TC_MAX_AUD_TRACKS)
        track = 0;

    if (vinfo) {
        int D_arg, D_arg_ms;  // for setting A/V sync

        /* Set frame size */
        if (MAY_SET(FRAMESIZE)) {
            if (vinfo->width > 0)
                vob->im_v_width = vinfo->width;
            if (vinfo->height > 0)
                vob->im_v_height = vinfo->height;
        }

        /* Set frame rate */
        if (MAY_SET(FPS)) {
            if (vinfo->frc > 0) {
                vob->im_frc = vinfo->frc;
                tc_frc_code_to_value(vob->im_frc, &vob->fps);
            } else if (vinfo->fps > 0)
                vob->fps = vinfo->fps;
        }

        /* Set aspect ratio */
        if (MAY_SET(IMASR)) {
            if (vinfo->asr > 0)
                vob->im_asr = vinfo->asr;
        }

        /* Set additional attributes */
        if (vinfo->attributes)
            vob->attributes = vinfo->attributes;

        /* Clear demux sync flag if appropriate */
        if (MAY_SET(DEMUX) && (vob->attributes & TC_INFO_NO_DEMUX)) {
            vob->demuxer = 0;
        }

        /* Calculate A/V sync correction */
        if (vinfo->pts_start > 0 && vinfo->track[track].pts_start > 0) {
            double pts_diff = vinfo->pts_start - vinfo->track[track].pts_start;
            D_arg = (int)(vob->fps * pts_diff);
            D_arg_ms = (int)((pts_diff - D_arg/vob->fps) * 1000);
        } else {
            D_arg = 0;
            D_arg_ms = 0;
        }
        /* This voodoo to determine whether to set the A/V sync parameters
         * is from the original probe.c, with the following comments:
         *    - case 1: demuxer disabled needs PTS sync mode
         *    - case 2: check if PTS of requested audio track requires
         *              video frame dropping
         *              vob->demuxer>0 and audio_pts > video_pts
         *    - case 3: fully PTS based sync modes requested
         */
        if ((MAY_SET(DEMUX) && (vob->attributes & TC_INFO_NO_DEMUX))
         || (MAY_SET(DEMUX) && (vinfo->pts_start < vinfo->track[track].pts_start))
         || (vob->demuxer == 3 || vob->demuxer == 4)
        ) {
            if (MAY_SET(AVSHIFT))
                vob->sync = D_arg;
            if (MAY_SET(AV_FINE))
                vob->sync_ms = D_arg_ms;
        }

        /* Set starting presentation unit */
        if (MAY_SET(SEEK)) {
            if (vinfo->unit_cnt > 0)
                vob->ps_unit = vinfo->unit_cnt;
        }

        /* Set format/codec flags and miscellaneous fields */
        if (vinfo->magic)
            vob->v_format_flag = vinfo->magic;
        if (vinfo->codec)
            vob->v_codec_flag = vinfo->codec;
        vob->pts_start = vinfo->pts_start;

        /* If the width or height are 0, assume no video was detected
         * (FIXME: what about -g?) */
        if (vinfo->width == 0 || vinfo->height == 0)
            vob->has_video = 0;

        /* If no separate audio file was found, use the video file for
         * audio processing */
        if (!ainfo)
            ainfo = vinfo;

    }  // if (vinfo)

    if (ainfo) {

        /* Set audio format parameters */
        if (MAY_SET(RATE)) {
            if (ainfo->track[track].samplerate > 0)
                vob->a_rate = ainfo->track[track].samplerate;
        }
        if (MAY_SET(BITS)) {
            if (ainfo->track[track].bits > 0)
                vob->a_bits = ainfo->track[track].bits;
        }
        if (MAY_SET(CHAN)) {
            if (ainfo->track[track].chan > 0)
                vob->a_chan = ainfo->track[track].chan;
        }

        /* Set audio codec, if not set by user */
        if (MAY_SET(ACODEC)) {
            if (ainfo->track[track].format > 0)
                vob->a_codec_flag = ainfo->track[track].format;
        }

        /* Set format flag and miscellaneous fields */
        if (ainfo->magic)
            vob->a_format_flag = ainfo->magic;
        if (ainfo->track[track].bitrate > 0)
            vob->a_stream_bitrate = ainfo->track[track].bitrate;
        if (ainfo->track[track].padrate > 0)
            vob->a_padrate = ainfo->track[track].padrate;
        if (ainfo->track[track].lang > 0)
            vob->lang_code = ainfo->track[track].lang;

        /* See if audio was detected */
        if (ainfo->num_tracks == 0)
            vob->has_audio = 0;
        if (ainfo->track[track].format == CODEC_NULL)
            vob->has_audio_track = 0;

        /* Set video format/codec fields as well if no video present */
        if (!vinfo) {
            if (ainfo->magic)
                vob->v_format_flag = ainfo->magic;
            if (ainfo->codec)
                vob->v_codec_flag = ainfo->codec;
        }

    }  // if (ainfo)

    /* Make note of whether the input is an XML file */
    if (vinfo && vinfo->magic != vinfo->magic_xml)
        vob->vmod_probed_xml = "xml";
    else
        vob->vmod_probed_xml = NULL;
    if (ainfo && ainfo->magic != ainfo->magic_xml)
        vob->amod_probed_xml = "xml";
    else
        vob->amod_probed_xml = NULL;
}

/*************************************************************************/

/**
 * select_modules:  Use the results of probing the input files to set 
 * global parameters.
 *
 * Parameters: flags: TC_PROBE_NO_xxx flags.
 *               vob: Pointer to global data structure.
 * Return value: None.
 * Preconditions: vob != NULL
 */

static void select_modules(int flags, vob_t *vob)
{
    char *default_amod;


    vob->vmod_probed = NULL;
    vob->amod_probed = NULL;

    /* If no video or audio, use null module */
    if (!vob->has_video) {
        vob->vmod_probed = "null";
        vob->im_v_width = 0;
        vob->im_v_height = 0;
    }
    if (!vob->has_audio) {
        vob->amod_probed = "null";
        vob->a_rate = 0;
        vob->a_chan = 0;
    }

    /* Choose a default audio module based on the audio codec */
    switch (vob->a_codec_flag) {
        case CODEC_MP2:    default_amod = "mp3";  break;
        case CODEC_MP3:    default_amod = "mp3";  break;
        case CODEC_A52:    default_amod = "ac3";  break;
        case CODEC_AC3:    default_amod = "ac3";  break;
        case CODEC_PCM:    default_amod = "raw";  break;
        case CODEC_VORBIS: default_amod = "ogg";  break;
        case CODEC_VAG:    default_amod = "vag";  break;
        default:           default_amod = "null"; break;
    }

    /* Choose modules based on file format */

    switch (vob->v_format_flag) {

      case TC_MAGIC_MPLAYER:
        vob->vmod_probed = "mplayer";
        vob->amod_probed = "mplayer";
        break;

      case TC_MAGIC_V4L_VIDEO:
        vob->vmod_probed = "v4l";
        if (MAY_SET(FRAMESIZE)) {
            vob->im_v_width  = PAL_W/2;
            vob->im_v_height = PAL_H/2;
            if (vob->im_v_codec != CODEC_RGB)
                vob->im_v_width &= -16;
        }
        break;

      case TC_MAGIC_V4L2_VIDEO:
        vob->vmod_probed = "v4l2";
        if (MAY_SET(FRAMESIZE)) {
            vob->im_v_width  = PAL_W/2;
            vob->im_v_height = PAL_H/2;
            if (vob->im_v_codec != CODEC_RGB)
                vob->im_v_width &= -16;
        }
        break;

      case TC_MAGIC_BKTR_VIDEO:
        vob->vmod_probed = "bktr";
        if (MAY_SET(FRAMESIZE) && !(vob->im_v_width>0 && vob->im_v_height>0)) {
            vob->im_v_width  = PAL_W/2;
            vob->im_v_height = PAL_H/2;
            if (vob->im_v_codec != CODEC_RGB)
                vob->im_v_width &= -16;
        }
        break;

      case TC_MAGIC_YUV4MPEG:
        vob->vmod_probed = "yuv4mpeg";
        break;

      case TC_MAGIC_BSDAV:
        vob->vmod_probed = "bsdav";
        break;

      case TC_MAGIC_NUV:
        vob->vmod_probed = "nuv";
        vob->amod_probed = "nuv";
        break;

      case TC_MAGIC_OGG:
        vob->vmod_probed = "ogg";
        vob->amod_probed = "ogg";

      case TC_MAGIC_DVD_NTSC:
        if (MAY_SET(DEMUX)) {
            if (vob->demuxer < 0)
                vob->demuxer = 1;
            /* Activate special handling for 24fps video */
            if (vob->fps < PAL_FPS && (vob->demuxer==1 || vob->demuxer==3))
                vob->demuxer++;
        }
        /* Fall through to common DVD handling */
      case TC_MAGIC_DVD_PAL:
        vob->vmod_probed = "dvd";
        vob->amod_probed = "dvd";
        break;

      case TC_MAGIC_AVI:
        if (vob->pass_flag & TC_VIDEO)
            vob->vmod_probed = "avi";
        break;

      case TC_MAGIC_MOV:
        vob->vmod_probed = "mov";
        break;

      case TC_MAGIC_TS:
        vob->vmod_probed = "ts";

      case TC_MAGIC_TIFF1:
      case TC_MAGIC_TIFF2:
      case TC_MAGIC_JPEG:
      case TC_MAGIC_PPM:
      case TC_MAGIC_PGM:
      case TC_MAGIC_BMP:
      case TC_MAGIC_PNG:
      case TC_MAGIC_GIF:
      case TC_MAGIC_SGI:
        vob->vmod_probed = "im";
        break;

      case TC_MAGIC_DV_NTSC:
      case TC_MAGIC_DV_PAL:
        if (vob->pass_flag & TC_VIDEO)
            vob->vmod_probed = "dv";
        break;

      case TC_MAGIC_CDXA:
        vob->vmod_probed = "vob";
        vob->amod_probed = "vob";
        break;

      case TC_MAGIC_MP3:
        vob->amod_probed = "mp3";
        break;

      case TC_MAGIC_AC3:
        vob->amod_probed = "ac3";
        break;

    }  // switch (vob->v_format_flag)

    switch (vob->a_format_flag) {
        case TC_MAGIC_V4L_AUDIO:   vob->amod_probed = "v4l";   break;
        case TC_MAGIC_V4L2_AUDIO:  vob->amod_probed = "v4l2";  break;
        case TC_MAGIC_SUNAU_AUDIO: vob->amod_probed = "sunau"; break;
        case TC_MAGIC_OSS_AUDIO:   vob->amod_probed = "oss";   break;
        case TC_MAGIC_BSDAV:       vob->amod_probed = "bsdav"; break;
        case TC_MAGIC_WAV:         vob->amod_probed = "raw";   break;
        case TC_MAGIC_MOV:         vob->amod_probed = "mov";   break;
        case TC_MAGIC_TS:          vob->amod_probed = "ts";    break;
        case TC_MAGIC_MP3:         vob->amod_probed = "mp3";   break;
        case TC_MAGIC_AC3:         vob->amod_probed = "ac3";   break;
        case TC_MAGIC_AVI:
          if (vob->pass_flag & TC_AUDIO)
              vob->amod_probed = "avi";
          break;
    }  // switch (vob->a_format_flag)

    /* Choose modules based on codec */
    switch (vob->v_codec_flag) {

      case TC_CODEC_DV:
        if (!vob->vmod_probed)
            vob->vmod_probed = "dv";
        if (!vob->amod_probed) {
            if (vob->v_format_flag == TC_MAGIC_AVI)
                vob->amod_probed = default_amod;
            else
                vob->amod_probed = "dv";
        }
        break;

      case TC_CODEC_MPEG:
      case TC_CODEC_M2V:
      case TC_CODEC_MPEG1:
        if (!vob->vmod_probed)
            vob->vmod_probed = "mpeg2";
        if (!vob->amod_probed)
            vob->amod_probed = default_amod;
        break;

      case TC_CODEC_MPEG2:
        if (!vob->vmod_probed) {
            vob->vmod_probed = "vob";
            if (MAY_SET(DEMUX)) {
                if (vob->demuxer < 0)
                    vob->demuxer = 1;
                /* Activate special handling for 24fps video */
                if (vob->fps < PAL_FPS && (vob->demuxer==1 || vob->demuxer==3))
                    vob->demuxer++;
            }
        }
        if (!vob->amod_probed)
            vob->amod_probed = vob->has_audio ? "vob" : "null";
        break;

      case TC_CODEC_MJPG:
      case TC_CODEC_MPG1:
      case TC_CODEC_MP42:
      case TC_CODEC_MP43:
      case TC_CODEC_RV10:
      case TC_CODEC_ASV1:
      case TC_CODEC_ASV2:
      case TC_CODEC_FFV1:
        if (!vob->vmod_probed)
            vob->vmod_probed = "ffmpeg";
        if (!vob->amod_probed)
            vob->amod_probed = default_amod;
        break;

      case TC_CODEC_LZO1:
      case TC_CODEC_LZO2:
        /* Overwrite video import module selected from format */
        vob->vmod_probed = "lzo";
        if (!vob->amod_probed)
            vob->amod_probed = default_amod;
        break;

      case TC_CODEC_THEORA:
        if (vob->v_format_flag != TC_MAGIC_OGG && !vob->vmod_probed)
            vob->vmod_probed = "mplayer";
        if (!vob->amod_probed)
            vob->amod_probed = default_amod;
        break;

      case TC_CODEC_DIVX3:
      case TC_CODEC_DIVX4:
      case TC_CODEC_DIVX5:
      case TC_CODEC_XVID:
        if (vob->v_format_flag != TC_MAGIC_OGG && !vob->vmod_probed)
            vob->vmod_probed = "ffmpeg";
        if (!vob->amod_probed)
            vob->amod_probed = default_amod;
        break;

      case TC_CODEC_YUV420P:
      case TC_CODEC_YUV422P:
      case TC_CODEC_RGB:
        if (!vob->vmod_probed)
            vob->vmod_probed = "raw";
        if (!vob->amod_probed)
            vob->amod_probed = default_amod;
        break;

    }  // switch (vob->v_codec_flag)

    /* If still not known, default to the null module */
    if (!vob->vmod_probed)
        vob->vmod_probed = "null";
    if (!vob->amod_probed)
        vob->amod_probed = "null";

    /* Set XML import modules */
    if (!vob->vmod_probed_xml)
        vob->vmod_probed_xml = vob->vmod_probed;
    if (!vob->amod_probed_xml)
        vob->amod_probed_xml = vob->amod_probed;
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
