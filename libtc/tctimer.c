/*
 * tctimer.c - simple timer code implementation for transcode
 * (C) 2006-2007 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License version 2.  See the file COPYING for details.
 */

#include "config.h"

#ifndef HAVE_GETTIMEOFDAY
# error "this module REQUIRES gettimeofday presence!"
#endif

#include <sys/time.h>
#include <time.h>
#include <errno.h>

#include "tctimer.h"
#include "libtc.h"

/*
 * Internal time representation:
 * XXX WRITEME
 */

/*************************************************************************/
/* utilities */

static inline uint64_t tc_timeval_to_microsecs(const struct timeval *tv)
{
    return (tv->tv_sec * 1000000 + tv->tv_usec);
}

uint64_t tc_gettime(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return tc_timeval_to_microsecs(&tv);
}  

/*************************************************************************/
/* generics */

/*************************************************************************/

static uint64_t tc_timer_generic_elapsed(TCTimer *timer)
{
    uint64_t r = 0, t = tc_gettime();
    r = t - timer->last_time;
    timer->last_time = t;
    return r;
}

/*************************************************************************/
/* timer-specific code */

/*************************************************************************/

static int tc_timer_soft_fini(TCTimer *timer)
{
    return 0; /* no internal state -> nothing to finalize */
}

static int tc_timer_soft_sleep(TCTimer *timer, uint64_t amount)
{
    struct timespec ts, tr;
    int ret;

    ts.tv_sec = amount / 1000000;
    ts.tv_nsec = (amount % 1000000) * 1000;

    do {
        ret = nanosleep(&ts, &tr);
        if (ret == -1) {
            if (errno != EINTR) {
                /* report fault */
                break;
            } else {
                /* reload */
                ts.tv_sec = tr.tv_sec;
                ts.tv_nsec = tr.tv_nsec;
            }
        }
    } while (ret != 0);
    return ret;
}

/*************************************************************************/
/* entry points */

/*************************************************************************/

int tc_timer_init_soft(TCTimer *timer, uint16_t frequency)
{
    int ret = -1;

    if (timer != NULL) {
        /* frequency: ignored, we relies on nanosleep() */
        timer->last_time = tc_gettime();

        timer->elapsed = tc_timer_generic_elapsed;
        timer->sleep = tc_timer_soft_sleep;
        timer->fini = tc_timer_soft_fini;

        ret = 0;
    }
    return ret;
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
