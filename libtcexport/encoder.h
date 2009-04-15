/*
 *  encoder.h - interface for the main encoder loop in transcode
 *
 *  Copyright (C) Thomas Oestreich - June 2001
 *  Updated and partially rewritten by
 *  Francesco Romani - January 2006
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

#ifndef ENCODER_H
#define ENCODER_H

#include "libtcmodule/tcmodule-core.h"

#include "transcode.h"
#include "framebuffer.h"

/*
 * MULTITHREADING WARNING:
 * It is in general *NOT SAFE* to call functions declared on this header from
 * different threads. See comments below.
 */

/*************************************************************************/

typedef struct tcencoder_ TCEncoder;
struct tcencoder_ {
    vob_t           *vob;
    TCFactory       factory;

    TCModule        vid_mod;
    TCModule        aud_mod;
};

/*************************************************************************/

int tc_encoder_init(TCEncoder *enc, vob_t *vob, TCFactory factory);

int tc_encoder_fini(TCEncoder *enc);

/*************************************************************************/

int tc_encoder_setup(TCEncoder *enc,
                     const char *vid_mod, const char *aud_mod);

void tc_encoder_shutdown(TCEncoder *enc);


/*************************************************************************/

int tc_encoder_open(TCEncoder *enc,
                    TCModuleExtraData *vid_xdata,
                    TCModuleExtraData *aud_xdata)

int tc_encoder_process(TCEncoder *enc,
                       TCFrameVideo *vin, TCFrameVideo *vout,
                       TCFrameAudio *ain, TCFrameAudio *aout);

int tc_encoder_flush(TCEncoder *enc,
                     TCFrameVideo *vout, TCFrameAudio *aout);

int tc_encoder_close(TCEncoder *enc);

/*************************************************************************/

#endif /* ENCODER_H */

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
