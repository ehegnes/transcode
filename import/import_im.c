/*
 *  import_im.c
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

#define MOD_NAME    "import_im.so"
#define MOD_VERSION "v0.0.4 (2003-09-15)"
#define MOD_CODEC   "(video) RGB"

#include "transcode.h"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_RGB | TC_CAP_VID;

#define MOD_PRE im
#include "import_def.h"

#include <time.h>
#include <sys/types.h>
#include <regex.h>

/* transcode defines these as well as ImageMagick. */
#undef PACKAGE_NAME
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_STRING

#include <magick/api.h>


#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

char
    *head = NULL,
    *tail = NULL;

int
    first_frame = 0,
    last_frame = 0,
    current_frame = 0,
    pad = 0;
 
  
/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  int
    result;

  char
      *regex,
      *frame,
      *filename;

  char
      printfspec[20];

  regex_t
        preg;

  regmatch_t
      pmatch[4];

  if(param->flag == TC_AUDIO) {
      return(0);
  }
  
  if(param->flag == TC_VIDEO) {

    param->fd = NULL;

    // get the frame name and range
    regex = "\\(.\\+[-._]\\)\\?\\([0-9]\\+\\)\\([-._].\\+\\)\\?";
    result = regcomp(&preg, regex, 0);
    if (result) {
        perror("ERROR:  Regex compile failed.\n");
        return(TC_IMPORT_ERROR);
    }

    result = regexec(&preg, vob->video_in_file, 4, pmatch, 0);
    if (result) {
        fprintf(stderr, "Regex match failed: no image sequence\n");
        head = malloc(strlen(vob->video_in_file) + 1);
	head = strcpy(head, vob->video_in_file);
        tail = malloc(1);
        tail[0] = 0;
        first_frame = -1;
        last_frame = 0x7fffffff;
    }
    else {
        // split the name into head, frame number, and tail
        head = malloc(pmatch[1].rm_eo - pmatch[1].rm_so + 1);
        head = strncpy(head, 
                       vob->video_in_file, 
                       pmatch[1].rm_eo - pmatch[1].rm_so);
        head[pmatch[1].rm_eo - pmatch[1].rm_so] = '\0';

        frame = malloc(pmatch[2].rm_eo - pmatch[2].rm_so + 1);
        frame = strncpy(frame, 
                        vob->video_in_file + pmatch[2].rm_so, 
                        pmatch[2].rm_eo - pmatch[2].rm_so);
        frame[pmatch[2].rm_eo - pmatch[2].rm_so] = '\0';

        // If the frame number is padded with zeros, record how many digits 
        // are actually being used.
        if (frame[0] == '0') {
            pad = pmatch[2].rm_eo - pmatch[2].rm_so;
        }
        first_frame = atoi(frame);

        tail = malloc(pmatch[3].rm_eo - pmatch[3].rm_so + 1);
        tail = strncpy(tail, 
               vob->video_in_file + pmatch[3].rm_so, 
               pmatch[3].rm_eo - pmatch[3].rm_so);
        tail[pmatch[3].rm_eo - pmatch[3].rm_so] = '\0';

        // find the last frame by trying to open files
        last_frame = first_frame; 
        filename = malloc(strlen(head) + pad + strlen(tail) + 1);
        frame = malloc(pad + 1);
        do {
            last_frame++;
            snprintf(printfspec, sizeof(printfspec), "%%s%%0%dd%%s", pad);
            snprintf(filename, strlen(head) + pad + strlen(tail) + 1,
                     printfspec, head, last_frame, tail);
        } while (close(open(filename, O_RDONLY)) != -1); 
        last_frame--;
        free(filename);
        free(frame);
    }

    current_frame = first_frame;

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
        *filename = NULL,
        *frame = NULL,
        *framespec = NULL;

    int
        column,
        row;


    if (current_frame > last_frame)
        return(TC_IMPORT_ERROR);

    // build the filename for the current frame
    filename = malloc(strlen(head) + pad + strlen(tail) + 1);
    if (pad) {
        frame = malloc(pad+1);
        framespec = malloc(10);
        snprintf(framespec, 10, "%%0%dd", pad);
        snprintf(frame, pad+1, framespec, current_frame);
        frame[pad] = '\0';
    }
    else if (first_frame >= 0) {
        frame = malloc(10);
        snprintf(frame, 10, "%d", current_frame);
    }
    strcpy(filename, head);
    if (frame != NULL) {
        strcpy(filename + strlen(head), frame);
        strcpy(filename + strlen(head) + strlen(frame), tail);
    }
    else
        strcpy(filename + strlen(head), tail);

    // Have ImageMagick open the file and read in the image data.
    GetExceptionInfo(&exception_info);
    image_info=CloneImageInfo((ImageInfo *) NULL);
    (void) strcpy(image_info->filename, filename);
    image=ReadImage(image_info,&exception_info);
    if (image == (Image *) NULL) {
        MagickError(exception_info.severity,
                    exception_info.reason,
                    exception_info.description);
	// skip
	return (TC_IMPORT_ERROR);
    }

    /*
     * Copy the pixels into a buffer in RGB order
     */
    pixel_packet = GetImagePixels(image, 0, 0, image->columns, image->rows);
    for (row = 0; row < image->rows; row++) {
        for (column = 0; column < image->columns; column++) {
          /*
           * The bit-shift 8 in the following lines is to convert 
           * 16-bit-per-channel images that may be read by ImageMagick 
           * into the 8-bit-per-channel images that transcode uses.
           * The bit-shift is still valid for 8-bit-per-channel images 
           * because when ImageMagick handles 8-bit images it still uses 
           * unsigned shorts, but stores the same 8-bit value in both 
           * the low and high byte.
           */
          param->buffer[(row * image->columns + column) * 3 + 0] =
               (char) (pixel_packet[(image->rows - row - 1) * image->columns + column].blue >> 8);
          param->buffer[(row * image->columns + column) * 3 + 1] =
               (char) (pixel_packet[(image->rows - row - 1) * image->columns + column].green >> 8);
          param->buffer[(row * image->columns + column) * 3 + 2] =
               (char) (pixel_packet[(image->rows - row - 1) * image->columns + column].red >> 8);
        }
    }

    if (current_frame == first_frame)
        param->attributes |= TC_FRAME_IS_KEYFRAME;

    current_frame++;

    // How do we do this?  The next line is not right (segfaults)
    // I can't find a DestroyPixelPacket() method.
    //free(pixel_packet);
    DestroyImage(image);
    DestroyImageInfo(image_info);
    DestroyExceptionInfo(&exception_info);
    free(filename);
    free(frame);

    return(0);
}

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{  
  if (param->fd != NULL) pclose(param->fd);
  if (head != NULL) free(head);
  if (tail != NULL) free(tail);

  DestroyMagick();

  return(0);
}


