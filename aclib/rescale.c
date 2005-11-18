/*
 * rescale.c -- take the weighted average of two sets of byte data
 * Written by Andrew Church <achurch@achurch.org>
 */

#include "ac.h"
#include "ac_internal.h"

static void rescale(const uint8_t *, const uint8_t *, uint8_t *, int,
                    uint32_t, uint32_t);
static void (*rescale_ptr)(const uint8_t *, const uint8_t *, uint8_t *, int,
                           uint32_t, uint32_t) = rescale;

/*************************************************************************/

/* External interface */

void ac_rescale(const uint8_t *src1, const uint8_t *src2,
                uint8_t *dest, int bytes, uint32_t weight1, uint32_t weight2)
{
    if (weight1 >= 0x10000)
        ac_memcpy(dest, src1, bytes);
    else if (weight2 >= 0x10000)
        ac_memcpy(dest, src2, bytes);
    else
        (*rescale_ptr)(src1, src2, dest, bytes, weight1, weight2);
}

/*************************************************************************/
/*************************************************************************/

/* Vanilla C version */

static void rescale(const uint8_t *src1, const uint8_t *src2,
                    uint8_t *dest, int bytes,
                    uint32_t weight1, uint32_t weight2)
{
    int i;
    for (i = 0; i < bytes; i++)
        dest[i] = (src1[i]*weight1 + src2[i]*weight2 + 32768) >> 16;
}

/*************************************************************************/

#ifdef ARCH_X86

/* MMX version */

static void rescale_mmx(const uint8_t *src1, const uint8_t *src2,
                        uint8_t *dest, int bytes,
                        uint32_t weight1, uint32_t weight2)
{
    if (bytes >= 8) {
        /* First store weights in MM4/MM5 to relieve register pressure;
         * save time by making 2 copies ahead of time in the general
         * registers.  Note that we shift right by 1 for MMX due to the
         * lack of an unsigned SIMD multiply instruction (PMULHUW). */
        asm("movd %%eax, %%mm4; movd %%edx, %%mm5"
            : : "a" ((weight1>>1)<<16|(weight1>>1)),
                "d" ((weight2>>1)<<16|(weight2>>1)));
        asm("\
            movq %%mm4, %%mm6           # MM6: 00 00 W1 W1              \n\
            psllq $32, %%mm4            # MM4: W1 W1 00 00              \n\
            por %%mm6, %%mm4            # MM4: W1 W1 W1 W1              \n\
            movq %%mm5, %%mm7           # MM7: 00 00 W2 W2              \n\
            psllq $32, %%mm5            # MM5: W2 W2 00 00              \n\
            por %%mm7, %%mm5            # MM5: W2 W2 W2 W2              \n\
            pxor %%mm7, %%mm7           # MM7: 00 00 00 00              \n\
            pxor %%mm6, %%mm6           # Put 0x0080*4 in MM6 (rounding)\n\
            pcmpeqw %%mm3, %%mm3                                        \n\
            psubw %%mm3, %%mm6                                          \n\
            psllw $7, %%mm6                                             \n\
            0:                                                          \n\
            movq -8(%%esi,%%ecx), %%mm0                                 \n\
            movq %%mm0, %%mm1                                           \n\
            punpcklbw %%mm7, %%mm0                                      \n\
            psllw $1, %%mm0             # Compensate for halved weights \n\
            pmulhw %%mm4, %%mm0         # And multiply                  \n\
            punpckhbw %%mm7, %%mm1                                      \n\
            psllw $1, %%mm1                                             \n\
            pmulhw %%mm4, %%mm1                                         \n\
            movq -8(%%edx,%%ecx), %%mm2                                 \n\
            movq %%mm2, %%mm3                                           \n\
            punpcklbw %%mm7, %%mm2                                      \n\
            psllw $1, %%mm2                                             \n\
            pmulhw %%mm5, %%mm2                                         \n\
            punpckhbw %%mm7, %%mm3                                      \n\
            psllw $1, %%mm3                                             \n\
            pmulhw %%mm5, %%mm3                                         \n\
            paddw %%mm2, %%mm0                                          \n\
            paddw %%mm3, %%mm1                                          \n\
            paddw %%mm6, %%mm0                                          \n\
            paddw %%mm6, %%mm1                                          \n\
            packuswb %%mm1, %%mm0                                       \n\
            movq %%mm0, -8(%%edi,%%ecx)                                 \n\
            subl $8, %%ecx                                              \n\
            jnz 0b                                                      \n\
            emms"
            : /* no outputs */
            : "S" (src1), "d" (src2), "D" (dest), "c" (bytes & ~7));
    }
    if (UNLIKELY(bytes & 7)) {
        rescale(src1+(bytes & ~7), src2+(bytes & ~7), dest+(bytes & ~7),
                bytes & 7, weight1, weight2);
    }
}


