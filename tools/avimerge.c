/*
 *  avimerge.c
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "avilib.h"
#include "../config.h"
#include "transcode.h"

#define EXE "avimerge"

void version()
{
  printf("%s (%s v%s) (C) 2001-2002 Thomas Östreich\n", EXE, PACKAGE, VERSION);
}

void usage(int status)
{
    version();
    printf("\nUsage: %s [options]\n", EXE);
    printf("\t -o file                   output file name\n");
    printf("\t -i file1 [file2 [...]]    input file(s)\n");
    printf("\t -p file                   multiplex additional audio track from file\n");
    printf("\t -a num                    audio track number [0]\n");
    exit(status);
}

static char data[SIZE_RGB_FRAME];

long sum_frames = 0;

int merger(avi_t *out, char *file)
{
    avi_t *in;
    long frames, n, bytes;
    int key, chan, j, aud_tracks;
    
    if(NULL == (in = AVI_open_input_file(file,1))) {
	AVI_print_error("AVI open");
	return(-1);
    }
    
    AVI_seek_start(in);
    frames =  AVI_video_frames(in);
    aud_tracks = AVI_audio_tracks(in);    

    for (n=0; n<frames; ++n) {
      
      // video
      bytes = AVI_read_frame(in, data, &key);
      
      if(bytes < 0) {
	AVI_print_error("AVI read video frame");
	return(-1);
      }
      
      if(AVI_write_frame(out, data, bytes, key)<0) {
	AVI_print_error("AVI write video frame");
	return(-1);
      }
      
      
      for(j=0; j<aud_tracks; ++j) {
	  
	  AVI_set_audio_track(in, j);
	  
	  // audio
	  chan = AVI_audio_channels(in);
	  
	  if(chan) {
	      while (AVI_can_read_audio(in)) {
		  bytes = AVI_audio_size(in, n);
		  
		  if(AVI_read_audio(in, data, bytes) < 0) {
		      AVI_print_error("AVI audio read frame");
		      return(-1);
		  }
	      
		  AVI_set_audio_track(out, j);

		  if(AVI_write_audio(out, data, bytes)<0) {
		      AVI_print_error("AVI write audio frame");
		      return(-1);
		  } 
	      }
	      
	  }
      }
      // progress
      fprintf(stderr, "[%s] (%06ld-%06ld)\r", file, sum_frames, sum_frames + n);
    }
    fprintf(stderr, "\n");
    
    AVI_close(in);
    
    sum_frames += n;
    
    return(0);
}


int main(int argc, char *argv[])
{
    
  
  avi_t *avifile, *avifile1, *avifile2;
  
  char *outfile=NULL, *infile=NULL, *audfile=NULL;
  
  long rate, mp3rate;
  
  int j, ch, cc=0, track_num=0;
  int width, height, format, chan, bits;
  
  double fps;
  
  char *codec;
  
  long offset, frames, n, bytes;
  
  int key;
  
  int aud_tracks;
  
  if(argc==1) usage(EXIT_FAILURE);
  
  while ((ch = getopt(argc, argv, "a:i:o:p:?h")) != -1) {
    
    switch (ch) {
      
    case 'i':
      
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      infile = optarg;
      
      break;
      
    case 'a':
      
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      track_num = atoi(optarg);
      
      if(track_num<0) usage(EXIT_FAILURE);
      
      break;
      
    case 'o':
      
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      outfile = optarg;
      
      break;
      
    case 'p':
      
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      audfile = optarg;
      
      break;
      
    case 'h':
      usage(EXIT_SUCCESS);
    default:
      usage(EXIT_FAILURE);
    }
  }
  
  if(outfile == NULL || infile == NULL) usage(EXIT_FAILURE);
  
  printf("scanning file %s for video/audio parameter\n", infile);
  
  // open first file for video/audio info read only
  if(NULL == (avifile1 = AVI_open_input_file(infile,1))) {
    AVI_print_error("AVI open");
    exit(1);
  }
  
  AVI_info(avifile1);
  
  // safety checks
  
  if(strcmp(infile, outfile)==0) {
    printf("error: output filename conflicts with input filename\n");
    exit(1);
  }
  
  ch = optind;
  
  while (ch < argc) {
    
    if(AVI_file_check(argv[ch])) {
      printf("error: file not found\n");
      exit(1);
    }
    
    if(strcmp(argv[ch++], outfile)==0) {
      printf("error: output filename conflicts with input filename\n");
      exit(1);
    }
  }
  
  // open output file
  if(NULL == (avifile = AVI_open_output_file(outfile))) {
    AVI_print_error("AVI open");
    exit(1);
  }
  
  // read video info;
  
  width =  AVI_video_width(avifile1);
  height =  AVI_video_height(avifile1);
  
  fps    =  AVI_frame_rate(avifile1);
  codec  =  AVI_video_compressor(avifile1);
  
  //set video in outputfile
  AVI_set_video(avifile, width, height, fps, codec);

  //multi audio tracks?
  aud_tracks = AVI_audio_tracks(avifile1);
  
  for(j=0; j<aud_tracks; ++j) {
    
      AVI_set_audio_track(avifile1, j);
    
      rate   =  AVI_audio_rate(avifile1);
      chan   =  AVI_audio_channels(avifile1);
      bits   =  AVI_audio_bits(avifile1);
    
      format =  AVI_audio_format(avifile1);
      mp3rate=  AVI_audio_mp3rate(avifile1);
      
      //set next track of output file
      AVI_set_audio_track(avifile, j);
      AVI_set_audio(avifile, chan, rate, bits, format, mp3rate);
  }
  
  if(audfile!=NULL) goto audio_merge;

  // close reopen in merger function
  AVI_close(avifile1);

  //-------------------------------------------------------------
  
  printf("merging multiple AVI-files (concatenating) ...\n");
  
  // extract and write to new files
  
  printf ("file %02d %s\n", ++cc, infile);
  merger(avifile, infile);
  
  while (optind < argc) {
    
    printf ("file %02d %s\n", ++cc, argv[optind]);
    merger(avifile, argv[optind++]);
  }
  
  // close new AVI file
  
  AVI_close(avifile);
  
  printf("... done merging %d files in %s\n", cc, outfile);
  
  // reopen file for video/audio info
  if(NULL == (avifile = AVI_open_input_file(outfile,1))) {
    AVI_print_error("AVI open");
    exit(1);
  }
  AVI_info(avifile);
  
  return(0);
  
  //-------------------------------------------------------------
  
 audio_merge:
  
  printf("merging audio %s track (multiplexing) ...\n", audfile);
  
  // open audio file read only
  if(NULL == (avifile2 = AVI_open_input_file(audfile,1))) {
    AVI_print_error("AVI open");
    exit(1);
  }
  
  AVI_info(avifile2);
  
  //switch to requested track 
  
  if(AVI_set_audio_track(avifile2, track_num)<0) {
    fprintf(stderr, "invalid audio track\n");
  }
  
  rate   =  AVI_audio_rate(avifile2);
  chan   =  AVI_audio_channels(avifile2);
  bits   =  AVI_audio_bits(avifile2);
  
  format =  AVI_audio_format(avifile2);
  mp3rate=  AVI_audio_mp3rate(avifile2);
  
  //set next track
  AVI_set_audio_track(avifile, aud_tracks);
  AVI_set_audio(avifile, chan, rate, bits, format, mp3rate);
  
  AVI_seek_start(avifile1);
  frames =  AVI_video_frames(avifile1);
  offset = 0;

  printf ("file %02d %s\n", ++cc, infile);

  for (n=0; n<frames; ++n) {
    
    // video
    bytes = AVI_read_frame(avifile1, data, &key);
    
    if(bytes < 0) {
      AVI_print_error("AVI read video frame");
      return(-1);
    }
    
    if(AVI_write_frame(avifile, data, bytes, key)<0) {
      AVI_print_error("AVI write video frame");
      return(-1);
    }
    
    for(j=0; j<aud_tracks; ++j) {
      
      AVI_set_audio_track(avifile1, j);
      
      // audio
      chan = AVI_audio_channels(avifile1);
      
      if(chan) {
	while (AVI_can_read_audio(avifile1)) {
	  bytes = AVI_audio_size(avifile1, n);
	
	  if(AVI_read_audio(avifile1, data, bytes) < 0) {
	    AVI_print_error("AVI audio read frame");
	    return(-1);
	  }
	
	  AVI_set_audio_track(avifile, j);

	  if(AVI_write_audio(avifile, data, bytes)<0) {
	    AVI_print_error("AVI write audio frame");
	    return(-1);
	  } 
	}
	  
      }
    }
    
    
    // merge additional track
    
    bytes = AVI_read_frame(avifile2, data, &key);
    // audio
    chan = AVI_audio_channels(avifile2);
    
    if(chan) {
      while (AVI_can_read_audio(avifile2)) {
	bytes = AVI_audio_size(avifile2, n);
      
	if(AVI_read_audio(avifile2, data, bytes) < 0) {
	  AVI_print_error("AVI audio read frame");
	  return(-1);
	}
      
	AVI_set_audio_track(avifile, aud_tracks);
      
	if(AVI_write_audio(avifile, data, bytes)<0) {
	  AVI_print_error("AVI write audio frame");
	  return(-1);
	} 
      }
	  
    }

    // progress
    fprintf(stderr, "[%s] (%06ld-%06ld)\r", outfile, offset, offset + n);

  }
  
  fprintf(stderr,"\n");
  
  offset = frames;

  //more files to merge?
  
  AVI_close(avifile1);
  
  while (optind < argc) {

    printf ("file %02d %s\n", ++cc, argv[optind]);
    
    if(NULL == ( avifile1 = AVI_open_input_file(argv[optind++],1))) {
      AVI_print_error("AVI open");
      goto finish;
    }
    
    AVI_seek_start(avifile1);
    frames =  AVI_video_frames(avifile1);
    
    for (n=0; n<frames; ++n) {
      
      // video
      bytes = AVI_read_frame(avifile1, data, &key);
      
      if(bytes < 0) {
	AVI_print_error("AVI read video frame");
	return(-1);
      }
      
      if(AVI_write_frame(avifile, data, bytes, key)<0) {
	AVI_print_error("AVI write video frame");
	return(-1);
      }
      
      for(j=0; j<aud_tracks; ++j) {
	
	AVI_set_audio_track(avifile1, j);
	
	// audio
	chan = AVI_audio_channels(avifile1);
	
	if(chan) {
	  while (AVI_can_read_audio(avifile1)) {
	  bytes = AVI_audio_size(avifile1, n);
	  
	  if(AVI_read_audio(avifile1, data, bytes) < 0) {
	    AVI_print_error("AVI audio read frame");
	    return(-1);
	  }
	  
	  AVI_set_audio_track(avifile, j);
	  
	  if(AVI_write_audio(avifile, data, bytes)<0) {
	    AVI_print_error("AVI write audio frame");
	    return(-1);
	  }
	  }
	  
	}
      }
      
      // merge additional track
      // audio
      
      chan = AVI_audio_channels(avifile2);
      
      if(chan) {
	bytes = AVI_audio_size(avifile2, offset+n);
	
	if(AVI_read_audio(avifile2, data, bytes) < 0) {
	  AVI_print_error("AVI audio read frame");
	  return(-1);
	}
	
	AVI_set_audio_track(avifile, aud_tracks);
	
	if(AVI_write_audio(avifile, data, bytes)<0) {
	  AVI_print_error("AVI write audio frame");
	  return(-1);
	} 
      }
      
      // progress
      fprintf(stderr, "[%s] (%06ld-%06ld)\r", outfile, offset, offset + n);
    }
    
    fprintf(stderr, "\n");
    
    offset += frames;
    AVI_close(avifile1);
  }
  
 finish:
  
  // close new AVI file
  
  printf("... done multiplexing in %s\n", outfile);
    
  AVI_info(avifile);
  AVI_close(avifile);
  
  return(0);
  
}
