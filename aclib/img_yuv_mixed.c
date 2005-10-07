/*
 * img_yuv_packed.c - YUV planar<->packed image format conversion routines
 * Written by Andrew Church <achurch@achurch.org>
 */

#include "ac.h"
#include "imgconvert.h"
#include "img_internal.h"

/*************************************************************************/
/*************************************************************************/

/* Standard C implementations */

/*************************************************************************/

/* Wrappers for UYVY and YVYU */
/* Note: we rely on YUY2<->{UYVY,YVYU} working for src==dest */
/* FIXME: when converting from UYVY/YVYU, src is destroyed! */

static int uyvy_yvyu_wrapper(u_int8_t **src, ImageFormat srcfmt,
			     u_int8_t **dest, ImageFormat destfmt,
			     int width, int height)
{
    if (srcfmt == IMG_UYVY || srcfmt == IMG_YVYU)
	return ac_imgconvert(src, srcfmt, src, IMG_YUY2, width, height)
	    && ac_imgconvert(src, IMG_YUY2, dest, destfmt, width, height);
    else
	return ac_imgconvert(src, srcfmt, dest, IMG_YUY2, width, height)
	    && ac_imgconvert(dest, IMG_YUY2, dest, destfmt, width, height);
}

static int yuv420p_uyvy(u_int8_t **src, u_int8_t **dest, int width, int height)
{ return uyvy_yvyu_wrapper(src, IMG_YUV420P, dest, IMG_UYVY, width, height); }

static int yuv420p_yvyu(u_int8_t **src, u_int8_t **dest, int width, int height)
{ return uyvy_yvyu_wrapper(src, IMG_YUV420P, dest, IMG_YVYU, width, height); }

static int yuv411p_uyvy(u_int8_t **src, u_int8_t **dest, int width, int height)
{ return uyvy_yvyu_wrapper(src, IMG_YUV411P, dest, IMG_UYVY, width, height); }

static int yuv411p_yvyu(u_int8_t **src, u_int8_t **dest, int width, int height)
{ return uyvy_yvyu_wrapper(src, IMG_YUV411P, dest, IMG_YVYU, width, height); }

static int yuv422p_uyvy(u_int8_t **src, u_int8_t **dest, int width, int height)
{ return uyvy_yvyu_wrapper(src, IMG_YUV422P, dest, IMG_UYVY, width, height); }

static int yuv422p_yvyu(u_int8_t **src, u_int8_t **dest, int width, int height)
{ return uyvy_yvyu_wrapper(src, IMG_YUV422P, dest, IMG_YVYU, width, height); }

static int yuv444p_uyvy(u_int8_t **src, u_int8_t **dest, int width, int height)
{ return uyvy_yvyu_wrapper(src, IMG_YUV444P, dest, IMG_UYVY, width, height); }

static int yuv444p_yvyu(u_int8_t **src, u_int8_t **dest, int width, int height)
{ return uyvy_yvyu_wrapper(src, IMG_YUV444P, dest, IMG_YVYU, width, height); }

static int uyvy_yuv420p(u_int8_t **src, u_int8_t **dest, int width, int height)
{ return uyvy_yvyu_wrapper(src, IMG_UYVY, dest, IMG_YUV420P, width, height); }

static int yvyu_yuv420p(u_int8_t **src, u_int8_t **dest, int width, int height)
{ return uyvy_yvyu_wrapper(src, IMG_YVYU, dest, IMG_YUV420P, width, height); }

static int uyvy_yuv411p(u_int8_t **src, u_int8_t **dest, int width, int height)
{ return uyvy_yvyu_wrapper(src, IMG_UYVY, dest, IMG_YUV411P, width, height); }

static int yvyu_yuv411p(u_int8_t **src, u_int8_t **dest, int width, int height)
{ return uyvy_yvyu_wrapper(src, IMG_YVYU, dest, IMG_YUV411P, width, height); }

