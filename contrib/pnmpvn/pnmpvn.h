/* pnmpvn.h by Jacob (Jack) Gryn

   PNM (PBM/PGM/PPM) and PVN (PVB/PVG/PBP) conversion Library

   * the PVN (PVB/PVG/PVP) file format, and this code
     is (c) 2003,2004 Jacob (Jack) Gryn

   * the author grants full rights to all who wish to use
     and distribute this code and the corresponding file
     formats on the assumption that credit is given to
     the author if asked

   Jacob (Jack) Gryn
 */

#ifndef PNMPVN_H
#define PNMPVN_H

#ifdef __cplusplus
  namespace std
  {
    extern "C"
    {                // or   extern "C++" {
#endif

/* converts pvn to multiple pnm files

   infile = input filename
   digits = # of digits in index in filename
            ie.  digits = 3, would give a filename img000.pgm
 */
int pvn2pnm(const char *infile, unsigned int digits);

/*
   - Opens a list of PGM/PPM/PNM files and saves them in PVG/PVP/PVN format

   infiles = list of input files
   infiles_count = # of files in list
   outfile = output filename
   framerate = framerate
*/
int pnm2pvn(const char **infiles, int infiles_count, const char *outfile, double framerate, unsigned int format, double maxcolour, unsigned int copies);

#ifdef __cplusplus
    }       // end extern
  }    // end namespace
#endif

#endif
