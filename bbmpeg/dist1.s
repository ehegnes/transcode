;;/*
;; *  dist1.s
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
svar	dw 0

section .text


;/*
; * total absolute difference between two (16*h) blocks
; * including optional half pel interpolation of blk1 (hx,hy)
; * blk1,blk2: addresses of top left pels of both blocks
; * lx:        distance (in bytes) of vertically adjacent pels
; * hx,hy:     flags for horizontal and/or vertical interpolation
; * h:         height of block (usually 8 or 16)
; * distlim:   bail out if sum exceeds this value
; */

%define off 20
	
;; 
;; int dist1_mmx(unsigned char *blk1, unsigned char *blk2,
;;		int lx, int hx, int hy, int h, int distlim)


cglobal  dist1_mmx
	
align 16
dist1_mmx:


;  //  int s = 0;

;  //mov %0, %%edi" : : "m" (h) );
;  //mov %0, %%edx" : : "m" (hy) );
;  //mov %0, %%eax" : : "m" (hx) );
;  //mov %0, %%esi" : : "m" (lx) );

	push ebx
	push ecx
	push edx
	push esi
	push edi

	mov edi, [esp + off + 24]	;h
	mov edx, [esp + off + 20]	;hy
	mov eax, [esp + off + 16]	;hx
	mov esi, [esp + off + 12]	;lx 		
	
	test edi, edi
	jle near d1exit

	pxor mm7, mm7

	test eax, eax
	jne d1is10
	test edx, edx
	jne d1is10

	xor edx, edx

;//mov %0, %%eax" : : "m" (blk1) );
;//mov %0, %%ebx" : : "m" (blk2) );

	mov eax, [esp + off + 4]	;blk1
	mov ebx, [esp + off + 8]	;blk2	
	
d1top00:
	movq mm0, [eax]
	movq mm1, [ebx]
	movq mm2, mm0
	psubusb mm0, mm1
	psubusb mm1, mm2
	por mm0, mm1
	movq mm2, [eax+8] ;//8(%eax), %mm2
	movq mm3, [ebx+8] ;//8(%ebx), %mm3
	movq mm4, mm2
	psubusb mm2, mm3
	psubusb mm3, mm4
	por mm2, mm3
	movq mm1, mm0
	punpcklbw mm0, mm7
	punpckhbw mm1, mm7
	movq mm3, mm2
	punpcklbw mm2, mm7
	punpckhbw mm3, mm7
	paddw mm0, mm1
	paddw mm0, mm2
	paddw mm0, mm3
	movq mm1, mm0
	punpcklwd mm0, mm7
	punpckhwd mm1, mm7
	paddd mm0, mm1
	movd ecx, mm0 ;mm0, ecx
	add edx, ecx
	psrlq mm0, $32 ;//$32, mm0
	movd ecx, mm0 ;//mm0, ecx
	add edx, ecx

;    //  cmp %0, edx" : : "m" (distlim) );
;; jge near d1exit1
	
	add eax, esi  ;//esi, eax
	add ebx, esi  ;//esi, ebx
	dec edi
	jg d1top00

	jmp d1exit		; return

d1is10:
	test eax, eax
	je near d1is01
	test edx, edx
	jne near d1is01

	xor edx, edx

;    //mov 0, eax" : : "m" (blk1) );
;    //mov 0, ebx" : : "m" (blk2) );
	
	mov eax, [esp + off + 4]	;blk1
	mov ebx, [esp + off + 8]	;blk2	
	
	pxor mm6, mm6
	pcmpeqw mm1, mm1
	psubw mm6, mm1

