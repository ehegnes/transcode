#ifndef _ENCORE_VOP_H
#define _ENCORE_VOP_H

#include "enctypes.h"

int CreateVop(Vop* pVop, int width, int height);
void FreeVop(Vop* pVop);
void SwapVops(Vop* pIm1, Vop* pIm2);
void SetEdges(Vop* pVop);
void Interpolate(const Vop* pRef, Vop* pInterH, Vop* pInterV, Vop* pInterHV, const int iRounding, int iChromOnly);
/*
void InterpolateH(const Vop* pRef, Vop* pInterpolated, const int iRounding, int iChromOnly);
void InterpolateV(const Vop* pRef, Vop* pInterpolated, const int iRounding, int iChromOnly);
void InterpolateHV(const Vop* pRef, Vop* pInterpolated, const int iRounding, int iChromOnly);
*/
#endif
