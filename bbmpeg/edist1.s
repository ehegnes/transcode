int edist1mmx(
unsigned char *blk1, unsigned char *blk2,
int lx, int distlim)
{
  int s = 0;

  asm ("mov %0, %%edi" : : "m" (distlim) );
  asm ("mov %0, %%esi" : : "m" (lx) );

  asm ("pxor %mm7, %mm7");

  asm ("xor %edx, %edx");
  asm ("mov %0, %%eax" : : "m" (blk1) );
  asm ("mov %0, %%ebx" : : "m" (blk2) );

  asm ("movd (%eax ), %mm0");
  asm ("movd (%ebx ), %mm1");
  asm ("movq %mm0, %mm2");
  asm ("psubusb %mm1, %mm0");
  asm ("psubusb %mm2, %mm1");
  asm ("por %mm1, %mm0");
  asm ("punpcklbw %mm7, %mm0");
  asm ("movq %mm0, %mm1");
  asm ("punpcklwd %mm7, %mm0");
  asm ("punpckhwd %mm7, %mm1");
  asm ("paddd %mm1, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");
  asm ("psrlq $32, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");

  asm ("cmp %edi, %edx");
  asm ("jge e1exit");
  asm ("add %esi, %eax");
  asm ("add %esi, %ebx");

  asm ("movd (%eax ), %mm0");
  asm ("movd (%ebx ), %mm1");
  asm ("movq %mm0, %mm2");
  asm ("psubusb %mm1, %mm0");
  asm ("psubusb %mm2, %mm1");
  asm ("por %mm1, %mm0");
  asm ("punpcklbw %mm7, %mm0");
  asm ("movq %mm0, %mm1");
  asm ("punpcklwd %mm7, %mm0");
  asm ("punpckhwd %mm7, %mm1");
  asm ("paddd %mm1, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");
  asm ("psrlq $32, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");

  asm ("cmp %edi, %edx");
  asm ("jge e1exit");
  asm ("add %esi, %eax");
  asm ("add %esi, %ebx");

  asm ("movd (%eax ), %mm0");
  asm ("movd (%ebx ), %mm1");
  asm ("movq %mm0, %mm2");
  asm ("psubusb %mm1, %mm0");
  asm ("psubusb %mm2, %mm1");
  asm ("por %mm1, %mm0");
  asm ("punpcklbw %mm7, %mm0");
  asm ("movq %mm0, %mm1");
  asm ("punpcklwd %mm7, %mm0");
  asm ("punpckhwd %mm7, %mm1");
  asm ("paddd %mm1, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");
  asm ("psrlq $32, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");

  asm ("cmp %edi, %edx");
  asm ("jge e1exit");
  asm ("add %esi, %eax");
  asm ("add %esi, %ebx");

  asm ("movd (%eax ), %mm0");
  asm ("movd (%ebx ), %mm1");
  asm ("movq %mm0, %mm2");
  asm ("psubusb %mm1, %mm0");
  asm ("psubusb %mm2, %mm1");
  asm ("por %mm1, %mm0");
  asm ("punpcklbw %mm7, %mm0");
  asm ("movq %mm0, %mm1");
  asm ("punpcklwd %mm7, %mm0");
  asm ("punpckhwd %mm7, %mm1");
  asm ("paddd %mm1, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");
  asm ("psrlq $32, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");

  asm ("e1exit:");
  asm ("mov %%edx, %0" : "=m" (s) : );
  asm ("emms");

  return s;
}


/* SSE version */

int edist1sse(
unsigned char *blk1, unsigned char *blk2,
int lx, int distlim)
{
  int s = 0;

  asm ("mov %0, %%edi" : : "m" (distlim) );
  asm ("mov %0, %%esi" : : "m" (lx) );

  asm ("pxor %mm7, %mm7");

  asm ("xor %edx, %edx");
  asm ("mov %0, %%eax" : : "m" (blk1) );
  asm ("mov %0, %%ebx" : : "m" (blk2) );

  asm ("movd (%eax ), %mm0");
  asm ("movd (%ebx ), %mm1");
  asm ("psadbw %mm1, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");

  asm ("cmp %edi, %edx");
  asm ("jge e1exitsse");
  asm ("add %esi, %eax");
  asm ("add %esi, %ebx");

  asm ("movd (%eax ), %mm0");
  asm ("movd (%ebx ), %mm1");
  asm ("psadbw %mm1, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");

  asm ("cmp %edi, %edx");
  asm ("jge e1exitsse");
  asm ("add %esi, %eax");
  asm ("add %esi, %ebx");

  asm ("movd (%eax ), %mm0");
  asm ("movd (%ebx ), %mm1");
  asm ("psadbw %mm1, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");

  asm ("cmp %edi, %edx");
  asm ("jge e1exitsse");
  asm ("add %esi, %eax");
  asm ("add %esi, %ebx");

  asm ("movd (%eax ), %mm0");
  asm ("movd (%ebx ), %mm1");
  asm ("psadbw %mm1, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");

  asm ("e1exitsse:");
  asm ("mov %%edx, %0" : "=m" (s) : );
  asm ("emms");

  return s;
}
