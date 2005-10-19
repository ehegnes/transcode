/*compile-command
set -x
gcc -O3 "$0" -DARCH_X86
exit $?
*/

/* Test a particular memcpy() implementation to check that it works with
 * all alignments and sizes. */

#define FUNCTION_TO_TEST memcpy_sse

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>

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
        char * chunk1, * chunk1_base;
        char * chunk2, * chunk2_base;
        char * chunkT;
//      int             ix;
        int             result;
//      unsigned long long start, stop;
//      struct rusage ru;

        chunk1_base = malloc(length+128+spill);
        chunk2_base = malloc(length+128+spill);
        chunk1 = (char *)((int)chunk1_base+63 & -64) + align1;
        chunk2 = (char *)((int)chunk2_base+63 & -64) + align2;

        memset(chunk1, 0x11, length+spill);
        memset(chunk2, 0x22, length+spill);

        chunkT = malloc(length+spill);
        memset(chunkT, 0x11, length);
        memset(chunkT+length, 0x22, spill);

        set_signals();
        if (sigsetjmp(env, 1)) {
                result = 0;
        } else {
//              getrusage(RUSAGE_SELF, &ru);
//              start = ru.ru_utime.tv_sec * 1000 + ru.ru_utime.tv_usec / 1000;

//              for(ix = 0; ix < it; ix++)
                        func(chunk2, chunk1, length);

//              getrusage(RUSAGE_SELF, &ru);
//              stop = ru.ru_utime.tv_sec * 1000 + ru.ru_utime.tv_usec / 1000;

                result = (memcmp(chunk2, chunkT, length+spill) == 0);
//              if (stop == start)
//                      stop++;
//              *msecs = stop - start;
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
