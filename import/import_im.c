/*
 *  import_im.c
 *
 *  Copyright (C) Thomas Oestreich - June 2001
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

#define MOD_NAME    "import_im.so"
#define MOD_VERSION "v0.0.4 (2003-09-15)"
#define MOD_CODEC   "(video) RGB"

#include "transcode.h"

#include <stdlib.h>
#include <stdio.h>

#define _MAGICKCORE_CONFIG_H  // to avoid conflicts with our config.h
#include <magick/api.h>

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_RGB | TC_CAP_VID;

#define MOD_PRE im
#include "import_def.h"

#include <time.h>
#include <sys/types.h>
#include <regex.h>


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
    result,
    string_length;

  long
    sret;

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
      return(TC_IMPORT_OK);
  }

  if(param->flag == TC_VIDEO) {

    param->fd = NULL;

    // get the frame name and range
    regex = "\\(.\\+[-._]\\)\\?\\([0-9]\\+\\)\\([-._].\\+\\)\\?";
    result = regcomp(&preg, regex, 0);
    if (result) {
        tc_log_perror(MOD_NAME, "ERROR:  Regex compile failed.\n");
        return(TC_IMPORT_ERROR);
    }

    result = regexec(&preg, vob->video_in_file, 4, pmatch, 0);
    if (result) {
        tc_log_warn(MOD_NAME, "Regex match failed: no image sequence");
	string_length = strlen(vob->video_in_file) + 1;
        if ((head = tc_malloc(string_length)) == NULL) {
	    tc_log_perror(MOD_NAME, "filename head");
	    return(TC_IMPORT_ERROR);
	}
	strlcpy(head, vob->video_in_file, string_length);
        tail = tc_malloc(1);
        tail[0] = 0;
        first_frame = -1;
        last_frame = 0x7fffffff;
    }
    else {
        // split the name into head, frame number, and tail
        string_length = pmatch[1].rm_eo - pmatch[1].rm_so + 1;
        if ((head = tc_malloc(string_length)) == NULL) {
            tc_log_perror(MOD_NAME, "filename head");
            return(TC_IMPORT_ERROR);
        }
        strlcpy(head, vob->video_in_file, string_length);

        string_length = pmatch[2].rm_eo - pmatch[2].rm_so + 1;
        if ((frame = tc_malloc(string_length)) == NULL) {
            tc_log_perror(MOD_NAME, "filename frame");
            return(TC_IMPORT_ERROR);
        }
        strlcpy(frame, vob->video_in_file + pmatch[2].rm_so, string_length);

        // If the frame number is padded with zeros, record how many digits
        // are actually being used.
        if (frame[0] == '0') {
            pad = pmatch[2].rm_eo - pmatch[2].rm_so;
        }
        first_frame = atoi(frame);

        string_length = pmatch[3].rm_eo - pmatch[3].rm_so + 1;
        if ((tail = tc_malloc(string_length)) == NULL) {
            tc_log_perror(MOD_NAME, "filename tail");
            return(TC_IMPORT_ERROR);
        }
        strlcpy(tail, vob->video_in_file + pmatch[3].rm_so, string_length);

        // find the last frame by trying to open files
        last_frame = first_frame;
        filename = tc_malloc(strlen(head) + pad + strlen(tail) + 1);
        /* why remalloc frame? */
        /* frame = malloc(pad + 1); */
        do {
            last_frame++;
            tc_snprintf(printfspec, sizeof(printfspec), "%%s%%0%dd%%s", pad);
            string_length = strlen(head) + pad + strlen(tail) + 1;
            sret = tc_snprintf(filename, string_length, printfspec, head,
                               last_frame, tail);
            if (sret < 0)
              return(TC_IMPORT_ERROR);
        } while (close(open(filename, O_RDONLY)) != -1);
        last_frame--;
        free(filename);
        free(frame);
    }

    current_frame = first_frame;

    // initialize ImageMagick
    InitializeMagick("");

    return(TC_IMPORT_OK);
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
        *frame = NULL;

    int
        column,
        row,
        string_length;


    if (current_frame > last_frame)
        return(TC_IMPORT_ERROR);

    // build the filename for the current frame
    string_length = strlen(head) + pad + strlen(tail) + 1;
    filename = tc_malloc(string_length);
    if (pad) {
        char framespec[10];
        frame = tc_malloc(pad+1);
        tc_snprintf(framespec, 10, "%%0%dd", pad);
        tc_snprintf(frame, pad+1, framespec, current_frame);
        frame[pad] = '\0';
    }
    else if (first_frame >= 0) {
        frame = tc_malloc(10);
        tc_snprintf(frame, 10, "%d", current_frame);
    }
    strlcpy(filename, head, string_length);
    if (frame != NULL) {
        strlcat(filename, frame, string_length);
        free(frame);
        frame = NULL;
    }
    strlcat(filename, tail, string_length);

    // Have ImageMagick open the file and read in the image data.
    GetExceptionInfo(&exception_info);
    image_info=CloneImageInfo((ImageInfo *) NULL);
    (void) strlcpy(image_info->filename, filename, MaxTextExtent);
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
               (char) (pixel_packet[(image->rows - row - 1) * image->columns +
                                                            column].red >> 8);
          param->buffer[(row * image->columns + column) * 3 + 1] =
               (char) (pixel_packet[(image->rows - row - 1) * image->columns +
                                                           column].green >> 8);
          param->buffer[(row * image->columns + column) * 3 + 2] =
               (char) (pixel_packet[(image->rows - row - 1) * image->columns +
                                                             column].blue >> 8);
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

    return(TC_IMPORT_OK);
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

  return(TC_IMPORT_OK);
}


