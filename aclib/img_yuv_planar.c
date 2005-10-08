/*
 * img_yuv_planar.c - YUV planar image format conversion routines
 * Written by Andrew Church <achurch@achurch.org>
 */

#include "ac.h"
#include "imgconvert.h"
#include "img_internal.h"

/*************************************************************************/
/*************************************************************************/

/* Standard C implementations */

/*************************************************************************/

/* Identity transformations */

static int yuv420p_copy(uint8_t **src, uint8_t **dest, int width, int height)
{
    ac_memcpy(dest[0], src[0], width*height);
    ac_memcpy(dest[1], src[1], width*height/4);
    ac_memcpy(dest[2], src[2], width*height/4);
    return 1;
}

static int yuv411p_copy(uint8_t **src, uint8_t **dest, int width, int height)
{
    ac_memcpy(dest[0], src[0], width*height);
    ac_memcpy(dest[1], src[1], width/4*height);
    ac_memcpy(dest[2], src[2], width/4*height);
    return 1;
}

static int yuv422p_copy(uint8_t **src, uint8_t **dest, int width, int height)
{
    ac_memcpy(dest[0], src[0], width*height);
    ac_memcpy(dest[1], src[1], width*height/2);
    ac_memcpy(dest[2], src[2], width*height/2);
    return 1;
}

static int yuv444p_copy(uint8_t **src, uint8_t **dest, int width, int height)
{
    ac_memcpy(dest[0], src[0], width*height);
    ac_memcpy(dest[1], src[1], width*height);
    ac_memcpy(dest[2], src[2], width*height);
    return 1;
}

/*************************************************************************/

/* Used for YUV420P->YV12 and YV12->YUV420P */

static int yuv420p_swap(uint8_t **src, uint8_t **dest, int width, int height)
{
    ac_memcpy(dest[0], src[0], width*height);
    ac_memcpy(dest[2], src[1], width*height/4);
    ac_memcpy(dest[1], src[2], width*height/4);
    return 1;
}

/*************************************************************************/

static int yuv420p_yuv411p(uint8_t **src, uint8_t **dest, int width, int height)
{
    int x, y;
    ac_memcpy(dest[0], src[0], width*height);
    for (y = 0; y < height; y += 2) {
	for (x = 0; x < width/2; x += 2) {
	    dest[1][y*(width/4)+x/2] =
		(src[1][(y/2)*(width/2)+x] + src[1][(y/2)*(width/2)+x+1]) / 2;
	    dest[2][y*(width/4)+x/2] =
		(src[2][(y/2)*(width/2)+x] + src[2][(y/2)*(width/2)+x+1]) / 2;
	}
	ac_memcpy(dest[1]+(y+1)*(width/4), dest[1]+y*(width/4), width/4);
	ac_memcpy(dest[2]+(y+1)*(width/4), dest[2]+y*(width/4), width/4);
    }
    return 1;
}

static int yuv420p_yuv422p(uint8_t **src, uint8_t **dest, int width, int height)
{
    int y;
    ac_memcpy(dest[0], src[0], width*height);
    for (y = 0; y < height; y += 2) {
	ac_memcpy(dest[1]+(y  )*(width/2), src[1]+(y/2)*(width/2), width/2);
	ac_memcpy(dest[1]+(y+1)*(width/2), src[1]+(y/2)*(width/2), width/2);
	ac_memcpy(dest[2]+(y  )*(width/2), src[2]+(y/2)*(width/2), width/2);
	ac_memcpy(dest[2]+(y+1)*(width/2), src[2]+(y/2)*(width/2), width/2);
    }
    return 1;
}

static int yuv420p_yuv444p(uint8_t **src, uint8_t **dest, int width, int height)
{
    int x, y;
    ac_memcpy(dest[0], src[0], width*height);
    for (y = 0; y < height; y += 2) {
	for (x = 0; x < width; x += 2) {
	    dest[1][y*width+x  ] = src[1][(y/2)*(width/2)+x];
	    dest[1][y*width+x+1] = src[1][(y/2)*(width/2)+x];
	    dest[2][y*width+x  ] = src[2][(y/2)*(width/2)+x];
	    dest[2][y*width+x+1] = src[2][(y/2)*(width/2)+x];
	}
	ac_memcpy(dest[1]+(y+1)*width, dest[1]+y*width, width);
	ac_memcpy(dest[2]+(y+1)*width, dest[2]+y*width, width);
    }
    return 1;
}