/* MMXEXT version (also for SSE) */

static void rescale_mmxext(const uint8_t *src1, const uint8_t *src2,
                           uint8_t *dest, int bytes,
                           uint32_t weight1, uint32_t weight2)
{
    if (bytes >= 8) {
        asm("movd %%eax, %%mm4; movd %%edx, %%mm5"
            : : "a" (weight1), "d" (weight2));
        asm("\
            pshufw $0, %%mm4, %%mm4     # MM4: W1 W1 W1 W1              \n\
            pshufw $0, %%mm5, %%mm5     # MM5: W2 W2 W2 W2              \n\
            pxor %%mm6, %%mm6           # Put 0x0080*4 in MM6 (rounding)\n\
            pcmpeqw %%mm7, %%mm7                                        \n\
            psubw %%mm7, %%mm6                                          \n\
            psllw $7, %%mm6                                             \n\
            0:                                                          \n\
            movq -8(%%esi,%%ecx), %%mm7                                 \n\
            pxor %%mm0, %%mm0           # Load data into high bytes     \n\
            punpcklbw %%mm7, %%mm0      # (gives 8.8 fixed point)       \n\
            pmulhuw %%mm4, %%mm0        # Result: 0000..FF00            \n\
            pxor %%mm1, %%mm1                                           \n\
            punpckhbw %%mm6, %%mm1                                      \n\
            pmulhuw %%mm4, %%mm1                                        \n\
            movq -8(%%edx,%%ecx), %%mm7                                 \n\
            pxor %%mm2, %%mm2                                           \n\
            punpcklbw %%mm7, %%mm2                                      \n\
            pmulhuw %%mm5, %%mm2                                        \n\
            pxor %%mm3, %%mm3                                           \n\
            punpckhbw %%mm7, %%mm3                                      \n\
            pmulhuw %%mm5, %%mm3                                        \n\
            paddw %%mm2, %%mm0                                          \n\
            paddw %%mm6, %%mm0                                          \n\
            psrlw $8, %%mm0             # Shift back down to 00..FF     \n\
            paddw %%mm3, %%mm1                                          \n\
            paddw %%mm6, %%mm1                                          \n\
            psrlw $8, %%mm1                                             \n\
            packuswb %%mm1, %%mm0                                       \n\
            movq %%mm0, -8(%%edi,%%ecx)                                 \n\
            subl $8, %%ecx                                              \n\
            jnz 0b                                                      \n\
            emms"
            : /* no outputs */
            : "S" (src1), "d" (src2), "D" (dest), "c" (bytes & ~7));
    }
    if (UNLIKELY(bytes & 7)) {
        rescale(src1+(bytes & ~7), src2+(bytes & ~7), dest+(bytes & ~7),
                bytes & 7, weight1, weight2);
    }
}

#endif  /* ARCH_X86 */

/*************************************************************************/

#if defined(ARCH_X86) || defined(ARCH_X86_64)

/* SSE2 version */

