/* pnmpvn.c by Jacob (Jack) Gryn

   PNM (PBM/PGM/PPM) and PVN (PVB/PVG/PBP) conversion Library

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
#include "pnm.h"
#include "pvn.h"

/* verify headers in all PNM files are accurate

   char **infiles = list of files
   int infiles_count = # of files in list

   returns VALID or (INVALID, OPENERROR or ERROR)
 */
int checkPNMHeaders(const char **infiles, int infiles_count)
{
  PNMParam first, cur;
  FILE *fp;
  int i, retVal;

#ifdef DEBUG
  printf("Verifying %d file headers:\n\n", infiles_count);
#endif

  /* This for loop will check all the files are properly formatted and the headers match */

  for (i=0; i < infiles_count; i++)
  {
#ifdef DEBUG
    printf("Checking file %s:  ", infiles[i]);
    fflush(stdout);
#endif
    if ((fp = fopen(infiles[i], "rb")) == NULL)
    {
      fprintf(stderr, "Error opening file %s for read\n", infiles[i]);
      return(OPENERROR);
    }

    retVal = readPNMHeader(fp, &cur);
    fclose(fp);

    if (retVal != VALID)
    {
      return(retVal);
    }

    if (i==0)
    {
      if (PNMParamCopy(&first, &cur) != VALID)
      {
        return(ERROR);
      }
    }
    else
    {
      if (PNMParamCompare(first, cur) != EQUAL)
      {
        fprintf(stderr, "Header parameters for file: %s does not match %s!\nPlease ensure that all images are in the same format and have the same pixel and colour dimensions!\n", infiles[i], infiles[0]);
        return(ERROR);
      }
    }
#ifdef DEBUG
    printf("OK\n");
#endif
  }
#ifdef DEBUG
  printf("Headers have been verified\n");
#endif
  return(VALID);
}

/* converts pvn to multiple pnm files

   infile = input filename
   digits = # of digits in index in filename
            ie.  digits = 3, would give a filename img000.pgm
 */
