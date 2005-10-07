/* pvnglobals.c

   global definitions & functions used in PVN & PNM libraries

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

/* get the file size of file pointer *fp (must be open) */
long filesize(FILE *fp)
{
  long fpos, fsize;

  if (fp == NULL)
    return(ERROR);

  if ((fpos=ftell(fp))==-1)
  {
    return(ERROR);
  }

  if (fseek(fp,0,SEEK_END)==-1)
  {
    return(ERROR);
  }
  
  if ((fsize=ftell(fp))==-1)
  {
    return(ERROR);
  }  

  if (fseek(fp,fpos,SEEK_SET)==-1)
  {
    return(ERROR);
  }
  return(fsize);  
}

/* read in 'count' unsigned integers of range 0-maxcolour and place values
   in buffer 'buf' from file pointer fp */
int asciiRead(unsigned char *buf, unsigned int count, FILE *fp, unsigned int maxcolour)
{
  unsigned int i;
  int retVal;
  int toRead;

  if (maxcolour > 65535)
  {
    fprintf(stderr, "24+ bit sample files are not supported in ASCII mode!\n");
  }

  for(i=0; i < count; i++)
  {
    retVal=fscanf(fp, "%d", &toRead);
    if ((retVal == 0) || (retVal == EOF))
    {
       fprintf(stderr, "Error reading ASCII value from file!\n");
       return(ERROR);
    }

    if((toRead < 0) || ((unsigned int)toRead > maxcolour))
    {
       fprintf(stderr, "ASCII value is out of range!\n");
       return(ERROR);
    }
    
    if (maxcolour == 1)
    {
      buf[i/8] = buf[i/8] << 1;
      buf[i/8] += toRead;
    }
    else if (maxcolour > 255)
    {
      buf[i*2]=toRead / 256;
      buf[(i*2)+1]=toRead % 256;
    }
    else
      buf[i]=toRead;
  }
  return(OK);
}

/* generate a numerically-indexed filename
   prefix = prefix of filename
   suffix = suffix / file extension
   filename = buffer to store output filename string
   val = index value
   digits = # of digits to use in index part of filename
 */
int genFileName(const char *prefix, const char *suffix, char filename[MAX_FILENAME_LENGTH], unsigned int val, unsigned int digits)
{
  /* PROBLEM IF log10(val) == an exact # */
#if 0
  unsigned int i;
#else
  char *digits_buf;
  char format_buf[10];
#endif
  unsigned int used_digits;
  if (val > 0)
    used_digits = 1 + (unsigned int)floor(log10((double)val));
  else
    used_digits = 1;

  if (digits < (1+(unsigned int)floor(log10((double)val))))
  {
    if(val == 0)
    {
      if (digits == 0)
        digits=1;
    }
    else
      digits=(unsigned int)ceil(log10((double)val));
  }

  if(digits + strlen(prefix) + strlen(suffix) > MAX_FILENAME_LENGTH)
  {
    fprintf(stderr, "Filename would be greater than the max filename length!\n");
    return(ERROR);
  }

  strlcpy(filename, prefix, MAX_FILENAME_LENGTH);

#if 0
  for (i=0; i < (digits-used_digits); i++)
  {
    strlcat(filename, "0", MAX_FILENAME_LENGTH);
  }

  sprintf(&filename[strlen(filename)], "%d%s", val,suffix);
#else
  snprintf(format_buf, sizeof(format_buf), "%%0%dd", digits);
  if ((digits_buf = malloc(digits + 1)) == NULL) {
    fprintf(stderr, "Could not allocate memory for digits_buf\n");
    return(ERROR);
  }
  snprintf(digits_buf, digits + 1, format_buf, val);
  strlcat(filename, digits_buf, MAX_FILENAME_LENGTH);
  strlcat(filename, suffix, MAX_FILENAME_LENGTH);
  free(digits_buf);
#endif

  return(OK);
}

