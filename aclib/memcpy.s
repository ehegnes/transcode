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
	;; void * ac_memcpy_mmx(void *dest, void *src, int bytes);
	;;

cglobal  ac_memcpy_mmx
extern memcpy

%define PENTIUM_LINE_SIZE 32	; PMMX/PII cache line size
%define PENTIUM_CACHE_SIZE 8192	; PMMX/PII total cache size
; Leave room because writes may touch the cache too (PII)
%define PENTIUM_CACHE_BLOCK (PENTIUM_CACHE_SIZE - PENTIUM_LINE_SIZE*2)

align 16
ac_memcpy_mmx:

	push ebx
	push esi
	push edi

	mov edi, [esp+12+4]	; dest
	mov esi, [esp+12+8]	; src
	mov ecx, [esp+12+12]	; bytes
	mov edx, ecx

	mov eax, 64		; constant

	cmp ecx, eax
	jb near .lastcopy		; Just rep movs if <64 bytes

	; Align destination pointer to a multiple of 8 bytes
	xor ecx, ecx
	mov cl, 7
	and ecx, edi
	jz .blockloop		; already aligned (edi%8 == 0)
	neg cl
	and cl, 7		; ecx = 8 - edi%8
	sub edx, ecx		; subtract from count
	rep movsb		; and copy

.blockloop:
	mov ebx, edx
	shr ebx, 6
	jz .lastcopy		; <64 bytes left
	cmp ebx, PENTIUM_CACHE_BLOCK/64
	jb .small
	mov ebx, PENTIUM_CACHE_BLOCK/64
.small:
	mov ecx, ebx
	shl ecx, 6
	sub edx, ecx
	add esi, ecx
.loop1:
	test eax, [esi-32]	; touch each cache line in reverse order
	test eax, [esi-64]
	sub esi, eax
	dec ebx
	jnz .loop1
	shr ecx, 6
.loop2:
	movq mm0, [esi   ]
	movq mm1, [esi+ 8]
	movq mm2, [esi+16]
	movq mm3, [esi+24]
	movq mm4, [esi+32]
	movq mm5, [esi+40]
	movq mm6, [esi+48]
	movq mm7, [esi+56]
	movq [edi   ], mm0
	movq [edi+ 8], mm1
	movq [edi+16], mm2
	movq [edi+24], mm3
	movq [edi+32], mm4
	movq [edi+40], mm5
	movq [edi+48], mm6
	movq [edi+56], mm7
	add esi, eax
	add edi, eax
	dec ecx
	jnz .loop2
	jmp .blockloop

.lastcopy:
	mov ecx, edx
	shr ecx, 2
	jz .lastcopy2
	rep movsd
.lastcopy2:
	xor ecx, ecx
	mov cl, 3
	and ecx, edx
	jz .end
	rep movsb

.end:
	; return dest
	emms
	pop edi
	pop esi
	pop ebx
	mov eax, [esp+4]
	ret


	;; 
	;; void * ac_memcpy_sse(void *dest, void *src, int bytes);
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

	; xor eax, eax		; exit
	mov eax, [esp+20]	; dest

	pop esi
	pop edi
	pop ecx
	pop ebx
	
	ret			


	;; 
	;; void * ac_memcpy_sse2(void *dest, void *src, int bytes);
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

	;; xor eax, eax		; exit
	mov eax, [esp+20]	; dest

	pop esi
	pop edi
	pop ecx
	pop ebx
	
	ret			
;; 
;; void * ac_memcpy_amdmmx(void *dest, void *src, int bytes);
;;

;; Code taken from AMD's Athlon Processor x86 Code Optimization Guide
;; "Use MMX Instructions for Block Copies and Block Fills"

%define TINY_BLOCK_COPY 	64
%define IN_CACHE_COPY 		64 * 1024
%define UNCACHED_COPY 		197 * 1024
%define CACHEBLOCK 		80h

cglobal  ac_memcpy_amdmmx

align 16
ac_memcpy_amdmmx

	push ebx
	push ecx
	push edi
	push esi

	mov ecx, [esp+28]	; bytes 
	mov edi, [esp+20]	; dest
	mov esi, [esp+24]	; src	
        mov ebx, ecx 		; keep a copy of count
        
        cld
        cmp ecx, TINY_BLOCK_COPY
        jb near .memcpy_ic_3 	; tiny? skip mmx copy
        
        cmp ecx, 32*1024 	; don't align between 32k-64k because
	jbe .memcpy_do_align 	; it appears to be slower
	cmp ecx, 64*1024
	jbe .memcpy_align_done
        
.memcpy_do_align:
	mov ecx, 8 		; a trick that's faster than rep movsb...
	sub ecx, edi 		; align destination to qword
	and ecx, 111b 		; get the low bits
	sub ebx, ecx 		; update copy count
	neg ecx 		; set up to jump into the array
	add ecx, .memcpy_align_done ; offset?
	jmp ecx 		; jump to array of movsb's
        
align 4
	movsb
	movsb
	movsb
	movsb
	movsb
	movsb
	movsb
	movsb
        
.memcpy_align_done: 		; destination is dword aligned
	mov ecx, ebx 		; number of bytes left to copy
	shr ecx, 6 		; get 64-byte block count
	jz .memcpy_ic_2 	; finish the last few bytes
	cmp ecx, IN_CACHE_COPY/64 ; too big 4 cache? use uncached copy 
	jae .memcpy_uc_test

