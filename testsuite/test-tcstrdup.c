/*
 * test-tcstrdup.c - testsuite for tc_*strdup* family (tc_functions,c)
 *                   everyone feel free to add more tests and improve
 *                   existing ones.
 * (C) 2006 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "config.h"

#define _GNU_SOURCE 1

#include "libtc/libtc.h"
#include <stdlib.h>
#include <stdio.h>

// test case 1

#define TEST_STRING "testing tc_str*dup()"

static int test_strdup(void)
{
    const char *s1 = TEST_STRING;
    char *s2 = NULL, *s3 = NULL;

    tc_info("test_strdup() begin");

    s2 = strdup(s1);
    s3 = tc_strdup(s1);

    if (strlen(s1) != strlen(s2)) {
        tc_error("string length mismatch: '%s' '%s'", s1, s2);
    }
    if (strlen(s1) != strlen(s3)) {
        tc_error("string length mismatch: '%s' '%s'", s1, s3);
    }
    if (strlen(s2) != strlen(s3)) {
        tc_error("string length mismatch: '%s' '%s'", s2, s3);
    }

    if (strcmp(s1, s2) != 0) {
        tc_error("string mismatch: '%s' '%s'", s1, s2);
    }
    if (strcmp(s1, s3) != 0) {
        tc_error("string mismatch: '%s' '%s'", s1, s3);
    }
    if (strcmp(s2, s3) != 0) {
        tc_error("string mismatch: '%s' '%s'", s2, s3);
    }

    free(s2);
    tc_free(s3);

    tc_info("test_strdup() end");
    return 0;
}

static int test_strndup(size_t n)
{
    const char *s1 = TEST_STRING;
    char *s2 = NULL, *s3 = NULL;

    tc_info("test_strndup(%lu) begin", (unsigned long)n);

    s2 = strndup(s1, n);
    s3 = tc_strndup(s1, n);

    if (strlen(s2) != strlen(s3)) {
        tc_error("string length mismatch: '%s' '%s'", s2, s3);
    }

    if (strcmp(s2, s3) != 0) {
        tc_error("string mismatch: '%s' '%s'", s2, s3);
    }

    free(s2);
    tc_free(s3);

    tc_info("test_strndup() end");
    return 0;
}

int main(void)
{
    test_strdup();

    test_strndup(0);
    test_strndup(1);
    test_strndup(5);

    test_strndup(strlen(TEST_STRING)-2);
    test_strndup(strlen(TEST_STRING)-1);

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
