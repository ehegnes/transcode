/*
 *  export_jpg.c
 *
 *  Copyright (C) Tilmann Bitterberg - September 2002
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

#include <stdio.h>
#include <stdlib.h>

#include "jpeglib.h"

/* quirk: jpeglib.h defines HAVE_STDLIB_H and config.h too */
#if defined(HAVE_STDLIB_H)
#undef HAVE_STDLIB_H
#endif

#include "transcode.h"
#include "yuv2rgb.h"


#define MOD_NAME    "export_jpg.so"
#define MOD_VERSION "v0.1.0 (2003-06-05)"
#define MOD_CODEC   "(video) *"

#define MOD_PRE jpg
#include "export_def.h"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_YUV|TC_CAP_RGB|TC_CAP_PCM|TC_CAP_AUD;

static char buf2[PATH_MAX];

static uint8_t tmp_buffer[SIZE_RGB_FRAME];

static int codec, width, height, row_bytes;

static int counter=0;
static char *prefix="frame.";
static int jpeg_quality =0;

static int interval=1;
static unsigned int int_counter=0;

JSAMPLE * image_buffer;	/* Points to large array of R,G,B-order data */

/* ------------------------------------------------------------ 
 *
 * init codec
 *
 * ------------------------------------------------------------*/

void write_JPEG_file (char * filename, int quality, int width, int height)
{
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  /* More stuff */
  FILE * outfile;		/* target file */
  JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
  int row_stride;		/* physical row width in image buffer */

  /* Step 1: allocate and initialize JPEG compression object */

  cinfo.err = jpeg_std_error(&jerr);
  /* Now we can initialize the JPEG compression object. */
  jpeg_create_compress(&cinfo);

  /* Step 2: specify data destination (eg, a file) */
  /* Note: steps 2 and 3 can be done in either order. */

  if ((outfile = fopen(filename, "wb")) == NULL) {
    fprintf(stderr, "can't open %s\n", filename);
    exit(1);
  }
  jpeg_stdio_dest(&cinfo, outfile);

  /* Step 3: set parameters for compression */

  /* First we supply a description of the input image.
   * Four fields of the cinfo struct must be filled in:
   */
  cinfo.image_width = width; 	/* image width and height, in pixels */
  cinfo.image_height = height;
  cinfo.input_components = 3;		/* # of color components per pixel */
  cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */

  jpeg_set_defaults(&cinfo);
  /* Now you can set any non-default parameters you wish to.
   * Here we just illustrate the use of quality (quantization table) scaling:
   */
  jpeg_set_quality(&cinfo, quality, TRUE); /* limit to baseline-JPEG values */

  /* Step 4: Start compressor */

  /* TRUE ensures that we will write a complete interchange-JPEG file.
   * Pass TRUE unless you are very sure of what you're doing.
   */
  jpeg_start_compress(&cinfo, TRUE);

  /* Step 5: while (scan lines remain to be written) */
  /*           jpeg_write_scanlines(...); */

  row_stride = cinfo.image_width * 3;	/* JSAMPLEs per row in image_buffer */

  while (cinfo.next_scanline < cinfo.image_height) {
    row_pointer[0] = & image_buffer[cinfo.next_scanline * row_stride];
    (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  /* Step 6: Finish compression */

  jpeg_finish_compress(&cinfo);
  /* After finish_compress, we can close the output file. */
  fclose(outfile);

  /* Step 7: release JPEG compression object */

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_compress(&cinfo);

}

MOD_init
{
    
    /* set the 'spit-out-frame' interval */
    interval = vob->frame_interval;
    
    if(param->flag == TC_VIDEO) {

      width = vob->ex_v_width;
      height = vob->ex_v_height;
      
      codec = (vob->im_v_codec == CODEC_YUV) ? CODEC_YUV:CODEC_RGB;

      if(vob->im_v_codec == CODEC_YUV) {
	yuv2rgb_init (vob->v_bpp, MODE_RGB);
	row_bytes = vob->v_bpp/8 * vob->ex_v_width;
      }
      
      
      return(0);
    }

    if(param->flag == TC_AUDIO) return(0);

    // invalid flag
    return(TC_EXPORT_ERROR); 
}

/* ------------------------------------------------------------ 
 *
 * open outputfile
 *
 * ------------------------------------------------------------*/

MOD_open
{
  
    if(param->flag == TC_VIDEO) {
      
      // video
      
	switch(vob->im_v_codec) {

	case CODEC_YUV:
	case CODEC_RGB:
	  
	  if(vob->video_out_file!=NULL && strcmp(vob->video_out_file,"/dev/null")!=0) prefix=vob->video_out_file;
	  
	  break;
	  
	default:
	  
	  fprintf(stderr, "[%s] codec not supported\n", MOD_NAME);
	  return(TC_EXPORT_ERROR); 
	  
	  break;
	}

	if(vob->ex_v_fcc != NULL && strlen(vob->ex_v_fcc) != 0) {
	  jpeg_quality=atoi(vob->ex_v_fcc);
	  if (jpeg_quality<=0) jpeg_quality = 75;
	  if (jpeg_quality>100) jpeg_quality = 100;
	} else {
	  jpeg_quality=75;
	}

	return(0);
    }
    
    
    if(param->flag == TC_AUDIO) return(0);
    
    // invalid flag
    return(TC_EXPORT_ERROR); 
}   

/* ------------------------------------------------------------ 
 *
 * encode and export
 *
 * ------------------------------------------------------------*/

MOD_encode
{
  
  char *out_buffer = param->buffer;

  if ((++int_counter-1) % interval != 0)
      return (0);

  if(param->flag == TC_VIDEO) { 


    if(((unsigned) snprintf(buf2, PATH_MAX, "%s%06d.%s", prefix, counter++, "jpg")>=PATH_MAX)) {
      perror("cmd buffer overflow");
      return(TC_EXPORT_ERROR);
    } 
    
    if(codec==CODEC_YUV) {
      yuv2rgb (tmp_buffer, 
	       param->buffer, 
	       param->buffer+5*width*height/4, 
	       param->buffer+width*height, 
	       width, height, row_bytes, width, width/2);
      
      out_buffer = tmp_buffer;
    }
    
    image_buffer = out_buffer;
    write_JPEG_file(buf2, jpeg_quality, width, height);
    
    return(0);
  }
  
  if(param->flag == TC_AUDIO) return(0);
  
  // invalid flag
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * stop encoder
 *
 * ------------------------------------------------------------*/

MOD_stop 
{
  
  if(param->flag == TC_VIDEO) {
    return(0);
  }

  if(param->flag == TC_AUDIO) return(0);
  
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * close outputfiles
 *
 * ------------------------------------------------------------*/

MOD_close
{  

    if(param->flag == TC_AUDIO) return(0);
    if(param->flag == TC_VIDEO) return(0);
    
    return(TC_EXPORT_ERROR);  
    
}

