/* Cpu detection code, extracted from mmx.h ((c)1997-99 by H. Dietz
   and R. Fisher). Converted to C and improved by Fabrice Bellard */

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#include "ac.h"

//exported
int mm_flag=-1;

/* ebx saving is necessary for PIC. gcc seems unable to see it alone */
#define cpuid(index,eax,ebx,ecx,edx)\
    __asm __volatile\
	("movl %%ebx, %%esi\n\t"\
         "cpuid\n\t"\
         "xchgl %%ebx, %%esi"\
         : "=a" (eax), "=S" (ebx),\
           "=c" (ecx), "=d" (edx)\
         : "0" (index));

/* Function to test if multimedia instructions are supported...  */
static int mm_support(void)
{
#ifdef ARCH_X86
    int rval;
    int eax, ebx, ecx, edx;
    
    __asm__ __volatile__ (
                          /* See if CPUID instruction is supported ... */
                          /* ... Get copies of EFLAGS into eax and ecx */
                          "pushf\n\t"
#ifdef __x86_64__
                          "pop %0\n\t"
#else
                          "popl %0\n\t"
#endif
                          "movl %0, %1\n\t"
                          
                          /* ... Toggle the ID bit in one copy and store */
                          /*     to the EFLAGS reg */
                          "xorl $0x200000, %0\n\t"
                          "push %0\n\t"
                          "popf\n\t"
                          
                          /* ... Get the (hopefully modified) EFLAGS */
                          "pushf\n\t"
#ifdef __x86_64__
                          "pop %0\n\t"
#else
                          "popl %0\n\t"
#endif
                          : "=a" (eax), "=c" (ecx)
                          :
                          : "cc" 
                          );
    
    if (eax == ecx)
        return 0; /* CPUID not supported */
    
    cpuid(0, eax, ebx, ecx, edx);

    if (ebx == 0x756e6547 &&
        edx == 0x49656e69 &&
        ecx == 0x6c65746e) {
        
        /* intel */
    inteltest:
        cpuid(1, eax, ebx, ecx, edx);
        if ((edx & 0x00800000) == 0)
            return 0;
        rval = MM_MMX;
        if (edx & 0x02000000) 
            rval |= MM_MMXEXT | MM_SSE;
        if (edx & 0x04000000) 
            rval |= MM_SSE2;
        return rval;
    } else if (ebx == 0x68747541 &&
               edx == 0x69746e65 &&
               ecx == 0x444d4163) {
        /* AMD */
        cpuid(0x80000000, eax, ebx, ecx, edx);
        if ((unsigned)eax < 0x80000001)
            goto inteltest;
        cpuid(0x80000001, eax, ebx, ecx, edx);
        if ((edx & 0x00800000) == 0)
            return 0;
        rval = MM_MMX;
        if (edx & 0x80000000)
            rval |= MM_3DNOW;
        if (edx & 0x00400000)
            rval |= MM_MMXEXT;
		if(edx & 0x02000000)
			rval |= MM_SSE;
		if(edx & 0x04000000)
			rval |= MM_SSE2;
        return rval;
    } else if (ebx == 0x69727943 &&
               edx == 0x736e4978 &&
               ecx == 0x64616574) {
        /* Cyrix Section */
        /* See if extended CPUID level 80000001 is supported */
        /* The value of CPUID/80000001 for the 6x86MX is undefined
           according to the Cyrix CPU Detection Guide (Preliminary
           Rev. 1.01 table 1), so we'll check the value of eax for
           CPUID/0 to see if standard CPUID level 2 is supported.
           According to the table, the only CPU which supports level
           2 is also the only one which supports extended CPUID levels.
        */
        if (eax != 2) 
            goto inteltest;
        cpuid(0x80000001, eax, ebx, ecx, edx);
        if ((eax & 0x00800000) == 0)
            return 0;
        rval = MM_MMX;
        if (eax & 0x01000000)
            rval |= MM_MMXEXT;
        return rval;
    } else {
        return 0;
    }
#else // not X86
    return 0;
#endif
}

int ac_mmflag(void) 
{
  if (mm_flag==-1) {
    mm_flag = mm_support();
#ifdef ARCH_X86
    mm_flag |= MM_IA32ASM;
#endif
  }
  return(mm_flag);
}

static char mmstr[64]="";

void ac_mmtest() 
{
  int cc=ac_mmflag();
  
  printf("(%s) available multimedia extensions:", __FILE__);
  
  if(cc & MM_SSE2) {
    printf(" sse2\n");
    return;
  } else if(cc & MM_SSE) {
    printf(" sse\n");
    return;
  } else if(cc & MM_3DNOW) {
    printf(" 3dnow\n");
    return;
  } else if(cc & MM_MMXEXT) {
    printf(" mmxext\n");
    return;
  } else if(cc & MM_MMX) {
    printf(" mmx\n");
    return;
  } else if(cc & MM_IA32ASM) {
    printf(" 32asm\n");
    return;
  } else printf(" C\n");
}

char *ac_mmstr(int flag, int mode) 
{
  int cc;
  
  if(flag==-1) 
    //get full mm caps
    cc=ac_mmflag();
  else
    cc=flag;
  
  //return max supported mm extensions, or str for user provided flag
  if(mode==0) {
    if(cc & MM_SSE2) {
      return("sse2");
      return;
    } else if(cc & MM_SSE) {
      return("sse");
      return;
    } else if(cc & MM_3DNOW) {
      return("3dnow");
      return;
    } else if(cc & MM_MMXEXT) {
      return("mmxext");
      return;
    } else if(cc & MM_MMX) {
      return("mmx");
      return;
    } else if(cc & MM_IA32ASM) {
      return("asm");
      return;
    } else return("C");
  } 


  //return full capability list
  if(mode==1) {
    if(cc & MM_SSE2) sprintf(mmstr, "sse2 ");
    if(cc & MM_SSE) strcat(mmstr, "sse "); 
    if(cc & MM_3DNOW) strcat(mmstr, "3dnow "); 
    if(cc & MM_MMXEXT) strcat(mmstr, "mmxext "); 
    if(cc & MM_MMX) strcat(mmstr, "mmx "); 
    if(cc & MM_IA32ASM) strcat(mmstr, "asm"); 
    return(mmstr);
  }
}
