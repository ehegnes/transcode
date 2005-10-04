/*
 * colorspace.h - defines for colorspace conversion routines
 * Written by Andrew Church <achurch@achurch.org>
 */

extern void colorspace_init(int accel);

typedef void (*yuv2rgb_func)(u_int8_t *dest, u_int8_t *srcY, u_int8_t *srcU,
			     u_int8_t *srcV, int width, int height,
			     int rgb_stride, int y_stride, int uv_stride);
extern yuv2rgb_func yuv2rgb;

typedef void (*rgb2yuv_func)(u_int8_t *destY, u_int8_t *destU, u_int8_t *destV,
			     u_int8_t *src, int width, int height,
			     int rgb_stride, int y_stride, int uv_stride);
extern rgb2yuv_func rgb2yuv;
