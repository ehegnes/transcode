/*
 *  video_trans.c
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

#include "framebuffer.h"
#include "video_trans.h"
#include "aclib/ac.h"

/* ------------------------------------------------------------ 
 *
 * video frame transformation auxiliary routines
 *
 * ------------------------------------------------------------*/

redtab_t vert_table_8[TC_MAX_V_FRAME_HEIGHT>>3];
redtab_t vert_table_8_up[TC_MAX_V_FRAME_HEIGHT>>3];
redtab_t hori_table_8[TC_MAX_V_FRAME_WIDTH>>3];
redtab_t hori_table_8_up[TC_MAX_V_FRAME_WIDTH>>3];

int vert_table_8_flag=0;
int vert_table_8_up_flag=0;
int hori_table_8_flag=0;
int hori_table_8_up_flag=0;

unsigned char gamma_table[256];
static int gamma_table_flag = 0;

static unsigned long *aa_table;
static int aa_table_flag = 0;
unsigned long *aa_table_c;
unsigned long *aa_table_x;
unsigned long *aa_table_y;
unsigned long *aa_table_d;


char *tmp_image=NULL;


static int yuv_vert_resize_init_flag=0;
static int rgb_vert_resize_init_flag=0;

void yuv_vert_resize_init(int width)
{
  
  yuv_vert_resize_init_flag=1;

  yuv_merge_8  = yuv_merge_C;
  yuv_merge_16 = yuv_merge_C;

#ifdef ARCH_X86
#ifdef HAVE_ASM_NASM
  
  if(((width>>1) % 16) == 0) {
    if(tc_accel & MM_MMXEXT) yuv_merge_8 = ac_rescale_mmxext;
    if(tc_accel & MM_SSE) yuv_merge_8 = ac_rescale_sse;
    if(tc_accel & MM_SSE2) yuv_merge_8 = ac_rescale_sse2; 
  } else {
    if(tc_accel & MM_SSE2 || tc_accel & MM_SSE || tc_accel & MM_MMXEXT)
      yuv_merge_8 = ac_rescale_mmxext;
  }
  
  if((width % 16) == 0) {
    if(tc_accel & MM_MMXEXT) yuv_merge_16=ac_rescale_mmxext;
    if(tc_accel & MM_SSE) yuv_merge_16=ac_rescale_sse;
    if(tc_accel & MM_SSE2) yuv_merge_16=ac_rescale_sse2; 
  } else {
    if(tc_accel & MM_SSE2 || tc_accel & MM_SSE || tc_accel & MM_MMXEXT) {
      yuv_merge_16 = ac_rescale_mmxext;
      yuv_merge_8 = yuv_merge_C;
    }
  }
  
#endif
#endif
  
  return;
}


void rgb_vert_resize_init()
{
  
  rgb_vert_resize_init_flag=1;
  
#ifdef ARCH_X86
#ifdef HAVE_ASM_NASM
  
  if(tc_accel & MM_SSE2) {
    rgb_merge=ac_rescale_sse2;
    return;
  }
  
  if(tc_accel & MM_SSE) {
    rgb_merge=ac_rescale_sse;
    return;
  }
  
  if(tc_accel & MM_MMXEXT) {
    rgb_merge=ac_rescale_mmxext;
    return;
  }
  
#endif
#endif
  
  rgb_merge=rgb_merge_C;
  
  return;
}

inline void clear_mmx()
{
#ifdef ARCH_X86
#ifdef HAVE_MMX
  __asm __volatile ("emms;":::"memory");    
#endif
#endif
}

void init_table_8(redtab_t *table, int length, int resize)
{
  int rows, i, j=0;
  
  double cc=0;
    
  // new number of rows per chunk, frame has 8 chunks.
  
  rows = (length - (resize<<3))>>3;
  
  if(verbose & TC_VIDCORE) printf("%8s %8s %10s %10s %10s %6s\n", "new row", "old row", "scaling", "weight1", "weight2", "inter");
  
  
  for(i=0; i<rows; ++i) {
    
    cc =  ((double) i) * length/(length - (resize<<3));
    
    j = (unsigned int) cc; 
    
    table[i].source = j;
    
    table[i].weight1 = cos((cc-j)*PI*0.5)*cos((cc-j)*PI*0.5) * 65536;
    table[i].weight2 = 65536 - table[i].weight1;
    
    table[i].dei=0;
    
    if((table[i].weight1 > 65536*dei_l_threshold) && (table[i].weight1 < 65536*dei_u_threshold)) table[i].dei=1;
   
    if(verbose & TC_VIDCORE) 
      printf("%8d %8d %10.4f %10.4f %10.4f %6d\n", 
	     i, j, cc, table[i].weight1/65536.0, table[i].weight2/65536.0,
	     table[i].dei);
  }
    

  return;    
}

