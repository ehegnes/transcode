
/*
 * test-pvmparser - testsuite for PVM3 configuration parser; 
 *                  everyone feel free to add more tests and improve
 *                  existing ones.
 * (C) 2007 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License version 2.  See the file COPYING for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#include "libtc/libtc.h"
#include "libtc/cfgfile.h"

#include "pvm3/pvm_parser.h"


int main(int argc, char *argv[])
{
    int full = TC_FALSE;
    pvm_config_env *env = NULL;
    
    if (argc != 2) {
        fprintf(stderr, "(%s) usage: %s pvm.cfg\n", __FILE__, argv[0]);
        exit(1);
    }
    if (argc == 3 && !strcmp(argv[2], "full")) {
        full = TC_TRUE;
    }
    env = pvm_parser_open(argv[1], TC_TRUE, full);

    pvm_parser_close();

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
