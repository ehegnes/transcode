/*
 *  import_im.c
 *
 *  Copyright (C) Thomas Oestreich - June 2001
 *  port to MagickWand API:
 *  Copyright (C) Francesco Romani - July 2007
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
#define MOD_VERSION "v0.1.0 (2007-07-17)"
#define MOD_CODEC   "(video) RGB"

/* Note: because of ImageMagick bogosity, this must be included first, so
 * we can undefine the PACKAGE_* symbols it splats into our namespace */
#include <wand/MagickWand.h>
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#include "transcode.h"
#include "libtc/optstr.h"

#include <stdlib.h>
#include <stdio.h>

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_RGB | TC_CAP_VID;

#define MOD_PRE im
#include "import_def.h"

#include <time.h>
#include <sys/types.h>
#include <regex.h>


static char *head = NULL, *tail = NULL;
static int first_frame = 0, last_frame = 0, current_frame = 0, pad = 0;
static int width = 0, height = 0;
static MagickWand *wand = NULL;
static int auto_seq_read = TC_TRUE; 
/* 
 * automagically read further images with filename like the first one 
 * enabled by default for backward compatibility, but obsoleted
 * by core option --directory_mode
 */

static int TCHandleMagickError(MagickWand *wand)
{
    ExceptionType severity;
    const char *description = MagickGetException(wand, &severity);

    tc_log_error(MOD_NAME, "%s", description);

    MagickRelinquishMemory((void*)description);
    return TC_IMPORT_ERROR;
}


/* ------------------------------------------------------------
 *
 * open stream
 *
 * ------------------------------------------------------------*/


/* I suspect we have a lot of potential memleaks in here -- FRomani */
MOD_open
{
    int result, string_length;
    long sret;
    char *regex, *frame, *filename;
    char printfspec[20];
    regex_t preg;
    regmatch_t pmatch[4];

    if (param->flag == TC_AUDIO) {
        return TC_IMPORT_OK;
    }

    if (param->flag == TC_VIDEO) {
        param->fd = NULL;

        // get the frame name and range
        regex = "\\(.\\+[-._]\\)\\?\\([0-9]\\+\\)\\([-._].\\+\\)\\?";
        result = regcomp(&preg, regex, 0);
        if (result) {
            tc_log_perror(MOD_NAME, "ERROR:  Regex compile failed.\n");
            return TC_IMPORT_ERROR;
        }

        result = regexec(&preg, vob->video_in_file, 4, pmatch, 0);
        if (result) {
            tc_log_warn(MOD_NAME, "Regex match failed: no image sequence");
            string_length = strlen(vob->video_in_file) + 1;
            head = tc_malloc(string_length);
            if (head == NULL) {
                tc_log_perror(MOD_NAME, "filename head");
                return TC_IMPORT_ERROR;
            }
            strlcpy(head, vob->video_in_file, string_length);
            tail = tc_malloc(1); /* URGH -- FRomani */
            tail[0] = 0;
            first_frame = -1;
            last_frame = 0x7fffffff;
        } else {
            // split the name into head, frame number, and tail
            string_length = pmatch[1].rm_eo - pmatch[1].rm_so + 1;
            head = tc_malloc(string_length);
            if (head == NULL) {
                tc_log_perror(MOD_NAME, "filename head");
                return TC_IMPORT_ERROR;
            }
            strlcpy(head, vob->video_in_file, string_length);

            string_length = pmatch[2].rm_eo - pmatch[2].rm_so + 1;
            frame = tc_malloc(string_length);
            if (frame == NULL) {
                tc_log_perror(MOD_NAME, "filename frame");
                return TC_IMPORT_ERROR;
            }
            strlcpy(frame, vob->video_in_file + pmatch[2].rm_so, string_length);

            // If the frame number is padded with zeros, record how many digits
            // are actually being used.
            if (frame[0] == '0') {
                pad = pmatch[2].rm_eo - pmatch[2].rm_so;
            }
            first_frame = atoi(frame);

            string_length = pmatch[3].rm_eo - pmatch[3].rm_so + 1;
            tail = tc_malloc(string_length);
            if (tail == NULL) {
                tc_log_perror(MOD_NAME, "filename tail");
                return TC_IMPORT_ERROR;
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
                  return TC_IMPORT_ERROR;
            } while (close(open(filename, O_RDONLY)) != -1); /* URGH -- Fromani */
            last_frame--;
            tc_free(filename);
            tc_free(frame);
        }

        current_frame = first_frame;

        if (vob->im_v_string != NULL) {
            if (optstr_lookup(vob->im_v_string, "noseq")) {
                auto_seq_read = TC_FALSE;
                tc_log_info(MOD_NAME, "automagic image sequential read disabled");
            }
        }
 
        width = vob->im_v_width;
        height = vob->im_v_height;

        MagickWandGenesis();
        wand = NewMagickWand();

        if (wand == NULL) {
            tc_log_error(MOD_NAME, "cannot create magick wand");
            return TC_IMPORT_ERROR;
        }

        return TC_IMPORT_OK;
    }

    return TC_IMPORT_ERROR;
}


