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

static int rgb_copy(uint8_t **src, uint8_t **dest, int width, int height)
{
    ac_memcpy(dest[0], src[0], width*height*3);
    return 1;
}

static int rgba_copy(uint8_t **src, uint8_t **dest, int width, int height)
{
    ac_memcpy(dest[0], src[0], width*height*4);
    return 1;
}

static int gray8_copy(uint8_t **src, uint8_t **dest, int width, int height)
{
    ac_memcpy(dest[0], src[0], width*height);
    return 1;
}

/*************************************************************************/

/* Conversions between various 32-bit formats, all usable when src==dest */

/* RGBA<->ABGR and ARGB<->BGRA: reverse byte order */
static int rgba_swapall(uint8_t **src, uint8_t **dest, int width, int height)
{
    uint32_t *srcp  = (uint32_t *)src[0];
    uint32_t *destp = (uint32_t *)dest[0];
    int i;
    for (i = 0; i < width*height; i++) {
	/* This shortcut works regardless of CPU endianness */
	destp[i] =  srcp[i]               >> 24
	         | (srcp[i] & 0x00FF0000) >>  8
	         | (srcp[i] & 0x0000FF00) <<  8
	         |  srcp[i]               << 24;
    }
    return 1;
}

/* RGBA<->BGRA: swap bytes 0 and 2 */
static int rgba_swap02(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	uint8_t tmp    = src[0][i*4+2];
	dest[0][i*4+2] = src[0][i*4  ];
	dest[0][i*4  ] = tmp;
	dest[0][i*4+1] = src[0][i*4+1];
	dest[0][i*4+3] = src[0][i*4+3];
    }
    return 1;
}

/* ARGB<->ABGR: swap bytes 1 and 3 */
static int rgba_swap13(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	uint8_t tmp    = src[0][i*4+3];
	dest[0][i*4+3] = src[0][i*4+1];
	dest[0][i*4+1] = tmp;
	dest[0][i*4  ] = src[0][i*4  ];
	dest[0][i*4+2] = src[0][i*4+2];
    }
    return 1;
}

/* RGBA->ARGB and BGRA->ABGR: alpha moves from byte 3 to byte 0 */
static int rgba_alpha30(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	uint8_t tmp    = src[0][i*4+3];
	dest[0][i*4+3] = src[0][i*4+2];
	dest[0][i*4+2] = src[0][i*4+1];
	dest[0][i*4+1] = src[0][i*4  ];
	dest[0][i*4  ] = tmp;
    }
    return 1;
}

/* ARGB->RGBA and ABGR->BGRA: alpha moves from byte 0 to byte 3 */
static int rgba_alpha03(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	uint8_t tmp    = src[0][i*4  ];
	dest[0][i*4  ] = src[0][i*4+1];
	dest[0][i*4+1] = src[0][i*4+2];
	dest[0][i*4+2] = src[0][i*4+3];
	dest[0][i*4+3] = tmp;
    }
    return 1;
}

/*************************************************************************/

static int rgb24_bgr24(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	dest[0][i*3  ] = src[0][i*3  ];
	dest[0][i*3+1] = src[0][i*3+1];
	dest[0][i*3+2] = src[0][i*3+2];
    }
    return 1;
}

#define bgr24_rgb24 rgb24_bgr24

static int rgb24_rgba32(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	dest[0][i*4  ] = src[0][i*3  ];
	dest[0][i*4+1] = src[0][i*3+1];
	dest[0][i*4+2] = src[0][i*3+2];
	dest[0][i*4+3] = 0;
    }
    return 1;
}

static int rgb24_abgr32(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	dest[0][i*4  ] = 0;
	dest[0][i*4+1] = src[0][i*3+2];
	dest[0][i*4+2] = src[0][i*3+1];
	dest[0][i*4+3] = src[0][i*3  ];
    }
    return 1;
}

static int rgb24_argb32(uint8_t **src, uint8_t **dest, int width, int height)
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

static int rgb24_bgra32(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	dest[0][i*4  ] = src[0][i*3+2];
	dest[0][i*4+1] = src[0][i*3+1];
	dest[0][i*4+2] = src[0][i*3  ];
	dest[0][i*4+3] = 0;
    }
    return 1;
}

static int rgb24_gray8(uint8_t **src, uint8_t **dest, int width, int height)
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

static int rgba32_rgb24(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	dest[0][i*3  ] = src[0][i*4  ];
	dest[0][i*3+1] = src[0][i*4+1];
	dest[0][i*3+2] = src[0][i*4+2];
    }
    return 1;
}

static int bgra32_rgb24(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	dest[0][i*3  ] = src[0][i*4+2];
	dest[0][i*3+1] = src[0][i*4+1];
	dest[0][i*3+2] = src[0][i*4  ];
    }
    return 1;
}

