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


void yuv_rescale_core(char *image, int width, int height, int reduce_h, int reduce_w)
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
      for (x = 0; x < n_width; x++)
	{
	    *out=*in;
	  
	    out = out + 1; 
	    in  = in + reduce_w;
	}
      in  = in + width * (reduce_h - 1);
    }
}

void yuv_rescale(char *image, int width, int height, int resize_h, int resize_w)
{

    int n_height, n_width, bytes;
    
    // Y 
    yuv_rescale_core(image, width, height, resize_h, resize_w);
    
    // Cr
    yuv_rescale_core(image + width*height, width/2, height/2, resize_h, resize_w);
    
    // Cb
    yuv_rescale_core(image + width*height + ((width*height)>>2), 
		     width/2, height/2, resize_h, resize_w);

    // shift fields:

    n_height = height/resize_h;
    n_width = width/resize_w;
    
    bytes = (n_height*n_width)>>2;

    // Cr
    memcpy(image + n_height * n_width, image + width * height, bytes);
    // Cb
    memcpy(image + n_height * n_width + bytes, image + width*height + ((width*height)>>2), bytes);

    return;
}


void yuv_flip_core(char *image, int width, int height)
{

  char *in, *out;
  
  unsigned int y, block;  
  
  block = width;
  
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

void yuv_flip(char *image, int width, int height)
{
    
    int block = (width*height)>>2;

    yuv_flip_core(image, width, height);
    yuv_flip_core(image + 4*block, width/2, height/2);
    yuv_flip_core(image + 5*block, width/2, height/2);
}

void yuv_hclip(char *image, int width, int height, int cols)
{

  char *in, *out;
  
  unsigned int y, block, offset, new, rowbytes; 
  unsigned int pixeltoins, size;

  if (cols>=0) {

    rowbytes = width;
    offset   = cols;
    block    = rowbytes - 2*cols; 

    // Y

    in   =  image + offset;
    out  =  image;
  
    for (y = 0; y < height; y++) {
      
	memcpy(out, in, block);

	// advance to next row

	in  += rowbytes;
	out += block;
    }


    // Cr

    new = (width-2*cols)*height;

    rowbytes = width/2;
    offset   = cols/2;
    block    = rowbytes - cols; 
  
    in   =  image + width*height + offset;
    out  =  image + new;

    for (y = 0; y < height/2; y++) {
      
	memcpy(out, in, block);

	// advance to next row

	in  += rowbytes;
	out += block;
    }


    // Cb

    new += (width/2-cols)*height/2;

    rowbytes = width/2;
    offset   = cols/2;
    block    = rowbytes - cols; 
  
    in   =  image + width*height + ((width*height)>>2) + offset;
    out  =  image + new;
  
    for (y = 0; y < height/2; y++) {
      
	memcpy(out, in, block);

	// advance to next row

	in  += rowbytes;
	out += block;
    }

    return;
  }
  
  /* cols < 0 */

  cols = -cols;
  rowbytes = width;
  offset   = cols;
  block    = rowbytes + 2*offset; 

  pixeltoins = 2*offset*height + cols*height;
  size     = width*height + width*height/2;
  memmove (image + pixeltoins, image, size);
  
  // Y

  in   =  image + pixeltoins;
  out  =  image + cols;
  
  for (y = 0; y < height; y++) {
      
      memset (out-cols, BLACK_BYTE_Y, cols);
      memmove (out, in, rowbytes);
      memset (out+rowbytes, BLACK_BYTE_Y, cols);
      
      // advance to next row
      
      in  += rowbytes;
      out += block;
  }

  // Cr

  new = (width+2*cols)*height;
  rowbytes = width/2;
  offset   = cols/2;
  block    = rowbytes + cols; 
  
  out  =  image + new + offset;
  in   =  image + pixeltoins + width*height;
  
  for (y = 0; y < height/2; y++) {
      
      memset(out-offset, BLACK_BYTE_UV, offset);
      memmove(out, in, rowbytes);
      memset(out+rowbytes, BLACK_BYTE_UV, offset);
      
      // advance to next row
      
      in  += rowbytes;
      out += block;
  }
  
  // Cb

  new += (width/2 + cols)*height/2;
  in  = image + pixeltoins + width*height + width*height/4;
  out = image + new + offset;

  for (y = 0; y < height/2; y++) {
      
      memset(out-offset, BLACK_BYTE_UV, offset);
      memmove(out, in, rowbytes);
      memset(out+rowbytes,BLACK_BYTE_UV , offset);
      
      // advance to next row
      
      in  += rowbytes;
      out += block;
  }
}

void yuv_clip_left_right(char *image, int width, int height, int cols_left, int cols_right)
{

  char *in, *out;
  
  unsigned int y, block, offset, new, rowbytes; 
  unsigned int pixeltoins, offset_left, offset_right;

  /* clip left and right */
  
  if (cols_left >=0 && cols_right >= 0) {

    // Y

    rowbytes = width;
    offset   = cols_left;
    block    = rowbytes - (cols_right+cols_left); 
  
    in   =  image + offset;
    out  =  image;
  
    for (y = 0; y < height; y++) {
      
	memcpy(out, in, block);
      
	// advance to next row
      
	in  += rowbytes;
	out += block;
    }


    // Cr

    new = (width-(cols_right+cols_left))*height;

    rowbytes = width/2;
    offset   = cols_left/2;
    block    = rowbytes - (cols_right+cols_left)/2; 
  
    in   =  image + width*height + offset;
    out  =  image + new;
  
    for (y = 0; y < height/2; y++) {
      
	memcpy(out, in, block);
      
	// advance to next row
      
	in  += rowbytes;
	out += block;
    }


    // Cb

    new += (width/2-(cols_right+cols_left)/2)*height/2;

    rowbytes = width/2;
    offset   = cols_left/2;
    block    = rowbytes - (cols_right+cols_left)/2; 
  
    in   =  image + width*height + ((width*height)>>2) + offset;
    out  =  image + new;
  
    for (y = 0; y < height/2; y++) {
      
	memcpy(out, in, block);
      
	// advance to next row
      
	in  += rowbytes;
	out += block;
    }
    return;
  }

  /* add black bars left and right */

  else if (cols_left < 0 && cols_right < 0) {
    
    cols_left    = -cols_left;
    cols_right   = -cols_right;
    rowbytes     = width;
    offset_left  = cols_left;
    offset_right = cols_right;
    block        = rowbytes + (offset_left+offset_right); 

    pixeltoins = (offset_left+offset_right)*height + (cols_left+cols_right)*height/2;
  
    /* make room */
    memmove (image + pixeltoins, image, width*height + width*height/2);
  
    // Y

    in   =  image + pixeltoins;
    out  =  image + cols_left;
  
    for (y = 0; y < height; y++) {
      
        memset (out-cols_left, BLACK_BYTE_Y, cols_left);
	memmove (out, in, rowbytes);
	memset (out+rowbytes, BLACK_BYTE_Y, cols_right);
      
	// advance to next row
      
	in  += rowbytes;
	out += block;
    }

    // Cr

    new = (width+cols_left+cols_right)*height;
    rowbytes = width/2;
    offset_left  = cols_left/2;
    offset_right = cols_right/2;
    block    = rowbytes + (cols_left+cols_right)/2; 
  
    out  =  image + new + offset_left;
    in   =  image + pixeltoins + width*height;
  
    for (y = 0; y < height/2; y++) {
      
        memset(out-offset_left, BLACK_BYTE_UV, offset_left);
	memmove(out, in, rowbytes);
	memset(out+rowbytes, BLACK_BYTE_UV, offset_right);
      
	// advance to next row
      
	in  += rowbytes;
	out += block;
    }
  
    // Cb

    /* read: new += (width/2 + (cols_left+cols_right)/2)*height/2; */
    new += (width + cols_left+cols_right)*height/4;
    in  = image + pixeltoins + width*height + width*height/4;
    out = image + new + offset_left;

    for (y = 0; y < height/2; y++) {
      
        memset(out-offset_left, BLACK_BYTE_UV, offset_left);
	memmove(out, in, rowbytes);
	memset(out+rowbytes, BLACK_BYTE_UV, offset_right);
      
	// advance to next row
      
	in  += rowbytes;
	out += block;
    }
    return;
  }

  /* insert black left and clip right */

  else if (cols_left < 0 && cols_right >= 0) {

    cols_left    = -cols_left;
    cols_right   = cols_right;
    rowbytes     = width - cols_right;
    offset_left  = cols_left;
    offset_right = cols_right;
    block        = rowbytes + (offset_left); 

    /* too much */
    pixeltoins = (offset_left)*height + (cols_left)*height/2;
  
    /* make room */
    memmove (image + pixeltoins, image, width*height + width*height/2);
  
    // Y

    in   =  image + pixeltoins;
    out  =  image + cols_left;
  
    for (y = 0; y < height; y++) {
      
        memset (out-cols_left, BLACK_BYTE_Y, cols_left);
	memmove (out, in, rowbytes);
      
	// advance to next row
      
	in  += (rowbytes+offset_right);
	out += block;
    }

    // Cr

    new = (width+cols_left-cols_right)*height;

    rowbytes     = (width - offset_right)/2;
    offset_left  = cols_left/2;
    offset_right = cols_right/2;
    block        = rowbytes + offset_left;
  
    in   =  image + pixeltoins + width*height;
    out  =  image + new + offset_left;
  
    for (y = 0; y < height/2; y++) {
      
        memset(out-offset_left, BLACK_BYTE_UV, offset_left);
	memmove(out, in, rowbytes);
      
	// advance to next row
      
	in  += (rowbytes+offset_right);
	out += block;
    }
  
    // Cb

    /* read: new += (width/2 + (cols_left-cols_right)/2)*height/2; */
    new += (width + cols_left-cols_right) * height/4;
    in  = image + pixeltoins + width*height + width*height/4;
    out = image + new + offset_left;

    for (y = 0; y < height/2; y++) {
      
        memset(out-offset_left, BLACK_BYTE_UV, offset_left);
	memmove(out, in, rowbytes);
      
	// advance to next row

	in  += (rowbytes + offset_right);
	out += block;
    }
    return;

  } 
  
  /* clip left and add black bars right */
  
  else if (cols_left >= 0 && cols_right < 0) {

    cols_left    = cols_left;
    cols_right   = -cols_right;
    rowbytes     = width - cols_left;
    offset_left  = cols_left;
    offset_right = cols_right;
    block        = rowbytes + (offset_right); 

    /* too much */
    pixeltoins = (offset_right)*height + (cols_right)*height/2;
  
    /* make room */
    memmove (image + pixeltoins, image, width*height + width*height/2);
  
    // Y

    in   =  image + pixeltoins + offset_left;
    out  =  image;

    for (y = 0; y < height; y++) {
      
        memmove (out, in, rowbytes);
	memset (out+rowbytes, BLACK_BYTE_Y, cols_right);
      
	// advance to next row
      
	in  += (rowbytes+offset_left);
	out += block;
    }

    // Cr

    new = (width-cols_left+cols_right)*height;

    rowbytes     = (width - offset_left)/2;
    offset_left  = cols_left/2;
    offset_right = cols_right/2;
    block        = rowbytes + offset_right;
  
    in   =  image + pixeltoins + width*height + offset_left;
    out  =  image + new;
  
    for (y = 0; y < height/2; y++) {
      
        memmove(out, in, rowbytes);
	memset(out+rowbytes, BLACK_BYTE_UV, offset_right);
      
	// advance to next row
      
	in  += (rowbytes+offset_left);
	out += block;
    }
  
    // Cb

    /* read: new += (width/2 + (-cols_left+cols_right)/2)*height/2; */
    new += (width - cols_left+cols_right) * height/4;
    in  = image + pixeltoins + width*height + width*height/4 + offset_left;
    out = image + new;

    for (y = 0; y < height/2; y++) {
      
        memmove(out, in, rowbytes);
	memset(out+rowbytes, BLACK_BYTE_UV, offset_right);
      
	// advance to next row

	in  += (rowbytes + offset_left);
	out += block;
    }
    return;

  }
}


void yuv_vclip(char *image, int width, int height, int lines)
{

  
  char *in, *out;
  
  unsigned offset, block, bar; 

  // Y
  
  block = width;

  if(lines>0) {
  
      in   =  image + block * lines;
      out  =  image;
      
      memcpy(out, in, block*(height - 2*lines));
      
      block = width/2;
      
      // Cr
      
      offset = width*(height-2*lines);
      
      in   =  image + width*height + block * lines/2;
      out  =  image + offset;
      
      memcpy(out, in, block*(height/2 - lines));
      
      // Cb
      
      offset += width/2*(height/2 - lines);
      
      in   =  image + width*height + ((width*height)>>2) + block * lines/2;
      out  =  image + offset;
      
      memcpy(out, in, block*(height/2 - lines));
      
      return;
  }


  // shift frame and generate black bars at top and bottom
  
  bar = (- lines * width)/4; //>0
  block = (width * height) /4;

  //Cb  
  
  in  = image + (4+1)*block;
  out = image + (4+1)*block + (8+2+1) * bar;

  memmove(out, in, block); 
  memset(out-bar, BLACK_BYTE_UV, bar);
  memset(out + block, BLACK_BYTE_UV, bar);

  //Cr  

  in  = image + 4*block;
  out = image + 4*block + (8+1) * bar;
  
  memmove(out, in, block); 
  memset(out-bar, BLACK_BYTE_UV, bar);
  memset(out + block, BLACK_BYTE_UV, bar);


  //Y

  in  = image;
  out = image + 4*bar;
  
  memmove(out, in, 4*block); 
  memset(image, BLACK_BYTE_Y, 4*bar);
  memset(out + 4*block, BLACK_BYTE_Y, 4*bar);
  
}


void yuv_clip_top_bottom(char *image, char *dest, int _width, int _height, int _lines_top, int _lines_bottom)
{
 
  char *in=NULL, *out=NULL, *next, *offset;

  int bytes=0;

  unsigned int block; 

  int height=0, width=0, lines_top, lines_bottom;

  //source and target
  next   = dest;
  offset = image;

  // Y
  
  height       = _height;
  width        = _width;
  lines_top    = _lines_top;
  lines_bottom = _lines_bottom;

  block  = width;

  if(lines_top>=0 && lines_bottom>=0) { 
    in    = offset + block * lines_top;
    out   = next;
    bytes = block*(height - (lines_bottom + lines_top));
    next += bytes;
  }

 if(lines_top<0 && lines_bottom>=0) { 
    in    = offset;
    bytes = block*(height - lines_bottom);
    out   = next - lines_top*block;
    memset(next, BLACK_BYTE_Y, -lines_top*block);
    next += block*(height - (lines_bottom+lines_top));
  }

  if(lines_top>=0 && lines_bottom<0) { 
    in    = offset + block * lines_top;
    bytes = block*(height - lines_top);
    memset(next+bytes, BLACK_BYTE_Y, -lines_bottom*block);
    out   = next;
    next += (bytes - block*lines_bottom);
  }

  if(lines_top<0 && lines_bottom<0) { 
    in    = offset;
    bytes = block * height;
    memset(next, BLACK_BYTE_Y, -lines_top*block);
    memset(next+bytes-lines_top*block, BLACK_BYTE_Y, -lines_bottom*block);
    out   = next - lines_top*block;
    next += (bytes - block*(lines_top + lines_bottom)); 
  }

  //transfer
  memcpy(out, in, bytes);

  height       = _height/2;
  width        = _width/2;
  lines_top    = _lines_top/2;
  lines_bottom = _lines_bottom/2;

  block = width;

  // Cr
  offset += _width*_height;
  
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
    memset(next, BLACK_BYTE_UV, -lines_top*block);
    next += block*(height - (lines_bottom+lines_top));
  }

  if(lines_top>=0 && lines_bottom<0) { 
    in    = offset + block * lines_top;
    bytes = block*(height - lines_top);
    memset(next+bytes, BLACK_BYTE_UV, -lines_bottom*block);
    out   = next;
    next += (bytes - block*lines_bottom);
  }

  if(lines_top<0 && lines_bottom<0) { 
    in    = offset;
    bytes = block * height;
    memset(next, BLACK_BYTE_UV, -lines_top*block);
    memset(next+bytes-lines_top*block , BLACK_BYTE_UV, -lines_bottom*block);
    out   = next - lines_top*block;
    next += (bytes - block*(lines_top + lines_bottom)); 
  }

  
  //transfer
  memcpy(out, in, bytes);

  // Cb
  offset += (_width*_height)/4;

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
    memset(next, BLACK_BYTE_UV, -lines_top*block);
    next += block*(height - (lines_bottom+lines_top));
  }

  if(lines_top>=0 && lines_bottom<0) { 
    in    = offset + block * lines_top;
    bytes = block*(height - lines_top);
    memset(next+bytes, BLACK_BYTE_UV, -lines_bottom*block);
    out   = next;
    next += (bytes - block*lines_bottom);
  }

  if(lines_top<0 && lines_bottom<0) { 
    in    = offset;
    bytes = block * height;
    memset(next, BLACK_BYTE_UV, -lines_top*block);
    memset(next+bytes-lines_top*block, BLACK_BYTE_UV, -lines_bottom*block);
    out   = next - lines_top*block;
    next += (bytes - block*(lines_top + lines_bottom)); 
  }

  //transfer
  memcpy(out, in, bytes);
}


