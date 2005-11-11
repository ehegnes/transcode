/* pnm.c by Jacob (Jack) Gryn

   PNM (PBM/PGM/PPM) Library

   * the PVN (PVB/PVG/PVP) file format, and this code
     is (c) 2003,2004 Jacob (Jack) Gryn

   * the author grants full rights to all who wish to use
     and distribute this code and the corresponding file
     formats on the assumption that credit is given to
     the author if asked

   Jacob (Jack) Gryn
 */
#ifndef PNM_H
#define PNM_H

#include "pvnglobals.h"

#ifdef __cplusplus
  namespace std
  {
    extern "C"
    {                // or   extern "C++" {
#endif

typedef struct _pnmparam { char magic[3]; unsigned int width; unsigned int height; unsigned int maxcolour;} PNMParam;

/* calculates the size of raster data in a PNM file based on a PNMParam header */
long calcPNMSize(PNMParam p);

/* Compare two parameter sets; return EQUAL if they are equal,
   or NOTEQUAL if they are not */
int PNMParamCompare(PNMParam first, PNMParam second);

/* Copy src parameters to dest
   returns VALID or ERROR */
int PNMParamCopy(PNMParam *dest, PNMParam *src);

/* Just retrieve the header
   returns  INVALID if it is not a PNM header
            VALID if everything is ok */
int readPNMHeader(FILE *fp, PNMParam *p);

/* write PNM Header to file *fp */
int writePNMHeader(FILE *fp, PNMParam p);

/* display the pnmparam header to stdout */
void showPNMHeader(PNMParam p);

#ifdef __cplusplus
    }       // end extern
  }    // end namespace
#endif

#endif
