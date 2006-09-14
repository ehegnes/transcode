/*
 *  tccodecs.h
 *
 *  Copyright (C) Thomas Oestreich - June 2001
 *  ripped from 'magic.h' by Francesco Romani - November 2005
 *
 *  This file is part of transcode, a video stream  processing tool
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

#ifndef TC_CODECS_H
#define TC_CODECS_H

#include <stdint.h>

typedef uint32_t TCCodecID;
/* just an unsigned integer big enough to store any TC_CODEC_* */

/*
 * codecs identifiers.
 * Each one must fit in exactly 32 bits.
 * And they should be probably moved to an enum.
 */

#define TC_CODEC_ERROR                 0xFFFFFFFF
#define TC_CODEC_ANY                   0xFFFFFFFE
#define TC_CODEC_UNKNOWN               0x00000000
#define TC_CODEC_RAW                   0xFEFEFEFE
#define TC_CODEC_PCM                   0x00000001
#define TC_CODEC_RGB                   0x00000024
#define TC_CODEC_AC3                   0x00002000
#define TC_CODEC_DTS                   0x0001000f
#define TC_CODEC_YV12                  0x32315659
#define TC_CODEC_YUV420P               0x30323449  /* I420 */
#define TC_CODEC_YUV422P               0x42323459  /* Y42B, see lavc/raw.c */
#define TC_CODEC_UYVY                  0x59565955
#define TC_CODEC_YUV2                  0x32565559
#define TC_CODEC_YUY2                  0x32595559
#define TC_CODEC_M2V                   0x000001b3
#define TC_CODEC_MPEG                  0x01000000
#define TC_CODEC_MPEG1                 0x00100000
#define TC_CODEC_MPEG1VIDEO            0x00100002
#define TC_CODEC_MPEG2                 0x00010000
#define TC_CODEC_MPEG2VIDEO            0x00010002
#define TC_CODEC_MPEG4VIDEO            0x00001002
#define TC_CODEC_DV                    0x00001000
#define TC_CODEC_MP3                   0x00000055
#define TC_CODEC_MP2                   0x00000050
#define TC_CODEC_NUV                   0x4e757070
#define TC_CODEC_PS1                   0x00007001
#define TC_CODEC_PS2                   0x00007002
#define TC_CODEC_DIVX3                 0x000031B3
#define TC_CODEC_MP42                  0x000031B4
#define TC_CODEC_MP43                  0x000031B5
#define TC_CODEC_DIVX4                 0x000041B6
#define TC_CODEC_DIVX5                 0x000051B6
#define TC_CODEC_XVID                  0x58766944
#define TC_CODEC_H264                  0x34363248
#define TC_CODEC_MJPG                  0xA0000010
#define TC_CODEC_MPG1                  0xA0000012
#define TC_CODEC_SUB                   0xA0000011
#define TC_CODEC_THEORA                0x00001234
#define TC_CODEC_VORBIS                0x0000FFFE
#define TC_CODEC_LZO1                  0x0001FFFE
#define TC_CODEC_RV10                  0x0002FFFE
#define TC_CODEC_SVQ1                  0x0003FFFE
#define TC_CODEC_SVQ3                  0x0004FFFE
#define TC_CODEC_VP3                   0x0005FFFE
#define TC_CODEC_4XM                   0x0006FFFE
#define TC_CODEC_WMV1                  0x0007FFFE
#define TC_CODEC_WMV2                  0x0008FFFE
#define TC_CODEC_HUFFYUV               0x0009FFFE
#define TC_CODEC_INDEO3                0x000AFFFE
#define TC_CODEC_H263P                 0x000BFFFE
#define TC_CODEC_H263I                 0x000CFFFE
#define TC_CODEC_LZO2                  0x000DFFFE
#define TC_CODEC_FRAPS                 0x000EFFFE
#define TC_CODEC_FFV1                  0x000FFFFE
#define TC_CODEC_ASV1                  0x0010FFFE
#define TC_CODEC_ASV2                  0x0011FFFE
#define TC_CODEC_VAG                   0x0000FEED
#define TC_CODEC_PV3                   0x50563301

#endif // TC_CODECS_H
