/*
 * img_yuv_packed.c - YUV packed image format conversion routines
 * Written by Andrew Church <achurch@achurch.org>
 */

#include "ac.h"
#include "imgconvert.h"
#include "img_internal.h"

/*************************************************************************/
/*************************************************************************/

/* Standard C implementations */

/*************************************************************************/

/* Identity transformation, works when src==dest */
static int yuv16_copy(uint8_t **src, uint8_t **dest, int width, int height)
{
    tc_memcpy(dest[0], src[0], width*height*2);
    return 1;
}

/* Used for YUY2->UYVY and UYVY->YUY2, works when src==dest */
static int yuv16_swap16(uint8_t **src, uint8_t **dest, int width, int height)
{
    u_int16_t *srcp  = (u_int16_t *)src[0];
    u_int16_t *destp = (u_int16_t *)dest[0];
    int i;
    for (i = 0; i < width*height; i++)
	destp[i] = srcp[i]>>8 | srcp[i]<<8;
    return 1;
}

/* Used for YUY2->YVYU and YVYU->YUY2, works when src==dest */
static int yuv16_swapuv(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height/2; i++) {
	uint8_t tmp   = src[0][i*4+1];
	dest[0][i*4  ] = src[0][i*4  ];
	dest[0][i*4+1] = src[0][i*4+3];
	dest[0][i*4+2] = src[0][i*4+2];
	dest[0][i*4+3] = tmp;
    }
    return 1;
}

/*************************************************************************/

static int uyvy_yvyu(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height/2; i++) {
	dest[0][i*4  ] = src[0][i*4+1];
	dest[0][i*4+1] = src[0][i*4+2];
	dest[0][i*4+2] = src[0][i*4+3];
	dest[0][i*4+3] = src[0][i*4  ];
    }
    return 1;
}

static int yvyu_uyvy(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height/2; i++) {
	dest[0][i*4  ] = src[0][i*4+3];
	dest[0][i*4+1] = src[0][i*4  ];
	dest[0][i*4+2] = src[0][i*4+1];
	dest[0][i*4+3] = src[0][i*4+2];
    }
    return 1;
}

/*************************************************************************/
/*************************************************************************/

/* Initialization */

int ac_imgconvert_init_yuv_packed(int accel)
{
    if (!register_conversion(IMG_YUY2,    IMG_YUY2,    yuv16_copy)
     || !register_conversion(IMG_YUY2,    IMG_UYVY,    yuv16_swap16)
     || !register_conversion(IMG_YUY2,    IMG_YVYU,    yuv16_swapuv)

     || !register_conversion(IMG_UYVY,    IMG_YUY2,    yuv16_swap16)
     || !register_conversion(IMG_UYVY,    IMG_UYVY,    yuv16_copy)
     || !register_conversion(IMG_UYVY,    IMG_YVYU,    uyvy_yvyu)

     || !register_conversion(IMG_YVYU,    IMG_YUY2,    yuv16_swapuv)
     || !register_conversion(IMG_YVYU,    IMG_UYVY,    yvyu_uyvy)
     || !register_conversion(IMG_YVYU,    IMG_YVYU,    yuv16_copy)
    ) {
	return 0;
    }
    return 1;
}

/*************************************************************************/
