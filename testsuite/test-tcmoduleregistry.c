/*
 * test-tcmoduleregistry.c -- testsuite for tcmoduleregistry* functions; 
 *                            everyone feel free to add more tests and improve
 *                            existing ones.
 * (C) 2009 - Francesco Romani <fromani -at- gmail -dot- com>
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

#include "src/transcode.h"
#include "libtc/tccodecs.h"
#include "libtcmodule/tcmodule-registry.h"

/*************************************************************************/

#define TC_TEST_BEGIN(NAME, FACTORY) \
static int tcregistry_ ## NAME ## _test(void) \
{ \
    const char *TC_TEST_name = # NAME ; \
    const char *TC_TEST_errmsg = ""; \
    int TC_TEST_step = -1; \
    \
    TCRegistry Reg = NULL; \
    \
    tc_log_info(__FILE__, "running test: [%s]", TC_TEST_name); \
    Reg = tc_new_module_registry((FACTORY), ".", TC_STATS); \
    if (!Reg) { \
        TC_TEST_errmsg = "can't create the registry instance"; \
        tc_log_error(__FILE__, "[%s] UNABLE to setup registry!!", TC_TEST_name); \
    } else {


#define TC_TEST_END \
        if (tc_del_module_registry(Reg) != TC_OK) { \
            tc_log_error(__FILE__, "[%s] UNABLE to delete registry!!", TC_TEST_name); \
            return 1; \
        } \
        return 0; \
    } \
TC_TEST_failure: \
    if (TC_TEST_step != -1) { \
        tc_log_warn(__FILE__, "FAILED test [%s] at step %i", TC_TEST_name, TC_TEST_step); \
    } \
    tc_log_warn(__FILE__, "FAILED test [%s] NOT verified: %s", TC_TEST_name, TC_TEST_errmsg); \
    return 1; \
}

#define TC_TEST_SET_STEP(STEP) do { \
    TC_TEST_step = (STEP); \
} while (0)

#define TC_TEST_UNSET_STEP do { \
    TC_TEST_step = -1; \
} while (0)

#define TC_TEST_IS_TRUE(EXPR) do { \
    int err = (EXPR); \
    if (!err) { \
        TC_TEST_errmsg = # EXPR ; \
        goto TC_TEST_failure; \
    } \
} while (0)


#define TC_RUN_TEST(NAME) \
    errors += tcregistry_ ## NAME ## _test()


/*************************************************************************/

static TCFactory factory = NULL;

/* kids, DO NOT try those (N_*) at home! */
TC_TEST_BEGIN(N_create_destroy, factory)
    TC_TEST_IS_TRUE(Reg != NULL);
TC_TEST_END

/*************************************************************************/

static int test_registry_all(void)
{
    int errors = 0;
    factory = tc_new_module_factory(".", TC_STATS);
    if (!factory) {
        tc_log_error(__FILE__,
                     "cannot create the module factory (all test aborted)");
        errors = 1;
    } else {

        TC_RUN_TEST(N_create_destroy);

        tc_del_module_factory(factory);
    }
    return errors;
}

int main(int argc, char *argv[])
{
    int errors = 0;
    
    libtc_init(&argc, &argv);
    
    errors = test_registry_all();

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

/*************************************************************************/
