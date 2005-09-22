/* -*- coding: iso-8859-1; -*-
 *
 *  rescale.c
 *
 *  Based on rescale.s:
 *  Copyright (C) Thomas Östreich - November 2002
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

#include "ac.h"

/*************************************************************************/

#ifdef ARCH_X86

int ac_rescale_mmxext(char *row1, char *row2, char *out, int bytes, 
		      unsigned long weight1, unsigned long weight2)
{
    /* Note that we could _probably_ just take &row1 and use offsets from
     * it instead, but some version of GCC is going to see that the rest of
     * the arguments aren't used and either spit out warnings or do other
     * weird and annoying things to us, so let's play it safe. --AC */
    struct {
	char *row1, *row2, *out;
	int bytes;
	unsigned long weight1, weight2;
    } args;
    args.row1 = row1;
    args.row2 = row2;
    args.out = out;
    args.bytes = bytes;
    args.weight1 = weight1;
    args.weight2 = weight2;

    asm("\
	push %%ebx							\n\
									\n\
	mov (%%esi), %%ebx	# row1					\n\
	mov 4(%%esi), %%eax	# row2					\n\
	mov 8(%%esi), %%edi	# out					\n\
	mov 16(%%esi), %%ecx	# w1					\n\
	mov 20(%%esi), %%edx	# w2					\n\
									\n\
	cmp %%edx, %%ecx						\n\
	jl mmxext.no_switch						\n\
									\n\
	mov %%eax, %%ecx						\n\
	mov %%ebx, %%eax						\n\
	mov %%ecx, %%ebx						\n\
	mov %%edx, %%ecx						\n\
									\n\
mmxext.no_switch:							\n\
									\n\
	movd %%ecx, %%mm3	# 0:	0:	0:	w1		\n\
									\n\
	movq %%mm3, %%mm7						\n\
	psllq $16, %%mm3	# 0:	0:	w1:	0		\n\
	por %%mm7, %%mm3	# 0:	0:	w1:	w1		\n\
									\n\
	movq %%mm3, %%mm7						\n\
	psllq $32, %%mm3	#w1:	w1:	0:	0		\n\
	por %%mm7, %%mm3	#w1:	w1:	w1:	w1		\n\
									\n\
	mov 12(%%esi), %%ecx	# bytes					\n\
	shr $3, %%ecx		# /8					\n\
									\n\
	pxor %%mm7, %%mm7	# zero					\n\
									\n\
									\n\
mmx.rescale:								\n\
	movq (%%ebx), %%mm0	#  A					\n\
	movq (%%eax), %%mm4	#  B					\n\
									\n\
### 0-3 (A-B)								\n\
	movq %%mm0, %%mm2						\n\
	psubusb %%mm4, %%mm2						\n\
	movq %%mm2, %%mm5	#  save					\n\
									\n\
	punpckhbw %%mm7, %%mm2						\n\
	pmulhuw %%mm3, %%mm2						\n\
									\n\
	packsswb %%mm7, %%mm2	# collapse				\n\
									\n\
	movq %%mm2, %%mm6	# save into mm6				\n\
	psllq $32, %%mm6						\n\
									\n\
### 4-7 (A-B)								\n\
	movq %%mm5, %%mm2						\n\
	punpcklbw %%mm7, %%mm2						\n\
	pmulhuw %%mm3, %%mm2						\n\
									\n\
	packsswb %%mm7, %%mm2	# collapse				\n\
									\n\
	por %%mm2, %%mm6	# save into mm6				\n\
									\n\
	movq %%mm4, %%mm5						\n\
	paddb %%mm6, %%mm5						\n\
									\n\
### 0-3 (B-A)								\n\
	movq %%mm4, %%mm2						\n\
	psubusb %%mm0, %%mm2						\n\
	movq %%mm2, %%mm4						\n\
									\n\
	punpckhbw %%mm7, %%mm2						\n\
	pmulhuw %%mm3, %%mm2						\n\
									\n\
	packsswb %%mm7, %%mm2	# collapse				\n\
									\n\
	movq %%mm2, %%mm6	# save into mm6				\n\
	psllq $32, %%mm6						\n\
									\n\
### 4-7 (B-A)								\n\
	movq %%mm4, %%mm2						\n\
	punpcklbw %%mm7, %%mm2						\n\
	pmulhuw %%mm3, %%mm2						\n\
									\n\
	packsswb %%mm7, %%mm2	# collapse				\n\
									\n\
	por %%mm2, %%mm6	# save into mm6				\n\
									\n\
	psubb %%mm6, %%mm5						\n\
									\n\
	movq %%mm5, (%%edi)						\n\
	## done								\n\
									\n\
	add $8, %%eax		# inc pointers				\n\
	add $8, %%ebx							\n\
	add $8, %%edi							\n\
									\n\
	dec %%ecx							\n\
	jg mmx.rescale							\n\
									\n\
	pop %%ebx							\n\
    " : /* no outputs */
      : "S" (&args), "m" (args)
      : "%eax", "%ecx", "%edx", "%edi"
    );
    return 0;
}