void yuv_mirror_core(char *image, int width,  int height)
{

  char *in, *out;

  char tt;

  unsigned int y, x, rowbytes;
  
  rowbytes = width;

  in   =  image + rowbytes/2;
  out  =  image + rowbytes/2 - 1;
  
  for (y = 0; y < height; ++y) {
      for (x = 0; x < width/2; x++) {

	  // push pixel on stack
  
	  tt = *(out);

	  // swap pixel

	  *out = *in; 

	  // pop pixel
	  
	  *in = tt;
	  
	  // adjust pointer within row 
	  
	  in +=1;
	  out -=1;
      }      
      
      // row finished, adjust pointer for new row loop
      
      in += rowbytes/2;
      out += rowbytes/2; 
  }
}

void yuv_mirror(char *image, int width, int height)
{
    int block = (width*height)>>2;

    yuv_mirror_core(image, width, height);
    yuv_mirror_core(image + 4*block, width/2, height/2);
    yuv_mirror_core(image + 5*block, width/2, height/2);
}


void yuv_swap(char *image, int width, int height)
{

  char *in, *out;

  int n, block;

  char tt;

  block = (width*height)>>2;


  in  = image + 4*block;
  out = image + 5*block;

  for(n=0; n<block; n++) {
    
    tt = in[n]; 
    in[n]  = out[n];
    out[n] = tt;
  }
}

