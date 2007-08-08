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
#define MOD_VERSION "v0.1.1 (2007-08-08)"
#define MOD_CODEC   "(video) RGB"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* Note: because of ImageMagick bogosity, this must be included first, so
 * we can undefine the PACKAGE_* symbols it splats into our namespace */
#ifdef HAVE_BROKEN_WAND
#include <wand/magick-wand.h>
#else /* we have a SANE wand header */
#include <wand/MagickWand.h>
#endif /* HAVE_BROKEN_WAND */

#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#include "transcode.h"

#include <stdlib.h>
#include <stdio.h>

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_RGB|TC_CAP_VID;

#define MOD_PRE im
#include "import_def.h"

#include <time.h>
#include <sys/types.h>
#include <regex.h>


static char *head = NULL, *tail = NULL;
static int first_frame = 0, current_frame = 0, pad = 0;
static int width = 0, height = 0;
static MagickWand *wand = NULL;

static int TCHandleMagickError(MagickWand *wand)
{
    ExceptionType severity;
    const char *description = MagickGetException(wand, &severity);

    fprintf(stderr, "[%s] %s\n", MOD_NAME, description);

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
    int result, slen = 0;
    long sret;
    char *regex = NULL, *frame = NULL;
    char printfspec[20];
    regex_t preg;
    regmatch_t pmatch[4];

    if (param->flag == TC_AUDIO) {
        return TC_IMPORT_OK;
    }

    if (param->flag == TC_VIDEO) {
        param->fd = NULL;

        // get the frame name and range
        regex = "\\([^0-9]\\+[-._]\\?\\)\\?\\([0-9]\\+\\)\\([-._].\\+\\)\\?";
        result = regcomp(&preg, regex, 0);
        if (result) {
            perror("regex compile");
            return TC_IMPORT_ERROR;
        }

        result = regexec(&preg, vob->video_in_file, 4, pmatch, 0);
        if (result) {
            fprintf(stderr, "[%s] regex match failed: no image sequence\n", MOD_NAME);
            slen = strlen(vob->video_in_file) + 1;
            head = malloc(slen);
            if (head == NULL) {
                perror("filename head");
                return TC_IMPORT_ERROR;
            }
            strlcpy(head, vob->video_in_file, slen);
            tail = malloc(1); /* URGH -- FRomani */
            tail[0] = 0;
            first_frame = -1;
            last_frame = 0x7fffffff;
        } else {
            // split the name into head, frame number, and tail
            slen = pmatch[1].rm_eo - pmatch[1].rm_so + 1;
            head = malloc(slen);
            if (head == NULL) {
                perror("filename head");
                return TC_IMPORT_ERROR;
            }
            strlcpy(head, vob->video_in_file, slen);

            slen = pmatch[2].rm_eo - pmatch[2].rm_so + 1;
            frame = malloc(slen);
            if (frame == NULL) {
                perror("filename frame");
                return TC_IMPORT_ERROR;
            }
            strlcpy(frame, vob->video_in_file + pmatch[2].rm_so, slen);

            // If the frame number is padded with zeros, record how many digits
            // are actually being used.
            if (frame[0] == '0') {
                pad = pmatch[2].rm_eo - pmatch[2].rm_so;
            }
            first_frame = atoi(frame);

            slen = pmatch[3].rm_eo - pmatch[3].rm_so + 1;
            tail = malloc(slen);
            if (tail == NULL) {
                perror("filename tail");
                return TC_IMPORT_ERROR;
            }
            strlcpy(tail, vob->video_in_file + pmatch[3].rm_so, slen);

            free(frame);
        }

        current_frame = first_frame;

        width = vob->im_v_width;
        height = vob->im_v_height;

        MagickWandGenesis();
        wand = NewMagickWand();

        if (wand == NULL) {
            fprintf(stderr, "[%s] cannot create magick wand\n", MOD_NAME);
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
    int slen;
    MagickBooleanType status;

    if (param->flag == TC_AUDIO) {
        return TC_IMPORT_OK;
    }

    if (param->flag == TC_VIDEO) {
        // build the filename for the current frame
        slen = strlen(head) + pad + strlen(tail) + 1;
        filename = malloc(slen);
        if (pad) {
            char framespec[10];
            frame = malloc(pad+1);
            snprintf(framespec, 10, "%%0%dd", pad);
            snprintf(frame, pad+1, framespec, current_frame);
            frame[pad] = '\0';
        } else if (first_frame >= 0) {
            frame = malloc(10);
            snprintf(frame, 10, "%d", current_frame);
        }
        strlcpy(filename, head, slen);
        if (frame != NULL) {
            strlcat(filename, frame, slen);
            free(frame);
            frame = NULL;
        }
        strlcat(filename, tail, slen);

        ClearMagickWand(wand);
        /* 
         * This avoids IM to buffer all read images.
         * I'm quite sure that this can be done in a smarter way,
         * but I haven't yet figured out how. -- FRomani
         */

        status = MagickReadImage(wand, filename);
        if (status == MagickFalse) {
            /* let's assume that image sequence ends here */
            return TC_IMPORT_ERROR;
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

        param->attributes |= TC_FRAME_IS_KEYFRAME;

        current_frame++;
    
        free(filename);

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
            free(head);
        if (tail != NULL)
            free(tail);

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
