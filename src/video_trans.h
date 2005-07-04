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

void rgb_rescale(char *image, int width, int height, int reduce_h, int reduce_w);
void rgb_flip(char *image, int width, int height);
void rgb_clip_left_right(char *image, int width, int height, int cols_left, int cols_right);
void rgb_clip_top_bottom(char *image, char *dest, int width, int height, int lines_top, int lines_bottom);
void rgb_hclip(char *image, int width, int height, int cols);
void rgb_vclip(char *image, int width, int height, int lines);
void rgb_mirror(char *image, int width,  int height);
void rgb_swap(char *image, int pixels);
int rgb_merge_C(char *row1, char *row2, char *out, int bytes, 
		unsigned long weight1, unsigned long weight2);
void rgb_vresize_8(char *image, int width, int height, int resize);
void rgb_hresize_8(char *image, int width, int height, int resize);
void rgb_hresize_8_up(char *image, char *tmp_image, int width, int height, int resize);
void rgb_vresize_8_up(char *image, char *tmp_image, int width, int height, int resize);
void rgb_deinterlace_linear(char *image, int width, int height);
void rgb_deinterlace_linear_blend(char *image, char *tmp, int width, int height);
inline void rgb_decolor(char *image, int bytes);
inline void rgb_gamma(char *image, int bytes);
void rgb_antialias(char *image, char *dest, int width, int height, int mode);
void rgb_zoom(char *image, int width, int height, int new_width, int new_height);
void rgb_zoom_DI(char *image, int width, int height, int new_width, int new_height);
void deinterlace_rgb_zoom(unsigned char *src, int width, int height);
void deinterlace_rgb_nozoom(unsigned char *src, int width, int height);

void init_table_8(redtab_t *table, int length, int resize);
void init_table_8_up(redtab_t *table, int length, int resize);
void init_gamma_table(unsigned char *table, double gamma);

void yuv_rescale(char *image, int width, int height, int reduce_h, int reduce_w);
void yuv_flip(char *image, int width, int height);
void yuv_hclip(char *image, int width, int height, int cols);
void yuv_clip_left_right(char *image, int width, int height, int cols_left, int cols_right);
void yuv_clip_top_bottom(char *image, char *dest, int width, int height, int lines_top, int lines_bottom);
void yuv_mirror(char *image, int width,  int height);
void yuv_swap(char *image, int width,  int height);

int yuv_merge_C(char *row1, char *row2, char *out, int bytes, 
		unsigned long weight1, unsigned long weight2);

void yuv_vresize_8(char *image, int width, int height, int resize);
void yuv_hresize_8(char *image, int width, int height, int resize);
void yuv_hresize_8_up(char *image, char *image_out, int width, int height, int resize);
void yuv_vresize_8_up(char *image, char *image_out, int width, int height, int resize);
void yuv_deinterlace_linear(char *image, int width, int height);
void yuv_deinterlace_linear_blend(char *image, char *tmp, int width, int height);
inline void yuv_decolor(char *image, int bytes);
inline void yuv_gamma(char *image, int bytes);
void yuv_vclip(char *image, int width, int height, int lines);
void yuv_zoom(char *image, char *tmp, int width, int height, int new_width, int new_height);
void yuv_zoom_DI(char *image, int width, int height, int new_width, int new_height);
void deinterlace_yuv_zoom(unsigned char *src, int width, int height);
void deinterlace_yuv_nozoom(unsigned char *src, int width, int height);
void yuv_antialias(char *image, char *dest, int width, int height, int mode);


void yuv422_rescale(char *image, int width, int height, int reduce_h, int reduce_w);
void yuv422_flip(char *image, int width, int height);
void yuv422_hclip(char *image, int width, int height, int cols);
void yuv422_clip_left_right(char *image, int width, int height, int cols_left, int cols_right);
void yuv422_clip_top_bottom(char *image, char *dest, int width, int height, int lines_top, int lines_bottom);
void yuv422_mirror(char *image, int width,  int height);
void yuv422_swap(char *image, int width,  int height);

int yuv422_merge_C(char *row1, char *row2, char *out, int bytes, 
		unsigned long weight1, unsigned long weight2);

void yuv422_vresize_8(char *image, int width, int height, int resize);
void yuv422_hresize_8(char *image, int width, int height, int resize);
void yuv422_hresize_8_up(char *image, char *tmp_image, int width, int height, int resize);
void yuv422_vresize_8_up(char *image, char *tmp_image, int width, int height, int resize);
void yuv422_deinterlace_linear(char *image, int width, int height);
void yuv422_deinterlace_linear_blend(char *image, char *tmp, int width, int height);
inline void yuv422_decolor(char *image, int bytes);
inline void yuv422_gamma(char *image, int bytes);
void yuv422_vclip(char *image, int width, int height, int lines);
void yuv422_zoom(char *image, char *tmp, int width, int height, int new_width, int new_height);
void yuv422_zoom_DI(char *image, int width, int height, int new_width, int new_height);
void deinterlace_yuv422_zoom(unsigned char *src, int width, int height);
void deinterlace_yuv422_nozoom(unsigned char *src, int width, int height);
void yuv422_antialias(char *image, char *dest, int width, int height, int mode);


extern char *tmp_image;
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

extern void clear_mmx();

# define d_threshold 25
# define s_threshold 25

# define YRED    0.299 //0.2125
# define YGREEN  0.587 //0.7154
# define YBLUE   0.114 //0.0721

extern int (*yuv_merge_8)(char *row1, char *row2, char *out, int bytes, 
		   unsigned long weight1, unsigned long weight2);

extern int (*yuv_merge_16)(char *row1, char *row2, char *out, int bytes, 
		    unsigned long weight1, unsigned long weight2);

extern int (*rgb_merge)(char *row1, char *row2, char *out, int bytes, 
		 unsigned long weight1, unsigned long weight2);

extern int (*yuv422_merge)(char *row1, char *row2, char *out, int bytes, 
		 unsigned long weight1, unsigned long weight2);


#endif
