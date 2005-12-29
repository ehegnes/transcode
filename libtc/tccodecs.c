/*
 * tccodecs.c - codecs helper functions
 * (C) 2005 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include <string.h>
#include <strings.h>
#include "src/transcode.h"
#include "tccodecs.h"

/* internal usage only ***************************************************/

typedef struct {
    int id;
    const char *name;
    const char *fourcc;
} TCCodecInfo;

/*
 * this table is *always* accessed in RO mode, so there is no need
 * to protect it with threading locks
 */
const TCCodecInfo codecs_info[] = {
    /* video codecs */
    { TC_CODEC_RGB, "rgb", "RGB" },
    { TC_CODEC_YUV420P, "yuv420p", "I420" },
    { TC_CODEC_YUV422P, "yuv422p", "UYVY" },
    { TC_CODEC_YUY2, "yuy2", "YUY2" }, // XXX: right fcc?
    { TC_CODEC_MPEG1, "mpeg1", "mpg1" },
    { TC_CODEC_MPEG2, "mpeg2", "mpg2" },
    { TC_CODEC_XVID, "xvid", "XVID" },
    { TC_CODEC_DIVX3, "divx3;-)", "DIVX" },
    { TC_CODEC_DIVX4, "divx4", "DIVX" },
    { TC_CODEC_DIVX5, "divx5", "DX50" },
    { TC_CODEC_MJPG, "mjpeg", "MJPG" },
    { TC_CODEC_DV, "dv", "DVSD" },
    { TC_CODEC_LZO1, "lzo1", "LZO1" },
    { TC_CODEC_LZO2, "lzo2", "LZO2" },
    /* FIXME: add more codec informations, on demand */

    /* audio codecs */
    { TC_CODEC_PCM, "pcm", NULL },
    { TC_CODEC_MP3, "mp3", NULL },
    { TC_CODEC_AC3, "ac3", NULL },
    { TC_CODEC_A52, "a52", NULL },
    /* FIXME: add more codec informations, on demand */

    /* special codecs*/
    { TC_CODEC_ANY, "everything", NULL },
    { TC_CODEC_UNKNOWN, "unknown", NULL },
    { TC_CODEC_ERROR, "error", NULL }, // XXX
                                            /* this MUST be the last one */
};

#define TC_CODEC_NOT_FOUND     -1

typedef int (*TCCodecFinder)(const TCCodecInfo *info, const void *userdata);

static int id_finder(const TCCodecInfo *info, const void *userdata)
{
    if (!info || !userdata) {
        return TC_FALSE;
    }

    return (*(int*)userdata == info->id) ?TC_TRUE :TC_FALSE;
}

static int name_finder(const TCCodecInfo *info, const void *userdata)
{
    if (!info || !userdata) {
        return TC_FALSE;
    }
    if(!info->name || (strcasecmp(info->name, userdata) != 0)) {
        return TC_FALSE;
    }
    return TC_TRUE;
}

#if 0  // not used
static int fourcc_finder(const TCCodecInfo *info, const void *userdata)
{
    if (!info || !userdata) {
        return TC_FALSE;
    }
    if(!info->fourcc || (strcasecmp(info->fourcc, userdata) != 0)) {
        return TC_FALSE;
    }
    return TC_TRUE;
}
#endif

static int find_tc_codec(const TCCodecInfo *infos, TCCodecFinder finder, const void *userdata)
{
    int found = TC_FALSE, i = 0;

    if (!infos) {
        return TC_CODEC_NOT_FOUND;
    }

    for (i = 0; infos[i].id != TC_CODEC_ERROR; i++) {
        found = finder(&infos[i], userdata);
        if (found) {
            break;
        }
    }
    if (!found) {
        i = TC_CODEC_NOT_FOUND; /* special index, meaning 'not found' */
    }

    return i;
}

/* public API ************************************************************/

const char* tc_codec_to_string(int codec)
{
    int idx = find_tc_codec(codecs_info, id_finder, &codec);

    if (idx == TC_CODEC_NOT_FOUND) { /* not found */
        return NULL;
    }
    return codecs_info[idx].name; /* can be NULL */
}

int tc_codec_from_string(const char *codec)
{
    int idx = find_tc_codec(codecs_info, name_finder, codec);

    if (idx == TC_CODEC_NOT_FOUND) { /* not found */
        return TC_CODEC_ERROR;
    }
    return codecs_info[idx].id;
}

const char* tc_codec_fourcc(int codec)
{
    int idx = find_tc_codec(codecs_info, id_finder, &codec);

    if (idx == TC_CODEC_NOT_FOUND) { /* not found */
        return NULL;
    }
    return codecs_info[idx].fourcc; /* can be NULL */
}