inline int yuv_merge_C(char *row1, char *row2, char *out, int bytes, 
		      unsigned long weight1, unsigned long weight2)
{
  // blend each color entry in two arrays and return
  // result in char *out

    unsigned int y;
    unsigned long tmp;
    register unsigned long w1 = weight1;
    register unsigned long w2 = weight2;

    for (y = bytes-1; y; --y) {
      tmp = w2 * (unsigned char) row2[y] + w1 *(unsigned char) row1[y];
      out[y] = (tmp>>16) & 0xff;
    }
    tmp = w2 * (unsigned char) row2[0] + w1 *(unsigned char) row1[0];
    out[0] = (tmp>>16) & 0xff;

    return(0);
}

inline int yuv_average_C(char *row1, char *row2, char *out, int bytes)
{
  
  // average of each color entry in two arrays and return
  // result in char *out
  
  unsigned int y;
  unsigned short tmp;
  
  for (y = 0; y<bytes; ++y) {
    tmp = ((unsigned char) row2[y] + (unsigned char) row1[y])>>1;
    out[y] = tmp & 0xff;
  }
  
  return(0);
}


void yuv_vresize_8_Y(char *image, int width, int height, int resize)
{
  
  char *in, *out;
  
  unsigned int i, j=0, row_bytes, chunk, rows, n_height, m; 
    
  // resize output video 
  
  row_bytes = width;
  
  // new height
  n_height = height - (resize<<3);
  
  //number of rows in new chunk
  rows =  n_height >>3;
  
  // chunk size
  chunk = (height >>3) * row_bytes;

  // dest row index
  m=0;

  for(j = 0; j < 8; ++j) {
    for (i = 0; i<rows; ++i) {
      
      in = image + j*chunk +  vert_table_8[i].source * row_bytes;
      out = image + m * row_bytes;
      
      yuv_merge_16(in, in+row_bytes, out, row_bytes, vert_table_8[i].weight1, vert_table_8[i].weight2);
      
      ++m;
    }
  }      
  
  return;
}


