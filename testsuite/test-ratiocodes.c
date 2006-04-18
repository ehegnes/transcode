/*
 * test-ratiocodes.c - testsuite for to/from ratio utility conversion
 *                     functions. Everyone feel free to add more tests
 *                     and improve existing ones.
 * (C) 2006 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "tc_defaults.h"
#include "libtc/libtc.h"
#include "libtc/ratiocodes.h"

#ifndef PACKAGE
#define PACKAGE __FILE__
#endif

#define DELTA (0.0005)

static int test_autoloop_fps(double fps)
{
    int ret = 0, frc;
    double myfps;

    ret = tc_frc_code_from_value(&frc, fps);
    if (ret == TC_NULL_MATCH) {
        tc_log_warn(PACKAGE, "fps: failed conversion_from for fps=%f", fps);
        return 1;
    }
    
    ret = tc_frc_code_to_value(frc, &myfps);
    if (ret == TC_NULL_MATCH) {
        tc_log_warn(PACKAGE, "fps: failed conversion_to for fps=%f", fps);
        return 2;
    }
 
    if (myfps - DELTA < fps && fps < myfps + DELTA) {
        tc_log_info(PACKAGE, "fps: test for fps=%f -> OK", fps);
    } else { 
        tc_log_warn(PACKAGE, "fps: test for fps=%f -> FAILED (%f)", fps, myfps);
        ret = -1;
    }
    return ret;
}

static int test_autoloop_frc1(int frc)
{
    int ret = 0, myfrc;
    double fps;

    ret = tc_frc_code_to_value(frc, &fps);
    if (ret == TC_NULL_MATCH) {
        tc_log_warn(PACKAGE, "frc1: failed conversion_to for frc=%i", frc);
        return 1;
    }
    
    ret = tc_frc_code_from_value(&myfrc, fps);
    if (ret == TC_NULL_MATCH) {
        tc_log_warn(PACKAGE, "frc1: failed conversion_from for frc=%i", frc);
        return 2;
    }
    
    if (frc == myfrc) {
        tc_log_info(PACKAGE, "frc1: test for frc=%i -> OK", frc);
    } else { 
        tc_log_warn(PACKAGE, "frc1: test for frc=%i -> FAILED (%i)", frc, myfrc);
        ret = -1;
    }
    return ret;
}

static int test_autoloop_frc2(int frc)
{
    int ret = 0, myfrc;
    TCPair pair;

    ret = tc_frc_code_to_ratio(frc, &pair.a, &pair.b);
    if (ret == TC_NULL_MATCH) {
        tc_log_warn(PACKAGE, "frc1: failed conversion_to for frc=%i", frc);
        return 1;
    }
    
    ret = tc_frc_code_from_ratio(&myfrc, pair.a, pair.b);
    if (ret == TC_NULL_MATCH) {
        tc_log_warn(PACKAGE, "frc1: failed conversion_from for frc=%i", frc);
        return 2;
    }
    
    if (frc == myfrc) {
        tc_log_info(PACKAGE, "frc1: test for frc=%i -> OK", frc);
    } else { 
        tc_log_warn(PACKAGE, "frc1: test for frc=%i -> FAILED (%i)", frc, myfrc);
        ret = -1;
    }
    return ret;
}

static int test_autoloop_ratio(TCPair pair)
{
    int ret = 0, frc;
    TCPair mypair;
   
    ret = tc_frc_code_from_ratio(&frc, pair.a, pair.b);
    if (ret == TC_NULL_MATCH) {
        tc_log_warn(PACKAGE, "ratio: failed conversion_from for ratio=%i/%i",
                             pair.a, pair.b);
        return 2;
    }
  
    ret = tc_frc_code_to_ratio(frc, &mypair.a, &mypair.b);
    if (ret == TC_NULL_MATCH) {
        tc_log_warn(PACKAGE, "ratio: failed conversion_to for ratio=%i/%i",
                             pair.a, pair.b);
        return 1;
    }
    
    if (pair.a == mypair.a && pair.b == mypair.b) {
        tc_log_info(PACKAGE, "ratio: test for ratio=%i/%i -> OK",
                             pair.a, pair.b);
    } else { 
        tc_log_warn(PACKAGE, "ratio: test for ratio=%i/%i -> FAILED (%i/%i)",
                             pair.a, pair.b, mypair.a, mypair.b);
        ret = -1;
    }
    return ret;
}


struct p_struct {
    int frc;
    TCPair ratio;
};

static const struct p_struct frc_ratios[] = {
    { 0, {0, 0} },
    { 1, { 24000, 1001 } },
    { 2, { 24000, 1000 } },
    { 3, { 25000, 1000 } },
    { 4, { 30000, 1001 } },
    { 5, { 30000, 1000 } },
    { 6, { 50000, 1000 } },
    { 7, { 60000, 1001 } },
    { 8, { 60000, 1000 } },
    { 9, { 1000, 1000 } },
    { 10, { 5000, 1000 } },
    { 11, { 10000, 1000 } },
    { 12, { 12000, 1000 } },
    { 13, { 15000, 1000 } },
};

struct fr_struct {
    int frc;
    double fps;
};

/* testing frc/fps pairs, picked not-so-randomly */
static const struct fr_struct fps_pairs[] = {
    { 0, 0.0 },
    { 1, NTSC_FILM },
    { 3, 25.0 },
    { 4, NTSC_VIDEO },
    { 7, (2*NTSC_VIDEO) },
    { 8, 60.0 },
    { 13, 15 },
//    { 15, 0 },
//    known issue: aliasing isn't handled properly
};

int main(void)
{
    int i = 0;

    for (i = 0; i < sizeof(fps_pairs)/sizeof(fps_pairs[0]); i++) {
        test_autoloop_fps(fps_pairs[i].fps);
        test_autoloop_frc1(fps_pairs[i].frc);
    }

    for (i = 0; i < sizeof(frc_ratios)/sizeof(frc_ratios[0]); i++) {
        test_autoloop_ratio(frc_ratios[i].ratio);
        test_autoloop_frc2(frc_ratios[i].frc);
    }

    return 0;
}

/*************************************************************************/

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
