/*
 * tcmoduleinfo.c - module informations (capabilities) and helper functions
 * (C) 2005 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include <string.h>
#include "src/transcode.h"
#include "libtc.h"
#include "tccodecs.h"
#include "tcmodule-info.h"

int tc_module_info_match(const TCModuleInfo *head, const TCModuleInfo *tail)
{
    int found = 0;
    int i = 0, j = 0;

    /* we need a pair of valid references to go further */
    if (head == NULL || tail == NULL) {
        return 0;
    }

    /*
     * a multiplexor module can be chained with nothing,
     * it must be placed at the end of the chain
     */
    if (head->features & TC_MODULE_FEATURE_MULTIPLEX) {
        return 0;
    }

    for (i = 0; !found && tail->codecs_in[i] != TC_CODEC_ERROR; i++) {
        /* TC_CODEC_ANY fit with everything :) */
        if (tail->codecs_in[i] == TC_CODEC_ANY) {
            found = 1;
            break;
        }
        for (j = 0; head->codecs_out[j] != TC_CODEC_ERROR; j++) {
            if (head->codecs_out[j] == tail->codecs_in[i]) {
                found = 1;
                break;
            }
        }
    }
    return found;
}

#define DATA_BUF_SIZE   128

static void codecs_to_string(const int *codecs, char *buffer,
                             size_t bufsize, const char *fallback_string)
{
    int found = 0;
    int i = 0;

    if (!buffer || bufsize < DATA_BUF_SIZE) {
        return;
    }

    buffer[0] = '\0';

    for (i = 0; codecs[i] != TC_CODEC_ERROR; i++) {
        const char *codec = tc_codec_to_string(codecs[i]);
        if (!codec) {
            continue;
        }
        strlcat(buffer, codec, bufsize);
        strlcat(buffer, " ", bufsize);
        found = 1;
    }
    if (!found) {
        strlcpy(buffer, fallback_string, bufsize);
    }
}

void tc_module_info_log(const TCModuleInfo *info, int verbose)
{
    char buffer[DATA_BUF_SIZE];

    if (!info) {
        return;
    }
    if (!info->name || (!info->version || !info->description)) {
        tc_log_error(__FILE__, "missing critical information for module");
        return;
    }

    if (verbose >= TC_STATS) {
        tc_log_info(info->name, "description:\n%s", info->description);
    }

    if (verbose >= TC_DEBUG) {
        if (info->features == TC_MODULE_FEATURE_NONE) {
            strlcpy(buffer, "none (this shouldn't happen!", DATA_BUF_SIZE);
        } else {
            tc_snprintf(buffer, DATA_BUF_SIZE, "%s%s%s",
                  (info->features & TC_MODULE_FEATURE_VIDEO) ?"video " :"",
                  (info->features & TC_MODULE_FEATURE_AUDIO) ?"audio " :"",
                  (info->features & TC_MODULE_FEATURE_EXTRA) ?"extra" :"");
        }
        tc_log_info(info->name, "can handle : %s", buffer);

        if (info->features == TC_MODULE_FEATURE_NONE) {
            strlcpy(buffer, "nothing (this shouldn't happen!", DATA_BUF_SIZE);
        } else {
            tc_snprintf(buffer, DATA_BUF_SIZE, "%s%s%s",
                        (info->features & TC_MODULE_FEATURE_FILTER)
                            ?"filtering " :"",
                        (info->features & TC_MODULE_FEATURE_ENCODE)
                            ?"encoding " :"",
                        (info->features & TC_MODULE_FEATURE_MULTIPLEX)
                            ?"multiplexing" :"");
        }
        tc_log_info(info->name, "can do     : %s", buffer);
    }

    if (verbose >= TC_INFO) {
        const char *str = (info->features & TC_MODULE_FEATURE_MULTIPLEX)
                                    ?"a media stream" :"nothing";
        codecs_to_string(info->codecs_in, buffer, DATA_BUF_SIZE, str);
        tc_log_info(info->name, "accepts    : %s", buffer);

        codecs_to_string(info->codecs_out, buffer, DATA_BUF_SIZE, str);
        tc_log_info(info->name, "produces   : %s", buffer);
    }
}

