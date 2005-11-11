/* pvnglobals.c

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
#include "pvn.h"

/* calculates the size of raster data in a PVN file based on a PVNParam
   header

   returns # of bytes if PV4/PV5/PV6(a/f/d);
*/
long calcPVNSize(PVNParam p)
{
  long pageSize = calcPVNPageSize(p);
  if (pageSize == INVALID)
    return(INVALID);
  else
    return(pageSize*p.depth);
}

/* calculates the size of raster data of a single image within a PVN file
   based on a PVNParam header

   returns # of bytes if PV4/PV5/PV6 (a/f/d);
*/
long calcPVNPageSize(PVNParam p)
{
  int pixel_bytes;

  if (p.magic[3] == 'a')
    pixel_bytes = (int)ceil(p.maxcolour/8.0);
  else if (p.magic[3] == 'b')
    pixel_bytes = (int)ceil(p.maxcolour/8.0);
  else if(p.magic[3] == 'f')
    pixel_bytes = 4;
  else if(p.magic[3] == 'd')
    pixel_bytes = 8;
  else
  {
    fprintf(stderr, "Unknown PVN format type of %s, only a, b, f and d are acceptable\n", p.magic);
    _exit(1);
  }

  switch(p.magic[2])
  {
    /* PVB FILES */
    case '4':
       /* (8 bits per byte + remainder padding bits) * height */
      return(p.height*((long)ceil((p.width)/8.0)));

    /* PVG FILES */
    case '5':
        return(pixel_bytes*p.height*p.width);

    /* PVP FILES */
    case '6':
      /* RGB * colourdepth (1-4 bytes precision) * height * width) */
        return(pixel_bytes*3*p.height*p.width);
    default:
      return(INVALID);
  }
}

/* Compare two parameter sets; return EQUAL if they are equal,
   or NOTEQUAL if they are not */
int PVNParamCompare(PVNParam first, PVNParam second)
{
  if (strcmp(first.magic, second.magic)!=0)
    return(NOTEQUAL);
  else if(first.width != second.width)
    return(NOTEQUAL);
  else if(first.height != second.height)
    return(NOTEQUAL);
  else if(first.depth != second.depth)
    return(NOTEQUAL);
  else if(first.maxcolour != second.maxcolour)
    return(NOTEQUAL);
  else if(first.framerate != second.framerate)
    return(NOTEQUAL);
  else
    return(EQUAL);
}

/* Copy src parameters to dest
   returns OK or ERROR */
int PVNParamCopy(PVNParam *dest, PVNParam *src)
{
  if ((dest==NULL) || (src == NULL))
  {
    fprintf(stderr, "Pointer Error\n");
    return(ERROR);
  }
  else
  {
    strlcpy(dest->magic, src->magic, 5);
    dest->width=src->width;
    dest->height=src->height;
    dest->depth=src->depth;
    dest->maxcolour=src->maxcolour;
    dest->framerate=src->framerate;
    return(OK);
  }
}

/* write PVN Header to file *fp */
int writePVNHeader(FILE *fp, PVNParam p)
{
#ifdef DEBUG
  printf("Writing Header . . .\n");
  showPVNHeader(p);
#endif
  if(fprintf(fp, "%s\n%d %d %d\n%f %f\n",
     p.magic, p.width, p.height, p.depth,
     p.maxcolour, p.framerate) == 0)
    return(ERROR);
  else
    return(OK);
}

/* display the pvnparam header to stdout */
void showPVNHeader(PVNParam p)
{
  printf("Magic: %s, Width: %d, Height: %d, Depth: %d, Framerate: %f Maxval: %f\n",
         p.magic, p.width, p.height, p.depth, p.framerate, p.maxcolour);
}

/* Just retrieve the header
   returns  INVALID if it is not a PNM header
            VALID if everything is ok */
