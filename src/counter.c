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
#include <math.h>

/*************************************************************************/

static int counter_active = 0;    /* Is the counter active? */
static int frames_to_encode = 0;  /* Total number of frames to encode */
static int encoded_frames = 0;    /* Number of frames encoded so far */
static double encoded_time = 0;   /* Time spent encoding so far */
static int frames_to_skip = 0;    /* Total number of frames to skip */
static int skipped_frames = 0;    /* Number of frames skipped so far */
static double skipped_time = 0;   /* Time spent skipping so far */
static int highest_frame = 0;     /* Highest frame number to be seen */

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
 * counter_add_range:  Add the given range of frames to the total number of
 * frames to be encoded or skipped.
 *
 * Parameters:  first: First frame of range.
 *               last: Last frame of range.
 *             encode: True (nonzero) if frames are to be encoded,
 *                     false (zero) if frames are being skipped.
 * Return value: None.
 * Preconditions: None.
 * Postconditions: None.
 */

void counter_add_range(int first, int last, int encode)
{
    if (encode) {
        frames_to_encode += last+1 - first;
    } else {
        frames_to_skip += last+1 - first;
    }
    if (last > highest_frame)
        highest_frame = last;
}

/*************************************************************************/

/**
 * counter_reset_ranges:  Reset the counter's stored range data.
 *
 * Parameters: None.
 * Return value: None.
 * Preconditions: None.
 * Postconditions: None.
 */

void counter_reset_ranges(void)
{
    frames_to_encode = 0;
    encoded_frames = 0;
    encoded_time = 0;
    frames_to_skip = 0;
    skipped_frames = 0;
    skipped_time = 0;
    highest_frame = 0;
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
    double now, timediff, fps;
    uint32_t buf1, buf2, buf3;
    /* Values of 'first' and `last' during last call (-1 = not called yet) */
    static int old_first = -1, old_last = -1;
    /* Time of first call for this range */
    static double start_time = 0;
    /* Time of last call */
    static double old_time = 0;

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

    timediff = now - old_time;
    old_time = now;
    if (old_first != first || old_last != last) {
        /* In CR mode, start a new counter line for each range if we don't
         * know the total number of frames to be encoded. */
        if (print_counter_cr && old_first != -1 && frames_to_encode == 0)
            printf("\n");
        start_time = now;
        old_first = first;
        old_last = last;
        /* We decrement the frame counts here to compensate for this frame
         * which took an unknown amount of time to complete. */
        if (encoding && frames_to_encode > 0)
            frames_to_encode--;
        else if (!encoding && frames_to_skip > 0)
            frames_to_skip--;
        return;
    }

    /* Note that we don't add 1 to the numerator here, since start_time is
     * the time we were called for the first frame, so frame first+1 is one
     * one frame later than start_time, not two. */
    fps = (frame - first) / (now - start_time);

    pthread_mutex_lock(&vbuffer_im_fill_lock);
    buf1 = vbuffer_im_fill_ctr;
    pthread_mutex_unlock(&vbuffer_im_fill_lock);

    pthread_mutex_lock(&vbuffer_xx_fill_lock);
    buf2 = vbuffer_xx_fill_ctr;
    pthread_mutex_unlock(&vbuffer_xx_fill_lock);

    pthread_mutex_lock(&vbuffer_ex_fill_lock);
    buf3 = vbuffer_ex_fill_ctr;
    pthread_mutex_unlock(&vbuffer_ex_fill_lock);

    if (last == -1) {
        /* Can't calculate ETA, just display current timestamp */
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

    } else if (frames_to_encode == 0) {
        /* Total number of frames unknown, just display for current range */
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
        /* Estimate time remaining for entire run */
        char eta_buf[100];
        double done;
        if (encoding) {
            encoded_frames++;
            encoded_time += timediff;
        } else {
            skipped_frames++;
            skipped_time += timediff;
        }
        if (encoded_frames > frames_to_encode)
            frames_to_encode = encoded_frames;
        if (skipped_frames > frames_to_skip)
            frames_to_skip = skipped_frames;
        if (encoded_frames == 0) {
            /* We don't know how long it will take to encode frames; avoid
             * understating the ETA, and just say we don't know */
            snprintf(eta_buf, sizeof(eta_buf), "--:--:--");
        } else {
            double encode_fps, skip_fps, total_time;
            int secleft;
            /* Find the processing speed for encoding and skipping */
            encode_fps = encoded_frames / encoded_time;
            if (skipped_frames > 0) {
                skip_fps = skipped_frames / skipped_time;
            } else {
                /* Just assume the same FPS for skipping as for encoding.
                 * Overstating the ETA isn't as bad as understating it, and
                 * certainly better than giving the user "unknown" for an
                 * entire encoding range. */
                skip_fps = encode_fps;
            }
            /* Estimate the total processing time required */
            total_time = (frames_to_encode / encode_fps)
                       + (frames_to_skip / skip_fps);
            /* Determine time left (round up) and convert to string */
            secleft = ceil(total_time - (encoded_time + skipped_time));
            snprintf(eta_buf, sizeof(eta_buf), "%d:%02d:%02d",
                     secleft/3600, (secleft/60) % 60, secleft % 60);
            /* Use the proper overall FPS in the status line */
            fps = encoding ? encode_fps : skip_fps;
        }
        /* Just use the frame ratio for completion percentage */
        done = (double)(encoded_frames + skipped_frames)
             / (double)(frames_to_encode + frames_to_skip);
        printf("%s frame [%d/%d], %6.2f fps, %5.1f%%, ETA: %s,"
               " (%2d|%2d|%2d)%s",
               encoding ? "encoding" : "skipping",
               frame, highest_frame+1,
               fps,
               100*done,
               eta_buf,
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
