;;/*
;; *  variance_mmx.s
;; *
;; *  Copyright (C) Thomas Östreich - November 2002
;; *
;; *  This file is part of transcode, a linux video stream processing tool
;; *
;; *  transcode is free software; you can redistribute it and/or modify
;; *  it under the terms of the GNU General Public License as published by
;; *  the Free Software Foundation; either version 2, or (at your option)
;; *  any later version.
;; *
;; *  transcode is distributed in the hope that it will be useful,
;; *  but WITHOUT ANY WARRANTY; without even the implied warranty of
;; *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; *  GNU General Public License for more details.
;; *
;; *  You should have received a copy of the GNU General Public License
;; *  along with GNU Make; see the file COPYING.  If not, write to
;; *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
;; *
;; */

	
;;/*
;; * variance of a (16*16) block, multiplied by 256
;; * p:  address of top left pel of block
;; * lx: distance (in bytes) of vertically adjacent pels
;; * MMX version
;; */
	
		
bits 32

%macro cglobal 1 
	%ifdef PREFIX
		global _%1 
		%define %1 _%1
	%else
		global %1
	%endif
%endmacro

section .data

align 16
unused	times 4	dw 1

section .text

cglobal  variance_16x16_mmx

	;; 
	;; int variance_16x16_mmx(char *p, int lx);
	;;

%macro VAR_MMX 0
	movq mm0, [eax]		
	movq mm2, [eax+8]

	movq mm1, mm0
	punpcklbw mm0, mm7
	punpckhbw mm1, mm7

	movq mm3, mm2
	punpcklbw mm2, mm7
	punpckhbw mm3, mm7

	movq mm5, mm0
	paddusw mm5, mm1
	paddusw mm5, mm2
	paddusw mm5, mm3

	movq mm6, mm5
	punpcklwd mm5, mm7
	punpckhwd mm6, mm7
	paddd mm5, mm6

	pmaddwd mm0, mm0
	pmaddwd mm1, mm1
	pmaddwd mm2, mm2
	pmaddwd mm3, mm3

	paddd mm0, mm1
	paddd mm0, mm2
	paddd mm0, mm3

	movd edx, mm5 
	add ecx, edx
	psrlq mm5, $32 
	movd edx, mm5 
	add ecx, edx
	movd edx, mm0 
	add ebx, edx
	psrlq mm0, $32 
	movd edx, mm0 
	add ebx, edx

	add eax, edi
%endmacro		

%define off 12	
			
align 16
variance_16x16_mmx:


	push edi
	push ebx
	push ecx
	
	mov eax, [esp + off + 4]	; p 
	mov edi, [esp + off + 8]	; lx 
	
	xor ebx, ebx		; zero
	xor ecx, ecx		; zero

	pxor mm7, mm7		; zero

	VAR_MMX
	VAR_MMX
	VAR_MMX
	VAR_MMX			
	VAR_MMX
	VAR_MMX
	VAR_MMX
	VAR_MMX
	VAR_MMX
	VAR_MMX
	VAR_MMX
	VAR_MMX			
	VAR_MMX
	VAR_MMX
	VAR_MMX
	VAR_MMX				

	imul ecx, ecx		; mean value squared
	shr ecx, $8		; /256
	sub ebx, ecx		; variance
				
	mov eax, ebx		; exit

	emms	
	pop ecx
	pop ebx
	pop edi
	
	ret
