/*
 * tcglob.h - simple iterator over a path collection expressed through
 *            glob (7) semantic.
 * (C) 2007 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#ifndef TCGLOB_H
#define TCGLOB_H

/*
 * Quick Summary:
 *
 */

#include <stdint.h>


typedef struct tcglob_ TCGlob;


/*
 * tc_glob_open:
 *
 */
TCGlob *tc_glob_open(const char *pattern, uint32_t flags);


/*
 * tc_glob_next:
 */
const char *tc_glob_next(TCGlob *tcg);


/*
 * tc_glob_has_more:
 */
int tc_glob_has_more(TCGlob *tcg);


/*
 * tc_glob_close:
 */
int tc_glob_close(TCGlob *tcg);

#endif /* TCGLOB_H */
