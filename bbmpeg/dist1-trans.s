;/*
; * total absolute difference between two (16*h) blocks
; * including optional half pel interpolation of blk1 (hx,hy)
; * blk1,blk2: addresses of top left pels of both blocks
; * lx:        distance (in bytes) of vertically adjacent pels
; * hx,hy:     flags for horizontal and/or vertical interpolation
; * h:         height of block (usually 8 or 16)
; * distlim:   bail out if sum exceeds this value
; */

;/* MMX version */

; int dist1mmx(unsigned char *blk1, unsigned char *blk2,
	       int lx, int hx, int hy, int h, int distlim)

;  //  int s = 0;

;  //mov %0, %%edi" : : "m" (h) );
;  //mov %0, %%edx" : : "m" (hy) );
;  //mov %0, %%eax" : : "m" (hx) );
;  //mov %0, %%esi" : : "m" (lx) );

.  test edi, edi
.  jle d1exit

.  pxor mm7, mm7

.  test eax, eax
.  jne d1is10
.  test edx, edx
.  jne d1is10

.  xor edx, edx

;//mov %0, %%eax" : : "m" (blk1) );
;//mov %0, %%ebx" : : "m" (blk2) );

.  d1top00:
.  movq mm0, [eax]
.  movq mm1, [ebx]
.  movq mm2, mm0
.  psubusb mm0, mm1
.  psubusb mm1, mm2
.  por mm0, mm1
.  movq mm2, [eax+8] ;//8(%eax), %mm2
.  movq mm3, [ebx+8] ;//8(%ebx), %mm3
.  movq mm4, mm2
.  psubusb mm2, mm3
.  psubusb mm3, mm4
.  por mm2, mm3
.  movq mm1, mm0
.  punpcklbw mm0, mm7
.  punpckhbw mm1, mm7
.  movq mm3, mm2
.  punpcklbw mm2, mm7
.  punpckhbw mm3, mm7
.  paddw mm0, mm1
.  paddw mm0, mm2
.  paddw mm0, mm3
.  movq mm1, mm0
.  punpcklwd mm0, mm7
.  punpckhwd mm1, mm7
.  paddd %mm0, %mm1
.  movd ecx, mm0 ;%mm0, %ecx
.  add %edx, %ecx
.  psrlq mm0, $32 ;//$32, %mm0
.  movd ecx, mm0 ;//%mm0, %ecx
.  add %edx, %ecx

;    //  cmp %0, %%edx" : : "m" (distlim) );

.  jge d1exit1
.  add eax, esi  ;//%esi, %eax
.  add ebx, esi  ;//%esi, %ebx
.  dec edi
.  jg d1top00
.  jmp d1exit1

.  d1is10:
.  test %eax, %eax
.  je d1is01
.  test %edx, %edx
.  jne d1is01

.  xor %edx, %edx

;    //mov %0, %%eax" : : "m" (blk1) );
;    //mov %0, %%ebx" : : "m" (blk2) );

.  pxor %mm6, %mm6
.  pcmpeqw %mm1, %mm1
.  psubw %mm6, %mm1