d1top10:
	movq mm0, [eax] ;//(eax ), mm0
	movq mm1, mm0
	punpcklbw mm0, mm7
	punpckhbw mm1, mm7
	movq mm2, [eax+1] ;//1(eax), mm2
	movq mm3, mm2
	punpcklbw mm2, mm7
	punpckhbw mm3, mm7
	paddw mm0, mm2 ;//mm2, mm0
	paddw mm1, mm3 ;//mm3, mm1
	paddw mm0, mm6 ;//mm6, mm0
	paddw mm1, mm6 ;//mm6, mm1
	psrlw mm0, $1 ;//$1, mm0
	psrlw mm1, $1 ;//$1, mm1
	packuswb mm0, mm1
	movq mm1, [ebx] ;//(ebx ), mm1
	movq mm2, mm0 ;//mm0, mm2
	psubusb mm0, mm1 ;//mm1, mm0
	psubusb mm1, mm2 ;//mm2, mm1
	por mm0, mm1 ;//mm1, mm0
	movq mm1, [eax+8] ;//8(eax), mm1
	movq mm2, mm1 ;//mm1, mm2
	punpcklbw mm1, mm7 ;//mm7, mm1
	punpckhbw mm2, mm7 ;//mm7, mm2
	movq mm3, [eax+9] ;//9(eax), mm3
	movq mm4, mm3 ;//mm3, mm4
	punpcklbw mm3, mm7 ;//mm7, mm3
	punpckhbw mm4, mm7 ;//mm7, mm4
	paddw mm1, mm3 ;//mm3, mm1
	paddw mm2, mm4 ;//mm4, mm2
	paddw mm1, mm6 ;//mm6, mm1
	paddw mm2, mm6 ;//mm6, mm2
	psrlw mm1, $1 ;//$1, mm1
	psrlw mm2, $1 ;//$1, mm2
	packuswb mm1, mm2 ;//mm2, mm1
	movq mm2, [ebx+8] ;//8(ebx), mm2
	movq mm3, mm1 ;//mm1, mm3
	psubusb mm1, mm2 ;//mm2, mm1
	psubusb mm2, mm3 ;//mm3, mm2
	por mm1, mm2 ;//mm2, mm1
	movq mm2, mm0 ;//mm0, mm2
	punpcklbw mm0, mm7 ;//mm7, mm0
	punpckhbw mm2, mm7 ;//mm7, mm2
	movq mm3, mm1 ;//mm1, mm3
	punpcklbw mm1, mm7 ;//mm7, mm1
	punpckhbw mm3, mm7 ;//mm7, mm3
	paddw mm0, mm1 ;//mm1, mm0
	paddw mm0, mm2 ;//mm2, mm0
	paddw mm0, mm3 ;//mm3, mm0
	movq mm1, mm0 ;//mm0, mm1
	punpcklwd mm0, mm7 ;//mm7, mm0
	punpckhwd mm1, mm7 ;//mm7, mm1
	paddd mm0, mm1 ;//mm1, mm0

	movd ecx, mm0 ;//mm0, ecx
	add edx, ecx ;//ecx, edx 
	psrlq mm0, $32 ;//$32, mm0
	movd ecx, mm0 ;//mm0, ecx
	add edx, ecx ;//ecx, edx

	add eax, esi ;//esi, eax
	add ebx, esi ;//esi, ebx
	dec edi
	jg d1top10

	jmp d1exit		; return

d1is01:
	test eax, eax
	jne near d1is11
	test edx, edx
	je near d1is11

;    //mov 0, eax" : : "m" (blk1) );
;    //mov 0, edx" : : "m" (blk2) );      ??????????? edx ?????

	mov eax, [esp + off + 4]	;blk1
	mov edx, [esp + off + 8]	;blk2	
	
	mov ebx, eax ;//eax, ebx
	add ebx, esi ;//esi, ebx

	pxor mm6, mm6
	pcmpeqw mm1, mm1
	psubw mm6, mm1 ;//mm1, mm6