int readPVNHeader(FILE *fp, PVNParam *p)
{
  char line[MAX_ASCII_LINE_LENGTH];
  char *tmpStr, tmpMagic[MAX_ASCII_LINE_LENGTH];
  int done = 0;
  long fsize, pos, calcSize;

  p->width=-1; p->height=-1; p->maxcolour=-1;
  p->depth=-1; p->framerate=UNDEF_FRAMERATE; /* clear these vars */

  tmpMagic[0] = 0; /* empty string */

#ifdef DEBUG
  printf("Reading Header . . .\n");
#endif
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
      sscanf(line, "%s %d %d %d %lf %lf", tmpMagic, &(p->width), &(p->height), &(p->depth), &(p->maxcolour), &(p->framerate));
      if (strlen(tmpMagic) != 4)
      {
        fprintf(stderr, "File Type Magic Number is an invalid length!\n");
        return(INVALID);
      }
      strncpy(p->magic, tmpMagic, 5); /* store 2 bytes + \0 in magic */
    }

    /* read remaining variables as they're available */
    else if (p->width == -1)
      sscanf(line, "%d %d %d %lf %lf", &(p->width), &(p->height), &(p->depth), &(p->maxcolour), &(p->framerate));
    else if (p->height == -1)
      sscanf(line, "%d %d %lf %lf", &(p->height), &(p->depth), &(p->maxcolour), &(p->framerate));
    else if (p->depth == -1)
      sscanf(line, "%d %lf %lf", &(p->depth), &(p->maxcolour), &(p->framerate));
    else if (p->maxcolour == -1)
      sscanf(line, "%lf %lf", &(p->maxcolour), &(p->framerate));
    else if (p->framerate == -1)
      sscanf(line, "%lf", &(p->framerate));

    if (p->framerate != UNDEF_FRAMERATE) /* we're done */
    {
      if ((p->height <= 0) || (p->width <= 0) || (p->depth < 0))
      {
        fprintf(stderr, "Height & width must be > 0, depth must be >= 0!\n");
        return(INVALID);
      }


      /* bitmapped versions cannot be float or double format ! */
      if ((p->magic[2] == '4') && (p->magic[3] != 'a'))
      {
        fprintf(stderr, "Bitmap PV4x files must be in unsigned integer format!\n");
        return(INVALID);
      }

      /* if we have a bitmapped version, then maxcolour must be 1 */
      if (p->magic[2] == '4')
      {
        if(p->maxcolour != 1)
        {
          fprintf(stderr, "Bitmap PV4x files must have a colour depth of 1!\n");
          return(INVALID);
        }
      }
      else if (((p->magic[3] == 'a') || (p->magic[3] == 'b')) && (((int)p->maxcolour % 8 != 0) || (p->maxcolour > 32) || (p->maxcolour==0)))
      {
        fprintf(stderr, "Max colour depth of %f is invalid; must be a multiple of 8 bits (max 32)!\n", p->maxcolour);
        return(INVALID);
      }

      done = 1;
    }
  }

#ifdef DEBUG
  showPVNHeader(*p);
#endif

  /* if we've got the right magic, then we can continue */
  if ((p->magic[0] == 'P') && (p->magic[1] == 'V') && ((p->magic[3] == 'a') || (p->magic[3] == 'b') || (p->magic[3]=='f') || (p->magic[3]=='d')))
  {
    switch(p->magic[2])
    {
      case '1':
      case '2':
      case '3':
        fprintf(stderr, "ASCII/'plain' PVN/PVB/PVG/PVP files are not supported!\n");
        return(INVALID);
      case '4':
      case '5':
      case '6':
        calcSize=calcPVNSize(*p);
        fsize=filesize(fp);
        pos=ftell(fp);
        if ((fsize - pos) == calcSize)
          return(VALID);
        else if (p->depth == 0) // ok if this is a streaming file
          return(VALID);
        else
        {
          fprintf(stderr, "File size does not match calculations\nCalc: %ld, Size: %ld", calcSize, fsize-pos);
          return(INVALID);
        }
      default:
        fprintf(stderr, "Only types PV4, PV5, PV6 are supported as input\n");
        return(INVALID);
    }
  }
  else
  { /* otherwise it's a bad file type */
    fprintf(stderr, "Only types PV4, PV5, PV6 are supported\n");
    return(1);
  }
}

/* converts a PVN to another format of PVN

   infile / outfile = in/output filenames
   framerate = new framerate; if framerate = UNDEF_FRAMERATE, use input framerate
   format = new format
   maxcolour = new maxcolour / range value (for floats/doubles)
 */