void yuv_vresize_8_up_Y(char *dest, char *image, int width, int height, int resize)
{
  
  char *in, *out, *last_row;
  
  unsigned int i, j=0, row_bytes, chunk, rows, n_height, m; 
    
  // resize output video 
  
  row_bytes = width;
  
  // make sure that we don't over-walk the end of the image

  last_row = image + (height-1)*width; 
    
  // new height
  n_height = height + (resize<<3);
  
  //number of rows in new chunk
  rows =  n_height >>3;
  
  // chunk size
  chunk = (height >>3) * row_bytes;

  // dest row index
  m=0;
  
  for(j = 0; j < 7; ++j) {
    for (i = 0; i < rows; i++) {
      
      in = image + j*chunk +  vert_table_8_up[i].source * row_bytes;
      out = dest + m * row_bytes;
      
      yuv_merge_16(in, in+row_bytes, out, row_bytes, vert_table_8_up[i].weight1,
      		   vert_table_8_up[i].weight2);
      ++m;
    }
  }      
  for(i=0; i< rows; i++){

    in = image + j*chunk +  vert_table_8_up[i].source * row_bytes;
    out = dest + m * row_bytes;

    if (in >= last_row){
      memcpy(out,last_row,row_bytes);
    } else {
      yuv_merge_16(in, in+row_bytes, out, row_bytes, vert_table_8_up[i].weight1,
      		   vert_table_8_up[i].weight2);
    }
    ++m;
  }

  return;
}


void yuv_vresize_16_CbCr(char *image, int width, int height, int resize)
{
  
  char *in, *out;
  
  unsigned int i, j=0, row_bytes, chunk, rows, n_height, m; 
    
  // resize output video 
  
  row_bytes = width;
  
  // new height
  n_height = height - (resize<<2);
  
  //number of rows in new chunk
  rows =  n_height >>2;
  
  // chunk size
  chunk = (height >>2) * row_bytes;

  // dest row index

  m=0;
  
  for(j = 0; j < 4; ++j) {
    
    for (i = 0; i < rows; i++) {
      
      in = image + j*chunk +  vert_table_8[i].source * row_bytes;
      out = image + m * row_bytes;
      
      yuv_merge_8(in, in+row_bytes, out, row_bytes, vert_table_8[i].weight1, vert_table_8[i].weight2);

      ++m;
    }
  }      

  return;
}

