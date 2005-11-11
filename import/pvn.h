/* pvn.h by Jacob (Jack) Gryn

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

#ifndef PVN_H
#define PVN_H

// we need a floating point number that will very rarely get used in the framerate field
#define UNDEF_FRAMERATE -0.99098765123

#ifdef __cplusplus
  namespace std
  {
    extern "C"
    {                // or   extern "C++" {
#endif

typedef struct _pvnparam { char magic[5]; unsigned int width; unsigned int height; unsigned int depth; double maxcolour; double framerate; } PVNParam;

/* calculates the size of raster data in a PVN file based on a PVNParam
   header

   returns # of bytes if PV4/PV5/PV6 (a/f/d);
*/
long calcPVNSize(PVNParam p);

/* calculates the size of raster data of a single image within a PVN file
   based on a PVNParam header

   returns # of bytes if PV4/PV5/PV6 (a/f/d);
*/
long calcPVNPageSize(PVNParam p);

/* Compare two parameter sets; return EQUAL if they are equal,
   or NOTEQUAL if they are not */
int PVNParamCompare(PVNParam first, PVNParam second);

/* Copy src parameters to dest
   returns VALID or ERROR */
int PVNParamCopy(PVNParam *dest, PVNParam *src);

/* Just retrieve the header
   returns  INVALID if it is not a PNM header
            VALID if everything is ok */
int readPVNHeader(FILE *fp, PVNParam *p);

/* write PVN Header to file *fp */
int writePVNHeader(FILE *fp, PVNParam p);

/* display the pvnparam header to stdout */
void showPVNHeader(PVNParam p);

/* converts a PVN to another format of PVN

   infile / outfile = in/output filenames
   framerate = new framerate; if framerate = NAN, use input framerate
   format = new format
   maxcolour = new maxcolour / range value (for floats/doubles)
 */
int pvnconvert(const char *infile, const char *outfile, double framerate, unsigned int format, double maxcolour);

#ifdef __cplusplus
    }       // end extern
  }    // end namespace
#endif

#endif
