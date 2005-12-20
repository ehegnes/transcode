/*compile-command
set -x
gcc -O3 -g -I. -I.. "$0" -DARCH_X86
exit $?
*/

/*
 * test-memcpy.c - test a particular memcpy() implementation to check that
 *                 it works with all alignments and sizes
 * Written by Andrew Church <achurch@achurch.org>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#define FUNCTION_TO_TEST memcpy_sse

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "config.h"

#include "memcpy.c"

static const int spill = 8;  // extra bytes to check for memcpy overrun
static sigjmp_buf env;
static void *old_SIGSEGV = NULL, *old_SIGILL = NULL;

static void sighandler(int sig)
{
    printf("*** %s\n", strsignal(sig));
    siglongjmp(env, 1);
}

static void set_signals(void)
{
    old_SIGSEGV = signal(SIGSEGV, sighandler);
    old_SIGILL  = signal(SIGILL , sighandler);
}

static void clear_signals(void)
{
    signal(SIGSEGV, old_SIGSEGV);
    signal(SIGILL , old_SIGILL );
}

/* align1 and align2 are 0..63 */
int testit(void *(*func)(void *, const void *, size_t), int align1, int align2, int length)
{
    char *chunk1, *chunk1_base;
    char *chunk2, *chunk2_base;
    char *chunkT;
    int result;

    chunk1_base = malloc(length+128+spill);
    chunk2_base = malloc(length+128+spill);
    chunk1 = (char *)((long)chunk1_base+63 & -64) + align1;
    chunk2 = (char *)((long)chunk2_base+63 & -64) + align2;

    memset(chunk1, 0x11, length+spill);
    memset(chunk2, 0x22, length+spill);

    chunkT = malloc(length+spill);
    memset(chunkT, 0x11, length);
    memset(chunkT+length, 0x22, spill);

    set_signals();
    if (sigsetjmp(env, 1)) {
        result = 0;
    } else {
        (*func)(chunk2, chunk1, length);
        result = (memcmp(chunk2, chunkT, length+spill) == 0);
    }
    clear_signals();

    free(chunk1_base);
    free(chunk2_base);
    free(chunkT);
    return result;
}

int main(int ac, char **av)
{
    int len, ofs1, ofs2;

    for (len = 1; ; len++) {
        fprintf(stderr, "%d\r", len);
        for (ofs1 = 0; ofs1 < 64; ofs1++) {
            for (ofs2 = 0; ofs2 < 64; ofs2++) {
                if (!testit(FUNCTION_TO_TEST, ofs1, ofs2, len))
                    printf("FAILED: ofs1=%d ofs2=%d len=%d\n",ofs1,ofs2,len);
            }
        }
    }
    return 0;
}

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
