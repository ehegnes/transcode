/*
 *  import_imlist.c
 *
 *  Copyright (C) Thomas Östreich - February 2002
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
#include <unistd.h>

#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "transcode.h"

// transcode defines this as well as ImageMagick.
#undef PACKAGE_NAME
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_STRING

#include <magick/api.h>

#define MOD_NAME    "import_imlist.so"
#define MOD_VERSION "v0.0.2 (2003-11-13)"
#define MOD_CODEC   "(video) RGB"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_RGB|TC_CAP_VID|TC_CAP_AUD;

#define MOD_PRE imlist
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

int
    first_frame = 0,
    last_frame = 0,
    current_frame = 0,
    pad = 0;

static FILE *fd; 
static char buffer[PATH_MAX+2];
  
/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  if(param->flag == TC_AUDIO) {
      return(0);
  }
  
  if(param->flag == TC_VIDEO) {

    param->fd = NULL;

    if((fd = fopen(vob->video_in_file, "r"))==NULL) return(TC_IMPORT_ERROR);
    
    // initialize ImageMagick
    InitializeMagick("");

    return(0);
  }
 
  return(TC_IMPORT_ERROR);
}


/* ------------------------------------------------------------ 
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/

MOD_decode {


    ExceptionInfo
        exception_info;

    ImageInfo
        *image_info;

    Image
        *image;

    PixelPacket
        *pixel_packet;

    char
        *filename = NULL;

    int
        column,
        row, n;

    if(param->flag == TC_AUDIO) return(0);
    
    // read a filename from the list
    if(fgets (buffer, PATH_MAX, fd)==NULL) return(TC_IMPORT_ERROR);    
    
    filename = buffer; 

    n=strlen(filename);
    if(n<2) return(TC_IMPORT_ERROR);  
    filename[n-1]='\0';
    
    // Have ImageMagick open the file and read in the image data.
    GetExceptionInfo(&exception_info);
    image_info=CloneImageInfo((ImageInfo *) NULL);
    (void) strcpy(image_info->filename, filename);

    image=ReadImage(image_info, &exception_info);
    if (image == (Image *) NULL) {
        MagickError(exception_info.severity,exception_info.reason,exception_info.description);
	// skipping
	return 0;
    }

    /*
     * Copy the pixels into a buffer in RGB order
     */
    pixel_packet = GetImagePixels(image, 0, 0, image->columns, image->rows);
    for (row = 0; row < image->rows; row++) {
        for (column = 0; column < image->columns; column++) {
          param->buffer[(row * image->columns + column) * 3 + 0] =
               pixel_packet[(image->rows - row - 1) * image->columns + column].blue;
          param->buffer[(row * image->columns + column) * 3 + 1] =
               pixel_packet[(image->rows - row - 1) * image->columns + column].green;
          param->buffer[(row * image->columns + column) * 3 + 2] =
               pixel_packet[(image->rows - row - 1) * image->columns + column].red;
        }
    }

    param->attributes |= TC_FRAME_IS_KEYFRAME;

    // How do we do this?  The next line is not right (segfaults)
    // I can't find a DestroyPixelPacket() method.
    //free(pixel_packet);
    DestroyImage(image);
    DestroyImageInfo(image_info);
    DestroyExceptionInfo(&exception_info);

    return(0);
}

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{  

  if(param->flag == TC_VIDEO) {
    
    if(fd != NULL) fclose(fd); fd = NULL;

    // This is very necessary
    DestroyMagick();

    
    return(0);
  }
  
  if(param->flag == TC_AUDIO) return(0);
 
  return(TC_IMPORT_ERROR);
}


