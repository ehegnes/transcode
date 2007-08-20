/*
 *  encoder-common.h - miscelanous asynchronous encoder functions.
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

#ifndef ENCODER_COMMON_H
#define ENCODER_COMMON_H

/*
 * MULTITHREADING NOTE:
 * It is *GUARANTEED SAFE* to call those functions from different threads.
 */

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

/*
 * tc_pause_request:
 *     toggle pausing; if pausing is enabled, further calls to tc_pause()
 *     will effectively pause application's current thread; otherwise,
 *     tc_pause() calls will do just nothing.
 *
 * Parameters:
 *     None.
 * Return value:
 *     None.
 */
void tc_pause_request(void);

/*
 * tc_pause:
 *     if pausing enabled, so if tc_pause_request was previously called
 *     at least once, pause the current application thread for at least
 *     TC_DELAY_MIN microseconds.
 *
 * Parameters:
 *     None.
 * Return value:
 *     None.
 * Side effects:
 *     Incoming socket requests (see socket code), if any, will be handled
 *     before to return.
 */
void tc_pause(void);

/*
 * tc_export_stop_nolock():
 *     (asynchronously) request to encoder to exit from an encoding loop
 *     as soon as is possible.
 *
 *     multithread safe: a thread different from encoder thread can
 *                       safely use this function to stop the encoder.
 *
 * Parameters:
 *     None
 * Return Value:
 *     None
 * Preconditions:
 *     Calling this function _outside_ of an encoding loop
 *     make very little (or no) sense, but it will not harm anything.
 */
//void tc_export_stop_nolock(void);

/*
 * tc_export_stop_requested():
 *     check if encoder has received a stop request
 *     (via tc_export_stop_nolock)
 *
 *     multithread safe: a thread different from encoder thread can
 *                       safely use this function to stop the encoder.
 *
 *     this function is mainly used by the incoder itself into an
 *     encoding loop.
 * Parameters:
 *     None
 * Return Value:
 *     1 if encoder stop was requested
 *     0 otherwise
 * Preconditions:
 *     Calling this function _outside_ of an encoding loop
 *     make very little (or no) sense, but it will not harm anything.
 */
//int tc_export_stop_requested(void);



void tc_pause(void);
void tc_pause_request(void);

#endif /* ENCODER_COMMON_H */
