/*
   RTjpeg (C) Justin Schoeman 1998 (justin@suntiger.ee.up.ac.za)

   With modifications by:
   (c) 1998, 1999 by Joerg Walter <trouble@moes.pmnet.uni-oldenburg.de>
   and
   (c) 1999 by Wim Taymans <wim.taymans@tvd.be>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdint.h>

extern void RTjpeg_init_Q(uint8_t Q);
extern void RTjpeg_init_compress(uint32_t *buf, int width, int height, uint8_t Q);
extern void RTjpeg_init_decompress(uint32_t *buf, int width, int height);
extern int RTjpeg_compressYUV420(int8_t *sp, unsigned char *bp);
extern int RTjpeg_compressYUV422(int8_t *sp, unsigned char *bp);
extern void RTjpeg_decompressYUV420(int8_t *sp, uint8_t *bp);
extern void RTjpeg_decompressYUV422(int8_t *sp, uint8_t *bp);
extern int RTjpeg_compress8(int8_t *sp, unsigned char *bp);
extern void RTjpeg_decompress8(int8_t *sp, uint8_t *bp);

extern void RTjpeg_init_mcompress(void);
extern int RTjpeg_mcompressYUV420(int8_t *sp, unsigned char *bp, uint16_t lmask, uint16_t cmask);
extern int RTjpeg_mcompressYUV422(int8_t *sp, unsigned char *bp, uint16_t lmask, uint16_t cmask);
extern int RTjpeg_mcompress8(int8_t *sp, unsigned char *bp, uint16_t lmask);
extern void RTjpeg_set_test(int i);

extern void RTjpeg_yuv420rgb(uint8_t *buf, uint8_t *rgb, int stride);
extern void RTjpeg_yuv422rgb(uint8_t *buf, uint8_t *rgb, int stride);
extern void RTjpeg_yuvrgb8(uint8_t *buf, uint8_t *rgb, int stride);
extern void RTjpeg_yuvrgb16(uint8_t *buf, uint8_t *rgb, int stride);
extern void RTjpeg_yuvrgb24(uint8_t *buf, uint8_t *rgb, int stride);
extern void RTjpeg_yuvrgb32(uint8_t *buf, uint8_t *rgb, int stride);


