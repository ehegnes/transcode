/*
 *  video_yuv.c
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

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "framebuffer.h"
#include "video_trans.h"
#include <string.h>
 
#include "zoom.h"
#include "aclib/ac.h"

#define BLACK_BYTE_Y 16
#define BLACK_BYTE_UV 128

/* ------------------------------------------------------------ 
 *
 * video frame transformation auxiliary routines
 *
 * ------------------------------------------------------------*/


// works -l (but doesn't do anything??)
void yuv422_mirror(char *image, int width, int height) 
{
  int x, y;
  char *in, *out;
  char u, v, y1, y2;
  int stride = 2*width;

  in  = image;
  out = image+(stride-4);

  //Y
  for (y = 0; y < height; ++y) {
      for (x = 0; x < stride/4; ++x) {

	  u  = in[0]; in[0] = out[0]; out[0] = u;
	  y1 = in[1]; in[1] = out[3]; out[3] = y1;
	  v  = in[2]; in[2] = out[2]; out[2] = v;
	  y2 = in[3]; in[3] = out[1]; out[1] = y2;

	in+=4;
	out-=4;
      }
      in = image + y*stride;
      out = image + y*stride + stride-4;
  }


}

static void yuv422_zoom_done(void)
{
  int id;
  
  //get thread id:
  
  id=get_fthread_id(0);
 
  zoom_image_done(tbuf[id].zoomer);
  tbuf[id].zoomer = NULL;
}

static void yuv422_zoom_init(char *image, char *tmp_buf, int width, int height, int new_width, int new_height)
{

  int id;

  vob_t *vob;
  
  //get thread id:
  
  id=get_fthread_id(0);
  
  tbuf[id].tmpBuffer = (pixel_t*)tmp_buf;

    // U
    zoom_setup_image(&tbuf[id].srcImageY, width/2, height, 4, image);
    // Y1 and Y2
    zoom_setup_image(&tbuf[id].srcImage, width, height, 2, image+1);
    // V
    zoom_setup_image(&tbuf[id].srcImageUV, width/2, height, 4, image+2);

    zoom_setup_image(&tbuf[id].dstImageY, new_width/2, new_height, 4, tbuf[id].tmpBuffer);
    zoom_setup_image(&tbuf[id].dstImage,  new_width, new_height, 2, tbuf[id].tmpBuffer+1);
    zoom_setup_image(&tbuf[id].dstImageUV, new_width/2, new_height, 4, tbuf[id].tmpBuffer+2);

    vob = tc_get_vob();

    tbuf[id].zoomer = zoom_image_init(&tbuf[id].dstImage, &tbuf[id].srcImage, vob->zoom_filter, vob->zoom_support);
    tbuf[id].zoomerY = zoom_image_init(&tbuf[id].dstImageY, &tbuf[id].srcImageY, vob->zoom_filter, vob->zoom_support);
    tbuf[id].zoomerUV = zoom_image_init(&tbuf[id].dstImageUV, &tbuf[id].srcImageUV, vob->zoom_filter, vob->zoom_support);
    
    atexit(yuv422_zoom_done);
}


// works! -Z
void yuv422_zoom(char *image, char *_tmp_buf, int width, int height, int new_width, int new_height)
{

  int id;
  char *tmp_buf=_tmp_buf;
  
  //get thread id:
  
  id=get_fthread_id(0);
  
  
  if (tbuf[id].zoomer == NULL)
    yuv422_zoom_init(image, tmp_buf, width, height, new_width, new_height);
  
  // Y1 and Y2
  tbuf[id].srcImage.data = image+1;
  tbuf[id].dstImage.data = tmp_buf+1;
  zoom_image_process(tbuf[id].zoomer);
  
  // U
  tbuf[id].srcImageY.data = image;
  tbuf[id].dstImageY.data = tmp_buf;
  zoom_image_process(tbuf[id].zoomerY);

  // V
  tbuf[id].srcImageUV.data = image+2;
  tbuf[id].dstImageUV.data = tmp_buf+2;
  zoom_image_process(tbuf[id].zoomerUV);
}
  

