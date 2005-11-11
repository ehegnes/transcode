/* pnm2pvn by Jacob (Jack) Gryn
   - Opens a list of PNM files and saves them in PVN format
   - Syntax: pnm2pvn output.pvn *.pnm
   - *.pnm refers to all input files, entered in order that they are to appear
     in PVN file

   PVN (PVB/PVG/PVP) Library

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

#define PNM2PVN_VERSION "0.1"

int main(int argc, char *argv[])
{
  char *outfile;
  int infiles_count;
  const char **infiles;
  double framerate = 0;
  int i;
  int minargc=2;
  int format=FORMAT_UNCHANGED;
  double maxcolour=0;
  int copies=0;

  printf("pnm2pvn v%s by Jacob (Jack) Gryn\n\n", PNM2PVN_VERSION);

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
// we are now allowing negative framerates
/*       if (framerate < 0)
       {
         fprintf(stderr, "Setting frame rate to 0/undefined!\n");
         framerate=0;
       } */
    }
    else if (argv[i][1]=='f')
    {
      format=FORMAT_FLOAT;
      sscanf(&(argv[i][2]), "%lf", &maxcolour);
      if (maxcolour <= 0)
      {
        fprintf(stderr, "Range value must be >= 0!\n");
        exit(1);
      }
    }
    else if (argv[i][1]=='d')
    {
      format=FORMAT_DOUBLE;
      sscanf(&(argv[i][2]), "%lf", &maxcolour);
      if (maxcolour <= 0)
      {
        fprintf(stderr, "Range value must be >= 0!\n");
        exit(1);
      }
    }
    else if (argv[i][1]=='u')
    {
      format=FORMAT_UINT;
      sscanf(&(argv[i][2]), "%lf", &maxcolour);
      if ((maxcolour < 0) || ((int)maxcolour % 8 != 0) || (maxcolour > 32))
      {
        fprintf(stderr, "Invalid integer format, setting to UINT_8!\n - Must be multiple of 8, and max 32\n");
        maxcolour=8;
      }
    }
    else if (argv[i][1]=='i')
    {
      format=FORMAT_INT;
      sscanf(&(argv[i][2]), "%lf", &maxcolour);
      if ((maxcolour < 0) || ((int)maxcolour % 8 != 0) || (maxcolour > 32))
      {
        fprintf(stderr, "Invalid integer format, setting to INT_8!\n - Must be multiple of 8, and max 32\n");
        maxcolour=8;
      }
    }
    else if (argv[i][1]=='c')
    {
      sscanf(&(argv[i][2]), "%d", &copies);
      if (copies <= 1)
      {
        fprintf(stderr, "# of copies must be > 1\n");
        exit(1);
      }
    }
  }


  outfile = argv[minargc-1];
  infiles_count = argc - minargc;
  if ((copies > 1) && (infiles_count != 1))
  {
    fprintf(stderr, "-cXX switch only works with one input file\n");
    exit(1);
  }

  infiles = (const char **)&argv[minargc];

  if (argc <= minargc)
  {
    printf("Syntax: \n %s [-r<framerate>] [-c<copies>] [-d<range>|-f<range>|-i<bits>|-u<bits>] output.pvn *.pnm\n\n", argv[0]);
    exit(1);
  }

  printf("Converting %d files into PVN file: %s\n", infiles_count, outfile);

  exit(pnm2pvn(infiles, infiles_count, outfile, framerate, format, maxcolour, copies));
}
