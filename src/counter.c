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

static int counter_active = 0;  /* Is the counter active? */

/*************************************************************************/
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
    struct timeval tv;
    struct timezone dummy_tz = {0,0};
    double now, fps;
    uint32_t buf1, buf2, buf3;
    /* Values of 'first' and `last' during last call (-1 = not called yet) */
    static int old_first = -1, old_last = -1;
    /* Time of first call for this range */
    static double start_time = 0;

    if (!tc_progress_meter
     || !counter_active
     || !print_counter_interval
     || frame % print_counter_interval != 0
    ) {
        return;
    }
    if (frame < 0 || first < 0) {
        static int warned = 0;
        if (!warned) {
            tc_log_warn(__FILE__, "invalid arguments to counter_print"
                        " (%d,%d,%d,%d)", encoding, frame, first, last);
            warned = 1;
        }
        return;
    }

    if (gettimeofday(&tv, &dummy_tz) != 0) {
        static int warned = 0;
        if (!warned) {
            tc_log_warn(__FILE__, "gettimeofday() failed!");
            warned = 1;
        }
        return;
    }
    now = tv.tv_sec + (double)tv.tv_usec/1000000.0;

    if (old_first != first || old_last != last) {
        start_time = now;
        if (print_counter_cr && old_first != -1)
            printf("\n");
        old_first = first;
        old_last = last;
    }

    if (now <= start_time)  // true on first call per range
        return;
    /* Note that we don't add 1 to the numerator here, since start_time is
     * the time we were called for the first frame, so frame first+1 is one
     * frame later than start_time, not two. */
    fps = (frame - first) / (now - start_time);
    if (fps <= 0 || fps >= 10000)
        return;

    pthread_mutex_lock(&vbuffer_im_fill_lock);
    buf1 = vbuffer_im_fill_ctr;
    pthread_mutex_unlock(&vbuffer_im_fill_lock);

    pthread_mutex_lock(&vbuffer_xx_fill_lock);
    buf2 = vbuffer_xx_fill_ctr;
    pthread_mutex_unlock(&vbuffer_xx_fill_lock);

    pthread_mutex_lock(&vbuffer_ex_fill_lock);
    buf3 = vbuffer_ex_fill_ctr;
    pthread_mutex_unlock(&vbuffer_ex_fill_lock);

    if (last != -1) {
        double done = (double)(frame - first + 1) / (double)(last+1 - first);
        int secleft = (last+1 - frame) / fps;
        printf("%s frame [%d/%d], %6.2f fps, %5.1f%%, ETA: %d:%02d:%02d,"
               " (%2d|%2d|%2d)%s",
               encoding ? "encoding" : "skipping",
               frame, last+1,
               fps,
               100*done,
               secleft/3600, (secleft/60) % 60, secleft % 60,
               buf1, buf2, buf3,
               print_counter_cr ? " \r" : "\n"
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
               print_counter_cr ? " \r" : "\n"
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
