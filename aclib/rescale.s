;;/*
;; *  rescale.s
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

extern print_mmx

%define off 16
	
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
	;; int ac_rescale_mmxext(char *row1, char *row2, char *out, int bytes, 
	;; 		         unsigned int weight1, unsigned int weight2)
	;;

cglobal ac_rescale_mmxext

align 16
ac_rescale_mmxext:	

	push ebx
	push ecx
	push edx	
	push edi

	mov ebx, [esp+off+ 4]	; row1
	mov eax, [esp+off+ 8]	; row2	

	mov edi, [esp+off+12]	; out		

	mov ecx, [esp+off+20]  ; w1
	mov edx, [esp+off+24]  ; w2	

	cmp ecx, edx
	jl .no_switch

	mov ecx, eax
	mov eax, ebx
	mov ebx, ecx 
	mov ecx, edx
	
.no_switch:	
		
	movd mm3, ecx		; 0:	0:	0:	w1	
	movq mm7, mm3

	psllq mm3, 16           ; 0:	0:	w1:	0
	por mm3, mm7            ; 0:	0:	w1:	w1
	
	psllq mm3, 16	
	por mm3, mm7            ; 0:	w1:	w1:	w1

	psllq mm3, 16	
	por mm3, mm7		;w1:	w1:	w1:	w1	

	mov ecx, [esp+off+16]	; bytes 
	shr ecx, 3		; /8
	
	pxor mm7, mm7           ; zero

	
.rescale:		
	movq mm0, [ebx]   	;  A
	movq mm4, [eax]	        ;  B

;;; 0-3	(A-B)
	movq mm2, mm0
	psubusb mm2, mm4
	movq mm5, mm2           ;  save
	
	punpckhbw mm2, mm7
	pmulhuw mm2, mm3	
	
	packsswb mm2, mm7       ; collapse
	
	movq mm6, mm2	        ; save into mm6
	psllq mm6, 32		

;;; 4-7 (A-B)
	movq mm2, mm5
	punpcklbw mm2, mm7
	pmulhuw mm2, mm3	
	
	packsswb mm2, mm7       ; collapse
	
	por mm6, mm2	        ; save into mm6

	movq mm5, mm4
	paddb mm5, mm6
	
;;; 0-3	(B-A)
	movq mm2, mm4
	psubusb mm2, mm0
	movq mm4, mm2
	
	punpckhbw mm2, mm7
	pmulhuw mm2, mm3	
	
	packsswb mm2, mm7       ; collapse
	
	movq mm6, mm2	        ; save into mm6
	psllq mm6, 32		

;;; 4-7 (B-A)
	movq mm2, mm4
	punpcklbw mm2, mm7
	pmulhuw mm2, mm3	
	
	packsswb mm2, mm7       ; collapse
	
	por mm6, mm2	        ; save into mm6

	psubb mm5, mm6
		
	movq [edi], mm5
	;; done

	add eax, 8		; inc pointers
	add ebx, 8
	add edi, 8
	
	dec ecx
	jg .rescale

.exit:		
	xor eax, eax		; exit

	pop edi
	pop edx
	pop ecx
	pop ebx
	
	ret			

	;; 
	;; int ac_rescale_sse(char *row1, char *row2, char *out, int bytes, 
	;; 		      unsigned int weight1, unsigned int weight2)
	;;

cglobal ac_rescale_sse

align 16
ac_rescale_sse:	

	push ebx
	push ecx
	push edx	
	push edi

	mov ebx, [esp+off+ 4]	; row1
	mov eax, [esp+off+ 8]	; row2	

	mov edi, [esp+off+12]	; out		

	mov ecx, [esp+off+20]  ; w1
	mov edx, [esp+off+24]  ; w2	

	cmp ecx, edx
	jl .no_switch

	mov ecx, eax
	mov eax, ebx
	mov ebx, ecx 
	mov ecx, edx
	
.no_switch:	
		
	movd mm3, ecx		; 0:	0:	0:	w1	
	movq mm7, mm3

	psllq mm3, 16           ; 0:	0:	w1:	0
	por mm3, mm7            ; 0:	0:	w1:	w1
	
	psllq mm3, 16	
	por mm3, mm7            ; 0:	w1:	w1:	w1

	psllq mm3, 16	
	por mm3, mm7		;w1:	w1:	w1:	w1	

	mov ecx, [esp+off+16]	; bytes 
	shr ecx, 3		; /8
	
	pxor mm7, mm7           ; zero

	
.rescale:		
	prefetchnta [eax]
	prefetchnta [ebx]

	movq mm0, [ebx]   	;  A
	movq mm4, [eax]	        ;  B

;;; 0-3	(A-B)
	movq mm2, mm0
	psubusb mm2, mm4
	movq mm5, mm2           ;  save
	
	punpckhbw mm2, mm7
	pmulhuw mm2, mm3	
	
	packsswb mm2, mm7       ; collapse
	
	movq mm6, mm2	        ; save into mm6
	psllq mm6, 32		

;;; 4-7 (A-B)
	movq mm2, mm5
	punpcklbw mm2, mm7
	pmulhuw mm2, mm3	
	
	packsswb mm2, mm7       ; collapse
	
	por mm6, mm2	        ; save into mm6

	movq mm5, mm4
	paddb mm5, mm6
	
;;; 0-3	(B-A)
	movq mm2, mm4
	psubusb mm2, mm0
	movq mm4, mm2
	
	punpckhbw mm2, mm7
	pmulhuw mm2, mm3	
	
	packsswb mm2, mm7       ; collapse
	
	movq mm6, mm2	        ; save into mm6
	psllq mm6, 32		

;;; 4-7 (B-A)
	movq mm2, mm4
	punpcklbw mm2, mm7
	pmulhuw mm2, mm3	
	
	packsswb mm2, mm7       ; collapse
	
	por mm6, mm2	        ; save into mm6

	psubb mm5, mm6
		
	movntq [edi], mm5
	;; done

	add eax, 8		; inc pointers
	add ebx, 8
	add edi, 8
	
	dec ecx
	jg .rescale

.exit:		
	sfence
		
	xor eax, eax		; exit

	pop edi
	pop edx
	pop ecx
	pop ebx
	
	ret			


	;; 
	;; int ac_rescale_sse2(char *row1, char *row2, char *out, int bytes, 
	;; 		       unsigned int weight1, unsigned int weight2)
	;;

cglobal ac_rescale_sse2

align 16
ac_rescale_sse2:	

	push ebx
	push ecx
	push edx	
	push edi

	mov ebx, [esp+off+ 4]	; row1
	mov eax, [esp+off+ 8]	; row2	

	mov edi, [esp+off+12]	; out		

	mov ecx, [esp+off+20]  ; w1
	mov edx, [esp+off+24]  ; w2	

	cmp ecx, edx
	jl .no_switch

	mov ecx, eax
	mov eax, ebx
	mov ebx, ecx 
	mov ecx, edx
	
.no_switch:	
		
	movd   xmm3, ecx	; 0 0 0 0 0 0 0 w1
	movdqa xmm7, xmm3

	psllq xmm3, 16          
	por xmm3, xmm7          
	
	psllq xmm3, 16	
	por xmm3, xmm7          

	psllq xmm3, 16	
	por xmm3, xmm7

	psllq xmm3, 16	
	por xmm3, xmm7		
	
	psllq xmm3, 16	
	por xmm3, xmm7		
	
	psllq xmm3, 16	
	por xmm3, xmm7

	psllq xmm3, 16	
	por xmm3, xmm7		; w1 w1 w1 w1 w1 w1 w1 w1
	
	mov ecx, [esp+off+16]		; bytes 
	shr ecx, 4			; /16
	
	pxor xmm7, xmm7			; zero

	
.rescale:		
	prefetchnta [eax]
	prefetchnta [ebx]

	movdqa xmm0, [ebx]		;  A
	movdqa xmm4, [eax]	        ;  B

;;; 0-3	(A-B)
	movdqa xmm2, xmm0
	psubusb xmm2, xmm4
	movdqa xmm5, xmm2		;  save
	
	punpckhbw xmm2, xmm7
	pmulhuw xmm2, xmm3	
	
	packsswb xmm2, xmm7		; collapse
	
	movdqa xmm6, xmm2	        ; save into xmm6
	pslldq xmm6, 64		

;;; 4-7 (A-B)
	movdqa xmm2, xmm5
	punpcklbw xmm2, xmm7
	pmulhuw xmm2, xmm3	
	
	packsswb xmm2, xmm7       ; collapse
	
	por xmm6, xmm2	          ; save into xmm6

	movdqa xmm5, xmm4
	paddb xmm5, xmm6
	
;;; 0-3	(B-A)
	movdqa xmm2, xmm4
	psubusb xmm2, xmm0
	movdqa xmm4, xmm2
	
	punpckhbw xmm2, xmm7
	pmulhuw xmm2, xmm3	
	
	packsswb xmm2, xmm7       ; collapse
	
	movdqa xmm6, xmm2	        ; save into xmm6
	pslldq xmm6, 64		

;;; 4-7 (B-A)
	movdqa xmm2, xmm4
	punpcklbw xmm2, xmm7
	pmulhuw xmm2, xmm3	
	
	packsswb xmm2, xmm7       ; collapse
	
	por xmm6, xmm2	          ; save into xmm6

	psubb xmm5, xmm6
		
	movntdq [edi], xmm5
	;; done

	add eax, 16		  ; inc pointers
	add ebx, 16
	add edi, 16
	
	dec ecx
	jg .rescale

.exit:		
	sfence

	xor eax, eax		; exit

	pop edi
	pop edx
	pop ecx
	pop ebx
	
	ret			


	