void yuv_vresize_16_up_CbCr(char *dest, char *image, int width, int height, int resize)
{
  
  char *in, *out, *last_row;
  
  unsigned int i, j=0, row_bytes, chunk, rows, n_height, m; 
    
  // resize output video 
  
  row_bytes = width;
  
  // make sure we don't walk off the end of the array
  last_row = image+(height-1)*width;
  // new height
  n_height = height + (resize<<2);
  
  //number of rows in new chunk
  rows =  n_height >>2;
  
  // chunk size
  chunk = (height >>2) * row_bytes;

  // dest row index
  m=0;
  
  for(j = 0; j < 3; ++j) {
    
    for (i = 0; i < rows; i++) {
      
      in = image + j*chunk +  vert_table_8_up[i].source * row_bytes;
      out = dest + m * row_bytes;
      
      yuv_merge_8(in, in+row_bytes, out, row_bytes, vert_table_8_up[i].weight1, vert_table_8_up[i].weight2);

      ++m;
    }
  }      
  for (i = 0; i < rows; i++) {

    in = image + j*chunk +  vert_table_8_up[i].source * row_bytes;
    out = dest + m * row_bytes;

    if (in >= last_row){
      memcpy(out,last_row,row_bytes);
    } else {
      yuv_merge_8(in, in+row_bytes, out, row_bytes, vert_table_8_up[i].weight1,
      		  vert_table_8_up[i].weight2);
    }
    ++m;
  }

  return;
}

void yuv_vresize_8(char *image, int width, int height, int resize)
{

    int n_height, bytes;
    
    // Y 
    yuv_vresize_8_Y(image, width, height, resize);
    
    // Cr
    yuv_vresize_16_CbCr(image + width*height, width/2, height/2, resize);
    
    // Cb
    yuv_vresize_16_CbCr(image + width*height + ((width*height)>>2), 
			width/2, height/2, resize);

    // shift fields:

    n_height = height - (resize<<3);
    bytes = (n_height*width)>>2;
    
    // Cr
    memcpy(image + n_height * width, image + width * height, bytes);
    // Cb
    memcpy(image + n_height * width + bytes, image + width*height + ((width*height)>>2), bytes);

    return;
}


void yuv_vresize_8_up(char *image, char *tmp_image, int width, int height, int resize)
{

    int n_height;

    n_height = height + (resize<<3);
    
    // Y 
    yuv_vresize_8_up_Y(tmp_image, image, width, height, resize);
    
    // Cr
    yuv_vresize_16_up_CbCr(tmp_image + width*n_height, image + width*height, 
			   width/2, height/2, resize);
    
    // Cb
    yuv_vresize_16_up_CbCr(tmp_image + width*n_height * 5/4, image + width*height*5/4, width/2, height/2, resize);

    return;
}


void yuv_hresize_8_Y(char *image, int width, int height, int resize)
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
	
	in  = image + j * blocks + hori_table_8[i].source;
	out = image + m;
	
	yuv_merge_C(in, in+1, out, 1, hori_table_8[i].weight1, hori_table_8[i].weight2);

	m+=1;
      }
    }      
    
    return;
}

void yuv_hresize_8_up_Y(char * dest, char *image, int width, int height, int resize)
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
	
	incr = j * blocks + hori_table_8_up[i].source;
	in  = image + incr;
	out = dest + m;
	
	(!((incr+1)%width)) ? *out=*in : yuv_merge_C(in, in+1, out, 1, hori_table_8_up[i].weight1, hori_table_8_up[i].weight2);

	m+=1;
      }
    }      
    
    return;
}


void yuv_hresize_16_CrCb(char *image, int width, int height, int resize)
{
    
    char *in, *out;
    
    unsigned int m, cols, i, j, pixels, n_width, blocks; 
    
    // resize output video 
    
    // process linear frame buffer;
    
    n_width = width - (resize<<2);
    pixels  = n_width * height;
    
    blocks = width>>2;
    
    cols = n_width >>2;
    
    // treat as linear buffer of arrays with cols pixels
    
    // pixel index
    m=0;
    
    for(j = 0; j < 4 * height; ++j) {
      
      for (i = 0; i < cols; i++) {
	
	in  = image + j * blocks + hori_table_8[i].source;
	out = image + m;
	
	yuv_merge_C(in, in+1, out, 1, hori_table_8[i].weight1, hori_table_8[i].weight2);
	m+=1;
      }
    }      
    
    return;
}


void yuv_hresize_16_up_CrCb(char *dest, char *image, int width, int height, int resize)
{
    
    char *in, *out;
    
    unsigned int m, cols, i, j, pixels, n_width, blocks, incr; 
    
    // resize output video 
    
    // process linear frame buffer;
    
    n_width = width + (resize<<2);
    pixels  = n_width * height;
    
    blocks = width>>2;
    
    cols = n_width >>2;
    
    // treat as linear buffer of arrays with cols pixels
    
    // pixel index
    m=0;
    
    for(j = 0; j < 4 * height; ++j) {
      for (i = 0; i < cols; i++) {
	
	incr = j * blocks + hori_table_8_up[i].source;
	in  = image + incr;
	out = dest + m;
	
	(!((incr+1)%width)) ? *out=*in : yuv_merge_C(in, in+1, out, 1, hori_table_8_up[i].weight1, hori_table_8_up[i].weight2);

	m+=1;
      }
    }      
    
    return;
}

void yuv_hresize_8(char *image, int width, int height, int resize)
{

    int n_width, bytes;
    
    // Y 
    yuv_hresize_8_Y(image, width, height, resize);
    
    // Cr
    yuv_hresize_16_CrCb(image + width*height, width/2, height/2, resize);
    
    // Cb
    yuv_hresize_16_CrCb(image + width*height + ((width*height)>>2), 
			width/2, height/2, resize);

    // shift fields:

    n_width = width - (resize<<3);
    bytes = (n_width * height)>>2;
    
    // Cr
    memcpy(image + n_width * height, image + width * height, bytes);
    // Cb
    memcpy(image + n_width * height + bytes, image + width*height + ((width*height)>>2), bytes);

    return;
}


