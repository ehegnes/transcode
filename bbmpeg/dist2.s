/*
 * total squared difference between two (16*h) blocks
 * including optional half pel interpolation of blk1 (hx,hy)
 * blk1,blk2: addresses of top left pels of both blocks
 * lx:        distance (in bytes) of vertically adjacent pels
 * hx,hy:     flags for horizontal and/or vertical interpolation
 * h:         height of block (usually 8 or 16)
 * MMX version
 */

int dist2mmx(
unsigned char *blk1, unsigned char *blk2,
int lx, int hx, int hy, int h)
{
  int s = 0;

  asm ("mov %0, %%edi" : : "m" (h) );
  asm ("mov %0, %%edx" : : "m" (hy) );
  asm ("mov %0, %%eax" : : "m" (hx) );
  asm ("mov %0, %%esi" : : "m" (lx) );

  asm ("test %edi, %edi");
  asm ("jle d2exit");

  asm ("pxor %mm7, %mm7");

  asm ("test %eax, %eax");
  asm ("jne d2is10");
  asm ("test %edx, %edx");
  asm ("jne d2is10");

  asm ("xor %edx, %edx");
  asm ("mov %0, %%eax" : : "m" (blk1) );
  asm ("mov %0, %%ebx" : : "m" (blk2) );

  asm ("d2top00:");
  asm ("movq (%eax ), %mm0");
  asm ("movq %mm0, %mm1");
  asm ("punpcklbw %mm7, %mm0");
  asm ("punpckhbw %mm7, %mm1");

  asm ("movq (%ebx ), %mm2");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");

  asm ("psubw %mm2, %mm0");
  asm ("psubw %mm3, %mm1");
  asm ("pmaddwd %mm0, %mm0");
  asm ("pmaddwd %mm1, %mm1");
  asm ("paddd %mm1, %mm0");

  asm ("movq 8(%eax), %mm1");
  asm ("movq %mm1, %mm2");
  asm ("punpcklbw %mm7, %mm1");
  asm ("punpckhbw %mm7, %mm2");

  asm ("movq 8(%ebx), %mm3");
  asm ("movq %mm3, %mm4");
  asm ("punpcklbw %mm7, %mm3");
  asm ("punpckhbw %mm7, %mm4");

  asm ("psubw %mm3, %mm1");
  asm ("psubw %mm4, %mm2");
  asm ("pmaddwd %mm1, %mm1");
  asm ("pmaddwd %mm2, %mm2");
  asm ("paddd %mm2, %mm1");

  asm ("paddd %mm1, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");
  asm ("psrlq $32, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");

  asm ("add %esi, %eax");
  asm ("add %esi, %ebx");
  asm ("dec %edi");
  asm ("jg d2top00");
  asm ("jmp d2exit1");

  asm ("d2is10:");
  asm ("test %eax, %eax");
  asm ("je d2is01");
  asm ("test %edx, %edx");
  asm ("jne d2is01");

  asm ("xor %edx, %edx");
  asm ("mov %0, %%eax" : : "m" (blk1) );
  asm ("mov %0, %%ebx" : : "m" (blk2) );

  asm ("pxor %mm6, %mm6");
  asm ("pcmpeqw %mm1, %mm1");
  asm ("psubw %mm1, %mm6");

  asm ("d2top10:");
  asm ("movq (%eax ), %mm0");
  asm ("movq %mm0, %mm1");
  asm ("punpcklbw %mm7, %mm0");
  asm ("punpckhbw %mm7, %mm1");
  asm ("movq 1(%eax), %mm2");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");
  asm ("paddw %mm2, %mm0");
  asm ("paddw %mm3, %mm1");
  asm ("paddw %mm6, %mm0");
  asm ("paddw %mm6, %mm1");
  asm ("psrlw $1, %mm0");
  asm ("psrlw $1, %mm1");

  asm ("movq (%ebx ), %mm2");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");

  asm ("psubw %mm2, %mm0");
  asm ("psubw %mm3, %mm1");
  asm ("pmaddwd %mm0, %mm0");
  asm ("pmaddwd %mm1, %mm1");
  asm ("paddd %mm1, %mm0");

  asm ("movq 8(%eax), %mm1");
  asm ("movq %mm1, %mm2");
  asm ("punpcklbw %mm7, %mm1");
  asm ("punpckhbw %mm7, %mm2");
  asm ("movq 9(%eax), %mm3");
  asm ("movq %mm3, %mm4");
  asm ("punpcklbw %mm7, %mm3");
  asm ("punpckhbw %mm7, %mm4");
  asm ("paddw %mm3, %mm1");
  asm ("paddw %mm4, %mm2");
  asm ("paddw %mm6, %mm1");
  asm ("paddw %mm6, %mm2");
  asm ("psrlw $1, %mm1");
  asm ("psrlw $1, %mm2");

  asm ("movq 8(%ebx), %mm3");
  asm ("movq %mm3, %mm4");
  asm ("punpcklbw %mm7, %mm3");
  asm ("punpckhbw %mm7, %mm4");

  asm ("psubw %mm3, %mm1");
  asm ("psubw %mm4, %mm2");
  asm ("pmaddwd %mm1, %mm1");
  asm ("pmaddwd %mm2, %mm2");
  asm ("paddd %mm2, %mm1");

  asm ("paddd %mm1, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");
  asm ("psrlq $32, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");

  asm ("add %esi, %eax");
  asm ("add %esi, %ebx");
  asm ("dec %edi");
  asm ("jg d2top10");
  asm ("jmp d2exit1");

  asm ("d2is01:");
  asm ("test %eax, %eax");
  asm ("jne d2is11");
  asm ("test %edx, %edx");
  asm ("je d2is11");

  asm ("mov %0, %%eax" : : "m" (blk1) );
  asm ("mov %0, %%edx" : : "m" (blk2) );
  asm ("mov %eax, %ebx");
  asm ("add %esi, %ebx");

  asm ("pxor %mm6, %mm6");
  asm ("pcmpeqw %mm1, %mm1");
  asm ("psubw %mm1, %mm6");

  asm ("d2top01:");
  asm ("movq (%eax ), %mm0");
  asm ("movq %mm0, %mm1");
  asm ("punpcklbw %mm7, %mm0");
  asm ("punpckhbw %mm7, %mm1");
  asm ("movq (%ebx ), %mm2");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");
  asm ("paddw %mm2, %mm0");
  asm ("paddw %mm3, %mm1");
  asm ("paddw %mm6, %mm0");
  asm ("paddw %mm6, %mm1");
  asm ("psrlw $1, %mm0");
  asm ("psrlw $1, %mm1");

  asm ("movq (%edx ), %mm2");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");

  asm ("psubw %mm2, %mm0");
  asm ("psubw %mm3, %mm1");
  asm ("pmaddwd %mm0, %mm0");
  asm ("pmaddwd %mm1, %mm1");
  asm ("paddd %mm1, %mm0");

  asm ("movq 8(%eax), %mm1");
  asm ("movq %mm1, %mm2");
  asm ("punpcklbw %mm7, %mm1");
  asm ("punpckhbw %mm7, %mm2");
  asm ("movq 8(%ebx), %mm3");
  asm ("movq %mm3, %mm4");
  asm ("punpcklbw %mm7, %mm3");
  asm ("punpckhbw %mm7, %mm4");
  asm ("paddw %mm3, %mm1");
  asm ("paddw %mm4, %mm2");
  asm ("paddw %mm6, %mm1");
  asm ("paddw %mm6, %mm2");
  asm ("psrlw $1, %mm1");
  asm ("psrlw $1, %mm2");

  asm ("movq 8(%edx), %mm3");
  asm ("movq %mm3, %mm4");
  asm ("punpcklbw %mm7, %mm3");
  asm ("punpckhbw %mm7, %mm4");

  asm ("psubw %mm3, %mm1");
  asm ("psubw %mm4, %mm2");
  asm ("pmaddwd %mm1, %mm1");
  asm ("pmaddwd %mm2, %mm2");
  asm ("paddd %mm2, %mm1");

  asm ("paddd %mm1, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %%ecx, %0" : "=m" (s) : );
  asm ("psrlq $32, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %%ecx, %0" : "=m" (s) : );

  asm ("mov %ebx, %eax");
  asm ("add %esi, %edx");
  asm ("add %esi, %ebx");
  asm ("dec %edi");
  asm ("jg d2top01");
  asm ("jmp d2exit");

  asm ("d2is11:");
  asm ("mov %0, %%eax" : : "m" (blk1) );
  asm ("mov %0, %%edx" : : "m" (blk2) );
  asm ("mov %eax, %ebx");
  asm ("add %esi, %ebx");

  asm ("d2top11:");
  asm ("movq (%eax ), %mm0");
  asm ("movq %mm0, %mm1");
  asm ("punpcklbw %mm7, %mm0");
  asm ("punpckhbw %mm7, %mm1");
  asm ("movq 1(%eax), %mm2");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");
  asm ("paddw %mm2, %mm0");
  asm ("paddw %mm3, %mm1");
  asm ("movq (%ebx ), %mm2");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");
  asm ("movq 1(%ebx), %mm4");
  asm ("movq %mm4, %mm5");
  asm ("punpcklbw %mm7, %mm4");
  asm ("punpckhbw %mm7, %mm5");
  asm ("paddw %mm4, %mm2");
  asm ("paddw %mm5, %mm3");
  asm ("paddw %mm2, %mm0");
  asm ("paddw %mm3, %mm1");
  asm ("pxor %mm6, %mm6");
  asm ("pcmpeqw %mm5, %mm5");
  asm ("psubw %mm5, %mm6");
  asm ("paddw %mm6, %mm6");
  asm ("paddw %mm6, %mm0");
  asm ("paddw %mm6, %mm1");
  asm ("psrlw $2, %mm0");
  asm ("psrlw $2, %mm1");

  asm ("movq (%edx ), %mm2");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");

  asm ("psubw %mm2, %mm0");
  asm ("psubw %mm3, %mm1");
  asm ("pmaddwd %mm0, %mm0");
  asm ("pmaddwd %mm1, %mm1");
  asm ("paddd %mm1, %mm0");

  asm ("movq 8(%eax), %mm1");
  asm ("movq %mm1, %mm2");
  asm ("punpcklbw %mm7, %mm1");
  asm ("punpckhbw %mm7, %mm2");
  asm ("movq 9(%eax), %mm3");
  asm ("movq %mm3, %mm4");
  asm ("punpcklbw %mm7, %mm3");
  asm ("punpckhbw %mm7, %mm4");
  asm ("paddw %mm3, %mm1");
  asm ("paddw %mm4, %mm2");
  asm ("movq 8(%ebx), %mm3");
  asm ("movq %mm3, %mm4");
  asm ("punpcklbw %mm7, %mm3");
  asm ("punpckhbw %mm7, %mm4");
  asm ("movq 9(%ebx), %mm5");
  asm ("movq %mm5, %mm6");
  asm ("punpcklbw %mm7, %mm5");
  asm ("punpckhbw %mm7, %mm6");
  asm ("paddw %mm5, %mm3");
  asm ("paddw %mm6, %mm4");
  asm ("paddw %mm3, %mm1");
  asm ("paddw %mm4, %mm2");
  asm ("pxor %mm6, %mm6");
  asm ("pcmpeqw %mm5, %mm5");
  asm ("psubw %mm5, %mm6");
  asm ("paddw %mm6, %mm6");
  asm ("paddw %mm6, %mm1");
  asm ("paddw %mm6, %mm2");
  asm ("psrlw $2, %mm1");
  asm ("psrlw $2, %mm2");

  asm ("movq 8(%edx), %mm3");
  asm ("movq %mm3, %mm4");
  asm ("punpcklbw %mm7, %mm3");
  asm ("punpckhbw %mm7, %mm4");

  asm ("psubw %mm3, %mm1");
  asm ("psubw %mm4, %mm2");
  asm ("pmaddwd %mm1, %mm1");
  asm ("pmaddwd %mm2, %mm2");
  asm ("paddd %mm2, %mm1");

  asm ("paddd %mm1, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %%ecx, %0" : "=m" (s) : );
  asm ("psrlq $32, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %%ecx, %0" : "=m" (s) : );

  asm ("mov %ebx, %eax");
  asm ("add %esi, %ebx");
  asm ("add %esi, %edx");
  asm ("dec %edi");
  asm ("jg d2top11");
  asm ("jmp d2exit");

  asm ("d2exit1:");
  asm ("mov %%edx, %0" : "=m" (s) : );

  asm ("d2exit:");
  asm ("emms");

  return s;
}

