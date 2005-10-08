/*
 * average.c -- average two sets of byte data
 * Written by Andrew Church <achurch@achurch.org>
 */

#include "ac.h"
#include "ac_internal.h"

static void average(const uint8_t *, const uint8_t *, uint8_t *, int);
static void (*average_ptr)(const uint8_t *, const uint8_t *, uint8_t *, int)
     = average;

/*************************************************************************/

/* External interface */

void ac_average(const uint8_t *src1, const uint8_t *src2,
		uint8_t *dest, int bytes)
{
    (*average_ptr)(src1, src2, dest, bytes);
}

/*************************************************************************/
/*************************************************************************/

/* Vanilla C version */

static void average(const uint8_t *src1, const uint8_t *src2,
		    uint8_t *dest, int bytes)
{
    int i;
    for (i = 0; i < bytes; i++)
	dest[i] = (src1[i]+src2[i]) / 2;
}

/*************************************************************************/

#ifdef ARCH_X86

/* MMX-optimized */

static void average_mmx(const uint8_t *src1, const uint8_t *src2,
			uint8_t *dest, int bytes)
{
    if (bytes >= 8) {
	asm("\
	    pxor %%mm7, %%mm7						\n\
	    0:								\n\
	    movq (%%esi,%%eax), %%mm0					\n\
	    movq %%mm0, %%mm1						\n\
	    punpcklbw %%mm7, %%mm0					\n\
	    punpckhbw %%mm7, %%mm1					\n\
	    movq (%%edx,%%eax), %%mm2					\n\
	    movq %%mm2, %%mm3						\n\
	    punpcklbw %%mm7, %%mm2					\n\
	    punpckhbw %%mm7, %%mm3					\n\
	    paddw %%mm2, %%mm0						\n\
	    psrlw $1, %%mm0						\n\
	    paddw %%mm3, %%mm1						\n\
	    psrlw $1, %%mm1						\n\
	    packuswb %%mm1, %%mm0					\n\
	    movq %%mm0, (%%edi,%%eax)					\n\
	    subl $8, %%eax						\n\
	    jnz	0b							\n\
	    emms"
	    : /* no outputs */
	    : "S" (src1), "d" (src2), "D" (dest), "a" ((bytes & ~7) - 8));
    }
    if (UNLIKELY(bytes & 7)) {
	average(src1+(bytes & ~7), src2+(bytes & ~7), dest+(bytes & ~7),
		bytes & 7);
    }
}


/* SSE-optimized (PAVGB available) */

static void average_sse(const uint8_t *src1, const uint8_t *src2,
			uint8_t *dest, int bytes)
{
    if (bytes >= 8) {
	asm("\
	    testl $~0x1F, %%eax						\n\
	    jz 1f							\n\
	    0:								\n\
	    movq (%%esi,%%eax), %%mm0					\n\
	    movq 8(%%esi,%%eax), %%mm1					\n\
	    movq 16(%%esi,%%eax), %%mm2					\n\
	    movq 24(%%esi,%%eax), %%mm3					\n\
	    movq (%%edx,%%eax), %%mm4					\n\
	    pavgb %%mm4, %%mm0						\n\
	    movq 8(%%edx,%%eax), %%mm5					\n\
	    pavgb %%mm5, %%mm1						\n\
	    movq 16(%%edx,%%eax), %%mm6					\n\
	    pavgb %%mm6, %%mm2						\n\
	    movq 24(%%edx,%%eax), %%mm7					\n\
	    pavgb %%mm7, %%mm3						\n\
	    movntq %%mm0, (%%edi,%%eax)					\n\
	    movntq %%mm1, 8(%%edi,%%eax)				\n\
	    movntq %%mm2, 16(%%edi,%%eax)				\n\
	    movntq %%mm3, 24(%%edi,%%eax)				\n\
	    subl $32, %%eax						\n\
	    testl $~0x1F, %%eax						\n\
	    jnz	0b							\n\
	    testl %%eax, %%eax						\n\
	    jz 2f							\n\
	    1:								\n\
	    movq (%%esi,%%eax), %%mm0					\n\
	    movq (%%edx,%%eax), %%mm1					\n\
	    pavgb %%mm1, %%mm0						\n\
	    movntq %%mm0, (%%edi,%%eax)					\n\
	    subl $8, %%eax						\n\
	    jnz 1b							\n\
	    2:								\n\
	    emms							\n\
	    sfence"
	    : /* no outputs */
	    : "S" (src1), "d" (src2), "D" (dest), "a" ((bytes & ~7) - 8));
    }
    if (UNLIKELY(bytes & 7)) {
	average(src1+(bytes & ~7), src2+(bytes & ~7), dest+(bytes & ~7),
		bytes & 7);
    }
}

