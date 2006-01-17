/*
 * frc_table.h - define the frame rate code table
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#ifndef FRC_TABLE_H
#define FRC_TABLE_H

static const double frc_table[16] = {0,
				     NTSC_FILM, 24,
				     25, NTSC_VIDEO, 30,
				     50, (2*NTSC_VIDEO), 60,
				     1, 5, 10, 12, 15,
				     0, 0};

static const int frc_codes[16][2] = {
    { 0, 0 },
    { 24000, 1001 },
    { 24000, 1000 },
    { 25000, 1000 },
    { 30000, 1001 },
    { 30000, 1000 },
    { 50000, 1000 },
    { 60000, 1001 },
    { 60000, 1000 },
    // XXX
    { 1000, 1000 },
    { 5000, 1000 },
    { 10000, 1000 },
    { 12000, 1000 },
    { 15000, 1000 },
    // XXX XXX
    { 0, 0 },
    { 0, 0 },
};

#endif  // FRC_TABLE_H
