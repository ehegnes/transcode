/*compile-command
set -x
gcc -O3 -g "$0" -DARCH_X86
exit $?
*/

/* Time all ac_imgconvert() implementations. */

#define WIDTH		768
#define HEIGHT		512
#define ITERATIONS	50	/* Minimum # of iterations */
#define MINTIME		100	/* Minmum msec to iterate */


#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "imgconvert.c"
#include "img_rgb_packed.c"
#include "img_yuv_mixed.c"
#include "img_yuv_packed.c"
#include "img_yuv_planar.c"
#include "img_yuv_rgb.c"
#include "memcpy.c"  /* used by some conversion functions */


#ifndef LINUX
typedef void (*sighandler_t)(int);
#endif
static sighandler_t old_SIGSEGV, old_SIGILL;
static int sigsave;
static sigjmp_buf env;

static void sighandler(int sig)
{
    sigsave = sig;
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

/* Return value: >=0 is time/iteration in usec, <0 is error
 *   -1: unknown error
 *   -2: ac_imgconvert_init(0) failed
 *   -3: ac_imgconvert_init(accel) failed
 *   -4: ac_imgconvert(0) failed
 *   -5: ac_imgconvert(accel) failed
 *   -6: compare failed
 *   -7: SIGSEGV
 *   -8: SIGILL
 */
int testit(uint8_t *srcimage, ImageFormat srcfmt, ImageFormat destfmt, int accel)
{
    static uint8_t srcbuf[WIDTH*HEIGHT*4], destbuf[WIDTH*HEIGHT*4], cmpbuf[WIDTH*HEIGHT*4];
    uint8_t *src[3], *dest[3];
    long long tdiff;
    unsigned long long start, stop;
    struct rusage ru;
    int i, icnt;

    memset(destbuf, 0, sizeof(destbuf));
    memset(cmpbuf, 0, sizeof(cmpbuf));

    sigsave = 0;
    set_signals();
    if (sigsetjmp(env, 1)) {
	clear_signals();
	return sigsave==SIGILL ? -8 : -7;
    }

    ac_memcpy_init(0);
    if (!ac_imgconvert_init(0))
	return -2;
    ac_memcpy(srcbuf, srcimage, sizeof(srcbuf));
    src[0] = srcbuf;
    if (IS_YUV_FORMAT(srcfmt))
	YUV_INIT_PLANES(src, srcbuf, srcfmt, WIDTH, HEIGHT);
    dest[0] = cmpbuf;
    if (IS_YUV_FORMAT(destfmt))
	YUV_INIT_PLANES(dest, cmpbuf, destfmt, WIDTH, HEIGHT);
    if (!ac_imgconvert(src, srcfmt, dest, destfmt, WIDTH, HEIGHT))
	return -4;

    ac_memcpy_init(accel);
    if (!ac_imgconvert_init(accel))
	return -3;
    // currently src can get destroyed--see img_yuv_mixed.c
    ac_memcpy(srcbuf, srcimage, sizeof(srcbuf));
    dest[0] = destbuf;
    if (IS_YUV_FORMAT(destfmt))
	YUV_INIT_PLANES(dest, destbuf, destfmt, WIDTH, HEIGHT);
    if (!ac_imgconvert(src, srcfmt, dest, destfmt, WIDTH, HEIGHT))
	return -5;

    tdiff = 0;
    for (i = 0; i < sizeof(destbuf); i++) {
	int diff = (int)destbuf[i] - (int)cmpbuf[i];
	if (diff < -1 || diff > 1)
	    return -6;
	tdiff += diff*diff;
    }
    if (tdiff >= WIDTH*HEIGHT/2)
	return -6;

    getrusage(RUSAGE_SELF, &ru);
    start = ru.ru_utime.tv_sec * 1000000ULL + ru.ru_utime.tv_usec;
    icnt = 0;
    do {
	for (i = 0; i < ITERATIONS; i++)
	    ac_imgconvert(src, srcfmt, dest, destfmt, WIDTH, HEIGHT);
	getrusage(RUSAGE_SELF, &ru);
	stop = ru.ru_utime.tv_sec * 1000000ULL + ru.ru_utime.tv_usec;
	icnt += ITERATIONS;
    } while (stop-start < MINTIME*1000);

    clear_signals();
    return (stop-start + icnt/2) / icnt;
}

/* Order of formats to test, with name strings; comment out ones you're
 * not interested in */
struct { ImageFormat fmt; const char *name; } fmtlist[] = {
    { IMG_YUV420P, "420P" },
    { IMG_YV12,    "YV12" },
    { IMG_YUV411P, "411P" },
    { IMG_YUV422P, "422P" },
    { IMG_YUV444P, "444P" },
    { IMG_YUY2,    "YUY2" },
    { IMG_UYVY,    "UYVY" },
    { IMG_YVYU,    "YVYU" },
//    { IMG_Y8,      " Y8 " },
    { IMG_RGB24,   "RGB " },
    { IMG_BGR24,   "BGR " },
    { IMG_RGBA32,  "RGBA" },
    { IMG_ABGR32,  "ABGR" },
    { IMG_ARGB32,  "ARGB" },
    { IMG_BGRA32,  "BGRA" },
//    { IMG_GRAY8,   "GRAY" },
    { IMG_NONE,    NULL }
};

int main(int argc, char **argv)
{
    static uint8_t srcbuf[WIDTH*HEIGHT*4];
    int accel = 0, compare = 0;
    int i, j;

    while (argc > 1) {
	if (strcmp(argv[--argc],"-h") == 0) {
	    fprintf(stderr, "Usage: %s [-c] [accel-name...]", argv[0]);
	    fprintf(stderr, "-c: compare with non-accelerated versions and report percentage speedup\n");
	    fprintf(stderr, "accel-name can be ia32asm, amd64asm, cmove, mmx, ...\n");
	    return 0;
	}
	if (strcmp(argv[argc],"-c") == 0)
	    compare = 1;
	else if (strcmp(argv[argc],"ia32asm") == 0)
	    accel |= AC_IA32ASM;
	else if (strcmp(argv[argc],"amd64asm") == 0)
	    accel |= AC_AMD64ASM;
	else if (strcmp(argv[argc],"cmove") == 0)
	    accel |= AC_CMOVE;
	else if (strcmp(argv[argc],"mmx") == 0)
	    accel |= AC_MMX;
	else if (strcmp(argv[argc],"mmxext") == 0)
	    accel |= AC_MMXEXT;
	else if (strcmp(argv[argc],"3dnow") == 0)
	    accel |= AC_3DNOW;
	else if (strcmp(argv[argc],"3dnowext") == 0)
	    accel |= AC_3DNOWEXT;
	else if (strcmp(argv[argc],"sse") == 0)
	    accel |= AC_SSE;
	else if (strcmp(argv[argc],"sse2") == 0)
	    accel |= AC_SSE2;
	else if (strcmp(argv[argc],"sse3") == 0)
	    accel |= AC_SSE3;
	else {
	    fprintf(stderr, "Unknown accel type `%s'\n", argv[argc]);
	    fprintf(stderr, "`%s -h' for help.\n", argv[0]);
	    return 1;
	}
    }

    srandom(0);  /* to give a standard "image" */
    for (i = 0; i < sizeof(srcbuf); i++)
	srcbuf[i] = random();

    printf("Acceleration flags:%s%s%s%s%s%s%s%s%s%s\n",
	   !accel                ? " none"     : "",
	   (accel & AC_IA32ASM ) ? " ia32asm"  : "",
	   (accel & AC_AMD64ASM) ? " amd64asm" : "",
	   (accel & AC_CMOVE   ) ? " cmove"    : "",
	   (accel & AC_MMX     ) ? " mmx"      : "",
	   (accel & AC_MMXEXT  ) ? " mmxext"   : "",
	   (accel & AC_3DNOW   ) ? " 3dnow"    : "",
	   (accel & AC_SSE     ) ? " sse"      : "",
	   (accel & AC_SSE2    ) ? " sse2"     : "",
	   (accel & AC_SSE3    ) ? " sse3"     : "");
    if (compare)
	printf("Units: conversions/time (unaccelerated = 100)\n\n");
    else
	printf("Units: conversions/sec (frame size: %dx%d)\n\n", WIDTH, HEIGHT);
    printf("     |");
    for (i = 0; fmtlist[i].fmt != IMG_NONE; i++)
	printf("%-4s|", fmtlist[i].name);
    printf("\n-----+");
    for (i = 0; fmtlist[i].fmt != IMG_NONE; i++)
	printf("----+");
    printf("\n");

    for (i = 0; fmtlist[i].fmt != IMG_NONE; i++) {
	printf("%-5s|", fmtlist[i].name);
	fflush(stdout);
	for (j = 0; fmtlist[j].fmt != IMG_NONE; j++) {
	    int res = testit(srcbuf, fmtlist[i].fmt, fmtlist[j].fmt, accel);
	    switch (res) {
	        case -1:
	        case -2:
	        case -3:
	        case -4:
	        case -5: printf("----|"); break;
	        case -6: printf("BAD |"); break;
	        case -7: printf("SEGV|"); break;
	        case -8: printf("ILL |"); break;
	        default:
		    if (compare) {
			int res0 = testit(srcbuf, fmtlist[i].fmt,
					  fmtlist[j].fmt, 0);
			if (res0 < 0)
			    printf("****|");
			else
			    printf("%4d|", (100*res0 + res/2) / res);
		    } else {
			printf("%4d|", (1000000+res/2)/res);
		    }
		    break;
	    }
	    fflush(stdout);
	}
	printf("\n");
    }

    return 0;
}
