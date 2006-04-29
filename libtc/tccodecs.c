/*
 * tccodecs.c - codecs helper functions
 * (C) 2005 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "libtc.h"
#include "src/transcode.h"
#include "tccodecs.h"

#include <string.h>

/* internal usage only ***************************************************/

typedef struct {
    uint32_t id;         /* a TC_CODEC_* vlaue */
    const char *name;    /* usually != fourcc */
    const char *fourcc;  /* real-world fourcc */
    const char *comment;
    int multipass;       /* multipass capable */
} TCCodecInfo;

/*
 * this table is *always* accessed in RO mode, so there is no need
 * to protect it with threading locks
 */
const TCCodecInfo tc_codecs_info[] = {
    /* video codecs */
    { TC_CODEC_RGB, "rgb", "RGB", "RGB uncompressed", 0 },
    { TC_CODEC_YUV420P, "yuv420p", "I420", "YUV 4:2:0 uncompressed", 0 },
    { TC_CODEC_YUV422P, "yuv422p", "UYVY", "YUV 4:2:2 uncompressed", 0 },
    { TC_CODEC_YUY2, "yuy2", "YUY2", "YUY2 uncompressed stream", 0 },
    // XXX: right fcc?
    { TC_CODEC_MPEG1VIDEO, "mpeg1video", "mpg1",
                           "MPEG 1 compliant video", 1 },
    { TC_CODEC_MPEG2VIDEO, "mpeg2video", "mpg2",
                           "MPEG 2 compliant video", 1 },
    { TC_CODEC_MPEG4VIDEO, "mpeg4video", "mpg4",                    // XXX
                           "MPEG 4 compliant video", 1 },
    { TC_CODEC_XVID, "xvid", "XVID", "MPEG 4 compliant video", 1 },
    { TC_CODEC_DIVX3, "divx3", "DIV3",
                      "old DivX 3 (msmpeg4v3) compatible video", 1 },
    { TC_CODEC_DIVX4, "divx4", "DIVX", "MPEG 4 compliant video", 1 }, // XXX
    { TC_CODEC_DIVX5, "divx5", "DX50", "MPEG 4 compliant video", 1 }, // XXX
    { TC_CODEC_MJPG, "mjpeg", "MJPG", "Motion JPEG", 0 },
    { TC_CODEC_DV, "dvvideo", "DVSD", "Digital Video", 0 },
    { TC_CODEC_LZO1, "lzo1", "LZO1",
                     "Fast lossless video codec version 1", 0 },
    { TC_CODEC_LZO2, "lzo2", "LZO2",
                     "Fast lossless video codec version 2", 0 },
    { TC_CODEC_MP42, "msmpeg4v2", "MP42",
                     "DivX3 compatible video (older version)", 1 },
    { TC_CODEC_RV10, "realvideo10", "RV10", "old RealVideo", 0 },
    { TC_CODEC_WMV1, "wmv1", "WMV1", "Windows Media Video v1", 1 },
    { TC_CODEC_WMV2, "wmv2", "WMV2", "Windows Media Video v2", 1 },
    { TC_CODEC_HUFFYUV, "huffyuv", "HFYU", "Lossless video", 1 },
    { TC_CODEC_H263P, "h.263+", "H263", "h.263 plus video ", 1 },
    // XXX: right fcc?
    { TC_CODEC_H263I, "h.263", "H263", "h.263 video", 0 },
    // XXX: right fcc?
    { TC_CODEC_FFV1, "ffv1", "FFV1",
                    "Experimental lossless ffmpeg codec", 1 },
    { TC_CODEC_ASV1, "asusvideo1", "ASV1", "ASUS video codec, v1", 0 },
    { TC_CODEC_ASV2, "asusvideo2", "ASV2", "ASUS video codec, v2", 0 },
    { TC_CODEC_H264, "h.264", "H264", "h.264 (AVC) video", 1 },
    /* FIXME: add more codec informations, on demand */

    /* audio codecs */
    { TC_CODEC_PCM, "pcm", NULL, NULL, 0 },
    { TC_CODEC_MP3, "mp3", NULL, NULL, 0 },
    { TC_CODEC_MP2, "mp2", NULL, NULL, 0 },
    { TC_CODEC_AC3, "ac3", NULL, NULL, 0 },
    { TC_CODEC_A52, "a52", NULL, NULL, 0 },
    { TC_CODEC_VORBIS, "vorbis", NULL, NULL, 0 },
    /* FIXME: add more codec informations, on demand */

    /* special codecs*/
    { TC_CODEC_ANY, "everything", NULL, NULL, 0 },
    { TC_CODEC_UNKNOWN, "unknown", NULL, NULL, 0 },
    { TC_CODEC_ERROR, "error", NULL, NULL, 0 }, // XXX
    /* this MUST be the last one */
};