/*************************************************************************/

static int yuv411p_yuv420p(uint8_t **src, uint8_t **dest, int width, int height)
{
    int x, y;
    ac_memcpy(dest[0], src[0], width*height);
    for (y = 0; y < height; y += 2) {
	for (x = 0; x < width/2; x += 2) {
	    dest[1][(y/2)*(width/2)+x] = (src[1][y*(width/4)+x/2]
				        + src[1][(y+1)*(width/4)+x/2]) / 2;
	    dest[2][(y/2)*(width/2)+x] = (src[2][y*(width/4)+x/2]
				        + src[2][(y+1)*(width/4)+x/2]) / 2;
	    dest[1][(y/2)*(width/2)+x+1] = dest[1][(y/2)*(width/2)+x];
	    dest[2][(y/2)*(width/2)+x+1] = dest[2][(y/2)*(width/2)+x];
	}
    }
    return 1;
}

static int yuv411p_yuv422p(uint8_t **src, uint8_t **dest, int width, int height)
{
    int x, y;
    ac_memcpy(dest[0], src[0], width*height);
    for (y = 0; y < height; y++) {
	for (x = 0; x < width/2; x += 2) {
	    dest[1][y*(width/2)+x  ] = src[1][y*(width/4)+x/2];
	    dest[1][y*(width/2)+x+1] = src[1][y*(width/4)+x/2];
	    dest[2][y*(width/2)+x  ] = src[2][y*(width/4)+x/2];
	    dest[2][y*(width/2)+x+1] = src[2][y*(width/4)+x/2];
	}
    }
    return 1;
}

static int yuv411p_yuv444p(uint8_t **src, uint8_t **dest, int width, int height)
{
    int x, y;
    ac_memcpy(dest[0], src[0], width*height);
    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x += 4) {
	    dest[1][y*width+x  ] = src[1][y*(width/4)+x/4];
	    dest[1][y*width+x+1] = src[1][y*(width/4)+x/4];
	    dest[1][y*width+x+2] = src[1][y*(width/4)+x/4];
	    dest[1][y*width+x+3] = src[1][y*(width/4)+x/4];
	    dest[2][y*width+x  ] = src[2][y*(width/4)+x/4];
	    dest[2][y*width+x+1] = src[2][y*(width/4)+x/4];
	    dest[2][y*width+x+2] = src[2][y*(width/4)+x/4];
	    dest[2][y*width+x+3] = src[2][y*(width/4)+x/4];
	}
    }
    return 1;
}

/*************************************************************************/

static int yuv422p_yuv420p(uint8_t **src, uint8_t **dest, int width, int height)
{
    int x, y;
    ac_memcpy(dest[0], src[0], width*height);
    for (y = 0; y < height; y += 2) {
	for (x = 0; x < width/2; x++) {
	    dest[1][(y/2)*(width/2)+x] = (src[1][y*(width/2)+x]
				        + src[1][(y+1)*(width/2)+x]) / 2;
	    dest[2][(y/2)*(width/2)+x] = (src[2][y*(width/2)+x]
				        + src[2][(y+1)*(width/2)+x]) / 2;
	}
    }
    return 1;
}

static int yuv422p_yuv411p(uint8_t **src, uint8_t **dest, int width, int height)
{
    int x, y;
    ac_memcpy(dest[0], src[0], width*height);
    for (y = 0; y < height; y++) {
	for (x = 0; x < width/2; x++) {
	    dest[1][y*(width/4)+x/2] = (src[1][y*(width/2)+x]
				      + src[1][y*(width/2)+x+1]) / 2;
	    dest[2][y*(width/4)+x/2] = (src[2][y*(width/2)+x]
				      + src[2][y*(width/2)+x+1]) / 2;
	}
    }
    return 1;
}

static int yuv422p_yuv444p(uint8_t **src, uint8_t **dest, int width, int height)
{
    int x, y;
    ac_memcpy(dest[0], src[0], width*height);
    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x += 2) {
	    dest[1][y*width+x  ] = src[1][y*(width/2)+x/2];
	    dest[1][y*width+x+1] = src[1][y*(width/2)+x/2];
	    dest[2][y*width+x  ] = src[2][y*(width/2)+x/2];
	    dest[2][y*width+x+1] = src[2][y*(width/2)+x/2];
	}
    }
    return 1;
}

