#include <string.h>
#include <stdio.h>
#include "ac.h"

void * (*tc_memcpy)(void *, const void *, size_t) = memcpy;

void tc_memcpy_init(int verbose, int mmflags)
{
	int accel = mmflags == -1 ? ac_mmflag() : mmflags;
	const char * method = "libc";
	
/* these functions are nasm assembly */
#ifdef HAVE_ASM_NASM
	if((accel & MM_MMXEXT) || (accel & MM_SSE))
	{
		method = "mmxext";
		tc_memcpy = ac_memcpy_amdmmx;
	}
	else
	{
		if(accel & MM_MMX)
		{
			method = "mmx";
			tc_memcpy = ac_memcpy_mmx;
		}
	}
#endif

	if(verbose)
		fprintf(stderr, "tc_memcpy: using %s for memcpy\n", method);
}
