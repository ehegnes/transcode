/*
 * test-cfg-filelist.c - testsuite for module_*_config_list family; 
 *                       everyone feel free to add more tests and improve
 *                       existing ones.
 * (C) 2007 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License version 2.  See the file COPYING for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "config.h"
#include "libtc/libtc.h"
#include "libtc/cfgfile.h"


int main(int argc, char *argv[])
{
    TCConfigList *list = NULL;
    const char *filename = NULL, *section = NULL;
    
    if (argc != 3) {
        fprintf(stderr, "(%s) usage: %s cfgfile section\n", __FILE__, argv[0]);
        exit(1);
    }
    filename = argv[1];
    section  = argv[2];

    libtc_init(NULL, NULL); /* XXX: dirty! */
    tc_set_config_dir("");  /* XXX: dirty? */
    list = module_read_config_list(filename, section, __FILE__);
    if (!list) {
        fprintf(stderr, "unable to scan '%s'\n", filename);
        exit(1);
    } else {
        module_print_config_list(list, section, "test");
        module_free_config_list(list, 0);
    }

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
