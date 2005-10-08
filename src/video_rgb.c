/*
 *  video_rgb.c
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

#include "transcode.h"
#include "framebuffer.h"
#include "video_trans.h"
#include "zoom.h"
#include "aclib/imgconvert.h"

#define BLACK_BYTE 0

/* ------------------------------------------------------------ 
 *
 * video frame transformation auxiliary routines
 *
 * ------------------------------------------------------------*/

void rgb_rescale(char *image, int width, int height, int reduce_h, int reduce_w)
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
	  ac_memcpy (out, in, 3);
	  
	  out = out + 3; 
	  in  = in + 3 * reduce_w;
	}
      in  = in + width * 3 * (reduce_h - 1);
    }
}


void rgb_flip(char *image, int width, int height)
{

  char *in, *out;

  // this shouldn't be too hard on the stack
  char rowbuffer[TC_MAX_V_FRAME_WIDTH*3];
  
  unsigned int y, block;  
  
  block = width * 3;
  
  in  = image + block*(height-1);
  out = image;
  
  for (y = height; y > height/2; y--)
    {

      ac_memcpy (rowbuffer, out, block);
      ac_memcpy (out, in, block);
      ac_memcpy (in, rowbuffer, block);
      
      out = out + block; 
      in  = in - block;
    }
}

void rgb_hclip(char *image, int width, int height, int cols)
{

  char *in, *out;
  
  unsigned int y, block, offset, rowbytes; 
  unsigned int pixeltoins;

  if (cols>0) {
    rowbytes = width * 3;
    offset   = cols * 3;
    block    = rowbytes - 2*offset; 

    in   =  image + offset;
    out  =  image;

    for (y = 0; y < height; y++) {

      ac_memcpy(out, in, block);

      // advance to next row

      in  += rowbytes;
      out += block;
    }
    return;
  }

  /* cols < 0 */

  cols = -cols;
  offset   = cols * 3;
  rowbytes = width * 3;
  pixeltoins = 2*offset*height;

  /* make space at start of image */
  memmove (image + pixeltoins, image, width*height*3);

  in = image + pixeltoins;
  out = image;

  for (y = 0; y < height ; y++) {

    /* black out beginning of line */
    memset (out, BLACK_BYTE, offset);

    /* move line */
    memmove (out+offset, in, rowbytes);
    
    /* advance */
    out = out + (rowbytes + 2*offset);
    in += rowbytes;

    /* black out end of line */
    memset (out-offset, BLACK_BYTE, offset);
  }

}