static void yuv422_zoom_done_DI(void)
{
  int id;
  
  //get thread id:
  
  id=get_fthread_id(1);
 
  if (tbuf_DI[id].zoomer)
      zoom_image_done(tbuf_DI[id].zoomer);
  tbuf_DI[id].zoomer = NULL;
  if (tbuf_DI[id].zoomerY)
      zoom_image_done(tbuf_DI[id].zoomerY);
  tbuf_DI[id].zoomerY = NULL;
  if (tbuf_DI[id].zoomerUV)
      zoom_image_done(tbuf_DI[id].zoomerUV);
  tbuf_DI[id].zoomerUV = NULL;
  if (tbuf_DI[id].tmpBuffer)
      free(tbuf_DI[id].tmpBuffer);
  tbuf_DI[id].tmpBuffer = NULL;
}

static void yuv422_zoom_init_DI(char *image, int width, int height, int new_width, int new_height, int id)
{

  vob_t *vob;

  tbuf_DI[id].tmpBuffer = (pixel_t*)malloc(new_width*new_height*2);
  
  // U
  zoom_setup_image(&tbuf[id].srcImageY, width/2, height, 4, image);
  // Y1 and Y2
  zoom_setup_image(&tbuf[id].srcImage, width, height, 2, image+1);
  // V
  zoom_setup_image(&tbuf[id].srcImageUV, width/2, height, 4, image+2);

  zoom_setup_image(&tbuf[id].dstImageY, new_width/2, new_height, 4, tbuf[id].tmpBuffer);
  zoom_setup_image(&tbuf[id].dstImage,  new_width, new_height, 2, tbuf[id].tmpBuffer+1);
  zoom_setup_image(&tbuf[id].dstImageUV, new_width/2, new_height, 4, tbuf[id].tmpBuffer+2);
  
  vob = tc_get_vob();

  tbuf[id].zoomer = zoom_image_init(&tbuf[id].dstImage, &tbuf[id].srcImage, vob->zoom_filter, vob->zoom_support);
  tbuf[id].zoomerY = zoom_image_init(&tbuf[id].dstImageY, &tbuf[id].srcImageY, vob->zoom_filter, vob->zoom_support);
  tbuf[id].zoomerUV = zoom_image_init(&tbuf[id].dstImageUV, &tbuf[id].srcImageUV, vob->zoom_filter, vob->zoom_support);

  atexit(yuv422_zoom_done_DI);
}


void yuv422_zoom_DI(char *image, int width, int height, int new_width, int new_height)
{

  int id;
  
  //get thread id:
  
  id=get_fthread_id(1);
  
  
  if (tbuf_DI[id].zoomer == NULL)
    yuv422_zoom_init_DI(image, width, height, new_width, new_height, id);
  
  
  // Y1 and Y2
  tbuf[id].srcImage.data = image+1;
  tbuf[id].dstImage.data = tbuf_DI[id].tmpBuffer+1;
  zoom_image_process(tbuf[id].zoomer);
  
  // U
  tbuf[id].srcImageY.data = image;
  tbuf[id].dstImageY.data = tbuf_DI[id].tmpBuffer;
  zoom_image_process(tbuf[id].zoomerY);

  // V
  tbuf[id].srcImageUV.data = image+2;
  tbuf[id].dstImageUV.data = tbuf_DI[id].tmpBuffer+2;
  zoom_image_process(tbuf[id].zoomerUV);
  memcpy(image, tbuf_DI[id].tmpBuffer, new_width*new_height*2);
}

// works -I3
void deinterlace_yuv422_zoom(unsigned char *src, int width, int height)
{

    char *in, *out;

    int i, block;

    // move first field into first half of frame buffer 

    // Y
    block = width*2;

    in  = src;
    out = src;

    //move every second row
    for (i=0; i<height; i=i+2) {
	
      memcpy(out, in, block);
      in  += 2*block;
      out += block;
    }

    //high quality zoom out
    yuv422_zoom_DI(src, width, height/2, width, height);
}


