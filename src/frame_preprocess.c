/*
 *  frame_preprocess.c
 *
 *  Copyright (C) Thomas Östreich - May 2002
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

#include "transcode.h"
#include "framebuffer.h"
#include "video_trans.h"

extern void check_clip_para(int p);

int preprocess_yuv_frame(vob_t *vob, vframe_list_t *ptr)
{
    
  /* ------------------------------------------------------------ 
   *
   * clip lines from top and bottom 
   *
   * ------------------------------------------------------------*/
    
  if(pre_im_clip && (vob->pre_im_clip_top || vob->pre_im_clip_bottom)) {
    
    check_clip_para(vob->pre_im_clip_top);
    check_clip_para(vob->pre_im_clip_bottom);
    
    if(vob->pre_im_clip_top==vob->pre_im_clip_bottom) {
	yuv_vclip(ptr->video_buf, ptr->v_width, ptr->v_height, vob->pre_im_clip_top);
    } else {

	yuv_clip_top_bottom(ptr->video_buf, ptr->video_buf_Y[ptr->free], ptr->v_width, ptr->v_height, vob->pre_im_clip_top, vob->pre_im_clip_bottom);
	
	// adjust pointer, clipped frame in tmp buffer
	ptr->video_buf = ptr->video_buf_Y[ptr->free];
	ptr->free = (ptr->free) ? 0:1;
    }
    
    // update frame_list_t *ptr
    
    ptr->v_height -= (vob->pre_im_clip_top + vob->pre_im_clip_bottom);
    
  }
  
  /* ------------------------------------------------------------ 
   *
   * clip coloums from left and right
   *
   * ------------------------------------------------------------*/
  
  if(pre_im_clip && (vob->pre_im_clip_left || vob->pre_im_clip_right)) {
    
    check_clip_para(vob->pre_im_clip_left);
    check_clip_para(vob->pre_im_clip_right);
    
    if(vob->pre_im_clip_left == vob->pre_im_clip_right) {
      yuv_hclip(ptr->video_buf, ptr->v_width, ptr->v_height, vob->pre_im_clip_left);
    } else {
      
      yuv_clip_left_right(ptr->video_buf, ptr->v_width, ptr->v_height, vob->pre_im_clip_left, vob->pre_im_clip_right);
    }	  
    
    // update frame_list_t *ptr
    
    ptr->v_width -= (vob->pre_im_clip_left + vob->pre_im_clip_right);
  }
  
  //done
  return(0);
}



int preprocess_rgb_frame(vob_t *vob, vframe_list_t *ptr)
{

  
  /* ------------------------------------------------------------ 
   *
   * clip rows from top/bottom before processing frame
   *
   * ------------------------------------------------------------*/
  
   if(pre_im_clip && (vob->pre_im_clip_top || vob->pre_im_clip_bottom)) {
	
	if(vob->pre_im_clip_top==vob->pre_im_clip_bottom) {
	    rgb_vclip(ptr->video_buf, ptr->v_width, ptr->v_height, vob->pre_im_clip_top);
	} else {
	    
	  rgb_clip_top_bottom(ptr->video_buf, ptr->video_buf_RGB[ptr->free], ptr->v_width, ptr->v_height, vob->pre_im_clip_top, vob->pre_im_clip_bottom);
	  
	  // adjust pointer, zoomed frame in tmp buffer
	  ptr->video_buf = ptr->video_buf_RGB[ptr->free];
	  ptr->free = (ptr->free) ? 0:1;
	}
	
	// update frame_list_t *ptr
	
	ptr->v_height -= (vob->pre_im_clip_top + vob->pre_im_clip_bottom);
	ptr->video_size = ptr->v_height *  ptr->v_width * ptr->v_bpp/8;
    }
    
    /* ------------------------------------------------------------ 
     *
     * clip coloums from left and right
     *
     * ------------------------------------------------------------*/

   if(pre_im_clip && (vob->pre_im_clip_left || vob->pre_im_clip_right)) {
      
      if(vob->pre_im_clip_left == vob->pre_im_clip_right) {
	  rgb_hclip(ptr->video_buf, ptr->v_width, ptr->v_height, vob->pre_im_clip_left);
      } else {
	  
	  rgb_clip_left_right(ptr->video_buf, ptr->v_width, ptr->v_height, vob->pre_im_clip_left, vob->pre_im_clip_right);
      }	  
      
      // update frame_list_t *ptr
      
      ptr->v_width -= (vob->pre_im_clip_left + vob->pre_im_clip_right);
      ptr->video_size = ptr->v_height *  ptr->v_width * ptr->v_bpp/8;
   }
   
   return(0);
}


/* ------------------------------------------------------------ 
 *
 * video frame transformation 
 *
 * image frame buffer: ptr->video_buf
 *
 * notes: (1) physical frame data at any time stored in frame_list_t *ptr
 *        (2) vob_t *vob structure contains information on
 *            video transformation and import/export frame parameter
 *
 * ------------------------------------------------------------*/


int preprocess_vid_frame(vob_t *vob, vframe_list_t *ptr)
    
{
  struct fc_time *t=vob->ttime;
  int skip=1;

  // set skip attribute very early based on -c
  while (t) {
      if (t->stf <= ptr->id && ptr->id < t->etf)  {
	  skip=0;
	  break;
      }
      t = t->next;
  }
  
  if (skip) {
      ptr->attributes |= TC_FRAME_IS_OUT_OF_RANGE;
      return 0;
  }

  // check for pass-through mode

  if(vob->pass_flag & TC_VIDEO) return(0);
  
  // check if a frame data are in RGB colorspace
  
  if(vob->im_v_codec == CODEC_RGB) {
      ptr->v_codec = CODEC_RGB;
      return(preprocess_rgb_frame(vob, ptr));
  }

  // check if frame data are in YCrCb colorspace
  // only a limited number of transformations yet supported

  // as of 0.5.0, all frame operations are available for RGB and YUV
  
  if(vob->im_v_codec == CODEC_YUV) {
      ptr->v_codec = CODEC_YUV;
      return(preprocess_yuv_frame(vob, ptr));
  }
  
  tc_error("Oops, invalid colorspace video frame data"); 
  
  return 0;
}

