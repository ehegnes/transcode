/*
 *  transcoders.h
 *
 *  Several parts of transcode for trivial color space conversions.
 *  See vid_aux.* and decode_dv.* for licensing details.
 */

extern void uyvytoyuy2(char *input, char *output, int width, int height);
extern void yv12toyuy2(char *_y, char *_u, char *_v, char *output, int width, int height);
extern void yuy2toyv12(char *_y, char *_u, char *_v, char *input, int width, int height);
extern void yuy2touyvy(char *dest, char *src, int width, int height);