// -G works
void yuv422_gamma(char *image, int len)
{
    int n;
    unsigned char *c;

    c = (unsigned char*) image+1;

    for(n=0; n<=len/2; ++n) {
      *c = gamma_table[*c];
      c+=2;
    }

    return;
}

// works
void yuv422_rescale_core(char *image, int width, int height, int reduce_h, int reduce_w)
{
  
  char *in, *out;
  
  unsigned int x, y; 
  
  unsigned int n_width, n_height;
  
  /* resize output video 
   */
  
  n_width = width / reduce_w;
  n_height = height / reduce_h;
  
  in  = image;
  out = image;

  for (y = 0; y < n_height; y++)
    {
      for (x = 0; x < n_width/4; x++)
	{
	    memcpy(out, in, 4);
	  
	    out = out + 4; 
	    in  = in + reduce_w*4;
	}
      in += width * (reduce_h - 1);
    }
}

// -r works
void yuv422_rescale(char *image, int width, int height, int resize_h, int resize_w)
{

    // all
    yuv422_rescale_core(image, width*2, height, resize_h, resize_w);
    
    return;
}


// -z works
void yuv422_flip(char *image, int width, int height)
{

  char *in, *out;

  // this shouldn't be too hard on the stack
  char rowbuffer[TC_MAX_V_FRAME_WIDTH];
  
  unsigned int y, block;  
  
  block = width*2;
  
  in  = image + block*(height-1);
  out = image;
  
  for (y = height; y > height/2; y--) {

    memcpy(rowbuffer, out, block);
    memcpy(out, in, block);
    memcpy(in, rowbuffer, block);
    
    out = out + block; 
    in  = in - block;
  }
}

// works -j 0, +-X
void yuv422_hclip(char *image, int width, int height, int cols)
{

  char *in, *out;
  
  unsigned int y, block, offset, rowbytes; 
  unsigned int pixeltoins, size;

  if (!cols) // no clipping
      return;

  if (cols>0) {

    rowbytes = width*2;
    offset   = cols*2;
    block    = rowbytes - 2*offset; 

    // Y

    in   =  image + offset;
    out  =  image;
  
    for (y = 0; y < height; y++) {
      
	memcpy(out, in, block);

	// advance to next row

	in  += rowbytes;
	out += block;
    }

    return;
  }
  
  /* cols < 0 */

  cols = -cols;
  rowbytes = width*2;
  offset   = cols*2;
  block    = rowbytes + 2*offset; 

  pixeltoins = 2*offset*height;
  size     = 2*width*height;
  memmove (image + pixeltoins, image, size);
  
  // Y

  in   =  image + pixeltoins;
  out  =  image + offset;
  
  for (y = 0; y < height; y++) {
      int x;
      unsigned char *t;
      
      t = (unsigned char *)out-offset;
      for (x=0; x<offset; x++) {
	  *t++ = (x&1)?BLACK_BYTE_Y:BLACK_BYTE_UV;
      }

      memmove (out, in, rowbytes);

      t = (unsigned char *)out+rowbytes;
      for (x=0; x<offset; x++) {
	  *t++ = (x&1)?BLACK_BYTE_Y:BLACK_BYTE_UV;
      }
      
      // advance to next row
      
      in  += rowbytes;
      out += block;
  }

  return;

}

