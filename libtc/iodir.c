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

static int tc_dirlist_next(TCDirList *tcdir)
{
    struct dirent *dent = NULL;
    int have_file = 0;

    if (tcdir == NULL) {
        return -1;
    }

    do {
        dent = readdir(tcdir->dir);
        if (dent == NULL) {
            break; /* all entries in dirlist have been processed */
        }

        if ((strncmp(dent->d_name, ".", 1) != 0)
          && (strcmp(dent->d_name, "..") != 0)) {
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

static int compare_name(const void *file1_ptr, const void *file2_ptr)
{
    return strcoll(*(const char **)file1_ptr, *(const char **)file2_ptr);
}

static int tc_dirlist_sortbuf(TCDirList *tcdir)
{
    int n = 0;

    if (tcdir == NULL) {
        return -1;
    }

    tcdir->entries = tc_malloc(tcdir->nfiles * sizeof(char *));
    if (tcdir->entries == NULL) {
        return -1;
    }

    while (tc_dirlist_next(tcdir) == 0) {
        tcdir->entries[n] = tc_strdup(tcdir->filename);
        if (tcdir->entries[n] == NULL) {
            tc_log_warn(__FILE__, "can't memorize dirlist entry "
                                  "for '%s'\n", tcdir->filename);
        }
        n++;
    }

    qsort(tcdir->entries, tcdir->nfiles, sizeof(char *), compare_name);

    tcdir->buffered = 1;
    tcdir->findex = 0;

    return 0;
}

static int tc_dirlist_set_path_sep(TCDirList *tcdir)
{
    size_t len = 0;
    char end_of_dir;

    len = strlen(tcdir->dir_name);
    if (len == 0) {
        return -1;
    }

    end_of_dir = tcdir->dir_name[len - 1];
    if (end_of_dir == '/') {
        tcdir->path_sep = "";
    } else {
        tcdir->path_sep = "/";
    }

    return 0;
}

int tc_dirlist_file_count(TCDirList *tcdir)
{
    if (tcdir == NULL) {
        return -1;
    }
    return tcdir->nfiles;
}


int tc_dirlist_open(TCDirList *tcdir, const char *dirname, int sort)
{
    int ret;

    if (tcdir == NULL) {
        return -1;
    }

    tcdir->filename[0] = '\0';
    tcdir->entries = NULL;
    tcdir->nfiles = 0;
    tcdir->findex = 0;
    tcdir->buffered = 0;
    tcdir->dir_name = dirname;

    ret = tc_dirlist_set_path_sep(tcdir);
    if (ret != 0) {
        return ret;
    }

    tcdir->dir = opendir(dirname);
    if (tcdir->dir == NULL) {
        return -1;
    }

    rewinddir(tcdir->dir);
    while (tc_dirlist_next(tcdir) == 0) {
        tcdir->nfiles++;
    }
    rewinddir(tcdir->dir);

    if (sort) {
        tc_dirlist_sortbuf(tcdir);
    }
    return 0;
}

void tc_dirlist_close(TCDirList *tcdir)
{
    if (tcdir != NULL) {
        if (tcdir->buffered == 1) {
            int i = 0;
            for (i = 0; i < tcdir->nfiles; i++) {
                if (tcdir->entries[i] != NULL) {
                    /* should be always true */
                    free(tcdir->entries[i]);
                    tcdir->nfiles--;
                }
            }

            if (tcdir->entries != NULL) {
                /* should be always true */
                free(tcdir->entries);
            }

            if (tcdir->nfiles > 0) {
                /* should never happen */
                tc_log_warn(__FILE__, "left out %i directory entries",
                            tcdir->nfiles);
            }
        }

        if (tcdir->dir != NULL) {
            closedir(tcdir->dir);
            tcdir->dir = NULL;
        }
    }
}

const char *tc_dirlist_scan(TCDirList *tcdir)
{
    const char *ret = NULL;

    if (tcdir == NULL) {
        return NULL;
    }

    if (tcdir->buffered == 0) {
        if (tc_dirlist_next(tcdir) == 0) {
            ret = tcdir->filename;
        }
    } else { /* tcdir->buffered == 0 */
        /* buffered */
        if (tcdir->findex < tcdir->nfiles) {
            ret = tcdir->entries[tcdir->findex++];
        }
    }

    return ret;
}

/*************************************************************************/

/* embedded simple tests for tc_dirlist*()

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

int test_sortbuf_scan(void)
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

int test_expected_failures(void)
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
END_TEST_CODE
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