d1top01:
	movq mm0, [eax] ;//(eax ), mm0
	movq mm1, mm0 ;//mm0, mm1
	punpcklbw mm0, mm7 ;//mm7, mm0
	punpckhbw mm1, mm7 ;//mm7, mm1
	movq mm2, [ebx] ;//(ebx ), mm2
	movq mm3, mm2 ;//mm2, mm3
	punpcklbw mm2, mm7 ;//mm7, mm2
	punpckhbw mm3, mm7 ;//mm7, mm3
	paddw mm0, mm2 ;//mm2, mm0
	paddw mm1, mm3 ;//mm3, mm1
	paddw mm0, mm6 ;//mm6, mm0
	paddw mm1, mm6 ;//mm6, mm1
	psrlw mm0, $1 ;//$1, mm0
	psrlw mm1, $1 ;//$1, mm1
	packuswb mm0, mm1 ;//mm1, mm0
	movq mm1, [edx] ;//(edx ), mm1
	movq mm2, mm0 ;//mm0, mm2
	psubusb mm0, mm1 ;//mm1, mm0
	psubusb mm1, mm2 ;//mm2, mm1
	por mm0, mm1 ;//mm1, mm0
	movq mm1, [eax+8] ;//8(eax), mm1
	movq mm2, mm1 ;//mm1, mm2
	punpcklbw mm2, mm7 ;//mm7, mm1
	punpckhbw mm2, mm7 ;//mm7, mm2
	movq mm3, [ebx+8] ;//8(ebx), mm3
	movq mm4, mm3 ;//mm3, mm4
	punpcklbw mm3, mm7 ;//mm7, mm3
	punpckhbw mm4, mm7 ;//mm7, mm4
	paddw mm1, mm3 ;//mm3, mm1
	paddw mm2, mm4 ;//mm4, mm2
	paddw mm1, mm6 ;//mm6, mm1
	paddw mm2, mm6 ;//mm6, mm2
	psrlw mm1, $1 ;//$1, mm1
	psrlw mm2, $1 ;//$1, mm2
	packuswb mm1, mm2 ;//mm2, mm1
	movq mm2, [ edx+8] ;//8(edx), mm2
	movq mm3, mm1 ;//mm1, mm3
	psubusb mm1, mm2 ;//mm2, mm1
	psubusb mm2, mm3 ;//mm3, mm2
	por mm1, mm2 ;//mm2, mm1
	movq mm0, mm2 ;//mm0, mm2
	punpcklbw mm0, mm7 ;//mm7, mm0
	punpckhbw mm2, mm7 ;//mm7, mm2
	movq mm3, mm1 ;//mm1, mm3
	punpcklbw mm1, mm7 ;//mm7, mm1
	punpckhbw mm3, mm7 ;//mm7, mm3
	paddw mm0, mm1 ;//mm1, mm0
	paddw mm0, mm2 ;//mm2, mm0
	paddw mm0, mm3 ;//mm3, mm0
	movq mm1, mm0 ;//mm0, mm1
	punpcklwd mm0, mm7 ;//mm7, mm0
	punpckhwd mm1, mm7 ;//mm7, mm1
	paddd mm0, mm1 ;//mm1, mm0
	movd ecx, mm0 ;//mm0, ecx

;//  add ecx, 0" : "=m" (s) : );
	add [svar], ecx
	
	psrlq mm0, $32 ;//$32, mm0
	movd ecx, mm0 ;//mm0, ecx

;//add ecx, 0" : "=m" (s) : );
	add [svar], ecx
	
	mov eax, ebx ;//ebx, eax
	add edx, esi ;//esi, edx
	add ebx, esi ;//esi, ebx
	dec edi
	jg d1top01
	jmp d1exit1

d1is11:

;//mov 0, eax" : : "m" (blk1) );
;//mov 0, edx" : : "m" (blk2) );

	mov eax, [esp + off + 4]	;blk1
	mov edx, [esp + off + 8]	;blk2	

	mov ebx, eax ;//eax, ebx
	add ebx, esi ;//esi, ebx