// works -j 0,+-X,0,+-Y
void yuv422_clip_left_right(char *image, int width, int height, int cols_left, int cols_right)
{
  char *in, *out;
  
  unsigned int y, block, offset, rowbytes; 
  unsigned int pixeltoins, offset_left, offset_right;

  if (cols_left >= 0 && cols_right >= 0) {

    rowbytes = width * 2;
    offset   = cols_left * 2;
    block    = rowbytes - (cols_left + cols_right)*2;


    in   =  image + offset;
    out  =  image;

    for (y = 0; y < height; y++) {

        memcpy(out, in, block);

        // advance to next row

        in  += rowbytes;
        out += block;
    }
    return;
  }

  /* insert black left and right */
  
  else if (cols_left < 0 && cols_right < 0) {

    cols_left    = -cols_left;
    cols_right   = -cols_right;
    offset_left  = cols_left * 2;
    offset_right = cols_right * 2;

    rowbytes = width * 2;
    pixeltoins = (offset_left+offset_right)*height;

    /* make space at start of image */
    memmove (image + pixeltoins, image, width*height*2);

    in = image + pixeltoins;
    out = image;

    for (y = 0; y < height ; y++) {

      int x;
      unsigned char *t;

      /* black out beginning of line */
      t = (unsigned char *)out;
      for (x=0; x<offset_left; x++) {
	  *t++ = (x&1)?BLACK_BYTE_Y:BLACK_BYTE_UV;
      }

      /* move line */
      memmove (out+offset_left, in, rowbytes);

      /* advance */
      out = out + (rowbytes + offset_left+offset_right);
      in += rowbytes;

      /* black out end of line */
      t = (unsigned char *)out-offset_right;
      for (x=0; x<offset_right; x++) {
	  *t++ = (x&1)?BLACK_BYTE_Y:BLACK_BYTE_UV;
      }
    }

    return;
  }

  /* insert black left and clip right */

  else if (cols_left < 0 && cols_right >= 0) {

    cols_left    = -cols_left;
    cols_right   = cols_right;
    offset_left  = cols_left * 2;
    offset_right = cols_right * 2;

    rowbytes = width * 2 - offset_right;
    pixeltoins = (offset_left)*height;

    /* make space at start of image */
    memmove (image + pixeltoins, image, width*height*2);

    in = image + pixeltoins;
    out = image;

    for (y = 0; y < height ; y++) {
      int x;
      unsigned char *t;

      /* black out beginning of line */
      t = (unsigned char *)out;
      for (x=0; x<offset_left; x++) {
	  *t++ = (x&1)?BLACK_BYTE_Y:BLACK_BYTE_UV;
      }

      /* move line */
      memmove (out+offset_left, in, rowbytes);

      /* advance */
      out += (rowbytes + offset_left);
      in  += (rowbytes + offset_right);

    }

    return;
  }

  /* clip left and insert black right */

  else if (cols_left >= 0 && cols_right < 0) {

    cols_left    = cols_left;
    cols_right   = -cols_right;
    offset_left  = cols_left * 2;
    offset_right = cols_right * 2;

    rowbytes = width * 2 - offset_left;
    pixeltoins = (offset_right)*height;

    /* make space at start of image */
    memmove (image + pixeltoins, image, width*height*2);

    in = image + pixeltoins;
    out = image;

    for (y = 0; y < height ; y++) {
      int x;
      unsigned char *t;

      /* move line */
      memmove (out, in+offset_left, rowbytes);

      /* black out end of line */
      t = (unsigned char *)out+rowbytes;
      for (x=0; x<offset_right; x++) {
	  *t++ = (x&1)?BLACK_BYTE_Y:BLACK_BYTE_UV;
      }

      /* advance */
      out += (rowbytes + offset_right);
      in  += (rowbytes + offset_left);

    }

    return;
  }

}


// works -j +-X, 0
void yuv422_vclip(char *image, int width, int height, int lines)
{
  char *in, *out;
  unsigned char *t;
  
  unsigned int y, block, bar, x;

  block = width * 2;    
  printf("here\n");

  if(lines>0) {
  
      in   =  image + block * lines;
      out  =  image;
      
      for (y = 0; y < height - 2*lines ; y++) {
	  
	  memcpy(out, in, block);
	  
	  in  += block;
	  out += block;
      }
      
      return;
  }
  
  // shift frame and generate black bars at top and bottom
  
  bar = - lines * block; //>0
  
  memmove(image + bar, image, width*height*2); 
  t = (unsigned char *)image;

  /* clear top */
  for (x=0; x<bar; x++) {
      *t++ = (x&1)?BLACK_BYTE_Y:BLACK_BYTE_UV;
  }

  /* clear bottom */
  t = (unsigned char *)image + width*height*2 + bar;
  for (x=0; x<bar; x++) {
      *t++ = (x&1)?BLACK_BYTE_Y:BLACK_BYTE_UV;
  }
  
  
}


