;;========================================================================
;;== mmx/sse accelarted subroutines for modules motion.c and transfrm.c ==
;;== ported and adjusted by Gerhard Monzel to use with transcode-0.6.2  ==
;;== (needs nasm to compile)                                            ==
;;==                                                                    ==
;;== - 12.11.2002: first realease                                       ==
;;== - 14.11.2002: some adjustments                                     ==
;;==                                                                    ==
;;== known bugs:                                                        ==
;;== - bdist2mmx will cause a segfault in transcode                     ==
;;========================================================================
bits 32

%macro cglobal 1 
	%ifdef PREFIX
		global _%1 
		%define %1 _%1
	%else
		global %1
	%endif
%endmacro

;;----------------------------------------------------------
section .data

align 16

svar	dw 0	;; int s
pfa	dw 0    ;; unsigned char *pfa,*pfb,*pfc,*pba,*pbb,*pbc;
pfb	dw 0
pfc     dw 0
pba	dw 0
pbb	dw 0
pbc	dw 0

;;unused times 4 dw 1
  
;;----------------------------------------------------------
section .text

%define off 20   
%define ofx 12

;;===========================
;;== routines for motion.c ==
;;===========================

;;-----------------------------------------------------------
;; int dist1mmx(unsigned char *blk1, unsigned char *blk2,
;;		int lx, int hx, int hy, int h, int distlim)
;;-----------------------------------------------------------
cglobal dist1mmx

align 16
dist1mmx:
     push       ebx
     push 	ecx
     push 	edx
     push 	esi
     push 	edi

     mov	dword [svar],0x0
     mov	edi,[esp+off+24]  ; 
     mov	edx,[esp+off+20]  ; 
     mov	eax,[esp+off+16]  ; 
     mov	esi,[esp+off+12]  ; 
     test	edi,edi;
     jle	near .d1exit;
     pxor	mm7,mm7;
     test	eax,eax;
     jne	.d1is10;
     test	edx,edx;
     jne	.d1is10;
     xor	edx,edx;
     mov	eax,[esp+off+4];
     mov	ebx,[esp+off+8];
 
.d1top00:
     movq	mm0,[eax];
     movq	mm1,[ebx];
     movq	mm2,mm0;
     psubusb	mm0,mm1;
     psubusb	mm1,mm2;
     por	mm0,mm1;
     movq	mm2,[eax+8];
     movq	mm3,[ebx+8];
     movq	mm4,mm2;
     psubusb	mm2,mm3;
     psubusb	mm3,mm4;
     por	mm2,mm3;
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm1;
     paddw	mm0,mm2;
     paddw	mm0,mm3;
     movq	mm1,mm0;
     punpcklwd	mm0,mm7;
     punpckhwd	mm1,mm7;
     paddd	mm0,mm1;
     movd	ecx,mm0;
     add	edx,ecx;
     psrlq	mm0,0x20;
     movd	ecx,mm0;
     add	edx,ecx;
     cmp	edx,[esp+off+28];  
     jge	near .d1exit1;	   
     add	eax,esi;
     add	ebx,esi;
     dec	edi;
     jg	        .d1top00;
     jmp	.d1exit1;
 
.d1is10:
     test	eax,eax;
     je	  	near .d1is01;
     test	edx,edx;
     jne	near .d1is01;
     xor	edx,edx;
     mov	eax,[esp+off+4];
     mov	ebx,[esp+off+8];
     pxor	mm6,mm6;
     pcmpeqw	mm1,mm1;
     psubw	mm6,mm1;
 
.d1top10:
     movq	mm0,[eax];
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     movq	mm2,[eax+0x1];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psrlw	mm0,0x1;
     psrlw	mm1,0x1;
     packuswb	mm0,mm1;
     movq	mm1,[ebx];
     movq	mm2,mm0;
     psubusb	mm0,mm1;
     psubusb	mm1,mm2;
     por	mm0,mm1;
     movq	mm1,[eax+0x8];
     movq	mm2,mm1;
     punpcklbw	mm1,mm7;
     punpckhbw	mm2,mm7;
     movq	mm3,[eax+0x9];
     movq	mm4,mm3;
     punpcklbw	mm3,mm7;
     punpckhbw	mm4,mm7;
     paddw	mm1,mm3;
     paddw	mm2,mm4;
     paddw	mm1,mm6;
     paddw	mm2,mm6;
     psrlw	mm1,0x1;
     psrlw	mm2,0x1;
     packuswb	mm1,mm2;
     movq	mm2,[ebx+0x8];
     movq	mm3,mm1;
     psubusb	mm1,mm2;
     psubusb	mm2,mm3;
     por	mm1,mm2;
     movq	mm2,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm2,mm7;
     movq	mm3,mm1;
     punpcklbw	mm1,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm1;
     paddw	mm0,mm2;
     paddw	mm0,mm3;
     movq	mm1,mm0;
     punpcklwd	mm0,mm7;
     punpckhwd	mm1,mm7;
     paddd	mm0,mm1;
     movd	ecx,mm0;
     add	edx,ecx;
     psrlq	mm0,0x20;
     movd	ecx,mm0;
     add	edx,ecx;
     add	eax,esi;
     add	ebx,esi;
     dec	edi;
     jg	        .d1top10;
     jmp	.d1exit1;
 
.d1is01:
     test	eax,eax;
     jne	near .d1is11;
     test	edx,edx;
     je	  	near .d1is11;
     mov	eax,[esp+off+4];
     mov	edx,[esp+off+8];
     mov	ebx,eax;
     add	ebx,esi;
     pxor	mm6,mm6;
     pcmpeqw	mm1,mm1;
     psubw	mm6,mm1;
 
