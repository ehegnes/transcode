#ifndef _ENCORE_MOTION_H
#define _ENCORE_MOTION_H

#include "enctypes.h"

/* Perform motion search on VOP pEnc->sReference, attempting to
match image pointed by pImage. Write decisions ( INTER, INTER4V, INTRA )
and vectors into pEnc->sCurrent.pMBs[]. Return part of macroblocks decided
to be intra ( 0<=retval<=1 ). Reconstruct VOP pEnc->sCurrent using data 
from pEnc->sReference and found motion vectors/macroblock decisions. */

float MotionEstimateCompensate(Encoder* pEnc, Image* pImage);

#endif
