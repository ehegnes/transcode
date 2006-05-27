/*
 * test-iodir.c - testsuite for TCDirList* family; 
 *                everyone feel free to add more tests and improve
 *                existing ones.
 * (C) 2006 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "libtc/libtc.h"
#include "libtc/iodir.h"


// typical use case #1

static int test_simple_scan(void)
{
    TCDirList dir;
    int ret;
    const char *pc = NULL;

    tc_info("test_simple_scan:");

    ret = tc_dirlist_open(&dir, "/", 0);
    if (ret != 0) {
        tc_error("tc_dirlist_open(\"/\") failed");
    }

    while ((pc = tc_dirlist_scan(&dir)) != NULL) {
        printf("%s\n", pc);
    }
    printf("file count: %i\n", tc_dirlist_file_count(&dir));

    tc_dirlist_close(&dir);

    return 0;
}

// typical use case #2

static int test_sortbuf_scan(void)
{
    TCDirList dir;
    int ret, i, j;
    const char *pc = NULL;

    tc_info("test_sortbuf_scan:");

    ret = tc_dirlist_open(&dir, "/", 1);
    if (ret != 0) {
        tc_error("tc_dirlist_open(\"/\") failed");
    }

    i = tc_dirlist_file_count(&dir);
    printf("file count: %i\n", i);

    while ((pc = tc_dirlist_scan(&dir)) != NULL) {
        printf("%s\n", pc);
    }
    j = tc_dirlist_file_count(&dir);
    printf("file count: %i\n", j);

    if (i != j) {
        tc_error("missed some files in sortbuf()");
    }

    tc_dirlist_close(&dir);

    return 0;
}

// some misc expected failures

static int test_expected_failures(void)
{
    TCDirList dir;
    int ret;

    tc_info("test_expected_failures: (no output means all clean)");

    ret = tc_dirlist_open(&dir, "/proc/self/cmdline", 0);
    if (ret == 0) {
        tc_error("tc_dirlist_open(\"/proc/self/cmdline\") succeded");
    }

    ret = tc_dirlist_open(&dir, "/proc/self/cmdline", 1);
    if (ret == 0) {
        tc_error("tc_dirlist_open(\"/proc/self/cmdline\", sorted) succeded");
    }

    ret = tc_dirlist_open(&dir, "/inexistent", 0);
    if (ret == 0) {
        tc_error("tc_dirlist_open(\"/inexistent\") succeded");
    }

    ret = tc_dirlist_open(&dir, "/inexistent", 1);
    if (ret == 0) {
        tc_error("tc_dirlist_open(\"/inexistent\", sorted) succeded");
    }
    return 0;
}

int main(void)
{
    test_simple_scan();
    test_sortbuf_scan();
    test_expected_failures();

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