/* takes a buffer, inbuf, with precision 'input_prec' bits (multiple of 8 bits)
         and size inbufsize
   and converts it to precision 'output_prec' (also multiple of 8 bits) in outbuf
         (with outbufsize)

   inbufsize/outbuf is the current size (in bytes) of the respective buffer.

   inbuf/outbuf are stored with most significant byte first

   returns OK or ERROR
*/
int changeBufPrecision(unsigned char *inbuf, unsigned long inbufsize, unsigned char *outbuf, unsigned long outbufsize, unsigned int input_prec, unsigned int output_prec)
{
  int input_bytes, output_bytes, byte_num, j;
  unsigned long i, outputPtr=0;

  if (output_prec == 0)
    output_prec = input_prec;
  /* must be multiple of 8 and > 0 */
  if (((input_prec % 8) != 0)||((output_prec % 8) != 0) || (input_prec == 0) || (output_prec == 0))
  {
    fprintf(stderr, "Precision is not multiple of 8!\n");
    return(ERROR);
  }

  if ((inbuf == NULL) || (outbuf == NULL))
  {
    fprintf(stderr, "A buffer is NULL!\n");
    return(ERROR);
  }

  input_bytes = input_prec / 8;
  output_bytes = output_prec / 8;

  for(i=0; i < inbufsize; i++)
  {
    byte_num = i % input_bytes;

    /* copy bytes until we reach max outbyte bytes/pixel; 
       if output bytes are less than input bytes, then ignore the rest */
    if(byte_num < output_bytes)
      outbuf[outputPtr++]=inbuf[i];

    /* if output bytes are > than input bytes, 
       and this is the last byte for input, then padd with 0's */
    if((byte_num==input_bytes-1)&&(output_bytes > input_bytes))
    {
      for(j=input_bytes; j < output_bytes; j++)
      {
        outbuf[outputPtr++]=0;
      }
    }
  }
  return(OK);
}

/* convert one buffer type to another, buffer memory for
   outbuf must be preallocated; size calculated with calc*size()
   NOTE: for input format of FORMAT_BIT, set inbufMaxcolour = width

   returns OK or ERROR */
