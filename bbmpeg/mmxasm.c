#if 0
/* RGB to YUV MMX routine */

static short int ycoef[7][4] = {     /* 32768 scaled y table, bgra order */
    {2363, 23442, 6963, 0},
    {3736, 19235, 9798, 0},
    {3736, 19235, 9798, 0},
    {3604, 19333, 9830, 0},
    {3736, 19235, 9798, 0},
    {3736, 19235, 9798, 0},
    {2851, 22970, 6947, 0}};

static short int ucoef[7][4] = {
    {16384, -12648, -3768, 0},
    {16384, -10846, -5538, 0},
    {16384, -10846, -5538, 0},
    {16384, -10846, -5538, 0},
    {16384, -10846, -5538, 0},
    {16384, -10846, -5538, 0},
    {16384, -12583, -3801, 0}};

static short int vcoef[7][4] = {
    {-1507, -14877, 16384, 0},
    {-2654, -13730, 16384, 0},
    {-2654, -13730, 16384, 0},
    {-2589, -13795, 16384, 0},
    {-2654, -13730, 16384, 0},
    {-2654, -13730, 16384, 0},
    {-1802, -14582, 16384, 0}};

static short int *ycoefs, *ucoefs, *vcoefs;

void init_rgb_to_yuv_mmx(int coeffs)
{
  int i;

  i = coeffs;
  if (i > 8)
    i = 3;

  ycoefs = &ycoef[i-1][0];
  ucoefs = &ucoef[i-1][0];
  vcoefs = &vcoef[i-1][0];
}

void RGBtoYUVmmx(unsigned char *src, unsigned char *desty, unsigned char *destu,
                 unsigned char *destv, int srcrowsize, int destrowsize,
                 int width, int height)
{
  unsigned char *yp, *up, *vp;
  unsigned char *prow;
  int i, j;

  _asm {
	xor       edx, edx
    	mov       eax, width
	sar       eax,1
        cmp       edx, eax
        jge       yuvexit

	mov       j, eax
	mov       eax, height
        mov       i, eax
	cmp       edx, eax
	jge       yuvexit

	mov       eax, desty
	mov       yp, eax
	mov       eax, destu
	mov       up, eax
	mov       eax, destv
	mov       vp, eax
	mov       eax, src
	mov       prow, eax
        pxor      MM7, MM7
        mov       eax, i

      heighttop:

        mov       i, eax
        mov       edi, j
        mov       ebx, prow
        mov       ecx, yp
        mov       edx, up
        mov       esi, vp

      widthtop:
        movq      MM5, [ebx]  // MM5 has 0 r2 g2 b2 0 r1 g1 b1, two pixels
        add       ebx, 8
        movq      MM6, MM5
        punpcklbw MM5, MM7 // MM5 has 0 r1 g1 b1
        punpckhbw MM6, MM7 // MM6 has 0 r2 g2 b2

        movq      MM0, MM5
        movq      MM1, MM6
        mov       eax, ycoefs
        pmaddwd   MM0, [eax] // MM0 has r1*cr and g1*cg+b1*cb
        movq      MM2, MM0
        psrlq     MM2, 32
        paddd     MM0, MM2   // MM0 has y1 in lower 32 bits
        pmaddwd   MM1, [eax] // MM1 has r2*cr and g2*cg+b2*cb
        movq      MM2, MM1
        psrlq     MM2, 32
        paddd     MM1, MM2   // MM1 has y2 in lower 32 bits
        movd      eax, MM0
        imul      eax, 219
        shr       eax, 8
        add       eax, 540672
        shr       eax, 15
        mov       [ecx], al
        inc       ecx
        movd      eax, MM1
        imul      eax, 219
        shr       eax, 8
        add       eax, 540672
        shr       eax, 15
        mov       [ecx], al
        inc       ecx

        movq      MM0, MM5
        movq      MM1, MM6
        mov       eax, ucoefs
        pmaddwd   MM0, [eax] // MM0 has r1*cr and g1*cg+b1*cb
        movq      MM2, MM0
        psrlq     MM2, 32
        paddd     MM0, MM2   // MM0 has u1 in lower 32 bits
        pmaddwd   MM1, [eax] // MM1 has r2*cr and g2*cg+b2*cb
        movq      MM2, MM1
        psrlq     MM2, 32
        paddd     MM1, MM2   // MM1 has u2 in lower 32 bits
        movd      eax, MM0
        imul      eax, 224
        sar       eax, 8
        add       eax, 4210688
        shr       eax, 15
        mov       [edx], al
        inc       edx
        movd      eax, MM1
        imul      eax, 224
        sar       eax, 8
        add       eax, 4210688
        shr       eax, 15
        mov       [edx], al
        inc       edx

        mov       eax, vcoefs
        pmaddwd   MM5, [eax] // MM5 has r1*cr and g1*cg+b1*cb
        movq      MM2, MM5
        psrlq     MM2, 32
        paddd     MM5, MM2   // MM5 has v1 in lower 32 bits
        pmaddwd   MM6, [eax] // MM6 has r2*cr and g2*cg+b2*cb
        movq      MM2, MM6
        psrlq     MM6, 32
        paddd     MM6, MM2   // MM6 has v2 in lower 32 bits
        movd      eax, MM5
        imul      eax, 224
        sar       eax, 8
        add       eax, 4210688
        shr       eax, 15
        mov       [esi], al
        inc       esi
        movd      eax, MM6
        imul      eax, 224
        sar       eax, 8
        add       eax, 4210688
        shr       eax, 15
        mov       [esi], al
        inc       esi

        dec       edi
        jnz       widthtop

        mov       eax, destrowsize
        add       yp, eax
        add       up, eax
        add       vp, eax
        mov       eax, srcrowsize
        sub       prow, eax
        mov       eax, i
        dec       eax
        jnz       heighttop

      yuvexit:
        emms
      }
}
#endif