d1top11:
	movq mm0, [eax]; //(eax ), mm0
	movq mm1, mm0 ;//mm0, mm1
	punpcklbw mm0, mm7 ;//mm7, mm0
	punpckhbw mm1, mm7 ;//mm7, mm1
	movq mm2, [eax+1] ;//1(eax), mm2
	movq mm3, mm2 ;//mm2, mm3
	punpcklbw mm2, mm7 ;//mm7, mm2
	punpckhbw mm3, mm7 ;//mm7, mm3
	paddw mm0, mm2 ;//mm2, mm0
	paddw mm1, mm3 ;//mm3, mm1
	movq mm2, [ebx]; //(ebx ), mm2
	movq mm3, mm2 ;//mm2, mm3
	punpcklbw mm2, mm7 ;//mm7, mm2
	punpckhbw mm3, mm7 ;//mm7, mm3
	movq mm4, [ebx+1] ;//1(ebx), mm4
	movq mm5, mm4 ;//mm4, mm5
	punpcklbw mm4, mm7 ;//mm7, mm4
	punpckhbw mm5, mm7 ;//mm7, mm5
	paddw mm2, mm4 ;//mm4, mm2
	paddw mm3, mm5 ;//mm5, mm3
	paddw mm0, mm2 ;//mm2, mm0
	paddw mm1, mm3 ;//mm3, mm1
	pxor mm6, mm6
	pcmpeqw mm5, mm5
	psubw mm6, mm5 ;//mm5, mm6
	paddw mm6, mm6
	paddw mm0, mm6 ;//mm6, mm0
	paddw mm1, mm6 ;//mm6, mm1
	psrlw mm0, $2 ;//$2, mm0
        psrlw mm1, $2 ;//$2, mm1
	packuswb mm0, mm1 ;//mm1, mm0
	movq mm1, [edx] ;//(edx ), mm1
	movq mm2, mm0 ;//mm0, mm2
	psubusb mm0, mm1 ;//mm1, mm0
	psubusb mm1, mm2 ;//mm2, mm1
	por mm0, mm1 ;//mm1, mm0
	movq mm1, [eax+8]; //8(eax), mm1
	movq mm1, mm1 ;//mm1, mm2
	punpcklbw mm1, mm7 ;//mm7, mm1
	punpckhbw mm2, mm7 ;//mm7, mm2
	movq mm3, [eax+9]; //9(eax), mm3
	movq mm4, mm3 ;//mm3, mm4
	punpcklbw mm3, mm7 ;//mm7, mm3
	punpckhbw mm4, mm7 ;//mm7, mm4
	paddw mm1, mm3 ;//mm3, mm1
	paddw mm2, mm4 ;//mm4, mm2
	movq mm3, [ebx+8] ;//8(ebx), mm3
	movq mm4, mm3 ;//mm3, mm4
	punpcklbw mm3, mm7 ;//mm7, mm3
	punpckhbw mm4, mm7 ;//mm7, mm4
	movq mm5, [ebx+9] ;//9(ebx), mm5
	movq mm6, mm5 ;//mm5, mm6
	punpcklbw mm5, mm7 ;//mm7, mm5
	punpckhbw mm6, mm7 ;//mm7, mm6
	paddw mm3, mm5 ;//mm5, mm3
	paddw mm4, mm6 ;//mm6, mm4
	paddw mm1, mm3 ;//mm3, mm1
	paddw mm2, mm4 ;//mm4, mm2
	pxor mm6, mm6
	pcmpeqw mm5, mm5
	psubw mm6, mm5 ;//mm5, mm6
	paddw mm6, mm6
	paddw mm1, mm6 ;//mm6, mm1
	paddw mm2, mm6 ;//mm6, mm2
	psrlw mm1, $2 ;//$2, mm1
	psrlw mm2, $2 ;//$2, mm2
	packuswb mm1, mm2 ;//mm2, mm1
	movq mm2, [edx+8] ;//8(edx), mm2
	movq mm3, mm1 ;//mm1, mm3
	psubusb mm1, mm2 ;//mm2, mm1
	psubusb mm2, mm3 ;//mm3, mm2
	por mm1, mm2 ;//mm2, mm1
	movq mm2, mm0 ;//mm0, mm2
	punpcklbw mm0, mm7 ;//mm7, mm0
	punpckhbw mm2, mm7 ;//mm7, mm2
        movq mm3, mm1 ;//mm1, mm3
	punpcklbw mm1, mm7 ;//mm7, mm1
	punpckhbw mm3, mm7 ;//mm7, mm3
	paddw mm0, mm1 ;//mm1, mm0
	paddw mm0, mm2 ;//mm2, mm0
	paddw mm0, mm3 ;//mm3, mm0
	movq mm1, mm0 ;//mm0, mm1
	punpcklwd mm0, mm7 ;//mm7, mm0
	punpckhwd mm1, mm7 ;//mm7, mm1
	paddd mm0, mm1 ;//mm1, mm0
	movd ecx, mm0 ;//mm0, ecx

;//  add ecx, 0" : "=m" (s) : );
	mov [svar], ecx		;
	
	psrlq mm0, $32 ;//$32, mm0
	movd ecx, mm0 ;//mm0, ecx
    
