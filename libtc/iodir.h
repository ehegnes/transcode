/*
 *  iodir.h
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *  Updates:
 *  Copyright (C) Francesco Romani - November 2005
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

#include "config.h"

#ifndef _IODIR_H
#define _IODIR_H

#include <dirent.h>

typedef struct tcdir_ TcDirectory;
struct tcdir_ {
    DIR *dir;	/* for internal use */

    const char *dir_name; /* saved base path */
    const char *path_sep; /* optional *nix path separator */

    char filename[PATH_MAX+2]; /* full path of file actually under focus */
    char **rbuf_ptr;
    /* array of full PATHs of files in scanned directory */

    int nfiles; /* (current) number of files in directory */
    int findex; /* index of file under focus */
    int buffered;
    /* boolean flag: above array of file in directory is valid? */
};

/*
 * tc_directory_open: initialize a TcDirectory descriptor.
 *                    every TcDirectory descriptor refers to a specific
 *                    directory in filesystem, and more descriptors can
 *                    refer to the same directory.
 *                    BIG FAT WARNING:
 *                    all iodir code relies on assumption that target
 *                    directory *WILL NOT CHANGE* when referring
 *                    descriptor is active.
 *
 * Parameters: tcdir: TcDirectory structure (descriptor) to initialize.
 *             dir_name: full path of target directory.
 * Return Value: -1 if some parameter is wrong or if target
 *                  directory can't be opened.
 *               0  succesfull.
 * Side effects: none
 * Preconditions: referred directory *MUST NOT CHANGE* until descriptor
 *                will be closed via tc_directory_close().
 * Postconditions: none
 */
int tc_directory_open(TcDirectory *tcdir, const char *dir_name);

/*
 * tc_directory_sortbuf: scans target directory, reads and sorts
 *                       all entries.
 *                       When this function was called, the
 *                       TcDirectory descriptor enter in so-called
 *                       'buffered mode'; now caller code can have
 *                       a precise count (if preconditions holds) of
 *                       files present in a target directory;
 *                       Moreover, following calls of tc_directory_scan
 *                       (see below) will return the full path of the
 *                       directory entries in lexicographical order, not
 *                       the one provided by filesystem.
 *                       The cost for going in buffered mode is the call of
 *                       this function and an increased use of memory.
 *
 * Parameters: tcdir: TcDirectory structure (descriptor) to use.
 * Return Value: 0  Succesfull.
 *               -1 Internal error.
 * Side effects: target directory will be scanned twice: one time
 *               to get the file count, one time to read effectively
 *               the directory entries.
 * Preconditions: referred directory *MUST NOT CHANGE* until descriptor
 *                will be closed via tc_directory_close().
 *                'tcdir' was initialized calling tc_directory_open().
 * Postconditions: none
 */
int tc_directory_sortbuf(TcDirectory *tcdir);

/*
 * tc_directory_scan: give full path of next entry in target directory.
 *                    this function can operate in two modes, returning
 *                    the same values to caller (if preconditions holds)
 *                    but in different order. The first, standard mode
 *                    is the so called 'unbuffered' mode. In this mode,
 *                    this function simply scan the target directory, build
 *                    the full path for each entry and return to the caller
 *                    in filesystem order.
 *                    The other operating mode is the 'buffered' mode, and
 *                    it's triggered by succesfull invocation of
 *                    tc_directory_sortbuf (see above) before the calling of
 *                    this function. When in buffered mode, this function
 *                    will return the full path of each entry in target
 *                    directory in lexicogrpaphical order.
 *                    See documentation of tc_directory_sdortbuf for details.
 *
 * Parameters: tcdir: TcDirectory structure (descriptor) to use.
 * Return Value: a constant pointer to full path of next entry
 *               NULL there are no more entries, or if an internal
 *               error occurs.
 * Side effects: in unbuffered mode, target directory will be scanned
 *               one time.
 * Preconditions: referred directory *MUST NOT CHANGE* until descriptor
 *                will be closed via tc_directory_close().
 *                'tcdir' was initialized calling tc_directory_open().
 * Postconditions: none
 */
const char *tc_directory_scan(TcDirectory *tcdir);

/*
 * tc_directory_close: finalize a TcDirectory structure (descriptor),
 *                     freeing all acquired resources.
 *
 * Parameters: tcdir: TcDirectory structure (descriptor) to close.
 * Return Value: none
 * Side effects: none
 * Preconditions: referred directory *MUST NOT BE CHANGED* until now.
 *                'tcdir' was initialized calling tc_directory_open()
 * Postconditions: none
 */
void tc_directory_close(TcDirectory *tcdir);

/*
 * tc_directory_file_count: return the actual count of files in target
 *                          directory. This value *can* change
 *                          between two or more invocation of
 *                          tc_directory_scan without previous calling
 *                          tc_directory_sortbuf.
 *                          See documentation of above two functions
 *                          for details.
 *
 * Parameters: tcdir: TcDirectory structure (descriptor) to use.
 * Return Value: actual count of files in target directory
 *               -1 if 'tcdir' is an invalid descriptor
 * Side effects: none
 * Preconditions: referred directory *MUST NOT CHANGE* until descriptor
 *                will be closed via tc_directory_close().
 *                'tcdir' was initialized calling tc_directory_open().
 * Postconditions: none
 */
int tc_directory_file_count(TcDirectory *tcdir);
#endif
