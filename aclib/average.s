;;/*
;; *  average.s
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

section .text

	;; 
	;; int ac_average_mmx(char *row1, char *row2, char *out, int bytes) 
	;;						
	;;

cglobal ac_average_mmx 

%macro INTER_8 0
	prefetchnta [eax]
	prefetchnta [ebx]		

	movq mm0, [eax]
	movq mm4, [ebx]

	movq mm2, mm0
	punpcklbw mm2, mm7
	
	movq mm6, mm4	
	punpcklbw mm6, mm7

	paddusw mm2, mm6
	psrlw mm2, 1

	movq mm3, mm2		; tmp

	movq mm2, mm0
	punpckhbw mm2, mm7
	
	movq mm6, mm4	
	punpckhbw mm6, mm7

	paddusw mm2, mm6
	psrlw mm2, 1

	packuswb mm3, mm2
	movntq [edi], mm3

	add eax, 8
	add ebx, 8
	add edi, 8
%endmacro

%define off esp+16
	
align 16
ac_average_mmx:

	push ebx 
	push ecx 
	push edi
	push esi
	
	mov eax, [off+ 4]	; src1
	mov ebx, [off+ 8]	; src2
	mov edi, [off+12]	; dest
	mov ecx, [off+16]	; bytes 
	
	shr ecx, 4		; /16
	
	pxor mm7, mm7		; zero
	
.loop:		
	INTER_8
	INTER_8	

	dec ecx 
	jg .loop

.exit:		

	xor eax, eax		; exit

	pop esi
	pop edi
	pop ecx	
	pop ebx

	ret			


	;; 
	;; int ac_average_sse(char *row1, char *row2, char *out, int bytes) 
	;;						
	;;

cglobal ac_average_sse 

%define off esp+16
	
align 16
ac_average_sse:

	push ebx 
	push ecx 
	push edi
	push esi
	
	mov eax, [off+ 4]	; src1
	mov ebx, [off+ 8]	; src2
	mov ecx, [off+16]	; bytes 

	mov esi, ecx	
	shr ecx, 5		; /32
	mov edi, ecx
	shl edi, 5

	sub esi, edi		
	
	mov edi, [off+12]	; dest
	
.32loop:		
	prefetchnta [eax]
	prefetchnta [ebx]

	movq mm0, [eax   ]
	movq mm1, [eax+ 8]
	movq mm2, [eax+16]	
	movq mm3, [eax+24]

	movq mm4, [ebx   ]
	movq mm5, [ebx+ 8]
	movq mm6, [ebx+16]	
	movq mm7, [ebx+24]			

	pavgb mm0, mm4
	pavgb mm1, mm5
	pavgb mm2, mm6
	pavgb mm3, mm7			
	
	movntq [edi   ], mm0
	movntq [edi+ 8], mm1
	movntq [edi+16], mm2
	movntq [edi+24], mm3			

	add eax, 32
	add ebx, 32
	add edi, 32

	dec ecx 
	jg .32loop

	shr esi, 3              ; /8
	jz .exit
	
.8loop
	movq mm0, [eax]	
	movq mm4, [ebx]
	pavgb mm0, mm4
	movntq [edi], mm0
	add eax, 8
	add ebx, 8
	add edi, 8
	
	dec esi 
	jg .8loop
	
.exit:		

	xor eax, eax		; exit

	pop esi
	pop edi
	pop ecx	
	pop ebx

	ret			

	;; 
	;; int ac_average_sse2(char *row1, char *row2, char *out, int bytes) 
	;;						
	;;

cglobal ac_average_sse2

%define off esp+16
	
align 16
ac_average_sse2:

	push ebx 
	push ecx 
	push edi
	push esi
	
	mov eax, [off+ 4]	; src1
	mov ebx, [off+ 8]	; src2
	mov ecx, [off+16]	; bytes 

	mov esi, ecx	
	shr ecx, 6		; /64
	mov edi, ecx
	shl edi, 6

	sub esi, edi		
	
	mov edi, [off+12]	; dest
	
.64loop:		
	prefetchnta [eax]
	prefetchnta [ebx]

	movdqa xmm0, [eax   ]
	movdqa xmm1, [eax+16]
	movdqa xmm2, [eax+32]	
	movdqa xmm3, [eax+48]

	movdqa xmm4, [ebx   ]
	movdqa xmm5, [ebx+16]
	movdqa xmm6, [ebx+32]	
	movdqa xmm7, [ebx+48]			

	pavgb xmm0, xmm4
	pavgb xmm1, xmm5
	pavgb xmm2, xmm6
	pavgb xmm3, xmm7			
	
	movntdq [edi   ], xmm0
	movntdq [edi+16], xmm1
	movntdq [edi+32], xmm2
	movntdq [edi+48], xmm3			

	add eax, 64
	add ebx, 64
	add edi, 64

	dec ecx 
	jg .64loop

	shr esi, 3              ; rest/8
	jz .exit
	
.8loop
	movq mm0, [eax]	
	movq mm4, [ebx]
	pavgb mm0, mm4
	movntq [edi], mm0
	add eax, 8
	add ebx, 8
	add edi, 8
	
	dec esi 
	jg .8loop
	
.exit:		

	xor eax, eax		; exit

	pop esi
	pop edi
	pop ecx	
	pop ebx

	ret			