;//add ecx, 0" : "=m" (s) : );
	mov [svar], ecx		;
	
	mov eax, ebx ;//ebx, eax
	add ebx, esi ;//esi, ebx
	add edx, esi ;//esi, edx
	dec edi
	jg d1top11
	jmp d1exit1

d1exit1:
 
;//mov edx, 0" : "=m" (s) : );

	mov edx, [svar]
		
d1exit:
		;; 	emms

	mov eax, edx
	
	pop edi
	pop esi
	pop edx
	pop ecx
	pop ebx

	ret


;; 
;; int dist1sse(unsigned char *blk1, unsigned char *blk2,
;;		int lx, int hx, int hy, int h, int distlim)


cglobal  dist1_sse
	
align 16
dist1_sse:


	push ebx
	push ecx
	push edx
	push esi
	push edi

	mov edi, [esp + off + 24]	;h
	mov edx, [esp + off + 20]	;hy
	mov eax, [esp + off + 16]	;hx
	mov esi, [esp + off + 12]	;lx 		
	
	test edi, edi
	jle near d1exitsse

	pxor mm7, mm7

	test eax, eax
	jne d1is10sse
	test edx, edx
	jne d1is10sse

	xor edx, edx

;//mov %0, %%eax" : : "m" (blk1) );
;//mov %0, %%ebx" : : "m" (blk2) );

	mov eax, [esp + off + 4]	;blk1
	mov ebx, [esp + off + 8]	;blk2	

;;;;;-----------------------------------------------------------
	
d1top00sse:
	movq mm0, [eax]

	psadbw mm0, [ebx]
	movq mm1, [eax+8]
	psadbw mm1, [ebx+8]
        paddd mm0, mm1
	
	movd ecx, mm0 ;//mm0, ecx
	add edx, ecx

		
;; //cmp %0, edx" : : "m" (distlim) )
;; //jge near d1exit1sse
	
	add eax, esi  ;//esi, eax
	add ebx, esi  ;//esi, ebx
	dec edi
	jg d1top00sse

	jmp d1exitsse		; return

d1is10sse:
	test eax, eax
	je near d1is01sse
	test edx, edx
	jne near d1is01sse

	xor edx, edx

;    //mov 0, eax" : : "m" (blk1) );
;    //mov 0, ebx" : : "m" (blk2) );
	
	mov eax, [esp + off + 4]	;blk1
	mov ebx, [esp + off + 8]	;blk2	
	
	pxor mm6, mm6
	pcmpeqw mm1, mm1
	psubw mm6, mm1

d1top10sse:
	movq mm0, [eax] ;//(eax ), mm0
	movq mm1, mm0
	punpcklbw mm0, mm7
	punpckhbw mm1, mm7
	movq mm2, [eax+1] ;//1(eax), mm2
	movq mm3, mm2
	punpcklbw mm2, mm7
	punpckhbw mm3, mm7
	paddw mm0, mm2 ;//mm2, mm0
	paddw mm1, mm3 ;//mm3, mm1
	paddw mm0, mm6 ;//mm6, mm0
	paddw mm1, mm6 ;//mm6, mm1
	psrlw mm0, $1 ;//$1, mm0
	psrlw mm1, $1 ;//$1, mm1
	packuswb mm0, mm1
	psadbw mm0, [ebx]
	
	movq mm1, [eax+8] ;//8(eax), mm1
	movq mm2, mm1 ;//mm1, mm2
	punpcklbw mm1, mm7 ;//mm7, mm1
	punpckhbw mm2, mm7 ;//mm7, mm2
	movq mm3, [eax+9] ;//9(eax), mm3
	movq mm4, mm3 ;//mm3, mm4
	punpcklbw mm3, mm7 ;//mm7, mm3
	punpckhbw mm4, mm7 ;//mm7, mm4
	paddw mm1, mm3 ;//mm3, mm1
	paddw mm2, mm4 ;//mm4, mm2
	paddw mm1, mm6 ;//mm6, mm1
	paddw mm2, mm6 ;//mm6, mm2
	psrlw mm1, $1 ;//$1, mm1
	psrlw mm2, $1 ;//$1, mm2
	packuswb mm1, mm2 ;//mm2, mm1
	psadbw mm1, [ebx+8]
	
	movq mm2, [ebx+8] ;//8(ebx), mm2
	movq mm3, mm1 ;//mm1, mm3
	psubusb mm1, mm2 ;//mm2, mm1
	psubusb mm2, mm3 ;//mm3, mm2
	por mm1, mm2 ;//mm2, mm1
	movq mm2, mm0 ;//mm0, mm2
	punpcklbw mm0, mm7 ;//mm7, mm0
	punpckhbw mm2, mm7 ;//mm7, mm2
	movq mm3, mm1 ;//mm1, mm3
	punpcklbw mm1, mm7 ;//mm7, mm1
	punpckhbw mm3, mm7 ;//mm7, mm3

	paddw mm0, mm1 ;//mm1, mm0
	movd ecx, mm0 ;//mm0, ecx
	add edx, ecx ;//ecx, edx 

	add eax, esi ;//esi, eax
	add ebx, esi ;//esi, ebx
	dec edi
	jg d1top10sse

	jmp d1exit1sse		; return

