/* pvnglobals.h

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
#ifndef PVNPNMGLOBALS_H
#define PVNPNMGLOBALS_H

#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#ifdef _MSC_VER
  #include <malloc.h>
  #define LITTLE_ENDIAN 1234
  #define BYTE_ORDER LITTLE_ENDIAN
#elif defined(__APPLE__)
  #define BYTE_ORDER BIG_ENDIAN
#else
  #include <malloc.h>
  #include <endian.h>
#endif


#ifdef __cplusplus
  namespace std
  {
    extern "C"
    {                // or   extern "C++" {
#endif

#define MAX_ASCII_LINE_LENGTH 1024
#define MAX_FILENAME_LENGTH 256

#define EQUAL 0
#define NOTEQUAL -1
#define VALID 0

#define OK 0
#define ERROR -1
#define OPENERROR -2
#define INVALID -3
#define UNKNOWN -4

#define FORMAT_UNCHANGED 0
#define FORMAT_UINT 1
#define FORMAT_INT 2
#define FORMAT_FLOAT 3
#define FORMAT_DOUBLE 4
#define FORMAT_BIT 5

/* get the file size of file pointer *fp (must be open) */
long filesize(FILE *fp);

/* read in 'count' unsigned integers of range 0-maxcolour and place values
   in buffer 'buf' from file pointer fp */
int asciiRead(unsigned char *buf, unsigned int count, FILE *fp, unsigned int maxcolour);

/* generate a numerically-indexed filename
   prefix = prefix of filename
   suffix = suffix / file extension
   filename = buffer to store output filename string
   val = index value
   digits = # of digits to use in index part of filename
 */
int genFileName(const char *prefix, const char *suffix, char filename[MAX_FILENAME_LENGTH], unsigned int val, unsigned int digits);

/* takes a buffer, inbuf, with precision 'input_prec' bits (multiple of 8 bits)
         and size inbufsize
   and converts it to precision 'output_prec' (also multiple of 8 bits) in outbuf
         (with outbufsize)

   inbufsize/outbuf is the current size (in bytes) of the respective buffer.

   inbuf/outbuf are stored with most significant byte first

   returns OK or ERROR
*/
int changeBufPrecision(unsigned char *inbuf, unsigned long inbufsize,
                       unsigned char *outbuf, unsigned long outbufsize,
                       unsigned int input_prec, unsigned int output_prec);

/* convert one buffer type to another, buffer memory for
   outbuf must be preallocated; size calculated with calc*size()
   NOTE: for input format of FORMAT_BIT, set inbufMaxcolour = width

   returns OK or ERROR */
int bufConvert(unsigned char *inbuf, unsigned long inbufsize,
               unsigned int inbufFormat, double inbufMaxcolour,
               unsigned char *outbuf, unsigned long outbufsize,
               unsigned int outbufFormat, double outbufMaxcolour);

/* take a single float and write it (w/big-endian) into the buffer
   there must be >= 4 bytes at *buf

   returns OK or ERROR */
int floatToBuf(float f, unsigned char *buf);

/* take a single double and write it (w/big-endian) into the buffer
   there must be >= 8 bytes at *buf

   returns OK or ERROR */
int doubleToBuf(double d, unsigned char *buf);

/* take an integer and write it (w/big-endian) into the buffer
   there must be >= maxcolour/8 bytes at *buf (prec is in bits)

   returns OK or ERROR */
int uintToBuf(unsigned long l, unsigned char *buf, unsigned int prec);
int sintToBuf(long l, unsigned char *buf, unsigned int prec);

/* take the next float from the (big-endian) buffer
   there must be >= 4 bytes at *buf

   returns OK or ERROR */
int bufToFloat(float *f, unsigned char *buf);

/* take the next double from the (big-endian) buffer
   there must be >= 8 bytes at *buf

   returns OK or ERROR */
int bufToDouble(double *d, unsigned char *buf);

/* Saves an integer from a buffer buf (big endian), of precision 'prec' bits
   into long *l

   returns OK or ERROR */
int bufToInt(unsigned long *l, unsigned char *buf, int prec);

/* convert an unsigned long to a float, adjusting the range from [-maxval,+maxval]
   note, input_prec is in bits! */
double ulFloatAdjust(unsigned long input, int input_prec, double maxval);

/* convert a signed long to a float, adjusting the range from [-maxval,+maxval]
   note, input_prec is in bits! */
double slFloatAdjust(long input, int input_prec, double maxval);

/* adjusting the range of a float/double to from [-maxval,+maxval] */
double dFloatAdjust(double input, double old_maxval, double new_maxval);

/* convert a float to a long, adjusting the range to output_prec
   note, output_prec is in bits! */
unsigned long FloatAdjustToULong(double input, double maxval, int output_prec);
long FloatAdjustToSLong(double input, double maxval, int output_prec);

/* copy buffer inbuf with insize to outbuf with outSize */
int bufCopy(unsigned char *inbuf,unsigned long inSize,unsigned char *outbuf,unsigned long outSize);

#ifdef __cplusplus
    }       // end extern
  }    // end namespace
#endif

#endif
