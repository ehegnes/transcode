/*
 * cpu_accel.c
 * Copyright (C) 1999-2001 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <inttypes.h>
#include <signal.h>
#include <setjmp.h>

#include "mm_accel.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)

  /*  Some miscelaneous stuff to allow checking whether SSE instructions cause
   *  illegal instruction errors.
   */

static sigjmp_buf sigill_recover;

static RETSIGTYPE sigillhandler(int sig)
{
    siglongjmp(sigill_recover, 1);
}

typedef RETSIGTYPE (*__sig_t)(int);

static int testsseill()
{
    int illegal;
#if defined(__CYGWIN__)
    /*  SSE causes a crash on CYGWIN, apparently.
     *  Perhaps the wrong signal is being caught or something along
     *  those line ;-) or maybe SSE itself won't work...
     */

    illegal = 1;
#else
    __sig_t old_handler = signal( SIGILL, sigillhandler);
    if (sigsetjmp(sigill_recover, 1) == 0) {
        asm ("movups %xmm0, %xmm0");
        illegal = 0;
    } else
        illegal = 1;

    signal(SIGILL, old_handler);
#endif

    return illegal;
}


static uint32_t arch_accel (void)
{
    long eax, ebx, ecx, edx;
    int32_t AMD;
    int32_t caps;

	/* Slightly weirdified cpuid that preserves the ebx and edi required
	   by gcc for PIC offset table and frame pointer */

#ifdef __LP64__
#  define REG_b "rbx"
#  define REG_S "rsi"
#else
#  define REG_b "ebx"
#  define REG_S "esi"
#endif
	   
#define cpuid(op,eax,ebx,ecx,edx)	\
    asm ("push %%"REG_b"\n"		\
	 "cpuid\n"			\
	 "mov   %%"REG_b", %%"REG_S"\n" \
	 "pop   %%"REG_b"\n"		\
	 : "=a" (eax),			\
	   "=S" (ebx),			\
	   "=c" (ecx),			\
	   "=d" (edx)			\
	 : "a" (op)			\
	 : "cc", "edi")

    asm ("pushf\n\t"
	 "pop %0\n\t"
	 "mov %0,%1\n\t"
	 "xor $0x200000,%0\n\t"
	 "push %0\n\t"
	 "popf\n\t"
	 "pushf\n\t"
	 "pop %0"
         : "=a" (eax),
	   "=c" (ecx)
	 :
	 : "cc");


    if (eax == ecx)  /* no cpuid */
        return 0;

    cpuid (0x00000000, eax, ebx, ecx, edx);
    if (!eax)  /* vendor string only */
	return 0;

    AMD = (ebx == 0x68747541) && (ecx == 0x444d4163) && (edx == 0x69746e65);

    cpuid (0x00000001, eax, ebx, ecx, edx);
    if (! (edx & 0x00800000))  /* no MMX */
        return 0;

    caps = MM_ACCEL_X86_MMX;

    /* If SSE capable CPU has same MMX extensions as AMD
       and then some. However, to use SSE O.S. must have signalled
       it use of FXSAVE/FXRSTOR through CR4.OSFXSR and hence FXSR (bit 24)
       here
     */

    if ((edx & 0x02000000))	
        caps = MM_ACCEL_X86_MMX | MM_ACCEL_X86_MMXEXT;

    if ((edx & 0x03000000) == 0x03000000 ) {

        /* Check whether O.S. has SSE support... has to be done with
           exception 'cos those Intel morons put the relevant bit
           in a reg that is only accesible in ring 0... doh! 
         */

        if (!testsseill())
            caps |= MM_ACCEL_X86_SSE;
    }

    cpuid (0x80000000, eax, ebx, ecx, edx);

    if (eax < 0x80000001)  /* no extended capabilities */
        return caps;

    cpuid (0x80000001, eax, ebx, ecx, edx);

    if (edx & 0x80000000)
        caps |= MM_ACCEL_X86_3DNOW;

    if (AMD && (edx & 0x00400000))  /* AMD MMX extensions */
        caps |= MM_ACCEL_X86_MMXEXT;

    return caps;
}
#endif

#ifdef ARCH_PPC
#ifdef HAVE_PPC_ALTIVEC
#include <signal.h>
#include <setjmp.h>

static sigjmp_buf jmpbuf;
static volatile sig_atomic_t canjump = 0;

static RETSIGTYPE sigill_handler (int sig)
{
    if (!canjump) {
	signal (sig, SIG_DFL);
	raise (sig);
    }

    canjump = 0;
    siglongjmp (jmpbuf, 1);
}

static uint32_t arch_accel (void)
{
    signal (SIGILL, sigill_handler);
    if (sigsetjmp (jmpbuf, 1)) {
	signal (SIGILL, SIG_DFL);
	return 0;
    }

    canjump = 1;

    asm volatile ("mtspr 256, %0\n\t"
		  "vand %%v0, %%v0, %%v0"
		  :
		  : "r" (-1));

    signal (SIGILL, SIG_DFL);
    return MM_ACCEL_PPC_ALTIVEC;
}
#endif /* HAVE_PPC_ALTIVEC */
#endif /* ARCH_PPC */

uint32_t mm_accel (void)
{
#if defined(ARCH_X86) || defined(ARCH_X86_64) || (defined (ARCH_PPC) && defined (HAVE_PPC_ALTIVEC))
    static int got_accel = 0;
    static uint32_t accel;

    if (!got_accel) {
	got_accel = 1;
	accel = arch_accel ();
    }

    return accel;
#else
    return 0;
#endif
}