void rgb_clip_left_right(char *image, int width, int height, int cols_left, int cols_right)
{

  char *in, *out;
  
  unsigned int y, block, offset, rowbytes; 
  unsigned int pixeltoins, offset_left, offset_right;

  if (cols_left >= 0 && cols_right >= 0) {

    rowbytes = width * 3;
    offset   = cols_left * 3;
    block    = rowbytes - (cols_left + cols_right)*3;


    in   =  image + offset;
    out  =  image;

    for (y = 0; y < height; y++) {

        ac_memcpy(out, in, block);

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
    offset_left  = cols_left * 3;
    offset_right = cols_right * 3;

    rowbytes = width * 3;
    pixeltoins = (offset_left+offset_right)*height;

    /* make space at start of image */
    memmove (image + pixeltoins, image, width*height*3);

    in = image + pixeltoins;
    out = image;

    for (y = 0; y < height ; y++) {

      /* black out beginning of line */
      memset (out, BLACK_BYTE, offset_left);

      /* move line */
      memmove (out+offset_left, in, rowbytes);

      /* advance */
      out = out + (rowbytes + offset_left+offset_right);
      in += rowbytes;

      /* black out end of line */
      memset (out-offset_right, BLACK_BYTE, offset_right);
    }

    return;
  }

  /* insert black left and clip right */

  else if (cols_left < 0 && cols_right >= 0) {

    cols_left    = -cols_left;
    cols_right   = cols_right;
    offset_left  = cols_left * 3;
    offset_right = cols_right * 3;

    rowbytes = width * 3 - offset_right;
    pixeltoins = (offset_left)*height;

    /* make space at start of image */
    memmove (image + pixeltoins, image, width*height*3);

    in = image + pixeltoins;
    out = image;

    for (y = 0; y < height ; y++) {

      /* black out beginning of line */
      memset (out, BLACK_BYTE, offset_left);

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
    offset_left  = cols_left * 3;
    offset_right = cols_right * 3;

    rowbytes = width * 3 - offset_left;
    pixeltoins = (offset_right)*height;

    /* make space at start of image */
    memmove (image + pixeltoins, image, width*height*3);

    in = image + pixeltoins;
    out = image;

    for (y = 0; y < height ; y++) {

      /* move line */
      memmove (out, in+offset_left, rowbytes);

      /* black out end of line */
      memset (out+rowbytes, BLACK_BYTE, offset_right);

      /* advance */
      out += (rowbytes + offset_right);
      in  += (rowbytes + offset_left);

    }

    return;
  }
}


void rgb_vclip(char *image, int width, int height, int lines)
{

  
  char *in, *out;
  
  unsigned int y, block, bar; 

  block = width * 3;    

  if(lines>0) {
  
      in   =  image + block * lines;
      out  =  image;
      
      for (y = 0; y < height - 2*lines ; y++) {
	  
	  ac_memcpy(out, in, block);
	  
	  in  += block;
	  out += block;
      }
      
      return;
  }
  
  // shift frame and generate black bars at top and bottom
  
  bar = - lines * block; //>0
  
  memmove(image + bar, image, width*height*3); 
  memset(image, BLACK_BYTE, bar);
  memset(image + width*height*3 + bar, BLACK_BYTE, bar);
  
}

void rgb_clip_top_bottom(char *image, char *dest, int width, int height, int _lines_top, int _lines_bottom)
{
  
  char *in=NULL, *out=NULL, *offset, *next;
  
  unsigned int block; 
  
  int bytes=0;
  
  int lines_top, lines_bottom;

  block = width * 3;

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
    memset(next, BLACK_BYTE, -lines_top*block);
    next += block*(height - (lines_bottom+lines_top));
  }

  if(lines_top>=0 && lines_bottom<0) { 
    in    = offset + block * lines_top;
    bytes = block*(height - lines_top);
    memset(next+bytes, BLACK_BYTE, -lines_bottom*block);
    out   = next;
    next += (bytes - block*lines_bottom);
  }

  if(lines_top<0 && lines_bottom<0) { 
    in    = offset;
    bytes = block * height;
    memset(next, BLACK_BYTE, -lines_top*block);
    memset(next+bytes, BLACK_BYTE, -lines_bottom*block);
    out   = next - lines_top*block;
    next += (bytes - block*(lines_top + lines_bottom)); 
  }

  //transfer
  ac_memcpy(out, in, bytes);
}


void rgb_mirror(char *image, int width,  int height)
{

  char *in, *out;

  char ttr, ttg, ttb;

  unsigned int y, x, rowbytes;
  
  rowbytes = width * 3;

  in   =  image + rowbytes/2;
  out  =  image + rowbytes/2 - 3;
  
  for (y = 0; y < height; ++y) {
      for (x = 0; x < width/2; x++) {

	  // push pixel on stack
  
	  ttr = *(out);
	  ttg = *(out+1);
	  ttb = *(out+2);

	  // swap pixel

	  *out = *in; 
	  *(out+1) = *(in+1);
	  *(out+2) = *(in+2); 

	  // pop pixel
	  
	  *in = ttr;
	  *(in+1) = ttg;
	  *(in+2) = ttb;
	  
	  // adjust pointer within row 
	  
	  in +=3;
	  out -=3;
      }      
      
      // row finished, adjust pointer for new row loop
      
      in += rowbytes/2;
      out += 3*rowbytes/2; 
  }
}

void rgb_swap(uint8_t *image, int pixels)
{
  uint8_t *tmp = malloc(pixels*3);
  if (!tmp) {
    tc_error("no memory for RGB swap");
    return;
  }
  ac_imgconvert(&image, IMG_RGB24, &tmp, IMG_BGR24, pixels, 1);
  ac_memcpy(image, tmp, pixels*3);
  free(tmp);
}


inline int rgb_merge_C(char *row1, char *row2, char *out, int bytes, 
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


void rgb_vresize_8(char *image, int width, int height, int resize)
{
  
  char *in, *out;
  
  unsigned int i, j=0, row_bytes, chunk, rows, n_height, m; 
    
  // resize output video 
  
  row_bytes = width * 3;
  
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
      
      ac_rescale(in, in+row_bytes, out, row_bytes,
		 vert_table_8[i].weight1, vert_table_8[i].weight2);
      ++m;
    }
  }      

  return;
}

