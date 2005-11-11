/* pvnconvert by Jacob (Jack) Gryn
   - changes the format of a .PVN (PVB/PVG/PVP) file

   * the PVN (PVB/PVG/PVP) file format, and this code
     is (c) 2003,2004 Jacob (Jack) Gryn

   * the author grants full rights to all who wish to use
     and distribute this code and the corresponding file
     formats on the assumption that credit is given to
     the author if asked

   Jacob (Jack) Gryn
 */
#include "pvnglobals.h"
#include "pvn.h"

#define PVNCONVERT_VERSION "0.2"

int main(int argc, char *argv[])
{
  const char *infile, *outfile;
  double framerate = UNDEF_FRAMERATE;
  int i;
  int minargc=2;
  int format=FORMAT_UNCHANGED;
  double maxc=1;

  printf("pvnconvert v%s by Jacob (Jack) Gryn\n\n", PVNCONVERT_VERSION);

  /* find the first entry that is not an 'option' */
  for (i=1; i < argc; i++)
  {
    if (argv[i][0] != '-')
    {
      minargc=i+1;
      break;
    }
    else if (argv[i][1]=='r')
    {
       sscanf(&(argv[i][2]), "%lf", &framerate);
/*       if (framerate < 0)
       {
         fprintf(stderr, "Setting frame rate to 0/undefined!\n");
         framerate=0;
       } */
    }
    else if (argv[i][1]=='f')
    {
      format=FORMAT_FLOAT;
      sscanf(&(argv[i][2]), "%lf", &maxc);
      if (maxc <= 0)
      {
        fprintf(stderr, "Range value must be >= 0!\n");
        exit(1);
      }
    }
    else if (argv[i][1]=='d')
    {
      format=FORMAT_DOUBLE;
      sscanf(&(argv[i][2]), "%lf", &maxc);
      if (maxc <= 0)
      {
        fprintf(stderr, "Range value must be >= 0!\n");
        exit(1);
      }
    }
    else if (argv[i][1]=='i')
    {
      format=FORMAT_INT;
      sscanf(&(argv[i][2]), "%lf", &maxc);
      if ((maxc < 0) || ((int)maxc % 8 != 0) || (maxc > 32))
      {
        fprintf(stderr, "Invalid integer format, setting to INT_8!\n - Must be multiple of 8, and max 32\n");
        maxc=8;
      }
    }
    else if (argv[i][1]=='u')
    {
      format=FORMAT_UINT;
      sscanf(&(argv[i][2]), "%lf", &maxc);
      if ((maxc < 0) || ((int)maxc % 8 != 0) || (maxc > 32))
      {
        fprintf(stderr, "Invalid integer format, setting to UINT_8!\n - Must be multiple of 8, and max 32\n");
        maxc=8;
      }
    }
  }

  infile = argv[minargc-1];
  outfile = argv[minargc];

  if ((argc <= minargc) || (format == FORMAT_UNCHANGED))
  {
    printf("Syntax: \n %s [-r<framerate>] -d<range>|-f<range>|-i<bits>|-u<bits> input.pvn output.pvn\n\n", argv[0]);
    exit(1);
  }

  exit(pvnconvert(infile, outfile, framerate, format, maxc));
}
