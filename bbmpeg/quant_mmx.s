extern bb_satlim
extern bb_tmp_quant_buf
extern bb_quant_buf

SECTION .text
		
global bb_quant_ni_mmx

align 32
bb_quant_ni_mmx:
	push	esi
	push	edi

        mov edi, [bb_quant_buf]      ; get temporary dst w

        mov esi, [esp + 8 + 8]    ; P2: short  *src         
        mov ebx, [esp + 8 + 12]   ; P3: ushort *quant_mat    
        mov ecx, [esp + 8 + 16]   ; P4: ushort *i_quant_mat  
        movd mm0, [esp + 8 + 20]  ; P5: int imquant -> (2^16 / mquant )
				  ; P6: int mquant -> not currently used
        movq mm1, mm0             
        punpcklwd mm0, mm1
        punpcklwd mm0, mm0        ; mm0 = [imquant|0..3]W

        pxor mm6, mm6             ; saturation / out-of-range accumulator(s)
        movd mm1, [esp + 8 + 28]  ; P7: int sat_limit  
        movq mm2, mm1
        punpcklwd mm1, mm2        ; [sat_limit|0..3]W
        punpcklwd mm1, mm1        ; mm1 = [sat_limit|0..3]W

        xor       edx, edx        ; Non-zero flag accumulator
        mov eax,  16              ; 16 quads to do
        jmp .nextquadniq

.nextquadniq:
        movq mm2, [esi]        ; mm0 = *psrc

        pxor    mm4, mm4
        pcmpgtw mm4, mm2       ; mm4 = *psrc < 0
        movq    mm7, mm2       ; mm7 = *psrc
        psllw   mm7, 1         ; mm7 = 2*(*psrc)
        pand    mm7, mm4       ; mm7 = 2*(*psrc)*(*psrc < 0)
        psubw   mm2, mm7       ; mm2 = abs(*psrc)

        ;  Check whether we'll saturate intermediate results
        movq    mm7, mm2
        pcmpgtw mm7, [bb_satlim]  ; Toobig for 16 bit arithmetic :-( (should be *very* rare)
        por     mm6, mm7

        ; Carry on with the arithmetic...
        psllw   mm2, 5         ; mm2 = 32*abs(*psrc)
        movq    mm7, [ebx]     ; mm7 = *pqm>>1
        psrlw   mm7, 1
        paddw   mm2, mm7       ; mm2 = 32*abs(*psrc)+((*pqm)) = "p"


        ; Do the first multiplication.  Cunningly we've set things up so
        ; it is exactly the top 16 bits we're interested in...
        ;
        ; We need the low word results for a rounding correction.
        ; This is *not* exact (that actual correction
        ; is the product (p*(*piqm)*(2^16%(*piqm)) + smallfactor ) >> 16
        ;  However we get very very few wrong and none too low (the most
        ; important) and no errors for small coefficients (also important)
        ; if we simply add p*(*piqm)+p instead ;-)
        movq    mm3, mm2
        pmullw  mm3, [ecx]
        movq    mm5, mm2
        psrlw   mm5, 1       ; Want to see if adding p would carry into upper 16 bits
        psrlw   mm3, 1
        paddw   mm3, mm5
        psrlw   mm3, 15      ; High bit in lsb rest 0's
        pmulhw  mm2, [ecx]   ; mm2 = (p*iqm+p) >> IQUANT_SCALE_POW2 ~= p / * qm

        ; To hide the latency lets update some pointers...
        add   esi, 8         ; 4 word's
        add   ecx, 8         ; 4 word's
        sub   eax, 1

        ; Now add rounding correction....
        paddw   mm2, mm3

        ; Do the second multiplication, again we ned to make a rounding adjustment
        movq    mm3, mm2
        pmullw  mm3, mm0
        movq    mm5, mm2
        psrlw   mm5, 1        ; Want to see if adding correction would carry into upper 16 bits
        psrlw   mm3, 1
        paddw   mm3, mm5
        psrlw   mm3, 15       ; High bit in lsb rest 0's

        pmulhw  mm2, mm0      ; mm2 ~= (p/(qm*mquant))

        ; To hide the latency lets update some more pointers...
        add   edi, 8
        add   ebx, 8

        ; Correct rounding and the factor of two (we want mm2 ~= p/(qm*2*mquant)
        paddw mm2, mm3
        psrlw mm2, 1

        ; Check for saturation
        movq mm7, mm2
        pcmpgtw mm7, mm1
        por     mm6, mm7      ; Accumulate that bad things happened...

        ; Accumulate non-zero flags
        movq   mm7, mm2
        movq   mm5, mm2
        psrlq   mm5, 32
        por     mm5, mm7
        movd    mm7, edx      ; edx  |= mm2 != 0
        por     mm7, mm5
        movd    edx, mm7

        ; Now correct the sign mm4 = *psrc < 0
        pxor mm7, mm7         ; mm7 = -2*mm2
        psubw mm7, mm2
        psllw mm7, 1
        pand  mm7, mm4        ; mm7 = -2*mm2 * (*psrc < 0)
        paddw mm2, mm7        ; mm7 = samesign(*psrc, mm2 )

        ; Store the quantised words....
        movq [edi-8], mm2
        test eax, eax
        jnz near .nextquadniq
;quit:

        ; Return saturation in low word and nzflag in high word of reuslt dword
        movq mm0, mm6
        psrlq mm0, 32
        por   mm6, mm0
        movd  eax, mm6
        mov   ebx, eax
        shr   ebx, 16
        or    eax, ebx
        and   eax, 0xffff         ;low word eax is saturation

        test  eax, eax
        jnz   .skipupdate

        mov   ecx, 8              ; 8 pairs of quads...
        mov   edi, [esp + 8 + 4]  ; P1: short *dst
        mov   esi, [bb_quant_buf]
.update:
        movq  mm0, [esi]
        movq  mm1, [esi+8]
        add   esi, 16
        movq  [edi], mm0
        movq  [edi+8], mm1
        add   edi, 16
        sub   ecx, 1
        jnz   .update
.skipupdate:
        mov   ebx, edx
        shl   ebx, 16
        or    edx, ebx
        and   edx, 0xffff0000     ; hiwgh word ecx is nzflag
        or    eax, edx

        pop   edi
        pop   esi
        emms                      ;  clear mmx registers
        ret