int tc_module_info_copy(const TCModuleInfo *src, TCModuleInfo *dst)
{
    int i = 0;

    if (!src || !dst) {
        return -1;
    }
    dst->features = src->features;
    dst->flags = src->flags;

    dst->name = tc_strdup(src->name);
    if (!dst->name) {
        goto no_mem_name;
    }
    dst->version = tc_strdup(src->version);
    if (!dst->version) {
        goto no_mem_version;
    }
    dst->description = tc_strdup(src->description);
    if (!dst->description) {
        goto no_mem_description;
    }

    for (i = 0; src->codecs_in[i] != TC_CODEC_ERROR; i++) {
        ; /* do nothing */
    }
    i++; /* for end mark (TC_CODEC_ERROR) */
    dst->codecs_in = tc_malloc(i * sizeof(int));
    if (!dst->codecs_in) {
        goto no_mem_codecs_in;
    }
    memcpy(dst->codecs_in, src->codecs_in, i * sizeof(int));

    for (i = 0; src->codecs_out[i] != TC_CODEC_ERROR; i++) {
        ; /* do nothing */
    }
    i++; /* for end mark (TC_CODEC_ERROR) */
    dst->codecs_out = tc_malloc(i * sizeof(int));
    if (!dst->codecs_out) {
        goto no_mem_codecs_out;
    }
    memcpy(dst->codecs_out, src->codecs_out, i * sizeof(int));

    return 0;

    /*
     * void* casts to silence warning from gcc 4.x (free requires void*,
     * argument is const something*
     */
no_mem_codecs_out:
    tc_free((void*)dst->codecs_in);
no_mem_codecs_in:
    tc_free((void*)dst->description);
no_mem_description:
    tc_free((void*)dst->version);
no_mem_version:
    tc_free((void*)dst->name);
no_mem_name:
    return 1;
}

void tc_module_info_free(TCModuleInfo *info)
{
    if (info != NULL) {
        /*
         * void* casts to silence warning from gcc 4.x (free requires void*,
         * argument is const something*
         */
        tc_free((void*)info->name);
        tc_free((void*)info->version);
        tc_free((void*)info->description);
        tc_free((void*)info->codecs_in);
        tc_free((void*)info->codecs_out);
    }
}

/*************************************************************************/