void rgb_vresize_8_up(char *image, char *tmp_image, int width, int height, int resize)
{
  
  char *in, *out, *last_row;
  
  unsigned int i, j=0, row_bytes, chunk, rows, n_height, m; 
    
  // resize output video 
  
  row_bytes = width * 3;
  
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
      
      ac_rescale(in, in+row_bytes, out, row_bytes,
		 vert_table_8_up[i].weight1, vert_table_8_up[i].weight2);

      ++m;
    }
  }      
  for (i = 0; i < rows; i++) {
  
    in = image + j * chunk +  vert_table_8_up[i].source * row_bytes;
    out = tmp_image + m * row_bytes;

    if (in >= last_row){
      ac_memcpy(out,in,row_bytes);
    } else {
      ac_rescale(in, in+row_bytes, out, row_bytes,
		 vert_table_8_up[i].weight1, vert_table_8_up[i].weight2);
    }
    ++m;
  }

  ac_memcpy(image, tmp_image, n_height*width*3);
  return;
}


void rgb_hresize_8(char *image, int width, int height, int resize)
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
	
	in  = image + j * blocks * 3 + hori_table_8[i].source * 3;
	out = image + m;

	out[0] = (in[0] * hori_table_8[i].weight1
		+ in[3] * hori_table_8[i].weight2 + 32768) >> 16;
	out[1] = (in[1] * hori_table_8[i].weight1
		+ in[4] * hori_table_8[i].weight2 + 32768) >> 16;
	out[2] = (in[2] * hori_table_8[i].weight1
		+ in[5] * hori_table_8[i].weight2 + 32768) >> 16;

	m+=3;
      }
    }      
    
    return;
}


void rgb_hresize_8_up(char *image, char *tmp_image, int width, int height, int resize)
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
	in  = image + incr *3;
	out = tmp_image + m;
	
	if (((incr+1) % width) == 0)
	    ac_memcpy(out, in, 3);
	else {
	    out[0] = (in[0] * hori_table_8[i].weight1
		    + in[3] * hori_table_8[i].weight2 + 32768) >> 16;
	    out[1] = (in[1] * hori_table_8[i].weight1
		    + in[4] * hori_table_8[i].weight2 + 32768) >> 16;
	    out[2] = (in[2] * hori_table_8[i].weight1
		    + in[5] * hori_table_8[i].weight2 + 32768) >> 16;
	}

	m+=3;
      }
    }      
    
    ac_memcpy(image, tmp_image, height*n_width*3);    
    return;
}


static inline void rgb_deinterlace_linear_blend_core(char *image, char *tmp, int width, int height)
{
}


void rgb_deinterlace_linear(char *image, int width, int height)
{
    char *in, *out;
    
    unsigned int y, block; 
    
    block = width * 3;

    in  = image;
    out = image;
    
    // convert half frame to full frame by simple interpolation
      
    out +=block;
    
    for (y = 0; y < (height>>1)-1; y++) {
      
      ac_average(in, in+(block<<1), out, block);
      
      in  += block<<1;
      out += block<<1;
    }
}

