/*
 *  transcode.h
 *
 *  Copyright (C) Thomas Östreich - March 2002
 *
 *  This file is part of transcode, a video stream processing tool
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

#ifndef _USEAGE_H
#define _USEAGE_H

//Chris C. Hoover <cchoover@charter.net>

void import_v4l_usage(void)
{
  printf( "  The v4l import feature allows you to capture audio and video from\n" );
  printf( "  the video for linux device you specify with the -i input file\n" );
  printf( "  selection parameter for video and the -p input file selection\n" );
  printf( "  parameter for audio. ex. -i /dev/video0 -p /dev/dsp. With the\n" );
  printf( "  import_v4l parameter you can specify the input channel on the video\n" );
  printf( "  device (ex. Composite, S-Video, Tuner) and if a tv tuner is chosen,\n" );
  printf( "  the TV station to tune to. If your composite video input is input #0\n" );
  printf( "  on the device then you would specify this input using --import_v4l 0.\n" );
  printf( "  The same would be true for the S-Video input; --import_v4l 1. For a\n" );
  printf( "  TV tuner you have several options available but you must have a .xawtv\n" );
  printf( "  config file available in your home directory. (http://bytesex.org/xawtv/)\n" );
  printf( "  You can specify the desired TV station either by name or by number.\n" );
  printf( "  --import_v4l 2,PBS or --import_v4l 2,ARD will choose the named\n" );
  printf( "  TV station. --import_v4l 2,14 or --import_v4l 2,E3 will choose\n" );
  printf( "  the TV station by it's TV channel number. In any case if settings for\n" );
  printf( "  brightness, contrast, color saturation or hue are found in the .xawtv\n" );
  printf( "  file then these will be used to set their respective values on the device.\n" );
  printf( "  As a special case --import_v4l -1 will start a recording without changing\n" );
  printf( "  the channel, station and video settings.\n" );
  printf( "  To control the length of the recording you can either use the\n  \"--record_v4l a-b\" option or the \"--duration hh:mm:ss\" option\n  (see: --more_help duration).\n  This will start the recording after a delay of \"a\" secs and stop\n" );
  printf( "  recording after \"b\" secs with (b-a)*fps frames captured.\n" );
  printf( "  If your system is fast enough to capture at full speed (i.e. 25fps for PAL\n" );
  printf( "  or 29.97fps for NTSC) then the recording will end when the desired number\n" );
  printf( "  of seconds have elapsed.\n" );
  printf( "  ex. transcode -i /dev/video0 -p /dev/dsp -x v4l,v4l -y divx4 -V \\\n" );
  printf( "                --import_v4l 2,ARD -u 100 --record-v4l 0-1800 -o test.avi\n" );
  exit( 0 );
}

void duration_usage(void)
{
  printf( "Use the --duration hh:mm:ss option to limit the length of an import_v4l\n");
  printf( "recording to hh hours, mm minutes and ss seconds. (see: --more_help import_v4l)\n");
  printf( "ex. transcode [...] --duration 1:23:45\n" );
  exit( 0 );
}

#endif