/* embedded tests
BEGIN_TEST_CODE

// compile command: gcc -Wall -g -O -I. -I.. source.c path/to/libtc.a
#include "src/transcode.h"
#include "tcmodule-info.h"
#include "tccodecs.h"

static const int empty_codecs[] = { TC_CODEC_ERROR };
static TCModuleInfo empty = {
    TC_MODULE_FEATURE_NONE,
    TC_MODULE_FLAG_NONE,
    "",
    "",
    "",
    empty_codecs,
    empty_codecs
};

static const int pass_enc_codecs[] = { TC_CODEC_ANY, TC_CODEC_ERROR };
static TCModuleInfo pass_enc = {
    TC_MODULE_FEATURE_ENCODE | TC_MODULE_FEATURE_VIDEO
        | TC_MODULE_FEATURE_AUDIO | TC_MODULE_FEATURE_EXTRA,
    TC_MODULE_FLAG_RECONFIGURABLE,
    "encode_pass.so",
    "0.0.1 (2005-11-14)",
    "accepts everything, outputs verbatim",
    pass_enc_codecs,
    pass_enc_codecs
};

static const int fake_mplex_codecs[] = { TC_CODEC_ANY, TC_CODEC_ERROR };
static TCModuleInfo fake_mplex = {
    TC_MODULE_FEATURE_MULTIPLEX | TC_MODULE_FEATURE_VIDEO
        | TC_MODULE_FEATURE_AUDIO | TC_MODULE_FEATURE_EXTRA,
    TC_MODULE_FLAG_RECONFIGURABLE,
    "mplex_null.so",
    "0.0.1 (2005-11-14)",
    "accepts and discards everything",
    fake_mplex_codecs,
    empty_codecs
};

static const int fake_mpeg_codecs_in[] = { TC_CODEC_YUV420P, TC_CODEC_ERROR };
static const int fake_mpeg_codecs_out[] = { TC_CODEC_MPEG1, TC_CODEC_MPEG2, TC_CODEC_XVID, TC_CODEC_ERROR };
static TCModuleInfo fake_mpeg_enc = {
    TC_MODULE_FEATURE_ENCODE | TC_MODULE_FEATURE_VIDEO,
    TC_MODULE_FLAG_NONE
    "encode_mpeg.so",
    "0.0.1 (2005-11-14)",
    "fake YUV420P -> MPEG video encoder",
    fake_mpeg_codecs_in,
    fake_mpeg_codecs_out
};

static const int fake_vorbis_codecs_in[] = { TC_CODEC_PCM, TC_CODEC_ERROR };
static const int fake_vorbis_codecs_out[] = { TC_CODEC_VORBIS, TC_CODEC_ERROR };
static TCModuleInfo fake_vorbis_enc = {
    TC_MODULE_FEATURE_ENCODE | TC_MODULE_FEATURE_AUDIO,
    TC_MODULE_FLAG_NONE,
    "encode_vorbis.so",
    "0.0.1 (2005-11-14)",
    "fake PCM -> Vorbis audio encoder",
    fake_vorbis_codecs_in,
    fake_vorbis_codecs_out
};

static const int fake_avi_codecs_in[] = { TC_CODEC_MPEG1, TC_CODEC_XVID, TC_CODEC_MP3, TC_CODEC_ERROR };
static TCModuleInfo fake_avi_mplex = {
    TC_MODULE_FEATURE_MULTIPLEX | TC_MODULE_FEATURE_VIDEO
        | TC_MODULE_FEATURE_AUDIO,
    TC_MODULE_FLAG_NONE,
    "mplex_avi.so",
    "0.0.1 (2005-11-14)",
    "fakes an AVI muxer",
    fake_avi_codecs_in,
    empty_codecs
 };

static const TCModuleInfo *fake_modules[] = {
    &empty, &pass_enc, &fake_mplex, &fake_mpeg_enc,
    &fake_vorbis_enc, &fake_avi_mplex
};
static const int fake_modules_count = 6;

void test_module_log(void)
{
    int verbosiness[4] = { TC_QUIET, TC_INFO, TC_DEBUG, TC_STATS };
    int i = 0, j = 0;

    for (i = 0; i < 4; i++) {
        fprintf(stderr, "*** at verbosiness level %i:\n", verbosiness[i]);
        fprintf(stderr, "----------------------------\n");
        for (j = 0; j < fake_modules_count; j++) {
            tc_module_info_log(fake_modules[j], verbosiness[i]);
            fputs("\n", stderr);
        }
    }
}

static void test_match_helper(TCModuleInfo *m1, TCModuleInfo *m2, int expected)
{
    int match = tc_module_info_match(m1, m2);

    if (match != expected) {
        tc_log_error(__FILE__, "'%s' <-%c-> '%s' FAILED",
                        m1->name,
                        (expected == 1) ?'-' :'!',
                        m2->name);
    } else {
        tc_log_info(__FILE__, "'%s' <-%c-> '%s' OK",
                        m1->name,
                        (expected == 1) ?'-' :'!',
                        m2->name);
    }

}

void test_module_match(void)
{
    test_match_helper(&empty, &empty, 0);
    test_match_helper(&empty, &fake_mpeg_enc, 0);
    test_match_helper(&fake_mpeg_enc, &empty, 0);

    test_match_helper(&pass_enc, &fake_mplex, 1);
    test_match_helper(&pass_enc, &fake_avi_mplex, 0);
    test_match_helper(&pass_enc, &fake_mpeg_enc, 0);

    test_match_helper(&fake_mpeg_enc, &fake_vorbis_enc, 0);
    test_match_helper(&fake_mpeg_enc, &fake_mplex, 1);
    test_match_helper(&fake_mpeg_enc, &fake_avi_mplex, 1);

    test_match_helper(&fake_vorbis_enc, &fake_mpeg_enc, 0);
    test_match_helper(&fake_vorbis_enc, &fake_mplex, 1);
    test_match_helper(&fake_vorbis_enc, &fake_avi_mplex, 0);
}

int main(void)
{
    //test_module_log();
    test_module_match();

    return 0;
}
END_TEST_CODE
*/

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