.d1top01:
     movq	mm0,[eax];
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     movq	mm2,[ebx];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psrlw	mm0,0x1;
     psrlw	mm1,0x1;
     packuswb	mm0,mm1;
     movq	mm1,[edx];
     movq	mm2,mm0;
     psubusb	mm0,mm1;
     psubusb	mm1,mm2;
     por	mm0,mm1;
     movq	mm1,[eax+0x8];
     movq	mm2,mm1;
     punpcklbw	mm1,mm7;
     punpckhbw	mm2,mm7;
     movq	mm3,[ebx+0x8];
     movq	mm4,mm3;
     punpcklbw	mm3,mm7;
     punpckhbw	mm4,mm7;
     paddw	mm1,mm3;
     paddw	mm2,mm4;
     paddw	mm1,mm6;
     paddw	mm2,mm6;
     psrlw	mm1,0x1;
     psrlw	mm2,0x1;
     packuswb	mm1,mm2;
     movq	mm2,[edx+0x8];
     movq	mm3,mm1;
     psubusb	mm1,mm2;
     psubusb	mm2,mm3;
     por	mm1,mm2;
     movq	mm2,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm2,mm7;
     movq	mm3,mm1;
     punpcklbw	mm1,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm1;
     paddw	mm0,mm2;
     paddw	mm0,mm3;
     movq	mm1,mm0;
     punpcklwd	mm0,mm7;
     punpckhwd	mm1,mm7;
     paddd	mm0,mm1;
     movd	ecx,mm0;
     add	[svar],ecx;
     psrlq	mm0,0x20;
     movd	ecx,mm0;
     add	[svar],ecx;
     mov	eax,ebx;
     add	edx,esi;
     add	ebx,esi;
     dec	edi;
     jg	        .d1top01;
     jmp	.d1exit;
 
.d1is11:
     mov	eax,[esp+off+4];
     mov	edx,[esp+off+8];
     mov	ebx,eax;
     add	ebx,esi;
 
.d1top11:
     movq	mm0,[eax];
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     movq	mm2,[eax+0x1];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     movq	mm2,[ebx];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     movq	mm4,[ebx+0x1];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     pxor	mm6,mm6;
     pcmpeqw	mm5,mm5;
     psubw	mm6,mm5;
     paddw	mm6,mm6;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psrlw	mm0,0x2;
     psrlw	mm1,0x2;
     packuswb	mm0,mm1;
     movq	mm1,[edx];
     movq	mm2,mm0;
     psubusb	mm0,mm1;
     psubusb	mm1,mm2;
     por	mm0,mm1;
     movq	mm1,[eax+0x8];
     movq	mm2,mm1;
     punpcklbw	mm1,mm7;
     punpckhbw	mm2,mm7;
     movq	mm3,[eax+0x9];
     movq	mm4,mm3;
     punpcklbw	mm3,mm7;
     punpckhbw	mm4,mm7;
     paddw	mm1,mm3;
     paddw	mm2,mm4;
     movq	mm3,[ebx+0x8];
     movq	mm4,mm3;
     punpcklbw	mm3,mm7;
     punpckhbw	mm4,mm7;
     movq	mm5,[ebx+0x9];
     movq	mm6,mm5;
     punpcklbw	mm5,mm7;
     punpckhbw	mm6,mm7;
     paddw	mm3,mm5;
     paddw	mm4,mm6;
     paddw	mm1,mm3;
     paddw	mm2,mm4;
     pxor	mm6,mm6;
     pcmpeqw	mm5,mm5;
     psubw	mm6,mm5;
     paddw	mm6,mm6;
     paddw	mm1,mm6;
     paddw	mm2,mm6;
     psrlw	mm1,0x2;
     psrlw	mm2,0x2;
     packuswb	mm1,mm2;
     movq	mm2,[edx+0x8];
     movq	mm3,mm1;
     psubusb	mm1,mm2;
     psubusb	mm2,mm3;
     por	mm1,mm2;
     movq	mm2,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm2,mm7;
     movq	mm3,mm1;
     punpcklbw	mm1,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm1;
     paddw	mm0,mm2;
     paddw	mm0,mm3;
     movq	mm1,mm0;
     punpcklwd	mm0,mm7;
     punpckhwd	mm1,mm7;
     paddd	mm0,mm1;
     movd	ecx,mm0;
     add	[svar],ecx; 
     psrlq	mm0,0x20;
     movd	ecx,mm0;
     add	[svar],ecx;  
     mov	eax,ebx;
     add	ebx,esi;
     add	edx,esi;
     dec	edi;
     jg	        .d1top11;
     jmp	.d1exit;
 
.d1exit1:
     mov	edx, [svar];
 
.d1exit:
     emms
     mov	eax,edx;

     pop 	edi
     pop 	esi
     pop 	edx
     pop 	ecx
     pop 	ebx

     ret
 
;;-----------------------------------------------------------
;; int dist1sse(unsigned char *blk1, unsigned char *blk2,
;;		int lx, int hx, int hy, int h, int distlim)
;;-----------------------------------------------------------
cglobal dist1sse

align 16
dist1sse:
     push       ebx
     push 	ecx
     push 	edx
     push 	esi
     push 	edi

     mov	dword [svar],0x0;
     mov	edi,[esp+off+24]  ; 
     mov	edx,[esp+off+20]  ; 
     mov	eax,[esp+off+16]  ; 
     mov	esi,[esp+off+12]  ; 

     test	edi,edi;
     jle	near .d1exitsse;
     pxor	mm7,mm7;
     test	eax,eax;
     jne	near .d1is10sse;
     test	edx,edx;
     jne	near .d1is10sse;
     xor	edx,edx;
     mov	eax,[esp+off+4];
     mov	ebx,[esp+off+8];
 
.d1top00sse:
     movq	mm0,[eax];
     psadbw	mm0,[ebx];
     movq	mm1,[eax+0x8];
     psadbw	mm1,[ebx+0x8];
     paddd	mm0,mm1;
     movd	ecx,mm0;
     add	edx,ecx;
     cmp	edx,[esp+off+28];
     jge	near .d1exit1sse;
     add	eax,esi;
     add	ebx,esi;
     dec	edi;
     jg		near .d1top00sse;
     jmp	near .d1exit1sse;
 
.d1is10sse:
     test	eax,eax;
     je		near .d1is01sse;
     test	edx,edx;
     jne	near .d1is01sse;
     xor	edx,edx;
     mov	eax,[esp+off+4];
     mov	ebx,[esp+off+8];
     pxor	mm6,mm6;
     pcmpeqw	mm1,mm1;
     psubw	mm6,mm1;
 