/* ------------------------------------------------------------
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/

MOD_decode
{
    char *filename = NULL, *frame = NULL;
    int string_length;
    MagickBooleanType status;

    if (param->flag == TC_AUDIO) {
        return TC_IMPORT_OK;
    }

    if (param->flag == TC_VIDEO) {
        if (current_frame > last_frame)
            return TC_IMPORT_ERROR;

        if (!auto_seq_read) {
            strlcat(filename, vob->video_in_file, string_length);
        } else {
            // build the filename for the current frame
            string_length = strlen(head) + pad + strlen(tail) + 1;
            filename = tc_malloc(string_length);
            if (pad) {
                char framespec[10];
                frame = tc_malloc(pad+1);
                tc_snprintf(framespec, 10, "%%0%dd", pad);
                tc_snprintf(frame, pad+1, framespec, current_frame);
                frame[pad] = '\0';
            } else if (first_frame >= 0) {
                frame = tc_malloc(10);
                tc_snprintf(frame, 10, "%d", current_frame);
            }
            strlcpy(filename, head, string_length);
            if (frame != NULL) {
                strlcat(filename, frame, string_length);
                tc_free(frame);
                frame = NULL;
            }
            strlcat(filename, tail, string_length);
        }

        ClearMagickWand(wand);
        /* 
         * This avoids IM to buffer all read images.
         * I'm quite sure that this can be done in a smarter way,
         * but I haven't yet figured out how. -- FRomani
         */

        status = MagickReadImage(wand, filename);
        if (status == MagickFalse) {
            return TCHandleMagickError(wand);
        }

        MagickSetLastIterator(wand);

        status =  MagickGetImagePixels(wand,
                                       0, 0, width, height,
                                       "RGB", CharPixel,
                                       param->buffer);
        /* param->size already set correctly by caller */
        if (status == MagickFalse) {
            return TCHandleMagickError(wand);
        }

        if (current_frame == first_frame)
            param->attributes |= TC_FRAME_IS_KEYFRAME;

        current_frame++;
    
        tc_free(filename);

        return TC_IMPORT_OK;
    }
    return TC_IMPORT_ERROR;
}

/* ------------------------------------------------------------
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{
    if (param->flag == TC_AUDIO) {
        return TC_IMPORT_OK;
    }

    if (param->flag == TC_VIDEO) {
        if (param->fd != NULL)
            pclose(param->fd);
        if (head != NULL)
            tc_free(head);
        if (tail != NULL)
            tc_free(tail);

        if (wand != NULL) {
            DestroyMagickWand(wand);
            MagickWandTerminus();
            wand = NULL;
        }
        return TC_IMPORT_OK;
    }
    return TC_IMPORT_ERROR;
}

/*************************************************************************/

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
