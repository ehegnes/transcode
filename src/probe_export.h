/*
 *  probe_export.h
 *
 *  Copyright (C) Tilmann Bitterberg - October 2003
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

#ifndef PROBE_EXPORT_H
#define PROBE_EXPORT_H

// add flags here
// possible flags are bitrate, etc
// if flag is set, use the extensions provided by the user.
// otherwise use the ones the export modules suggests.

#define TC_PROBE_NO_EXPORT_VEXT      (1<< 0) // --ext X,..
#define TC_PROBE_NO_EXPORT_AEXT      (1<< 1) // --ext ..,X
#define TC_PROBE_NO_EXPORT_VFMT      (1<< 2) // unused
#define TC_PROBE_NO_EXPORT_AFMT      (1<< 3) // unused
#define TC_PROBE_NO_EXPORT_VBITRATE  (1<< 4) // -w
#define TC_PROBE_NO_EXPORT_ABITRATE  (1<< 5) // -b
#define TC_PROBE_NO_EXPORT_FIELDS    (1<< 6) // --encode_fields
#define TC_PROBE_NO_EXPORT_VMODULE   (1<< 7) // -y X,..
#define TC_PROBE_NO_EXPORT_AMODULE   (1<< 8) // -y ..,X
#define TC_PROBE_NO_EXPORT_FRC       (1<< 9) // --export_fps ..,X
#define TC_PROBE_NO_EXPORT_FPS       (1<<10) // --export_fps X,..
#define TC_PROBE_NO_EXPORT_VCODEC    (1<<11) // -F
#define TC_PROBE_NO_EXPORT_ACODEC    (1<<12) // -N
#define TC_PROBE_NO_EXPORT_ARATE     (1<<13) // -E X,..,..
#define TC_PROBE_NO_EXPORT_ABITS     (1<<14) // -E ..,X,..
#define TC_PROBE_NO_EXPORT_ACHANS    (1<<15) // -E ..,..,X
#define TC_PROBE_NO_EXPORT_VMAXRATE  (1<<16) // unused
#define TC_PROBE_NO_EXPORT_VMINRATE  (1<<17) // unused
#define TC_PROBE_NO_EXPORT_ASR       (1<<18) // --export_asr
#define TC_PROBE_NO_EXPORT_PAR       (1<<19) // --export_par
#define TC_PROBE_NO_EXPORT_GOP       (1<<20) // divx key frames

extern unsigned int probe_export_attributes;

extern char *audio_ext;
extern char *video_ext;


#endif
