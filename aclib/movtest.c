#include <stdio.h>
#include <stdlib.h>

#include "../config.h"

#include "ac.h"

#define LOOPS  2000
#define KBYTES 1024

static unsigned char *bufalloc(size_t size)
{

#ifdef HAVE_GETPAGESIZE
   int buffer_align=getpagesize();
#else
   int buffer_align=0;
#endif

   char *buf = malloc(size + buffer_align);

   int adjust;

   if (buf == NULL) {
       fprintf(stderr, "(%s) out of memory", __FILE__);
   }
   
   adjust = buffer_align - ((int) buf) % buffer_align;

   if (adjust == buffer_align)
      adjust = 0;

   return (unsigned char *) (buf + adjust);
}

int main(int argc, char *argv[])
{
    int n, size=KBYTES;
    int ac=0, loop=LOOPS;
    char *buffer1, *buffer2;

    if(argc >1 ) ac=atoi(argv[1]);
    if(argc >2 ) size=atoi(argv[2])*1024;

    loop = LOOPS*KBYTES/size;

    size *=1024;

    printf("bytes=%d MB, loops=%d, accel=%s\n", size/(1024*1024), loop, ac_mmstr(ac));

    buffer1 = bufalloc(size);
    memset(buffer1, 0xff, size);

    buffer2 = bufalloc(size);
    memset(buffer2, 0xff, size);

    //print available multimedia extensions
    ac_mmtest();
      
    for(n=0; n<loop; ++n) {
      
      switch (ac) {
	
      case MM_MMX:
	ac_memcpy_mmx(buffer1, buffer2, size);
	break;

      case MM_SSE:
	ac_memcpy_sse(buffer1, buffer2, size);
	break;

      case MM_SSE2:
	ac_memcpy_sse2(buffer1, buffer2, size);
	break;

      case MM_C:
	memcpy(buffer1, buffer2, size);
	break;

      default:
	printf("unsupported\n");
	break;

      } //ac
    } //loop

  return(0);
}
