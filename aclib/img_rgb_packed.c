/*
 * img_rgb_packed.c - RGB packed image format conversion routines
 * Written by Andrew Church <achurch@achurch.org>
 */

#include "ac.h"
#include "imgconvert.h"
#include "img_internal.h"

/*************************************************************************/
/*************************************************************************/

/* Standard C implementations */

/*************************************************************************/

/* Identity transformations, all work when src==dest */

static int rgb24_copy(u_int8_t **src, u_int8_t **dest, int width, int height)
{
    tc_memcpy(dest[0], src[0], width*height*3);
    return 1;
}

static int argb32_copy(u_int8_t **src, u_int8_t **dest, int width, int height)
{
    tc_memcpy(dest[0], src[0], width*height*4);
    return 1;
}

static int gray8_copy(u_int8_t **src, u_int8_t **dest, int width, int height)
{
    tc_memcpy(dest[0], src[0], width*height);
    return 1;
}

/*************************************************************************/

static int rgb24_argb32(u_int8_t **src, u_int8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	dest[0][i*4  ] = 0;
	dest[0][i*4+1] = src[0][i*3  ];
	dest[0][i*4+2] = src[0][i*3+1];
	dest[0][i*4+3] = src[0][i*3+2];
    }
    return 1;
}

static int rgb24_gray8(u_int8_t **src, u_int8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	/* Use the Y part of a YUV transformation, scaled to 0..255 */
	int r = src[0][i*3  ];
	int g = src[0][i*3+1];
	int b = src[0][i*3+2];
	dest[0][i] = (19595*r + 38470*g + 7471*b + 32768) >> 16;
    }
    return 1;
}

/*************************************************************************/

static int argb32_rgb24(u_int8_t **src, u_int8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	dest[0][i*3  ] = src[0][i*4+1];
	dest[0][i*3+1] = src[0][i*4+2];
	dest[0][i*3+2] = src[0][i*4+3];
    }
    return 1;
}

static int argb32_gray8(u_int8_t **src, u_int8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	/* Use the Y part of a YUV transformation, scaled to 0..255 */
	int r = src[0][i*4+1];
	int g = src[0][i*4+2];
	int b = src[0][i*4+3];
	dest[0][i] = (19595*r + 38470*g + 7471*b + 32768) >> 16;
    }
    return 1;
}

/*************************************************************************/

static int gray8_rgb24(u_int8_t **src, u_int8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	dest[0][i*3  ] = src[0][i];
	dest[0][i*3+1] = src[0][i];
	dest[0][i*3+2] = src[0][i];
    }
    return 1;
}

static int gray8_argb32(u_int8_t **src, u_int8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	dest[0][i*4  ] = 0;
	dest[0][i*4+1] = src[0][i];
	dest[0][i*4+2] = src[0][i];
	dest[0][i*4+3] = src[0][i];
    }
    return 1;
}

/*************************************************************************/
/*************************************************************************/

/* Initialization */

int ac_imgconvert_init_rgb_packed(int accel)
{
    if (!register_conversion(IMG_RGB24,   IMG_RGB24,   rgb24_copy)
     || !register_conversion(IMG_RGB24,   IMG_ARGB32,  rgb24_argb32)
     || !register_conversion(IMG_RGB24,   IMG_GRAY8,   rgb24_gray8)

     || !register_conversion(IMG_ARGB32,  IMG_RGB24,   argb32_rgb24)
     || !register_conversion(IMG_ARGB32,  IMG_ARGB32,  argb32_copy)
     || !register_conversion(IMG_ARGB32,  IMG_GRAY8,   argb32_gray8)

     || !register_conversion(IMG_GRAY8,   IMG_RGB24,   gray8_rgb24)
     || !register_conversion(IMG_GRAY8,   IMG_ARGB32,  gray8_argb32)
     || !register_conversion(IMG_GRAY8,   IMG_GRAY8,   gray8_copy)
    ) {
	return 0;
    }
    return 1;
}

/*************************************************************************/
