#ifndef _ENCORE_BLOCK_H
#define _ENCORE_BLOCK_H
#include "enctypes.h"
#include "bitstream.h"

/** Analyse and pack into bitstream a single macroblock with coords 'x'*'y',
assuming that it's intra ( first function ) or inter ( second one ).
For P-VOPs second function will sometimes call the first one. **/
void EncodeMacroblockIntra(Encoder* pEnc, Image* im, Bitstream* bs, int x, int y);
void EncodeMacroblockInter(Encoder* pEnc, Image* im, Bitstream* bs, int x, int y);

#endif