void rgb_deinterlace_linear_blend(char *image, char *tmp, int width, int height)
{
  char *in, *out;
  
  unsigned int y, block; 
  
  block = 3*width;
  
  //(1)
  //copy frame to 2. internal frame buffer

  // EMS
  // memcpy_accel(tmp, image, block*height);
  ac_memcpy(tmp, image, block*height);

  //(2)
  //convert first field to full frame by simple interpolation
  //row(1)=(row(0)+row(2))/2
  //row(3)=(row(2)+row(4))/2
  //...


  in  = image;
  out = image;
  
  out +=block;
  
  for (y = 0; y < (height>>1)-1; y++) {
    
    ac_average(in, in+(block<<1), out, block);
    
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
    
    ac_average(in, in+(block<<1), out, block);
    
    in  += block<<1;
    out += block<<1;
  }
  
  //(4)
  //blend both frames
  ac_average(image, tmp, image, block*height);
  
  return;
}


inline void rgb_decolor(char *image, int bytes)
{
    unsigned int y;
    unsigned short tmp;
    
    double ftmp;
    
    for (y = 0; y<bytes; y=y+3) {
	ftmp = YRED * (unsigned char) image[y] + YGREEN *(unsigned char) image[y+1] + YBLUE * (unsigned char) image[y+2]; 
	
	tmp = (unsigned short) ftmp;
	
	image[y]   = tmp & 0xff;
	image[y+1] = tmp & 0xff;
	image[y+2] = tmp & 0xff;
    }
    
    return;
}


inline static int samecolor(char *color1, char *color2, int bytes)
{
  
  int i;
  unsigned short maxdiff=0, diff;
  
  for(i=0; i<bytes; i++) {
    diff = abs((unsigned char) color1[i] - (unsigned char) color2[i]);
    if (diff > maxdiff) 
      maxdiff = diff;
  }
  
  return (maxdiff < s_threshold);
}

inline static int diffcolor(char *color, char *color1, char *color2, int bytes)
{
  int i;
  unsigned short maxdiff=0, diff;
  
  for(i=0; i<bytes; i++) {
    diff = abs((unsigned char) color[i] - (unsigned char) color1[i]);
    if (diff > maxdiff) 
      maxdiff = diff;
  }
  if (maxdiff < d_threshold)
    return TC_FALSE; 
  
  maxdiff=0;
  
  for(i=0; i<bytes; i++) {
    diff = abs((unsigned char)color[i] - (unsigned char)color2[i]);
    if (diff > maxdiff) 
      maxdiff = diff;
  }
  return (maxdiff > d_threshold);
}


#define NORTH (src_ptr - srowstride)
#define SOUTH (src_ptr + srowstride)
#define EAST  (src_ptr + bpp)
#define WEST  (src_ptr - bpp)


static void antialias(char *inrow, char *outrow, int pixels)
{
    
  // bytes per pixel
  const unsigned short bpp = BPP/8;
  
  unsigned char *dest_ptr, *src_ptr;
  
  unsigned int i, j;
  
  //bytes per row
  unsigned int srowstride=(pixels+2)*bpp;  
  
  unsigned long tmp;
  
  src_ptr  = inrow;
  dest_ptr = outrow;
  
  
  for(i=0; i < pixels; ++i) {
    
    //byte test
    if ((samecolor(WEST,NORTH,bpp) && 
	 diffcolor(WEST,SOUTH,EAST,bpp)) ||
	(samecolor(WEST,SOUTH,bpp) &&
	 diffcolor(WEST,NORTH,EAST,bpp)) ||
	(samecolor(EAST,NORTH,bpp) && 
	 diffcolor(EAST,SOUTH,WEST,bpp)) ||
	(samecolor(EAST,SOUTH,bpp) &&
	 diffcolor(EAST,NORTH,WEST,bpp))) {
      
	// pixel bytes loop
	for (j=0; j<bpp; ++j) {
	    
	    tmp = aa_table_c[*src_ptr] 
		+ aa_table_d[*(src_ptr-srowstride-bpp)]  
		+ aa_table_y[*(src_ptr-srowstride)]   
		+ aa_table_d[*(src_ptr-srowstride+bpp)] 
		+ aa_table_x[*(src_ptr-bpp)] 
		+ aa_table_x[*(src_ptr+bpp)] 
		+ aa_table_d[*(src_ptr+srowstride-bpp)] 
		+ aa_table_y[*(src_ptr+srowstride)] 
		+ aa_table_d[*(src_ptr+srowstride+bpp)];
	    
	    *dest_ptr = (verbose & TC_DEBUG) ? 255 & 0xff : (tmp>>16) & 0xff;
	    
	    dest_ptr++;
	    src_ptr++;
	} 
	
    } else { //elseif not aliasing 
	
	ac_memcpy(dest_ptr, src_ptr, bpp);
	dest_ptr +=bpp;
	src_ptr  +=bpp;
	
    } // endif not aliasing 	
  } // next pixel
  
  return;
}

