#include <stdio.h>
#include <stdlib.h>

#include "config.h"

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

char buffer1[16]="HGFEDCBA\n";
char buffer2[16]="ABCDEFGH\n";

int main(int argc, char *argv[])
{

  //char *buffer1, *buffer2;
  
  //buffer1 = bufalloc(size);
  //memset(buffer1, 0xff, size);
  
  //buffer2 = bufalloc(size);
  //memset(buffer2, 0xff, size);
  
  //print available multimedia extensions
  ac_mmtest();
  
  ac_rescale_mmx(buffer1, buffer2, buffer1, 8, 32768, 32768);
 
  return(0);
}