int bufConvert(unsigned char *inbuf, unsigned long inbufsize,
               unsigned int inbufFormat, double inbufMaxcolour,
               unsigned char *outbuf, unsigned long outbufsize,
               unsigned int outbufFormat, double outbufMaxcolour)
{
  unsigned long i;
  int in_prec_bytes, out_prec_bytes;
  unsigned long l_temp = 0;
  long sl_temp = 0;
  float f_temp;
  double d_temp;

  if(outbufFormat == FORMAT_UNCHANGED)
    outbufFormat = inbufFormat;

  if (outbufFormat == FORMAT_BIT)
  {
    fprintf(stderr, "There is currently no support for outputting to 1-BIT BITMAP (FORMAT_BIT) format\n");
    return(ERROR);
  }

  if ((inbufFormat != FORMAT_INT) && (inbufFormat != FORMAT_UINT) && (inbufFormat != FORMAT_FLOAT) && (inbufFormat != FORMAT_DOUBLE) && (inbufFormat != FORMAT_BIT))
  {
    fprintf(stderr, "Invalid input format!\n");
    return(ERROR);
  }

  if ((outbufFormat != FORMAT_INT) && (outbufFormat != FORMAT_UINT) && (outbufFormat != FORMAT_FLOAT) && (outbufFormat != FORMAT_DOUBLE))
  {
    fprintf(stderr, "Invalid output format!\n");
    return(ERROR);
  }

  if ( ((inbufFormat == FORMAT_INT) || (inbufFormat == FORMAT_UINT)) && ((inbufMaxcolour > 32) || (inbufMaxcolour < 8) || ((int)inbufMaxcolour % 8 != 0)))
  {
    fprintf(stderr, "Input Max colour value of %d is out of range!\n", (int)inbufMaxcolour);
    return(ERROR);
  }
  if (((outbufFormat == FORMAT_INT) || (outbufFormat == FORMAT_UINT)) && ((outbufMaxcolour > 32) || (outbufMaxcolour < 0) || ((int)outbufMaxcolour % 8 != 0)))
  {
    fprintf(stderr, "Output Max colour value of %d is out of range!\n", (int)outbufMaxcolour);
    return(ERROR);
  }

  if (((inbufFormat == FORMAT_FLOAT) || (inbufFormat == FORMAT_DOUBLE)) && (inbufMaxcolour <= 0))
  {
    fprintf(stderr, "Input Max colour value out of range!\n");
    return(ERROR);
  }

  if (((inbufFormat == FORMAT_FLOAT) || (inbufFormat == FORMAT_DOUBLE)) && (outbufMaxcolour < 0))
  {
    fprintf(stderr, "Output Max colour value out of range!\n");
    return(ERROR);
  }

  if (((inbufFormat == FORMAT_FLOAT) || (inbufFormat == FORMAT_DOUBLE)) && (outbufMaxcolour == 0))
    outbufMaxcolour=inbufMaxcolour;

  if ((inbuf == NULL) || (outbuf == NULL))
  {
    fprintf(stderr, "NULL Buffer!\n");
    return(ERROR);
  }

  if ((inbufFormat == FORMAT_BIT) && ((outbufFormat == FORMAT_INT) || (outbufFormat == FORMAT_UINT)))
  {
    unsigned int width=(unsigned int)inbufMaxcolour;
    unsigned int columns=(unsigned int)ceil(width/8.0)*8;
    unsigned int whiteVal=(unsigned int)pow(2,outbufMaxcolour)-1;
    unsigned int ptr=0;
    out_prec_bytes=(int)outbufMaxcolour/8;

    for(i=0; i < inbufsize*8; i++)
    {
      if((i % columns) < width)
      {
        l_temp=inbuf[i/8] >> (7-(i%8));
        l_temp &= 1;

        /* not sure why, but bits are NOT'ed to get correct values,
           0 = white, 1 = black?? */
        l_temp = 1 - l_temp;

        l_temp *= whiteVal;

        if(outbufFormat == FORMAT_INT) // if its a signed integer
        {
          sl_temp = (int)l_temp - (int)pow(2,outbufMaxcolour-1);
          if(sintToBuf(sl_temp, &(outbuf[ptr*out_prec_bytes]),(unsigned int)outbufMaxcolour)!=OK)
          {
            fprintf(stderr, "Error converting integer (%ld) to buffer!\n", sl_temp);
            return(ERROR);
          }
        }
        else // FORMAT_UINT
        {
          if(uintToBuf(l_temp, &(outbuf[ptr*out_prec_bytes]),(unsigned int)outbufMaxcolour)!=OK)
          {
            fprintf(stderr, "Error converting integer (%ld) to buffer!\n", l_temp);
            return(ERROR);
          }
        }
        ptr++;
      }
    }
    return(OK);
  }

  if ((inbufFormat == FORMAT_BIT) && ((outbufFormat == FORMAT_FLOAT) || (outbufFormat == FORMAT_DOUBLE)))
  {
    unsigned int width=(unsigned int)inbufMaxcolour;
    unsigned int columns=(unsigned int)ceil(width/8.0)*8;
    unsigned int ptr=0;
    if (outbufFormat == FORMAT_FLOAT)
      out_prec_bytes=4;
    else if (outbufFormat == FORMAT_DOUBLE)
      out_prec_bytes=8;
    else
    {
      fprintf(stderr, "This error shouldn't happen, only here to prevent -Wall warnings\n");
      _exit(1);
    }

    for(i=0; i < inbufsize*8; i++)
    {
      if((i % columns) < width)
      {
        l_temp=inbuf[i/8] >> (7-(i%8));
        l_temp &= 1;

        /* not sure why, but bits are NOT'ed to get correct values,
           0 = white, 1 = black?? */
        l_temp = 1 - l_temp;
        if(l_temp == 0)
          f_temp = (float)-outbufMaxcolour;
        else
          f_temp = (float)outbufMaxcolour;

        if (outbufFormat == FORMAT_FLOAT)
        {
          if(l_temp == 0)
            f_temp = (float)-outbufMaxcolour;
          else
            f_temp = (float)outbufMaxcolour;
          if(floatToBuf(f_temp, &(outbuf[ptr*out_prec_bytes]))!=OK)
          {
            fprintf(stderr, "Error converting float to buffer!\n");
            return(ERROR);
          }
        }
        else // FORMAT_DOUBLE
        {
          if(l_temp == 0)
            d_temp = (double)-outbufMaxcolour;
          else
            d_temp = (double)outbufMaxcolour;
          if(doubleToBuf(d_temp, &(outbuf[ptr*out_prec_bytes]))!=OK)
          {
            fprintf(stderr, "Error converting double to buffer!\n");
            return(ERROR);
          }
        }
        ptr++;
      }
    }
    return(OK);
  }

  if ((inbufFormat == FORMAT_UINT) && (outbufFormat == FORMAT_UINT))
  {
    return(changeBufPrecision(inbuf, inbufsize, outbuf, outbufsize,(unsigned int)inbufMaxcolour,(unsigned int)outbufMaxcolour));
  }

  if ((inbufFormat == FORMAT_INT) && (outbufFormat == FORMAT_INT))
  {
    return(changeBufPrecision(inbuf, inbufsize, outbuf, outbufsize,(unsigned int)inbufMaxcolour,(unsigned int)outbufMaxcolour));
  }

  if ((inbufFormat == FORMAT_INT) && (outbufFormat == FORMAT_UINT))
  {
    out_prec_bytes=(int)outbufMaxcolour/8;

    if(changeBufPrecision(inbuf, inbufsize, outbuf, outbufsize,(unsigned int)inbufMaxcolour,(unsigned int)outbufMaxcolour) != OK)
    {
      fprintf(stderr, "Error changing buffer precision\n");
      _exit(1);
    }

    for(i=0; i < outbufsize; i++)
     if(i % out_prec_bytes == 0)
       outbuf[i] += 128;  // adding 128 to the most significant byte is equivalent to adding 2^(maxcolour-1) to the entire thing
    return(OK);
  }

  if ((inbufFormat == FORMAT_UINT) && (outbufFormat == FORMAT_INT))
  {
    out_prec_bytes=(int)outbufMaxcolour/8;

    if(changeBufPrecision(inbuf, inbufsize, outbuf, outbufsize,(unsigned int)inbufMaxcolour,(unsigned int)outbufMaxcolour) != OK)
    {
      fprintf(stderr, "Error changing buffer precision\n");
      _exit(1);
    }

    for(i=0; i < outbufsize; i++)
     if(i % out_prec_bytes == 0) 
       outbuf[i] -= 128;  // subtracting 128 to the most significant byte is equivalent to subtracting 2^(maxcolour-1) to the entire thing
    return(OK);
  }

  if (((inbufFormat == FORMAT_INT) || (inbufFormat == FORMAT_UINT)) && ((outbufFormat == FORMAT_FLOAT)||(outbufFormat == FORMAT_DOUBLE)))
  {
    in_prec_bytes = (int)inbufMaxcolour/8;
    if (outbufFormat == FORMAT_FLOAT)
      out_prec_bytes=4;
    else if (outbufFormat == FORMAT_DOUBLE)
      out_prec_bytes=8;
    else
    {
      fprintf(stderr, "This error shouldn't happen, only here to prevent -Wall warnings\n");
      _exit(1);
    }

    if ((inbufsize / in_prec_bytes) > (outbufsize / out_prec_bytes))
    {
      fprintf(stderr, "Not enough memory in output buffer!\n");
      return(ERROR);
    }

    for(i=0; i < inbufsize/in_prec_bytes; i++)
    {
      if(bufToInt(&l_temp, &(inbuf[i*in_prec_bytes]), (int)inbufMaxcolour) != OK)
      {
        fprintf(stderr, "Error converting buffer to integer!\n");
        return(ERROR);
      }
  
      if (outbufFormat == FORMAT_FLOAT)
      {
        if(inbufFormat == FORMAT_INT)
          f_temp=(float)slFloatAdjust(l_temp, (int)inbufMaxcolour, (double)outbufMaxcolour);
        else if(inbufFormat == FORMAT_UINT)
          f_temp=(float)ulFloatAdjust(l_temp, (int)inbufMaxcolour, (double)outbufMaxcolour);
        if(floatToBuf(f_temp, &(outbuf[i*out_prec_bytes]))!=OK)
        {
          fprintf(stderr, "Error converting float to buffer!\n");
          return(ERROR);
        }
      }
      else if (outbufFormat == FORMAT_DOUBLE)
      {
        if(inbufFormat == FORMAT_INT)
          d_temp=slFloatAdjust(l_temp, (int)inbufMaxcolour, (double)outbufMaxcolour);
        else if(inbufFormat == FORMAT_UINT)
          d_temp=ulFloatAdjust(l_temp, (int)inbufMaxcolour, (double)outbufMaxcolour);
        if(doubleToBuf(d_temp, &(outbuf[i*out_prec_bytes]))!=OK)
        {
          fprintf(stderr, "Error converting double to buffer!\n");
          return(ERROR);
        }
      }
    }
    return(OK);
  }

  if (((inbufFormat == FORMAT_FLOAT) || (inbufFormat == FORMAT_DOUBLE)) && ((outbufFormat == FORMAT_INT) || (outbufFormat == FORMAT_UINT)))
  {
    if (inbufFormat == FORMAT_FLOAT)
      in_prec_bytes=4;
    else if (inbufFormat == FORMAT_DOUBLE)
      in_prec_bytes=8;
    else
    {
      fprintf(stderr, "This error shouldn't happen, only here to prevent -Wall warnings\n");
      exit(1);
    }
    out_prec_bytes=(int)outbufMaxcolour/8;

    if ((inbufsize / in_prec_bytes) > (outbufsize / out_prec_bytes))
    {
      fprintf(stderr, "Not enough memory in output buffer!\n");
      return(ERROR);
    }

    for(i=0; i < inbufsize/in_prec_bytes; i++)
    {
      if (inbufFormat == FORMAT_FLOAT)
      {
        if(bufToFloat(&f_temp, &(inbuf[i*in_prec_bytes])) != OK)
        {
          fprintf(stderr, "Error converting buffer to float!\n");
          return(ERROR);
        }
        if(outbufFormat == FORMAT_INT)
          sl_temp=FloatAdjustToSLong(f_temp, inbufMaxcolour, (int)outbufMaxcolour);
        else
          l_temp=FloatAdjustToULong(f_temp, inbufMaxcolour, (int)outbufMaxcolour);
      }
      else if (inbufFormat == FORMAT_DOUBLE)
      {
        if(bufToDouble(&d_temp, &(inbuf[i*in_prec_bytes])) != OK)
        {
          fprintf(stderr, "Error converting buffer to float!\n");
          return(ERROR);
        }
        if(outbufFormat == FORMAT_INT)
          sl_temp=(unsigned long)FloatAdjustToSLong(d_temp, inbufMaxcolour, (int)outbufMaxcolour);
        else
          l_temp=FloatAdjustToULong(d_temp, inbufMaxcolour, (int)outbufMaxcolour);
      }

      if(outbufFormat == FORMAT_INT)
      {
        if(sintToBuf(sl_temp, &(outbuf[i*out_prec_bytes]),(unsigned int)outbufMaxcolour)!=OK)
        {
          fprintf(stderr, "Error converting integer (%ld) to buffer!\n", l_temp);
          return(ERROR);
        }
      }
      else // FORMAT_UINT
      {
        if(uintToBuf(l_temp, &(outbuf[i*out_prec_bytes]),(unsigned int)outbufMaxcolour)!=OK)
        {
          fprintf(stderr, "Error converting integer (%ld) to buffer!\n", l_temp);
          return(ERROR);
        }
      }
    }
    return(OK);
  }

  if (((inbufFormat == FORMAT_FLOAT) || (inbufFormat == FORMAT_DOUBLE)) && ((outbufFormat == FORMAT_FLOAT)||(outbufFormat == FORMAT_DOUBLE)))
  {
    if (inbufFormat == FORMAT_FLOAT)
      in_prec_bytes=4;
    else if (inbufFormat == FORMAT_DOUBLE)
      in_prec_bytes=8;
    else
    {
      fprintf(stderr, "This error shouldn't happen, only here to prevent -Wall warnings\n");
      exit(1);
    }

    if (outbufFormat == FORMAT_FLOAT)
      out_prec_bytes=4;
    else if (outbufFormat == FORMAT_DOUBLE)
      out_prec_bytes=8;
    else
    {
      fprintf(stderr, "This error shouldn't happen, only here to prevent -Wall warnings\n");
      exit(1);
    }

    if ((inbufsize / in_prec_bytes) > (outbufsize / out_prec_bytes))
    {
      fprintf(stderr, "Not enough memory in output buffer!\n");
      return(ERROR);
    }

    for(i=0; i < inbufsize/in_prec_bytes; i++)
    {
      if ((inbufFormat == FORMAT_FLOAT) && (outbufFormat == FORMAT_FLOAT))
      {
        if(bufToFloat(&f_temp, &(inbuf[i*in_prec_bytes])) != OK)
        {
          fprintf(stderr, "Error converting buffer to float!\n");
          return(ERROR);
        }

        f_temp=(float)dFloatAdjust(f_temp, inbufMaxcolour, outbufMaxcolour);
        if(floatToBuf(f_temp, &(outbuf[i*out_prec_bytes]))!=OK)
        {
          fprintf(stderr, "Error converting float to buffer!\n");
          return(ERROR);
        }
      }
      else if ((inbufFormat == FORMAT_FLOAT) && (outbufFormat == FORMAT_DOUBLE))
      {
        if(bufToFloat(&f_temp, &(inbuf[i*in_prec_bytes])) != OK)
        {
          fprintf(stderr, "Error converting buffer to float!\n");
          return(ERROR);
        }

        d_temp=dFloatAdjust(f_temp, inbufMaxcolour, outbufMaxcolour);
        if(doubleToBuf(d_temp, &(outbuf[i*out_prec_bytes]))!=OK)
        {
          fprintf(stderr, "Error converting float to buffer!\n");
          return(ERROR);
        }
      }
      else if ((inbufFormat == FORMAT_DOUBLE) && (outbufFormat == FORMAT_FLOAT))
      {
        if(bufToDouble(&d_temp, &(inbuf[i*in_prec_bytes])) != OK)
        {
          fprintf(stderr, "Error converting buffer to double!\n");
          return(ERROR);
        }

        f_temp=(float)dFloatAdjust(d_temp, inbufMaxcolour, outbufMaxcolour);
        if(floatToBuf(f_temp, &(outbuf[i*out_prec_bytes]))!=OK)
        {
          fprintf(stderr, "Error converting float to buffer!\n");
          return(ERROR);
        }
      }
      else if ((inbufFormat == FORMAT_DOUBLE) && (outbufFormat == FORMAT_DOUBLE))
      {
        if(bufToDouble(&d_temp, &(inbuf[i*in_prec_bytes])) != OK)
        {
          fprintf(stderr, "Error converting buffer to float!\n");
          return(ERROR);
        }

        d_temp=dFloatAdjust(d_temp, inbufMaxcolour, outbufMaxcolour);
        if(doubleToBuf(f_temp, &(outbuf[i*out_prec_bytes]))!=OK)
        {
          fprintf(stderr, "Error converting float to buffer!\n");
          return(ERROR);
        }
      }
    }
    return(OK);
  }

  return(ERROR);
}

