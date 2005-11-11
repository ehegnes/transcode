/*
 *  filter.h
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

#ifndef _FILTER_H
#define _FILTER_H

#include "framebuffer.h"

#define MAX_FILTER 16

#define M_BUF_SIZE 8192

#define MAX_FILTER_NAME_LEN 32

typedef struct filter_s {

  int id;
  int status;

  int unload;

  char *options;

  void *handle;

  char *name;
  int namelen;

  int (*entry)(void *ptr, void *options);

} filter_t;

extern char *plugins_string;

int process_vid_plugins(vframe_list_t *ptr);
int process_aud_plugins(aframe_list_t *ptr);

int plugin_close(void);
int filter_close(void);

int plugin_init(vob_t *vob);
int filter_init(void);

// instance maybe -1 to pick the first match
filter_t * plugin_by_name (char *name, int instance);

// s == "name[#instance][=.*]"
int plugin_find_id(char *s);
int plugin_get_handle (char *name);
int plugin_disable_id (int id);
int plugin_enable_id (int id);
int plugin_single_close(int id);

int load_single_plugin (char *mfilter_string);

int plugin_list_disabled(char *buf);
int plugin_list_enabled(char *buf);
int plugin_list_loaded(char *buf);

char * filter_single_readconf(int id);
int filter_single_configure_handle(int handle, char *options);
#endif

// filter module entry point
int tc_filter(frame_list_t *ptr, char *options);