.  d1top10:
.  movq mm0, [eax] ;//(%eax ), %mm0
.  movq %mm1, %mm0
.  punpcklbw %mm0, %mm7
.  punpckhbw %mm1, %mm7
.  movq mm2, [eax+1] ;//1(%eax), %mm2
.  movq %mm3, %mm2
.  punpcklbw %mm2, %mm7
.  punpckhbw %mm3, %mm7
.  paddw mm0, mm2 ;//%mm2, %mm0
.  paddw mm1, mm3 ;//%mm3, %mm1
.  paddw mm0, mm6 ;//%mm6, %mm0
.  paddw mm1, mm6 ;//%mm6, %mm1
.  psrlw mm0, $1 ;//$1, %mm0
.  psrlw mm1, $1 ;//$1, %mm1
.  packuswb %mm0, %mm1
.  movq mm1, [ebx] ;//(%ebx ), %mm1
.  movq mm2, mm0 ;//%mm0, %mm2
.  psubusb mm0, mm1 ;//%mm1, %mm0
.  psubusb mm1, mm2 ;//%mm2, %mm1
.  por mm0, mm1 ;//%mm1, %mm0
.  movq mm1, [eax+8] ;//8(%eax), %mm1
.  movq mm2, mm1 ;//%mm1, %mm2
.  punpcklbw mm1, mm7 ;//%mm7, %mm1
.  punpckhbw mm2, mm7 ;//%mm7, %mm2
.  movq mm3, [eax+9] ;//9(%eax), %mm3
.  movq mm4, mm3 ;//%mm3, %mm4
.  punpcklbw mm3, mm7 ;//%mm7, %mm3
.  punpckhbw mm4, mm7 ;//%mm7, %mm4
.  paddw mm1, mm3 ;//%mm3, %mm1
.  paddw mm2, mm4 ;//%mm4, %mm2
.  paddw mm1, mm6 ;//%mm6, %mm1
.  paddw mm2, mm6 ;//%mm6, %mm2
.  psrlw mm1, $1 ;//$1, %mm1
.  psrlw mm2, $1 ;//$1, %mm2
.  packuswb mm1, mm2 ;//%mm2, %mm1
.  movq mm2, [ebx+8] ;//8(%ebx), %mm2
.  movq mm3, mm1 ;//%mm1, %mm3
.  psubusb mm1, mm2 ;//%mm2, %mm1
.  psubusb mm2, mm3 ;//%mm3, %mm2
.  por mm1, mm2 ;//%mm2, %mm1
.  movq mm2, mm0 ;//%mm0, %mm2
.  punpcklbw mm0, mm7 ;//%mm7, %mm0
.  punpckhbw mm2, mm7 ;//%mm7, %mm2
.  movq mm3, mm1 ;//%mm1, %mm3
.  punpcklbw mm1, mm7 ;//%mm7, %mm1
.  punpckhbw mm3, mm7 ;//%mm7, %mm3
.  paddw mm0, mm1 ;//%mm1, %mm0
.  paddw mm0, mm2 ;//%mm2, %mm0
.  paddw mm0, mm3 ;//%mm3, %mm0
.  movq mm1, mm0 ;//%mm0, %mm1
.  punpcklwd mm0, mm7 ;//%mm7, %mm0
.  punpckhwd mm1, mm7 ;//%mm7, %mm1
.  paddd mm0, mm1 ;//%mm1, %mm0
.  movd ecx, mm0 ;//%mm0, %ecx
.  add edx, ecx ;//%ecx, %edx 
.  psrlq mm0, $32 ;//$32, %mm0
.  movd ecx, mm0 ;//%mm0, %ecx
.  add edx, ecx ;//%ecx, %edx

.  add eax, esi ;//%esi, %eax
.  add ebx, esi ;//%esi, %ebx
.  dec edi
.  jg d1top10
.  jmp d1exit1

.  d1is01:
.  test %eax, %eax
.  jne d1is11
.  test %edx, %edx
.  je d1is11

;    //  mov %0, %%eax" : : "m" (blk1) );
;    //mov %0, %%edx" : : "m" (blk2) );

.  mov ebx, eax ;//%eax, %ebx
.  add ebx, esi ;//%esi, %ebx

.  pxor %mm6, %mm6
.  pcmpeqw %mm1, %mm1
.  psubw mm6, mm1 ;//%mm1, %mm6

