/*
 * vid_aux.c - auxiliary video processing routines
 * Written by Andrew Church <achurch@achurch.org>
 */

#include "vid_aux.h"

static int         va_width   = 0;
static int         va_height  = 0;
static uint8_t    *va_buffer  = NULL;

/*************************************************************************/

/* Set image width and height */
int tcv_convert_init(int width, int height)
{
    if (width <= 0 || height <= 0)
	return 0;
    va_width   = width;
    va_height  = height;
    free(va_buffer);
    va_buffer = malloc(width*height*4);  /* max 4 bytes per pixel */
    return 1;
}

/*************************************************************************/

/* Convert image in place (does nothing if formats are the same) */
int tcv_convert(uint8_t *image, ImageFormat srcfmt, ImageFormat destfmt)
{
    uint8_t *srcplanes[3], *destplanes[3];
    int size;

    if (srcfmt == destfmt && srcfmt != IMG_NONE)
	return 1;  /* formats are the same */

    switch (destfmt) {
	case IMG_YUV420P:
	case IMG_YV12   : size = va_width*va_height
			       + 2*(va_width/2)*(va_height/2); break;
	case IMG_YUV411P: size = va_width*va_height
			       + 2*(va_width/4)*va_height; break;
	case IMG_YUV422P: size = va_width*va_height
			       + 2*(va_width/2)*va_height; break;
	case IMG_YUV444P: size = va_width*va_height*3; break;
	case IMG_YUY2   :
	case IMG_UYVY   :
	case IMG_YVYU   : size = va_width*va_height*2; break;
	case IMG_Y8     :
	case IMG_GRAY8  : size = va_width*va_height; break;
	case IMG_RGB24  :
	case IMG_BGR24  : size = va_width*va_height*3; break;
	case IMG_RGBA32 :
	case IMG_ABGR32 :
	case IMG_ARGB32 :
	case IMG_BGRA32 : size = va_width*va_height*4; break;
	default         : return 0;
    }

    YUV_INIT_PLANES(srcplanes, image, srcfmt, va_width, va_height);
    YUV_INIT_PLANES(destplanes, va_buffer, destfmt, va_width, va_height);
    if (!ac_imgconvert(srcplanes, srcfmt, destplanes, destfmt,
		       va_width, va_height))
	return 0;

    ac_memcpy(image, va_buffer, size);
    return 1;
}

/*************************************************************************/