void yuv_hresize_8_up(char *image, char *tmp_image, int width, int height, int resize)
{

    int n_width;

    n_width = width + (resize<<3);
    
    // Y 
    yuv_hresize_8_up_Y(tmp_image, image, width, height, resize);
    
    // Cr
    yuv_hresize_16_up_CrCb(tmp_image + n_width*height, image +width*height,
			   width/2, height/2, resize);
    
    // Cb
    yuv_hresize_16_up_CrCb(tmp_image + n_width*height*5/4, 
			   image + width*height*5/4,
			   width/2, height/2, resize);
    
    return;
}

static int (*yuv_average) (char *row1, char *row2, char *out, int bytes);
static int (*memcpy_accel) (char *dest, char *source, int bytes);

inline static int memcpy_C(char *dest, char *source, int bytes)
{
  memcpy(dest, source, bytes);
  return(0);
}


inline void yuv_deinterlace_linear_core(char *image, int width, int height)
{
    char *in, *out;
    
    unsigned int y, block; 
    
    block = width;

    in  = image;
    out = image;
    
    // convert half frame to full frame by simple interpolation
      
    out +=block;
    
    for (y = 0; y < (height>>1)-1; y++) {
      
      yuv_average(in, in+(block<<1), out, block);
      
      in  += block<<1;
      out += block<<1;
    }
    
    // clone last row
    
    memcpy(out, in, block);

    return;
}

inline void yuv_deinterlace_linear_blend_core(char *image, char *tmp, int width, int height)
{
  char *in, *out;
  
  unsigned int y, block; 
  
  block = width;
  
  //(1)
  //copy frame to 2. internal frame buffer

  memcpy_accel(tmp, image, width*height);

  //(2)
  //convert first field to full frame by simple interpolation
  //row(1)=(row(0)+row(2))/2
  //row(3)=(row(2)+row(4))/2
  //...


  in  = image;
  out = image;
  
  out +=block;
  
  for (y = 0; y < (height>>1)-1; y++) {
    
    yuv_average(in, in+(block<<1), out, block);
    
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
    
    yuv_average(in, in+(block<<1), out, block);
    
    in  += block<<1;
    out += block<<1;
  }
  
  //(4)
  //blend both frames
  yuv_average(image, tmp, image, width*height);
  
  return;
}

void yuv_deinterlace_linear(char *image, int width, int height)
{
  //default ia32 C mode:
  yuv_average =  yuv_average_C;
  
#ifdef ARCH_X86 
#ifdef HAVE_ASM_NASM

  if(tc_accel & MM_MMX)  yuv_average = ac_average_mmx;  
  if(tc_accel & MM_SSE)  yuv_average = ac_average_sse;
  if(tc_accel & MM_SSE2) yuv_average = ac_average_sse2;
  
#endif
#endif
  
  //process only Y component
  yuv_deinterlace_linear_core(image, width, height);

  clear_mmx();
}


void yuv_deinterlace_linear_blend(char *image, char *tmp, int width, int height)
{

  //default ia32 C mode:
  yuv_average =  yuv_average_C;
  memcpy_accel = memcpy_C;
  
#ifdef ARCH_X86 
#ifdef HAVE_ASM_NASM

  if(tc_accel & MM_MMX) {
    yuv_average = ac_average_mmx;  
    memcpy_accel = ac_memcpy_mmx;
  }

  if(tc_accel & MM_SSE) {
    yuv_average = ac_average_sse;
    memcpy_accel = ac_memcpy_sse;
  }

  if(tc_accel & MM_SSE2) {
    yuv_average = ac_average_sse2;
    memcpy_accel = ac_memcpy_sse2;
  }

  
#endif
#endif
  
  //process only Y component
  yuv_deinterlace_linear_blend_core(image, tmp, width, height);

  clear_mmx();
}


inline void yuv_decolor(char *image, int offset)
{
    
    unsigned int y;

    for (y = 0; y<offset/2; ++y) image[offset + y] = BLACK_BYTE_UV;
    
    return;
}

static void yuv_zoom_done(void)
{

  int id;
  
  //get thread id:
  
  id=get_fthread_id(0);
  
  if (tbuf[id].zoomerY)
    zoom_image_done(tbuf[id].zoomerY);
  tbuf[id].zoomerY  = NULL;

  if (tbuf[id].zoomerUV)
    zoom_image_done(tbuf[id].zoomerUV);
  tbuf[id].zoomerUV = NULL;
//  free(tbuf[id].tmpBuffer);
}


static void yuv_zoom_init(char *image, char *tmp_buf, int width, int height, int new_width, int new_height)
{
  
  int id;

  vob_t *vob;
  
  //get thread id:
  
  id=get_fthread_id(0);

  //tbuf[id].tmpBuffer = (pixel_t*) malloc(new_width*new_height + (new_width*new_height)/2);

  tbuf[id].tmpBuffer = (pixel_t*) tmp_buf;

  if(tbuf[id].tmpBuffer==NULL) tc_error("out of memory\n");
  
  zoom_setup_image(&tbuf[id].srcImageY, width, height, 1, image);
  zoom_setup_image(&tbuf[id].srcImageUV, width/2, height/2, 1, image + width*height);
  zoom_setup_image(&tbuf[id].dstImageY, new_width, new_height, 1, tbuf[id].tmpBuffer);
  zoom_setup_image(&tbuf[id].dstImageUV, new_width/2, new_height/2, 1, tbuf[id].tmpBuffer + new_width*new_height);
  
  vob= tc_get_vob();

  tbuf[id].zoomerY = zoom_image_init(&tbuf[id].dstImageY, &tbuf[id].srcImageY, vob->zoom_filter, vob->zoom_support);
  tbuf[id].zoomerUV = zoom_image_init(&tbuf[id].dstImageUV, &tbuf[id].srcImageUV, vob->zoom_filter, vob->zoom_support);

  atexit(yuv_zoom_done);
}


