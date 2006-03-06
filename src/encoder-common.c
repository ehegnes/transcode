/*
 *  encoder-common.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *  Updated and partially rewritten by
 *  Francesco Romani -January 2006
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#include <pthread.h>
#include "tc_defaults.h"

static volatile int exit_flag = TC_FALSE;

void tc_export_stop_nolock(void)
{
    exit_flag = TC_TRUE;
}

int tc_export_stop_requested(void)
{
    return exit_flag;
}


/* counter, for stats and more */
static uint32_t frames_encoded = 0;
static uint32_t frames_dropped = 0;
static uint32_t frames_skipped = 0;
static uint32_t frames_cloned = 0;
/* counters can be accessed by other (ex: import) threads */
static pthread_mutex_t frame_counter_lock = PTHREAD_MUTEX_INITIALIZER;

uint32_t tc_get_frames_encoded(void)
{
    uint32_t val;
    
    pthread_mutex_lock(&frame_counter_lock);
    val = frames_encoded;
    pthread_mutex_unlock(&frame_counter_lock);

    return val;
}

void tc_update_frames_encoded(uint32_t val)
{
    pthread_mutex_lock(&frame_counter_lock);
    frames_encoded += val;
    pthread_mutex_unlock(&frame_counter_lock);
}

uint32_t tc_get_frames_dropped(void)
{
    uint32_t val;
    
    pthread_mutex_lock(&frame_counter_lock);
    val = frames_dropped;
    pthread_mutex_unlock(&frame_counter_lock);
    
    return val;
}

void tc_update_frames_dropped(uint32_t val)
{
    pthread_mutex_lock(&frame_counter_lock);
    frames_dropped += val;
    pthread_mutex_unlock(&frame_counter_lock);
}

uint32_t tc_get_frames_skipped(void)
{
    uint32_t val;
    
    pthread_mutex_lock(&frame_counter_lock);
    val = frames_skipped;
    pthread_mutex_unlock(&frame_counter_lock);
    
    return val;
}

void tc_update_frames_skipped(uint32_t val)
{
    pthread_mutex_lock(&frame_counter_lock);
    frames_skipped += val;
    pthread_mutex_unlock(&frame_counter_lock);
}

uint32_t tc_get_frames_cloned(void)
{
    uint32_t val;
    
    pthread_mutex_lock(&frame_counter_lock);
    val = frames_cloned;
    pthread_mutex_unlock(&frame_counter_lock);
    
    return val;
}

void tc_update_frames_cloned(uint32_t val)
{
    pthread_mutex_lock(&frame_counter_lock);
    frames_cloned += val;
    pthread_mutex_unlock(&frame_counter_lock);
}

uint32_t tc_get_frames_skipped_cloned(void)
{
    uint32_t s, c;
    
    pthread_mutex_lock(&frame_counter_lock);
    s = frames_skipped;
    c = frames_cloned;
    pthread_mutex_unlock(&frame_counter_lock);
    
    return(c - s);
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
