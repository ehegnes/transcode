/* pvn2pnm by Jacob (Jack) Gryn

   Converts a PVN file into multiple PNM (PBM/PGM/PPM) files.

   * the PVN (PVB/PVG/PVP) file format, and this code
     is (c) 2003,2004 Jacob (Jack) Gryn

   * the author grants full rights to all who wish to use
     and distribute this code and the corresponding file
     formats on the assumption that credit is given to
     the author if asked

   Jacob (Jack) Gryn
 */
#include "pvnglobals.h"
#include "pnmpvn.h"

#define PVN2PNM_VERSION "0.1"

int main(int argc, char *argv[])
{
  char *infile;
  int digits;
  int minargc=1;

  printf("pvn2pnm v%s by Jacob (Jack) Gryn\n\n", PVN2PNM_VERSION);

  if (argc > 1)
  {
    if ((argv[1][0]=='-') && (argv[1][1]=='d'))
    {
       sscanf(&(argv[1][2]), "%d", &digits);

       if((digits < 0) || (digits > 9))
       {
         fprintf(stderr, "Digits must be between 0-9");
         digits=0;
       }

       minargc++;
    }
    else
      digits=0;
  }

  infile = argv[minargc];

  if (argc <= minargc)
  {
    printf("Syntax: \n	%s [-d<DIGITS>] input.pvn\n\n", argv[0]);
    printf("DIGITS = number of digits in frame # in filename;\n         if < number of digits in highest frame, it will be ignored!\n\n");
    exit(1);
  }

  exit(pvn2pnm(infile, digits)); 
}