.  d1top01:
.  movq mm0, [eax] ;//(%eax ), %mm0
.  movq mm1, mm0 ;//%mm0, %mm1
.  punpcklbw mm0, mm7 ;//%mm7, %mm0
.  punpckhbw mm1, mm7 ;//%mm7, %mm1
.  movq mm2, [ebx] ;//(%ebx ), %mm2
.  movq mm3, mm2 ;//%mm2, %mm3
.  punpcklbw mm2, mm7 ;//%mm7, %mm2
.  punpckhbw mm3, mm7 ;//%mm7, %mm3
.  paddw mm0, mm2 ;//%mm2, %mm0
.  paddw mm1, mm3 ;//%mm3, %mm1
.  paddw mm0, mm6 ;//%mm6, %mm0
.  paddw mm1, mm6 ;//%mm6, %mm1
.  psrlw mm0, $1 ;//$1, %mm0
.  psrlw mm1, $1 ;//$1, %mm1
.  packuswb mm0, mm1 ;//%mm1, %mm0
.  movq mm1, [edx] ;//(%edx ), %mm1
.  movq mm2, mm0 ;//%mm0, %mm2
.  psubusb mm0, mm1 ;//%mm1, %mm0
.  psubusb mm1, mm2 ;//%mm2, %mm1
.  por mm0, mm1 ;//%mm1, %mm0
.  movq mm1, [eax+8] ;//8(%eax), %mm1
.  movq mm2, mm1 ;//%mm1, %mm2
.  punpcklbw mm2, mm7 ;//%mm7, %mm1
.  punpckhbw mm2, mm7 ;//%mm7, %mm2
.  movq mm3, [ebx+8] ;//8(%ebx), %mm3
.  movq mm4, mm3 ;//%mm3, %mm4
.  punpcklbw mm3, mm7 ;//%mm7, %mm3
.  punpckhbw mm4, mm7 ;//%mm7, %mm4
.  paddw mm1, mm3 ;//%mm3, %mm1
.  paddw mm2, mm4 ;//%mm4, %mm2
.  paddw mm1, mm6 ;//%mm6, %mm1
.  paddw mm2, mm6 ;//%mm6, %mm2
.  psrlw mm1, $1 ;//$1, %mm1
.  psrlw mm2, $1 ;//$1, %mm2
.  packuswb mm1, mm2 ;//%mm2, %mm1
.  movq mm2, [ edx+8] ;//8(%edx), %mm2
.  movq mm3, mm1 ;//%mm1, %mm3
.  psubusb mm1, mm2 ;//%mm2, %mm1
.  psubusb mm2, mm3 ;//%mm3, %mm2
.  por mm1, mm2 ;//%mm2, %mm1
.  movq mm0, mm2 ;//%mm0, %mm2
.  punpcklbw mm0, mm7 ;//%mm7, %mm0
.  punpckhbw mm2, mm7 ;//%mm7, %mm2
.  movq mm3, mm1 ;//%mm1, %mm3
.  punpcklbw mm1, mm7 ;//%mm7, %mm1
.  punpckhbw mm3, mm7 ;//%mm7, %mm3
.  paddw mm0, mm1 ;//%mm1, %mm0
.  paddw mm0, mm2 ;//%mm2, %mm0
.  paddw mm0, mm3 ;//%mm3, %mm0
.  movq mm1, mm0 ;//%mm0, %mm1
.  punpcklwd mm0, mm7 ;//%mm7, %mm0
.  punpckhwd mm1, mm7 ;//%mm7, %mm1
.  paddd mm0, mm1 ;//%mm1, %mm0
.  movd ecx, mm0 ;//%mm0, %ecx

;//  add %%ecx, %0" : "=m" (s) : );

.  psrlq mm0, $32 ;//$32, %mm0
.  movd ecx, mm0 ;//%mm0, %ecx

;//add %%ecx, %0" : "=m" (s) : );

.  mov eax, ebx ;//%ebx, %eax
.  add edx, esi ;//%esi, %edx
.  add ebx, esi ;//%esi, %ebx
.  dec edi
.  jg d1top01
.  jmp d1exit

.  d1is11:

;//mov %0, %%eax" : : "m" (blk1) );
;//mov %0, %%edx" : : "m" (blk2) );

.  mov ebx, eax ;//%eax, %ebx
.  add ebx, esi ;//%esi, %ebx