/* take a single float and write it (w/big-endian) into the buffer
   there must be >= 4 bytes at *buf

   returns OK or ERROR */
int floatToBuf(float f, unsigned char *buf)
{
  int i;
  unsigned char *p = (unsigned char *)&f;

  if (buf == NULL)
    return(ERROR);

  if(BYTE_ORDER == LITTLE_ENDIAN)
  {
    for (i = 0; i < 4; i++)
    {
      buf[i] = p[3-i];
    }
  }
  else
  {
    for (i = 0; i < 4; i++)
    {
      buf[i] = p[i];
    }
  }
  return(OK);
}

/* take a single double and write it (w/big-endian) into the buffer
   there must be >= 8 bytes at *buf

   returns OK or ERROR */
int doubleToBuf(double d, unsigned char *buf)
{
  int i;
  unsigned char *p = (unsigned char *)&d;

  if (buf == NULL)
    return(ERROR);

  if(BYTE_ORDER == LITTLE_ENDIAN)
  {
    for (i = 0; i < 8; i++)
    {
      buf[i] = p[7-i];
    }
  }
  else
  {
    for (i = 0; i < 8; i++)
    {
      buf[i] = p[i];
    }
  }

  return(OK);
}

/* take an integer and write it (w/big-endian) into the buffer
   there must be >= prec/8 bytes at *buf (prec is in bits)

   returns OK or ERROR */