#endif  /* ARCH_X86 */

/*************************************************************************/

#if defined(ARCH_X86) || defined(ARCH_X86_64)

#ifdef ARCH_X86_64
# define EAX "%%rax"
# define EDX "%%rdx"
# define ESI "%%rsi"
# define EDI "%%rdi"
#else
# define EAX "%%eax"
# define EDX "%%edx"
# define ESI "%%esi"
# define EDI "%%edi"
#endif

static void average_sse2(const uint8_t *src1, const uint8_t *src2,
			 uint8_t *dest, int bytes)
{
    if (bytes >= 8) {
	asm("\
	    testl $~0x3F, %%eax						\n\
	    jz 1f							\n\
	    0:								\n\
	    movdqu ("ESI","EAX"), %%xmm0				\n\
	    movdqu 16("ESI","EAX"), %%xmm1				\n\
	    movdqu 32("ESI","EAX"), %%xmm2				\n\
	    movdqu 48("ESI","EAX"), %%xmm3				\n\
	    movdqu ("EDX","EAX"), %%xmm4				\n\
	    pavgb %%xmm4, %%xmm0					\n\
	    movdqu 16("EDX","EAX"), %%xmm5				\n\
	    pavgb %%xmm5, %%xmm1					\n\
	    movdqu 32("EDX","EAX"), %%xmm6				\n\
	    pavgb %%xmm6, %%xmm2					\n\
	    movdqu 48("EDX","EAX"), %%xmm7				\n\
	    pavgb %%xmm7, %%xmm3					\n\
	    movntdq %%xmm0, ("EDI","EAX")				\n\
	    movntdq %%xmm1, 16("EDI","EAX")				\n\
	    movntdq %%xmm2, 32("EDI","EAX")				\n\
	    movntdq %%xmm3, 48("EDI","EAX")				\n\
	    subl $64, %%eax						\n\
	    testl $~0x3F, %%eax						\n\
	    jnz	0b							\n\
	    testl %%eax, %%eax						\n\
	    jz 2f							\n\
	    1:								\n\
	    movq ("ESI","EAX"), %%mm0					\n\
	    movq ("EDX","EAX"), %%mm1					\n\
	    pavgb %%mm1, %%mm0						\n\
	    movntq %%mm0, ("EDI","EAX")					\n\
	    subl $8, %%eax						\n\
	    jnz 1b							\n\
	    2:								\n\
	    emms							\n\
	    sfence"
	    : /* no outputs */
	    : "S" (src1), "d" (src2), "D" (dest), "a" ((bytes & ~7) - 8));
    }
    if (UNLIKELY(bytes & 7)) {
	average(src1+(bytes & ~7), src2+(bytes & ~7), dest+(bytes & ~7),
		bytes & 7);
    }
}

#endif  /* ARCH_X86 || ARCH_X86_64 */

/*************************************************************************/
/*************************************************************************/

/* Initialization routine. */

int ac_average_init(int accel)
{
#if defined(ARCH_X86)
    if (HAS_ACCEL(accel, AC_MMX))
	average_ptr = average_mmx;
    if (HAS_ACCEL(accel, AC_SSE))
	average_ptr = average_sse;
#endif
#if defined(ARCH_X86) || defined(ARCH_X86_64)
    if (HAS_ACCEL(accel, AC_SSE2))
	average_ptr = average_sse2;
#endif

    return 1;
}

/*************************************************************************/
