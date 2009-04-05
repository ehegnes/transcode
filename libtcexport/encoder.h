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

#include "transcode.h"
#include "framebuffer.h"
#include "export.h"

#include "libtcmodule/tcmodule-core.h"

/*
 * MULTITHREADING WARNING:
 * It is in general *NOT SAFE* to call functions declared on this header from
 * different threads. See comments below.
 */

/*************************************************************************/


/*************************************************************************
 * helper routines. Client code needs to call those routines before
 * (tc_export_init/tc_export_setup) or after (tc_export_shutdown)
 * all the others.
 */

/*
 * tc_export_init:
 *      select a TCEncoderBuffer and a TCFactory to use for further
 *      operations. Both buffer and factory will be used until a new
 *      call to tc_export_init occurs.
 *      PLEASE NOTE: there NOT are sensible defaults, so client
 *      cose NEEDS to call this function as first one in code using
 *      encoder.
 *
 * Parameters:
 *      buffer: TCEncoderBuffer to use in future encoding loops.
 *     factory: TCFactory to use for future module (un)loading.
 * Return Value:
 *       0: succesfull
 *      !0: given one or more bad (NULL) value(s).
 */
int tc_encoder_setup(vob_t *vob, TCEncoderBuffer *buffer, TCFactory factory);

/*
 * tc_export_shutdown:
 *      revert operations done by tc_export_setup, unloading encoder and
 *      multiplexor modules.
 *
 * Parameters:
 *      None.
 * Return Value:
 *      None.
 * Preconditions:
 *      tc_export_setup() was previously called. To call this function if
 *      tc_export_setup() wasn't called will cause undefined behaviour.
 */
void tc_encoder_shutdown(void);


/*************************************************************************/

/*************************************************************************
 * main encoder API.
 *
 * There isn't explicit reference to encoder data structure,
 * so there always be one and only one global hidden encoder instance.
 * In current (and in the prevedible future) doesn't make sense to
 * have more than one encoder, so it's instance is global, hidden, implicit.
 *
 * PLEASE NOTE:
 * current encoder does not _explicitely_ use more than one thread.
 * This means that audio and video encoding, as well as multiplexing, happens
 * sequentially on the same (and unique) encoder thread.
 * It's definitively possible (and already happens) that real encoder code loaded
 * by modules uses internally more than one thread, but this is completely opaque
 * to encoder.
 */

/*
 * tc_encoder_init:
 *     initialize the A/V encoders, by (re)configuring encoder modules.
 *
 * Parameters:
 *     vob: pointer to vob_t.
 *          tc_encoder_init need to fetch from a vob structure some informations
 *          needed by it's inizalitation.
 * Return Value:
 *     -1: error configuring modules. Reason of error will be notified
 *         via tc_log*().
 *      0: succesfull.
 */
int tc_encoder_init(vob_t *vob);

/*
 * tc_encoder_loop:
 *      encodes a range of frames from stream(s) using given settings.
 *      This is the main and inner encoding loop.
 *      Encoding usually halts with last frame in range is encountered, but
 *      it can also stop if some error happens when acquiring new frames,
 *      or, of course, if there is an asynchronous stop request
 *      Please note that FIRST frame in given range will be encoded, but
 *      LAST frame in given range will NOT.
 *
 * Parameters:
 *         vob: pointer to a vob_t structure holding both informations about
 *              input streams and settings for output streams
 *              (i.e.: bitrate, GOP size...).
 * frame_first: sequence number of first frame in range to encode.
 *              All frames before this one will be acquired via
 *              TCEncoderBuffer routines, but will also be discarded.
 *  frame_last: sequence number of last frame in range to encode.
 *              *encoding halts when this frame is acquired*, so this
 *              frame will NOT encoded.
 * Return Value:
 *      None.
 * Preconditions:
 *      encoder properly initialized. This means:
 *      tc_export_init() called succesfully;
 *      tc_export_setup() called succesfully;
 *      tc_encoder_init() called succesfully;
 *      tc_encoder_open() called succesfully;
 */
void tc_encoder_loop(vob_t *vob, int frame_first, int frame_last);

/*
 * tc_encoder_stop:
 *      stop both the audio and the video encoders.
 *
 * Parameters:
 *      None.
 * Return Value:
 *      0: succesfull.
 *     <0: failure, reason will be notified via tc_log*().
 */
int tc_encoder_stop(void);

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