// works -j +-X,0,+-Y,0
void yuv422_clip_top_bottom(char *image, char *dest, int width, int height, int _lines_top, int _lines_bottom)
{
  char *in=NULL, *out=NULL, *offset, *next;
  
  unsigned int block, x; 
  unsigned char *t;
  
  int bytes=0;
  
  int lines_top, lines_bottom;

  block = width * 2;

  lines_top    = _lines_bottom;
  lines_bottom = _lines_top;

  offset = image;
  next=dest;

  if(lines_top>=0 && lines_bottom>=0) { 
    in    = offset + block * lines_top;
    out   = next;
    bytes = block*(height - (lines_bottom+lines_top));
    next += bytes;
  }

  if(lines_top<0 && lines_bottom>=0) { 
    in    = offset;
    bytes = block*(height - lines_bottom);
    out   = next - lines_top*block;
    t = (unsigned char *)next;
    for (x=0; x< -lines_top*block; x++) {
	*t++ = (x&1)?BLACK_BYTE_Y:BLACK_BYTE_UV;
    }
    next += block*(height - (lines_bottom+lines_top));
  }

  if(lines_top>=0 && lines_bottom<0) { 
    in    = offset + block * lines_top;
    bytes = block*(height - lines_top);
    t = (unsigned char *)next+bytes;
    for (x=0; x< -lines_bottom*block; x++) {
	*t++ = (x&1)?BLACK_BYTE_Y:BLACK_BYTE_UV;
    }
    out   = next;
    next += (bytes - block*lines_bottom);
  }

  if(lines_top<0 && lines_bottom<0) { 
    in    = offset;
    bytes = block * height;
    t = (unsigned char *)next;
    for (x=0; x< -lines_top*block; x++) {
	*t++ = (x&1)?BLACK_BYTE_Y:BLACK_BYTE_UV;
    }
    t = (unsigned char *)next+bytes;
    for (x=0; x< -lines_bottom*block; x++) {
	*t++ = (x&1)?BLACK_BYTE_Y:BLACK_BYTE_UV;
    }
    out   = next - lines_top*block;
    next += (bytes - block*(lines_top + lines_bottom)); 
  }

  //transfer
  memcpy(out, in, bytes);
 
}


// works (rgbswap) -k
void yuv422_swap(char *image, int width, int height)
{
  int i;

  for (i=0; i<2*width*height; i+=4) {
    char tt    = image[i];
    image[i]   = image[i+2];
    image[i+2] = tt;
  }

}

// works -K
void yuv422_decolor(char *image, int offset)
{
  int x;

  for (x=0; x<offset; x++) {
    *image = BLACK_BYTE_UV;
    image += 2;
  }

}

inline int yuv422_merge_C(char *row1, char *row2, char *out, int bytes, 
			unsigned long weight1, unsigned long weight2)
{
  
  //blend each color entry in two arrays and return
  //result in char *out
    
    unsigned int y;
    unsigned long tmp;
    register unsigned long w1 = weight1;
    register unsigned long w2 = weight2;

    for (y = 0; y<bytes; ++y) {
      tmp = w2 * (unsigned char) row2[y] + w1 *(unsigned char) row1[y];
      out[y] = (tmp>>16) & 0xff;
    }

    return(0);
}

inline int yuv422_average_C(char *row1, char *row2, char *out, int bytes)
{
  
  //calculate the average of each color entry in two arrays and return
  //result in char *out
  
  unsigned int y=0;
  unsigned short tmp;
  
  for (y = 0; y<bytes; ++y) {
    tmp = ((unsigned char) row2[y] + (unsigned char) row1[y])>>1;
    out[y] = tmp & 0xff;
  }
  
  return(0);
}