.d1top10sse:
     movq	mm0,[eax];
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     movq	mm2,[eax+0x1];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psrlw	mm0,0x1;
     psrlw	mm1,0x1;
     packuswb	mm0,mm1;
     psadbw	mm0,[ebx];
     movq	mm1,[eax+0x8];
     movq	mm2,mm1;
     punpcklbw	mm1,mm7;
     punpckhbw	mm2,mm7;
     movq	mm3,[eax+0x9];
     movq	mm4,mm3;
     punpcklbw	mm3,mm7;
     punpckhbw	mm4,mm7;
     paddw	mm1,mm3;
     paddw	mm2,mm4;
     paddw	mm1,mm6;
     paddw	mm2,mm6;
     psrlw	mm1,0x1;
     psrlw	mm2,0x1;
     packuswb	mm1,mm2;
     psadbw	mm1,[ebx+0x8];
     paddd	mm0,mm1;
     movd	ecx,mm0;
     add	edx,ecx;
     add	eax,esi;
     add	ebx,esi;
     dec	edi;
     jg		near .d1top10sse;
     jmp	near .d1exit1sse;
 
.d1is01sse:
     test	eax,eax;
     jne	near .d1is11sse;
     test	edx,edx;
     je		near .d1is11sse;
     mov	eax,[esp+off+4];
     mov	ebx,[esp+off+8];
     mov	edx,eax;
     add	edx,esi;
     pxor	mm6,mm6;
     pcmpeqw	mm1,mm1;
     psubw	mm6,mm1;
 
.d1top01sse:
     movq	mm0,[eax];
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     movq	mm2,[edx];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psrlw	mm0,0x1;
     psrlw	mm1,0x1;
     packuswb	mm0,mm1;
     psadbw	mm0,[ebx];
     movq	mm1,[eax+0x8];
     movq	mm2,mm1;
     punpcklbw	mm1,mm7;
     punpckhbw	mm2,mm7;
     movq	mm3,[edx+0x8];
     movq	mm4,mm3;
     punpcklbw	mm3,mm7;
     punpckhbw	mm4,mm7;
     paddw	mm1,mm3;
     paddw	mm2,mm4;
     paddw	mm1,mm6;
     paddw	mm2,mm6;
     psrlw	mm1,0x1;
     psrlw	mm2,0x1;
     packuswb	mm1,mm2;
     psadbw	mm1,[ebx+0x8];
     paddd	mm0,mm1;
     movd	ecx,mm0;
     add	[svar],ecx;
     mov	eax,edx;
     add	ebx,esi;
     add	edx,esi;
     dec	edi;
     jg		near .d1top01sse;
     jmp	near .d1exitsse;
 
.d1is11sse:
     mov	eax,[esp+off+4];
     mov	ebx,[esp+off+8];
     mov	edx,eax;
     add	edx,esi;
 
.d1top11sse:
     movq	mm0,[eax];
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     movq	mm2,[eax+0x1];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     movq	mm2,[edx];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     movq	mm4,[edx+0x1];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     pxor	mm6,mm6;
     pcmpeqw	mm5,mm5;
     psubw	mm6,mm5;
     paddw	mm6,mm6;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psrlw	mm0,0x2;
     psrlw	mm1,0x2;
     packuswb	mm0,mm1;
     psadbw	mm0,[ebx];
     movq	mm1,[eax+0x8];
     movq	mm2,mm1;
     punpcklbw	mm1,mm7;
     punpckhbw	mm2,mm7;
     movq	mm3,[eax+0x9];
     movq	mm4,mm3;
     punpcklbw	mm3,mm7;
     punpckhbw	mm4,mm7;
     paddw	mm1,mm3;
     paddw	mm2,mm4;
     movq	mm3,[edx+0x8];
     movq	mm4,mm3;
     punpcklbw	mm3,mm7;
     punpckhbw	mm4,mm7;
     movq	mm5,[edx+0x9];
     movq	mm6,mm5;
     punpcklbw	mm5,mm7;
     punpckhbw	mm6,mm7;
     paddw	mm3,mm5;
     paddw	mm4,mm6;
     paddw	mm1,mm3;
     paddw	mm2,mm4;
     pxor	mm6,mm6;
     pcmpeqw	mm5,mm5;
     psubw	mm6,mm5;
     paddw	mm6,mm6;
     paddw	mm1,mm6;
     paddw	mm2,mm6;
     psrlw	mm1,0x2;
     psrlw	mm2,0x2;
     packuswb	mm1,mm2;
     psadbw	mm1,[ebx+0x8];
     paddd	mm0,mm1;
     movd	ecx,mm0;
     add	[svar],ecx;
     mov	eax,edx;
     add	edx,esi;
     add	ebx,esi;
     dec	edi;
     jg		near .d1top11sse;
     jmp	near .d1exitsse;
 
.d1exit1sse:
     mov	edx, [svar];
 
.d1exitsse:
     emms
     mov	eax,edx;
     pop 	edi
     pop 	esi
     pop 	edx
     pop 	ecx
     pop 	ebx

     ret
 
;;-----------------------------------------------------------
;; int dist2mmx(unsigned char *blk1, unsigned char *blk2,
;;		int lx, int hx, int hy, int h)
;;-----------------------------------------------------------
cglobal dist2mmx

align 16
dist2mmx:
     push 	ebx
     push 	ecx
     push 	edx
     push 	esi
     push 	edi

     mov	dword [svar],0x0
     mov	edi,[esp+off+24];
     mov	edx,[esp+off+20];
     mov	eax,[esp+off+16];
     mov	esi,[esp+off+12];
     test	edi,edi;
     jle	near .d2exit;
     pxor	mm7,mm7;
     test	eax,eax;
     jne	.d2is10;
     test	edx,edx;
     jne	.d2is10;
     xor	edx,edx;
     mov	eax,[esp+off+4];
     mov	ebx,[esp+off+8];
 
