/*
 * multiplexor.h -- interface for the multiplexor in transcode
 * (C) 2009 Francesco Romani <fromani at gmail dot com>
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

#ifndef MULTIPLEXOR_H
#define MULTIPLEXOR_H

#include "libtcmodule/tcmodule-core.h"

#include "transcode.h"
#include "framebuffer.h"

/*
 * MULTITHREADING WARNING:
 * It is in general *NOT SAFE* to call functions declared on this header from
 * different threads. See comments below.
 */

/*************************************************************************
 * new-style output rotation support.
 * This couple of functions
 *      tc_multiplexor_limit_frames
 *      tc_multiplexor_limit_megabytes
 *
 * Allow the client code to automatically split output into chunks by
 * specifying a maxmimum size, resp. in frames OR in megabytes, for each
 * output chunk.
 *
 * Those functions MUST BE used BEFORE to call first tc_multiplexor_open(),
 * otherwise will fall into unspecifed behaviour.
 * It's important to note that client code CAN call multiple times
 * (even if isn't usually useful to do so ;) ) tc_multiplexor_limit*,
 * but only one limit can be used, so the last limit set will be used.
 * In other words, is NOT (yet) possible to limit output chunk size
 * BOTH by frames and by size.
 */

/*
 * tc_multiplexor_limit_frames:
 *     rotate output file(s) every given amount of encoded frames.
 *
 * Parameters:
 *        vob: pointer to main vob_t structure.
 *     frames: maximum of frames that every output chunk should contain.
 * Return value:
 *     None.
 */
void tc_multiplexor_limit_frames(vob_t *vob, uint32_t frames);

/*
 * tc_multiplexor_limit_megabytes:
 *     rotate output file(s) after a given amount of data was encoded.
 *
 * Parameters:
 *           vob: pointer to main vob_t structure.
 *     megabytes: maximum size that every output chunk should have.
 * Return value:
 *     None.
 */
void tc_multiplexor_limit_megabytes(vob_t *vob, uint32_t megabytes);


/*************************************************************************/

int tc_multiplexor_init(vob_t *vob, TCFactory factory);

int tc_multiplexor_fini(void);

/*
 * tc_multiplexor_open:
 *     open output file(s), by (re)configuring multiplexor module.
 *
 * Parameters:
 *     vob: pointer to vob_t.
 *          tc_multiplexor_open need to fetch from a vob structure some informations
 *          needed by it's inizalitation.
 * Return Value:
 *     -1: error configuring module(s) or opening file(s). Reason of error will be
 *         notified via tc_log*().
 *      0: succesfull.
 */
int tc_multiplexor_open(const char *sink_main, const char *sink_aux);

/*
 * tc_multiplexor_close:
 *      stop multiplexor and close output file.
 *
 * Parameters:
 *      None.
 * Return Value:
 *      0: succesfull.
 *     <0: failure, reason will be notified via tc_log*().
 */
int tc_multiplexor_close(void);


int tc_multiplexor_export(TCFrameVideo *vframe, TCFrameAudio *aframe);

/*************************************************************************/

#endif /* MULTIPLEXOR_H */

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
