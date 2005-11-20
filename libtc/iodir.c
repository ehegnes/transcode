/*
 *  iodir.c
 *
 *  Copyright (C) Thomas Östreich - May 2002
 *
 *  This file is part of transcode, a video stream processing tool
 *
 *  transcode is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  transcode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdlib.h>
#include <string.h>
#include "iodir.h"
#include "libtc.h"

#ifdef SYS_BSD
typedef off_t off64_t;
#define lseek64 lseek
#endif

static int compare_name(const void *file1_ptr, const void *file2_ptr)
{
    return strcoll(*(const char **)file1_ptr, *(const char **)file2_ptr);
}

int tc_directory_file_count(TCDirectory *tcdir)
{
    if (tcdir == NULL) {
        return -1;
    }
    return tcdir->nfiles;
}

int tc_directory_open(TCDirectory *tcdir, const char *dir_name)
{
    size_t len = 0;
    char end_of_dir;

    if (tcdir == NULL) {
        return -1;
    }

    tcdir->filename[0] = '\0';
    tcdir->rbuf_ptr = NULL;
    tcdir->nfiles = 0;
    tcdir->findex = 0;
    tcdir->buffered = 0;

    len = strlen(dir_name);
    if (dir_name == NULL || len == 0) {
        return -1;
    }
    tcdir->dir_name = dir_name;

    end_of_dir = tcdir->dir_name[len - 1];
    if (end_of_dir == '/') {
        tcdir->path_sep = "";
    } else {
        tcdir->path_sep = "/";
    }

    tcdir->dir = opendir(dir_name);
    if (tcdir->dir == NULL) {
        return -1;
    }

    return 0;
}

static void tc_directory_freebuf(TCDirectory *tcdir)
{
    if (tcdir != NULL) {
        if (tcdir->buffered == 1) {
            int i = 0;
            for (i = 0; i < tcdir->nfiles; i++) {
                if (tcdir->rbuf_ptr[i] != NULL) {
                    /* should be always true */
                    free(tcdir->rbuf_ptr[i]);
                }
            }

            if (tcdir->rbuf_ptr != NULL) {
                /* should be always true */
                free(tcdir->rbuf_ptr);
            }

            tcdir->nfiles = 0;
        }
    }
}

void tc_directory_close(TCDirectory *tcdir)
{
    if (tcdir != NULL) {
        tc_directory_freebuf(tcdir);
        if (tcdir->dir != NULL) {
            closedir(tcdir->dir);
            tcdir->dir = NULL;
        }
    }
}

static int tc_directory_next(TCDirectory *tcdir)
{
    struct dirent *dent = NULL;
    int have_file = 0;

    if (tcdir == NULL) {
        return -1;
    }

    do {
        dent = readdir(tcdir->dir);
        if (dent == NULL) {
            break; /* all entries in directory have been processed */
        }

        if ((strncmp(dent->d_name, ".", 1) != 0)
            && (strcmp(dent->d_name, "..") != 0)
        ) {
            /* discard special files */
            have_file = 1;
        }

    } while (!have_file);

    if (have_file) {
        tc_snprintf(tcdir->filename, sizeof(tcdir->filename),
                    "%s%s%s", tcdir->dir_name, tcdir->path_sep,
                    dent->d_name);
        return 0;
    }
    return 1;
}

const char *tc_directory_scan(TCDirectory *tcdir)
{
    const char *ret = NULL;

    if (tcdir != NULL) {
        if (tcdir->buffered == 0) {
            if (tc_directory_next(tcdir) != 0) {
                ret = NULL;
            } else {
                tcdir->nfiles++;
                ret = tcdir->filename;
            }
        } else { /* tcdir->buffered == 0 */
            /* buffered */
            if (tcdir->findex < tcdir->nfiles) {
                ret = tcdir->rbuf_ptr[tcdir->findex++];
            } else {
                ret = NULL;
            }
        }
    }

    return ret;
}

int tc_directory_sortbuf(TCDirectory *tcdir)
{
    int n = 0;

    if (tcdir == NULL) {
        return -1;
    }

    rewinddir(tcdir->dir);
    while (tc_directory_next(tcdir) == 0) {
        tcdir->nfiles++;
    }
    rewinddir(tcdir->dir);

    tcdir->rbuf_ptr = tc_malloc(tcdir->nfiles * sizeof(char *));
    if (tcdir->rbuf_ptr == NULL) {
        return -1;
    }

    while (tc_directory_next(tcdir) == 0) {
        tcdir->rbuf_ptr[n] = tc_strdup(tcdir->filename);
        if (tcdir->rbuf_ptr[n] == NULL) {
            tc_log_warn(__FILE__, "can't memorize directory entry "
                                  "for '%s'\n", tcdir->filename);
        }
        n++;
    }

    qsort(tcdir->rbuf_ptr, tcdir->nfiles, sizeof(char *), compare_name);

    tcdir->buffered = 1;
    tcdir->findex = 0;

    return 0;
}

/*************************************************************************/

/* embedded simple tests for tc_directory*()

BEGIN_TEST_CODE

// compile command: gcc -Wall -g -O -I. -I.. source.c path/to/libtc.a
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "libtc.h"
#include "iodir.h"


// typical use case #1

int test_simple_scan(void)
{
    TCDirectory dir;
    int ret;
    const char *pc = NULL;

    tc_info("test_simple_scan:");

    ret = tc_directory_open(&dir, "/");
    if (ret != 0) {
        tc_error("tc_directory_open(\"/\") failed");
    }

    while ((pc = tc_directory_scan(&dir)) != NULL) {
        printf("%s\n", pc);
    }
    printf("file count: %i\n", tc_directory_file_count(&dir));

    tc_directory_close(&dir);

    return 0;
}

// typical use case #2

int test_sortbuf_scan(void)
{
    TCDirectory dir;
    int ret, i, j;
    const char *pc = NULL;

    tc_info("test_sortbuf_scan:");

    ret = tc_directory_open(&dir, "/");
    if (ret != 0) {
        tc_error("tc_directory_open(\"/\") failed");
    }

    ret = tc_directory_sortbuf(&dir);
    if (ret != 0) {
        tc_error("tc_directory_sortbuf(\"/\") failed");
    }
    i = tc_directory_file_count(&dir);
    printf("file count: %i\n", i);

    while ((pc = tc_directory_scan(&dir)) != NULL) {
        printf("%s\n", pc);
    }
    j = tc_directory_file_count(&dir);
    printf("file count: %i\n", j);

    if (i != j) {
        tc_error("missed some files in sortbuf()");
    }

    tc_directory_close(&dir);

    return 0;
}

// some misc expected failures

int test_expected_failures(void)
{
    TCDirectory dir;
    int ret;

    tc_info("test_expected_failures:");

    ret = tc_directory_open(&dir, "/proc/self/cmdline");
    if (ret == 0) {
        tc_error("tc_directory_open(\"/proc/self/cmdline\") succeded");
    }

    ret = tc_directory_open(&dir, "/inexistent");
    if (ret == 0) {
        tc_error("tc_directory_open(\"/inexistent\") succeded");
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
BEGIN_TEST_CODE
*/

/**************************************************************************/

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
