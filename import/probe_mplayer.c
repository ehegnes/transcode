/*
 * probe_mplayer.c: probe stream data using external mplayer binary
 * Written by Francesco Romani <fromani -at- gmail -dot- com>
 * (C) 2005 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the
 * GNU General Public License (version 2 or later).
 * See the file COPYING for details.
 */

#include "transcode.h"
#include "libtc.h"
#include "ioaux.h"

#include <stdio.h>
#include <string.h>

#define P_BUF_SIZE    (1024)
#define LINE_SIZE     (256)
#define LINE_MIN_LEN  (10) /* = strlen("ID_LENGTH=") */

#define TAG_VBITRATE    "ID_VIDEO_BITRATE"
#define TAG_WIDTH       "ID_VIDEO_WIDTH"
#define TAG_HEIGHT      "ID_VIDEO_HEIGHT"
#define TAG_FPS         "ID_VIDEO_FPS"
#define TAG_ASR         "ID_VIDEO_ASPECT"
#define TAG_ABITRATE    "ID_AUDIO_BITRATE"
#define TAG_ARATE       "ID_AUDIO_RATE"
#define TAG_ACHANS      "ID_AUDIO_NCH"


#define VAL_SEP         '=' /* single character! */

static int fetch_val_int(const char *line)
{
    const char *pc = strchr(line, VAL_SEP);
    if (pc != NULL && (pc + 1) != NULL) {
        return atoi(pc + 1);
    }
    return -1;
}

static double fetch_val_double(const char *line)
{
    const char *pc = strchr(line, VAL_SEP);
    if (pc != NULL && (pc + 1) != NULL) {
        return atof(pc + 1);
    }
    return -1.0;
}

static int is_identify_line(const char *line)
{
    size_t len = 0;
    if (!line) {
        return 0;
    }
    
    len = strlen(line);
    if (len < LINE_MIN_LEN) {
        return 0;
    }
    
    if (line[0] == 'I' && line[1] == 'D' && line[2] == '_') {
        return 1;
    }
    return 0;
}    

static void parse_identify_line(const char *line, probe_info_t *info)
{
    int do_audio = 0;
    
    if (!line || !info) {
        return;
    }

    if (0 == strncmp(TAG_VBITRATE, line, strlen(TAG_VBITRATE))) {
        info->bitrate = fetch_val_int(line) / 1000;
    } else if (0 == strncmp(TAG_WIDTH, line, strlen(TAG_WIDTH))) {
        info->width = fetch_val_int(line);
    } else if (0 == strncmp(TAG_HEIGHT, line, strlen(TAG_HEIGHT))) {
        info->height = fetch_val_int(line);
    } else if (0 == strncmp(TAG_FPS, line, strlen(TAG_FPS))) {
        info->fps = fetch_val_double(line);
        info->frc = fps2frc(info->fps);
    } else if (0 == strncmp(TAG_ABITRATE, line, strlen(TAG_ABITRATE))) {
        info->track[0].bits = fetch_val_int(line) / 1000;
        do_audio = 1;
    } else if (0 == strncmp(TAG_ACHANS, line, strlen(TAG_ACHANS))) {
        info->track[0].chan = fetch_val_int(line);
        do_audio = 1;
    } else if(0 == strncmp(TAG_ARATE, line, strlen(TAG_ARATE))) {
        info->track[0].samplerate = fetch_val_int(line);
        do_audio = 1;
    }
    
    if(do_audio) {
        /* common audio settings */
        info->track[0].format = 0x1; /* PCM */
        info->num_tracks = 1;
    }
}

void probe_mplayer(info_t *ipipe)
{
    char probe_cmd_buf[P_BUF_SIZE] = { 0, };
    FILE *pipefd = NULL;
    int ret;

    if (!ipipe) {
        /* should never happen */
        return;
    }
   
    if(tc_test_program("mplayer") != 0) {
        tc_log_error(__FILE__, "probe aborted: mplayer binary not found.");
        return;
    }
    
    ipipe->probe_info->codec = TC_CODEC_UNKNOWN;
    /* otherwise we don't use mplayer... */
    
    ret = tc_snprintf(probe_cmd_buf, P_BUF_SIZE, 
                "mplayer -quiet -identify -ao null "
                "-vo null -frames 0 %s 2> /dev/null",
                ipipe->name);
    if (ret < 0) {
        return;
    }
    
    pipefd = popen(probe_cmd_buf, "r");
    if (pipefd != NULL) {
        char line_buf[LINE_SIZE] = { 0, };
        
        /* if mplayer runs, we are pretty confident to get right results */
        while(fgets(line_buf, LINE_SIZE, pipefd) != NULL) {
            line_buf[LINE_SIZE -  1] = '\0';
            if (is_identify_line(line_buf)) {
                parse_identify_line(line_buf, ipipe->probe_info);
            }
        }
        pclose(pipefd);
        
        /* final adjustement */
        ipipe->probe_info->magic = TC_MAGIC_MPLAYER;
    } else {
        /* error in probing */
        ipipe->error = 1;
        ipipe->probe_info->magic = TC_MAGIC_UNKNOWN;
    }
    return;
}