void init_table_8_up(redtab_t *table, int length, int resize)
{
  int rows, i, j=0;
  
  double cc=0;
    
  // new number of rows per chunk, frame has 8 chunks.
  
  rows = (length + (resize<<3))>> 3;
  
  if(verbose & TC_VIDCORE) printf("%8s %8s %10s %10s %10s %6s\n", "new row", "old row", "scaling", "weight1", "weight2", "inter");
  
  
  for(i=0; i<rows; ++i) {
    
    cc =  ((double) i) * length/(length + (resize<<3));
    
    j = (unsigned int) cc; 
    
    table[i].source = j;
    
    table[i].weight1 = cos((cc-j)*PI*0.5)*cos((cc-j)*PI*0.5) * 65536;
    table[i].weight2 = 65536 - table[i].weight1;
    
    table[i].dei=0;
    
    if((table[i].weight1 > 65536*dei_l_threshold) && (table[i].weight1 < 65536*dei_u_threshold)) table[i].dei=1;
   
    if(verbose & TC_VIDCORE) 
      printf("%8d %8d %10.4f %10.4f %10.4f %6d\n", 
	     i, j, cc, table[i].weight1/65536.0, table[i].weight2/65536.0,
	     table[i].dei);
  }
    

  return;    
}

void init_gamma_table(unsigned char *table, double gamma)
{
  int i;
  
  double val;

  /*  build pre-calculated gamma correction lookup table  */
    
  for (i=0; i<256; i++) {
    val = i/255.0;
    val = pow(val, gamma);
    val = 255.0*val;
    table[i] = (unsigned char) val;
  }

  return;    
}

void check_clip_para(int p)
{

  static int notify=0;

  if(notify) return;
  if((p - ((p>>1)<<1)) != 0) 
    printf("[%s] warning: odd clip parameter invalid for Y'CbCr processing mode\n", __FILE__);
  notify=1;
}

void init_aa_table(double aa_weight, double aa_bias)
{
  int i;

  if(aa_table_flag) return;
	  
  if((aa_table = (unsigned long *) malloc(4*256*sizeof(unsigned long)))==NULL) {
    fprintf(stderr, "(%s) out of memory\n", __FILE__);
    exit(1);
  }
  
  aa_table_c = aa_table;
  aa_table_x = &aa_table[256];
  aa_table_y = &aa_table[2*256];
  aa_table_d = &aa_table[3*256];
  
  for(i=0; i<256; ++i) {
    aa_table_c[i] = i*aa_weight * 65536;
    aa_table_x[i] = i*aa_bias*(1.0-aa_weight)/4.0*65536;
    aa_table_y[i] = i*(1.0-aa_bias)*(1.0-aa_weight)/4.0* 65536;
    aa_table_d[i] = (aa_table_x[i]+aa_table_y[i])/2;
  }
  aa_table_flag = 1; 
}
   
