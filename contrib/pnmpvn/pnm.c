/* pnm.c by Jacob (Jack) Gryn

   PNM (PBM/PGM/PPM) Library

   * this code is (c) 2003,2004 Jacob (Jack) Gryn

   * the author grants full rights to all who wish to use
     and distribute this code and the corresponding file
     formats on the assumption that credit is given to
     the author if asked

   Jacob (Jack) Gryn
 */
#include "pvnglobals.h"
#include "pnm.h"

/* calculates the size of raster data in a PNM file based on a PNMParam 
   header 

   returns # of bytes if P4/P5/P6; 
   or # bytes required to store ascii numbers in P1/P2/P3 in binary
*/
long calcPNMSize(PNMParam p)
{
    switch(p.magic[1])
    {
      /* PBM FILES */
      case '1':
      case '4':
         /* (8 bits per byte + remainder padding bits) * height */
#ifdef DEBUG
printf("height %d, width %d, width/8 is %f, ceil of that is %f, total is %d\n", p.height, p.width, p.width/8.0, ceil(p.width/8.0), p.height*ceil(p.width/8.0));
#endif
        return(p.height*ceil(p.width/8.0));

      /* PGM FILES */
      case '2':
      case '5':
        if(p.maxcolour <= 255)
          return(p.height*p.width);
        else
          return(2*p.height*p.width);

      /* PPM FILES */
      case '3':
      case '6':
        /* RGB * colourdepth (1 or 2 bytes) * height * width) */
        if(p.maxcolour <= 255)
          return(3*p.height*p.width);
        else
          return(3*2*p.height*p.width);
      default:
        return(INVALID);
    }
}

/* Compare two parameter sets; return EQUAL if they are equal,
   or NOTEQUAL if they are not */
int PNMParamCompare(PNMParam first, PNMParam second)
{
  if (strcmp(first.magic, second.magic)!=0)
    return(NOTEQUAL);
  else if(first.width != second.width)
    return(NOTEQUAL);
  else if(first.height != second.height)
    return(NOTEQUAL);
  else if(first.maxcolour != second.maxcolour)
    return(NOTEQUAL);
  else
    return(EQUAL);
}

/* Copy src parameters to dest 
   returns OK or ERROR */
int PNMParamCopy(PNMParam *dest, PNMParam *src)
{
  if ((dest==NULL) || (src == NULL))
  {
    fprintf(stderr, "Pointer Error\n"); 
    return(ERROR);
  }
  else
  {
    strcpy(dest->magic, src->magic);
    dest->width=src->width;
    dest->height=src->height;
    dest->maxcolour=src->maxcolour;
    return(OK);
  }
}

/* Just retrieve the header
   returns  INVALID if it is not a PNM header
            VALID if everything is ok */
int readPNMHeader(FILE *fp, PNMParam *p)
{
  char line[MAX_ASCII_LINE_LENGTH];
  char *tmpStr, tmpMagic[MAX_ASCII_LINE_LENGTH];
  int done = 0;
  long fsize, pos, calcSize;

  p->width=-1; p->height=-1; p->maxcolour=-1; /* clear these vars */

  tmpMagic[0] = 0; /* empty string */

  while(!done)
  {
    /* get a line */
    if (fgets(line, MAX_ASCII_LINE_LENGTH, fp) == NULL)
    {
      fprintf(stderr, "Invalid header!\n"); /* if EOF then bad file */
      return(INVALID);
    }

    /* remove comments */
    if ((tmpStr = (char *)strchr(line,'#')) != NULL)
      tmpStr[0] = 0;

    /* if didn't read magic yet, read EVERYTHING */
    if (tmpMagic[0] == 0)
    {
      sscanf(line, "%s %d %d %d", tmpMagic, &(p->width), &(p->height), &(p->maxcolour));
      if (strlen(tmpMagic) != 2)
      {
        fprintf(stderr, "File Type Magic Number is an invalid length!\n");
        return(INVALID);
      }
      strncpy(p->magic, tmpMagic, 3); /* store 2 bytes + \0 in magic */
    }

    /* set maxcolour to 1 in PBM files (P1/P4) */
    if((p->magic[1] == '1') || (p->magic[1] == '4'))
    {
      p->maxcolour=1;
    }

    /* read remaining variables as they're available */
    if (p->width == -1)
      sscanf(line, "%d %d %d", &(p->width), &(p->height), &(p->maxcolour));
    else if (p->height == -1)
      sscanf(line, "%d %d", &(p->height), &(p->maxcolour));
    else if (p->maxcolour == -1)
      sscanf(line, "%d", &(p->maxcolour));

    if ((p->height != -1) && (p->maxcolour != -1)) /* we're done */
    {
      if ((p->height <= 0) || (p->width <= 0) || (p->maxcolour <= 0))
      {
        fprintf(stderr, "Height, width & maxcolour must be > 0!\n");
        return(INVALID);
      }

      if (p->maxcolour > 65535)
      {
        fprintf(stderr, "Max colour value is invalid; must be between 1 and 65535!\n");
        return(INVALID);         
      }

      done = 1;
    }
  }

  /* if we've got the right magic, then we can continue */
  if (p->magic[0] == 'P')
  {
    switch(p->magic[1])
    {
      case '1':
      case '2':
      case '3':
        return(VALID);
      case '4':
      case '5':
      case '6':
        calcSize=calcPNMSize(*p);
        fsize=filesize(fp);
        pos=ftell(fp);
        if ((fsize - pos) % calcSize == 0)
          return(VALID);
        else
        {
          fprintf(stderr, "File size does not match calculations\nCalc: %ld, Size: %ld", calcSize, fsize-pos);
          return(INVALID);
        }
      default:
        fprintf(stderr, "Only types P1, P2, P3, P4, P5, P6 are supported as input\n");
        return(INVALID);
    }
  }
  else
  { /* otherwise it's a bad file type */
    fprintf(stderr, "Only types P1, P2, P3, P4, P5, P6 are supported\n");
    return(1);
  }
}

/* write PNM Header to file *fp */
int writePNMHeader(FILE *fp, PNMParam p)
{
  int retVal;

  if (p.magic[1] == '4')
    retVal=fprintf(fp, "%s\n%d %d\n", p.magic, p.width, p.height);
  else
    retVal=fprintf(fp, "%s\n%d %d\n%d\n", p.magic, p.width, p.height, p.maxcolour);

  if (retVal==0)
    return(ERROR);
  else
    return(OK);
}

/* display the pnmparam header to stdout */
void showPNMHeader(PNMParam p)
{
  printf("Magic: %s, Width: %d, Height: %d, Maxval: %d\n",
         p.magic, p.width, p.height, p.maxcolour);
}