/* motion estimation MMX routines */

/*
 * total absolute difference between two (16*h) blocks
 * including optional half pel interpolation of blk1 (hx,hy)
 * blk1,blk2: addresses of top left pels of both blocks
 * lx:        distance (in bytes) of vertically adjacent pels
 * hx,hy:     flags for horizontal and/or vertical interpolation
 * h:         height of block (usually 8 or 16)
 * distlim:   bail out if sum exceeds this value
 */

/* MMX version */

int dist1mmx(
unsigned char *blk1, unsigned char *blk2,
int lx, int hx, int hy, int h,
int distlim)
{
  int s = 0;

  asm ("mov %0, %%edi" : : "m" (h) );
  asm ("mov %0, %%edx" : : "m" (hy) );
  asm ("mov %0, %%eax" : : "m" (hx) );
  asm ("mov %0, %%esi" : : "m" (lx) );

  asm ("test %edi, %edi");
  asm ("jle d1exit");

  asm ("pxor %mm7, %mm7");

  asm ("test %eax, %eax");
  asm ("jne d1is10");
  asm ("test %edx, %edx");
  asm ("jne d1is10");

  asm ("xor %edx, %edx");
  asm ("mov %0, %%eax" : : "m" (blk1) );
  asm ("mov %0, %%ebx" : : "m" (blk2) );

  asm ("d1top00:");
  asm ("movq (%eax ), %mm0");
  asm ("movq (%ebx ), %mm1");
  asm ("movq %mm0, %mm2");
  asm ("psubusb %mm1, %mm0");
  asm ("psubusb %mm2, %mm1");
  asm ("por %mm1, %mm0");
  asm ("movq 8(%eax), %mm2");
  asm ("movq 8(%ebx), %mm3");
  asm ("movq %mm2, %mm4");
  asm ("psubusb %mm3, %mm2");
  asm ("psubusb %mm4, %mm3");
  asm ("por %mm3, %mm2");
  asm ("movq %mm0, %mm1");
  asm ("punpcklbw %mm7, %mm0");
  asm ("punpckhbw %mm7, %mm1");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");
  asm ("paddw %mm1, %mm0");
  asm ("paddw %mm2, %mm0");
  asm ("paddw %mm3, %mm0");
  asm ("movq %mm0, %mm1");
  asm ("punpcklwd %mm7, %mm0");
  asm ("punpckhwd %mm7, %mm1");
  asm ("paddd %mm1, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");
  asm ("psrlq $32, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");

  asm ("cmp %0, %%edx" : : "m" (distlim) );
  asm ("jge d1exit1");
  asm ("add %esi, %eax");
  asm ("add %esi, %ebx");
  asm ("dec %edi");
  asm ("jg d1top00");
  asm ("jmp d1exit1");

  asm ("d1is10:");
  asm ("test %eax, %eax");
  asm ("je d1is01");
  asm ("test %edx, %edx");
  asm ("jne d1is01");

  asm ("xor %edx, %edx");
  asm ("mov %0, %%eax" : : "m" (blk1) );
  asm ("mov %0, %%ebx" : : "m" (blk2) );

  asm ("pxor %mm6, %mm6");
  asm ("pcmpeqw %mm1, %mm1");
  asm ("psubw %mm1, %mm6");

  asm ("d1top10:");
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
  asm ("packuswb %mm1, %mm0");
  asm ("movq (%ebx ), %mm1");
  asm ("movq %mm0, %mm2");
  asm ("psubusb %mm1, %mm0");
  asm ("psubusb %mm2, %mm1");
  asm ("por %mm1, %mm0");
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
  asm ("packuswb %mm2, %mm1");
  asm ("movq 8(%ebx), %mm2");
  asm ("movq %mm1, %mm3");
  asm ("psubusb %mm2, %mm1");
  asm ("psubusb %mm3, %mm2");
  asm ("por %mm2, %mm1");
  asm ("movq %mm0, %mm2");
  asm ("punpcklbw %mm7, %mm0");
  asm ("punpckhbw %mm7, %mm2");
  asm ("movq %mm1, %mm3");
  asm ("punpcklbw %mm7, %mm1");
  asm ("punpckhbw %mm7, %mm3");
  asm ("paddw %mm1, %mm0");
  asm ("paddw %mm2, %mm0");
  asm ("paddw %mm3, %mm0");
  asm ("movq %mm0, %mm1");
  asm ("punpcklwd %mm7, %mm0");
  asm ("punpckhwd %mm7, %mm1");
  asm ("paddd %mm1, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");
  asm ("psrlq $32, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");

  asm ("add %esi, %eax");
  asm ("add %esi, %ebx");
  asm ("dec %edi");
  asm ("jg d1top10");
  asm ("jmp d1exit1");

  asm ("d1is01:");
  asm ("test %eax, %eax");
  asm ("jne d1is11");
  asm ("test %edx, %edx");
  asm ("je d1is11");

  asm ("mov %0, %%eax" : : "m" (blk1) );
  asm ("mov %0, %%edx" : : "m" (blk2) );
  asm ("mov %eax, %ebx");
  asm ("add %esi, %ebx");

  asm ("pxor %mm6, %mm6");
  asm ("pcmpeqw %mm1, %mm1");
  asm ("psubw %mm1, %mm6");

  asm ("d1top01:");
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
  asm ("packuswb %mm1, %mm0");
  asm ("movq (%edx ), %mm1");
  asm ("movq %mm0, %mm2");
  asm ("psubusb %mm1, %mm0");
  asm ("psubusb %mm2, %mm1");
  asm ("por %mm1, %mm0");
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
  asm ("packuswb %mm2, %mm1");
  asm ("movq 8(%edx), %mm2");
  asm ("movq %mm1, %mm3");
  asm ("psubusb %mm2, %mm1");
  asm ("psubusb %mm3, %mm2");
  asm ("por %mm2, %mm1");
  asm ("movq %mm0, %mm2");
  asm ("punpcklbw %mm7, %mm0");
  asm ("punpckhbw %mm7, %mm2");
  asm ("movq %mm1, %mm3");
  asm ("punpcklbw %mm7, %mm1");
  asm ("punpckhbw %mm7, %mm3");
  asm ("paddw %mm1, %mm0");
  asm ("paddw %mm2, %mm0");
  asm ("paddw %mm3, %mm0");
  asm ("movq %mm0, %mm1");
  asm ("punpcklwd %mm7, %mm0");
  asm ("punpckhwd %mm7, %mm1");
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
  asm ("jg d1top01");
  asm ("jmp d1exit");

  asm ("d1is11:");
  asm ("mov %0, %%eax" : : "m" (blk1) );
  asm ("mov %0, %%edx" : : "m" (blk2) );
  asm ("mov %eax, %ebx");
  asm ("add %esi, %ebx");

  asm ("d1top11:");
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
  asm ("packuswb %mm1, %mm0");
  asm ("movq (%edx ), %mm1");
  asm ("movq %mm0, %mm2");
  asm ("psubusb %mm1, %mm0");
  asm ("psubusb %mm2, %mm1");
  asm ("por %mm1, %mm0");
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
  asm ("packuswb %mm2, %mm1");
  asm ("movq 8(%edx), %mm2");
  asm ("movq %mm1, %mm3");
  asm ("psubusb %mm2, %mm1");
  asm ("psubusb %mm3, %mm2");
  asm ("por %mm2, %mm1");
  asm ("movq %mm0, %mm2");
  asm ("punpcklbw %mm7, %mm0");
  asm ("punpckhbw %mm7, %mm2");
  asm ("movq %mm1, %mm3");
  asm ("punpcklbw %mm7, %mm1");
  asm ("punpckhbw %mm7, %mm3");
  asm ("paddw %mm1, %mm0");
  asm ("paddw %mm2, %mm0");
  asm ("paddw %mm3, %mm0");
  asm ("movq %mm0, %mm1");
  asm ("punpcklwd %mm7, %mm0");
  asm ("punpckhwd %mm7, %mm1");
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
  asm ("jg d1top11");
  asm ("jmp d1exit");

  asm ("d1exit1:");
  asm ("mov %%edx, %0" : "=m" (s) : );

  asm ("d1exit:");
  asm ("emms");

  return s;
}

