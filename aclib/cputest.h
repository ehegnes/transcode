/*
 * DSP utils
 * Copyright (c) 2000, 2001, 2002 Fabrice Bellard.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef CPUTEST_H
#define CPUTEST_H

#include "ac.h"

#ifdef HAVE_MMX


static inline void emms(void)
{
    __asm __volatile ("emms;":::"memory");
}

#define emms_c() \
{\
    if (mm_flags & MM_MMX)\
        emms();\
}



#define __align8 __attribute__ ((aligned (8)))

#elif defined(ARCH_ARMV4L)

#define emms_c()

/* This is to use 4 bytes read to the IDCT pointers for some 'zero'
   line ptimizations */
#define __align8 __attribute__ ((aligned (4)))

#elif defined(HAVE_MLIB)
 
#define emms_c()

/* SPARC/VIS IDCT needs 8-byte aligned DCT blocks */
#define __align8 __attribute__ ((aligned (8)))

void dsputil_init_mlib(void);   

#elif defined(ARCH_ALPHA)

#define emms_c()
#define __align8 __attribute__ ((aligned (8)))

#elif defined(ARCH_POWERPC)

#define emms_c()
#define __align8 __attribute__ ((aligned (16)))

#elif defined(HAVE_MMI)

#define emms_c()

#define __align8 __attribute__ ((aligned (16)))

void dsputil_init_mmi(void);   

#else

#define emms_c()

#define __align8

#endif
              
#endif
