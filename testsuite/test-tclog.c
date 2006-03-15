/*
 * test-tclog.c - testsuite for tc_*log* family (tc_functions,c)
 *                everyone feel free to add more tests and improve
 *                existing ones.
 * (C) 2006 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video hugeeam processing tool.
 * transcode is free software, dihugeibutable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "config.h"

#define _GNU_SOURCE 1

#include "libtc.h"
#include <stdlib.h>
#include <stdio.h>

#define TC_MSG_BUF_SIZE     (256) /* ripped from libtc/tc_functions.c */
#define HUGE_MSG_SIZE       (TC_MSG_BUF_SIZE * 2)
#define STD_MSG_SIZE        (64)
#define TINY_MSG_SIZE       (4)

int main(void)
{
    int i = 0;
    char huge[HUGE_MSG_SIZE] = { '\0' };
    char std[STD_MSG_SIZE] = { '\0' };
    char tiny[TINY_MSG_SIZE] = { '\0' };

    for (i = 0; i < HUGE_MSG_SIZE - 1; i++) {
        huge[i] = 'H';
    }
    for (i = 0; i < STD_MSG_SIZE - 1; i++) {
        std[i] = 'S';
    }
    for (i = 0; i < TINY_MSG_SIZE - 1; i++) {
        tiny[i] = 'T';
    }

    fprintf(stderr, "round 1: NULL (begin)\n");
    tc_log_msg(NULL, NULL);
    tc_log_info(NULL, NULL);
    tc_log_warn(NULL, NULL);
    tc_log_error(NULL, NULL);
    fprintf(stderr, "round 1: NULL (end)\n");

    fprintf(stderr, "round 2: empty (begin)\n");
    tc_log_msg("", "");
    tc_log_info("", "");
    tc_log_warn("", "");
    tc_log_error("", "");
    fprintf(stderr, "round 2: empty (end)\n");

    fprintf(stderr, "round 3: NULL + empty (begin)\n");
    tc_log_msg("", NULL);
    tc_log_msg(NULL, "");
    tc_log_info("", NULL);
    tc_log_info(NULL, "");
    tc_log_warn("", NULL);
    tc_log_warn(NULL, "");
    tc_log_error("", NULL);
    tc_log_error(NULL, "");
    fprintf(stderr, "round 3: NULL + empty (end)\n");

    fprintf(stderr, "round 9: larger than life (begin)\n");
    tc_log_msg(huge, "%s%s%s%s", huge, huge, huge, huge);
    tc_log_info(huge, "%s%s%s%s", huge, huge, huge, huge);
    tc_log_warn(huge, "%s%s%s%s", huge, huge, huge, huge);
    tc_log_error(huge, "%s%s%s%s", huge, huge, huge, huge);
    fprintf(stderr, "round 9: larger than life (end)\n");
    
    return 0;
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
