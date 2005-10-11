/*
 *  video_trans.h
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  This file is part of transcode, a video stream processing tool
 *      
 *  transcode is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  transcode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */

#ifndef _VIDEO_TRANS_H
#define _VIDEO_TRANS_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "transcode.h"
#include "frame_threads.h"

# ifndef PI
# define PI        3.14159265358979323846      /* pi */
# endif

#define dei_u_threshold 0.75
#define dei_l_threshold 0.25

typedef struct _redtab_t {

    int source;

    unsigned long weight1, weight2;

    int offset;

    int dei;

} redtab_t;


void init_aa_table(double aa_weight, double aa_bias);

int process_vid_frame(vob_t *vob, vframe_list_t *ptr);
int preprocess_vid_frame(vob_t *vob, vframe_list_t *ptr);
int postprocess_vid_frame(vob_t *vob, vframe_list_t *ptr);

void rgb_rescale(uint8_t *image, int width, int height, int reduce_h, int reduce_w);
void rgb_flip(uint8_t *image, int width, int height);
void rgb_clip_left_right(uint8_t *image, int width, int height, int cols_left, int cols_right);
void rgb_clip_top_bottom(uint8_t *image, uint8_t *dest, int width, int height, int lines_top, int lines_bottom);
void rgb_hclip(uint8_t *image, int width, int height, int cols);
void rgb_vclip(uint8_t *image, int width, int height, int lines);
void rgb_mirror(uint8_t *image, int width,  int height);
void rgb_swap(uint8_t *image, int pixels);
void rgb_vresize_8(uint8_t *image, int width, int height, int resize);
void rgb_hresize_8(uint8_t *image, int width, int height, int resize);
void rgb_hresize_8_up(uint8_t *image, uint8_t *tmp_image, int width, int height, int resize);
void rgb_vresize_8_up(uint8_t *image, uint8_t *tmp_image, int width, int height, int resize);
void rgb_deinterlace_linear(uint8_t *image, int width, int height);
void rgb_deinterlace_linear_blend(uint8_t *image, uint8_t *tmp, int width, int height);
inline void rgb_decolor(uint8_t *image, int bytes);
inline void rgb_gamma(uint8_t *image, int bytes);
void rgb_antialias(uint8_t *image, uint8_t *dest, int width, int height, int mode);
void rgb_zoom(uint8_t *image, int width, int height, int new_width, int new_height);
void rgb_zoom_DI(uint8_t *image, int width, int height, int new_width, int new_height);
void deinterlace_rgb_zoom(uint8_t *src, int width, int height);
void deinterlace_rgb_nozoom(uint8_t *src, int width, int height);

void init_table_8(redtab_t *table, int length, int resize);
void init_table_8_up(redtab_t *table, int length, int resize);
void init_gamma_table(uint8_t *table, double gamma);

void check_clip_para(int p);

void yuv_rescale(uint8_t *image, int width, int height, int reduce_h, int reduce_w);
void yuv_flip(uint8_t *image, int width, int height);
void yuv_hclip(uint8_t *image, int width, int height, int cols);
void yuv_clip_left_right(uint8_t *image, int width, int height, int cols_left, int cols_right);
void yuv_clip_top_bottom(uint8_t *image, uint8_t *dest, int width, int height, int lines_top, int lines_bottom);
void yuv_mirror(uint8_t *image, int width,  int height);
void yuv_swap(uint8_t *image, int width,  int height);
void yuv_vresize_8(uint8_t *image, int width, int height, int resize);
void yuv_hresize_8(uint8_t *image, int width, int height, int resize);
void yuv_hresize_8_up(uint8_t *image, uint8_t *image_out, int width, int height, int resize);
void yuv_vresize_8_up(uint8_t *image, uint8_t *image_out, int width, int height, int resize);
void yuv_deinterlace_linear(uint8_t *image, int width, int height);
void yuv_deinterlace_linear_blend(uint8_t *image, uint8_t *tmp, int width, int height);
inline void yuv_decolor(uint8_t *image, int bytes);
inline void yuv_gamma(uint8_t *image, int bytes);
void yuv_vclip(uint8_t *image, int width, int height, int lines);
void yuv_zoom(uint8_t *image, uint8_t *tmp, int width, int height, int new_width, int new_height);
void yuv_zoom_DI(uint8_t *image, int width, int height, int new_width, int new_height);
void deinterlace_yuv_zoom(uint8_t *src, int width, int height);
void deinterlace_yuv_nozoom(uint8_t *src, int width, int height);
void yuv_antialias(uint8_t *image, uint8_t *dest, int width, int height, int mode);


void yuv422_rescale(uint8_t *image, int width, int height, int reduce_h, int reduce_w);
void yuv422_flip(uint8_t *image, int width, int height);
void yuv422_hclip(uint8_t *image, int width, int height, int cols);
void yuv422_clip_left_right(uint8_t *image, int width, int height, int cols_left, int cols_right);
void yuv422_clip_top_bottom(uint8_t *image, uint8_t *dest, int width, int height, int lines_top, int lines_bottom);
void yuv422_mirror(uint8_t *image, int width,  int height);
void yuv422_swap(uint8_t *image, int width,  int height);
void yuv422_vresize_8(uint8_t *image, int width, int height, int resize);
void yuv422_hresize_8(uint8_t *image, int width, int height, int resize);
void yuv422_hresize_8_up(uint8_t *image, uint8_t *tmp_image, int width, int height, int resize);
void yuv422_vresize_8_up(uint8_t *image, uint8_t *tmp_image, int width, int height, int resize);
void yuv422_deinterlace_linear(uint8_t *image, int width, int height);
void yuv422_deinterlace_linear_blend(uint8_t *image, uint8_t *tmp, int width, int height);
inline void yuv422_decolor(uint8_t *image, int bytes);
inline void yuv422_gamma(uint8_t *image, int bytes);
void yuv422_vclip(uint8_t *image, int width, int height, int lines);
void yuv422_zoom(uint8_t *image, uint8_t *tmp, int width, int height, int new_width, int new_height);
void yuv422_zoom_DI(uint8_t *image, int width, int height, int new_width, int new_height);
void deinterlace_yuv422_zoom(uint8_t *src, int width, int height);
void deinterlace_yuv422_nozoom(uint8_t *src, int width, int height);
void yuv422_antialias(uint8_t *image, uint8_t *dest, int width, int height, int mode);


extern uint8_t *tmp_image;
extern int vert_table_8_flag;
extern int hori_table_8_flag;
extern redtab_t vert_table_8[];
extern redtab_t vert_table_8_up[];
extern redtab_t hori_table_8[];
extern redtab_t hori_table_8_up[];

extern int gamma_table_flag;
extern unsigned char gamma_table[];

extern unsigned long *aa_table_c;
extern unsigned long *aa_table_x;
extern unsigned long *aa_table_d;	    
extern unsigned long *aa_table_y;

extern void clear_mmx(void);

# define d_threshold 25
# define s_threshold 25

# define YRED    0.299 //0.2125
# define YGREEN  0.587 //0.7154
# define YBLUE   0.114 //0.0721

#endif