.d2top00:
     movq	mm0,[eax];
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     movq	mm2,[ebx];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     psubw	mm0,mm2;
     psubw	mm1,mm3;
     pmaddwd	mm0,mm0;
     pmaddwd	mm1,mm1;
     paddd	mm0,mm1;
     movq	mm1,[eax+0x8];
     movq	mm2,mm1;
     punpcklbw	mm1,mm7;
     punpckhbw	mm2,mm7;
     movq	mm3,[ebx+0x8];
     movq	mm4,mm3;
     punpcklbw	mm3,mm7;
     punpckhbw	mm4,mm7;
     psubw	mm1,mm3;
     psubw	mm2,mm4;
     pmaddwd	mm1,mm1;
     pmaddwd	mm2,mm2;
     paddd	mm1,mm2;
     paddd	mm0,mm1;
     movd	ecx,mm0;
     add	edx,ecx;
     psrlq	mm0,0x20;
     movd	ecx,mm0;
     add	edx,ecx;
     add	eax,esi;
     add	ebx,esi;
     dec	edi;
     jg	  	.d2top00;
     jmp	.d2exit1;
 
.d2is10:
     test	eax,eax;
     je	  	near .d2is01;
     test	edx,edx;
     jne	near .d2is01;
     xor	edx,edx;
     mov	eax,[esp+off+4];
     mov	ebx,[esp+off+8];
     pxor	mm6,mm6;
     pcmpeqw	mm1,mm1;
     psubw	mm6,mm1;
 
.d2top10:
     movq	mm0,[eax];
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     movq	mm2,[eax+0x1];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psrlw	mm0,0x1;
     psrlw	mm1,0x1;
     movq	mm2,[ebx];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     psubw	mm0,mm2;
     psubw	mm1,mm3;
     pmaddwd	mm0,mm0;
     pmaddwd	mm1,mm1;
     paddd	mm0,mm1;
     movq	mm1,[eax+0x8];
     movq	mm2,mm1;
     punpcklbw	mm1,mm7;
     punpckhbw	mm2,mm7;
     movq	mm3,[eax+0x9];
     movq	mm4,mm3;
     punpcklbw	mm3,mm7;
     punpckhbw	mm4,mm7;
     paddw	mm1,mm3;
     paddw	mm2,mm4;
     paddw	mm1,mm6;
     paddw	mm2,mm6;
     psrlw	mm1,0x1;
     psrlw	mm2,0x1;
     movq	mm3,[ebx+0x8];
     movq	mm4,mm3;
     punpcklbw	mm3,mm7;
     punpckhbw	mm4,mm7;
     psubw	mm1,mm3;
     psubw	mm2,mm4;
     pmaddwd	mm1,mm1;
     pmaddwd	mm2,mm2;
     paddd	mm1,mm2;
     paddd	mm0,mm1;
     movd	ecx,mm0;
     add	edx,ecx;
     psrlq	mm0,0x20;
     movd	ecx,mm0;
     add	edx,ecx;
     add	eax,esi;
     add	ebx,esi;
     dec	edi;
     jg	        .d2top10;
     jmp	.d2exit1;
 
.d2is01:
     test	eax,eax;
     jne	near .d2is11;
     test	edx,edx;
     je	  	near .d2is11;
     mov	eax,[esp+off+4];
     mov	edx,[esp+off+8];
     mov	ebx,eax;
     add	ebx,esi;
     pxor	mm6,mm6;
     pcmpeqw	mm1,mm1;
     psubw	mm6,mm1;
 
.d2top01:
     movq	mm0,[eax];
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     movq	mm2,[ebx];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psrlw	mm0,0x1;
     psrlw	mm1,0x1;
     movq	mm2,[edx];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     psubw	mm0,mm2;
     psubw	mm1,mm3;
     pmaddwd	mm0,mm0;
     pmaddwd	mm1,mm1;
     paddd	mm0,mm1;
     movq	mm1,[eax+0x8];
     movq	mm2,mm1;
     punpcklbw	mm1,mm7;
     punpckhbw	mm2,mm7;
     movq	mm3,[ebx+0x8];
     movq	mm4,mm3;
     punpcklbw	mm3,mm7;
     punpckhbw	mm4,mm7;
     paddw	mm1,mm3;
     paddw	mm2,mm4;
     paddw	mm1,mm6;
     paddw	mm2,mm6;
     psrlw	mm1,0x1;
     psrlw	mm2,0x1;
     movq	mm3,[edx+0x8];
     movq	mm4,mm3;
     punpcklbw	mm3,mm7;
     punpckhbw	mm4,mm7;
     psubw	mm1,mm3;
     psubw	mm2,mm4;
     pmaddwd	mm1,mm1;
     pmaddwd	mm2,mm2;
     paddd	mm1,mm2;
     paddd	mm0,mm1;
     movd	ecx,mm0;
     add	[svar],ecx;
     psrlq	mm0,0x20;
     movd	ecx,mm0;
     add	[svar],ecx;
     mov	eax,ebx;
     add	edx,esi;
     add	ebx,esi;
     dec	edi;
     jg	        .d2top01;
     jmp	.d2exit;
 
.d2is11:
     mov	eax,[esp+off+4];
     mov	edx,[esp+off+8];
     mov	ebx,eax;
     add	ebx,esi;
 
