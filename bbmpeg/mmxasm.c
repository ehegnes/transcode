/* mmxasm.c, mmx enabled routines */

//#ifdef __BORLANDC__
//#define EMIT db
//#else
//#define EMIT _emit
//#endif

#include "main.h"
//#include <excpt.h>

/* taken from AMD's CPUID_EX example */

unsigned int get_feature_flags()
{
  unsigned int result    = 0;
  unsigned int signature = 0;
  char vendor[13]        = "UnknownVendr";  /* Needs to be exactly 12 chars */
#ifdef MMX_GMO
  /* Define known vendor strings here */

  char vendorAMD[13]     = "AuthenticAMD";  /* Needs to be exactly 12 chars */
  /*;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Step 1: Check if processor has CPUID support. Processor faults
    ;; with an illegal instruction exception if the instruction is not
    ;; supported. This step catches the exception and immediately returns
    ;; with feature string bits with all 0s, if the exception occurs.
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;*/
  __try {
	__asm xor    eax, eax
	__asm xor    ebx, ebx
	__asm xor    ecx, ecx
	__asm xor    edx, edx
	__asm cpuid
      }

  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    return (0);
  }

  result |= FEATURE_CPUID;

  _asm {
         //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
         //;; Step 2: Check if CPUID supports function 1 (signature/std features)
         //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

         xor     eax, eax                       //; CPUID function #0
         cpuid                                  //; largest std func/vendor string
         mov     dword ptr [vendor], ebx        //; save
         mov     dword ptr [vendor+4], edx      //;  vendor
         mov     dword ptr [vendor+8], ecx      //;   string
         test    eax, eax                       //; largest standard function==0?
         jz      $no_standard_features          //; yes, no standard features func
         or      [result], FEATURE_STD_FEATURES //; does have standard features

         //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
         //;; Step 3: Get standard feature flags and signature
         //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

         mov     eax, 1                        //; CPUID function #1
         cpuid                                 //; get signature/std feature flgs
         mov     [signature], eax              //; save processor signature

         //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
         //;; Step 4: Extract desired features from standard feature flags
         //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

         //;; Check for time stamp counter support

         mov     ecx, CPUID_STD_TSC            //; bit 4 indicates TSC support
         and     ecx, edx                      //; supports TSC ? CPUID_STD_TSC:0
         neg     ecx                           //; supports TSC ? CY : NC
         sbb     ecx, ecx                      //; supports TSC ? 0xffffffff:0
         and     ecx, FEATURE_TSC              //; supports TSC ? FEATURE_TSC:0
         or      [result], ecx                 //; merge into feature flags

         //;; Check for MMX support

         mov     ecx, CPUID_STD_MMX            //; bit 23 indicates MMX support
         and     ecx, edx                      //; supports MMX ? CPUID_STD_MMX:0
         neg     ecx                           //; supports MMX ? CY : NC
         sbb     ecx, ecx                      //; supports MMX ? 0xffffffff:0
         and     ecx, FEATURE_MMX              //; supports MMX ? FEATURE_MMX:0
         or      [result], ecx                 //; merge into feature flags

         //;; Check for CMOV support

         mov     ecx, CPUID_STD_CMOV           //; bit 15 indicates CMOV support
         and     ecx, edx                      //; supports CMOV?CPUID_STD_CMOV:0
         neg     ecx                           //; supports CMOV ? CY : NC
         sbb     ecx, ecx                      //; supports CMOV ? 0xffffffff:0
         and     ecx, FEATURE_CMOV             //; supports CMOV ? FEATURE_CMOV:0
         or      [result], ecx                 //; merge into feature flags

         //;; Check support for P6-style MTRRs

         mov     ecx, CPUID_STD_MTRR           //; bit 12 indicates MTRR support
         and     ecx, edx                      //; supports MTRR?CPUID_STD_MTRR:0
         neg     ecx                           //; supports MTRR ? CY : NC
         sbb     ecx, ecx                      //; supports MTRR ? 0xffffffff:0
         and     ecx, FEATURE_P6_MTRR          //; supports MTRR ? FEATURE_MTRR:0
         or      [result], ecx                 //; merge into feature flags

         //;; Check for initial SSE support. There can still be partial SSE
         //;; support. Step 9 will check for partial support.

         mov     ecx, CPUID_STD_SSE            //; bit 25 indicates SSE support
         and     ecx, edx                      //; supports SSE ? CPUID_STD_SSE:0
         neg     ecx                           //; supports SSE ? CY : NC
         sbb     ecx, ecx                      //; supports SSE ? 0xffffffff:0
         and     ecx, (FEATURE_MMXEXT+FEATURE_SSEFP) //; supports SSE ?
                                               //; FEATURE_MMXEXT+FEATURE_SSEFP:0
         or      [result], ecx                 //; merge into feature flags

         //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
         //;; Step 5: Check for CPUID extended functions
         //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

         mov     eax, 0x80000000                //; extended function 0x80000000
         cpuid                                  //; largest extended function
         cmp     eax, 0x80000000                //; no function > 0x80000000 ?
         jbe     $no_extended_features          //; yes, no extended feature flags
         or      [result], FEATURE_EXT_FEATURES //; does have ext. feature flags

         //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
         //;; Step 6: Get extended feature flags
         //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

         mov     eax, 0x80000001               //; CPUID ext. function 0x80000001
         cpuid                                 //; EDX = extended feature flags

         //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
         //;; Step 7: Extract vendor independent features from extended flags
         //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

         //;; Check for 3DNow! support (vendor independent)

         mov     ecx, CPUID_EXT_3DNOW          //; bit 31 indicates 3DNow! supprt
         and     ecx, edx                      //; supp 3DNow! ?CPUID_EXT_3DNOW:0
         neg     ecx                           //; supports 3DNow! ? CY : NC
         sbb     ecx, ecx                      //; supports 3DNow! ? 0xffffffff:0
         and     ecx, FEATURE_3DNOW            //; support 3DNow!?FEATURE_3DNOW:0
         or      [result], ecx                 //; merge into feature flags

         //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
         //;; Step 8: Determine CPU vendor
         //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

         lea     esi, vendorAMD                //; AMD's vendor string
         lea     edi, vendor                   //; this CPU's vendor string
         mov     ecx, 12                       //; strings are 12 characters
         cld                                   //; compare lowest to highest
         repe    cmpsb                         //; current vendor str == AMD's ?
         jnz     $not_AMD                      //; no, CPU vendor is not AMD

         //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
         //;; Step 9: Check AMD specific extended features
         //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

         mov     ecx, CPUID_EXT_AMD_3DNOWEXT   //; bit 30 indicates 3DNow! ext.
         and     ecx, edx                      //; 3DNow! ext?
         neg     ecx                           //; 3DNow! ext ? CY : NC
         sbb     ecx, ecx                      //; 3DNow! ext ? 0xffffffff : 0
         and     ecx, FEATURE_3DNOWEXT         //; 3DNow! ext?FEATURE_3DNOWEXT:0
         or      [result], ecx                 //; merge into feature flags

         test    [result], FEATURE_MMXEXT      //; determined SSE MMX support?
         jnz     $has_mmxext                   //; yes, don't need to check again

         //;; Check support for AMD's multimedia instruction set additions

         mov     ecx, CPUID_EXT_AMD_MMXEXT     //; bit 22 indicates MMX extension
         and     ecx, edx                      //; MMX ext?CPUID_EXT_AMD_MMXEXT:0
         neg     ecx                           //; MMX ext? CY : NC
         sbb     ecx, ecx                      //; MMX ext? 0xffffffff : 0
         and     ecx, FEATURE_MMXEXT           //; MMX ext ? FEATURE_MMXEXT:0
         or      [result], ecx                 //; merge into feature flags

      $has_mmxext:

         //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
         //;; Step 10: Check AMD-specific features not reported by CPUID
         //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

         //;; Check support for AMD-K6 processor-style MTRRs

         mov     eax, [signature] 	//; get processor signature
         and     eax, 0xFFF 		//; extract family/model/stepping
         cmp     eax, 0x588 		//; CPU < AMD-K6-2/CXT ? CY : NC
         sbb     edx, edx 		//; CPU < AMD-K6-2/CXT ? 0xffffffff:0
         not     edx 			//; CPU < AMD-K6-2/CXT ? 0:0xffffffff
         cmp     eax, 0x600 		//; CPU < AMD Athlon ? CY : NC
         sbb     ecx, ecx 		//; CPU < AMD-K6 ? 0xffffffff:0
         and     ecx, edx 		//; (CPU>=AMD-K6-2/CXT)&&
					//; (CPU<AMD Athlon) ? 0xffffffff:0
         and     ecx, FEATURE_K6_MTRR 	//; (CPU>=AMD-K6-2/CXT)&&
					//; (CPU<AMD Athlon) ? FEATURE_K6_MTRR:0
         or      [result], ecx 		//; merge into feature flags

         jmp     $all_done 		//; desired features determined

      $not_AMD:

         /* Extract features specific to non AMD CPUs */

      $no_extended_features:
      $no_standard_features:
      $all_done:
  }
   /* The FP part of SSE introduces a new architectural state and therefore
      requires support from the operating system. So even if CPUID indicates
      support for SSE FP, the application might not be able to use it. If
      CPUID indicates support for SSE FP, check here whether it is also
      supported by the OS, and turn off the SSE FP feature bit if there
      is no OS support for SSE FP.

      Operating systems that do not support SSE FP return an illegal
      instruction exception if execution of an SSE FP instruction is performed.
      Here, a sample SSE FP instruction is executed, and is checked for an
      exception using the (non-standard) __try/__except mechanism
      of Microsoft Visual C.
   */

  if (result & FEATURE_SSEFP)
  {
    __try {
          __asm EMIT 0x0f
          __asm EMIT 0x56
          __asm EMIT 0xC0    //;; orps xmm0, xmm0
          return (result);
       }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return (result & (~FEATURE_SSEFP));
    }
  }
  else
