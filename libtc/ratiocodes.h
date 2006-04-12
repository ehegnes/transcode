/*
 * ratiocodes.h - database for all ratio/codes (asr, sar, dar, frc...)
 *                used in transcode
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#ifndef RATIOCODES_H
#define RATIOCODES_H

typedef struct tcpair_ TCPair;
struct tcpair_ {
    int a; /* numerator, width... */
    int b; /* denominator, height... */
};

#define TC_NULL_MATCH  (-1)

#define TC_FRC_RATIO_FROM_FPS(fps, pn, pd) \
do { \
    *(pn) = (int)((fps) * 1000.0); \
    *(pd) = 1000; \
} while(0)

/*
 * TCPair VS int/int dilemma:
 *
 * Why don't use out TCPair instead of int/int couples in
 * functions interface below?
 * this code (and the whole transcode in general) need to interact
 * with a broad variety of foreign code with various conventions,
 * interface, styles and so on. I've no *YET* found a clean way
 * to use a custom struct (TCPair) for those situations without
 * requiring to use a temporary variable, that looks quite
 * clumsy to me. So I've chosen to fall back to minimum
 * comune denominator, that is a couple of int or pointer
 * to ints.
 *
 * Of course this can change if a better and cleanest solution
 * pops out :)
 *
 * BTW, TCPair is extensively used in internal processing and
 * in private code.
 */

/*
 * tc_frc_code_from_value:
 *    detect the right frame ratio code (frc) given a frame rate value as
 *    real number.
 * 
 * Parameters:
 *    frc_code: pointer to integer where detected frc code will be stored.
 *              Can be NULL: if so, frc code will be detected but not stored.
 *         fps: value of frame rate, as real number.
 * Return Value:
 *    TC_NULL_MATCH if input value isn't known
 *    >= 0 otherwise
 */
int tc_frc_code_from_value(int *frc_code, double fps);

/*
 * tc_frc_code_to_value:
 *    detect the right frame ratio value as real number given a frame rate
 *    code (frc).
 * 
 * Parameters:
 *    frc_code: frame rate code.
 *         fps: pointer to double where detected frc value will be stored.
 *              Can be NULL: if so, frc value will be detected but not stored.
 * Return Value:
 *    TC_NULL_MATCH if input value isn't known
 *    >= 0 otherwise
 */
int tc_frc_code_to_value(int frc_code, double *fps);

/*
 * tc_frc_code_from_ratio:
 *    detect the right frame ratio code (frc) given a frame rate fraction
 *    as pair of integers.
 * 
 * Parameters:
 *    frc_code: pointer to integer where detected frc code will be stored.
 *              Can be NULL: if so, frc code will be detected but not
 *              stored.
 *           n: numerator of given frame ratio fraction.
 *           d: denominator of given frame ratio fraction.
 * Return Value:
 *    TC_NULL_MATCH if input value isn't known
 *    >= 0 otherwise
 */
int tc_frc_code_from_ratio(int *frc_code, int n, int d);

/*
 * tc_frc_code_to_ratio:
 *    detect the right frame ratio fraction as pair of integers given a
 *    frame rate code (frc).
 * 
 * Parameters:
 *    frc_code: frame rate code.
 *           n: pointer to integer where numerator of frame rate fraction
 *              will ne stored. Can be NULL: if so, frc fraction will be
 *              detected but not stored.
 *           d: pointer to integer where denominator of frame rate fraction
 *              will ne stored. Can be NULL: if so, frc fraction will be
 *              detected but not stored.
 * Return Value:
 *    TC_NULL_MATCH if input value isn't known
 *    >= 0 otherwise
 */
int tc_frc_code_to_ratio(int frc_code, int *n, int *d);

/*
 * tc_asr_code_from_ratio:
 *    detect the right aspect ratio code (asr) given an aspect
 *    rate fraction as pair of integers.
 * 
 * Parameters:
 *    asr_code: pointer to integer where detected asr code will be stored.
 *              Can be NULL: if so, asr code will be detected but not
 *              stored.
 *           n: numerator of given aspect ratio fraction.
 *           d: denominator of given aspect ratio fraction.
 * Return Value:
 *    TC_NULL_MATCH if input value isn't known
 *    >= 0 otherwise
 */
int tc_asr_code_from_ratio(int *asr_code, int n, int d);

/*
 * tc_asr_code_to_ratio:
 *    detect the right aspect ratio fraction as pair of integers given a
 *    aspect ratio code (asr).
 * 
 * Parameters:
 *    asr_code: aspect ratio code.
 *           n: pointer to integer where numerator of aspect ratio fraction
 *              will ne stored. Can be NULL: if so, asr fraction will be
 *              detected but not stored.
 *           d: pointer to integer where denominator of aspect ratio
 *              fraction will ne stored. Can be NULL: if so, asr fraction
 *              will be detected but not stored.
 * Return Value:
 *    TC_NULL_MATCH if input value isn't known
 *    >= 0 otherwise
 */
int tc_asr_code_to_ratio(int asr_code, int *n, int *d);

#endif  /* RATIOCODES_H */