int pvnconvert(const char *infile, const char *outfile, double framerate, unsigned int format, double maxcolour)
{
//  unsigned int i;
  int retVal;
  PVNParam inParams;
  PVNParam outParams;
  FILE *in, *out;
  long inCalcSize, outCalcSize;
  unsigned char *inbuf, *outbuf;
  unsigned int inFormat;

  /* check for valid format */
  if ((format != FORMAT_FLOAT) && (format!= FORMAT_DOUBLE) && (format != FORMAT_INT) && (format != FORMAT_UINT))
  {
    fprintf(stderr, "Invalid output format!\n");
    return(ERROR);
  }

  if (((format == FORMAT_INT) || (format == FORMAT_UINT)) && ((maxcolour > 32) || (maxcolour <= 0) || ((int)maxcolour % 8 != 0)))
  {
    fprintf(stderr, "Invalid maxcolour value, must be multiple of 8, and a max of 32!\n");
    return(ERROR);
  }

  if ((format != FORMAT_INT) && (format != FORMAT_UINT) && (maxcolour <= 0))
  {
    fprintf(stderr, "Invalid max range value, must be > 0!\n");
    return(ERROR);
  }

// now that we're using doubles; and allowing negative numbers; UNDEF_FRAMERATE is the special case
//  if (framerate < -1) // -1 is ok, that means use whatever is in the input
/*  {
    fprintf(stderr, "Invalid frame rate, must be >= 0\n");
    return(ERROR);
  } */

#ifdef DEBUG
  printf("Opening and verifying %s:\n\n", infile);
#endif

  if ((in = fopen(infile, "rb")) == NULL)
  {
      fprintf(stderr, "Error opening file %s for read\n", infile);
      _exit(OPENERROR);
  }

  if (readPVNHeader(in, &inParams) != VALID)
  {
    return(ERROR);
  }

  PVNParamCopy(&outParams, &inParams);

  if(inParams.magic[3] == 'a')
    inFormat = FORMAT_UINT;
  else if(inParams.magic[3] == 'b')
    inFormat = FORMAT_INT;
  else if(inParams.magic[3] == 'f')
    inFormat = FORMAT_FLOAT;
  else if(inParams.magic[3] == 'd')
    inFormat = FORMAT_DOUBLE;
  else
  {
    fprintf(stderr, "Unknown PVN format type, only a, b, f and d are acceptable\n");
    _exit(1);
  }

  if(inParams.magic[2] == '4')
  {
    outParams.magic[2] = '5';
    inFormat=FORMAT_BIT;
  }

  if(framerate==UNDEF_FRAMERATE)
    framerate=inParams.framerate;

  if(format == FORMAT_UINT)
    outParams.magic[3]='a';  /* PVx already exists before the a from the copy */
  else if(format == FORMAT_INT)
    outParams.magic[3]='b';
  else if(format == FORMAT_FLOAT)
    outParams.magic[3]='f';
  else if(format == FORMAT_DOUBLE)
    outParams.magic[3]='d';

  outParams.maxcolour = maxcolour;
  outParams.framerate = framerate;


#ifdef DEBUG
  printf("Creating %s\n", outfile);
#endif

  if ((out = fopen(outfile, "wb")) == NULL)
  {
    fprintf(stderr, "Error opening file %s for writing\n", outfile);
    _exit(OPENERROR);
  }

  if(writePVNHeader(out, outParams) != OK)
  {
    fprintf(stderr, "Error writing header information\n");
    _exit(OPENERROR);
  }

  inCalcSize=calcPVNPageSize(inParams);
  outCalcSize=calcPVNPageSize(outParams);

  inbuf=(unsigned char *)malloc(inCalcSize);
  outbuf=(unsigned char *)malloc(outCalcSize);

//  for(i=0; i < inParams.depth; i++)
  while(fread(inbuf, inCalcSize, 1, in) != 0)
  {
    if((inFormat == format) && (inParams.maxcolour == outParams.maxcolour))
    {
#ifdef DEBUG
      printf("Only modifying header!\n");
#endif
      if(bufCopy(inbuf,inCalcSize,outbuf,outCalcSize) != OK)
      {
        fprintf(stderr, "Error copying buffers!\n");
        fclose(out);
        remove(outfile);
        free(inbuf);
        free(outbuf);
        return(ERROR);
      }
    }
    else
    {
      // for PV4a, since bufConvert needs width parameter passed in maxcolour, set it here.
      if(inParams.magic[2] == '4')
        inParams.maxcolour = inParams.width;
      retVal=bufConvert(inbuf, inCalcSize, inFormat, inParams.maxcolour, outbuf, outCalcSize, format, outParams.maxcolour);
      if (retVal==ERROR)
      {
        fprintf(stderr, "Buffer conversion error!\n");
        fclose(out);
        remove(outfile);
        free(inbuf);
        free(outbuf);
        return(ERROR);
      }
    }

    retVal=(int)fwrite(outbuf, outCalcSize, 1, out);
    if (retVal == 0)
    {
      fclose(out);
      remove(outfile);
      free(inbuf);
      free(outbuf);
      return(ERROR);
    }
  }

  fclose(out);
#ifdef DEBUG
  printf("...done\n");
#endif
  free(inbuf);
  free(outbuf);
  return(OK);
}