#endif
    return (result);
}

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

/*
 * variance of a (16*16) block, multiplied by 256
 * p:  address of top left pel of block
 * lx: distance (in bytes) of vertically adjacent pels
 * MMX version
 */

int variancemmx(
unsigned char *p,
int lx)
{
  unsigned int s2 = 0;

  asm ("mov %0, %%eax" : : "m" (p) );
  asm ("mov %0, %%edi" : : "m" (lx) );
  asm ("xor %ebx, %ebx");
  asm ("mov %ebx, %ecx");
  asm ("mov $16, %esi");

  asm ("pxor %mm7, %mm7");

  asm ("vartop:");
  asm ("movq (%eax ), %mm0");
  asm ("movq 8(%eax), %mm2");

  asm ("movq %mm0, %mm1");
  asm ("punpcklbw %mm7, %mm0");
  asm ("punpckhbw %mm7, %mm1");

  asm ("movq %mm2, %mm3");
  asm ("punpcklbw %mm7, %mm2");
  asm ("punpckhbw %mm7, %mm3");

  asm ("movq %mm0, %mm5");
  asm ("paddusw %mm1, %mm5");
  asm ("paddusw %mm2, %mm5");
  asm ("paddusw %mm3, %mm5");

  asm ("movq %mm5, %mm6");
  asm ("punpcklwd %mm7, %mm5");
  asm ("punpckhwd %mm7, %mm6");
  asm ("paddd %mm6, %mm5");

  asm ("pmaddwd %mm0, %mm0");
  asm ("pmaddwd %mm1, %mm1");
  asm ("pmaddwd %mm2, %mm2");
  asm ("pmaddwd %mm3, %mm3");

  asm ("paddd %mm1, %mm0");
  asm ("paddd %mm2, %mm0");
  asm ("paddd %mm3, %mm0");

  asm ("movd %mm5, %edx");
  asm ("add %edx, %ecx");
  asm ("psrlq $32, %mm5");
  asm ("movd %mm5, %edx");
  asm ("add %edx, %ecx");
  asm ("movd %mm0, %edx");
  asm ("add %edx, %ebx");
  asm ("psrlq $32, %mm0");
  asm ("movd %mm0, %edx");
  asm ("add %edx, %ebx");

  asm ("add %edi, %eax");
  asm ("dec %esi");
  asm ("jg vartop");

  asm ("imul %ecx, %ecx");
  asm ("shr $8, %ecx");
  asm ("sub %ecx, %ebx");
  asm ("mov %%ebx, %0" : "=m" (s2) : );
  asm ("emms");

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