.d2top11:
     movq	mm0,[eax];
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     movq	mm2,[eax+0x1];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     movq	mm2,[ebx];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     movq	mm4,[ebx+0x1];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     pxor	mm6,mm6;
     pcmpeqw	mm5,mm5;
     psubw	mm6,mm5;
     paddw	mm6,mm6;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psrlw	mm0,0x2;
     psrlw	mm1,0x2;
     movq	mm2,[edx];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     psubw	mm0,mm2;
     psubw	mm1,mm3;
     pmaddwd	mm0,mm0;
     pmaddwd	mm1,mm1;
     paddd	mm0,mm1;
     movq	mm1,[eax+0x8];
     movq	mm2,mm1;
     punpcklbw	mm1,mm7;
     punpckhbw	mm2,mm7;
     movq	mm3,[eax+0x9];
     movq	mm4,mm3;
     punpcklbw	mm3,mm7;
     punpckhbw	mm4,mm7;
     paddw	mm1,mm3;
     paddw	mm2,mm4;
     movq	mm3,[ebx+0x8];
     movq	mm4,mm3;
     punpcklbw	mm3,mm7;
     punpckhbw	mm4,mm7;
     movq	mm5,[ebx+0x9];
     movq	mm6,mm5;
     punpcklbw	mm5,mm7;
     punpckhbw	mm6,mm7;
     paddw	mm3,mm5;
     paddw	mm4,mm6;
     paddw	mm1,mm3;
     paddw	mm2,mm4;
     pxor	mm6,mm6;
     pcmpeqw	mm5,mm5;
     psubw	mm6,mm5;
     paddw	mm6,mm6;
     paddw	mm1,mm6;
     paddw	mm2,mm6;
     psrlw	mm1,0x2;
     psrlw	mm2,0x2;
     movq	mm3,[edx+0x8];
     movq	mm4,mm3;
     punpcklbw	mm3,mm7;
     punpckhbw	mm4,mm7;
     psubw	mm1,mm3;
     psubw	mm2,mm4;
     pmaddwd	mm1,mm1;
     pmaddwd	mm2,mm2;
     paddd	mm1,mm2;
     paddd	mm0,mm1;
     movd	ecx,mm0;
     add	[svar],ecx;  
     psrlq	mm0,0x20; 
     movd	ecx,mm0; 
     add	[svar],ecx;  
     mov	eax,ebx;
     add	ebx,esi;
     add	edx,esi;
     dec	edi;
     jg	        .d2top11;
     jmp	.d2exit;
 
.d2exit1:
     mov	[svar],edx;
 
.d2exit:
     emms
     mov	eax,edx;
     lea	esi,[esi];

     pop 	edi
     pop	esi
     pop 	edx
     pop 	ecx
     pop 	ebx

     ret
 
;;-----------------------------------------------------------
;; int bdist1mmx(unsigned char *pf, unsigned char *pb, 
;;		 unsigned char *p2, int lx, 
;;		 int hxf, int hyf, int hxb, int hyb, int h)
;;-----------------------------------------------------------
cglobal bdist1mmx

align 16
bdist1mmx:
     push       ebx
     push 	ecx
     push 	edx
     push 	esi
     push 	edi

     mov	dword [svar],0x0
     mov	edx,[esp+off+28];
     mov	eax,[esp+off+20];
     mov	esi,[esp+off+16];
     mov	ecx,[esp+off+4];

     add	ecx,eax;
     mov	[pfa],ecx;
     mov	ecx,esi;
     imul	ecx,[esp+off+24];
     mov	ebx,[esp+off+4];
     add	ecx,ebx;
     mov	[pfb],ecx;
     add	eax,ecx;
     mov	[pfc],eax;
     mov	eax,[esp+off+8];
     add	eax,edx;
     mov	[pba],eax;
     mov	eax,esi;
     imul	eax,[esp+off+32];
     mov	ecx,[esp+off+8];
     add	eax,ecx;
     mov	[pbb],eax;
     add	edx,eax;
     mov	[pbc],edx;
     xor	esi,esi;
     mov	[svar],esi;
     mov	edi,[esp+off+36];
     test	edi,edi;
     jle	near .bdist1exit;
     pxor	mm7,mm7;
     pxor	mm6,mm6;
     pcmpeqw	mm5,mm5;
     psubw	mm6,mm5;
     psllw	mm6,0x1;
 
.bdist1top:
     mov	eax,[esp+off+4];
     mov	ebx,[pfa];
     mov	ecx,[pfb];
     mov	edx,[pfc];
     movq	mm0,[eax];
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     movq	mm2,[ebx];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     movq	mm2,[ecx];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     movq	mm2,[edx];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psrlw	mm0,0x2;
     psrlw	mm1,0x2;
     mov	eax,[esp+off+8];
     mov	ebx,[pba];
     mov	ecx,[pbb];
     mov	edx,[pbc];
     movq	mm2,[eax];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     movq	mm4,[ebx];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     movq	mm4,[ecx];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     movq	mm4,[edx];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     paddw	mm2,mm6;
     paddw	mm3,mm6;
     psrlw	mm2,0x2;
     psrlw	mm3,0x2;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     psrlw	mm6,0x1;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psllw	mm6,0x1;
     psrlw	mm0,0x1;
     psrlw	mm1,0x1;
     packuswb	mm0,mm1;
     mov	eax,[esp+off+12];
     movq	mm1,[eax];
     movq	mm2,mm0;
     psubusb	mm0,mm1;
     psubusb	mm1,mm2;
     por	mm0,mm1;
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     paddw	mm0,mm1;
     movq	mm1,mm0;
     punpcklwd	mm0,mm7;
     punpckhwd	mm1,mm7;
     paddd	mm0,mm1;
     movd	eax,mm0;
     psrlq	mm0,0x20;
     movd	ebx,mm0;
     add	esi,eax;
     add	esi,ebx;
     mov	eax,[esp+off+4];
     mov	ebx,[pfa];
     mov	ecx,[pfb];
     mov	edx,[pfc];
     movq	mm0,[eax+0x8];
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     movq	mm2,[ebx+0x8];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     movq	mm2,[ecx+0x8];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     movq	mm2,[edx+0x8];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psrlw	mm0,0x2;
     psrlw	mm1,0x2;
     mov	eax,[esp+off+8];
     mov	ebx,[pba];
     mov	ecx,[pbb];
     mov	edx,[pbc];
     movq	mm2,[eax+0x8];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     movq	mm4,[ebx+0x8];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     movq	mm4,[ecx+0x8];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     movq	mm4,[edx+0x8];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     paddw	mm2,mm6;
     paddw	mm3,mm6;
     psrlw	mm2,0x2;
     psrlw	mm3,0x2;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     psrlw	mm6,0x1;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psllw	mm6,0x1;
     psrlw	mm0,0x1;
     psrlw	mm1,0x1;
     packuswb	mm0,mm1;
     mov	eax,[ebp+off+12];
     movq	mm1,[eax+0x8];
     movq	mm2,mm0;
     psubusb	mm0,mm1;
     psubusb	mm1,mm2;
     por	mm0,mm1;
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     paddw	mm0,mm1;
     movq	mm1,mm0;
     punpcklwd	mm0,mm7;
     punpckhwd	mm1,mm7;
     paddd	mm0,mm1;
     movd	eax,mm0;
     psrlq	mm0,0x20;
     movd	ebx,mm0;
     add	esi,eax;
     add	esi,ebx;
     mov	eax,[esp+off+16];
     add	[esp+off+12],eax;
     add	[esp+off+4],eax;
     add	[pfa],eax;
     add	[pfb],eax;
     add	[pfc],eax;
     add	[esp+off+8],eax;
     add	[pba],eax;
     add	[pbb],eax;
     add	[pbc],eax;
     dec	edi;
     jg		near .bdist1top;
     mov	[svar],esi;
 
