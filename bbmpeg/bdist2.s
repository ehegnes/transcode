/*
 * squared error between a (16*h) block and a bidirectional
 * prediction
 *
 * p2: address of top left pel of block
 * pf,hxf,hyf: address and half pel flags of forward ref. block
 * pb,hxb,hyb: address and half pel flags of backward ref. block
 * h: height of block
 * lx: distance (in bytes) of vertically adjacent pels in p2,pf,pb
 * MMX version
 */

int bdist2mmx(
unsigned char *pf, unsigned char *pb, unsigned char *p2,
int lx, int hxf, int hyf, int hxb, int hyb, int h)
{
  unsigned char *pfa,*pfb,*pfc,*pba,*pbb,*pbc;
  int s;

  asm ("mov %0, %%edx" : : "m" (hxb) );
  asm ("mov %0, %%eax" : : "m" (hxf) );
  asm ("mov %0, %%esi" : : "m" (lx) );

  asm ("mov %0, %%ecx" : : "m" (pf) );
  asm ("add %eax, %ecx");
  asm ("mov %%ecx, %0" : "=m" (pfa) : );
  asm ("mov %esi, %ecx");
  asm ("imul %0, %%ecx" : : "m" (hyf) );
  asm ("mov %0, %%ebx" : : "m" (pf) );
  asm ("add %ebx, %ecx");
  asm ("mov %%ecx, %0" : "=m" (pfb) : );
  asm ("add %ecx, %eax");
  asm ("mov %%eax, %0" : "=m" (pfc) : );
  asm ("mov %0, %%eax" : : "m" (pb) );
  asm ("add %edx, %eax");
  asm ("mov %%eax, %0" : "=m" (pba) : );
  asm ("mov %esi, %eax");
  asm ("imul %0, %%eax" : : "m" (hyb) );
  asm ("mov %0, %%ecx" : : "m" (pb) );
  asm ("add %ecx, %eax");
  asm ("mov %%eax, %0" : "=m" (pbb) : );
  asm ("add %eax, %edx");
  asm ("mov %%edx, %0" : "=m" (pbc) : );
  asm ("xor %esi, %esi");
  asm ("mov %%esi, %0" : "=m" (s) : );

  asm ("mov %0, %%edi" : : "m" (h) );
  asm ("test %edi, %edi");
  asm ("jle bdist2exit");

  asm ("pxor %mm7, %mm7");
  asm ("pxor %mm6, %mm6");
  asm ("pcmpeqw %mm5, %mm5");
  asm ("psubw %mm5, %mm6");
  asm ("psllw $1, %mm6");

  asm ("bdist2top:");
  asm ("mov %0, %%eax" : : "m" (pf) );
  asm ("mov %0, %%ebx" : : "m" (pfa) );
  asm ("mov %0, %%ecx" : : "m" (pfb) );
  asm ("mov %0, %%edx" : : "m" (pfc) );
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
  asm ("movq (%ecx ), %mm2");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");
  asm ("paddw %mm2, %mm0");
  asm ("paddw %mm3, %mm1");
  asm ("movq (%edx ), %mm2");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");
  asm ("paddw %mm2, %mm0");
  asm ("paddw %mm3, %mm1");
  asm ("paddw %mm6, %mm0");
  asm ("paddw %mm6, %mm1");
  asm ("psrlw $2, %mm0");
  asm ("psrlw $2, %mm1");

  asm ("mov %0, %%eax" : : "m" (pb) );
  asm ("mov %0, %%ebx" : : "m" (pba) );
  asm ("mov %0, %%ecx" : : "m" (pbb) );
  asm ("mov %0, %%edx" : : "m" (pbc) );
  asm ("movq (%eax ), %mm2");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");
  asm ("movq (%ebx ), %mm4");
  asm ("movq %mm4, %mm5");
  asm ("punpcklbw %mm7, %mm4");
  asm ("punpckhbw %mm7, %mm5");
  asm ("paddw %mm4, %mm2");
  asm ("paddw %mm5, %mm3");
  asm ("movq (%ecx ), %mm4");
  asm ("movq %mm4, %mm5");
  asm ("punpcklbw %mm7, %mm4");
  asm ("punpckhbw %mm7, %mm5");
  asm ("paddw %mm4, %mm2");
  asm ("paddw %mm5, %mm3");
  asm ("movq (%edx ), %mm4");
  asm ("movq %mm4, %mm5");
  asm ("punpcklbw %mm7, %mm4");
  asm ("punpckhbw %mm7, %mm5");
  asm ("paddw %mm4, %mm2");
  asm ("paddw %mm5, %mm3");

  asm ("paddw %mm6, %mm2");
  asm ("paddw %mm6, %mm3");
  asm ("psrlw $2, %mm2");
  asm ("psrlw $2, %mm3");

  asm ("paddw %mm2, %mm0");
  asm ("paddw %mm3, %mm1");
  asm ("psrlw $1, %mm6");
  asm ("paddw %mm6, %mm0");
  asm ("paddw %mm6, %mm1");
  asm ("psllw $1, %mm6");
  asm ("psrlw $1, %mm0");
  asm ("psrlw $1, %mm1");

  asm ("mov %0, %%eax" : : "m" (p2) );
  asm ("movq (%eax ), %mm2");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");

  asm ("psubw %mm2, %mm0");
  asm ("psubw %mm3, %mm1");
  asm ("pmaddwd %mm0, %mm0");
  asm ("pmaddwd %mm1, %mm1");
  asm ("paddd %mm1, %mm0");

  asm ("movd %mm0, %eax");
  asm ("psrlq $32, %mm0");
  asm ("movd %mm0, %ebx");
  asm ("add %eax, %esi");
  asm ("add %ebx, %esi");

  asm ("mov %0, %%eax" : : "m" (pf) );
  asm ("mov %0, %%ebx" : : "m" (pfa) );
  asm ("mov %0, %%ecx" : : "m" (pfb) );
  asm ("mov %0, %%edx" : : "m" (pfc) );
  asm ("movq 8(%eax), %mm0");
  asm ("movq %mm0, %mm1");
  asm ("punpcklbw %mm7, %mm0");
  asm ("punpckhbw %mm7, %mm1");
  asm ("movq 8(%ebx), %mm2");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");
  asm ("paddw %mm2, %mm0");
  asm ("paddw %mm3, %mm1");
  asm ("movq 8(%ecx), %mm2");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");
  asm ("paddw %mm2, %mm0");
  asm ("paddw %mm3, %mm1");
  asm ("movq 8(%edx), %mm2");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");
  asm ("paddw %mm2, %mm0");
  asm ("paddw %mm3, %mm1");
  asm ("paddw %mm6, %mm0");
  asm ("paddw %mm6, %mm1");
  asm ("psrlw $2, %mm0");
  asm ("psrlw $2, %mm1");

  asm ("mov %0, %%eax" : : "m" (pb) );
  asm ("mov %0, %%ebx" : : "m" (pba) );
  asm ("mov %0, %%ecx" : : "m" (pbb) );
  asm ("mov %0, %%edx" : : "m" (pbc) );
  asm ("movq 8(%eax), %mm2");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");
  asm ("movq 8(%ebx), %mm4");
  asm ("movq %mm4, %mm5");
  asm ("punpcklbw %mm7, %mm4");
  asm ("punpckhbw %mm7, %mm5");
  asm ("paddw %mm4, %mm2");
  asm ("paddw %mm5, %mm3");
  asm ("movq 8(%ecx), %mm4");
  asm ("movq %mm4, %mm5");
  asm ("punpcklbw %mm7, %mm4");
  asm ("punpckhbw %mm7, %mm5");
  asm ("paddw %mm4, %mm2");
  asm ("paddw %mm5, %mm3");
  asm ("movq 8(%edx), %mm4");
  asm ("movq %mm4, %mm5");
  asm ("punpcklbw %mm7, %mm4");
  asm ("punpckhbw %mm7, %mm5");
  asm ("paddw %mm4, %mm2");
  asm ("paddw %mm5, %mm3");
  asm ("paddw %mm6, %mm2");
  asm ("paddw %mm6, %mm3");
  asm ("psrlw $2, %mm2");
  asm ("psrlw $2, %mm3");

  asm ("paddw %mm2, %mm0");
  asm ("paddw %mm3, %mm1");
  asm ("psrlw $1, %mm6");
  asm ("paddW %mm6, %mm0");
  asm ("paddw %mm6, %mm1");
  asm ("psllw $1, %mm6");
  asm ("psrlw $1, %mm0");
  asm ("psrlw $1, %mm1");

  asm ("mov %0, %%eax" : : "m" (p2) );
  asm ("movq 8(%eax), %mm2");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");

  asm ("psubw %mm2, %mm0");
  asm ("psubw %mm3, %mm1");
  asm ("pmaddwd %mm0, %mm0");
  asm ("pmaddwd %mm1, %mm1");
  asm ("paddd %mm1, %mm0");

  asm ("movd %mm0, %eax");
  asm ("psrlq $32, %mm0");
  asm ("movd %mm0, %ebx");
  asm ("add %eax, %esi");
  asm ("add %ebx, %esi");

  asm ("mov %0, %%eax" : : "m" (lx) );
  asm ("add %%eax, %0" : "=m" (p2) : );
  asm ("add %%eax, %0" : "=m" (pf) : );
  asm ("add %%eax, %0" : "=m" (pfa) : );
  asm ("add %%eax, %0" : "=m" (pfb) : );
  asm ("add %%eax, %0" : "=m" (pfc) : );
  asm ("add %%eax, %0" : "=m" (pb) : );
  asm ("add %%eax, %0" : "=m" (pba) : );
  asm ("add %%eax, %0" : "=m" (pbb) : );
  asm ("add %%eax, %0" : "=m" (pbc) : );

  asm ("dec %edi");
  asm ("jg bdist2top");
  asm ("mov %%esi, %0" : "=m" (s) : );

  asm ("bdist2exit:");
  asm ("emms");

  return s;
}

k