void yuv_zoom(char *image, char *_tmp_buf, int width, int height, int new_width, int new_height)
{

  int id;
  char *tmp_buf=_tmp_buf;

  //get thread id:
  
  id=get_fthread_id(0);

  if (tbuf[id].zoomerY == NULL)
    yuv_zoom_init(image, tmp_buf, width, height, new_width, new_height);

  //tmp_buf = tbuf[id].tmpBuffer;

  tbuf[id].srcImageY.data = image;
  tbuf[id].dstImageY.data = tmp_buf;
  zoom_image_process(tbuf[id].zoomerY);

  tbuf[id].srcImageUV.data = image + width*height;
  tbuf[id].dstImageUV.data = tmp_buf + new_width*new_height;
  zoom_image_process(tbuf[id].zoomerUV);
  
  tbuf[id].srcImageUV.data = image + width*height + ((width*height)>>2);
  tbuf[id].dstImageUV.data = tmp_buf + new_width*new_height + ((new_width*new_height)>>2);
  zoom_image_process(tbuf[id].zoomerUV);
  
  //  memcpy(image, tbuf[id].tmpBuffer, new_width*new_height + (new_width*new_height)/2);

}


static void yuv_zoom_done_DI(void)
{

  int id;
  
  //get thread id:
  
  id=get_fthread_id(1);
  
  if (tbuf_DI[id].zoomerY)
    zoom_image_done(tbuf_DI[id].zoomerY);
  tbuf_DI[id].zoomerY  = NULL;

  if (tbuf_DI[id].zoomerUV)
    zoom_image_done(tbuf_DI[id].zoomerUV);
  tbuf_DI[id].zoomerUV  = NULL;

  if (tbuf_DI[id].tmpBuffer)
      free(tbuf_DI[id].tmpBuffer);
  tbuf_DI[id].tmpBuffer = NULL;
}


static void yuv_zoom_init_DI(char *image, int width, int height, int new_width, int new_height, int id)
{
  
  vob_t *vob;
  
  tbuf_DI[id].tmpBuffer = (pixel_t*) malloc(new_width*new_height + (new_width*new_height)/2);

  if(tbuf_DI[id].tmpBuffer==NULL) tc_error("out of memory\n");
  
  zoom_setup_image(&tbuf_DI[id].srcImageY, width, height, 1, image);
  zoom_setup_image(&tbuf_DI[id].srcImageUV, width/2, height/2, 1, image + width*height);
  zoom_setup_image(&tbuf_DI[id].dstImageY, new_width, new_height, 1, tbuf_DI[id].tmpBuffer);
  zoom_setup_image(&tbuf_DI[id].dstImageUV, new_width/2, new_height/2, 1, tbuf_DI[id].tmpBuffer + new_width*new_height);
  
  vob = tc_get_vob();
  
  tbuf_DI[id].zoomerY = zoom_image_init(&tbuf_DI[id].dstImageY, &tbuf_DI[id].srcImageY, vob->zoom_filter, vob->zoom_support);
  tbuf_DI[id].zoomerUV = zoom_image_init(&tbuf_DI[id].dstImageUV, &tbuf_DI[id].srcImageUV, vob->zoom_filter, vob->zoom_support);

  atexit(yuv_zoom_done_DI);
}


void yuv_zoom_DI(char *image, int width, int height, int new_width, int new_height)
{

  int id;
  
  //get thread id:
  
  id=get_fthread_id(1);

  if (tbuf_DI[id].zoomerY == NULL)
    yuv_zoom_init_DI(image, width, height, new_width, new_height, id);

  tbuf_DI[id].srcImageY.data = image;
  tbuf_DI[id].dstImageY.data = tbuf_DI[id].tmpBuffer;
  zoom_image_process(tbuf_DI[id].zoomerY);

  tbuf_DI[id].srcImageUV.data = image + width*height;
  tbuf_DI[id].dstImageUV.data = tbuf_DI[id].tmpBuffer + new_width*new_height;
  zoom_image_process(tbuf_DI[id].zoomerUV);
  
  tbuf_DI[id].srcImageUV.data = image + width*height + ((width*height)>>2);
  tbuf_DI[id].dstImageUV.data = tbuf_DI[id].tmpBuffer + new_width*new_height + ((new_width*new_height)>>2);
  zoom_image_process(tbuf_DI[id].zoomerUV);
  
  memcpy(image, tbuf_DI[id].tmpBuffer, new_width*new_height + (new_width*new_height)/2);
}



void yuv_gamma(char *image, int len)
{
    int n;
    unsigned char *c;

    c = (unsigned char*) image;

    for(n=0; n<=len; ++n) {
      *c = gamma_table[*c];
      ++c;
    }

    return;
}

void deinterlace_yuv_zoom(unsigned char *src, int width, int height)
{

    char *in, *out;

    int i, block;

    // move first field into first half of frame buffer 

    // Y
    block = width;

    in  = src;
    out = src;

    //move every second row
    for (i=0; i<height; i=i+2) {
	
      memcpy(out, in, block);
      in  += 2*block;
      out += block;
    }

    block = width/2;

    // Cb

    in  = src + width*height;
    out = src + width*height/2;

    //move every second row
    for (i=0; i<height/2; i=i+2) {
      
      memcpy(out, in, block);
      in  += 2*block;
      out += block;
    }

    // Cr

    in  = src + width*height*5/4;
    out = src + width*height*5/8;

    //move every second row
    for (i=0; i<height/2; i=i+2) {
      
      memcpy(out, in, block);
      in  += 2*block;
      out += block;
    }
    
    //high quality zoom out
    yuv_zoom_DI(src, width, height/2, width, height);
  
}