#undef NORTH
#undef SOUTH
#undef EAST
#undef WEST


void rgb_antialias(char *image, char *dest, int width, int height, int mode)
{
    
    int i, j, block, pixels;
    
    char *in, *out;
    
    
    j = height >>3;
    
    block = width * 3;
    
    pixels = width-2;
    
    in  = image;
    out = dest;
    
    switch(mode) {
	
    case 2: // only process the new rows, that have been created by previous               // resize operation with maximum spacing
	
	
	for (i=0; i<height-1; ++i) {
	    
	    in  += block;
	    out += block;
	    
	    if (vert_table_8[i%j].dei) {
	      antialias(in+3, out+3, pixels);
	    
	      //first and last pixel untouched
	      *out = *in; 
	      *(out+1) = *(in+1); 
	      *(out+2) = *(in+2);
	      *(out+(pixels+1)*3) = *(in+(pixels+1)*3); 
	      *(out+(pixels+1)*3+1) = *(in+(pixels+1)*3+1); 
	      *(out+(pixels+1)*3+2) = *(in+(pixels+1)*3+2); 
	    } else 
	      //copy untouched row
	      ac_memcpy(out, in, width*3);
	}
	
	break;
	
    case 1: // process rows, created by deinterlace operation
	
	in+=block;
	
	for (i=0; i<(height>>1); ++i) {
	    antialias(in+3, out+3, pixels);

	    //first and last pixel untouched
	    *out = *in; 
	    *(out+1) = *(in+1); 
	    *(out+2) = *(in+2);
	    *(out+(pixels+1)*3) = *(in+(pixels+1)*3); 
	    *(out+(pixels+1)*3+1) = *(in+(pixels+1)*3+1); 
	    *(out+(pixels+1)*3+2) = *(in+(pixels+1)*3+2);
	    
	    //copy untouched row
	    ac_memcpy(out+block, in+block, width*3);

	    in  += block<<1;
	    out += block<<1;
	}	      
	
	break;

    case 3: // full frame processing
      
      for (i=0; i<height-1; ++i) {
	
	in  += block;
	out += block;
	
	antialias(in+3, out+3, pixels);
	
	//first and last pixel untouched
	
	*out = *in; 
	*(out+1) = *(in+1); 
	*(out+2) = *(in+2);
	*(out+(pixels+1)*3) = *(in+(pixels+1)*3); 
	*(out+(pixels+1)*3+1) = *(in+(pixels+1)*3+1); 
	*(out+(pixels+1)*3+2) = *(in+(pixels+1)*3+2);
      }	      
      
      break;
      
    case 0:
    default:
      break;
    }

    //first and last row untouched
    ac_memcpy(dest, image, width*3);
    ac_memcpy(dest+(height-1)*width*3, image+(height-1)*width*3, width*3);
    
    return;
}

static void rgb_zoom_done(void)
{
  int id;
  
  //get thread id:
  
  id=get_fthread_id(0);
 
  zoom_image_done(tbuf[id].zoomer);
  free(tbuf[id].tmpBuffer);
}

static void rgb_zoom_init(char *image, int width, int height, int new_width, int new_height)
{

  int id;

  vob_t *vob;
  
  //get thread id:
  
  id=get_fthread_id(0);
  
  tbuf[id].tmpBuffer = (pixel_t*)malloc(new_width*new_height*3);

    zoom_setup_image(&tbuf[id].srcImage, width, height, 3, image);
    zoom_setup_image(&tbuf[id].dstImage, new_width, new_height, 3, tbuf[id].tmpBuffer);

    vob = tc_get_vob();

    tbuf[id].zoomer = zoom_image_init(&tbuf[id].dstImage, &tbuf[id].srcImage, vob->zoom_filter, vob->zoom_support);
    
    atexit(rgb_zoom_done);
}


