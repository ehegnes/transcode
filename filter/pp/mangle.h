/* mangle.h - This file has some CPP macros to deal with different symbol
 * mangling across binary formats.
 * (c)2002 by Felix Buenemann <atmosfear at users.sourceforge.net>
 * File licensed under the GPL, see http://www.fsf.org/ for more info.
 */

#ifndef __MANGLE_H
#define __MANGLE_H

/* Feel free to add more to the list, eg. a.out IMO */
#if defined(__CYGWIN__) || defined(__MINGW32__) || defined(__OS2__) || \
   (defined(__OpenBSD__) && !defined(__ELF__))
#define MANGLE(a) "_" #a
#else
#define MANGLE(a) #a
#endif

/* Kludge:
 * gcc-2.95 wont compile postprocess_template in -fPIC mode
 * gcc-3.2.3 does work -- tibit
 */
#if defined(PIC) && (__GNUC__ == 2)
#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_SSE
#undef HAVE_3DNOW
#warning All optimizations disabled because of gcc-2.95
#endif

#endif /* !__MANGLE_H */

