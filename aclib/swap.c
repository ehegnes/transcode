/* -*- coding: iso-8859-1; -*-
 *
 *  swap.c
 *
 *  Based on swap.s:
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

#define SWAP_4 "\
				# --EAX--- --EDX--- --EBX--- --EDI---	\n\
	mov (%%esi), %%eax	# R1B0G0R0				\n\
	bswap %%eax		# R0G0B0R1				\n\
	ror $8, %%eax		# R1R0G0B0				\n\
	mov %%eax, %%edx	#          R1R0G0B0			\n\
	and $mask0111, %%eax	# ..R0G0B0				\n\
	xor %%eax, %%edx	#          R1......			\n\
									\n\
	mov 4(%%esi), %%ebx	#                   G2R2B1G1		\n\
	mov %%ebx, %%edi	#                            G2R2B1G1	\n\
	and $mask0010, %%edi	#                            ....B1..	\n\
	rol $16, %%edi		#                            B1......	\n\
	or %%edi, %%eax		# B1R0G0B0                   --free--	\n\
	mov %%eax, (%%esi)	# --done--				\n\
									\n\
	mov %%ebx, %%edi	#                            G2R2B1G1	\n\
	and $mask1001, %%ebx	#                   G2....G1		\n\
	ror $16, %%edx		#          ....R1..			\n\
	or %%edx, %%ebx		#          --free-- G2..R1G1		\n\
									\n\
	mov 8(%%esi), %%eax	# B3G3R3B2				\n\
	bswap %%eax		# B2R3G3B3				\n\
	rol $8, %%eax		# R3G3B3B2				\n\
	mov %%eax, %%edx	#          R3G3B3B2			\n\
	and $mask0001, %%edx	#          ......B2			\n\
	rol $16, %%edx		#          ..B2....			\n\
	or %%edx, %%ebx		#          --free-- G2B2R1G1		\n\
	mov %%ebx, 4(%%esi)	#                   --done--		\n\
									\n\
	and $mask1110, %%eax	# R3G3B3..				\n\
	and $mask0100, %%edi	#                            ..R2....	\n\
	ror $16, %%edi		#                            ......R2	\n\
	or %%edi, %%eax		# R3B3B3R2                   --free--	\n\
	mov %%eax, 8(%%esi)	# --done--				\n\
									\n\
	add $12, %%esi							\n\
"

int ac_swap_rgb2bgr_asm(char *im, int pixels)
{
    asm("\
mask0001 = 0x000000FF							\n\
mask0010 = 0x0000FF00							\n\
mask0100 = 0x00FF0000							\n\
mask0111 = 0x00FFFFFF							\n\
mask1001 = 0xFF0000FF							\n\
mask1110 = 0xFFFFFF00							\n\
									\n\
	push %%ebx							\n\
									\n\
.loop:									\n"
	SWAP_4
	SWAP_4
	SWAP_4
	SWAP_4

	SWAP_4
	SWAP_4
	SWAP_4
	SWAP_4

	SWAP_4
	SWAP_4
	SWAP_4
	SWAP_4

	SWAP_4
	SWAP_4
	SWAP_4
	SWAP_4
"									\n\
	sub $64, %%ecx		# done with 64 pixels			\n\
	jg .loop							\n\
									\n\
.exit:									\n\
	pop %%ebx							\n\
    " : /* no outputs */
      : "S" (im), "c" (pixels)
      : "%eax", "%edx", "%edi"
    );
    return 0;
}

#endif  /* ARCH_X86 */

/*************************************************************************/

#ifdef ARCH_X86_64

#define SWAP_4 "\
				# --EAX--- --EDX--- --EBX--- --EDI---	\n\
	mov (%%rsi), %%eax	# R1B0G0R0				\n\
	bswap %%eax		# R0G0B0R1				\n\
	ror $8, %%eax		# R1R0G0B0				\n\
	mov %%eax, %%edx	#          R1R0G0B0			\n\
	and %%r12d, %%eax	# ..R0G0B0				\n\
	xor %%eax, %%edx	#          R1......			\n\
									\n\
	mov 4(%%rsi), %%ebx	#                   G2R2B1G1		\n\
	mov %%ebx, %%edi	#                            G2R2B1G1	\n\
	and %%r9d, %%edi	#                            ....B1..	\n\
	rol $16, %%edi		#                            B1......	\n\
	or %%edi, %%eax		# B1R0G0B0                   --free--	\n\
	mov %%eax, (%%rsi)	# --done--				\n\
									\n\
	mov %%ebx, %%edi	#                            G2R2B1G1	\n\
	and %%r11d, %%ebx	#                   G2....G1		\n\
	ror $16, %%edx		#          ....R1..			\n\
	or %%edx, %%ebx		#          --free-- G2..R1G1		\n\
									\n\
	mov 8(%%rsi), %%eax	# B3G3R3B2				\n\
	bswap %%eax		# B2R3G3B3				\n\
	rol $8, %%eax		# R3G3B3B2				\n\
	mov %%eax, %%edx	#          R3G3B3B2			\n\
	and %%r8d, %%edx	#          ......B2			\n\
	rol $16, %%edx		#          ..B2....			\n\
	or %%edx, %%ebx		#          --free-- G2B2R1G1		\n\
	mov %%ebx, 4(%%rsi)	#                   --done--		\n\
									\n\
	and %%r13d, %%eax	# R3G3B3..				\n\
	and %%r10d, %%edi	#                            ..R2....	\n\
	ror $16, %%edi		#                            ......R2	\n\
	or %%edi, %%eax		# R3B3B3R2                   --free--	\n\
	mov %%eax, 8(%%rsi)	# --done--				\n\
									\n\
	add $12, %%rsi							\n\
"

int ac_swap_rgb2bgr_asm64(char *im, int pixels)
{
    asm("\
mask0001 = 0x000000FF							\n\
mask0010 = 0x0000FF00							\n\
mask0100 = 0x00FF0000							\n\
mask0111 = 0x00FFFFFF							\n\
mask1001 = 0xFF0000FF							\n\
mask1110 = 0xFFFFFF00							\n\
									\n\
	push %%rbx							\n\
									\n\
	mov $mask0001, %%r8d						\n\
	mov $mask0010, %%r9d						\n\
	mov $mask0010, %%r10d						\n\
	mov $mask0100, %%r11d						\n\
	mov $mask0111, %%r12d						\n\
	mov $mask1110, %%r13d						\n\
									\n\
.loop:									\n"
	SWAP_4
	SWAP_4
	SWAP_4
	SWAP_4

	SWAP_4
	SWAP_4
	SWAP_4
	SWAP_4

	SWAP_4
	SWAP_4
	SWAP_4
	SWAP_4

	SWAP_4
	SWAP_4
	SWAP_4
	SWAP_4
"									\n\
	sub $64, %%ecx		# done with 64 pixels			\n\
	jg .loop							\n\
									\n\
.exit:									\n\
	pop %%rbx							\n\
    " : /* no outputs */
      : "S" (im), "c" (pixels)
      : "%rax", "%rdx", "%rdi", "%r8", "%r9", "%r10", "%r11", "%r12", "%r13"
    );
    return 0;
}

#endif  /* ARCH_X86_64 */

/*************************************************************************/