/* SSE version */

int dist1sse(
unsigned char *blk1, unsigned char *blk2,
int lx, int hx, int hy, int h,
int distlim)
{
  int s = 0;

  asm ("mov %0, %%edi" : : "m" (h) );
  asm ("mov %0, %%edx" : : "m" (hy) );
  asm ("mov %0, %%eax" : : "m" (hx) );
  asm ("mov %0, %%esi" : : "m" (lx) );

  asm ("test %edi, %edi");
  asm ("jle d1exitsse");

  asm ("pxor %mm7, %mm7");

  asm ("test %eax, %eax");
  asm ("jne d1is10sse");
  asm ("test %edx, %edx");
  asm ("jne d1is10sse");

  asm ("xor %edx, %edx");
  asm ("mov %0, %%eax" : : "m" (blk1) );
  asm ("mov %0, %%ebx" : : "m" (blk2) );

  asm ("d1top00sse:");
  asm ("movq (%eax ), %mm0");
  asm ("psadbw (%ebx ), %mm0");
  asm ("movq 8(%eax), %mm1");
  asm ("psadbw 8(%ebx), %mm1");
  asm ("paddd %mm1, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");

  asm ("cmp %0, %%edx" : : "m" (distlim) );
  asm ("jge d1exit1sse");
  asm ("add %esi, %eax");
  asm ("add %esi, %ebx");
  asm ("dec %edi");
  asm ("jg d1top00sse");
  asm ("jmp d1exit1sse");

  asm ("d1is10sse:");
  asm ("test %eax, %eax");
  asm ("je d1is01sse");
  asm ("test %edx, %edx");
  asm ("jne d1is01sse");

  asm ("xor %edx, %edx");
  asm ("mov %0, %%eax" : : "m" (blk1) );
  asm ("mov %0, %%ebx" : : "m" (blk2) );

  asm ("pxor %mm6, %mm6");
  asm ("pcmpeqw %mm1, %mm1");
  asm ("psubw %mm1, %mm6");

  asm ("d1top10sse:");
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
  asm ("packuswb %mm1, %mm0");
  asm ("psadbw (%ebx ), %mm0");

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
  asm ("packuswb %mm2, %mm1");
  asm ("psadbw 8(%ebx), %mm1");

  asm ("paddd %mm1, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %ecx, %edx");

  asm ("add %esi, %eax");
  asm ("add %esi, %ebx");
  asm ("dec %edi");
  asm ("jg d1top10sse");
  asm ("jmp d1exit1sse");

  asm ("d1is01sse:");
  asm ("test %eax, %eax");
  asm ("jne d1is11sse");
  asm ("test %edx, %edx");
  asm ("je d1is11sse");

  asm ("mov %0, %%eax" : : "m" (blk1) );
  asm ("mov %0, %%ebx" : : "m" (blk2) );
  asm ("mov %eax, %edx");
  asm ("add %esi, %edx");

  asm ("pxor %mm6, %mm6");
  asm ("pcmpeqw %mm1, %mm1");
  asm ("psubw %mm1, %mm6");

  asm ("d1top01sse:");
  asm ("movq (%eax ), %mm0");
  asm ("movq %mm0, %mm1");
  asm ("punpcklbw %mm7, %mm0");
  asm ("punpckhbw %mm7, %mm1");
  asm ("movq (%edx ), %mm2");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");
  asm ("paddw %mm2, %mm0");
  asm ("paddw %mm3, %mm1");
  asm ("paddw %mm6, %mm0");
  asm ("paddw %mm6, %mm1");
  asm ("psrlw $1, %mm0");
  asm ("psrlw $1, %mm1");
  asm ("packuswb %mm1, %mm0");

  asm ("psadbw (%ebx ), %mm0");

  asm ("movq 8(%eax), %mm1");
  asm ("movq %mm1, %mm2");
  asm ("punpcklbw %mm7, %mm1");
  asm ("punpckhbw %mm7, %mm2");
  asm ("movq 8(%edx), %mm3");
  asm ("movq %mm3, %mm4");
  asm ("punpcklbw %mm7, %mm3");
  asm ("punpckhbw %mm7, %mm4");
  asm ("paddw %mm3, %mm1");
  asm ("paddw %mm4, %mm2");
  asm ("paddw %mm6, %mm1");
  asm ("paddw %mm6, %mm2");
  asm ("psrlw $1, %mm1");
  asm ("psrlw $1, %mm2");
  asm ("packuswb %mm2, %mm1");

  asm ("psadbw 8(%ebx), %mm1");

  asm ("paddd %mm1, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %%ecx, %0" : "=m" (s) : );

  asm ("mov %edx, %eax");
  asm ("add %esi, %ebx");
  asm ("add %esi, %edx");
  asm ("dec %edi");
  asm ("jg d1top01sse");
  asm ("jmp d1exitsse");

  asm ("d1is11sse:");
  asm ("mov %0, %%eax" : : "m" (blk1) );
  asm ("mov %0, %%ebx" : : "m" (blk2) );
  asm ("mov %eax, %edx");
  asm ("add %esi, %edx");

  asm ("d1top11sse:");
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
  asm ("movq (%edx ), %mm2");
  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");
  asm ("movq 1(%edx), %mm4");
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
  asm ("packuswb %mm1, %mm0");

  asm ("psadbw (%ebx ), %mm0");

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
  asm ("movq 8(%edx), %mm3");
  asm ("movq %mm3, %mm4");
  asm ("punpcklbw %mm7, %mm3");
  asm ("punpckhbw %mm7, %mm4");
  asm ("movq 9(%edx), %mm5");
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
  asm ("packuswb %mm2, %mm1");

  asm ("psadbw 8(%ebx), %mm1");

  asm ("paddd %mm1, %mm0");
  asm ("movd %mm0, %ecx");
  asm ("add %%ecx, %0" : "=m" (s) : );

  asm ("mov %edx, %eax");
  asm ("add %esi, %edx");
  asm ("add %esi, %ebx");
  asm ("dec %edi");
  asm ("jg d1top11sse");
  asm ("jmp d1exitsse");

  asm ("d1exit1sse:");
  asm ("mov %%edx, %0" : "=m" (s) : );

  asm ("d1exitsse:");
  asm ("emms");


  return s;
}


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

