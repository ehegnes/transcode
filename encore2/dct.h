#ifndef _ENCORE_DCT_H
#define _ENCORE_DCT_H
#include "portab.h"

void fdct_enc(int16_t *block);
void fdct_enc_fast(int16_t *block);
void init_fdct_enc();
void idct_enc(int16_t *block);
void init_idct_enc();


void fdct_mm32(int16_t *blk);
void Fast_IDCT(int16_t *x);

#endif
