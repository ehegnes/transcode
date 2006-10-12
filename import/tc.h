/*
 *  tc.h
 *
 *  Copyright (C) Thomas Oestreich - June 2001
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

#ifndef _TC_H
#define _TC_H

void decode_a52(decode_t *decode);
void extract_ac3(info_t *ipipe);

void decode_mp2(decode_t *decode);
void decode_mp3(decode_t *decode);
void extract_mp3(info_t *ipipe);

void decode_mpeg2(decode_t *decode);
void extract_mpeg2(info_t *ipipe);

void decode_yuv(decode_t *decode);
void extract_yuv(info_t *ipipe);

void extract_pcm(info_t *ipipe);
void extract_rgb(info_t *ipipe);

void extract_dv(info_t *ipipe);
void decode_dv(decode_t *decode);
void probe_dv(info_t *ipipe);

void tcdemux_thread(info_t *ipipe);
void tcprobe_thread(info_t *ipipe);

void extract_avi(info_t *ipipe);

void decode_lavc(decode_t *decode);
void decode_mov(decode_t *decode);

void extract_lzo(info_t *ipipe);
void decode_lzo(decode_t *decode);

void probe_yuv(info_t *ipipe);
void probe_nuv(info_t *ipipe);
void probe_wav(info_t *ipipe);
void probe_ac3(info_t *ipipe);
void probe_dts(info_t *ipipe);
void probe_mp3(info_t *ipipe);
void probe_avi(info_t *ipipe);
void probe_net(info_t *ipipe);
void probe_tiff(info_t *ipipe);
void probe_im(info_t *ipipe);
void probe_mov(info_t *ipipe);
void probe_xml(info_t *ipipe);
void probe_ogg(info_t *ipipe);
void extract_ogm(info_t *ipipe);
void decode_ogg(decode_t *decode);
void probe_vnc(info_t *ipipe);
void probe_v4l(info_t *ipipe);
void probe_bktr(info_t *ipipe);
void probe_sunau(info_t *ipipe);
void probe_bsdav(info_t *ipipe);
void probe_oss(info_t *ipipe);

void probe_mxf(info_t *ipipe);
void extract_mxf(info_t *ipipe);

void probe_dvd(info_t *ipipe);

void probe_mplayer(info_t *ipipe);

void probe_pv3(info_t *ipipe);

void probe_x11(info_t *ipipe);

void probe_ffmpeg(info_t *ipipe);

void probe_pvn(info_t *ipipe);

#endif