/*
 * absolute difference error between a (16*h) block and a bidirectional
 * prediction
 *
 * p2: address of top left pel of block
 * pf,hxf,hyf: address and half pel flags of forward ref. block
 * pb,hxb,hyb: address and half pel flags of backward ref. block
 * h: height of block
 * lx: distance (in bytes) of vertically adjacent pels in p2,pf,pb
 * MMX version
 */

int bdist1mmx(
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
  asm ("jle bdist1exit");

  asm ("pxor %mm7, %mm7");
  asm ("pxor %mm6, %mm6");
  asm ("pcmpeqw %mm5, %mm5");
  asm ("psubw %mm5, %mm6");
  asm ("psllw $1, %mm6");

  asm ("bdist1top:");
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
  asm ("packuswb %mm1, %mm0");

  asm ("mov %0, %%eax" : : "m" (p2) );
  asm ("movq (%eax ), %mm1");
  asm ("movq %mm0, %mm2");
  asm ("psubusb %mm1, %mm0");
  asm ("psubusb %mm2, %mm1");
  asm ("por %mm1, %mm0");
  asm ("movq %mm0, %mm1");
  asm ("punpcklbw %mm7, %mm0");
  asm ("punpckhbw %mm7, %mm1");
  asm ("paddw %mm1, %mm0");
  asm ("movq %mm0, %mm1");
  asm ("punpcklwd %mm7, %mm0");
  asm ("punpckhwd %mm7, %mm1");

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
  asm ("packuswb %mm1, %mm0");
  asm ("mov %0, %%eax" : : "m" (p2) );
  asm ("movq 8(%eax), %mm1");
  asm ("movq %mm0, %mm2");
  asm ("psubusb %mm1, %mm0");
  asm ("psubusb %mm2, %mm1");
  asm ("por %mm1, %mm0");
  asm ("movq %mm0, %mm1");
  asm ("punpcklbw %mm7, %mm0");
  asm ("punpckhbw %mm7, %mm1");
  asm ("paddw %mm1, %mm0");
  asm ("movq %mm0, %mm1");
  asm ("punpcklwd %mm7, %mm0");
  asm ("punpckhwd %mm7, %mm1");
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
  asm ("jg bdist1top");
  asm ("mov %%esi, %0" : "=m" (s) : );

  asm ("bdist1exit:");
  asm ("emms");

  return s;
}

