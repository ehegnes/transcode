#include <string.h>
#include <stdio.h>
#include "ac.h"

void * (*tc_memcpy)(void *, const void *, size_t) = memcpy;

void tc_memcpy_init(int verbose, int mmflags)
{
	int accel = mmflags == -1 ? ac_mmflag() : mmflags;
	const char * method = "libc";
	
#ifdef ARCH_X86
	if((accel & MM_MMXEXT) || (accel & MM_SSE))
	{
		method = "amdmmx";
		tc_memcpy = ac_memcpy_amdmmx;
	}
#endif

	if(verbose)
		fprintf(stderr, "tc_memcpy: using %s for memcpy\n", method);
}