.bdist1exit:
     emms
     mov	edx,[svar];
     mov	eax,edx;
     lea	esi,[esi];

     pop 	edi
     pop 	esi
     pop 	edx
     pop 	ecx
     pop 	ebx

     ret
 
;;-----------------------------------------------------------
;; int bdist1sse(unsigned char *pf, unsigned char *pb, 
;;		 unsigned char *p2, int lx, 
;;		 int hxf, int hyf, int hxb, int hyb, int h)
;;-----------------------------------------------------------
cglobal bdist1sse

align 16
bdist1sse:
     push       ebx
     push 	ecx
     push 	edx
     push 	esi
     push 	edi

     mov	dword [svar],0x0
     mov	edx,[esp+off+28];
     mov	eax,[esp+off+20];
     mov	esi,[esp+off+16];
     mov	ecx,[esp+off+4];

     add	ecx,eax;
     mov	[pfa],ecx;
     mov	ecx,esi;
     imul	ecx,[esp+off+24];
     mov	ebx,[esp+off+4];
     add	ecx,ebx;
     mov	[pfb],ecx;
     add	eax,ecx;
     mov	[pfc],eax;
     mov	eax,[esp+off+8];
     add	eax,edx;
     mov	[pba],eax;
     mov	eax,esi;
     imul	eax,[esp+off+32];
     mov	ecx,[esp+off+8];
     add	eax,ecx;
     mov	[pbb],eax;
     add	edx,eax;
     mov	[pbc],edx;
     xor	esi,esi;
     mov	[svar],esi;
     mov	edi,[esp+off+36];
     test	edi,edi;
     jle	near .bd1exitsse;
     pxor	mm7,mm7;
     pxor	mm6,mm6;
     pcmpeqw	mm5,mm5;
     psubw	mm6,mm5;
     psllw	mm6,0x1;

.bd1topsse:
     mov	eax,[esp+off+4];
     mov	ebx,[pfa];
     mov	ecx,[pfb];
     mov	edx,[pfc];
     movq	mm0,[eax];
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     movq	mm2,[ebx];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     movq	mm2,[ecx];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     movq	mm2,[edx];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psrlw	mm0,0x2;
     psrlw	mm1,0x2;
     mov	eax,[esp+off+8];
     mov	ebx,[pba];
     mov	ecx,[pbb];
     mov	edx,[pbc];
     movq	mm2,[eax];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     movq	mm4,[ebx];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     movq	mm4,[ecx];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     movq	mm4,[edx];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     paddw	mm2,mm6;
     paddw	mm3,mm6;
     psrlw	mm2,0x2;
     psrlw	mm3,0x2;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     psrlw	mm6,0x1;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psllw	mm6,0x1;
     psrlw	mm0,0x1;
     psrlw	mm1,0x1;
     packuswb	mm0,mm1;
     mov	ebx,[esp+off+12];
     psadbw	mm0,[ebx];
     movd	eax,mm0;
     add	esi,eax;
     mov	eax,[esp+off+4];
     mov	ebx,[pfa];
     mov	ecx,[pfb];
     mov	edx,[pfc];
     movq	mm0,[eax+0x8];
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     movq	mm2,[ebx+0x8];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     movq	mm2,[ecx+0x8];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     movq	mm2,[edx+0x8];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psrlw	mm0,0x2;
     psrlw	mm1,0x2;
     mov	eax,[esp+off+8];
     mov	ebx,[pba];
     mov	ecx,[pbb];
     mov	edx,[pbc];
     movq	mm2,[eax+0x8];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     movq	mm4,[ebx+0x8];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     movq	mm4,[ecx+0x8];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     movq	mm4,[edx+0x8];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     paddw	mm2,mm6;
     paddw	mm3,mm6;
     psrlw	mm2,0x2;
     psrlw	mm3,0x2;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     psrlw	mm6,0x1;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psllw	mm6,0x1;
     psrlw	mm0,0x1;
     psrlw	mm1,0x1;
     packuswb	mm0,mm1;
     mov	ebx,[esp+off+12];
     psadbw	mm0,[ebx+0x8];
     movd	eax,mm0;
     add	esi,eax;
     mov	eax,[esp+off+16];
     add	[esp+off+12],eax;
     add	[esp+off+4],eax;
     add	[pfa],eax;
     add	[pfb],eax;
     add	[pfc],eax;
     add	[esp+off+8],eax;
     add	[pba],eax;
     add	[pbb],eax;
     add	[pbc],eax;
     dec	edi;
     jg		near .bd1topsse;
     mov	[svar],esi;
 
.bd1exitsse:
     emms
     mov	edx,[svar];
     mov	eax,edx;
     lea	esi,[esi];

     pop 	edi
     pop 	esi
     pop 	edx
     pop 	ecx
     pop 	ebx

     ret
 
;;-----------------------------------------------------------
;;int bdist2mmx(unsigned char *pf, unsigned char *pb, 
;;		unsigned char *p2, int lx, 
;;		int hxf, int hyf, int hxb, int hyb, int h)
;;-----------------------------------------------------------
cglobal bdist2mmx