d1is01sse:
	test eax, eax
	jne near d1is11sse
	test edx, edx
	je near d1is11sse

	mov eax, [esp + off + 4]	;blk1
	mov ebx, [esp + off + 8]	;blk2	
	
	mov edx, eax ;//eax, ebx
 	add edx, esi ;//esi, ebx

	pxor mm6, mm6
	pcmpeqw mm1, mm1
	psubw mm6, mm1 ;//mm1, mm6

d1top01sse:
	movq mm0, [eax] ;//(eax ), mm0
	movq mm1, mm0 ;//mm0, mm1
	punpcklbw mm0, mm7 ;//mm7, mm0
	punpckhbw mm1, mm7 ;//mm7, mm1
	movq mm2, [ebx] ;//(ebx ), mm2
	movq mm3, mm2 ;//mm2, mm3
	punpcklbw mm2, mm7 ;//mm7, mm2
	punpckhbw mm3, mm7 ;//mm7, mm3
	paddw mm0, mm2 ;//mm2, mm0
	paddw mm1, mm3 ;//mm3, mm1
	paddw mm0, mm6 ;//mm6, mm0
	paddw mm1, mm6 ;//mm6, mm1
	psrlw mm0, $1 ;//$1, mm0
	psrlw mm1, $1 ;//$1, mm1
	packuswb mm0, mm1 ;//mm1, mm0

	psadbw mm0, [ebx] 
	
	movq mm1, [eax+8] ;//8(eax), mm1
	movq mm2, mm1 ;//mm1, mm2
	punpcklbw mm2, mm7 ;//mm7, mm1
	punpckhbw mm2, mm7 ;//mm7, mm2
	movq mm3, [ebx+8] ;//8(ebx), mm3
	movq mm4, mm3 ;//mm3, mm4
	punpcklbw mm3, mm7 ;//mm7, mm3
	punpckhbw mm4, mm7 ;//mm7, mm4
	paddw mm1, mm3 ;//mm3, mm1
	paddw mm2, mm4 ;//mm4, mm2
	paddw mm1, mm6 ;//mm6, mm1
	paddw mm2, mm6 ;//mm6, mm2
	psrlw mm1, $1 ;//$1, mm1
	psrlw mm2, $1 ;//$1, mm2
	packuswb mm1, mm2 ;//mm2, mm1

	psadbw mm1, [ebx]
	
	paddd mm0, mm1 ;//mm1, mm0
	movd ecx, mm0 ;//mm0, ecx

;//  add ecx, 0" : "=m" (s) : );
	add [svar], ecx
	
	mov eax, edx ;//ebx, eax  ????
	add ebx, esi ;//esi, edx
	add edx, esi ;//esi, ebx
	dec edi
	jg d1top01sse
	jmp d1exit1sse

d1is11sse:

;//mov 0, eax" : : "m" (blk1) );
;//mov 0, edx" : : "m" (blk2) );

	mov eax, [esp + off + 4]	;blk1
	mov ebx, [esp + off + 8]	;blk2	????? ebx

	mov edx, eax ;//eax, ebx
	add edx, esi ;//esi, ebx         ?????