void rgb_zoom(char *image, int width, int height, int new_width, int new_height)
{

  int id;
  
  //get thread id:
  
  id=get_fthread_id(0);
  
  
  if (tbuf[id].zoomer == NULL)
    rgb_zoom_init(image, width, height, new_width, new_height);
  
  tbuf[id].srcImage.data = image;
  tbuf[id].dstImage.data = tbuf[id].tmpBuffer;
  zoom_image_process(tbuf[id].zoomer);
  tbuf[id].srcImage.data++;
  tbuf[id].dstImage.data++;
  zoom_image_process(tbuf[id].zoomer);
  tbuf[id].srcImage.data++;
  tbuf[id].dstImage.data++;
  zoom_image_process(tbuf[id].zoomer);
  
  ac_memcpy(image, tbuf[id].tmpBuffer, new_width*new_height*3);
}

static void rgb_zoom_done_DI(void)
{
  int id;
  
  //get thread id:
  
  id=get_fthread_id(1);
 
  zoom_image_done(tbuf_DI[id].zoomer);
  free(tbuf_DI[id].tmpBuffer);
}

static void rgb_zoom_init_DI(char *image, int width, int height, int new_width, int new_height, int id)
{

  vob_t *vob;

  tbuf_DI[id].tmpBuffer = (pixel_t*)malloc(new_width*new_height*3);
  
  zoom_setup_image(&tbuf_DI[id].srcImage, width, height, 3, image);
  zoom_setup_image(&tbuf_DI[id].dstImage, new_width, new_height, 3, tbuf_DI[id].tmpBuffer);
  
  vob = tc_get_vob();
  
  tbuf_DI[id].zoomer = zoom_image_init(&tbuf_DI[id].dstImage, &tbuf_DI[id].srcImage, vob->zoom_filter, vob->zoom_support);
  
  atexit(rgb_zoom_done_DI);
}


void rgb_zoom_DI(char *image, int width, int height, int new_width, int new_height)
{

  int id;
  
  //get thread id:
  
  id=get_fthread_id(1);
  
  
  if (tbuf_DI[id].zoomer == NULL)
    rgb_zoom_init_DI(image, width, height, new_width, new_height, id);
  
  tbuf_DI[id].srcImage.data = image;
  tbuf_DI[id].dstImage.data = tbuf_DI[id].tmpBuffer;
  zoom_image_process(tbuf_DI[id].zoomer);
  tbuf_DI[id].srcImage.data++;
  tbuf_DI[id].dstImage.data++;
  zoom_image_process(tbuf_DI[id].zoomer);
  tbuf_DI[id].srcImage.data++;
  tbuf_DI[id].dstImage.data++;
  zoom_image_process(tbuf_DI[id].zoomer);
  
  ac_memcpy(image, tbuf_DI[id].tmpBuffer, new_width*new_height*3);
}


void rgb_gamma(char *image, int len)
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

void deinterlace_rgb_zoom(unsigned char *src, int width, int height)
{

    char *in, *out;

    int i, block;

    // move first field into first half of frame buffer 

    block = 3*width;

    in  = src;
    out = src;

    //move every second row
    for (i=0; i<height; i=i+2) {
	
	ac_memcpy(out, in, block);
	in  += 2*block;
	out += block;
    }

    //high quality zoom out
    rgb_zoom_DI(src, width, height/2, width, height);
  
}

void deinterlace_rgb_nozoom(unsigned char *src, int width, int height)
{

    char *in, *out;

    int i, block;

    // move first field into first half of frame buffer 

    block = 3*width;

    in  = src;
    out = src;

    //move every second row
    for (i=0; i<height; i=i+2) {
	
	ac_memcpy(out, in, block);
	in  += 2*block;
	out += block;
    }
}