static int uyvy_yuv422p(u_int8_t **src, u_int8_t **dest, int width, int height)
{ return uyvy_yvyu_wrapper(src, IMG_UYVY, dest, IMG_YUV422P, width, height); }

static int yvyu_yuv422p(u_int8_t **src, u_int8_t **dest, int width, int height)
{ return uyvy_yvyu_wrapper(src, IMG_YVYU, dest, IMG_YUV422P, width, height); }

static int uyvy_yuv444p(u_int8_t **src, u_int8_t **dest, int width, int height)
{ return uyvy_yvyu_wrapper(src, IMG_UYVY, dest, IMG_YUV444P, width, height); }

static int yvyu_yuv444p(u_int8_t **src, u_int8_t **dest, int width, int height)
{ return uyvy_yvyu_wrapper(src, IMG_YVYU, dest, IMG_YUV444P, width, height); }

/*************************************************************************/

static int yuv420p_yuy2(u_int8_t **src, u_int8_t **dest, int width, int height)
{
    int x, y;

    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x += 2) {
	    dest[0][(y*width+x)*2  ] = src[0][y*width+x];
	    dest[0][(y*width+x)*2+1] = src[1][(y/2)*(width/2)+x/2];
	    dest[0][(y*width+x)*2+2] = src[0][y*width+x+1];
	    dest[0][(y*width+x)*2+3] = src[2][(y/2)*(width/2)+x/2];
	}
    }
    return 1;
}

static int yuv411p_yuy2(u_int8_t **src, u_int8_t **dest, int width, int height)
{
    int x, y;

    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x += 2) {
	    dest[0][(y*width+x)*2  ] = src[0][y*width+x];
	    dest[0][(y*width+x)*2+1] = src[1][y*(width/4)+x/4];
	    dest[0][(y*width+x)*2+2] = src[0][y*width+x+1];
	    dest[0][(y*width+x)*2+3] = src[2][y*(width/4)+x/4];
	}
    }
    return 1;
}

static int yuv422p_yuy2(u_int8_t **src, u_int8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height/2; i++) {
	dest[0][i*4  ] = src[0][i*2];
	dest[0][i*4+1] = src[1][i];
	dest[0][i*4+2] = src[0][i*2+1];
	dest[0][i*4+3] = src[2][i];
    }
    return 1;
}

static int yuv444p_yuy2(u_int8_t **src, u_int8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height/2; i++) {
	dest[0][i*4  ] = src[0][i*2];
	dest[0][i*4+1] = (src[1][i*2] + src[1][i*2+1]) / 2;
	dest[0][i*4+2] = src[0][i*2+1];
	dest[0][i*4+3] = (src[2][i*2] + src[2][i*2+1]) / 2;
    }
    return 1;
}

/*************************************************************************/

static int yuy2_yuv420p(u_int8_t **src, u_int8_t **dest, int width, int height)
{
    int x, y;

    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x += 2) {
	    dest[0][y*width+x  ] = src[0][(y*width+x)*2  ];
	    dest[0][y*width+x+1] = src[0][(y*width+x)*2+2];
	    if (y%2 == 0) {
		dest[1][(y/2)*(width/2)+x/2] = src[0][(y*width+x)*2+1];
		dest[2][(y/2)*(width/2)+x/2] = src[0][(y*width+x)*2+3];
	    } else {
		dest[1][(y/2)*(width/2)+x/2] =
		    (dest[1][(y/2)*(width/2)+x/2] + src[0][(y*width+x)*2+1]) / 2;
		dest[2][(y/2)*(width/2)+x/2] =
		    (dest[2][(y/2)*(width/2)+x/2] + src[0][(y*width+x)*2+3]) / 2;
	    }
	}
    }
    return 1;
}

