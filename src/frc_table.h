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

#endif  // FRC_TABLE_H
