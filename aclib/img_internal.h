/*
 * img_internal.h - imgconvert internal use header
 * Written by Andrew Church <achurch@achurch.org>
 */

#ifndef ACLIB_IMG_INTERNAL_H
#define ACLIB_IMG_INTERNAL_H

/* Type of a conversion function */
typedef int (*ConversionFunc)(uint8_t **src, uint8_t **dest,
                              int width, int height);

/* Function to register a conversion */
extern int register_conversion(ImageFormat srcfmt, ImageFormat destfmt,
                               ConversionFunc function);

/* Initialization routines */
extern int ac_imgconvert_init(int accel);
extern int ac_imgconvert_init_yuv_planar(int accel);
extern int ac_imgconvert_init_yuv_packed(int accel);
extern int ac_imgconvert_init_yuv_mixed(int accel);
extern int ac_imgconvert_init_yuv_rgb(int accel);
extern int ac_imgconvert_init_rgb_packed(int accel);

#endif  /* ACLIB_IMG_INTERNAL_H */