static int yuy2_yuv411p(u_int8_t **src, u_int8_t **dest, int width, int height)
{
    int x, y;
    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x += 4) {
	    dest[0][y*width+x]       = src[0][(y*width+x)*2  ];
	    dest[0][y*width+x+1]     = src[0][(y*width+x)*2+2];
	    dest[0][y*width+x+2]     = src[0][(y*width+x)*2+4];
	    dest[0][y*width+x+3]     = src[0][(y*width+x)*2+6];
	    dest[1][y*(width/4)+x/4] = (src[0][(y*width+x)*2+1]
				      + src[0][(y*width+x)*2+5]) / 2;
	    dest[2][y*(width/4)+x/4] = (src[0][(y*width+x)*2+3]
				      + src[0][(y*width+x)*2+7]) / 2;
	}
    }
    return 1;
}

static int yuy2_yuv422p(u_int8_t **src, u_int8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height/2; i++) {
	dest[0][i*2]   = src[0][i*4  ];
	dest[1][i]     = src[0][i*4+1];
	dest[0][i*2+1] = src[0][i*4+2];
	dest[2][i]     = src[0][i*4+3];
    }
    return 1;
}

static int yuy2_yuv444p(u_int8_t **src, u_int8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i += 2) {
	dest[0][i]   = src[0][i*2  ];
	dest[1][i]   = src[0][i*2+1];
	dest[1][i+1] = src[0][i*2+1];
	dest[0][i+1] = src[0][i*2+2];
	dest[2][i]   = src[0][i*2+3];
	dest[2][i+1] = src[0][i*2+3];
    }
    return 1;
}

/*************************************************************************/
/*************************************************************************/

/* Initialization */

int ac_imgconvert_init_yuv_mixed(int accel)
{
    if (!register_conversion(IMG_YUV420P, IMG_YUY2,    yuv420p_yuy2)
     || !register_conversion(IMG_YUV411P, IMG_YUY2,    yuv411p_yuy2)
     || !register_conversion(IMG_YUV422P, IMG_YUY2,    yuv422p_yuy2)
     || !register_conversion(IMG_YUV444P, IMG_YUY2,    yuv444p_yuy2)
     || !register_conversion(IMG_YUV420P, IMG_UYVY,    yuv420p_uyvy)
     || !register_conversion(IMG_YUV411P, IMG_UYVY,    yuv411p_uyvy)
     || !register_conversion(IMG_YUV422P, IMG_UYVY,    yuv422p_uyvy)
     || !register_conversion(IMG_YUV444P, IMG_UYVY,    yuv444p_uyvy)
     || !register_conversion(IMG_YUV420P, IMG_YVYU,    yuv420p_yvyu)
     || !register_conversion(IMG_YUV411P, IMG_YVYU,    yuv411p_yvyu)
     || !register_conversion(IMG_YUV422P, IMG_YVYU,    yuv422p_yvyu)
     || !register_conversion(IMG_YUV444P, IMG_YVYU,    yuv444p_yvyu)

     || !register_conversion(IMG_YUY2,    IMG_YUV420P, yuy2_yuv420p)
     || !register_conversion(IMG_YUY2,    IMG_YUV411P, yuy2_yuv411p)
     || !register_conversion(IMG_YUY2,    IMG_YUV422P, yuy2_yuv422p)
     || !register_conversion(IMG_YUY2,    IMG_YUV444P, yuy2_yuv444p)
     || !register_conversion(IMG_UYVY,    IMG_YUV420P, uyvy_yuv420p)
     || !register_conversion(IMG_UYVY,    IMG_YUV411P, uyvy_yuv411p)
     || !register_conversion(IMG_UYVY,    IMG_YUV422P, uyvy_yuv422p)
     || !register_conversion(IMG_UYVY,    IMG_YUV444P, uyvy_yuv444p)
     || !register_conversion(IMG_YVYU,    IMG_YUV420P, yvyu_yuv420p)
     || !register_conversion(IMG_YVYU,    IMG_YUV411P, yvyu_yuv411p)
     || !register_conversion(IMG_YVYU,    IMG_YUV422P, yvyu_yuv422p)
     || !register_conversion(IMG_YVYU,    IMG_YUV444P, yvyu_yuv444p)
    ) {
	return 0;
    }
    return 1;
}

/*************************************************************************/