int pvn2pnm(const char *infile, unsigned int digits)
{
  int i, retVal;
  PVNParam inParams;
  PNMParam outParams;
  FILE *in, *out;
  long inCalcSize, outCalcSize;
  unsigned char *inbuf, *outbuf;
  char *tmpStr;
  char outfile[MAX_FILENAME_LENGTH];
  char suffix[5];
  char prefix[strlen(infile)];

#ifdef DEBUG
  printf("Opening and verifying %s:\n\n", infile);
#endif

  if ((in = fopen(infile, "rb")) == NULL)
  {
      fprintf(stderr, "Error opening file %s for read\n", infile);
      exit(OPENERROR);    
  }

  if (readPVNHeader(in, &inParams) != VALID)
  {
    return(ERROR);
  }

  strcpy(prefix, infile);
  if ((tmpStr = (char *)strrchr(prefix,'.')) != NULL)
    tmpStr[0] = 0;

  switch(inParams.magic[2])
  {
    case '4': 
      strcpy(outParams.magic, "P4");
      strcpy(suffix, ".pbm");
      break;
    case '5': 
      strcpy(outParams.magic, "P5");
      strcpy(suffix, ".pgm");
      break;
    case '6': 
      strcpy(outParams.magic, "P6");
      strcpy(suffix, ".ppm");
      break;
  }

  outParams.width=inParams.width;
  outParams.height=inParams.height;

  if(inParams.magic[3] == 'a')
  {
    if(inParams.magic[2] == '4')
      outParams.maxcolour=1;
    else if(inParams.maxcolour >= 16)
      outParams.maxcolour=65535;
    else
      outParams.maxcolour=255;
  }
  else if(inParams.magic[3] == 'b')
  {
    if(inParams.maxcolour >= 16)
      outParams.maxcolour=65535;
    else
      outParams.maxcolour=255;
  }
  else /* for floats & doubles */
    outParams.maxcolour=65535;

  inCalcSize=calcPVNPageSize(inParams);
  outCalcSize=calcPNMSize(outParams);

  inbuf=(unsigned char *)malloc(inCalcSize);
  outbuf=(unsigned char *)malloc(outCalcSize);

  if (digits < (1+(unsigned int)floor(log10(inParams.depth))))
  {
#ifdef DEBUG
    printf("Setting # of digits in frame number to %d!\n", (unsigned int)ceil(log10(inParams.depth)));
#endif
    digits=(unsigned int)ceil(log10(inParams.depth));
  }
  if (digits==0) // this may be the case for streaming files
    digits=5;
//  for(i=0; i < inParams.depth; i++)
  i=0;
  while(fread(inbuf, inCalcSize, 1, in) != 0)
  {
    if(genFileName(prefix, suffix, outfile, i, digits) != OK)
    {
      fprintf(stderr, "Error generating filename for output!\n");
      free(inbuf);
      free(outbuf);
      return(ERROR);
    }
#ifdef DEBUG
    printf("Creating %s\n", outfile);
#endif

    if ((out = fopen(outfile, "wb")) == NULL)
    {
      fprintf(stderr, "Error opening file %s for writing\n", outfile);
      exit(OPENERROR);
    }

    if(writePNMHeader(out, outParams) != OK)
    {
      fprintf(stderr, "Error writing header\n");
      exit(OPENERROR);
    }

    if (inParams.magic[3] == 'a')
    {
      if((inParams.magic[2] == '4') || (inParams.maxcolour == ceil(log(outParams.maxcolour)/log(2))))
      {
        if(bufCopy(inbuf,inCalcSize,outbuf,outCalcSize) != OK)
        {
          fprintf(stderr, "Error copying buffers!\n");
          fclose(out);
          free(inbuf);
          free(outbuf);
          return(ERROR);
        }
      }
      else
      {
        retVal=changeBufPrecision(inbuf, inCalcSize, outbuf, outCalcSize, inParams.maxcolour, log(outParams.maxcolour+1)/log(2));
        if (retVal==ERROR)
        {
          fprintf(stderr, "Error converting buffers!\n");
          fclose(out);
          free(inbuf);
          free(outbuf);
          return(ERROR);
        }
      }
    }
    else if (inParams.magic[3] == 'b')
    {
      retVal=bufConvert(inbuf, inCalcSize, FORMAT_INT, inParams.maxcolour, outbuf, outCalcSize, FORMAT_UINT, log(outParams.maxcolour+1)/log(2));
      if (retVal==ERROR)
      {
        fprintf(stderr, "Error converting buffers!\n");
        fclose(out);
        free(inbuf);
        free(outbuf);
        return(ERROR);
      }
    }
    else if (inParams.magic[3] == 'f')
    {
      retVal=bufConvert(inbuf, inCalcSize, FORMAT_FLOAT, inParams.maxcolour, outbuf, outCalcSize, FORMAT_UINT, log(outParams.maxcolour+1)/log(2));
      if (retVal==ERROR)
      {
        fclose(out);
        free(inbuf);
        free(outbuf);
        return(ERROR);
      }
    }
    else if (inParams.magic[3] == 'd')
    {
      retVal=bufConvert(inbuf, inCalcSize, FORMAT_DOUBLE, inParams.maxcolour, outbuf, outCalcSize, FORMAT_UINT, log(outParams.maxcolour+1)/log(2));
      if (retVal==ERROR)
      {
        fclose(out);
        free(inbuf);
        free(outbuf);
        return(ERROR);
      }
    }

    retVal=fwrite(outbuf, outCalcSize, 1, out);
    if (retVal == 0)
    {
      fclose(out);
      free(inbuf);
      free(outbuf);
      return(ERROR);
    }
    fclose(out);
    i++; // new
  }
#ifdef DEBUG
  printf("...done\n");
#endif
  free(inbuf);
  free(outbuf);
  return(OK);
}


