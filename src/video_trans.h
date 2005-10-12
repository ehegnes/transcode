/*
 * video_trans.h - header for video frame transformation routines
 * Written by Andrew Church <achurch@achurch.org>
 * Based on code written by Thomas Oestreich.
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#ifndef _VIDEO_TRANS_H
#define _VIDEO_TRANS_H

#include "transcode.h"
#include <math.h>

/*************************************************************************/

/* Data for fast resizing operations */

#ifndef PI
# define PI 3.14159265358979323846264338327950
#endif

/* Threshold for antialiasing resized pixels/lines; we apply antialiasing
 * only when the (normalized) weights are strictly between these values */
#define RESIZE_AATHRESH_U 0.75
#define RESIZE_AATHRESH_L 0.25

typedef struct _resize_table_t {
    int source;
    uint32_t weight1, weight2;
    int antialias;  /* flag: antialias or no? */
} resize_table_t;

/* Antialiasing threshold for determining whether two pixels are the same
 * color. */
#define AA_DIFFERENT	25

/*************************************************************************/

/* Video frame processing functions. */

int process_vid_frame(vob_t *vob, vframe_list_t *ptr);
int preprocess_vid_frame(vob_t *vob, vframe_list_t *ptr);
int postprocess_vid_frame(vob_t *vob, vframe_list_t *ptr);

/*************************************************************************/

#endif  /* _VIDEO_TRANS_H */