#ifdef ARCH_X86_64
# define ECX "%%rcx"
# define EDX "%%rdx"
# define ESI "%%rsi"
# define EDI "%%rdi"
#else
# define ECX "%%ecx"
# define EDX "%%edx"
# define ESI "%%esi"
# define EDI "%%edi"
#endif

static void rescale_sse2(const uint8_t *src1, const uint8_t *src2,
                         uint8_t *dest, int bytes,
                         uint32_t weight1, uint32_t weight2)
{
    if (bytes >= 16) {
        asm("movd %%eax, %%xmm4; movd %%edx, %%xmm5"
            : : "a" (weight1<<16|weight1), "d" (weight2<<16|weight2));
        asm("\
            pshufd $0, %%xmm4, %%xmm4   # XMM4: W1 W1 W1 W1 W1 W1 W1 W1 \n\
            pshufd $0, %%xmm5, %%xmm5   # XMM5: W2 W2 W2 W2 W2 W2 W2 W2 \n\
            pxor %%xmm6, %%xmm6         # Put 0x0080*4 in XMM6 (rounding)\n\
            pcmpeqw %%xmm7, %%xmm7                                      \n\
            psubw %%xmm7, %%xmm6                                        \n\
            psllw $7, %%xmm6                                            \n\
            0:                                                          \n\
            movdqu -16("ESI","ECX"), %%xmm7                             \n\
            pxor %%xmm0, %%xmm0                                         \n\
            punpcklbw %%xmm7, %%xmm0                                    \n\
            pmulhuw %%xmm4, %%xmm0                                      \n\
            pxor %%xmm1, %%xmm1                                         \n\
            punpckhbw %%xmm7, %%xmm1                                    \n\
            pmulhuw %%xmm4, %%xmm1                                      \n\
            movdqu -16("EDX","ECX"), %%xmm7                             \n\
            pxor %%xmm2, %%xmm2                                         \n\
            punpcklbw %%xmm7, %%xmm2                                    \n\
            pmulhuw %%xmm5, %%xmm2                                      \n\
            pxor %%xmm3, %%xmm3                                         \n\
            punpckhbw %%xmm7, %%xmm3                                    \n\
            pmulhuw %%xmm5, %%xmm3                                      \n\
            paddw %%xmm2, %%xmm0                                        \n\
            paddw %%xmm6, %%xmm0                                        \n\
            psrlw $8, %%xmm0                                            \n\
            paddw %%xmm3, %%xmm1                                        \n\
            paddw %%xmm6, %%xmm1                                        \n\
            psrlw $8, %%xmm1                                            \n\
            packuswb %%xmm1, %%xmm0                                     \n\
            movdqu %%xmm0, -16("EDI","ECX")                             \n\
            subl $16, %%ecx                                             \n\
            jnz 0b                                                      \n\
            emms"
            : /* no outputs */
            : "S" (src1), "d" (src2), "D" (dest), "c" (bytes & ~15));
    }
    if (UNLIKELY(bytes & 15)) {
        rescale(src1+(bytes & ~15), src2+(bytes & ~15), dest+(bytes & ~15),
                bytes & 15, weight1, weight2);
    }
}

#endif  /* ARCH_X86 || ARCH_X86_64 */

/*************************************************************************/
/*************************************************************************/

/* Initialization routine. */

int ac_rescale_init(int accel)
{
    rescale_ptr = rescale;

#if defined(ARCH_X86)
    if (HAS_ACCEL(accel, AC_MMX))
        rescale_ptr = rescale_mmx;
    if (HAS_ACCEL(accel, AC_MMXEXT) || HAS_ACCEL(accel, AC_SSE))
        rescale_ptr = rescale_mmxext;
#endif
#if defined(ARCH_X86) || defined(ARCH_X86_64)
    if (HAS_ACCEL(accel, AC_SSE2))
        rescale_ptr = rescale_sse2;
#endif

    return 1;
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
