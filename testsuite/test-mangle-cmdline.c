/*
 * test-mangle-cmdline - testsuite for tc_mangle_cmdline() function;
 *                       everyone feel free to add more tests and improve
 *                       existing ones.
 * (C) 2006 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "config.h"

#define _GNU_SOURCE 1
#define MAX_OPTS    (32)

#include "libtc/libtc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>



#define DUMP_OPTS(AC, AV) do { \
    int i = 0; \
    printf("argc=%i\n", (AC)); \
    for (i = 0; i < (AC); i++) { \
        printf("argv[%i] = %s\n", i, (AV)[i]); \
    } \
} while (0)


static int in_set(const char *val, const char **val_set, int set_len)
{
    int i = 0;

    for (i = 0; i < set_len; i++) {
        if (!strcmp(val_set[i], val)) {
            return 1;
        }
    }
    return 0;
}

static int test_mangle_option(int argc, char **argv, const char *option, int hasval)
{
    int ac = argc, i = 0, ret;
    char *av[MAX_OPTS];
    const char *useless = NULL;

    assert(argc <= MAX_OPTS);

    for (i = 0; i < argc; i++) {
        av[i] = argv[i];
    }
    ret = tc_mangle_cmdline(&argc, &argv, option, (hasval) ?(&useless) :NULL);
    tc_info("mangling: %i", ret);
    if (ret != 0) {
        DUMP_OPTS(argc, argv);
        if (ac != argc) {
            tc_warn("missing argument (argc not changed)");
            return 1;
        }
        for (i = 0; i < argc; i++) {
            if (av[i] != argv[i]
             || strcmp(av[i], argv[i]) != 0) {
                tc_warn("argument diversion (%s VS %s @ %i)", av[i], argv[i], i);
                return 1;
            }
        }
        if (!in_set(option, (const char **)argv, argc)) {
            tc_warn("option still present");
            return 1;
        }
    } else {
        int na = ac - ((hasval) ?2 :1);
        DUMP_OPTS(argc, argv);
        if (na != argc) {
            tc_warn("argument number mismatch (expected %i|got %i)", na, argc);
            return 1;
        }
        if (in_set(option, (const char **)argv, argc)) {
            tc_warn("option still present");
            return 1;
        }
        for (i = 0; i < ac; i++) {
            if (!in_set(argv[i], (const char **)av, ac)) {
                tc_warn("missing argument: %s", argv[i]);
                return 1;
            }
        }
    }
    return 0;
}

#define TEST_OPT(OPT, HASVAL) do { \
    char *argv[] = { "testprogram", "-c", "-v", "-A", "1", \
                     "--foo", "bar", "--baz", "-t" }; \
    int argc = 9; \
    int ret = 0; \
    \
    tc_info("TEST BEGINS HERE ==================================="); \
    puts("base commandline:"); \
    DUMP_OPTS(argc, argv); \
    \
    puts("removing " OPT ": "); \
    ret = test_mangle_option(argc, argv, OPT, HASVAL); \
    if (ret) { \
        tc_warn("test with %s: FAILED", OPT); \
        err++; \
    } else { \
        tc_info("test with %s: ok", OPT); \
    } \
    tc_info("TEST ENDS HERE ====================================="); \
} while (0)


int main(void)
{
    int err = 0;

    TEST_OPT("-c", TC_FALSE);
    TEST_OPT("-v", TC_FALSE);
    TEST_OPT("-A", TC_TRUE);
    TEST_OPT("--baz", TC_FALSE);
    TEST_OPT("--foo", TC_TRUE);

    return (err > 0) ?1 :0;
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
