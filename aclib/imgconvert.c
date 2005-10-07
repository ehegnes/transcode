/*
 * imgconvert.c - image format conversion routines
 * Written by Andrew Church <achurch@achurch.org>
 */

#include <stdlib.h>
#include "ac.h"
#include "imgconvert.h"
#include "img_internal.h"

/*************************************************************************/

static struct {
    ImageFormat srcfmt, destfmt;
    ConversionFunc func;
} *conversions;
static int n_conversions = 0;

/*************************************************************************/
/*************************************************************************/

/* Initialization routine.  Call with the desired acceleration flags (from
 * ac.h) before calling ac_imgconvert(). */

int ac_imgconvert_init(int accel)
{
    if (!ac_imgconvert_init_yuv_planar(accel)
     || !ac_imgconvert_init_yuv_packed(accel)
     || !ac_imgconvert_init_yuv_mixed(accel)
     || !ac_imgconvert_init_yuv_rgb(accel)
     || !ac_imgconvert_init_rgb_packed(accel)
    ) {
	tc_error("ac_imgconvert_init() failed");
	return 0;
    }
    return 1;
}

/*************************************************************************/

/* Image conversion routine.  src and dest are arrays of pointers to planes
 * (for packed formats with only one plane, just use &data); srcfmt and
 * destfmt specify the source and destination image formats (IMG_*).
 * width and height are in pixels.  Returns 1 on success, 0 on failure. */

int ac_imgconvert(u_int8_t **src, ImageFormat srcfmt,
		  u_int8_t **dest, ImageFormat destfmt,
		  int width, int height)
{
    int i;

    for (i = 0; i < n_conversions; i++) {
	if (conversions[i].srcfmt==srcfmt && conversions[i].destfmt==destfmt)
	    return (*conversions[i].func)(src, dest, width, height);
    }

/* FIXME: the below logic should probably be in a separate function that
 *        has an init that allocates a temp buffer as needed */
#if 0  /* FIXME: need temp buffer */
    if (IS_YUV_FORMAT(srcfmt) && IS_YUV_FORMAT(destfmt)) {
	if (srcfmt == IMG_YUV_DEFAULT || destfmt == IMG_YUV_DEFAULT)
	    return 0;  /* can't convert */
	return ac_imgconvert(src, srcfmt, tmp, IMG_YUV_DEFAULT, width, height)
	    && ac_imgconvert(tmp, IMG_YUV_DEFAULT, dest, destfmt, width, height);
    }

    if (IS_RGB_FORMAT(srcfmt) && IS_RGB_FORMAT(destfmt)) {
	if (srcfmt == IMG_RGB_DEFAULT || destfmt == IMG_RGB_DEFAULT)
	    return 0;  /* can't convert */
	return ac_imgconvert(src, srcfmt, tmp, IMG_RGB_DEFAULT, width, height)
	    && ac_imgconvert(tmp, IMG_RGB_DEFAULT, dest, destfmt, width, height);
    }
#endif

    return 0;
}

/*************************************************************************/
/*************************************************************************/

/* Internal use only! */

int register_conversion(ImageFormat srcfmt, ImageFormat destfmt,
			ConversionFunc function)
{
    int i;

    for (i = 0; i < n_conversions; i++) {
	if (conversions[i].srcfmt==srcfmt && conversions[i].destfmt==destfmt) {
	    conversions[i].func = function;
	    return 1;
	}
    }

    if (!(conversions = realloc(conversions,
				(n_conversions+1) * sizeof(*conversions)))) {
	tc_error("register_conversion(): out of memory");
	return 0;
    }
    conversions[n_conversions].srcfmt  = srcfmt;
    conversions[n_conversions].destfmt = destfmt;
    conversions[n_conversions].func    = function;
    n_conversions++;
    return 1;
}

/*************************************************************************/
