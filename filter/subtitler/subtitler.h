/*
 * Copyright (C) Thomas Östreich - June 2001
 *
 * This file is part of transcode, a linux video stream processing tool
 *
 * subtitle code by Jan Panteltje
 * 
 * transcode is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * Deinterlace routines by Miguel Freitas
 * based of DScaler project sources (deinterlace.sourceforge.net)
 *
 * Currently only available for Xv driver and MMX extensions
 *
 */

#ifndef _SUBTITLER_H_
#define _SUBTITLER_H_

/* maximum movie length in frames */
#define MAX_FRAMES	300000

/* set some limits for this system */
#define MAX_H_PIXELS		2048
#define MAX_V_PIXELS		2048
#define MAX_SCREEN_LINES	200

/* temp string sizes */
#define READSIZE	65535
#define TEMP_SIZE	65535

/* for status in frame browser USE 1 2 4 8 */
#define NEW_ENTRY		0
#define NO_SPACE		1
#define TOO_LONG		2
#define TXT_HOLD		4

/* for object type IDs */
#define FORMATTED_TEXT			1
#define X_Y_Z_T_TEXT			2
#define X_Y_Z_T_PICTURE			3
#define X_Y_Z_T_FRAME_COUNTER	4
#define X_Y_Z_T_MOVIE			5
#define MAIN_MOVIE				6
#define SUBTITLE_CONTROL		7

/* for formatting subtitles */
#define SUBTITLE_H_FACTOR	.02
#define SUBTITLE_V_FACTOR	.042

/* for this specfic default font */
#define EXTRA_CHAR_SPACE	1 //l followed by t overlap in this font

/*
for masking out areas in rote and shear.
These 2 values are related, and I have not figured out the relation yet.
YUV_MASK is used to prevent picture areas to be cut out. 
*/
#define LUMINANCE_MASK	178
#define YUV_MASK		164

/* status of operations on an object, use 1, 2, 4, etc. */
#define OBJECT_STATUS_NEW			0
#define OBJECT_STATUS_INIT			1
#define OBJECT_STATUS_GOTO			2
#define OBJECT_STATUS_HAVE_X_DEST	4
#define OBJECT_STATUS_HAVE_Y_DEST	8
#define OBJECT_STATUS_HAVE_Z_DEST	16

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <stddef.h>
#include <pwd.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <limits.h>
#include <pthread.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "transcode.h"
#include "framebuffer.h"
#include "load_font.h"
#include "frame_list.h"
#include "object_list.h"

int debug_flag;
font_desc_t *vo_font;
font_desc_t *subtitle_current_font_descriptor;
uint8_t *ImageData;
int image_width, image_height;
int default_font;
struct passwd *userinfo;
char *home_dir;
char *user_name;
int sync_mode;
int osd_transp;
int screen_start[MAX_H_PIXELS];
char *tptr;
int screen_lines;
char screen_text[MAX_SCREEN_LINES][MAX_H_PIXELS];
char format_start_str[50];
char format_end_str[50];
int vert_position;
int line_height;
int line_h_start, line_h_end;
int center_flag;
int wtop, wbottom, hstart, hend;
int window_top, window_bottom;
char *frame_memory0, *frame_memory1 ;
int file_format;
char *subtitle_file;
char *default_font_dir;
vob_t *vob;
char *selected_data_directory;
char *selected_project;
int frame_offset;
double dmax_vector;

/* for x11 stuff */
int show_output_flag;
int window_open_flag;
int window_size, buffer_size;
unsigned char *ucptrs, *ucptrd;
int color_depth;
/* end x11 stuff */

/* maximum number of movie objects that can be inserted */
#define MAX_MOVIES	1024
/* threads for other instances of transcode in insert movie */
pthread_t *movie_thread[MAX_MOVIES];
pthread_attr_t *attributes;

/* global subtitle parameters */
double ssat, dssat, scol, dscol;
double default_font_factor;

/* for rotate and shear, the luminance where we cut out the border */
int border_luminance;
int default_border_luminance;

double subtitle_h_factor;
double subtitle_v_factor;
double extra_character_space;

/* this last, so proto knows about structure definitions etc. */
#include "subtitler_proto.h"

#define SUBTITLER_VERSION "-0.6.3"

#endif /* _SUBTITLER_H_ */