align 16
bdist2mmx:
     push       ebx
     push 	ecx
     push 	edx
     push 	esi
     push 	edi

     mov	dword [svar],0x0
     mov	edx,[esp+off+28];
     mov	eax,[esp+off+20];
     mov	esi,[esp+off+16];
     mov	ecx,[esp+off+4];

     add	ecx,eax;
     mov	[pfa],ecx;
     mov	ecx,esi;
     imul	ecx,[esp+off+24];
     mov	ebx,[esp+off+4];
     add	ecx,ebx;
     mov	[pfb],ecx;
     add	eax,ecx;
     mov	[pfc],eax;
     mov	eax,[esp+off+8];
     add	eax,edx;
     mov	[pba],eax;
     mov	eax,esi;
     imul	eax,[esp+off+32];
     mov	ecx,[esp+off+8];
     add	eax,ecx;
     mov	[pbb],eax;
     add	edx,eax;
     mov	[pbc],edx;
     xor	esi,esi;
     mov	[svar],esi;
     mov	edi,[esp+off+36];
     test	edi,edi;
     jle	near .bdist2exit;
     pxor	mm7,mm7;
     pxor	mm6,mm6;
     pcmpeqw	mm5,mm5;
     psubw	mm6,mm5;
     psllw	mm6,0x1;
 
.bdist2top:
     mov	eax,[esp+off+4];
     mov	ebx,[pfa];
     mov	ecx,[pfb];
     mov	edx,[pfc];
     movq	mm0,[eax];
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     movq	mm2,[ebx];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     movq	mm2,[ecx];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     movq	mm2,[edx];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psrlw	mm0,0x2;
     psrlw	mm1,0x2;
     mov	eax,[esp+off+8];
     mov	ebx,[pba];
     mov	ecx,[pbb];
     mov	edx,[pbc];
     movq	mm2,[eax];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     movq	mm4,[ebx];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     movq	mm4,[ecx];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     movq	mm4,[edx];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     paddw	mm2,mm6;
     paddw	mm3,mm6;
     psrlw	mm2,0x2;
     psrlw	mm3,0x2;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     psrlw	mm6,0x1;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psllw	mm6,0x1;
     psrlw	mm0,0x1;
     psrlw	mm1,0x1;
     mov	eax,[esp+off+12];
     movq	mm2,[eax];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     psubw	mm0,mm2;
     psubw	mm1,mm3;
     pmaddwd	mm0,mm0;
     pmaddwd	mm1,mm1;
     paddd	mm0,mm1;
     movd	eax,mm0;
     psrlq	mm0,0x20;
     movd	ebx,mm0;
     add	esi,eax;
     add	esi,ebx;
     mov	eax,[esp+off+4];
     mov	ebx,[pfa];
     mov	ecx,[pfb];
     mov	edx,[pfc];
     movq	mm0,[eax+0x8];
     movq	mm1,mm0;
     punpcklbw	mm0,mm7;
     punpckhbw	mm1,mm7;
     movq	mm2,[ebx+0x8];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     movq	mm2,[ecx+0x8];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     movq	mm2,[edx+0x8];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psrlw	mm0,0x2;
     psrlw	mm1,0x2;
     mov	eax,[esp+off+8];
     mov	ebx,[pba];
     mov	ecx,[pbb];
     mov	edx,[pbc];
     movq	mm2,[eax+0x8];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     movq	mm4,[ebx+0x8];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     movq	mm4,[ecx+0x8];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     movq	mm4,[edx+0x8];
     movq	mm5,mm4;
     punpcklbw	mm4,mm7;
     punpckhbw	mm5,mm7;
     paddw	mm2,mm4;
     paddw	mm3,mm5;
     paddw	mm2,mm6;
     paddw	mm3,mm6;
     psrlw	mm2,0x2;
     psrlw	mm3,0x2;
     paddw	mm0,mm2;
     paddw	mm1,mm3;
     psrlw	mm6,0x1;
     paddw	mm0,mm6;
     paddw	mm1,mm6;
     psllw	mm6,0x1;
     psrlw	mm0,0x1;
     psrlw	mm1,0x1;
     mov	eax,[esp+off+12];
     movq	mm2,[eax+0x8];
     movq	mm3,mm2;
     punpcklbw	mm2,mm7;
     punpckhbw	mm3,mm7;
     psubw	mm0,mm2;
     psubw	mm1,mm3;
     pmaddwd	mm0,mm0;
     pmaddwd	mm1,mm1;
     paddd	mm0,mm1;
     movd	eax,mm0;
     psrlq	mm0,0x20;
     movd	ebx,mm0;
     add	esi,eax;
     add	esi,ebx;
     mov	eax,[esp+off+16];
     add	[esp+off+12],eax;
     add	[esp+off+4],eax;
     add	[pfa],eax;
     add	[pfb],eax;
     add	[pfc],eax;
     add	[esp+off+8],eax;
     add	[pba],eax;
     add	[pbb],eax;
     add	[pbc],eax;
     dec	edi;
     jg		near .bdist2top;
     mov	[svar],esi;
 
.bdist2exit:
     emms
     mov	edx,[svar];
     mov	eax,edx;
     lea	esi,[esi];

     pop 	edi
     pop 	esi
     pop 	edx
     pop 	ecx
     pop 	ebx

     ret

 
;;-----------------------------------------------------------
;; int variancemmx(unsigned char *p,int lx)
;;-----------------------------------------------------------
cglobal variancemmx

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

align 16
variancemmx:
	push 	edi
	push	ebx
	push 	ecx
	
	mov 	eax, [esp + ofx + 4]	; p 
	mov 	edi, [esp + ofx + 8]	; lx 
	
	xor 	ebx, ebx		; zero
	xor 	ecx, ecx		; zero

	pxor 	mm7, mm7		; zero

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

	imul 	ecx, ecx		; mean value squared
	shr 	ecx, $8		        ; /256
	sub 	ebx, ecx		; variance
				
	mov 	eax, ebx		; exit

	pop 	ecx
	pop 	ebx
	pop 	edi
	
	ret

;;-----------------------------------------------------------
;; int edist1mmx(unsigned char *blk1, unsigned char *blk2,
;;		 int lx, int distlim)
;;-----------------------------------------------------------
cglobal edist1mmx

