//
//  quantize_ni_mmx.s:  MMX optimized coefficient quantization sub-routine
//
//  Copyright (C) 2000 Andrew Stevens <as@comlab.ox.ac.uk>
//
//  This program is free software; you can reaxstribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

unsigned short int bb_satlim[4] = {1024-1, 1024-1, 1024-1, 1024-1};
unsigned short int bb_tmp_quant_buf[64];
unsigned short int *bb_quant_buf = &bb_tmp_quant_buf[0];

/*
int quantize_ni_mmx(short *dst, short *src,
                             unsigned short *quant_mat, unsigned short *i_quant_mat,
                     int imquant, int mquant, int sat_limit)
*/
//  See quantize.c: quant_non_intra_inv()  for reference implementation in C...
                ////  mquant is not currently used.
// eax = row counter...
// ebx = pqm
// ecx = piqm  ; Matrix of quads first (2^16/quant)
                          // then (2^16/quant)*(2^16%quant) the second part is for rounding
// edx = satlim
// edi = psrc
// esi = pdst

// mm0 = [imquant|0..3]W
// mm1 = [sat_limit|0..3]W
// mm2 = *psrc -> src
// mm3 = rounding corrections...
// mm4 = flags...
// mm5 = saturation accumulator...
// mm6 = nzflag accumulator...
// mm7 = temp