static int rgba32_gray8(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	/* Use the Y part of a YUV transformation, scaled to 0..255 */
	int r = src[0][i*4  ];
	int g = src[0][i*4+1];
	int b = src[0][i*4+2];
	dest[0][i] = (19595*r + 38470*g + 7471*b + 32768) >> 16;
    }
    return 1;
}

/*************************************************************************/

static int argb32_rgb24(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	dest[0][i*3  ] = src[0][i*4+1];
	dest[0][i*3+1] = src[0][i*4+2];
	dest[0][i*3+2] = src[0][i*4+3];
    }
    return 1;
}

static int abgr32_rgb24(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	dest[0][i*3  ] = src[0][i*4+3];
	dest[0][i*3+1] = src[0][i*4+2];
	dest[0][i*3+2] = src[0][i*4+1];
    }
    return 1;
}

static int argb32_gray8(uint8_t **src, uint8_t **dest, int width, int height)
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

static int gray8_rgb24(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	dest[0][i*3  ] = src[0][i];
	dest[0][i*3+1] = src[0][i];
	dest[0][i*3+2] = src[0][i];
    }
    return 1;
}

static int gray8_rgba32(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++) {
	dest[0][i*4  ] = src[0][i];
	dest[0][i*4+1] = src[0][i];
	dest[0][i*4+2] = src[0][i];
	dest[0][i*4+3] = 0;
    }
    return 1;
}

static int gray8_argb32(uint8_t **src, uint8_t **dest, int width, int height)
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

#if defined(ARCH_X86) || defined(ARCH_X86_64)

#define DEFINE_MASK_DATA
#include "img_x86_common.h"

/*************************************************************************/

/* Basic assembly routines */

/* RGBA<->ABGR and ARGB<->BGRA: reverse byte order */
static int rgba_swapall_x86(uint8_t **src, uint8_t **dest, int width, int height)
{
    ASM_REV32_X86(width*height);
    return 1;
}

/* RGBA<->BGRA: swap bytes 0 and 2 */
static int rgba_swap02_x86(uint8_t **src, uint8_t **dest, int width, int height)
{
    ASM_SWAP32_02_X86(width*height);
    return 1;
}

/* ARGB<->ABGR: swap bytes 1 and 3 */
static int rgba_swap13_x86(uint8_t **src, uint8_t **dest, int width, int height)
{
    ASM_SWAP32_13_X86(width*height);
    return 1;
}

/* RGBA->ARGB and BGRA->ABGR: alpha moves from byte 3 to byte 0 */
static int rgba_alpha30_x86(uint8_t **src, uint8_t **dest, int width, int height)
{
    ASM_ROL32_X86(width*height);
    return 1;
}

/* ARGB->RGBA and ABGR->BGRA: alpha moves from byte 0 to byte 3 */
static int rgba_alpha03_x86(uint8_t **src, uint8_t **dest, int width, int height)
{
    ASM_ROR32_X86(width*height);
    return 1;
}

/*************************************************************************/

/* MMX-optimized routines */

/* RGBA<->ABGR and ARGB<->BGRA: reverse byte order */
static int rgba_swapall_mmx(uint8_t **src, uint8_t **dest, int width, int height)
{
    ASM_REV32_MMX(width*height);
    return 1;
}

/* RGBA<->BGRA: swap bytes 0 and 2 */
static int rgba_swap02_mmx(uint8_t **src, uint8_t **dest, int width, int height)
{
    ASM_SWAP32_02_MMX(width*height);
    return 1;
}

/* ARGB<->ABGR: swap bytes 1 and 3 */
static int rgba_swap13_mmx(uint8_t **src, uint8_t **dest, int width, int height)
{
    ASM_SWAP32_13_MMX(width*height);
    return 1;
}

/* RGBA->ARGB and BGRA->ABGR: alpha moves from byte 3 to byte 0 */
static int rgba_alpha30_mmx(uint8_t **src, uint8_t **dest, int width, int height)
{
    ASM_ROL32_MMX(width*height);
    return 1;
}

/* ARGB->RGBA and ABGR->BGRA: alpha moves from byte 0 to byte 3 */
static int rgba_alpha03_mmx(uint8_t **src, uint8_t **dest, int width, int height)
{
    ASM_ROR32_MMX(width*height);
    return 1;
}

/*************************************************************************/

/* SSE2-optimized routines */
/* These are just copies of the MMX routines with registers and data sizes
 * changed. */

/* RGBA<->ABGR and ARGB<->BGRA: reverse byte order */
static int rgba_swapall_sse2(uint8_t **src, uint8_t **dest, int width, int height)
{
    ASM_REV32_SSE2(width*height);
    return 1;
}

