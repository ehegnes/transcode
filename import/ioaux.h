/*
 *  ioaux.h
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  This file is part of transcode, a video stream  processing tool
 *
 *  transcode is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  transcode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _IOAUX_H
#define _IOAUX_H

#include "transcode.h"
#include "magic.h"


/* this exit is provided by the import module or frontend */
extern void import_exit(int ret);

long fileinfo(int fd, int skipy);
int fileinfo_dir(char *name, int *fd, long *cc);
long streaminfo(int fd);

char *filetype(long magic);
char *filemagic(long magic);

void scan_pes(int verbose, FILE *fd);
void probe_pes(info_t *ipipe);
int probe_dvd(info_t *ipipe);

int open_dir(char *name, int *fd, long *stype);

int enc_bitrate(long frames, double fps, int abit, char *s, int cdsize);
unsigned int stream_read_int16(unsigned char *s);
unsigned int stream_read_int32(unsigned char *s);

double read_time_stamp(unsigned char *s);
unsigned int read_tc_time_stamp(char *s);
long read_time_stamp_long(unsigned char *s);
int fps2frc(double _fps);
void import_info(int code, char *EXE);


void probe_ts(info_t *ipipe);
int ts_read(int fd_in, int fd_out, int demux_pid);

#define VOB_PACKET_SIZE   0x800
#define VOB_PACKET_OFFSET    22

typedef struct sync_info_s {

  long int enc_frame;
  long int adj_frame;

  long int sequence;

  double dec_fps;
  double enc_fps;

  double pts;

  int pulldown;
  int drop_seq;

} sync_info_t;


//packet type
#define P_ID_AC3  0xbd
#define P_ID_MP3  0xbc
#define P_ID_MPEG 0xe0
#define P_ID_PROG 0xbb
#define P_ID_PADD 0xbe

//stream type
#define TC_STYPE_ERROR        0xFFFFFFFF
#define TC_STYPE_UNKNOWN      0x00000000
#define TC_STYPE_FILE         0x00000001
#define TC_STYPE_STDIN        0x00000002

#define ERROR_END_OF_STREAM        1
#define ERROR_INVALID_FRAME        2
#define ERROR_INVALID_FRAME_SIZE   3
#define ERROR_INVALID_HEADER       4
#endif   /* _IOAUX_H */
