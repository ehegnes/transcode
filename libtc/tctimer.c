/*
 * tctimer.h - simple timer code for transcode
 * (C) 2006 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#ifndef TCTIMER_H
#define TCTIMER_H

#include "config.h"

#ifndef HAVE_GETTIMEOFDAY
# error "this module REQUIRES gettimeofday presence!"
#endif

#include <sys/time.h>
#include <time.h>
#include <errno.h>

#ifdef OS_LINUX /* RTC timing requires file acces */
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/ioctl.h>
# include <unistd.h>
# include <fcntl.h>
#endif /* OS_LINUX */

#include "tctimer.h"
#include "libtc.h"

/*
 * Internal time representation:
 * XXX WRITEME
 */

typedef struct tctimer_ TCTimer;
struct tctimer_ {
    uint32_t last_time;

    int (*fini)(TCTimer *timer);
    uint32_t (*elapsed)(TCTimer *timer);
    int (*sleep)(TCTimer *timer, uint32_t amount);
};


int tc_timer_init_soft(TCTimer *timer, uint16_t frequency);

/*************************************************************************/
/* utilities */

static inline uint32_t tc_timer_timeval_to_microsecs(struct timeval *tv)
{
    return (tv->tv_sec * 1000000 + tv->tv_usec);
}

static uint32_t tc_timer_current(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return tc_timer_timeval_to_microsecs(&tv);
}  

/*************************************************************************/
/* generics */

/*************************************************************************/

static uint32_t tc_timer_generic_elapsed(TCTimer *timer)
{
    uint32_t r = 0, t = tc_timer_current();
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

static int tc_timer_soft_sleep(TCTimer *timer, uint32_t amount)
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
        timer->last_time = tc_timer_current();

        timer->elapsed = tc_timer_generic_elapsed;
        timer->sleep = tc_timer_soft_sleep;
        timer->fini = tc_timer_soft_fini;

        ret = 0;
    }
    return ret;
}

int tc_timer_init_best(TCTimer *timer, uint16_t frequency)
{
    tc_log_info(__FILE__, "Intializing software timer...");
    return tc_timer_init_soft(timer, frequency);
}

#endif /* TCTIMER_H */
