#ifndef __ENCORE_RGB2YUV_H
#define __ENCORE_RGB2YUV_H

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#if !defined(uint8_t)
#include <inttypes.h>
#endif

#include <string.h>

int RGB2YUV(int x_dim, int y_dim, uint8_t *bmp, uint8_t *y_out,
	    uint8_t *u_out, uint8_t *v_out, int x_stride, int flip);
int YUV2YUV(int x_dim, int y_dim, uint8_t *bmp, uint8_t *y_out,
	    uint8_t *u_out, uint8_t *v_out, int x_stride, int flip);
void init_rgb2yuv();

void yuv422_to_yuv420p(int x_dim, int y_dim, uint8_t *bmp, 
					  uint8_t *y_out, uint8_t *u_out, uint8_t *v_out, 
					  int x_stride, int flip);


#endif