int uintToBuf(unsigned long l, unsigned char *buf, unsigned int prec)
{
  int i;
  int prec_bytes;

  if ((prec <= 0) || (prec %8 != 0) || (prec > 32))
    return(ERROR);

  prec_bytes = prec/8;

  if (buf == NULL)
    return(ERROR);

  if(l >= pow(2,prec))
    return(ERROR);

  for(i=0; i < prec_bytes; i++)
  {
    buf[prec_bytes-i-1]= (unsigned char)l % 256;
    l = l >> 8;
  }

  return(OK);
}

int sintToBuf(long l, unsigned char *buf, unsigned int prec)
{
  int i;
  int prec_bytes;

  if ((prec <= 0) || (prec %8 != 0) || (prec > 32))
    return(ERROR);

  prec_bytes = prec/8;

  if (buf == NULL)
    return(ERROR);

  if( (l >= pow(2,prec-1)) || (l < -pow(2,prec-1)) )
    return(ERROR);

  for(i=0; i < prec_bytes; i++)
  {
    buf[prec_bytes-i-1]= ((unsigned char)l) % 256;
    l = l >> 8;
  }

  return(OK);
}


/* take the next float from the (big-endian) buffer
   there must be >= 4 bytes at *buf

   returns OK or ERROR */
int bufToFloat(float *f, unsigned char *buf)
{
  int i;
  unsigned char *p = (unsigned char *)f;

  if (buf == NULL)
    return(ERROR);

  if(BYTE_ORDER == LITTLE_ENDIAN)
  {
    for (i = 0; i < 4; i++)
    {
      p[i] = buf[3-i];
    }
  }
  else
  {
    for (i = 0; i < 4; i++)
    {
      p[i] = buf[i];
    }
  }

  return(OK);
}