void deinterlace_yuv_nozoom(unsigned char *src, int width, int height)
{

    char *in, *out;

    int i, block;

    // move first field into first half of frame buffer 

    // Y
    block = width;

    in  = src;
    out = src;

    //move every second row
    for (i=0; i<height; i=i+2) {
	
      memcpy(out, in, block);
      in  += 2*block;
      out += block;
    }

    block = width/2;

    // Cb

    in  = src + width*height;
    out = src + width*height/2;

    //move every second row
    for (i=0; i<height/2; i=i+2) {
      
      memcpy(out, in, block);
      in  += 2*block;
      out += block;
    }

    // Cr

    in  = src + width*height*5/4;
    out = src + width*height*5/8;

    //move every second row
    for (i=0; i<height/2; i=i+2) {
      
      memcpy(out, in, block);
      in  += 2*block;
      out += block;
    }
}


void merge_yuv_fields(unsigned char *src1, unsigned char *src2, int width, int height)
{
  
    char *in, *out;

    int i, block;

    block = width;

    in  = src2 + block;
    out = src1 + block;

    //move every second row
    //Y
    for (i=0; i<height; i=i+2) {
	
	memcpy(out, in, block);
	in  += 2*block;
	out += 2*block;
    }


    block = width/2;

    //Cb
    in  = src2 + width*height + block;
    out = src1 + width*height + block;

    //move every second row
    for (i=0; i<height/2; i=i+2) {
	
	memcpy(out, in, block);
	in  += 2*block;
	out += 2*block;
    }


    //Cr
    in  = src2 + width*height*5/4 + block;
    out = src1 + width*height*5/4 + block;

    //move every second row
    for (i=0; i<height/2; i=i+2) {
	
	memcpy(out, in, block);
	in  += 2*block;
	out += 2*block;
    }
}



inline static int samecolor(char *color1, char *color2)
{
    
    unsigned short diff;
    
    diff = abs((unsigned char) *color1 - (unsigned char) *color2);
    
    return (diff < s_threshold);
}

inline static int diffcolor(char *color, char *color1, char *color2)
{
  unsigned short diff;
  
  diff = abs((unsigned char) *color - (unsigned char)*color1);
  if (diff < d_threshold) return TC_FALSE; 
  
  diff = abs((unsigned char) *color - (unsigned char)*color2);
  return (diff > d_threshold);
}


#define NORTH (src_ptr - srowstride)
#define SOUTH (src_ptr + srowstride)
#define EAST  (src_ptr + 1)
#define WEST  (src_ptr - 1)


static void antialias(char *inrow, char *outrow, int pixels)
{
    
  unsigned char *dest_ptr, *src_ptr;
  
  unsigned int i;
  
  //bytes per row
  unsigned int srowstride=(pixels+2);  
  
  unsigned long tmp;
  
  src_ptr  = inrow;
  dest_ptr = outrow;
  
  
  for(i=0; i < pixels; ++i) {
      
      //byte test
      if ((samecolor(WEST,NORTH) && diffcolor(WEST,SOUTH,EAST)) ||
	  (samecolor(WEST,SOUTH) && diffcolor(WEST,NORTH,EAST)) ||
	  (samecolor(EAST,NORTH) && diffcolor(EAST,SOUTH,WEST)) ||
	  (samecolor(EAST,SOUTH) && diffcolor(EAST,NORTH,WEST))) {
	  
	  tmp = aa_table_c[(uint8_t) *src_ptr] 
	      + aa_table_d[(uint8_t)*(src_ptr-srowstride-1)]  
	      + aa_table_y[(uint8_t)*(src_ptr-srowstride)]   
	      + aa_table_d[(uint8_t)*(src_ptr-srowstride+1)] 
	      + aa_table_x[(uint8_t)*(src_ptr-1)] 
	      + aa_table_x[(uint8_t)*(src_ptr+1)] 
	      + aa_table_d[(uint8_t)*(src_ptr+srowstride-1)] 
	      + aa_table_y[(uint8_t)*(src_ptr+srowstride)] 
	      + aa_table_d[(uint8_t)*(src_ptr+srowstride+1)];
	  
	  *dest_ptr = (verbose & TC_DEBUG) ? 255 & 0xff : (tmp>>16) & 0xff;

      } else { //elseif not aliasing 
	  
	  *dest_ptr = *src_ptr;
      
      } // endif not aliasing
      
      ++dest_ptr;
      ++src_ptr;
      
  } // next pixel
  
  return;
}

#undef NORTH
#undef SOUTH
#undef EAST
#undef WEST


void yuv_antialias(char *image, char *dest, int width, int height, int mode)
{
    
    int i, j, block, pixels;
    
    char *in, *out;
    
    
    j = height >>3;
    
    block = width;
    
    pixels = width-2;
    
    in  = image;
    out = dest;
    
    switch(mode) {
	
    case 2: // only process the new rows, that have been created by previous               // resize operation with maximum spacing
	
	
	for (i=0; i<height-1; ++i) {
	    
	    in  += block;
	    out += block;
	    
	    if (vert_table_8[i%j].dei) {
		antialias(in+1, out+1, pixels);
		
		//first and last pixel untouched
		*out = *in;
		*(out+pixels+1) = *(in+pixels+1); 
	    } else 
	      //copy untouched row
	      memcpy(out, in, width);
	}
	
	break;
	
    case 1: // process rows, created by deinterlace operation
	
	in+=block;
	
	for (i=0; i<(height>>1); ++i) {

	    //first and last pixel untouched
	    *out = *in;
	    *(out+pixels+1) = *(in+pixels+1); 

	    antialias(in+1, out+1, pixels);

	    //copy untouched row
	    memcpy(out+block, in+block, width);

	    in  += block<<1;
	    out += block<<1;
	}	      
	
	break;


    case 3: // full frame processing (slowest)
	
	for (i=0; i<height-1; ++i) {

	    in  += block;
	    out += block;

	    //first and last pixel untouched
	    *out = *in;
	    *(out+pixels+1) = *(in+pixels+1); 
	    
	    antialias(in+1, out+1, pixels);
	}	      
	
      break;
      
    case 0:
    default:
	// do nothing
      break;
    }
    
    //first and last row untouched
    memcpy(dest, image, pixels+2);
    memcpy(dest+(height-1)*width, image+(height-1)*width, pixels+2);
    
    return;
}