align 16
.memcpy_ic_1: 			; 64-byte block copies, in-cache copy
	prefetchnta [esi + (200*64/34+192)] ; start reading ahead
	movq mm0, [esi+0] 	; read 64 bits
	movq mm1, [esi+8]
	movq [edi+0], mm0 	; write 64 bits
	movq [edi+8], mm1 	; note: the normal movq writes the
	movq mm2, [esi+16] 	; data to cache; a cache line will be
	movq mm3, [esi+24] 	; allocated as needed, to store the data
	movq [edi+16], mm2
	movq [edi+24], mm3
	movq mm0, [esi+32]
	movq mm1, [esi+40]
	movq [edi+32], mm0
	movq [edi+40], mm1
	movq mm2, [esi+48]
	movq mm3, [esi+56]
	movq [edi+48], mm2
	movq [edi+56], mm3

	add esi, 64 		; update source pointer
	add edi, 64 		; update destination pointer
	dec ecx 		; count down
	jnz .memcpy_ic_1 	; last 64-byte block?
        
.memcpy_ic_2:
	mov ecx, ebx 		; has valid low 6 bits of the byte count
.memcpy_ic_3:
	shr ecx, 2 		; dword count
	and ecx, 1111b 		; only look at the "remainder" bits
	neg ecx 		; set up to jump into the array
	add ecx, .memcpy_last_few  ; offset?
	jmp ecx 		; jump to array of movsd's
        
.memcpy_uc_test:
	cmp ecx, UNCACHED_COPY/64 ; big enough? use block prefetch copy
	jae .memcpy_bp_1
.memcpy_64_test:
	or ecx, ecx 		; tail end of block prefetch will jump here
	jz .memcpy_ic_2 	; no more 64-byte blocks left

align 16
.memcpy_uc_1: 			; 64-byte blocks, uncached copy
	prefetchnta [esi + (200*64/34+192)] ; start reading ahead
	movq mm0,[esi+0] 	; read 64 bits
	add edi,64 		; update destination pointer
	movq mm1,[esi+8]
	add esi,64 		; update source pointer
	movq mm2,[esi-48]
	movntq [edi-64], mm0 	; write 64 bits, bypassing the cache
	movq mm0,[esi-40] 	; note: movntq also prevents the CPU
	movntq [edi-56], mm1 	; from READING the destination address
	movq mm1,[esi-32] 	; into the cache, only to be over-written
	movntq [edi-48], mm2 	; so that also helps performance
	movq mm2,[esi-24]
	movntq [edi-40], mm0
	movq mm0,[esi-16]
	movntq [edi-32], mm1
	movq mm1,[esi-8]
	movntq [edi-24], mm2
	movntq [edi-16], mm0
	dec ecx
	movntq [edi-8], mm1
	jnz .memcpy_uc_1 	; last 64-byte block?
	jmp .memcpy_ic_2 	; almost dont
        
.memcpy_bp_1: 			; large blocks, block prefetch copy
	cmp ecx, CACHEBLOCK 	; big enough to run another prefetch loop?
	jl .memcpy_64_test 	; no, back to regular uncached copy
	mov eax, CACHEBLOCK / 2 ; block prefetch loop, unrolled 2X
	add esi, CACHEBLOCK * 64 ; move to the top of the block
align 16
.memcpy_bp_2:
	mov edx, [esi-64] 	; grab one address per cache line
	mov edx, [esi-128] 	; grab one address per cache line
	sub esi, 128 		; go reverse order
	dec eax 		; count down the cache lines
	jnz .memcpy_bp_2 	; keep grabbing more lines into cache
	mov eax, CACHEBLOCK 	; now that it's in cache, do the copy
        
align 16
.memcpy_bp_3:
	movq mm0, [esi ] 	; read 64 bits
	movq mm1, [esi+ 8]
	movq mm2, [esi+16]
	movq mm3, [esi+24]
	movq mm4, [esi+32]
	movq mm5, [esi+40]
	movq mm6, [esi+48]
	movq mm7, [esi+56]
	add esi, 64 		; update source pointer
	movntq [edi ], mm0 	; write 64 bits, bypassing cache
	movntq [edi+ 8], mm1 	; note: movntq also prevents the CPU
	movntq [edi+16], mm2 	; from READING the destination address
	movntq [edi+24], mm3 	; into the cache, only to be over-written,
	movntq [edi+32], mm4 	; so that also helps performance
	movntq [edi+40], mm5
	movntq [edi+48], mm6
	movntq [edi+56], mm7
	add edi, 64 		; update dest pointer
	dec eax 		; count down
	jnz .memcpy_bp_3 	; keep copying
	sub ecx, CACHEBLOCK 	; update the 64-byte block count
	jmp .memcpy_bp_1 	; keep processing blocks
        
align 4
	movsd
	movsd 			; perform last 1-15 dword copies
	movsd
	movsd
	movsd
	movsd
	movsd
	movsd
	movsd
	movsd 			; perform last 1-7 dword copies
	movsd
	movsd
	movsd
	movsd
	movsd
	movsd

.memcpy_last_few: 		; dword aligned from before movsd's
	mov ecx, ebx 		; has valid low 2 bits of the byte count
	and ecx, 11b 		; the last few cows must come home
	jz .memcpy_final 	; no more, let's leave
	rep movsb 		; the last 1, 2, or 3 bytes

.memcpy_final:
	emms 			; clean up the MMX state
	sfence 			; flush the write buffer
;;	xor eax, eax		; ret value = 0
	mov eax, [esp+20] 	; ret value = destination pointer // EMS
        
	pop esi
	pop edi
	pop ecx
	pop ebx
	
	ret