/* take the next double from the (big-endian) buffer
   there must be >= 8 bytes at *buf

   returns OK or ERROR */
int bufToDouble(double *d, unsigned char *buf)
{
  int i;
  unsigned char *p = (unsigned char *)d;

  if (buf == NULL)
    return(ERROR);

  if(BYTE_ORDER == LITTLE_ENDIAN)
  {
    for (i = 0; i < 8; i++)
    {
      p[i] = buf[7-i];
    }
  }
  else
  {
    for (i = 0; i < 8; i++)
    {
      p[i] = buf[i];
    }
  }

  return(OK);
}

/* Saves an integer from a buffer buf (big endian), of precision 'prec' bits 
   into long *l

   returns OK or ERROR */

int bufToInt(unsigned long *l, unsigned char *buf, int prec)
{
  int prec_bytes, i;

  if (buf == NULL)
    return(ERROR);

  if ((prec <= 0) || (prec %8 != 0) || (prec > 32))
    return(ERROR);

  prec_bytes = prec/8;

  *l=0;
  for(i=0; i < prec_bytes; i++)
  {
    *l *= 256;
    *l += buf[i];
  }

  return(OK);
}

/* convert an unsigned long to a float, adjusting the range from [-maxval,+maxval] 
   note, input_prec is in bits! */
