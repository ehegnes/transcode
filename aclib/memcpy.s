;;/*
;; *  memcpy.s
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

	;; 
	;; int ac_memcpy_mmx(char *dest, char *src, int bytes);
	;;

cglobal  ac_memcpy_mmx

align 16
ac_memcpy_mmx:

	push ebx
	push ecx
	push edi
	push esi

	mov ebx, [esp+20]	; dest
	mov eax, [esp+24]	; src	
	mov ecx, [esp+28]	; bytes 

.64entry:	
	mov esi, ecx
	shr esi, 6		; /64
	jz .rest
	
.64loop:		
	movq mm0, [eax   ]
	movq mm1, [eax+ 8]
	movq mm2, [eax+16]
	movq mm3, [eax+24]
	movq mm4, [eax+32]
	movq mm5, [eax+40]
	movq mm6, [eax+48]
	movq mm7, [eax+56]						
		
	movq [ebx   ], mm0
	movq [ebx+ 8], mm1
	movq [ebx+16], mm2
	movq [ebx+24], mm3
	movq [ebx+32], mm4
	movq [ebx+40], mm5
	movq [ebx+48], mm6
	movq [ebx+56], mm7	

	add eax, 64
	add ebx, 64
	sub ecx, 64
	dec esi
	jg .64loop

.rest:
	mov esi, eax
	mov edi, ebx
	std
	rep movsb
	
.exit:		
	xor eax, eax		; exit

	pop esi
	pop edi
	pop ecx
	pop ebx
	
	ret			


	;; 
	;; int ac_memcpy_sse(char *dest, char *src, int bytes);
	;;

cglobal  ac_memcpy_sse

align 16
ac_memcpy_sse:

	push ebx
	push ecx
	push edi
	push esi

	mov ebx, [esp+20]	; dest
	mov eax, [esp+24]	; src	
	mov ecx, [esp+28]	; bytes 

.64entry:	
	mov esi, ecx
	shr esi, 6		; /64
	jz .rest
	
.64loop:		
	prefetchnta [eax]
	prefetchnta [eax+32]		

	movaps xmm0, [eax   ]
	movaps xmm1, [eax+16]
	movaps xmm2, [eax+32]
	movaps xmm3, [eax+48]

	movntps [ebx   ], xmm0
	movntps [ebx+16], xmm1
	movntps [ebx+32], xmm2
	movntps [ebx+48], xmm3

	add eax, 64
	add ebx, 64
	sub ecx, 64
	dec esi
	jg .64loop

.rest:
	prefetchnta [eax]	
	mov esi, eax
	mov edi, ebx
	std
	rep movsb
	
.exit:		
	sfence

	xor eax, eax		; exit

	pop esi
	pop edi
	pop ecx
	pop ebx
	
	ret			


	;; 
	;; int ac_memcpy_sse2(char *dest, char *src, int bytes);
	;;

cglobal  ac_memcpy_sse2

align 16
ac_memcpy_sse2:

	push ebx
	push ecx
	push edi
	push esi

	mov ebx, [esp+20]	; dest
	mov eax, [esp+24]	; src	
	mov ecx, [esp+28]	; bytes 

.64entry:	
	mov esi, ecx
	shr esi, 6		; /64
	jz .rest
	
.64loop:		
	prefetchnta [eax]
	prefetchnta [eax+32]		

	movdqa xmm0, [eax   ]
	movdqa xmm1, [eax+16]
	movdqa xmm2, [eax+32]
	movdqa xmm3, [eax+48]

	movntdq [ebx   ], xmm0
	movntdq [ebx+16], xmm1
	movntdq [ebx+32], xmm2
	movntdq [ebx+48], xmm3

	add eax, 64
	add ebx, 64
	sub ecx, 64
	dec esi
	jg .64loop

.rest:
	prefetchnta [eax]	
	mov esi, eax
	mov edi, ebx
	std
	rep movsb
	
.exit:		
	sfence

	xor eax, eax		; exit

	pop esi
	pop edi
	pop ecx
	pop ebx
	
	ret			
		