static int (*yuv422_average) (char *row1, char *row2, char *out, int bytes);
static int (*memcpy_accel) (char *dest, char *source, int bytes);

inline static int memcpy_C(char *dest, char *source, int bytes)
{
  memcpy(dest, source, bytes);
  return(0);
}


// works
void yuv422_vresize_8(char *image, int width, int height, int resize)
{
  
  char *in, *out;
  
  unsigned int i, j=0, row_bytes, chunk, rows, n_height, m; 
    
  // resize output video 
  
  row_bytes = width * 2;
  
  // new height
  n_height = height - (resize<<3);
  
  //number of rows in new chunk
  rows =  n_height >>3 ;
  
  // chunk size
  chunk = (height >>3) * row_bytes;

  // dest row index
  m=0;
  
  for(j = 0; j < 8 ; ++j) {
    
    for (i = 0; i < rows; i++) {
      
      in = image + j*chunk +  vert_table_8[i].source * row_bytes;
      out = image + m * row_bytes;
      
      yuv422_merge_C(in, in+row_bytes, out, row_bytes, vert_table_8[i].weight1, vert_table_8[i].weight2);
      ++m;
    }
  }      

  return;
}

// works
void yuv422_vresize_8_up(char *image, int width, int height, int resize)
{
  
  char *in, *out, *last_row;
  
  unsigned int i, j=0, row_bytes, chunk, rows, n_height, m; 
    
  // resize output video 
  
  row_bytes = width * 2;
  
  // make sure we don't walk past the end

  last_row = image + (height-1)*row_bytes;

  // new height
  n_height = height + (resize<<3);
  
  //number of rows in new chunk
  rows =  n_height >>3 ;
  
  // chunk size
  chunk = (height >>3) * row_bytes;

  // dest row index
  m=0;
  
  for(j = 0; j < 7 ; ++j) {
    
    for (i = 0; i < rows; i++) {
      
      in = image + j * chunk +  vert_table_8_up[i].source * row_bytes;
      out = tmp_image + m * row_bytes;
      
      yuv422_merge_C(in, in+row_bytes, out, row_bytes, vert_table_8_up[i].weight1, vert_table_8_up[i].weight2);

      ++m;
    }
  }      
  for (i = 0; i < rows; i++) {
  
    in = image + j * chunk +  vert_table_8_up[i].source * row_bytes;
    out = tmp_image + m * row_bytes;

    if (in >= last_row){
      memcpy(out,in,row_bytes);
    } else {
      yuv422_merge_C(in, in+row_bytes, out, row_bytes, vert_table_8_up[i].weight1,
      		vert_table_8_up[i].weight2);
    }
    ++m;
  }

  memcpy(image, tmp_image, n_height*width*2);
  return;
}


// works
void yuv422_hresize_8(char *image, int width, int height, int resize)
{
    
    char *in, *out;
    
    unsigned int m, cols, i, j, pixels, n_width, blocks; 
    
    // resize output video 
    
    // process linear frame buffer;
    
    n_width = width - (resize<<3);
    pixels  = n_width * height;
    
    blocks = width>>3;
    
    cols = n_width >>3;
    
    // treat as linear buffer of arrays with cols pixels
    
    // pixel index
    m=0;
    
    for(j = 0; j < 8 * height; ++j) {
      
      for (i = 0; i < cols; i++) {
	
	in  = image + j * blocks * 4 + hori_table_8[i].source * 4;
	out = image + m;
	
	yuv422_merge_C(in, in+4, out, 4, hori_table_8[i].weight1, hori_table_8[i].weight2);

	m+=4;
      }
    }      
    
    return;
}


