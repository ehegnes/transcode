/*
 *  ioaux.c
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


#include "ioaux.h"
#include "xio.h"

void import_info(int code, char *EXE)
{
    fprintf(stderr, "[%s] exit code (%d)\n", EXE, code);
}


unsigned int stream_read_int16(unsigned char *s)
{
  unsigned int a, b, result;

  a = s[0];
  b = s[1];

  result = (a << 8) | b;
  return result;
}


unsigned int stream_read_int32(unsigned char *s)
{
  unsigned int a, b, c, d, result;

  a = s[0];
  b = s[1];
  c = s[2];
  d = s[3];

  result = (a << 24) | (b << 16) | (c << 8) | d;
  return result;
}


double read_time_stamp(unsigned char *s)
{

  unsigned long i, j;
  unsigned long clock_ref=0, clock_ref_ext=0;

  if(s[0] & 0x40) {

    i = stream_read_int32(s);
    j = stream_read_int16(s+4);

    if(i & 0x40000000 || (i >> 28) == 2)
      {
	clock_ref  = ((i & 0x31000000) << 3);
	clock_ref |= ((i & 0x03fff800) << 4);
	clock_ref |= ((i & 0x000003ff) << 5);
	clock_ref |= ((j & 0xf800) >> 11);
	clock_ref_ext = (j >> 1) & 0x1ff;
      }
  }

  return (double)(clock_ref + clock_ref_ext / 300) / 90000;
}

unsigned int read_tc_time_stamp(char *s)
{

  unsigned long i, j;
  unsigned long clock_ref=0, clock_ref_ext=0;

  if(s[0] & 0x40) {

    i = stream_read_int32(s);
    j = stream_read_int16(s+4);

    if(i & 0x40000000 || (i >> 28) == 2) {
      clock_ref  = ((i & 0x31000000) << 3);
      clock_ref |= ((i & 0x03fff800) << 4);
      clock_ref |= ((i & 0x000003ff) << 5);
      clock_ref |= ((j & 0xf800) >> 11);
      clock_ref_ext = (j >> 1) & 0x1ff;
    }
  }

  return ((unsigned int) (clock_ref * 300 + clock_ref_ext));
}


long read_time_stamp_long(unsigned char *s)
{

  unsigned long i, j;
  unsigned long clock_ref=0, clock_ref_ext=0;

  if(s[0] & 0x40) {

    i = stream_read_int32(s);
    j = stream_read_int16(s+4);

    if(i & 0x40000000 || (i >> 28) == 2)
      {
	clock_ref  = ((i & 0x31000000) << 3);
	clock_ref |= ((i & 0x03fff800) << 4);
	clock_ref |= ((i & 0x000003ff) << 5);
	clock_ref |= ((j & 0xf800) >> 11);
	clock_ref_ext = (j >> 1) & 0x1ff;
      }
  }

  return (clock_ref);
}

int fps2frc(double _fps)
{

    float fps=_fps;

    //official
    if(fps<=0.0f) return(0);
    if(fps>23.0f && fps<24.0f) return(1);
    if(fps==24.0f) return(2);
    if(fps==25.0f) return(3);
    if(fps>29.0f && fps<30.0f) return(4);
    if(fps==30.0f) return(5);
    if(fps==50.0f) return(6);
    if(fps>59.0f && fps < 60.0f) return(7);
    if(fps==60.0f) return(8);

    //unofficial
    if(fps==1.0f) return(9);
    if(fps==5.0f) return(10);
    if(fps==10.0f) return(11);
    if(fps==12.0f) return(12);
    if(fps==15.0f) return(13);

    return(0);
}