double ulFloatAdjust(unsigned long input, int input_prec, double maxval)
{
  double old_maxval = pow(2,input_prec)-1;
  double mult=(2.0*maxval)/old_maxval;

  return((input * mult)-maxval);
}

/* convert a signed long to a float, adjusting the range from [-maxval,+maxval] 
   note, input_prec is in bits! */
double slFloatAdjust(long input, int input_prec, double maxval)
{
  double old_maxval = pow(2,input_prec)-1;
  double mult=(2.0*maxval)/old_maxval;

  return((input+0.5) * mult);  // the 0.5 in order so that if input is minimum value, it corresponds to -maxval
}

/* adjusting the range of a float/double to from [-maxval,+maxval] */
double dFloatAdjust(double input, double old_maxval, double new_maxval)
{
  double mult=new_maxval/old_maxval;

  return(input * mult);
}

/* convert a float to an unsigned long, adjusting the range to output_prec 
   note, output_prec is in bits! */
unsigned long FloatAdjustToULong(double input, double maxval, int output_prec)
{
  double mult=(pow(2,output_prec)-1)/(2*maxval);
  unsigned long l = (unsigned long)((input+maxval)*mult);

  return(l);
}

long FloatAdjustToSLong(double input, double maxval, int output_prec)
{
  double mult=(pow(2,output_prec)-1)/(2*maxval);
  long l = (long)(input*mult);

  return(l);
}

/* copy buffer inbuf with insize to outbuf with outSize */
int bufCopy(unsigned char *inbuf,unsigned long inSize,unsigned char *outbuf,unsigned long outSize)
{
  unsigned long i;

  if ((inbuf == NULL) || (outbuf == NULL))
    return(ERROR);

  if (inSize != outSize)
    return(ERROR);

  for(i=0;i<inSize;i++)
    outbuf[i]=inbuf[i];

  return(OK);
}