/*************************************************************************/

int ac_rescale_sse(char *row1, char *row2, char *out, int bytes,
		   unsigned long weight1, unsigned long weight2)
{
    struct {
	char *row1, *row2, *out;
	int bytes;
	unsigned long weight1, weight2;
    } args;
    args.row1 = row1;
    args.row2 = row2;
    args.out = out;
    args.bytes = bytes;
    args.weight1 = weight1;
    args.weight2 = weight2;

    asm("\
	push %%ebx							\n\
									\n\
	mov (%%esi), %%ebx	# row1					\n\
	mov 4(%%esi), %%eax	# row2					\n\
	mov 8(%%esi), %%edi	# out					\n\
	mov 16(%%esi), %%ecx	# w1					\n\
	mov 20(%%esi), %%edx	# w2					\n\
									\n\
	cmp %%edx, %%ecx						\n\
	jl sse.no_switch						\n\
									\n\
	mov %%eax, %%ecx						\n\
	mov %%ebx, %%eax						\n\
	mov %%ecx, %%ebx						\n\
	mov %%edx, %%ecx						\n\
									\n\
sse.no_switch:								\n\
									\n\
	movd %%ecx, %%mm3	# 0:	0:	0:	w1		\n\
									\n\
	movq %%mm3, %%mm7						\n\
	psllq $16, %%mm3	# 0:	0:	w1:	0		\n\
	por %%mm7, %%mm3	# 0:	0:	w1:	w1		\n\
									\n\
	movq %%mm3, %%mm7						\n\
	psllq $32, %%mm3	#w1:	w1:	0:	0		\n\
	por %%mm7, %%mm3	#w1:	w1:	w1:	w1		\n\
									\n\
	mov 12(%%esi), %%ecx	# bytes					\n\
	shr $3, %%ecx		# /8					\n\
									\n\
	pxor %%mm7, %%mm7	# zero					\n\
									\n\
									\n\
sse.rescale:								\n\
	prefetchnta (%%eax)						\n\
	prefetchnta (%%ebx)						\n\
									\n\
	movq (%%ebx), %%mm0	#  A					\n\
	movq (%%eax), %%mm4	#  B					\n\
									\n\
### 0-3 (A-B)								\n\
	movq %%mm0, %%mm2						\n\
	psubusb %%mm4, %%mm2						\n\
	movq %%mm2, %%mm5	#  save					\n\
									\n\
	punpckhbw %%mm7, %%mm2						\n\
	pmulhuw %%mm3, %%mm2						\n\
									\n\
	packsswb %%mm7, %%mm2	# collapse				\n\
									\n\
	movq %%mm2, %%mm6	# save into mm6				\n\
	psllq $32, %%mm6						\n\
									\n\
### 4-7 (A-B)								\n\
	movq %%mm5, %%mm2						\n\
	punpcklbw %%mm7, %%mm2						\n\
	pmulhuw %%mm3, %%mm2						\n\
									\n\
	packsswb %%mm7, %%mm2	# collapse				\n\
									\n\
	por %%mm2, %%mm6	# save into mm6				\n\
									\n\
	movq %%mm4, %%mm5						\n\
	paddb %%mm6, %%mm5						\n\
									\n\
### 0-3 (B-A)								\n\
	movq %%mm4, %%mm2						\n\
	psubusb %%mm0, %%mm2						\n\
	movq %%mm2, %%mm4						\n\
									\n\
	punpckhbw %%mm7, %%mm2						\n\
	pmulhuw %%mm3, %%mm2						\n\
									\n\
	packsswb %%mm7, %%mm2	# collapse				\n\
									\n\
	movq %%mm2, %%mm6	# save into mm6				\n\
	psllq $32, %%mm6						\n\
									\n\
### 4-7 (B-A)								\n\
	movq %%mm4, %%mm2						\n\
	punpcklbw %%mm7, %%mm2						\n\
	pmulhuw %%mm3, %%mm2						\n\
									\n\
	packsswb %%mm7, %%mm2	# collapse				\n\
									\n\
	por %%mm2, %%mm6	# save into mm6				\n\
									\n\
	psubb %%mm6, %%mm5						\n\
									\n\
	movntq %%mm5, (%%edi)						\n\
	## done								\n\
									\n\
	add $8, %%eax		# inc pointers				\n\
	add $8, %%ebx							\n\
	add $8, %%edi							\n\
									\n\
	dec %%ecx							\n\
	jg sse.rescale							\n\
									\n\
	sfence								\n\
	pop %%ebx							\n\
    " : /* no outputs */
      : "S" (&args), "m" (args)
      : "%eax", "%ecx", "%edx", "%edi"
    );
    return 0;
}

