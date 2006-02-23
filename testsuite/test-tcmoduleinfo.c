/*
 * test-tcmodulenfo.c - testsuite for tcmoduleinfo* functions 
 *                      everyone feel free to add more tests and improve
 *                      existing ones.
 * (C) 2006 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */


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
    TC_MODULE_FLAG_NONE,
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
    test_module_match();

    return 0;
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
