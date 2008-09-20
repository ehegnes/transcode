/*
 * test-tclist.c -- testsuite for TCList* family; 
 *                   everyone feel free to add more tests and improve
 *                   existing ones.
 * (C) 2008 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 *
 * transcode is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * transcode is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "libtc/libtc.h"
#include "libtc/tclist.h"


/*************************************************************************/

#define TC_TEST_BEGIN(NAME, CACHED) \
static int tclist_ ## NAME ## _test(void) \
{ \
    const char *testname = # NAME ; \
    const char *errmsg = ""; \
    int err = 1; \
    TCList L; \
    \
    tc_log_info(__FILE__, "running test: [%s]", # NAME); \
    if (tc_list_init(&L, (CACHED)) == TC_OK) {


#define TC_TEST_END \
        if (tc_list_fini(&L) != TC_OK) { \
            return 1; \
        } \
        return 0; \
    } \
test_failure: \
    tc_log_warn(__FILE__, "FAILED test [%s] NOT verified: %s", testname, errmsg); \
    return 1; \
}


#define TC_TEST_IS_TRUE(EXPR) do { \
    err = (EXPR); \
    if (!err) { \
        errmsg = # EXPR ; \
        goto test_failure; \
    } \
} while (0)


#define TC_RUN_TEST(NAME) \
    errors += tclist_ ## NAME ## _test()

/*************************************************************************/


TC_TEST_BEGIN(just_init, 0)
    TC_TEST_IS_TRUE(tc_list_size(&L) == 0);
TC_TEST_END

TC_TEST_BEGIN(append, 0)
    long num = 42;
    TC_TEST_IS_TRUE(tc_list_append(&L, &num) == TC_OK);
    TC_TEST_IS_TRUE(tc_list_size(&L) == 1);
TC_TEST_END

TC_TEST_BEGIN(append_get, 0)
    long num = 42, *res;
    TC_TEST_IS_TRUE(tc_list_append(&L, &num) == TC_OK);
    TC_TEST_IS_TRUE(tc_list_size(&L) == 1);
    res = tc_list_get(&L, 0);
    TC_TEST_IS_TRUE(&num == res);
TC_TEST_END

TC_TEST_BEGIN(prepend_get, 0)
    long num = 42, *res;
    TC_TEST_IS_TRUE(tc_list_prepend(&L, &num) == TC_OK);
    TC_TEST_IS_TRUE(tc_list_size(&L) == 1);
    res = tc_list_get(&L, 0);
    TC_TEST_IS_TRUE(&num == res);
TC_TEST_END

TC_TEST_BEGIN(appendN_get, 0)
    long *res, nums[] = { 23, 42, 18, 75, 73, 99, 14, 29 };
    int i = 0, len = sizeof(nums)/sizeof(nums[0]);
    for (i = 0; i < len; i++) {
        TC_TEST_IS_TRUE(tc_list_append(&L, &(nums[i])) == TC_OK);
    }
    TC_TEST_IS_TRUE(tc_list_size(&L) == len);
    res = tc_list_get(&L, 0);
    TC_TEST_IS_TRUE(&(nums[0]) == res);
TC_TEST_END

TC_TEST_BEGIN(prependN_get, 0)
    long *res, nums[] = { 23, 42, 18, 75, 73, 99, 14, 29 };
    int i = 0, len = sizeof(nums)/sizeof(nums[0]);
    for (i = 0; i < len; i++) {
        TC_TEST_IS_TRUE(tc_list_prepend(&L, &(nums[i])) == TC_OK);
    }
    TC_TEST_IS_TRUE(tc_list_size(&L) == len);
    res = tc_list_get(&L, 0);
    TC_TEST_IS_TRUE(&(nums[len-1]) == res);
TC_TEST_END




/*************************************************************************/

static int test_list_all(void)
{
    int errors = 0;

    TC_RUN_TEST(just_init);
    TC_RUN_TEST(append);
    TC_RUN_TEST(append_get);
    TC_RUN_TEST(prepend_get);
    TC_RUN_TEST(appendN_get);
    TC_RUN_TEST(prependN_get);

    return errors;
}

int main(void)
{
    int errors = test_list_all();

    putchar('\n');
    tc_log_info(__FILE__, "test summary: %i error%s (%s)",
                errors,
                (errors > 1) ?"s" :"",
                (errors > 0) ?"FAILED" :"PASSED");
    return (errors > 0) ?1 :0;
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