#endif  /* ARCH_X86 */

/*************************************************************************/

#if defined(ARCH_X86) || defined(ARCH_X86_64)

#ifdef ARCH_X86_64
# define EAX "%%rax"
# define EBX "%%rbx"
# define ECX "%%rcx"
# define EDX "%%rdx"
# define ESI "%%rsi"
# define EDI "%%rdi"
#else
# define EAX "%%eax"
# define EBX "%%ebx"
# define ECX "%%ecx"
# define EDX "%%edx"
# define ESI "%%esi"
# define EDI "%%edi"
#endif

int ac_rescale_sse2(char *row1, char *row2, char *out, int bytes,
		    unsigned long weight1, unsigned long weight2)
{
    struct {
	int bytes, pad;
	char *row1, *row2, *out;
	unsigned long weight1, weight2;
    } args;
    args.row1 = row1;
    args.row2 = row2;
    args.out = out;
    args.bytes = bytes;
    args.weight1 = weight1;
    args.weight2 = weight2;

    asm("\
	push "EBX"							\n\
									\n"
#ifdef ARCH_X86_64
"	mov 8(%%rsi), %%rbx	# row1					\n\
	mov 16(%%rsi), %%rax	# row2					\n\
	mov 24(%%rsi), %%rdi	# out					\n\
	mov 32(%%rsi), %%rcx	# w1					\n\
	mov 40(%%rsi), %%rdx	# w2					\n"
#else
"	mov 8(%%esi), %%ebx	# row1					\n\
	mov 12(%%esi), %%eax	# row2					\n\
	mov 16(%%esi), %%edi	# out					\n\
	mov 20(%%esi), %%ecx	# w1					\n\
	mov 24(%%esi), %%edx	# w2					\n"
#endif
"									\n\
	cmp "EDX", "ECX"						\n\
	jl sse2.no_switch						\n\
									\n\
	mov "EAX", "ECX"						\n\
	mov "EBX", "EAX"						\n\
	mov "ECX", "EBX"						\n\
	mov "EDX", "ECX"						\n\
									\n\
sse2.no_switch:								\n\
									\n\
	movd %%ecx, %%xmm3	# 0 0 0 0 0 0 0 w1			\n\
									\n\
	movdqa %%xmm3, %%xmm7						\n\
	psllq $16, %%xmm3						\n\
	por %%xmm7, %%xmm3	# 0 0 0 0 0 0 w1 w1			\n\
									\n\
	movdqa %%xmm3, %%xmm7						\n\
	psllq $32, %%xmm3						\n\
	por %%xmm7, %%xmm3	# 0 0 0 0 w1 w1 w1 w1			\n\
									\n\
	movdqa %%xmm3, %%xmm7						\n\
	psllq $64, %%xmm3						\n\
	por %%xmm7, %%xmm3	# w1 w1 w1 w1 w1 w1 w1 w1		\n\
									\n\
	mov ("ESI"), %%ecx	# bytes					\n\
	shr $4, %%ecx		# /16					\n\
									\n\
	pxor %%xmm7, %%xmm7	# zero					\n\
									\n\
									\n\
sse2.rescale:								\n\
	prefetchnta ("EAX")						\n\
	prefetchnta ("EBX")						\n\
									\n\
	movdqa ("EBX"), %%xmm0	#  A					\n\
	movdqa ("EAX"), %%xmm4	#  B					\n\
									\n\
### 0-3 (A-B)								\n\
	movdqa %%xmm0, %%xmm2						\n\
	psubusb %%xmm4, %%xmm2						\n\
	movdqa %%xmm2, %%xmm5	#  save					\n\
									\n\
	punpckhbw %%xmm7, %%xmm2					\n\
	pmulhuw %%xmm3, %%xmm2						\n\
									\n\
	packsswb %%xmm7, %%xmm2	# collapse				\n\
									\n\
	movdqa %%xmm2, %%xmm6	# save into xmm6			\n\
	pslldq $64, %%xmm6						\n\
									\n\
### 4-7 (A-B)								\n\
	movdqa %%xmm5, %%xmm2						\n\
	punpcklbw %%xmm7, %%xmm2					\n\
	pmulhuw %%xmm3, %%xmm2						\n\
									\n\
	packsswb %%xmm7, %%xmm2	# collapse				\n\
									\n\
	por %%xmm2, %%xmm6	# save into xmm6			\n\
									\n\
	movdqa %%xmm4, %%xmm5						\n\
	paddb %%xmm6, %%xmm5						\n\
									\n\
### 0-3 (B-A)								\n\
	movdqa %%xmm4, %%xmm2						\n\
	psubusb %%xmm0, %%xmm2						\n\
	movdqa %%xmm2, %%xmm4						\n\
									\n\
	punpckhbw %%xmm7, %%xmm2					\n\
	pmulhuw %%xmm3, %%xmm2						\n\
									\n\
	packsswb %%xmm7, %%xmm2	# collapse				\n\
									\n\
	movdqa %%xmm2, %%xmm6	# save into xmm6			\n\
	pslldq $64, %%xmm6						\n\
									\n\
### 4-7 (B-A)								\n\
	movdqa %%xmm4, %%xmm2						\n\
	punpcklbw %%xmm7, %%xmm2					\n\
	pmulhuw %%xmm3, %%xmm2						\n\
									\n\
	packsswb %%xmm7, %%xmm2	# collapse				\n\
									\n\
	por %%xmm2, %%xmm6	# save into xmm6			\n\
									\n\
	psubb %%xmm6, %%xmm5						\n\
									\n\
	movntdq %%xmm5, ("EDI")						\n\
	## done								\n\
									\n\
	add $16, "EAX"		# inc pointers				\n\
	add $16, "EBX"							\n\
	add $16, "EDI"							\n\
									\n\
	dec %%ecx							\n\
	jg sse2.rescale							\n\
									\n\
	sfence								\n\
	pop "EBX"							\n\
    " : /* no outputs */
      : "S" (&args), "m" (args)
#ifdef ARCH_X86_64
      : "%rax", "%rcx", "%rdx", "%rdi"
#else
      : "%eax", "%ecx", "%edx", "%edi"
#endif
    );
    return 0;
}

#endif  /* ARCH_X86 || ARCH_X86_64 */

/*************************************************************************/
