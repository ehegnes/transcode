/*
 *  export.h -- the transcode export layer. Again.
 *
 *  Copyright (C) Thomas Oestreich - June 2001
 *  Updated and partially rewritten by
 *  Francesco Romani - January 2006/February 2009
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

#include "transcode.h"
#include "runcontrol.h"
#include "framebuffer.h"

#include "libtcmodule/tcmodule-core.h"


#ifndef EXPORT_H
#define EXPORT_H

/*
 * MULTITHREADING NOTE:
 * It is *GUARANTEED SAFE* to call the following functions 
 * from different threads.
 */
/*************************************************************************/

/*
 * tc_get_frames_{dropped,skipped,encoded,cloned,skipped_cloned}:
 *     get the current value of a frame counter.
 *
 * Parameters:
 *     None
 * Return Value:
 *     the current value of requested counter
 */
uint32_t tc_get_frames_dropped(void);
uint32_t tc_get_frames_skipped(void);
uint32_t tc_get_frames_encoded(void);
uint32_t tc_get_frames_cloned(void);
uint32_t tc_get_frames_skipped_cloned(void);

/*
 * tc_update_frames_{dropped,skipped,encoded,cloned}:
 *     update the current value of a frame counter of a given value.
 *
 * Parameters:
 *     val: value to be added to the current value of requested counter.
 *     This parameter is usually just '1' (one)
 * Return Value:
 *     None
 */
void tc_update_frames_dropped(uint32_t val);
void tc_update_frames_skipped(uint32_t val);
void tc_update_frames_encoded(uint32_t val);
void tc_update_frames_cloned(uint32_t val);

/**
 * tc_export_{audio,video}_notify:
 *      notify encoder that a new {audio,video} frame is ready
 *      to be encoded.
 *      You NEED to call those functions to properly syncronize encoder
 *      and avoid deadlocks.
 *
 * Parameters:
 *      None.
 * Return Value:
 *      None.
 */
void tc_export_audio_notify(void);
void tc_export_video_notify(void);

/*************************************************************************/

int tc_export_new(vob_t *vob, TCFactory factory, TCRunControl RC);

int tc_export_del(void);

/*
 * tc_export_setup:
 *      load export modules (encoders and multiplexor) using Module Factory
 *      selected via tc_export_init, checking if loaded modules are
 *      compatible with requested audio/video codec, and prepare for
 *      real encoding.
 *
 * Parameters:
 *     vob: pointer to vob_t.
 *          tc_export_setup need to fetch from a vob structure some informations
 *          needed by proper loading (es: module path).
 *   a_mod: name of audio encoder module to load.
 *   v_mod: name of video encoder module to load.
 *   m_mod: name of multiplexor module to load.
 * Return Value:
 *      0: succesfull
 *     <0: failure: failed to load one or more requested modules,
 *         *OR* there is at least one incompatibility between requested
 *         modules and requested codecs.
 *         (i.e. audio encoder module VS requested audio codec)
 *         (i.e. video encoder module VS multiplexor module)
 * Preconditions:
 *      Module Factory avalaible and selected using tc_export_init.
 */
int tc_export_setup(const char *a_mod, const char *v_mod, const char *m_mod);

int tc_export_setup_aux(const char *m_mod);


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
void tc_export_shutdown(void);




/*************************************************************************
 * new-style output rotation support.
 * This couple of functions
 *      tc_export_rotation_limit_frames
 *      tc_export_rotation_limit_megabytes
 *
 * Allow the client code to automatically split output into chunks by
 * specifying a maxmimum size, resp. in frames OR in megabytes, for each
 * output chunk.
 *
 * Those functions MUST BE used BEFORE to call first tc_export_open(),
 * otherwise will fall into unspecifed behaviour.
 * It's important to note that client code CAN call multiple times
 * (even if isn't usually useful to do so ;) ) tc_export_rotation_limit*,
 * but only one limit can be used, so the last limit set will be used.
 * In other words, is NOT (yet) possible to limit output chunk size
 * BOTH by frames and by size.
 */

/*
 * tc_export_rotation_limit_frames:
 *     rotate output file(s) every given amount of encoded frames.
 *
 * Parameters:
 *     frames: maximum of frames that every output chunk should contain.
 * Return value:
 *     None.
 */
void tc_export_rotation_limit_frames(uint32_t frames);

/*
 * tc_export_rotation_limit_megabytes:
 *     rotate output file(s) after a given amount of data was encoded.
 *
 * Parameters:
 *     megabytes: maximum size that every output chunk should have.
 * Return value:
 *     None.
 */
void tc_export_rotation_limit_megabytes(uint32_t megabytes);


/*************************************************************************/

/*************************************************************************
 * main export API.
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
 * tc_export_init:
 *     initialize the A/V encoders, by (re)configuring encoder modules.
 *
 * Parameters:
 *     None.
 * Return Value:
 *     -1: error configuring modules. Reason of error will be notified
 *         via tc_log*().
 *      0: succesfull.
 */
int tc_export_init(void);

/*
 * tc_export_open:
 *     open output file(s), by (re)configuring multiplexor module.
 *
 * Parameters:
 *     None.
 * Return Value:
 *     -1: error configuring module(s) or opening file(s). Reason of error will be
 *         notified via tc_log*().
 *      0: succesfull.
 */
int tc_export_open(void);

/*
 * tc_export_loop:
 *      encodes a range of frames from stream(s) using given settings.
 *      This is the main and inner encoding loop.
 *      Encoding usually halts with last frame in range is encountered, but
 *      it can also stop if some error happens when acquiring new frames,
 *      or, of course, if there is an asynchronous stop request
 *      Please note that FIRST frame in given range will be encoded, but
 *      LAST frame in given range will NOT.
 *
 * Parameters:
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
 *      tc_export_init() called succesfully;
 *      tc_export_open() called succesfully;
 */
void tc_export_loop(TCFrameSource *fs, int frame_first, int frame_last);

int tc_export_frames(int frame_id,
                     TCFrameVideo *vframe, TCFrameAudio *aframe);

int tc_export_flush(void);

/*
 * tc_export_stop:
 *      stop both the audio and the video encoders.
 *
 * Parameters:
 *      None.
 * Return Value:
 *      0: succesfull.
 *     <0: failure, reason will be notified via tc_log*().
 */
int tc_export_stop(void);

/*
 * tc_export_close:
 *      stop multiplexor and close output file.
 *
 * Parameters:
 *      None.
 * Return Value:
 *      0: succesfull.
 *     <0: failure, reason will be notified via tc_log*().
 */
int tc_export_close(void);


/*************************************************************************/

#endif /* EXPORT_H */

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
