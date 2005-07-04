/*
 *  import_null.c
 *
 *  Copyright (C) Jacob Meuser <jakemsr@jakemsr.com> - May 2005
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

#define MOD_NAME    "import_bsdav.so"
#define MOD_VERSION "v0.0.1 (2005-05-14)"
#define MOD_CODEC   "(video) raw | (audio) raw"

#include "transcode.h"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_AUD | TC_CAP_PCM |
    TC_CAP_VID| TC_CAP_RGB | TC_CAP_YUV | TC_CAP_YUY2 | TC_CAP_YUV422;

#define MOD_PRE bsdav
#include "import_def.h"

char import_cmd_buf[TC_BUF_MAX];

/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
int sret;

    verbose_flag = vob->verbose;

    if (tc_test_program("bsdavdemux") != 0)
        return(TC_IMPORT_ERROR);

    switch (param->flag) {
    case TC_VIDEO:

        if (verbose_flag & TC_DEBUG) {
            fprintf(stderr,
                "[%s] bsdav raw video\n",
                MOD_NAME);
        }

        sret = snprintf(import_cmd_buf, TC_BUF_MAX,
	  "bsdavdemux -i \"%s\" -o /dev/stdout", vob->video_in_file);
        if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
            return(TC_IMPORT_ERROR);

	if (verbose_flag & TC_INFO)
            printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

	if ((param->fd = popen(import_cmd_buf, "r")) == NULL) {
            perror("popen bsdav video stream");
            return(TC_IMPORT_ERROR);
        }
        break;

    case TC_AUDIO:

        if (verbose_flag & TC_DEBUG) {
            fprintf(stderr,
                "[%s] bsdav raw audio\n",
                MOD_NAME);
        }

        sret = snprintf(import_cmd_buf, TC_BUF_MAX,
	  "bsdavdemux -i \"%s\" -O /dev/stdout", vob->audio_in_file);
        if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
            return(TC_IMPORT_ERROR);

	if (verbose_flag & TC_INFO)
            printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

	if ((param->fd = popen(import_cmd_buf, "r")) == NULL) {
            perror("popen bsdav audio stream");
            return(TC_IMPORT_ERROR);
        }
        break;

    default:
        fprintf(stderr,
            "[%s] unsupported request (init)\n",
            MOD_NAME);
        return(TC_IMPORT_ERROR);
        break;
    }

    return(TC_IMPORT_OK);
}


/* ------------------------------------------------------------ 
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/

MOD_decode
{
    return(TC_IMPORT_OK);
}

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{
    switch (param->flag) {

    case TC_VIDEO:
        if (param->fd != NULL)
            pclose(param->fd);
        break;

    case TC_AUDIO:
        if (param->fd != NULL)
            pclose(param->fd);
        break;

    default:
        fprintf(stderr,
            "[%s] unsupported request (init)\n",
            MOD_NAME);
        return(TC_IMPORT_ERROR);
        break;

    }
    return(TC_IMPORT_OK);
}



