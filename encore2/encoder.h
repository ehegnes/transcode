#ifndef _ENCORE_ENCODER_H
#define _ENCORE_ENCODER_H

#include "vop.h"
#include "encore2.h"

int CreateEncoder(ENC_PARAM* pParam);
int FreeEncoder(Encoder* pEnc);
int EncodeFrame(Encoder* pEnc, ENC_FRAME* pFrame, ENC_RESULT* pResult);

#endif
