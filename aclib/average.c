/* -*- coding: iso-8859-1; -*-
 *
 *  average.c
 *
 *  Based on average.s:
 *  Copyright (C) Thomas Östreich - November 2002
 *
 *  This file is part of transcode, a linux video stream processing tool
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

#ifdef ARCH_X86

/*************************************************************************/

#define INTER_8 "\
	movq (%%eax), %%mm0						\n\
	movq (%%ebx), %%mm4						\n\
									\n\
	movq %%mm0, %%mm2						\n\
	punpcklbw %%mm7, %%mm2						\n\
									\n\
	movq %%mm4, %%mm6						\n\
	punpcklbw %%mm7, %%mm6						\n\
									\n\
	paddusw %%mm6, %%mm2						\n\
	psrlw $1, %%mm2							\n\
									\n\
	movq %%mm2, %%mm3	# tmp					\n\
									\n\
	movq %%mm0, %%mm2						\n\
	punpckhbw %%mm7, %%mm2						\n\
									\n\
	movq %%mm4, %%mm6						\n\
	punpckhbw %%mm7, %%mm6						\n\
									\n\
	paddusw %%mm6, %%mm2						\n\
	psrlw $1, %%mm2							\n\
									\n\
	packuswb %%mm2, %%mm3						\n\
	movq %%mm3, (%%edi)						\n\
									\n\
	add $8, %%eax							\n\
	add $8, %%ebx							\n\
	add $8, %%edi							\n\
"

int ac_average_mmx(char *row1, char *row2, char *out, int bytes)
{
    asm("\
	push %%ebx							\n\
	mov %%edx, %%ebx						\n\
									\n\
	shr $4, %%ecx		# /16					\n\
	pxor %%mm7, %%mm7	# zero					\n\
									\n\
mmx.loop:								\n"
	INTER_8
	INTER_8
"	dec %%ecx							\n\
	jg mmx.loop							\n\
									\n\
	pop %%ebx							\n\
    " : /* no outputs */
      : "a" (row1), "d" (row2), "D" (out), "c" (bytes)
    );
    return 0;
}

/*************************************************************************/

int ac_average_sse(char *row1, char *row2, char *out, int bytes)
{
    asm("\
	push %%ebx							\n\
	mov %%edx, %%ebx						\n\
									\n\
	push %%edi							\n\
	mov %%ecx, %%esi						\n\
	shr $5, %%ecx		# /32					\n\
	mov %%ecx, %%edi						\n\
	shl $5, %%edi							\n\
									\n\
	sub %%edi, %%esi						\n\
									\n\
	pop %%edi		# out					\n\
									\n\
sse.32loop:								\n\
	prefetchnta (%%eax)						\n\
	prefetchnta (%%ebx)						\n\
									\n\
	movq   (%%eax), %%mm0						\n\
	movq  8(%%eax), %%mm1						\n\
	movq 16(%%eax), %%mm2						\n\
	movq 24(%%eax), %%mm3						\n\
									\n\
	movq   (%%ebx), %%mm4						\n\
	movq  8(%%ebx), %%mm5						\n\
	movq 16(%%ebx), %%mm6						\n\
	movq 24(%%ebx), %%mm7						\n\
									\n\
	pavgb %%mm4, %%mm0						\n\
	pavgb %%mm5, %%mm1						\n\
	pavgb %%mm6, %%mm2						\n\
	pavgb %%mm7, %%mm3						\n\
									\n\
	movntq %%mm0,   (%%edi)						\n\
	movntq %%mm1,  8(%%edi)						\n\
	movntq %%mm2, 16(%%edi)						\n\
	movntq %%mm3, 24(%%edi)						\n\
									\n\
	add $32, %%eax							\n\
	add $32, %%ebx							\n\
	add $32, %%edi							\n\
	dec %%ecx							\n\
	jg sse.32loop							\n\
									\n\
	shr $3, %%esi		# /8					\n\
	jz sse.exit							\n\
									\n\
sse.8loop:								\n\
	movq (%%eax), %%mm0						\n\
	movq (%%ebx), %%mm4						\n\
	pavgb %%mm4, %%mm0						\n\
	movntq %%mm0, (%%edi)						\n\
	add $8, %%eax							\n\
	add $8, %%ebx							\n\
	add $8, %%edi							\n\
	dec %%esi							\n\
	jg sse.8loop							\n\
									\n\
sse.exit:								\n\
	pop %%ebx							\n\
    " : /* no outputs */
      : "a" (row1), "d" (row2), "D" (out), "c" (bytes)
    );
    return 0;
}

/*************************************************************************/

int ac_average_sse2(char *row1, char *row2, char *out, int bytes)
{
    asm("\
	push %%ebx							\n\
	mov %%edx, %%ebx						\n\
									\n\
	push %%edi							\n\
	mov %%ecx, %%esi						\n\
	shr $6, %%ecx		# /64					\n\
	mov %%ecx, %%edi						\n\
	shl $6, %%edi							\n\
									\n\
	sub %%edi, %%esi						\n\
									\n\
	pop %%edi		# out					\n\
									\n\
sse2.64loop:								\n\
	prefetchnta (%%eax)						\n\
	prefetchnta (%%ebx)						\n\
									\n\
	movdqa   (%%eax), %%xmm0					\n\
	movdqa 16(%%eax), %%xmm1					\n\
	movdqa 32(%%eax), %%xmm2					\n\
	movdqa 48(%%eax), %%xmm3					\n\
									\n\
	movdqa   (%%ebx), %%xmm4					\n\
	movdqa 16(%%ebx), %%xmm5					\n\
	movdqa 32(%%ebx), %%xmm6					\n\
	movdqa 48(%%ebx), %%xmm7					\n\
									\n\
	pavgb %%xmm4, %%xmm0						\n\
	pavgb %%xmm5, %%xmm1						\n\
	pavgb %%xmm6, %%xmm2						\n\
	pavgb %%xmm7, %%xmm3						\n\
									\n\
	movntdq %%xmm0,   (%%edi)					\n\
	movntdq %%xmm1, 16(%%edi)					\n\
	movntdq %%xmm2, 32(%%edi)					\n\
	movntdq %%xmm3, 48(%%edi)					\n\
									\n\
	add $64, %%eax							\n\
	add $64, %%ebx							\n\
	add $64, %%edi							\n\
	dec %%ecx							\n\
	jg sse2.64loop							\n\
									\n\
	shr $3, %%esi		# rest/8				\n\
	jz sse2.exit							\n\
									\n\
sse2.8loop:								\n\
	movq (%%eax), %%mm0						\n\
	movq (%%ebx), %%mm4						\n\
	pavgb %%mm4, %%mm0						\n\
	movntq %%mm0, (%%edi)						\n\
	add $8, %%eax							\n\
	add $8, %%ebx							\n\
	add $8, %%edi							\n\
	dec %%esi							\n\
	jg sse2.8loop							\n\
									\n\
sse2.exit:								\n\
	pop %%ebx							\n\
    " : /* no outputs */
      : "a" (row1), "d" (row2), "D" (out), "c" (bytes)
    );
    return 0;
}

/*************************************************************************/

#endif  /* ARCH_X86 */