int process_yuv_frame(vob_t *vob, vframe_list_t *ptr)
{
    
  /* ------------------------------------------------------------ 
   *
   * clip lines from top and bottom 
   *
   * ------------------------------------------------------------*/
    
  if(im_clip && (vob->im_clip_top || vob->im_clip_bottom)) {
    
    check_clip_para(vob->im_clip_top);
    check_clip_para(vob->im_clip_bottom);
    
    if(vob->im_clip_top==vob->im_clip_bottom) {
	yuv_vclip(ptr->video_buf, ptr->v_width, ptr->v_height, vob->im_clip_top);
    } else {

	yuv_clip_top_bottom(ptr->video_buf, ptr->video_buf_Y[ptr->free], ptr->v_width, ptr->v_height, vob->im_clip_top, vob->im_clip_bottom);
	
	// adjust pointer, clipped frame in tmp buffer
	ptr->video_buf = ptr->video_buf_Y[ptr->free];
	ptr->free = (ptr->free) ? 0:1;
    }
    
    // update frame_list_t *ptr
    
    ptr->v_height -= (vob->im_clip_top + vob->im_clip_bottom);
    ptr->video_size = ptr->v_height *  ptr->v_width * ptr->v_bpp/8/2;
    
  }
  
  /* ------------------------------------------------------------ 
   *
   * clip coloums from left and right
   *
   * ------------------------------------------------------------*/
  
  if(im_clip && (vob->im_clip_left || vob->im_clip_right)) {
    
    check_clip_para(vob->im_clip_left);
    check_clip_para(vob->im_clip_right);
    
    if(vob->im_clip_left == vob->im_clip_right) {
      yuv_hclip(ptr->video_buf, ptr->v_width, ptr->v_height, vob->im_clip_left);
    } else {
      
      yuv_clip_left_right(ptr->video_buf, ptr->v_width, ptr->v_height, vob->im_clip_left, vob->im_clip_right);
    }	  
    
    // update frame_list_t *ptr
    
    ptr->v_width -= (vob->im_clip_left + vob->im_clip_right);
    ptr->video_size = ptr->v_height *  ptr->v_width * ptr->v_bpp/8/2;
  }
  
  /* ------------------------------------------------------------ 
   *
   * deinterlace video frame
   *
   * ------------------------------------------------------------*/
  
  if(vob->deinterlace>0) {
    
    switch (vob->deinterlace) {
      
    case 1:
      yuv_deinterlace_linear(ptr->video_buf, ptr->v_width, ptr->v_height);
      break;
      
    case 2:
      //handled by encoder
      break;
      
    case 3:
      deinterlace_yuv_zoom(ptr->video_buf, ptr->v_width, ptr->v_height);
      break;

    case 4:
      deinterlace_yuv_nozoom(ptr->video_buf, ptr->v_width, ptr->v_height);
      ptr->v_height /=2;
      break;

    case 5:
      yuv_deinterlace_linear_blend(ptr->video_buf, ptr->video_buf_Y[ptr->free], ptr->v_width, ptr->v_height);
      break;
    }
  }


  // check for de-interlace attribute


  if(ptr->attributes & TC_FRAME_IS_INTERLACED) {
      
      switch (ptr->deinter_flag) {
	  
      case 1:
	  yuv_deinterlace_linear(ptr->video_buf, ptr->v_width, ptr->v_height);
	  break;
      
      case 3:
	  deinterlace_yuv_zoom(ptr->video_buf, ptr->v_width, ptr->v_height);
	  break;
      
      case 5:
	yuv_deinterlace_linear_blend(ptr->video_buf, ptr->video_buf_Y[ptr->free], ptr->v_width, ptr->v_height);
	break;
      }
  }
  
  // no update for frame_list_t *ptr required
  
  /* ------------------------------------------------------------ 
   *
   * vertical resize of frame (up)
   *
   * ------------------------------------------------------------*/
  
  if(resize2 && vob->vert_resize2) {

    if(!yuv_vert_resize_init_flag) yuv_vert_resize_init(ptr->v_width);
    
    if(!vert_table_8_up_flag) {
      init_table_8_up(vert_table_8_up, ptr->v_height, vob->vert_resize2);
      vert_table_8_up_flag=1;
    }
    
    yuv_vresize_8_up(ptr->video_buf, ptr->video_buf_Y[ptr->free], ptr->v_width, ptr->v_height, vob->vert_resize2);
    
    ptr->video_buf = ptr->video_buf_Y[ptr->free];
    ptr->free = (ptr->free) ? 0:1;

    ptr->v_height += vob->vert_resize2<<3;    
    ptr->video_size = ptr->v_height *  ptr->v_width * ptr->v_bpp/8/2;

    clear_mmx();

  }
 
  /* ------------------------------------------------------------ 
   *
   * horizontal resize of frame (up)
   *
   * ------------------------------------------------------------*/

  if(resize2 && vob->hori_resize2) {

    if(!hori_table_8_up_flag) {
      init_table_8_up(hori_table_8_up, ptr->v_width, vob->hori_resize2);
      hori_table_8_up_flag=1;
    }
    
    yuv_hresize_8_up(ptr->video_buf, ptr->video_buf_Y[ptr->free], ptr->v_width, ptr->v_height, vob->hori_resize2);

    ptr->video_buf = ptr->video_buf_Y[ptr->free];
    ptr->free = (ptr->free) ? 0:1;

    ptr->v_width += vob->hori_resize2<<3;    
    ptr->video_size = ptr->v_height *  ptr->v_width * ptr->v_bpp/8/2;
  }


  /* ------------------------------------------------------------ 
   *
   * vertical resize of frame (down)
   *
   * ------------------------------------------------------------*/

  if(resize1 && vob->vert_resize1) {

    if(!yuv_vert_resize_init_flag) yuv_vert_resize_init(ptr->v_width);

    if(!vert_table_8_flag) {
      init_table_8(vert_table_8, ptr->v_height, vob->vert_resize1);
      vert_table_8_flag=1;
    }
    
    yuv_vresize_8(ptr->video_buf, ptr->v_width,  ptr->v_height, vob->vert_resize1);
    
    // update frame_list_t *ptr
    
    ptr->v_height -= vob->vert_resize1<<3;
    ptr->video_size = ptr->v_height *  ptr->v_width * ptr->v_bpp/8/2;

    clear_mmx();

  }
  

  /* ------------------------------------------------------------ 
   *
   * horizontal resize of frame (down)
   *
   * ------------------------------------------------------------*/
  
  if(resize1 && vob->hori_resize1) {
    
    if(!hori_table_8_flag) { 
      init_table_8(hori_table_8, ptr->v_width, vob->hori_resize1);
      hori_table_8_flag=1;
    }
    
    yuv_hresize_8(ptr->video_buf, ptr->v_width, ptr->v_height, vob->hori_resize1);
    
    // update frame_list_t *ptr
    
    ptr->v_width -= vob->hori_resize1<<3;
    ptr->video_size = ptr->v_height *  ptr->v_width * ptr->v_bpp/8/2;
  }


  /* ------------------------------------------------------------ 
   *
   * zoom video frame with filtering
   *
   * ------------------------------------------------------------*/
  
  if(zoom) {
    
    yuv_zoom(ptr->video_buf, ptr->video_buf_Y[ptr->free], ptr->v_width, ptr->v_height, vob->zoom_width, vob->zoom_height);
    
    // adjust pointer, zoomed frame in tmp buffer
    ptr->video_buf = ptr->video_buf_Y[ptr->free];
    ptr->free = (ptr->free) ? 0:1;
    
    // update frame_list_t *ptr
    
    ptr->v_width    = vob->zoom_width;
    ptr->v_height   = vob->zoom_height; 
    ptr->video_size = vob->zoom_width * vob->zoom_height * vob->v_bpp/8/2;

  }


  /* ------------------------------------------------------------ 
   *
   * post-processing: clip lines from top and bottom 
   *
   * ------------------------------------------------------------*/
  
  if(ex_clip && (vob->ex_clip_top || vob->ex_clip_bottom)) {

    check_clip_para(vob->ex_clip_top);
    check_clip_para(vob->ex_clip_bottom);    
    
      if(vob->ex_clip_top==vob->ex_clip_bottom) {
	  yuv_vclip(ptr->video_buf, ptr->v_width, ptr->v_height, vob->ex_clip_top);
      } else {
	  
	  yuv_clip_top_bottom(ptr->video_buf, ptr->video_buf_Y[ptr->free], ptr->v_width, ptr->v_height, vob->ex_clip_top, vob->ex_clip_bottom);

	  // adjust pointer, clipped frame in tmp buffer
	  ptr->video_buf = ptr->video_buf_Y[ptr->free];
	  ptr->free = (ptr->free) ? 0:1;
      }
      
      // update frame_list_t *ptr
      
      ptr->v_height -= (vob->ex_clip_top + vob->ex_clip_bottom);
      ptr->video_size = ptr->v_height *  ptr->v_width * ptr->v_bpp/8/2;
  }

  /* ------------------------------------------------------------ 
   *
   * post-processing: clip cols from left and right
   *
   * ------------------------------------------------------------*/

  if(ex_clip && (vob->ex_clip_left || vob->ex_clip_right)) {

    check_clip_para(vob->ex_clip_left);
    check_clip_para(vob->ex_clip_right);
    
      if(vob->ex_clip_left == vob->ex_clip_right) {
	  yuv_hclip(ptr->video_buf, ptr->v_width, ptr->v_height, vob->ex_clip_left);
      } else { 
	  
	  yuv_clip_left_right(ptr->video_buf, ptr->v_width, ptr->v_height, vob->ex_clip_left, vob->ex_clip_right);
	  
      }
      
    // update frame_list_t *ptr
    
    ptr->v_width -= (vob->ex_clip_left + vob->ex_clip_right);
    ptr->video_size = ptr->v_height *  ptr->v_width * ptr->v_bpp/8/2;
  }
 
  /* ------------------------------------------------------------ 
   *
   * rescale video frame
   *
   * ------------------------------------------------------------*/
  
  if(rescale) {
    
    yuv_rescale(ptr->video_buf, ptr->v_width, ptr->v_height, vob->reduce_h, vob->reduce_w);
  
    // update frame_list_t *ptr

    ptr->v_width    /= vob->reduce_w; 
    ptr->v_height   /= vob->reduce_h; 
    ptr->video_size = ptr->v_height *  ptr->v_width * ptr->v_bpp/8/2;

  }

  /* ------------------------------------------------------------ 
   *
   * flip picture upside down
   *
   * ------------------------------------------------------------*/
  
  if(flip) {
    
    yuv_flip(ptr->video_buf, ptr->v_width, ptr->v_height);   
    
    // no update for frame_list_t *ptr required
  }



  /* ------------------------------------------------------------ 
   *
   * mirror picture
   *
   * ------------------------------------------------------------*/
  
  if(mirror) {

      yuv_mirror(ptr->video_buf, ptr->v_width, ptr->v_height); 
      
      // no update for frame_list_t *ptr required
      
  }

  /* ------------------------------------------------------------ 
   *
   * swap Cr with Cb
   *
   * ------------------------------------------------------------*/
  
  if(rgbswap) {
    
    yuv_swap(ptr->video_buf, ptr->v_width, ptr->v_height);
  
    // no update for frame_list_t *ptr required
    
  }


  /* ------------------------------------------------------------ 
   *
   * b/w mode
   *
   * ------------------------------------------------------------*/
  
  if(decolor) {
    
    yuv_decolor(ptr->video_buf, ptr->v_width * ptr->v_height);
    
    // no update for frame_list_t *ptr required
      
  }


 /* ------------------------------------------------------------ 
   *
   * gamma correction
   *
   * ------------------------------------------------------------*/
  
  if(dgamma) {
      
      if(!gamma_table_flag) {
	  init_gamma_table(gamma_table, vob->gamma);
	  gamma_table_flag = 1;
      }

      yuv_gamma(ptr->video_buf, ptr->v_width * ptr->v_height);
      
      // no update for frame_list_t *ptr required
      
  }

  /* ------------------------------------------------------------ 
   *
   * antialias video frame
   *
   * ------------------------------------------------------------*/
  
  if(vob->antialias) {
    
    if(!aa_table_flag) init_aa_table(vob->aa_weight, vob->aa_bias);
    
    //UV components unchanged
    memcpy(ptr->video_buf_Y[ptr->free]+ptr->v_width*ptr->v_height, ptr->video_buf + ptr->v_width*ptr->v_height, ptr->v_width*ptr->v_height/2);
    
    yuv_antialias(ptr->video_buf, ptr->video_buf_Y[ptr->free], ptr->v_width, ptr->v_height, vob->antialias);
    
    // adjust pointer, zoomed frame in tmp buffer
    ptr->video_buf = ptr->video_buf_Y[ptr->free];
    ptr->free = (ptr->free) ? 0:1;
    
    // no update for frame_list_t *ptr required
  }   
    
  //done
  return(0);
}



