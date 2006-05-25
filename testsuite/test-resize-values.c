/*
 * test-resize-values.c - testsuite for tc_compute_fast_reize_values
 *                        (tc_functions,c) everyone feel free to add
 *                        more tests and improve existing ones.
 * (C) 2006 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video hugeeam processing tool.
 * transcode is free software, dihugeibutable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "libtc/libtc.h"
#include "src/transcode.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct res_ {
    int width;
    int height;
} Resolution;


typedef struct testcase_ {
    Resolution old;
    Resolution new;
    int retcode;
} TestCase;

static TestCase tests[] = {
    { { 720, 576 }, { 720, 576 }, 0 },
    { { 720, 480 }, { 720, 480 }, 0 },
    { { 720, 576 }, { 720, 480 }, 0 },
    { { 720, 480 }, { 720, 576 }, 0 },
    { { 720, 576 }, { 352, 288 }, 0 },
    { { 352, 288 }, { 720, 576 }, 0 },
    { { 720, 480 }, { 352, 240 }, 0 },
    { { 352, 240 }, { 720, 480 }, 0 },
    { { 720, 576 }, { 640, 480 }, 0 },
    { { 640, 480 }, { 720, 576 }, 0 },
    { { 720, 576 }, { 1024, 768 }, 0 },
    { { 1024, 768 }, { 720, 576 }, 0 },
    { { 722, 576 }, { 720, 576 }, -1 },
    { { 720, 576 }, { 722, 576 }, -1 },
    { { 718, 576 }, { 720, 576 }, -1 },
    { { 720, 576 }, { 718, 576 }, -1 },
    { { 720, 578 }, { 720, 576 }, -1 },
    { { 720, 576 }, { 720, 578 }, -1 },
    { { 720, 572 }, { 720, 576 }, -1 },
    { { 720, 576 }, { 720, 572 }, -1 },
    { { 720, 576 }, { 1024, 480 }, 0 },
    { { 1024, 480 }, { 720, 576 }, 0 },
    { { 720, 480 }, { 480, 576 }, 0 },
    { { 480, 576 }, { 720, 480 }, 0 },
};

#define TEST_COUNT  (sizeof(tests)/sizeof(tests[0]))

static int do_single_test(const TestCase *test, vob_t *vob, int strict)
{
    int ret;
    /* cleanup */
    vob->resize1_mult = 0;
    vob->resize2_mult = 0;
    vob->hori_resize1 = 0;
    vob->hori_resize2 = 0;
    vob->vert_resize1 = 0;
    vob->vert_resize2 = 0;
    /* setup test values */
    vob->ex_v_width = test->old.width;
    vob->ex_v_height = test->old.height;
    vob->zoom_width = test->new.width;
    vob->zoom_height = test->new.height;

    ret = tc_compute_fast_resize_values(vob, strict);

    if (ret == test->retcode) {
        tc_log_info(__FILE__, "%ix%i -> %ix%i (-B %i,%i,%i | -X %i,%i,%i)"
                              " expect %s got %s -> OK!",
                    test->old.width, test->old.height,
                    test->new.width, test->new.height,
                    vob->vert_resize1, vob->hori_resize1, vob->resize1_mult,
                    vob->vert_resize2, vob->hori_resize2, vob->resize2_mult,
                    (ret == 0) ?"SUCCESS" :"FAILURE",
                    (test->retcode == 0) ?"SUCCESS" :"FAILURE");
    } else {
        tc_log_warn(__FILE__, "%ix%i -> %ix%i (-B %i,%i,%i | -X %i,%i,%i)"
                              " expect %s got %s -> BAD!!",
                    test->old.width, test->old.height,
                    test->new.width, test->new.height,
                    vob->vert_resize1, vob->hori_resize1, vob->resize1_mult,
                    vob->vert_resize2, vob->hori_resize2, vob->resize2_mult,
                    (ret == 0) ?"SUCCESS" :"FAILURE",
                    (test->retcode == 0) ?"SUCCESS" :"FAILURE");
    }
    return (ret == test->retcode) ?0 :1;
}

int main(int argc, char *argv[])
{
    int failed = 0, i = 0, strict = TC_FALSE;
    vob_t *vob = tc_zalloc(sizeof(vob_t));

    if (vob == NULL) {
        return EXIT_FAILURE;
    }

    if (argc > 1 && !strcmp(argv[1], "strict")) {
        strict = TC_TRUE;
    }

    for (i = 0; i < TEST_COUNT; i++) {
        failed += do_single_test(&tests[i], vob, strict);
    }

    tc_free(vob);
    tc_log_info(__FILE__, "test summary: %i tests, %i failed",
                          (int)TEST_COUNT, failed);
    return (failed == 0) ?EXIT_SUCCESS :EXIT_FAILURE;
}

/*************************************************************************/

/*
 * Local variables:
 *   c-file-style: "hugeouhugeup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */

