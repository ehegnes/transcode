/*
 *  tcmplex.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  This file is part of transcode, a linux video stream processing tool
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "main.h"
#include "bbencode.h"


static int verbose=0;

extern int domplex(int has_video, int has_audio);

#define EXE "tcmplex"

/* ------------------------------------------------------------ 
 *
 * print a usage/version message
 *
 * ------------------------------------------------------------*/

void version(char *exe)
{
    // print id string to stderr
    fprintf(stderr, "%s (%s v%s) (C) 2001 Thomas Oestreich\n", exe, PACKAGE, VERSION);
}

void usage()
{
  version(EXE);

  fprintf(stderr,"\nUsage: %s [options] [-]\n", EXE);
  fprintf(stderr,"\t -i name        video stream filename\n");
  fprintf(stderr,"\t -p name        audio stream (track 0) filename\n");
  fprintf(stderr,"\t -s name        audio stream (track 1) filename (optional)\n");
  fprintf(stderr,"\t -o name        muliplexed program/system stream filename\n");
  fprintf(stderr,"\t -m mode        predefined settings [1]\n");
  fprintf(stderr,"\t                1 = mpeg1 vbr, buffer 46Kb (*** default XVCD)\n");
  fprintf(stderr,"\t                b = mpeg1 vbr, buffer 224Kb (experimental)\n");
  fprintf(stderr,"\t                2 = mpeg2 vbr\n");
  fprintf(stderr,"\t                d = DVD\n");
  fprintf(stderr,"\t                s = SVCD\n");
  fprintf(stderr,"\t                v = VCD\n"); 

  //fprintf(stderr,"\t -N             NTSC mode [PAL]\n");
  //fprintf(stderr,"\t -P             set 3:2 pulldown flags [off]\n");

  fprintf(stderr,"\t -D v[,a[,a1]]  sync correction for video,audio0,audio1 in ms\n");

  fprintf(stderr,"\t -c a-b         multiplex selected time interval in seconds [all]\n");
  
  fprintf(stderr,"\t -B             generates a profile template on stdout [off]\n");
  fprintf(stderr,"\t -F filename    user profile filename [off]\n");
  fprintf(stderr,"\t -d verbosity   verbosity mode [1]\n");
  fprintf(stderr,"\t -v             print version\n");

  exit(0);
}

int main(int argc, char **argv)
{
  char mux_type      = PRO_MPEG1;
  int  has_audio     = 0;
  int  has_video     = 0;
  int  has_output    = 0;
  int  tv_type       = ENCODE_PAL;
  int  pulldown      = 0;

  long ivideo_delay_ms=-1, iaudio_delay_ms=-1, iaudio1_delay_ms=-1;

  long mux_start=-1, mux_stop=-1;

  char *profile_name=NULL;

  int ch;

   while ((ch = getopt(argc, argv, "o:i:vBd:p:m:F:s:ND:Pc:h?")) != -1) {
      
	switch (ch) {
	  
	case 'i': 
	  
	  if(optarg[0]=='-') usage();
	  strcpy(VideoFilename, optarg);
	  has_video = 1;
	  
	  break;

	case 'F': 
	  
	  if(optarg[0]=='-') usage();
	  profile_name = optarg;
	  
	  break;

	case 'N': 
	  
	  tv_type = ENCODE_NTSC;
	  break;

	case 'P': 
	  
	  pulldown = 1;
	  break;

	case 'm': 
	  
	  if(optarg[0]=='-') usage();
	  memcpy((char*) &mux_type, optarg, 1);
	  
	  break;

	case 'p': 
	  
	  if(optarg[0]=='-') usage();
	  strcpy(AudioFilename, optarg);
	  has_audio = 1;
	  break;

	case 's': 
	  
	  if(optarg[0]=='-') usage();
	  strcpy(Audio1Filename, optarg);
	  has_audio = 1;
	  break;

	case 'o': 
	  
	  if(optarg[0]=='-') usage();
	  strcpy(ProgramFilename, optarg);
	  has_output = 1;
	  break;
	  
	case 'd': 
	  
	  if(optarg[0]=='-') usage();
	  verbose = atoi(optarg);
	  
	  break;

	case 'D': 
	  
	  if(optarg[0]=='-') usage();

	  //overwrite profile defaults

	  sscanf(optarg, "%ld,%ld,%ld", &ivideo_delay_ms, &iaudio_delay_ms, &iaudio1_delay_ms);
	  break;

	case 'c': 
	  
	  if(optarg[0]=='-') usage();

	  //overwrite profile defaults

	  sscanf(optarg, "%ld-%ld", &mux_start, &mux_stop);
	  break;

	    
	case 'B':
	  
	  bb_gen_profile();
	  exit(0);
	  break;

	case 'v': 
	  
	  version(EXE);
	  exit(0);
	  break;
	  
	case '?':
	case 'h':
	default:
	    usage();
	}
    }

   
   // need at least a file name
   if(argc==1) usage();
   
   if(!has_output) {
       fprintf(stderr, "output filename option -o required\n");
       exit(1);
   }
   
   //-- setup parameters --
   //----------------------
   bb_set_profile(profile_name, mux_type, tv_type, 0, 0, pulldown, 1, 0, 0);

   //ThOe cmd line parameter have higher priority

   video_delay_ms = (ivideo_delay_ms!=-1) ? ivideo_delay_ms:video_delay_ms;
   audio_delay_ms = (iaudio_delay_ms!=-1) ? iaudio_delay_ms:audio_delay_ms;
   audio1_delay_ms = (iaudio1_delay_ms!=-1) ? iaudio1_delay_ms:audio1_delay_ms;

   mux_start_time = (mux_start != -1) ? mux_start:mux_start_time;
   mux_stop_time = (mux_stop != -1) ? mux_stop:mux_stop_time;

   //max_file_size = 700;

   //-- do the job --
   //----------------
   domplex(has_video, has_audio);
   
   fprintf(stderr, "\n\n");
   
   return EXIT_SUCCESS;
}

