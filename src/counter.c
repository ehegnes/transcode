/*
 * counter.c - transcode progress counter routines
 * Written by Andrew Church <achurch@achurch.org>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "transcode.h"
#include "counter.h"
#include "frame_threads.h"

/*************************************************************************/

static double start_time;       /* Time at which counter was started */
static int counter_active = 0;  /* Is the counter active? */

/*************************************************************************/
/*************************************************************************/

/**
 * counter_init:  Reset the counter's start time.
 *
 * Parameters: None.
 * Return value: None.
 * Preconditions: None.
 * Postconditions: None.
 */

static void counter_init(void)
{
    struct timeval tv;
    /* We don't care about the timezone value, but pass a dummy argument
     * to gettimeofday() in case a broken implementation chokes on NULL */
    struct timezone dummy_tz = {0,0};

    if (gettimeofday(&tv, &dummy_tz) != 0) {
        static int warned = 0;
        if (!warned) {
            tc_log_warn(__FILE__, "gettimeofday() failed!");
            warned = 1;
        }
    }
    start_time = tv.tv_sec + (double)tv.tv_usec/1000000.0;
}

/*************************************************************************/

/**
 * counter_on:  Activate the counter display.
 *
 * Parameters: None.
 * Return value: None.
 * Preconditions: None.
 * Postconditions: None.
 */

void counter_on(void)
{
    counter_active = 1;
}

/*************************************************************************/

/**
 * counter_off:  Deactivate the counter display.
 *
 * Parameters: None.
 * Return value: None.
 * Preconditions: None.
 * Postconditions: None.
 */

void counter_off(void)
{
    counter_active = 0;
}

/*************************************************************************/

/**
 * counter_print:  Display the progress counter, if active.
 *
 * Parameters: encoding: True (nonzero) if frames are being encoded,
 *                       false (zero) if frames are being skipped.
 *                frame: Current frame being encoded or skipped.
 *                first: First frame of current range.
 *                 last: Last frame of current range, -1 if unknown.
 * Return value: None.
 * Preconditions: None.
 * Postconditions: None.
 */

void counter_print(int encoding, int frame, int first, int last)
{
    /* Were we encoding or skipping last time? (-1 = first call) */
    static int was_encoding = -1;
    struct timeval tv;
    struct timezone dummy_tz = {0,0};
    double now, fps;
    uint32_t buf1, buf2, buf3;

    if (!tc_progress_meter
     || !counter_active
     || !print_counter_interval
     || frame % print_counter_interval != 0
    ) {
        return;
    }

    encoding = (encoding != 0 ? 1 : 0);  // force to 1 or 0
    if (was_encoding != encoding) {
        counter_init();
        if (print_counter_cr && was_encoding != -1)
            printf("\n");
    }
    was_encoding = encoding;

    if (gettimeofday(&tv, &dummy_tz) != 0) {
        static int warned = 0;
        if (!warned) {
            tc_log_warn(__FILE__, "gettimeofday() failed!");
            warned = 1;
        }
        return;
    }
    now = tv.tv_sec + (double)tv.tv_usec/1000000.0;
    fps = (frame - first) / (now - start_time);
    if (fps <= 0 || fps >= 10000)
        return;

    pthread_mutex_lock(&vbuffer_im_fill_lock);
    buf1=vbuffer_im_fill_ctr;
    pthread_mutex_unlock(&vbuffer_im_fill_lock);

    pthread_mutex_lock(&vbuffer_xx_fill_lock);
    buf2=vbuffer_xx_fill_ctr;
    pthread_mutex_unlock(&vbuffer_xx_fill_lock);

    pthread_mutex_lock(&vbuffer_ex_fill_lock);
    buf3=vbuffer_ex_fill_ctr;
    pthread_mutex_unlock(&vbuffer_ex_fill_lock);

    if (last != -1) {
        double done = (double)(frame - first) / (double)(last+1 - first);
        int secleft = (last+1 - frame) / fps;
        printf("%s frame [%d/%d], %6.2f fps, %5.1f%%, ETA: %d:%02d:%02d,"
               " (%2d|%2d|%2d)%s",
               encoding ? "encoding" : "skipping",
               frame, last+1,
               fps,
               100*done,
               secleft/3600, (secleft/60) % 60, secleft % 60,
               buf1, buf2, buf3,
               print_counter_cr ? "  \r" : "\n"
        );
    } else {
        vob_t *vob = tc_get_vob();
        int time = (double)frame / ((vob->fps<1.0) ? 1.0 : vob->fps);
	printf("%s frames [%d-%d], %6.2f fps, EMT: %d:%02d:%02d,"
               "  (%2d|%2d|%2d)%s",
               encoding ? "encoding" : "skipping",
               first, frame,
               fps,
               time/3600, (time/60) % 60, time % 60,
               buf1, buf2, buf3,
               print_counter_cr ? "  \r" : "\n"
        );
    }
    fflush(stdout);
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
