/*
 *  frame_video.h
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  This file is part of transcode, a linux video stream processing tool
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

#include <pthread.h>
#include <stdlib.h>

#ifndef _FRAME_VIDEO_H
#define _FRAME_VIDEO_H 

#define FRAME_VIDEO_NULL  -1
#define FRAME_VIDEO_EMPTY  0
#define FRAME_VIDEO_READY  1
#define FRAME_VIDEO_LOCKED 2
#define FRAME_VIDEO_WAIT   3


typedef struct frame_video_list {
  
  int id;        // frame number
  int status;    // frame status
  
  char *video_buf;

  struct frame_video_list *next;
  struct frame_video_list *prev;
  
  } frame_video_list_t;

frame_video_list_t *frame_video_register(int id);
void frame_video_remove(frame_video_list_t *ptr);
frame_video_list_t *frame_video_retrieve();
frame_video_list_t *frame_video_retrieve_status(int old_status, int new_status);
void frame_video_set_status(frame_video_list_t *ptr, int status);

extern pthread_mutex_t frame_video_list_lock;

extern frame_video_list_t *frame_video_list_head;
extern frame_video_list_t *frame_video_list_tail;


#endif
