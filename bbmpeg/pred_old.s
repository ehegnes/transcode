/* subtract prediction from block data */
void sub_pred_mmx(unsigned char *pred,
                  unsigned char *cur,
                  int lx, short *blk)
{
  asm ("mov %0, %%eax" : : "m" (cur) );
  asm ("mov %0, %%ebx" : : "m" (pred) );
  asm ("mov %0, %%ecx" : : "m" (blk) );
  asm ("mov %0, %%edi" : : "m" (lx) );
  asm ("mov $8, %esi");
  asm ("pxor %mm7, %mm7");
  asm ("sub_top:");
  asm ("movq (%eax ), %mm0");
  asm ("add %edi, %eax");
  asm ("movq (%ebx ), %mm2");
  asm ("add %edi, %ebx");
  asm ("movq %mm0, %mm1");
  asm ("punpcklbw %mm7, %mm0");
  asm ("punpckhbw %mm7, %mm1");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");

  asm ("psubw %mm2, %mm0");
  asm ("psubw %mm3, %mm1");

  asm ("movq %mm0, (%ecx )");
  asm ("movq %mm1, 8(%ecx)");
  asm ("add $16, %ecx");

  asm ("dec %esi");
  asm ("jg sub_top");

  asm ("emms");
}

/* add prediction and prediction error, saturate to 0...255 */
void add_pred_mmx(unsigned char *pred,
                  unsigned char *cur,
                  int lx, short *blk)
{

  asm ("mov %0, %%eax" : : "m" (cur) );
  asm ("mov %0, %%ebx" : : "m" (pred) );
  asm ("mov %0, %%ecx" : : "m" (blk) );
  asm ("mov %0, %%edi" : : "m" (lx) );
  asm ("mov $8, %esi");
  asm ("pxor %mm7, %mm7");
  asm ("add_top:");
  asm ("movq (%ecx ), %mm0");
  asm ("movq 8(%ecx), %mm1");
  asm ("add $16, %ecx");
  asm ("movq (%ebx ), %mm2");
  asm ("add %edi, %ebx");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");

  asm ("paddw %mm2, %mm0");
  asm ("paddw %mm3, %mm1");
  asm ("packuswb %mm1, %mm0");

  asm ("movq %mm0, (%eax )");
  asm ("add %edi, %eax");

  asm ("dec %esi");
  asm ("jg add_top");

  asm ("emms");

}

