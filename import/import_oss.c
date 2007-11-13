/*
 *  import_oss.c
 *
 *  Copyright (C) Jacob Meuser - September 2004
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

#define MOD_NAME	"import_oss.so"
#define MOD_VERSION	"v0.0.1 (2005-05-12)"
#define MOD_CODEC	"(audio) pcm"

#include "transcode.h"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_PCM;

#define MOD_PRE oss
#include "import_def.h"

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <sys/ioctl.h>
#ifdef HAVE_SYS_SOUNDCARD_H
# include <sys/soundcard.h>
#else
# ifdef HAVE_SOUNDCARD_H
#  include <soundcard.h>
# endif
#endif

#include "optstr.h"

static int oss_fd = -1;

int oss_init(const char *, int, int, int);
int oss_grab(size_t, char *);
int oss_stop(void);


int oss_init(const char *audio_device,
                    int sample_rate, int precision, int channels)
{
    int encoding;

    if (!strcmp(audio_device, "/dev/null") || !strcmp(audio_device, "/dev/zero"))
        return(0);

    if (precision != 8 && precision != 16) {
        fprintf(stderr,
            "[%s] bits/sample must be 8 or 16\n",
            MOD_NAME);
        return(1);
    }

    encoding = (precision == 8) ? AFMT_U8 : AFMT_S16_LE;

    if ((oss_fd = open(audio_device, O_RDONLY)) < 0) {
        perror(MOD_NAME "open audio device");
        return(1);
    }

    if (ioctl(oss_fd, SNDCTL_DSP_SETFMT, &encoding) < 0) {
        perror("SNDCTL_DSP_SETFMT");
        return(1);
    }

    if (ioctl(oss_fd, SNDCTL_DSP_CHANNELS, &channels) < 0) {
        perror("SNDCTL_DSP_CHANNELS");
        return(1);
    }

    if (ioctl(oss_fd, SNDCTL_DSP_SPEED, &sample_rate) < 0) {
        perror("SNDCTL_DSP_SPEED");
        return(1);
    }

    return(0);
}

int oss_grab(size_t size, char *buffer)
{
    int left;
    int offset;
    int received;

    for (left = size, offset = 0; left > 0;) {
        received = read(oss_fd, buffer + offset, left);
        if (received == 0) {
            fprintf(stderr,
                "[%s] audio grab: received == 0\n", MOD_NAME);
        }
        if (received < 0) {
            if (errno == EINTR) {
                received = 0;
            } else {
                perror(MOD_NAME "audio grab");
                return(1);
            }
        }
        if (received > left) {
            fprintf(stderr,
                "[%s] read returns more bytes than requested\n"
                "requested: %d, returned: %d\n",
                MOD_NAME, left, received);
            return(1);
        }
        offset += received;
        left -= received;
    }
    return(0);
}

int oss_stop(void)
{
    close(oss_fd);
    oss_fd = -1;

    if (verbose_flag & TC_STATS) {
        fprintf(stderr,
            "[%s] totals: (not implemented)", MOD_NAME);
    }

    return(0);
}


/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
    int ret = TC_IMPORT_OK;

    switch (param->flag) {
      case TC_VIDEO:
        fprintf(stderr,
            "[%s] unsupported request (init video)\n",
            MOD_NAME);
        ret = TC_IMPORT_ERROR;
        break;
      case TC_AUDIO:
        if (verbose_flag & TC_DEBUG) {
            fprintf(stderr,
                "[%s] OSS audio grabbing\n",
                MOD_NAME);
        }
        if (oss_init(vob->audio_in_file,
                      vob->a_rate, vob->a_bits, vob->a_chan)) {
            ret = TC_IMPORT_ERROR;
        }
        break;
      default:
        fprintf(stderr,
            "[%s] unsupported request (init)\n",
            MOD_NAME);
        ret = TC_IMPORT_ERROR;
        break;
    }

    return(ret);
}


/* ------------------------------------------------------------ 
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/

MOD_decode
{
    int ret = TC_IMPORT_OK;

    switch (param->flag) {
      case TC_VIDEO:
        fprintf(stderr,
            "[%s] unsupported request (decode video)\n",
            MOD_NAME);
        ret = TC_IMPORT_ERROR;
        break;
      case TC_AUDIO:
        if (oss_grab(param->size, param->buffer)) {
            fprintf(stderr,
                "[%s] error in grabbing audio\n",
                MOD_NAME);
            ret = TC_IMPORT_ERROR;
        }
        break;
      default:
        fprintf(stderr,
            "[%s] unsupported request (decode)\n",
            MOD_NAME);
        ret = TC_IMPORT_ERROR;
        break;
    }

    return(ret);
}

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{  
    int ret = TC_IMPORT_OK;

    switch (param->flag) {
      case TC_VIDEO:
        fprintf(stderr,
            "[%s] unsupported request (close video)\n",
            MOD_NAME);
        ret = TC_IMPORT_ERROR;
        break;
      case TC_AUDIO:
        oss_stop();
        break;
      default:
        fprintf(stderr,
            "[%s] unsupported request (close)\n",
            MOD_NAME);
        ret = TC_IMPORT_ERROR;
        break;
    }

    return(ret);
}