//works
void yuv422_hresize_8_up(char *image, int width, int height, int resize)
{
    
    char *in, *out;
    
    unsigned int m, cols, i, j, pixels, n_width, blocks, incr; 
    
    // resize output video 
    
    // process linear frame buffer;
    
    n_width = width + (resize<<3);
    pixels  = n_width * height;
    
    blocks = width>>3;
    
    cols = n_width >>3;
    
    // treat as linear buffer of arrays with cols pixels
    
    // pixel index
    m=0;
    
    for(j = 0; j < 8 * height; ++j) {
      
      for (i = 0; i < cols; i++) {
	
	incr = j * blocks + hori_table_8_up[i].source ;
	in  = image + incr *4;
	out = tmp_image + m;
	
	(!((incr+1)%width)) ? 
	    (void)memcpy(out, in, 4) : 
	    yuv422_merge_C(in, in+4, out, 4, 
		    hori_table_8_up[i].weight1, hori_table_8_up[i].weight2);

	m+=4;
      }
    }      
    
    memcpy(image, tmp_image, height*n_width*2);    
    return;
}

inline void yuv422_deinterlace_core(char *image, int width, int height)
{
    char *in, *out;
    
    unsigned int y, block; 
    
    block = width * 2;

    in  = image;
    out = image;
    
    // convert half frame to full frame by simple interpolation
      
    out +=block;
    
    for (y = 0; y < (height>>1)-1; y++) {
      
      yuv422_average(in, in+(block<<1), out, block);
      
      in  += block<<1;
      out += block<<1;
    }
    
    return;
}

inline void yuv422_deinterlace_linear_blend_core(char *image, char *tmp, int width, int height)
{
  char *in, *out;
  
  unsigned int y, block; 
  
  block = 2*width;
  
  //(1)
  //copy frame to 2. internal frame buffer

  memcpy_accel(tmp, image, block*height);

  //(2)
  //convert first field to full frame by simple interpolation
  //row(1)=(row(0)+row(2))/2
  //row(3)=(row(2)+row(4))/2
  //...


  in  = image;
  out = image;
  
  out +=block;
  
  for (y = 0; y < (height>>1)-1; y++) {
    
    yuv422_average(in, in+(block<<1), out, block);
    
    in  += block<<1;
    out += block<<1;
  }

  //(3)
  //convert second field to full frame by simple interpolation
  //row(2)=(row(1)+row(3))/2
  //row(4)=(row(3)+row(5))/2
  //...

  in  = tmp+block;
  out = tmp;
  
  out +=block<<1;
  
  for (y = 0; y < (height>>1)-1; y++) {
    
    yuv422_average(in, in+(block<<1), out, block);
    
    in  += block<<1;
    out += block<<1;
  }
  
  //(4)
  //blend both frames
  yuv422_average(image, tmp, image, block*height);
  
  return;
}


// works
void yuv422_deinterlace_linear(char *image, int width, int height)
{

  //default ia32 C mode:
  yuv422_average =  yuv422_average_C;
  
#if 0
#ifdef ARCH_X86 
#ifdef HAVE_ASM_NASM
  
  if(tc_accel & MM_MMX)  yuv422_average = ac_average_mmx;
  if(tc_accel & MM_SSE)  yuv422_average = ac_average_sse;
  if(tc_accel & MM_SSE2) yuv422_average = ac_average_sse2;
  
#endif
#endif
#endif

  yuv422_deinterlace_core(image, width, height);
  
  clear_mmx();
}

// works
void yuv422_deinterlace_linear_blend(char *image, char *tmp, int width, int height)
{

  //default ia32 C mode:
  yuv422_average =  yuv422_average_C;
  memcpy_accel = memcpy_C;
  
#if 0
#ifdef ARCH_X86 
#ifdef HAVE_ASM_NASM

  if(tc_accel & MM_MMX) {
    yuv422_average = ac_average_mmx;  
    memcpy_accel = ac_memcpy_mmx;
  }

  if(tc_accel & MM_SSE) {
    yuv422_average = ac_average_sse;
    memcpy_accel = ac_memcpy_sse;
  }

  if(tc_accel & MM_SSE2) {
    yuv422_average = ac_average_sse2;
    memcpy_accel = ac_memcpy_sse2;
  }

  
#endif
#endif
#endif // 0
  
  //process only Y component
  yuv422_deinterlace_linear_blend_core(image, tmp, width, height);

  clear_mmx();
}