/* RGBA<->BGRA: swap bytes 0 and 2 */
static int rgba_swap02_sse2(uint8_t **src, uint8_t **dest, int width, int height)
{
    ASM_SWAP32_02_SSE2(width*height);
    return 1;
}

/* ARGB<->ABGR: swap bytes 1 and 3 */
static int rgba_swap13_sse2(uint8_t **src, uint8_t **dest, int width, int height)
{
    ASM_SWAP32_13_SSE2(width*height);
    return 1;
}

/* RGBA->ARGB and BGRA->ABGR: alpha moves from byte 3 to byte 0 */
static int rgba_alpha30_sse2(uint8_t **src, uint8_t **dest, int width, int height)
{
    ASM_ROL32_SSE2(width*height);
    return 1;
}

/* ARGB->RGBA and ABGR->BGRA: alpha moves from byte 0 to byte 3 */
static int rgba_alpha03_sse2(uint8_t **src, uint8_t **dest, int width, int height)
{
    ASM_ROR32_SSE2(width*height);
    return 1;
}

/*************************************************************************/

#endif  /* ARCH_X86 || ARCH_X86_64 */

/*************************************************************************/
/*************************************************************************/

/* Initialization */

int ac_imgconvert_init_rgb_packed(int accel)
{
    if (!register_conversion(IMG_RGB24,   IMG_RGB24,   rgb_copy)
     || !register_conversion(IMG_RGB24,   IMG_BGR24,   rgb24_bgr24)
     || !register_conversion(IMG_RGB24,   IMG_RGBA32,  rgb24_rgba32)
     || !register_conversion(IMG_RGB24,   IMG_ABGR32,  rgb24_abgr32)
     || !register_conversion(IMG_RGB24,   IMG_ARGB32,  rgb24_argb32)
     || !register_conversion(IMG_RGB24,   IMG_BGRA32,  rgb24_bgra32)
     || !register_conversion(IMG_RGB24,   IMG_GRAY8,   rgb24_gray8)

     || !register_conversion(IMG_BGR24,   IMG_BGR24,   rgb_copy)
     || !register_conversion(IMG_BGR24,   IMG_BGR24,   bgr24_rgb24)

     || !register_conversion(IMG_RGBA32,  IMG_RGB24,   rgba32_rgb24)
     || !register_conversion(IMG_RGBA32,  IMG_RGBA32,  rgba_copy)
     || !register_conversion(IMG_RGBA32,  IMG_ABGR32,  rgba_swapall)
     || !register_conversion(IMG_RGBA32,  IMG_ARGB32,  rgba_alpha30)
     || !register_conversion(IMG_RGBA32,  IMG_BGRA32,  rgba_swap02)
     || !register_conversion(IMG_RGBA32,  IMG_GRAY8,   rgba32_gray8)

     || !register_conversion(IMG_ABGR32,  IMG_RGB24,   abgr32_rgb24)
     || !register_conversion(IMG_ABGR32,  IMG_RGBA32,  rgba_swapall)
     || !register_conversion(IMG_ABGR32,  IMG_ABGR32,  rgba_copy)
     || !register_conversion(IMG_ABGR32,  IMG_ARGB32,  rgba_swap13)
     || !register_conversion(IMG_ABGR32,  IMG_BGRA32,  rgba_alpha03)
     || !register_conversion(IMG_ABGR32,  IMG_GRAY8,   argb32_gray8)

     || !register_conversion(IMG_ARGB32,  IMG_RGB24,   argb32_rgb24)
     || !register_conversion(IMG_ARGB32,  IMG_RGBA32,  rgba_alpha03)
     || !register_conversion(IMG_ARGB32,  IMG_ABGR32,  rgba_swap13)
     || !register_conversion(IMG_ARGB32,  IMG_ARGB32,  rgba_copy)
     || !register_conversion(IMG_ARGB32,  IMG_BGRA32,  rgba_swapall)
     || !register_conversion(IMG_ARGB32,  IMG_GRAY8,   argb32_gray8)

     || !register_conversion(IMG_BGRA32,  IMG_RGB24,   bgra32_rgb24)
     || !register_conversion(IMG_BGRA32,  IMG_RGBA32,  rgba_swap02)
     || !register_conversion(IMG_BGRA32,  IMG_ABGR32,  rgba_alpha30)
     || !register_conversion(IMG_BGRA32,  IMG_ARGB32,  rgba_swapall)
     || !register_conversion(IMG_BGRA32,  IMG_BGRA32,  rgba_copy)
     || !register_conversion(IMG_BGRA32,  IMG_GRAY8,   argb32_gray8)

     || !register_conversion(IMG_GRAY8,   IMG_RGB24,   gray8_rgb24)
     || !register_conversion(IMG_GRAY8,   IMG_RGBA32,  gray8_rgba32)
     || !register_conversion(IMG_GRAY8,   IMG_ARGB32,  gray8_argb32)
     || !register_conversion(IMG_GRAY8,   IMG_GRAY8,   gray8_copy)
    ) {
	return 0;
    }

#if defined(ARCH_X86) || defined(ARCH_X86_64)

    if (accel & (AC_IA32ASM | AC_AMD64ASM)) {
	if (!register_conversion(IMG_RGBA32,  IMG_ABGR32,  rgba_swapall_x86)
	 || !register_conversion(IMG_RGBA32,  IMG_ARGB32,  rgba_alpha30_x86)
	 || !register_conversion(IMG_RGBA32,  IMG_BGRA32,  rgba_swap02_x86)

	 || !register_conversion(IMG_ABGR32,  IMG_RGBA32,  rgba_swapall_x86)
	 || !register_conversion(IMG_ABGR32,  IMG_ARGB32,  rgba_swap13_x86)
	 || !register_conversion(IMG_ABGR32,  IMG_BGRA32,  rgba_alpha03_x86)

	 || !register_conversion(IMG_ARGB32,  IMG_RGBA32,  rgba_alpha03_x86)
	 || !register_conversion(IMG_ARGB32,  IMG_ABGR32,  rgba_swap13_x86)
	 || !register_conversion(IMG_ARGB32,  IMG_BGRA32,  rgba_swapall_x86)

	 || !register_conversion(IMG_BGRA32,  IMG_RGBA32,  rgba_swap02_x86)
	 || !register_conversion(IMG_BGRA32,  IMG_ABGR32,  rgba_alpha30_x86)
	 || !register_conversion(IMG_BGRA32,  IMG_ARGB32,  rgba_swapall_x86)
	) {
	    return 0;
	}
    }

    if (accel & AC_MMX) {
	if (!register_conversion(IMG_RGBA32,  IMG_ABGR32,  rgba_swapall_mmx)
	 || !register_conversion(IMG_RGBA32,  IMG_ARGB32,  rgba_alpha30_mmx)
	 || !register_conversion(IMG_RGBA32,  IMG_BGRA32,  rgba_swap02_mmx)

	 || !register_conversion(IMG_ABGR32,  IMG_RGBA32,  rgba_swapall_mmx)
	 || !register_conversion(IMG_ABGR32,  IMG_ARGB32,  rgba_swap13_mmx)
	 || !register_conversion(IMG_ABGR32,  IMG_BGRA32,  rgba_alpha03_mmx)

	 || !register_conversion(IMG_ARGB32,  IMG_RGBA32,  rgba_alpha03_mmx)
	 || !register_conversion(IMG_ARGB32,  IMG_ABGR32,  rgba_swap13_mmx)
	 || !register_conversion(IMG_ARGB32,  IMG_BGRA32,  rgba_swapall_mmx)

	 || !register_conversion(IMG_BGRA32,  IMG_RGBA32,  rgba_swap02_mmx)
	 || !register_conversion(IMG_BGRA32,  IMG_ABGR32,  rgba_alpha30_mmx)
	 || !register_conversion(IMG_BGRA32,  IMG_ARGB32,  rgba_swapall_mmx)
	) {
	    return 0;
	}
    }

    if (accel & AC_SSE2) {
	if (!register_conversion(IMG_RGBA32,  IMG_ABGR32,  rgba_swapall_sse2)
	 || !register_conversion(IMG_RGBA32,  IMG_ARGB32,  rgba_alpha30_sse2)
	 || !register_conversion(IMG_RGBA32,  IMG_BGRA32,  rgba_swap02_sse2)

	 || !register_conversion(IMG_ABGR32,  IMG_RGBA32,  rgba_swapall_sse2)
	 || !register_conversion(IMG_ABGR32,  IMG_ARGB32,  rgba_swap13_sse2)
	 || !register_conversion(IMG_ABGR32,  IMG_BGRA32,  rgba_alpha03_sse2)

	 || !register_conversion(IMG_ARGB32,  IMG_RGBA32,  rgba_alpha03_sse2)
	 || !register_conversion(IMG_ARGB32,  IMG_ABGR32,  rgba_swap13_sse2)
	 || !register_conversion(IMG_ARGB32,  IMG_BGRA32,  rgba_swapall_sse2)

	 || !register_conversion(IMG_BGRA32,  IMG_RGBA32,  rgba_swap02_sse2)
	 || !register_conversion(IMG_BGRA32,  IMG_ABGR32,  rgba_alpha30_sse2)
	 || !register_conversion(IMG_BGRA32,  IMG_ARGB32,  rgba_swapall_sse2)
	) {
	    return 0;
	}
    }

#endif

    return 1;
}

/*************************************************************************/