/* 
   - Opens a list of PGM/PPM/PNM files and saves them in PVG/PVP/PVN format

   infiles = list of input files
   infiles_count = # of files in list
   outfile = output filename
   framerate = framerate
   copies = # of copies to make (assuming 1 infiles_count)
*/
int pnm2pvn(const char **infiles, int infiles_count, const char *outfile, double framerate, unsigned int format, double maxcolour, unsigned int copies)
{
  int i, j, retVal;
  PNMParam first, cur;
  PVNParam outParams;
  FILE *out, *fp;
  long fsize, pos, calcSize, calcOutSize;
  unsigned char *inbuf, *outbuf;

  if((copies > 1) && (infiles_count != 1))
  {
    fprintf(stderr, "Copies can only be > 0 if infiles is == 1\n");
    return(ERROR);
  }
  else if (copies < 1)
    copies=1;

  if (format == FORMAT_UNCHANGED)
    format = FORMAT_UINT;

  /* check for valid format */
  if ((format != FORMAT_FLOAT) && (format!= FORMAT_DOUBLE) && (format != FORMAT_INT) && (format != FORMAT_UINT))
  {
    fprintf(stderr, "Invalid output format!\n");
    return(ERROR);
  }

  if (((format == FORMAT_INT) || (format == FORMAT_UINT)) && ((maxcolour > 32) || (maxcolour < 0) || ((int)maxcolour % 8 != 0)))
  {
    fprintf(stderr, "Invalid maxcolour value, must be multiple of 8, and a max of 32!\n");
    return(ERROR);
  }

  if ((format != FORMAT_INT) && (format != FORMAT_UINT) && (maxcolour <= 0))
  {
    fprintf(stderr, "Invalid max range value, must be > 0!\n");
    return(ERROR);
  }

  if (checkPNMHeaders(infiles, infiles_count) != VALID)
  {
    return(ERROR);
  }

  if ((out = fopen(outfile, "wb")) == NULL)
  {
      fprintf(stderr, "Error opening file %s for write\n", outfile);
      exit(OPENERROR);    
  }

#ifdef DEBUG
  printf("Writing output to %s:\n\n", outfile);
#endif

  /* Now that everything matches, we can start processing! */  
  for(i=0; i < infiles_count; i++)
  {
    if ((fp = fopen(infiles[i], "rb")) == NULL)
    {
      fprintf(stderr, "Error opening file %s for read\n", infiles[i]);
      fclose(out);
      remove(outfile);
      return(OPENERROR);
    }
    retVal = readPNMHeader(fp, &cur);

    if (retVal != VALID)
    {
      fclose(fp);
      fclose(out);
      remove(outfile);
      return(retVal);
    }

    if (i==0)
    {
      if (PNMParamCopy(&first, &cur) != OK)
      {
        fclose(fp);
        fclose(out);
        remove(outfile);
        return(ERROR);
      }

      /* set magic #'s for output, we will not use ASCII versions of output */
      switch(first.magic[1])
      {
        /* PBM FILES */
        case '1':
        case '4':
          if ((format == FORMAT_FLOAT) || (format == FORMAT_DOUBLE))
          {
            fprintf(stderr, "PBM files cannot be converted to floats/doubles!\n");
            fclose(fp);
            fclose(out);
            remove(outfile);
            return(ERROR);
          }
          else
            strcpy(outParams.magic, "PV4a");
          break;

        /* PGM FILES */
        case '2':
        case '5':
          if (format == FORMAT_FLOAT)
            strcpy(outParams.magic, "PV5f");
          else if (format == FORMAT_DOUBLE)
            strcpy(outParams.magic, "PV5d");
          else
            strcpy(outParams.magic, "PV5a");
          break;

        /* PPM FILES */
        case '3':
        case '6':
          if (format == FORMAT_FLOAT)
            strcpy(outParams.magic, "PV6f");
          else if (format == FORMAT_DOUBLE)
            strcpy(outParams.magic, "PV6d");
          else
            strcpy(outParams.magic, "PV6a");
          break;
      }
      outParams.width=first.width;
      outParams.height=first.height;
      outParams.depth=infiles_count;
      if((infiles_count == 1) && (copies > 1))
        outParams.depth=copies;

      if((first.magic[1]=='1') || (first.magic[1] == '4'))
        outParams.maxcolour=1;
      else if ((maxcolour == 0) && ((format == FORMAT_INT) || (format == FORMAT_UINT)))
        outParams.maxcolour=ceil(log(first.maxcolour)/log(2));
      else
        outParams.maxcolour=maxcolour;
      outParams.framerate=framerate;
      if(writePVNHeader(out,outParams) != OK)
      {
        fclose(fp);
        fclose(out);
        remove(outfile);
        return(ERROR);
      }
    }
    else
    {
      if (PNMParamCompare(first, cur) != EQUAL)
      {
        fprintf(stderr, "Header parameters for file: %s does not match %s!\nPlease ensure that all images are in the same format and have the same pixel and colour dimensions!\n", infiles[i], infiles[0]);
        fclose(fp);
        fclose(out);
        remove(outfile);
        return(ERROR);
      }
    }    
    calcSize=calcPNMSize(cur);
    fsize=filesize(fp);
    pos=ftell(fp);
    inbuf=(unsigned char *)malloc(calcSize);
    
    switch(cur.magic[1])
    {
      /* ASCII FILES */
      case '1':
        /* remember, we must read 8 bits for each byte */
        retVal=asciiRead(inbuf, calcSize*8, fp, cur.maxcolour);

        if (retVal != OK)
        {
          fclose(fp);
          fclose(out);
          remove(outfile);
          free(inbuf);
          return(ERROR);
        }
        break;

      case '2':
      case '3':
        /* 16 bit per colour-pixel uses 2 bytes, but we need to pass it 
           a param in # of integers, not bytes */
        if (cur.maxcolour > 255)
          retVal=asciiRead(inbuf, calcSize/2, fp, cur.maxcolour);
        else
          retVal=asciiRead(inbuf, calcSize, fp, cur.maxcolour);

        if (retVal != OK)
        {
          fclose(fp);
          fclose(out);
          remove(outfile);
          free(inbuf);
          return(ERROR);
        }
        break;

      /* BINARY FILES */
      case '4':
      case '5':
      case '6':
        retVal=fread(inbuf, calcSize, 1, fp);
        if (retVal == 0)
        {
          fclose(fp);
          fclose(out);
          remove(outfile);
          free(inbuf);
          return(ERROR);
        }
        break;
    }

    calcOutSize=calcPVNPageSize(outParams);
    outbuf=(unsigned char *)malloc(calcOutSize);

    if(((first.magic[1]=='1')||(first.magic[1]=='4')) || (ceil(log(first.maxcolour)/log(2)) == maxcolour))
    {
      if(bufCopy(inbuf,calcSize,outbuf,calcOutSize) != OK)
      {
        fprintf(stderr, "Error copying buffers!\n");
        fclose(fp);
        fclose(out);
        remove(outfile);
        free(inbuf);
        free(outbuf);
        return(ERROR);
      }
    }
    else
    {
      if (bufConvert(inbuf,calcSize,FORMAT_UINT,ceil(log(first.maxcolour)/log(2)),outbuf,calcOutSize,format,maxcolour) != OK)
      {
        fprintf(stderr, "Error converting buffers!\n");
        fclose(fp);
        fclose(out);
        remove(outfile);
        free(inbuf);
        free(outbuf);
        return(ERROR);
      }
    }

    for(j=0; j < copies; j++)
    {
      retVal=fwrite(outbuf, calcOutSize, 1, out);
      if (retVal == 0)
      {
        fclose(fp);
        fclose(out);
        remove(outfile);
        free(inbuf);
        free(outbuf);
        return(ERROR);
      }
    }
    free(inbuf);
    free(outbuf);
    fclose(fp);
  }
#ifdef DEBUG
  printf("...done\n");
#endif
  return(OK);
}
