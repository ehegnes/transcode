;;/*
;; *  swap.s
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

%define mask1 0FFFFFF00h
%define mask2 0000000FFh
%define mask3 0FF000000h
%define mask4 0FF0000FFh
%define mask5 00000FF00h
%define mask6 000FFFFFFh
%define mask7 000FF0000h						
				 
section .text

	;; 
	;; int ac_swap_rgb2bgr_asm(char *im, int pixels) 
	;;						
	;;

cglobal ac_swap_rgb2bgr_asm

%define off esp+20

%macro  SWAP_4 0
	prefetchnta [esi]		
	mov eax, [esi]		; R1 B0 G0 R0  (1)
	bswap eax               ; R0 G0 B0 R1
	ror eax, 8	        ; R1 R0 G0 B0
	mov edx, eax      

	and eax, mask6		; 00 R0 G0 B0
	xor edx, eax		; R1 00 00 00  

	mov ebx, [esi+4]	; G2 R2 B1 G1  (2)
	mov edi, ebx

	and edi, mask5          ; 00 00 B1 00
	rol edi, 16	        ; B1 00 00 00

	or eax, edi             ; B1 R0 G0 B0  (done) //edi free
	mov [esi], eax

	mov edi, ebx            ; tmp ebx
		
	and ebx, mask4          ; G2 00 00 G1
	ror edx, 16	        ; 00 00 R1 00
	or ebx, edx             ; G2 00 R1 G1 //edx free

	mov eax, [esi+8]	; B3 G3 R3 B2  (3)
	bswap eax               ; B2 R3 G3 B3
	rol eax, 8              ; R3 G3 B3 B2 

	mov edx, eax
	and edx, mask2		; 00 00 00 B2
	rol edx, 16             ; 00 B2 00 00

	or ebx, edx             ; G2 B2 R1 G1  (done)
	mov [esi+4], ebx
	
	and eax, mask1          ; R3 G3 B3 00 

	and edi, mask7		; 00 R2 00 00
	ror edi, 16		; 00 00 00 R2

	or eax, edi             ; R3 B3 B3 R2 (done)
	mov [esi+8], eax
	
	add esi, 12
%endmacro		
	
%macro	TEST_4 0
	mov eax, [esi]		; R1 B0 G0 R0
	and eax, mask5		; 00 00 G0 00
	mov [esi], eax	

	mov eax, [esi+4]	; G2 R2 B1 G1
	and eax, mask4          ; G1 00 00 G2
	mov [esi+4], eax

	mov eax, [esi+8]	; B3 G3 R3 B2 
	and eax, mask7		; 00 G3 00 00
	mov [esi+8], eax
	
	add esi, 12
%endmacro		
		
align 16
ac_swap_rgb2bgr_asm:

	push ebx 
	push ecx
	push edx
	push edi
	push esi
	
	mov esi, [off+ 4]	; im
	mov ecx, [off+ 8]	; pixels

.loop:
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
							
	sub ecx, 64		; done with 64 pixels
	jg .loop

.exit:		

	xor eax, eax		; exit

	pop esi
	pop edi
	pop edx		
	pop ecx	
	pop ebx

	ret			