d1top11sse:
	movq mm0, [eax]; //(eax ), mm0
	movq mm1, mm0 ;//mm0, mm1
	punpcklbw mm0, mm7 ;//mm7, mm0
	punpckhbw mm1, mm7 ;//mm7, mm1
	movq mm2, [eax+1] ;//1(eax), mm2
	movq mm3, mm2 ;//mm2, mm3
	punpcklbw mm2, mm7 ;//mm7, mm2
	punpckhbw mm3, mm7 ;//mm7, mm3
	paddw mm0, mm2 ;//mm2, mm0
	paddw mm1, mm3 ;//mm3, mm1
	movq mm2, [ebx]; //(ebx ), mm2
	movq mm3, mm2 ;//mm2, mm3
	punpcklbw mm2, mm7 ;//mm7, mm2
	punpckhbw mm3, mm7 ;//mm7, mm3
	movq mm4, [ebx+1] ;//1(ebx), mm4
	movq mm5, mm4 ;//mm4, mm5
	punpcklbw mm4, mm7 ;//mm7, mm4
	punpckhbw mm5, mm7 ;//mm7, mm5
	paddw mm2, mm4 ;//mm4, mm2
	paddw mm3, mm5 ;//mm5, mm3
	paddw mm0, mm2 ;//mm2, mm0
	paddw mm1, mm3 ;//mm3, mm1
	pxor mm6, mm6
	pcmpeqw mm5, mm5
	psubw mm6, mm5 ;//mm5, mm6
	paddw mm6, mm6
	paddw mm0, mm6 ;//mm6, mm0
	paddw mm1, mm6 ;//mm6, mm1
	psrlw mm0, $2 ;//$2, mm0
        psrlw mm1, $2 ;//$2, mm1
	packuswb mm0, mm1 ;//mm1, mm0

	psadbw mm0, [ebx]

	movq mm1, [eax+8]; //8(eax), mm1
	movq mm1, mm1 ;//mm1, mm2
	punpcklbw mm1, mm7 ;//mm7, mm1
	punpckhbw mm2, mm7 ;//mm7, mm2
	movq mm3, [eax+9]; //9(eax), mm3
	movq mm4, mm3 ;//mm3, mm4
	punpcklbw mm3, mm7 ;//mm7, mm3
	punpckhbw mm4, mm7 ;//mm7, mm4
	paddw mm1, mm3 ;//mm3, mm1
	paddw mm2, mm4 ;//mm4, mm2
	movq mm3, [ebx+8] ;//8(ebx), mm3
	movq mm4, mm3 ;//mm3, mm4
	punpcklbw mm3, mm7 ;//mm7, mm3
	punpckhbw mm4, mm7 ;//mm7, mm4
	movq mm5, [ebx+9] ;//9(ebx), mm5
	movq mm6, mm5 ;//mm5, mm6
	punpcklbw mm5, mm7 ;//mm7, mm5
	punpckhbw mm6, mm7 ;//mm7, mm6
	paddw mm3, mm5 ;//mm5, mm3
	paddw mm4, mm6 ;//mm6, mm4
	paddw mm1, mm3 ;//mm3, mm1
	paddw mm2, mm4 ;//mm4, mm2
	pxor mm6, mm6
	pcmpeqw mm5, mm5
	psubw mm6, mm5 ;//mm5, mm6
	paddw mm6, mm6
	paddw mm1, mm6 ;//mm6, mm1
	paddw mm2, mm6 ;//mm6, mm2
	psrlw mm1, $2 ;//$2, mm1
	psrlw mm2, $2 ;//$2, mm2
	packuswb mm1, mm2 ;//mm2, mm1

	psadbw mm1, [ebx+8]
	
	paddd mm0, mm1 ;//mm1, mm0
	movd ecx, mm0 ;//mm0, ecx

;//  add ecx, 0" : "=m" (s) : );
	mov [svar], ecx		;
	
	mov eax, edx ;//ebx, eax   ???? ebx
 	add ebx, esi ;//esi, ebx
	add edx, esi ;//esi, edx
	dec edi
	jg d1top11sse
	jmp d1exit1sse

;;; ------------------------------------------------------------


d1exit1sse:

;//mov edx, 0" : "=m" (s) : );

	mov edx, [svar]
		
d1exitsse:
	emms

	mov eax, edx
	
	pop edi
	pop esi
	pop edx
	pop ecx
	pop ebx

	ret

