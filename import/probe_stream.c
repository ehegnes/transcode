/*
 *  probe_stream.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  This file is part of transcode, a video stream processing tool
 *
 *  transcode is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  transcode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "transcode.h"
#include "tcinfo.h"
#include "ioaux.h"
#include "tc.h"
#include "aux_pes.h"
#include "libtc/libtc.h"


static ProbeInfo probe_info;

void tcprobe_thread(info_t *ipipe)
{
    verbose = ipipe->verbose;

    ipipe->probe_info = &probe_info;
    ipipe->probe = 1;

    /* data structure will be filled by subroutines */
    memset((char*) &probe_info, 0, sizeof(ProbeInfo));
    probe_info.magic = ipipe->magic;

    /* ------------------------------------------------------------
     * check file type/magic and take action to probe for contents
     * ------------------------------------------------------------*/

    switch(ipipe->magic) {
      case TC_MAGIC_MPLAYER:
        probe_mplayer(ipipe);
        break;

      case TC_MAGIC_AVI:
        probe_avi(ipipe);
        break;

      case TC_MAGIC_VNC:
        probe_vnc(ipipe);
        break;

      case TC_MAGIC_TIFF1:   /* image formats (multiple fallbacks) */
      case TC_MAGIC_TIFF2:
      case TC_MAGIC_JPEG:
      case TC_MAGIC_BMP:
      case TC_MAGIC_PNG:
      case TC_MAGIC_GIF:
      case TC_MAGIC_PPM:
      case TC_MAGIC_PGM:
      case TC_MAGIC_SGI:
        probe_im(ipipe); /* ImageMagick serve all */
        break;

      case TC_MAGIC_MXF:
        probe_mxf(ipipe);
        break;

      case TC_MAGIC_V4L_VIDEO:
      case TC_MAGIC_V4L_AUDIO:
        probe_v4l(ipipe);
        break;

      case TC_MAGIC_BKTR_VIDEO:
        probe_bktr(ipipe);
        break;

      case TC_MAGIC_SUNAU_AUDIO:
        probe_sunau(ipipe);
        break;

      case TC_MAGIC_BSDAV:
        probe_bsdav(ipipe);
        break;

      case TC_MAGIC_OSS_AUDIO:
        probe_oss(ipipe);
        break;

      case TC_MAGIC_OGG:
        probe_ogg(ipipe);
        break;

      case TC_MAGIC_CDXA:
        probe_pes(ipipe);
        ipipe->probe_info->attributes |= TC_INFO_NO_DEMUX;
        break;

      case TC_MAGIC_MPEG_PS: /* MPEG Program Stream */
      case TC_MAGIC_VOB:     /* backward compatibility fallback */
        probe_pes(ipipe);
        /* NTSC video/film check */
        if (verbose >= TC_DEBUG) {
            tc_log_msg(__FILE__, "att0=%d, att1=%d",
                       ipipe->probe_info->ext_attributes[0],
                       ipipe->probe_info->ext_attributes[1]);
        }
        if (ipipe->probe_info->codec == TC_CODEC_MPEG2
         && ipipe->probe_info->height == 480
         && ipipe->probe_info->width == 720) {
            if (ipipe->probe_info->ext_attributes[0] > 2 * ipipe->probe_info->ext_attributes[1]
             || ipipe->probe_info->ext_attributes[1] == 0) {
                ipipe->probe_info->is_video = 1;
            }

            if (ipipe->probe_info->is_video) {
                ipipe->probe_info->fps = NTSC_VIDEO;
                ipipe->probe_info->frc = 4;
            } else {
                ipipe->probe_info->fps = NTSC_FILM;
                ipipe->probe_info->frc = 1;
            }
        }

        if (ipipe->probe_info->codec == TC_CODEC_MPEG1) {
            ipipe->probe_info->magic=TC_MAGIC_MPEG_PS;
        }

        /*
         * check for need of special import module,
         * that does not rely on 2k packs
         */
        if (ipipe->probe_info->attributes & TC_INFO_NO_DEMUX) {
            ipipe->probe_info->codec = TC_CODEC_MPEG;
            ipipe->probe_info->magic=TC_MAGIC_MPEG_PS; /* XXX: doubtful */
        }
        break;

      case TC_MAGIC_MPEG_ES: /* MPEG Elementary Stream */
      case TC_MAGIC_M2V:     /* backward compatibility fallback */
        probe_pes(ipipe);
        /* make sure not to use the demuxer */
        ipipe->probe_info->codec=TC_CODEC_MPEG;
        ipipe->probe_info->magic=TC_MAGIC_MPEG_ES;
        break;

      case TC_MAGIC_MPEG_PES:/* MPEG Packetized Elementary Stream */
      case TC_MAGIC_MPEG:    /* backward compatibility fallback */
        probe_pes(ipipe);
        ipipe->probe_info->attributes |= TC_INFO_NO_DEMUX;
        break;

      case TC_MAGIC_DVD:
      case TC_MAGIC_DVD_PAL:
      case TC_MAGIC_DVD_NTSC:
        probe_dvd(ipipe);
        break;

      case TC_MAGIC_YUV4MPEG:
        probe_yuv(ipipe);
        break;

      case TC_MAGIC_NUV:
        probe_nuv(ipipe);
        break;

      case TC_MAGIC_MOV:
        probe_mov(ipipe);
        break;

      case TC_MAGIC_XML:
        probe_xml(ipipe);
        break;

      case TC_MAGIC_TS:
        probe_ts(ipipe);
        break;

      case TC_MAGIC_WAV:
        probe_wav(ipipe);
        break;

      case TC_MAGIC_DTS:
        probe_dts(ipipe);
        break;

      case TC_MAGIC_AC3:
        probe_ac3(ipipe);
        break;

      case TC_MAGIC_MP3:
      case TC_MAGIC_MP3_2:
      case TC_MAGIC_MP3_2_5:
      case TC_MAGIC_MP2:
        probe_mp3(ipipe);
        break;

      case TC_MAGIC_DV_PAL:
      case TC_MAGIC_DV_NTSC:
        probe_dv(ipipe);
        ipipe->probe_info->magic=TC_MAGIC_DV_PAL;
        break;

      case TC_MAGIC_PV3:
        probe_pv3(ipipe);
        break;

      case TC_MAGIC_X11:
        probe_x11(ipipe);
        break;

      default:
        ipipe->error=2;
    }
    if (ipipe->magic == TC_MAGIC_XML) {
        ipipe->probe_info->magic_xml=TC_MAGIC_XML;
        /*
         * used in transcode to load import_xml and to have
         * the correct type of the video/audio
         */
    } else {
        ipipe->probe_info->magic_xml=ipipe->probe_info->magic;
    }

    return;
}

