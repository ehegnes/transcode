#ifndef _ENCORE_QUANTIZE_H
#define _ENCORE_QUANTIZE_H
#include "enctypes.h"
/** H.263 quantization/dequantization **/
void quantize_intra(int16_t* block, uint8_t quantizer, uint8_t dc_scaler);
void quantize_inter(int16_t* block, uint8_t quantizer);

void dequantize_intra(int16_t* block, uint8_t quantizer, uint8_t dc_scaler);
void dequantize_inter(int16_t* block, uint8_t quantizer);

#endif
