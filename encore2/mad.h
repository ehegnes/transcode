#ifndef _ENCORE_MAD_H
#define _ENCORE_MAD_H
#include "vop.h"
float MAD_Image(const Image* pIm, const Vop* pVop);
// x & y in blocks ( 8 pixel units )
// dx & dy in pixels
int32_t SAD_Block(const Image* pIm, const Vop* pVop, int x, int y, int dx, int dy, int32_t sad_opt, int component);

// x & y in macroblocks
// dx & dy in half-pixels

int32_t SAD_Macroblock(const Image* pIm, 
    const Vop* pVopN, const Vop* pVopH, const Vop* pVopV, const Vop* pVopHV,
    int x, int y, int dx, int dy, int sad_opt, int quality);

//int32_t SAD_Macroblock(const Image* pIm, 
//    const Vop* pVop,
//    int x, int y, int dx, int dy, int sad_opt);

// x & y in macroblocks
int32_t SAD_Deviation_MB(const Image* pIm, int x, int y, int width);

#endif