align 16
edist1mmx:
     push       ebx
     push 	ecx
     push 	edx
     push 	esi
     push 	edi
     
     mov	dword [svar],0x0;
     mov	edi,[esp+off+16];
     mov	esi,[esp+off+12];
     pxor	mm7,mm7;
     xor	edx,edx;
     mov	eax,[esp+off+4];
     mov	ebx,[esp+off+8];
     movd	mm0,[eax];
     movd	mm1,[ebx];
     movq	mm2,mm0;
     psubusb	mm0,mm1;
     psubusb	mm1,mm2;
     por	mm0,mm1;
     punpcklbw	mm0,mm7;
     movq	mm1,mm0;
     punpcklwd	mm0,mm7;
     punpckhwd	mm1,mm7;
     paddd	mm0,mm1;
     movd	ecx,mm0;
     add	edx,ecx;
     psrlq	mm0,0x20;
     movd	ecx,mm0;
     add	edx,ecx;
     cmp	edx,edi;
     jge	near .e1exit;
     add	eax,esi;
     add	ebx,esi;
     movd	mm0,[eax];
     movd	mm1,[ebx];
     movq	mm2,mm0;
     psubusb	mm0,mm1;
     psubusb	mm1,mm2;
     por	mm0,mm1;
     punpcklbw	mm0,mm7;
     movq	mm1,mm0;
     punpcklwd	mm0,mm7;
     punpckhwd	mm1,mm7;
     paddd	mm0,mm1;
     movd	ecx,mm0;
     add	edx,ecx;
     psrlq	mm0,0x20;
     movd	ecx,mm0;
     add	edx,ecx;
     cmp	edx,edi;
     jge	near .e1exit;
     add	eax,esi;
     add	ebx,esi;
     movd	mm0,[eax];
     movd	mm1,[ebx];
     movq	mm2,mm0;
     psubusb	mm0,mm1;
     psubusb	mm1,mm2;
     por	mm0,mm1;
     punpcklbw	mm0,mm7;
     movq	mm1,mm0;
     punpcklwd	mm0,mm7;
     punpckhwd	mm1,mm7;
     paddd	mm0,mm1;
     movd	ecx,mm0;
     add	edx,ecx;
     psrlq	mm0,0x20;
     movd	ecx,mm0;
     add	edx,ecx;
     cmp	edx,edi;
     jge	near .e1exit;
     add	eax,esi;
     add	ebx,esi;
     movd	mm0,[eax];
     movd	mm1,[ebx];
     movq	mm2,mm0;
     psubusb	mm0,mm1;
     psubusb	mm1,mm2;
     por	mm0,mm1;
     punpcklbw	mm0,mm7;
     movq	mm1,mm0;
     punpcklwd	mm0,mm7;
     punpckhwd	mm1,mm7;
     paddd	mm0,mm1;
     movd	ecx,mm0;
     add	edx,ecx;
     psrlq	mm0,0x20;
     movd	ecx,mm0;
     add	edx,ecx;
 
.e1exit:
     mov	edx, [svar];
     emms
     mov	eax,edx;

     pop 	edi
     pop 	esi
     pop 	edx
     pop 	ecx
     pop 	ebx

     ret
 
;;-----------------------------------------------------------
;; int edist1sse(unsigned char *blk1, unsigned char *blk2,
;;		 int lx, int distlim)
;;-----------------------------------------------------------
cglobal edist1sse

align 16
edist1sse:
     push       ebx
     push 	ecx
     push 	edx
     push 	esi
     push 	edi
     
     mov	dword [svar],0x0;
     mov	edi,[esp+off+16];
     mov	esi,[esp+off+12];
     pxor	mm7,mm7;
     xor	edx,edx;
     mov	eax,[esp+off+4];
     mov	ebx,[esp+off+8];
     movd	mm0,[eax];
     movd	mm1,[ebx];
     psadbw	mm0,mm1;
     movd	ecx,mm0;
     add	edx,ecx;
     cmp	edx,edi;
     jge	near .e1exitsse;
     add	eax,esi;
     add	ebx,esi;
     movd	mm0,[eax];
     movd	mm1,[ebx];
     psadbw	mm0,mm1;
     movd	ecx,mm0;
     add	edx,ecx;
     cmp	edx,edi;
     jge	near .e1exitsse;
     add	eax,esi;
     add	ebx,esi;
     movd	mm0,[eax];
     movd	mm1,[ebx];
     psadbw	mm0,mm1;
     movd	ecx,mm0;
     add	edx,ecx;
     cmp	edx,edi;
     jge	near .e1exitsse;
     add	eax,esi;
     add	ebx,esi;
     movd	mm0,[eax];
     movd	mm1,[ebx];
     psadbw	mm0,mm1;
     movd	ecx,mm0;
     add	edx,ecx;
 
.e1exitsse:
     mov	edx, [svar];
     emms
     mov	eax,edx;

     pop 	edi
     pop 	esi
     pop 	edx
     pop 	ecx
     pop 	ebx

     ret

;;=============================
;;== routines for transfrm.c ==
;;=============================
 
;;-----------------------------------------------------------
;; void sub_pred_mmx(unsigned char *pred, unsigned char *cur,
;;		     int lx, short *blk)
;;-----------------------------------------------------------
cglobal sub_pred_mmx

align 16
sub_pred_mmx:
     push       ebx
     push 	ecx
     push 	edx
     push 	esi
     push 	edi

     mov	eax,[esp+off+8];
     mov	ebx,[esp+off+4];
     mov	ecx,[esp+off+16];
     mov	edi,[esp+off+12];
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
     jg		near .sub_top;
     
     emms

     pop 	edi
     pop 	esi
     pop 	edx
     pop 	ecx
     pop 	ebx

     ret
 
;;-----------------------------------------------------------
;; void add_pred_mmx(unsigned char *pred, unsigned char *cur,
;;		     int lx, short *blk)
;;-----------------------------------------------------------
cglobal add_pred_mmx

align 16
add_pred_mmx:

     push       ebx
     push 	ecx
     push 	edx
     push 	esi
     push 	edi

     mov	eax,[esp+off+8];
     mov	ebx,[esp+off+4];
     mov	ecx,[esp+off+16];
     mov	edi,[esp+off+12];
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
     jg		near .add_top;
   
     emms

     pop 	edi
     pop 	esi
     pop 	edx
     pop 	ecx
     pop 	ebx

     ret