/*************************************************************************/

static int yuv444p_yuv420p(uint8_t **src, uint8_t **dest, int width, int height)
{
    int x, y;
    ac_memcpy(dest[0], src[0], width*height);
    for (y = 0; y < height; y += 2) {
	for (x = 0; x < width; x += 2) {
	    dest[1][(y/2)*(width/2)+x/2] = (src[1][y*width+x]
				          + src[1][y*width+x+1]
				          + src[1][(y+1)*width+x]
				          + src[1][(y+1)*width+x+1]) / 4;
	    dest[2][(y/2)*(width/2)+x/2] = (src[2][y*width+x]
				          + src[2][y*width+x+1]
				          + src[2][(y+1)*width+x]
				          + src[2][(y+1)*width+x+1]) / 4;
	}
    }
    return 1;
}

static int yuv444p_yuv411p(uint8_t **src, uint8_t **dest, int width, int height)
{
    int x, y;
    ac_memcpy(dest[0], src[0], width*height);
    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x += 4) {
	    dest[1][y*(width/4)+x/4] = (src[1][y*width+x]
				      + src[1][y*width+x+1]
				      + src[1][y*width+x+2]
				      + src[1][y*width+x+3]) / 4;
	    dest[2][y*(width/4)+x/4] = (src[2][y*width+x]
				      + src[2][y*width+x+1]
				      + src[2][y*width+x+2]
				      + src[2][y*width+x+3]) / 4;
	}
    }
    return 1;
}

static int yuv444p_yuv422p(uint8_t **src, uint8_t **dest, int width, int height)
{
    int x, y;
    ac_memcpy(dest[0], src[0], width*height);
    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x += 2) {
	    dest[1][y*(width/2)+x/2] = (src[1][y*width+x]
				      + src[1][y*width+x+1]) / 2;
	    dest[2][y*(width/2)+x/2] = (src[2][y*width+x]
				      + src[2][y*width+x+1]) / 2;
	}
    }
    return 1;
}

/*************************************************************************/
/*************************************************************************/

/* Initialization */

int ac_imgconvert_init_yuv_planar(int accel)
{
    if (!register_conversion(IMG_YUV420P, IMG_YUV420P, yuv420p_copy)
     || !register_conversion(IMG_YUV420P, IMG_YV12,    yuv420p_swap)
     || !register_conversion(IMG_YUV420P, IMG_YUV411P, yuv420p_yuv411p)
     || !register_conversion(IMG_YUV420P, IMG_YUV422P, yuv420p_yuv422p)
     || !register_conversion(IMG_YUV420P, IMG_YUV444P, yuv420p_yuv444p)

     || !register_conversion(IMG_YV12,    IMG_YUV420P, yuv420p_swap)
     || !register_conversion(IMG_YV12,    IMG_YV12,    yuv420p_copy)

     || !register_conversion(IMG_YUV411P, IMG_YUV420P, yuv411p_yuv420p)
     || !register_conversion(IMG_YUV411P, IMG_YUV411P, yuv411p_copy)
     || !register_conversion(IMG_YUV411P, IMG_YUV422P, yuv411p_yuv422p)
     || !register_conversion(IMG_YUV411P, IMG_YUV444P, yuv411p_yuv444p)

     || !register_conversion(IMG_YUV422P, IMG_YUV420P, yuv422p_yuv420p)
     || !register_conversion(IMG_YUV422P, IMG_YUV411P, yuv422p_yuv411p)
     || !register_conversion(IMG_YUV422P, IMG_YUV422P, yuv422p_copy)
     || !register_conversion(IMG_YUV422P, IMG_YUV444P, yuv422p_yuv444p)

     || !register_conversion(IMG_YUV444P, IMG_YUV420P, yuv444p_yuv420p)
     || !register_conversion(IMG_YUV444P, IMG_YUV411P, yuv444p_yuv411p)
     || !register_conversion(IMG_YUV444P, IMG_YUV422P, yuv444p_yuv422p)
     || !register_conversion(IMG_YUV444P, IMG_YUV444P, yuv444p_copy)
    ) {
	return 0;
    }
    return 1;
}

/*************************************************************************/
