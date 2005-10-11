/*
 * vid_aux.h - auxiliary video processing routines
 * Written by Andrew Church <achurch@achurch.org>
 */

#ifndef EXPORT_VID_AUX_H
#define EXPORT_VID_AUX_H

#include "transcode.h"
#include "aclib/imgconvert.h"

/* Set image width and height.  Returns 1 on success, 0 on failure. */
int tcv_convert_init(int width, int height);

/* Convert image in place (does nothing if formats are the same).
 * Returns 1 on success, 0 on failure. */
int tcv_convert(uint8_t *image, ImageFormat srcfmt, ImageFormat destfmt);

#endif
