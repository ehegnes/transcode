;;/*
;; *  pred_mmx.s
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

%define off 16
	
section .text

;;-----------------------------------------------------------
;; int sub_pred_mmx(unsigned char *pred, unsigned char *cur,
;;		     int lx, short *blk)
;;-----------------------------------------------------------
cglobal sub_pred_mmx

align 16
	
sub_pred_mmx:

	push edi
	push esi	
	push ebx
	push ecx

     mov	eax,[esp+off+8]	  ; cur
     mov	ebx,[esp+off+4]	  ; pred
     mov	ecx,[esp+off+16]  ; blk
     mov	edi,[esp+off+12]  ; lx 

     mov	esi,0x8;
     pxor	mm7,mm7;
 
.sub_top:
     movq	mm0,[eax];
     add	eax,edi;
     movq	mm2,[ebx];
     add	ebx,edi;
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     psubw	mm0,mm2;
     psubw	mm1,mm3;
     movq	[ecx],mm0;
     movq	[ecx+0x8],mm1;
     add	ecx,0x10;
     dec	esi;
     jg	near .sub_top;

	emms

	pop ecx			;
	pop ebx
	pop esi
	pop edi	
	
	xor eax, eax
	
     ret
 
;;-----------------------------------------------------------
;; int add_pred_mmx(unsigned char *pred, unsigned char *cur,
;;		     int lx, short *blk)
;;-----------------------------------------------------------

cglobal add_pred_mmx

align 16
	
add_pred_mmx:

	push edi
	push esi	
	push ebx
	push ecx

     mov	eax,[esp+off+8]	  ; cur
     mov	ebx,[esp+off+4]	  ; pred
     mov	ecx,[esp+off+16]  ; blk
     mov	edi,[esp+off+12]  ; lx 
		
     mov	esi,0x8;
     pxor	mm7,mm7;
 
.add_top:
     movq	mm0,[ecx];
     movq	mm1,[ecx+0x8];
     add	ecx,0x10;
     movq	mm2,[ebx];
     add	ebx,edi;
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     packuswb	mm0,mm1;
     movq	[eax],mm0;
     add	eax,edi;
     dec	esi;
     jg	near .add_top;

     emms

	pop ecx			;
	pop ebx
	pop esi
	pop edi	

	xor eax, eax
		
     ret
 