int process_rgb_frame(vob_t *vob, vframe_list_t *ptr)
{


  /* ------------------------------------------------------------ 
   *
   * clip rows from top/bottom before processing frame
   *
   * ------------------------------------------------------------*/
  
  if(im_clip && (vob->im_clip_top || vob->im_clip_bottom)) {
    
    if(vob->im_clip_top==vob->im_clip_bottom) {
      rgb_vclip(ptr->video_buf, ptr->v_width, ptr->v_height, vob->im_clip_top);
    } else {
      
      rgb_clip_top_bottom(ptr->video_buf, ptr->video_buf_RGB[ptr->free], ptr->v_width, ptr->v_height, vob->im_clip_top, vob->im_clip_bottom);
      
      // adjust pointer, zoomed frame in tmp buffer
      ptr->video_buf = ptr->video_buf_RGB[ptr->free];
      ptr->free = (ptr->free) ? 0:1;
    }
    
    // update frame_list_t *ptr
    
    ptr->v_height -= (vob->im_clip_top + vob->im_clip_bottom);
    ptr->video_size = ptr->v_height *  ptr->v_width * ptr->v_bpp/8;
  }
  
  /* ------------------------------------------------------------ 
   *
   * clip coloums from left and right
   *
   * ------------------------------------------------------------*/
  
  if(im_clip && (vob->im_clip_left || vob->im_clip_right)) {
    
    if(vob->im_clip_left == vob->im_clip_right) {
      rgb_hclip(ptr->video_buf, ptr->v_width, ptr->v_height, vob->im_clip_left);
    } else {
      
      rgb_clip_left_right(ptr->video_buf, ptr->v_width, ptr->v_height, vob->im_clip_left, vob->im_clip_right);
    }	  
    
    // update frame_list_t *ptr
    
    ptr->v_width -= (vob->im_clip_left + vob->im_clip_right);
    ptr->video_size = ptr->v_height *  ptr->v_width * ptr->v_bpp/8;
  }
  
  /* ------------------------------------------------------------ 
   *
   * deinterlace video frame
   *
   * ------------------------------------------------------------*/
  
  switch (vob->deinterlace) {
    
  case 1:
    
    rgb_deinterlace_linear(ptr->video_buf, ptr->v_width, ptr->v_height);
    break;
    
  case 2:
    //handled by encoder
    break;
    
  case 3:
    deinterlace_rgb_zoom(ptr->video_buf, ptr->v_width, ptr->v_height);
    break;
    
   case 4:
     deinterlace_rgb_nozoom(ptr->video_buf, ptr->v_width, ptr->v_height);
     ptr->v_height /=2;
     break;
     
   case 5:
     rgb_deinterlace_linear_blend(ptr->video_buf, ptr->video_buf_RGB[ptr->free], ptr->v_width, ptr->v_height);
     break;
   }
   
   // check for de-interlace attribute
   
   
   if(ptr->attributes & TC_FRAME_IS_INTERLACED) {
     
     switch (ptr->deinter_flag) {
       
     case 1:
       rgb_deinterlace_linear(ptr->video_buf, ptr->v_width, ptr->v_height);
       break;
       
     case 3:
       deinterlace_rgb_zoom(ptr->video_buf, ptr->v_width, ptr->v_height);
       break;
       
     case 5:
       rgb_deinterlace_linear_blend(ptr->video_buf, ptr->video_buf_RGB[ptr->free], ptr->v_width, ptr->v_height);
       break;
     }
   }
   
   // no update for frame_list_t *ptr required
   
   /* ------------------------------------------------------------ 
    *
    * vertical resize of frame (up)
    *
    * ------------------------------------------------------------*/
   
   if(resize2 && vob->vert_resize2) {
     
     if(!rgb_vert_resize_init_flag) rgb_vert_resize_init();
     
     if(tmp_image == NULL) {
       
      if((tmp_image = (char *)calloc(1, SIZE_RGB_FRAME))==NULL) {
	fprintf(stderr, "(%s) out of memory", __FILE__);
	exit(1);
      }
    }
    
    if(!vert_table_8_up_flag) {
      init_table_8_up(vert_table_8_up, ptr->v_height, vob->vert_resize2);
      vert_table_8_up_flag=1;
    }
    
    rgb_vresize_8_up(ptr->video_buf, ptr->v_width, ptr->v_height, vob->vert_resize2);

    ptr->v_height += vob->vert_resize2<<3;    
    ptr->video_size = ptr->v_height *  ptr->v_width * ptr->v_bpp/8;

    clear_mmx();
  }
  
  /* ------------------------------------------------------------ 
   *
   * horizontal resize of frame (up)
   *
   * ------------------------------------------------------------*/

  if(resize2 && vob->hori_resize2) {

    if(tmp_image == NULL) {
      
      if((tmp_image = (char *)calloc(1, SIZE_RGB_FRAME))==NULL) {
	fprintf(stderr, "(%s) out of memory", __FILE__);
	exit(1);
      }
    }
    
    if(!hori_table_8_up_flag) {
      init_table_8_up(hori_table_8_up, ptr->v_width, vob->hori_resize2);
      hori_table_8_up_flag=1;
    }
    
    rgb_hresize_8_up(ptr->video_buf, ptr->v_width, ptr->v_height, vob->hori_resize2);

    ptr->v_width += vob->hori_resize2<<3;    
    ptr->video_size = ptr->v_height *  ptr->v_width * ptr->v_bpp/8;
    
  }

  /* ------------------------------------------------------------ 
   *
   * vertical resize of frame (down)
   *
   * ------------------------------------------------------------*/
  
  if(resize1 && vob->vert_resize1) {

    if(!rgb_vert_resize_init_flag) rgb_vert_resize_init();
    
    if(!vert_table_8_flag) {
      init_table_8(vert_table_8, ptr->v_height, vob->vert_resize1);
      vert_table_8_flag=1;
    }
    
    rgb_vresize_8(ptr->video_buf, ptr->v_width,  ptr->v_height, vob->vert_resize1);
    
    // update frame_list_t *ptr
    
    ptr->v_height -= vob->vert_resize1<<3;
    ptr->video_size = ptr->v_height *  ptr->v_width * ptr->v_bpp/8;

    clear_mmx();
    
  }

  /* ------------------------------------------------------------ 
   *
   * horizontal resize of frame (down)
   *
   * ------------------------------------------------------------*/
  
  if(resize1 && vob->hori_resize1) {
    
    if(!hori_table_8_flag) { 
      init_table_8(hori_table_8, ptr->v_width, vob->hori_resize1);
      hori_table_8_flag=1;
    }
    
    rgb_hresize_8(ptr->video_buf, ptr->v_width, ptr->v_height, vob->hori_resize1);
    
    // update frame_list_t *ptr
    
    ptr->v_width -= vob->hori_resize1<<3;
    ptr->video_size = ptr->v_height *  ptr->v_width * ptr->v_bpp/8;
  }
  
 
  /* ------------------------------------------------------------ 
   *
   * zoom video frame with filtering
   *
   * ------------------------------------------------------------*/

  if(zoom) {
    
    rgb_zoom(ptr->video_buf, ptr->v_width, ptr->v_height, vob->zoom_width, vob->zoom_height);
  
    // update frame_list_t *ptr

    ptr->v_width    = vob->zoom_width;
    ptr->v_height   = vob->zoom_height; 
    ptr->video_size = vob->zoom_width * vob->zoom_height * vob->v_bpp/8;

  }

  /* ------------------------------------------------------------ 
   *
   * post-processing: clip lines from top and bottom 
   *
   * ------------------------------------------------------------*/

  if(ex_clip && (vob->ex_clip_top || vob->ex_clip_bottom)) {
    

      if(vob->ex_clip_top==vob->ex_clip_bottom) {
	  rgb_vclip(ptr->video_buf, ptr->v_width, ptr->v_height, vob->ex_clip_top);
      } else {
	  
	rgb_clip_top_bottom(ptr->video_buf, ptr->video_buf_RGB[ptr->free], ptr->v_width, ptr->v_height, vob->ex_clip_top, vob->ex_clip_bottom);
	
	// adjust pointer, zoomed frame in tmp buffer
	ptr->video_buf = ptr->video_buf_RGB[ptr->free];
	ptr->free = (ptr->free) ? 0:1;
      }
      
      // update frame_list_t *ptr
      
      ptr->v_height -= (vob->ex_clip_top + vob->ex_clip_bottom);
      ptr->video_size = ptr->v_height *  ptr->v_width * ptr->v_bpp/8;
  }

  /* ------------------------------------------------------------ 
   *
   * post-processing: clip cols from left and right
   *
   * ------------------------------------------------------------*/

  if(ex_clip && (vob->ex_clip_left || vob->ex_clip_right)) {

      if(vob->ex_clip_left == vob->ex_clip_right) {
	  rgb_hclip(ptr->video_buf, ptr->v_width, ptr->v_height, vob->ex_clip_left);
      } else { 
	  
	  rgb_clip_left_right(ptr->video_buf, ptr->v_width, ptr->v_height, vob->ex_clip_left, vob->ex_clip_right);
	  
      }
      
      // update frame_list_t *ptr
    
      ptr->v_width -= (vob->ex_clip_left + vob->ex_clip_right);
      ptr->video_size = ptr->v_height *  ptr->v_width * ptr->v_bpp/8;
  }
  
  /* ------------------------------------------------------------ 
   *
   * rescale video frame
   *
   * ------------------------------------------------------------*/
  
  if(rescale) {
    
    rgb_rescale(ptr->video_buf, ptr->v_width, ptr->v_height, vob->reduce_h, vob->reduce_w);
  
    // update frame_list_t *ptr

    ptr->v_width    /= vob->reduce_w; 
    ptr->v_height   /= vob->reduce_h; 

    ptr->video_size  = ptr->v_height * ptr->v_width * ptr->v_bpp>>3;
  }


  /* ------------------------------------------------------------ 
   *
   * flip picture upside down
   *
   * ------------------------------------------------------------*/
  
  
  if(flip) {
    
    rgb_flip(ptr->video_buf, ptr->v_width, ptr->v_height);   
    
    // no update for frame_list_t *ptr required
  
  }

  /* ------------------------------------------------------------ 
   *
   * mirror picture
   *
   * ------------------------------------------------------------*/
  
  if(mirror) {

      rgb_mirror(ptr->video_buf, ptr->v_width, ptr->v_height); 
      
      // no update for frame_list_t *ptr required
  
  }


  /* ------------------------------------------------------------ 
   *
   * swap red with blue
   *
   * ------------------------------------------------------------*/
  
  if(rgbswap) {
    
    rgb_swap(ptr->video_buf, ptr->v_width * ptr->v_height);
    
    // no update for frame_list_t *ptr required
    
  }


  /* ------------------------------------------------------------ 
   *
   * b/w mode
   *
   * ------------------------------------------------------------*/
  
  if(decolor) {
      
      rgb_decolor(ptr->video_buf, ptr->v_width * ptr->v_height * ptr->v_bpp>>3);
      
      // no update for frame_list_t *ptr required
      
  }
  
 /* ------------------------------------------------------------ 
   *
   * gamma correction
   *
   * ------------------------------------------------------------*/
  
  if(dgamma) {
      
      if(!gamma_table_flag) {
	  init_gamma_table(gamma_table, vob->gamma);
	  gamma_table_flag = 1;
      }

      rgb_gamma(ptr->video_buf, ptr->v_width * ptr->v_height * ptr->v_bpp>>3);
      
      // no update for frame_list_t *ptr required
      
  }

  /* ------------------------------------------------------------ 
   *
   * antialias video frame
   *
   * ------------------------------------------------------------*/

  if(vob->antialias) {

    if(!aa_table_flag) init_aa_table(vob->aa_weight, vob->aa_bias);
    
    rgb_antialias(ptr->video_buf, ptr->video_buf_RGB[ptr->free], ptr->v_width, ptr->v_height, vob->antialias);
    
    // adjust pointer, zoomed frame in tmp buffer
    ptr->video_buf = ptr->video_buf_RGB[ptr->free];
    ptr->free = (ptr->free) ? 0:1;

    // no update for frame_list_t *ptr required
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


int process_vid_frame(vob_t *vob, vframe_list_t *ptr)
    
{

  // check for pass-through mode

  if(vob->pass_flag & TC_VIDEO) return(0);

  if (ptr->attributes & TC_FRAME_IS_SKIPPED)
      return 0;
  
  // check if a frame data are in RGB colorspace
  
  if(vob->im_v_codec == CODEC_RGB) {
      ptr->v_codec = CODEC_RGB;
      return(process_rgb_frame(vob, ptr));
  }

  // check if frame data are in YCrCb colorspace
  // only a limited number of transformations yet supported

  // as of 0.5.0, all frame operations are available for RGB and YUV
  
  if(vob->im_v_codec == CODEC_YUV) {
      ptr->v_codec = CODEC_YUV;
      return(process_yuv_frame(vob, ptr));
  }
  
  tc_error("Oops, invalid colorspace video frame data"); 
  
  return 0;
}