.  d1top11:
.  movq mm0, [eax]; //(%eax ), %mm0
.  movq mm1, mm0 ;//%mm0, %mm1
.  punpcklbw mm0, mm7 ;//%mm7, %mm0
.  punpckhbw mm1, mm7 ;//%mm7, %mm1
.  movq mm2, [eax+1] ;//1(%eax), %mm2
.  movq mm3, mm2 ;//%mm2, %mm3
.  punpcklbw mm2, mm7 ;//%mm7, %mm2
.  punpckhbw mm3, mm7 ;//%mm7, %mm3
.  paddw mm0, mm2 ;//%mm2, %mm0
.  paddw mm1, mm3 ;//%mm3, %mm1
.  movq mm2, [ebx]; //(%ebx ), %mm2
.  movq mm3, mm2 ;//%mm2, %mm3
.  punpcklbw mm2, mm7 ;//%mm7, %mm2
.  punpckhbw mm3, mm7 ;//%mm7, %mm3
.  movq mm4, [ebx+1] ;//1(%ebx), %mm4
.  movq mm5, mm4 ;//%mm4, %mm5
.  punpcklbw mm4, mm7 ;//%mm7, %mm4
.  punpckhbw mm5, mm7 ;//%mm7, %mm5
.  paddw mm2, mm4 ;//%mm4, %mm2
.  paddw mm3, mm5 ;//%mm5, %mm3
.  paddw mm0, mm2 ;//%mm2, %mm0
.  paddw mm1, mm3 ;//%mm3, %mm1
.  pxor %mm6, %mm6
.  pcmpeqw %mm5, %mm5
.  psubw mm6, mm5 ;//%mm5, %mm6
.  paddw %mm6, %mm6
.  paddw mm0, mm6 ;//%mm6, %mm0
.  paddw mm1, mm6 ;//%mm6, %mm1
.  psrlw mm0, $2 ;//$2, %mm0
.  psrlw mm1, $2 ;//$2, %mm1
.  packuswb mm0, mm1 ;//%mm1, %mm0
.  movq mm1, [edx] ;//(%edx ), %mm1
.  movq mm2, mm0 ;//%mm0, %mm2
.  psubusb mm0, mm1 ;//%mm1, %mm0
.  psubusb mm1, mm2 ;//%mm2, %mm1
.  por mm0, mm1 ;//%mm1, %mm0
.  movq mm1, [eax+8]; //8(%eax), %mm1
.  movq mm1, mm1 ;//%mm1, %mm2
.  punpcklbw mm1, mm7 ;//%mm7, %mm1
.  punpckhbw mm2, mm7 ;//%mm7, %mm2
.  movq mm3, [eax+9]; //9(%eax), %mm3
.  movq mm4, mm3 ;//%mm3, %mm4
.  punpcklbw mm3, mm7 ;//%mm7, %mm3
.  punpckhbw mm4, mm7 ;//%mm7, %mm4
.  paddw mm1, mm3 ;//%mm3, %mm1
.  paddw mm2, mm4 ;//%mm4, %mm2
.  movq mm3, [ebx+8] ;//8(%ebx), %mm3
.  movq mm4, mm3 ;//%mm3, %mm4
.  punpcklbw mm3, mm7 ;//%mm7, %mm3
.  punpckhbw mm4, mm7 ;//%mm7, %mm4
.  movq mm5, [ebx+9] ;//9(%ebx), %mm5
.  movq mm6, mm5 ;//%mm5, %mm6
.  punpcklbw mm5, mm7 ;//%mm7, %mm5
.  punpckhbw mm6, mm7 ;//%mm7, %mm6
.  paddw mm3, mm5 ;//%mm5, %mm3
.  paddw mm4, mm6 ;//%mm6, %mm4
.  paddw mm1, mm3 ;//%mm3, %mm1
.  paddw mm2, mm4 ;//%mm4, %mm2
.  pxor %mm6, %mm6
.  pcmpeqw %mm5, %mm5
.  psubw mm6, mm5 ;//%mm5, %mm6
.  paddw %mm6, %mm6
.  paddw mm1, mm6 ;//%mm6, %mm1
.  paddw mm2, mm6 ;//%mm6, %mm2
.  psrlw mm1, $2 ;//$2, %mm1
.  psrlw mm2, $2 ;//$2, %mm2
.  packuswb mm1, mm2 ;//%mm2, %mm1
.  movq mm2, [edx+8] ;//8(%edx), %mm2
.  movq mm3, mm1 ;//%mm1, %mm3
.  psubusb mm1, mm2 ;//%mm2, %mm1
.  psubusb mm2, mm3 ;//%mm3, %mm2
.  por mm1, mm2 ;//%mm2, %mm1
.  movq mm2, mm0 ;//%mm0, %mm2
.  punpcklbw mm0, mm7 ;//%mm7, %mm0
.  punpckhbw mm2, mm7 ;//%mm7, %mm2
.  movq mm3, mm1 ;//%mm1, %mm3
.  punpcklbw mm1, mm7 ;//%mm7, %mm1
.  punpckhbw mm3, mm7 ;//%mm7, %mm3
.  paddw mm0, mm1 ;//%mm1, %mm0
.  paddw mm0, mm2 ;//%mm2, %mm0
.  paddw mm0, mm3 ;//%mm3, %mm0
.  movq mm1, mm0 ;//%mm0, %mm1
.  punpcklwd mm0, mm7 ;//%mm7, %mm0
.  punpckhwd mm1, mm7 ;//%mm7, %mm1
.  paddd mm0, mm1 ;//%mm1, %mm0
.  movd ecx, mm0 ;//%mm0, %ecx

