/*
 * This code is in the public domain.
 * Its a pipe multiplexer which can be used to execute pipes at the same time
 *
 * tccat -i file.vob | mpipe \
 *    "tcdemux -x vob -W>seek.log" \
 *    "tcextract -t vob -x ac3 | tcdecode -x a52 | tcscan -x pcm"
 *
 * compile with
 *  
 *  gcc -O2 -Wall -o mpipe mpipe.c
 *
 * (C) Tilmann Bitterberg
 * Thu Jul 31 15:21:12 CEST 2003
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CMD1 "cat"
#define CMD2 "wc -l"

#define MAXCMD 16
#define BUFSIZE 4096
#define EXE "mpipe"

int main (int argc, char *argv[])
{
   FILE *f[MAXCMD];
   int i=0;
   int numcmds=argc-1;
   char buf[BUFSIZE];
   size_t n=0;

   if (argc <= 1) {
       fprintf(stderr, "[%s] syntax: %s \"cmd1\" \"cmd2\" .. \"cmdN\"\n", EXE, argv[0]);
       return 1;
   }

   memset (buf, 0, BUFSIZE);

   for (i=0; i<MAXCMD; ++i) {
       f[i] = NULL;
   }

#if 0
   fprintf(stderr, "[%s] Found %d commands to execute.\n", EXE, numcmds);
   for (i=1; i<numcmds+1; ++i) {
       fprintf(stderr, "No. (%d): %s\n", i, argv[i]);
   }
#endif

   for (i=0; i<numcmds; ++i) {
       f[i] = popen(argv[i+1], "w");
       if (!f[i]) {
	   fprintf(stderr, "[%s] Cant exec %s -- ignoring\n", EXE, argv[i+1]);
       }
   }

   while ((n = fread(buf, 1, BUFSIZE, stdin))>0) {
       for (i=0; i<numcmds; i++) {
	   if (f[i]) {
	       fwrite(buf, n, 1, f[i]);
	   }
       }
       
   }

   for (i=0; i<numcmds; ++i) {
       if (f[i]) pclose(f[i]);
   }


   return 0;
}
