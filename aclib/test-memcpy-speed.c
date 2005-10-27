/*compile-command
set -x
gcc -O3 -g -I. -I.. "$0" -DARCH_X86
exit $?
*/

/* Time all memcpy() implementations. */

#define TESTSIZE        0x10000
#define ITERATIONS      ((1<<28) / TESTSIZE)


#define _GNU_SOURCE

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

#ifndef LINUX
typedef void (*sighandler_t)(int);
#endif
static sighandler_t old_SIGSEGV, old_SIGILL;
static sigjmp_buf env;

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
void testit(void * (*func)(void *, const void *, size_t), int align1, int align2, int length, int * msecs, int * ok)
{
        char * chunk1, * chunk1_base;
        char * chunk2, * chunk2_base;
        char * chunkT;
        int             ix;
        int             result;
        unsigned long long start, stop;
        struct rusage ru;

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
                *ok = 0;
                *msecs = -1;
        } else {
                getrusage(RUSAGE_SELF, &ru);
                start = ru.ru_utime.tv_sec * 1000 + ru.ru_utime.tv_usec / 1000;

                for(ix = 0; ix < ITERATIONS; ix++)
                        func(chunk2, chunk1, length);

                getrusage(RUSAGE_SELF, &ru);
                stop = ru.ru_utime.tv_sec * 1000 + ru.ru_utime.tv_usec / 1000;

                *ok = (memcmp(chunk2, chunkT, length+spill) == 0);
                if (stop == start)
                        stop++;
                *msecs = stop - start;
        }
        clear_signals();

        free(chunk1_base);
        free(chunk2_base);
        free(chunkT);
}

/* Alignments/sizes to test */
static struct {
    int align1, align2, length;
} tests[] = {
    {  0,  0, TESTSIZE },
    {  0,  1, TESTSIZE },
    {  0,  4, TESTSIZE },
    {  0,  8, TESTSIZE },
    {  0, 63, TESTSIZE },
    {  1,  0, TESTSIZE },
    {  1,  1, TESTSIZE },
    {  1,  4, TESTSIZE },
    {  1,  8, TESTSIZE },
    {  1, 63, TESTSIZE },
    {  4,  0, TESTSIZE },
    {  4,  1, TESTSIZE },
    {  8,  0, TESTSIZE },
    {  8,  1, TESTSIZE },
    { 63,  0, TESTSIZE },
    { 63,  1, TESTSIZE },
    {  0,  0, TESTSIZE|63 },
    {  0,  1, TESTSIZE|63 },
    {  0,  4, TESTSIZE|63 },
    {  0,  8, TESTSIZE|63 },
    {  0, 63, TESTSIZE|63 },
    {  1,  0, TESTSIZE|63 },
    {  1,  1, TESTSIZE|63 },
    {  1,  4, TESTSIZE|63 },
    {  1,  8, TESTSIZE|63 },
    {  1, 63, TESTSIZE|63 },
    { -1, -1, -1 }
};

int main(int argc, char ** argv)
{
    int msecs, ok, i;

    printf("method     ok   msecs  iterations/sec\n");

    for (i = 0; tests[i].align1 >= 0; i++) {
        printf("[%d/%d/%#x]\n", tests[i].align1, tests[i].align2, tests[i].length);
        testit(memcpy, tests[i].align1, tests[i].align2, tests[i].length, &msecs, &ok);
        printf("* libc    %-3s %6d  %d\n", ok ? "yes" : "no ", msecs, ITERATIONS / msecs);
#ifdef ARCH_X86
        testit(memcpy_mmx, tests[i].align1, tests[i].align2, tests[i].length, &msecs, &ok);
        printf("* mmx     %-3s %6d  %d\n", ok ? "yes" : "no ", msecs, ITERATIONS / msecs);
        testit(memcpy_sse, tests[i].align1, tests[i].align2, tests[i].length, &msecs, &ok);
        printf("* sse     %-3s %6d  %d\n", ok ? "yes" : "no ", msecs, ITERATIONS / msecs);
#endif
#if defined(ARCH_X86_64)
        testit(memcpy_amd64, tests[i].align1, tests[i].align2, tests[i].length, &msecs, &ok);
        printf("* amd64   %-3s %6d  %d\n", ok ? "yes" : "no ", msecs, ITERATIONS / msecs);
#endif
    }
    return(0);
}