/*
 * TCCodecMatcher:
 *      generic codec finder function family.
 *      tell if a TCCodecInfo descriptor match certains given criterias
 *      using a function-dependent method.
 *      See also 'find_tc_codec' function.
 *
 * Parameters:
 *      info: a pointer to a TCCodecInfo descriptor to be examinated
 *  userdata: a pointer to data with function-dependent meaning
 * Return Value:
 *      TC_TRUE if function succeed,
 *      TC_FALSE otherwise.
 */
typedef int (*TCCodecMatcher)(const TCCodecInfo *info, const void *userdata);

/*
 * id_matcher:
 *      match a TCCodecInfo descriptor on codec's id.
 *      'userdata' must be an *address* of an uint32_t containing a TC_CODEC_*
 *      to match.
 *
 * Parameters:
 *      as for TCCodecMatcher
 * Return Value:
 *      as for TCCodecMatcher
 */
static int id_matcher(const TCCodecInfo *info, const void *userdata)
{
    if (info == NULL || userdata == NULL) {
        return TC_FALSE;
    }

    return (*(int*)userdata == info->id) ?TC_TRUE :TC_FALSE;
}

/*
 * name_matcher:
 *      match a TCCodecInfo descriptor on codec's name (note: note != fourcc).
 *      'userdata' must be the C-string to match.
 *      Note: ignore case.
 *
 * Parameters:
 *      as for TCCodecMatcher
 * Return Value:
 *      as for TCCodecMatcher
 */
static int name_matcher(const TCCodecInfo *info, const void *userdata)
{
    if (info == NULL || userdata == NULL) {
        return TC_FALSE;
    }
    if(!info->name || (strcasecmp(info->name, userdata) != 0)) {
        return TC_FALSE;
    }
    return TC_TRUE;
}

/*
 * find_tc_codec:
 *      find a TCCodecInfo descriptor matching certains given criterias.
 *      It scans the whole TCCodecInfos table applying the given
 *      matcher with the given data to each element, halting when a match
 *      is found
 *
 * Parameters:
 *      matcher: a TCCodecMatcher to be applied to find the descriptor.
 *     userdata: matching data to be passed to matcher together with a table
 *               entry.
 *
 * Return Value:
 *      >= 0: index of an entry in TCCodecInfo in table if an entry match
 *            the finding criteria
 *      TC_NULL_MATCH if no entry matches the given criteria
 */
static int find_tc_codec(const TCCodecInfo *infos,
                         TCCodecMatcher matcher,
                         const void *userdata)
{
    int found = TC_FALSE, i = 0;

    if (infos == NULL) {
        return TC_NULL_MATCH;
    }

    for (i = 0; infos[i].id != TC_CODEC_ERROR; i++) {
        found = matcher(&infos[i], userdata);
        if (found) {
            break;
        }
    }
    if (!found) {
        i = TC_NULL_MATCH;
    }

    return i;
}

/* public API ************************************************************/

const char* tc_codec_to_string(int codec)
{
    int idx = find_tc_codec(tc_codecs_info, id_matcher, &codec);

    if (idx == TC_NULL_MATCH) { /* not found */
        return NULL;
    }
    return tc_codecs_info[idx].name; /* can be NULL */
}

int tc_codec_from_string(const char *codec)
{
    int idx = find_tc_codec(tc_codecs_info, name_matcher, codec);

    if (idx == TC_NULL_MATCH) { /* not found */
        return TC_CODEC_ERROR;
    }
    return tc_codecs_info[idx].id;
}

const char* tc_codec_fourcc(int codec)
{
    int idx = find_tc_codec(tc_codecs_info, id_matcher, &codec);

    if (idx == TC_NULL_MATCH) { /* not found */
        return NULL;
    }
    return tc_codecs_info[idx].fourcc; /* can be NULL */
}

int tc_codec_description(int codec, char *buf, size_t bufsize)
{
    int idx = find_tc_codec(tc_codecs_info, id_matcher, &codec);
    int ret;

    if (idx == TC_NULL_MATCH) { /* not found */
        return -1;
    }

    ret = tc_snprintf(buf, bufsize, "%-12s: (fourcc=%s multipass=%-3s) %s",
                      tc_codecs_info[idx].name,
                      tc_codecs_info[idx].fourcc,
                      tc_codecs_info[idx].multipass ?"yes" :"no",
                      tc_codecs_info[idx].comment);
    return ret;
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
