/*
 * ratiocodes.c - database for all ratio/codes (asr, sar, dar, frc...)
 *                used in transcode
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include <stdlib.h>
#include <math.h>

#include "ratiocodes.h"


#define TABLE_LEN(tab) (sizeof((tab))/sizeof((tab)[0]))

/* WARNING: this table MUST BE in frc order */
static const double frc_table[16] = {
    0,
    (24000.0/1001.0),
    24,
    25,
    (30000.0/1001.0),
    30,
    50,
    (2*(30000.0/1001.0)),
    60,
    1,
    5,
    10,
    12,
    15,
    0,
    0
};

/* WARNING: this table MUST BE in frc order */
static const TCPair frc_ratios[16] = {
    { 0, 0 },
    { 24000, 1001 },
    { 24000, 1000 },
    { 25000, 1000 },
    { 30000, 1001 },
    { 30000, 1000 },
    { 50000, 1000 },
    { 60000, 1001 },
    { 60000, 1000 },
    /* XXX */
    { 1000, 1000 },
    { 5000, 1000 },
    { 10000, 1000 },
    { 12000, 1000 },
    { 15000, 1000 },
    /* XXX  */
    { 0, 0 },
    { 0, 0 },
};

/* WARNING: this table MUST BE in asr order */
static const TCPair asr_ratios[8] = {
    { 0, 0 },
    { 1, 1},
    { 4, 3},
    { 16, 9},
    { 221, 100},
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
};


#define DELTA 0.0005
int tc_frc_code_from_value(int *frc_code, double fps)
{
    int frc = TC_NULL_MATCH, i = 0;
    double mindiff = DELTA;
    
    for (i = 0; i < TABLE_LEN(frc_table); i++) {
        double diff = fabs(frc_table[i] - fps);
        if (diff < mindiff) {
            mindiff = diff;
            frc = i;
        }
    }
    if (frc_code != NULL && frc != TC_NULL_MATCH) {
        *frc_code = frc;    
    }
    return frc;
}
#undef DELTA

int tc_frc_code_to_value(int frc_code, double *fps)
{
    if ((fps != NULL && frc_code >= 0)
      && frc_code <= TABLE_LEN(frc_table)) {
        *fps = frc_table[frc_code];
        return 0;
    }
    return TC_NULL_MATCH;
}

/*
 * match_ratio:
 *      helper for various detection functions. Scans a ratio
 *      table (that MUST be in frc order) looking for corrispondences
 *      between a ratio and a ratio code.
 *
 * Parameters:
 *     pairs: pointer to an array of TCPair to scan
 *       len: number of pairs to consider
 *         n: numerator of ratio to look for. Use TC_NULL_MATCH
 *            if this function must look for a corrispondency of code
 *            and not for a corrispondency of ratio.
 *         d: denominator of ratio to look for. Use TC_NULL_MATCH
 *            if this function must look for a corrispondency of code
 *            and not for a corrispondency of ratio.
 *      code: code of ratio to look for. Use TC_NULL_MATCH if this function
 *            must look for a corrispondency of ratio.
 * Return Value:
 *     TC_NULL_MATCH if input parameter(s) isn't known.
 *     >= 0 index in table of given corrispondency.
 * Precondintions:
 *     given pairs table MUST BE in code (frc, asr) order.
 *     pairs != NULL.
 */
static int match_ratio(const TCPair *pairs, size_t len,
                       int n, int d, int code)
{
    int i = 0, r = TC_NULL_MATCH;
    for (i = 0; i < len; i++) {
        if (i == code) {
            r = i;
            break;
        }
        if (n == pairs[i].a && d == pairs[i].b) {
            r = i;
            break;
        }
    }
    return r;
}

int tc_asr_code_from_ratio(int *asr_code, int n, int d)
{
    int asr = match_ratio(asr_ratios, TABLE_LEN(asr_ratios),
                          n, d, TC_NULL_MATCH);
    if (asr_code != NULL && asr != TC_NULL_MATCH) {
        *asr_code = asr;
    }
    return asr;
}

int tc_asr_code_to_ratio(int asr_code, int *n, int *d)
{
    int asr = match_ratio(asr_ratios, TABLE_LEN(asr_ratios),
                          TC_NULL_MATCH, TC_NULL_MATCH, asr_code);
    if ((n != NULL && d != NULL) && asr != TC_NULL_MATCH) {
        *n = asr_ratios[asr].a;
        *d = asr_ratios[asr].b;
    }
    return asr;
}

int tc_frc_code_from_ratio(int *frc_code, int n, int d)
{
    int frc = match_ratio(frc_ratios, TABLE_LEN(frc_ratios),
                          n, d, TC_NULL_MATCH);
    if (frc_code != NULL && frc != TC_NULL_MATCH) {
        *frc_code = frc;
    }
    return frc;
}

int tc_frc_code_to_ratio(int frc_code, int *n, int *d)
{
    int frc = match_ratio(frc_ratios, TABLE_LEN(frc_ratios),
                          TC_NULL_MATCH, TC_NULL_MATCH, frc_code);
    if ((n != NULL && d != NULL) && frc != TC_NULL_MATCH) {
        *n = frc_ratios[frc].a;
        *d = frc_ratios[frc].b;
    }
    return frc;
}

