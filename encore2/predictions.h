#ifndef _ENCORE_PREDICTIONS_H
#define _ENCORE_PREDICTIONS_H

/** Find a prediction for motion vector in MB 'x'*'y', block 'block',
component 'comp' ( 0 - x, 1 - y ), using MVs stored in pVop->pMBs[]. 
Implementation exactly matches find_pmv() from decore/mp4_picture.c,
name is different to avoid linker error. **/
int16_t enc_find_pmv(Vop* pVop, int x, int y, int block, int comp);

/** Find predictions for DCT coefficients 'dct_codes' in MB 'x'*'y',
block 'block'. Store determined prediction direction in 'acpred_direction',
predicted DC coeff in '*dc_pred', pointer to predicted AC coeffs in
'ac_pred'. **/
int32_t calc_acdc_prediction(Vop* pVop, int x, int y, int block, 
    int* acpred_direction, const int16_t* dct_codes, 
    int16_t* dc_pred, const int16_t** ac_pred,
    uint8_t iDcScaler);

#endif