;//  add %%ecx, %0" : "=m" (s) : );

.  psrlq mm0, $32 ;//$32, %mm0
.  movd ecx, mm0 ;//%mm0, %ecx
    
;//add %%ecx, %0" : "=m" (s) : );

.  mov eax, ebx ;//%ebx, %eax
.  add ebx, esi ;//%esi, %ebx
.  add edx, esi ;//%esi, %edx
.  dec edi
.  jg d1top11
.  jmp d1exit

.  d1exit1:
 
;//mov %%edx, %0" : "=m" (s) : );

.  d1exit:
.  emms

;//return s;
}

;/* SSE version */

;  int dist1sse(unsigned char *blk1, unsigned char *blk2, int lx, int hx, int h;               y, int h, int distlim)

{
  int s = 0;

//  mov %0, %%edi" : : "m" (h) );
//  mov %0, %%edx" : : "m" (hy) );
//  mov %0, %%eax" : : "m" (hx) );
//  mov %0, %%esi" : : "m" (lx) );

  test %edi, %edi
  jle d1exitsse

  pxor %mm7, %mm7

  test %eax, %eax
  jne d1is10sse
  test %edx, %edx
  jne d1is10sse

  xor %edx, %edx
//  mov %0, %%eax" : : "m" (blk1) );
//  mov %0, %%ebx" : : "m" (blk2) );

  d1top00sse:
  movq (%eax ), %mm0
  psadbw (%ebx ), %mm0
  movq 8(%eax), %mm1
  psadbw 8(%ebx), %mm1
  paddd %mm1, %mm0
  movd %mm0, %ecx
  add %ecx, %edx

//  cmp %0, %%edx" : : "m" (distlim) );
  jge d1exit1sse
  add %esi, %eax
  add %esi, %ebx
  dec %edi
  jg d1top00sse
  jmp d1exit1sse

  d1is10sse:
  test %eax, %eax
  je d1is01sse
  test %edx, %edx
  jne d1is01sse

  xor %edx, %edx
//  mov %0, %%eax" : : "m" (blk1) );
//  mov %0, %%ebx" : : "m" (blk2) );

  pxor %mm6, %mm6
  pcmpeqw %mm1, %mm1
  psubw %mm1, %mm6

  d1top10sse:
  movq (%eax ), %mm0
  movq %mm0, %mm1
  punpcklbw %mm7, %mm0
  punpckhbw %mm7, %mm1
  movq 1(%eax), %mm2
  movq %mm2, %mm3
  punpcklbw %mm7, %mm2
  punpckhbw %mm7, %mm3
  paddw %mm2, %mm0
  paddw %mm3, %mm1
  paddw %mm6, %mm0
  paddw %mm6, %mm1
  psrlw $1, %mm0
  psrlw $1, %mm1
  packuswb %mm1, %mm0
  psadbw (%ebx ), %mm0

  movq 8(%eax), %mm1
  movq %mm1, %mm2
  punpcklbw %mm7, %mm1
  punpckhbw %mm7, %mm2
  movq 9(%eax), %mm3
  movq %mm3, %mm4
  punpcklbw %mm7, %mm3
  punpckhbw %mm7, %mm4
  paddw %mm3, %mm1
  paddw %mm4, %mm2
  paddw %mm6, %mm1
  paddw %mm6, %mm2
  psrlw $1, %mm1
  psrlw $1, %mm2
  packuswb %mm2, %mm1
  psadbw 8(%ebx), %mm1

  paddd %mm1, %mm0
  movd %mm0, %ecx
  add %ecx, %edx

  add %esi, %eax
  add %esi, %ebx
  dec %edi
  jg d1top10sse
  jmp d1exit1sse

  d1is01sse:
  test %eax, %eax
  jne d1is11sse
  test %edx, %edx
  je d1is11sse

  mov %0, %%eax" : : "m" (blk1) );
  mov %0, %%ebx" : : "m" (blk2) );


	
  mov %eax, %edx
  add %esi, %edx

  pxor %mm6, %mm6
  pcmpeqw %mm1, %mm1
  psubw %mm1, %mm6

  d1top01sse:
  movq (%eax ), %mm0
  movq %mm0, %mm1
  punpcklbw %mm7, %mm0
  punpckhbw %mm7, %mm1
  movq (%edx ), %mm2
  movq %mm2, %mm3
  punpcklbw %mm7, %mm2
  punpckhbw %mm7, %mm3
  paddw %mm2, %mm0
  paddw %mm3, %mm1
  paddw %mm6, %mm0
  paddw %mm6, %mm1
  psrlw $1, %mm0
  psrlw $1, %mm1
  packuswb %mm1, %mm0

  psadbw (%ebx ), %mm0

  movq 8(%eax), %mm1
  movq %mm1, %mm2
  punpcklbw %mm7, %mm1
  punpckhbw %mm7, %mm2
  movq 8(%edx), %mm3
  movq %mm3, %mm4
  punpcklbw %mm7, %mm3
  punpckhbw %mm7, %mm4
  paddw %mm3, %mm1
  paddw %mm4, %mm2
  paddw %mm6, %mm1
  paddw %mm6, %mm2
  psrlw $1, %mm1
  psrlw $1, %mm2
  packuswb %mm2, %mm1

  psadbw 8(%ebx), %mm1

  paddd %mm1, %mm0
  movd %mm0, %ecx
//  add %%ecx, %0" : "=m" (s) : );

  mov %edx, %eax
  add %esi, %ebx
  add %esi, %edx
  dec %edi
  jg d1top01sse
  jmp d1exitsse

  d1is11sse:
//  mov %0, %%eax" : : "m" (blk1) );
//  mov %0, %%ebx" : : "m" (blk2) );
  mov %eax, %edx
  add %esi, %edx

  d1top11sse:
  movq (%eax ), %mm0
  movq %mm0, %mm1
  punpcklbw %mm7, %mm0
  punpckhbw %mm7, %mm1
  movq 1(%eax), %mm2
  movq %mm2, %mm3
  punpcklbw %mm7, %mm2
  punpckhbw %mm7, %mm3
  paddw %mm2, %mm0
  paddw %mm3, %mm1
  movq (%edx ), %mm2
  movq %mm2, %mm3
  punpcklbw %mm7, %mm2
  punpckhbw %mm7, %mm3
  movq 1(%edx), %mm4
  movq %mm4, %mm5
  punpcklbw %mm7, %mm4
  punpckhbw %mm7, %mm5
  paddw %mm4, %mm2
  paddw %mm5, %mm3
  paddw %mm2, %mm0
  paddw %mm3, %mm1
  pxor %mm6, %mm6
  pcmpeqw %mm5, %mm5
  psubw %mm5, %mm6
  paddw %mm6, %mm6
  paddw %mm6, %mm0
  paddw %mm6, %mm1
  psrlw $2, %mm0
  psrlw $2, %mm1
  packuswb %mm1, %mm0

  psadbw (%ebx ), %mm0

  movq 8(%eax), %mm1
  movq %mm1, %mm2
  punpcklbw %mm7, %mm1
  punpckhbw %mm7, %mm2
  movq 9(%eax), %mm3
  movq %mm3, %mm4
  punpcklbw %mm7, %mm3
  punpckhbw %mm7, %mm4
  paddw %mm3, %mm1
  paddw %mm4, %mm2
  movq 8(%edx), %mm3
  movq %mm3, %mm4
  punpcklbw %mm7, %mm3
  punpckhbw %mm7, %mm4
  movq 9(%edx), %mm5
  movq %mm5, %mm6
  punpcklbw %mm7, %mm5
  punpckhbw %mm7, %mm6
  paddw %mm5, %mm3
  paddw %mm6, %mm4
  paddw %mm3, %mm1
  paddw %mm4, %mm2
  pxor %mm6, %mm6
  pcmpeqw %mm5, %mm5
  psubw %mm5, %mm6
  paddw %mm6, %mm6
  paddw %mm6, %mm1
  paddw %mm6, %mm2
  psrlw $2, %mm1
  psrlw $2, %mm2
  packuswb %mm2, %mm1

  psadbw 8(%ebx), %mm1

  paddd %mm1, %mm0
  movd %mm0, %ecx

//  add %%ecx, %0" : "=m" (s) :

  mov %edx, %eax
  add %esi, %edx
  add %esi, %ebx
  dec %edi
  jg d1top11sse
  jmp d1exitsse

  d1exit1sse:

//  mov %%edx, %0" : "=m" (s) :

  d1exitsse:
  emms

  return s;
}

