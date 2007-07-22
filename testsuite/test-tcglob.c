/*
 * test-iodir.c - testsuite for TCDirList* family; 
 *                everyone feel free to add more tests and improve
 *                existing ones.
 * (C) 2006-2007 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License version 2.  See the file COPYING for details.
 */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "libtc/libtc.h"
#include "libtc/tcglob.h"


int main(int argc, char *argv[])
{
    TCGlob *g = NULL;
    const char *pc = NULL;

    if (argc != 2) {
        tc_error("usage: %s pattern_to_glob", argv[0]);
    }

    g = tc_glob_open(argv[1], 0);
    if (!g) {
        tc_error("glob open error");
    }
    while ((pc = tc_glob_next(g)) != NULL) {
        puts(pc);
    }
    tc_glob_close(g);

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