/* SSE version */

int bdist1sse(
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
  asm ("jle bd1exitsse");

  asm ("pxor %mm7, %mm7");
  asm ("pxor %mm6, %mm6");
  asm ("pcmpeqw %mm5, %mm5");
  asm ("psubw %mm5, %mm6");
  asm ("psllw $1, %mm6");

  asm ("bd1topsse:");
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
  asm ("packuswb %mm1, %mm0");

  asm ("mov %0, %%ebx" : : "m" (p2) );
  asm ("psadbw (%ebx ), %mm0");

  asm ("movd %mm0, %eax");
  asm ("add %eax, %esi");
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
  asm ("paddw %mm6, %mm0");
  asm ("paddw %mm6, %mm1");
  asm ("psllw $1, %mm6");
  asm ("psrlw $1, %mm0");
  asm ("psrlw $1, %mm1");
  asm ("packuswb %mm1, %mm0");

  asm ("mov %0, %%ebx" : : "m" (p2) );
  asm ("psadbw 8(%ebx), %mm0");

  asm ("movd %mm0, %eax");
  asm ("add %eax, %esi");

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
  asm ("jg bd1topsse");
  asm ("mov %%esi, %0" : "=m" (s) : );

  asm ("bd1exitsse:");
  asm ("emms");


  return s;
}

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
