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
// TODO: Simplify this code. Immediatly

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "avilib.h"
#include "../config.h"
#include "transcode.h"
#include "aud_scan.h"

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
    printf("\t -b n                      handle vbr audio [1] (should not hurt)\n");
    printf("\t -f FILE                   read AVI comments from FILE [off]\n");
    exit(status);
}

static char data[SIZE_RGB_FRAME];
static char *comfile = NULL;
long sum_frames = 0;
int is_vbr=1;

int merger(avi_t *out, char *file)
{
    avi_t *in;
    long frames, n, bytes;
    int key, chan, j, aud_tracks;
    int aud_bitrate=0, format;

    double vid_ms = 0.0, fps;
    double aud_ms[AVI_MAX_TRACKS];

    for (j=0; j<AVI_MAX_TRACKS; j++) 
	aud_ms[j] = 0.0;
    
    if(NULL == (in = AVI_open_input_file(file,1))) {
	AVI_print_error("AVI open");
	return(-1);
    }
    
    AVI_seek_start(in);
    fps    =  AVI_frame_rate(in);
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

      
      vid_ms = (n+1)*1000.0/fps;
      
      for(j=0; j<aud_tracks; ++j) {
	  
	int rate, bits;
	  AVI_set_audio_track(in, j);
          format =  AVI_audio_format(in);
	  
	  // audio
	  chan = AVI_audio_channels(in);
	  rate = AVI_audio_rate(in);
	  bits = AVI_audio_bits(in);
	  bits = bits==0?16:bits;
	  AVI_set_audio_track(out, j);
	  
	  if(chan) {
	      if (tc_format_ms_supported(format)) {

		  while (aud_ms[j] < vid_ms) {

		      aud_bitrate = format==0x1?1:0;

		      if( (bytes = AVI_read_audio_chunk(in, data)) < 0) {
			  AVI_print_error("AVI audio read frame");
			  aud_ms[j] = vid_ms;
			  break;
		      }      
		      //fprintf(stderr, "len (%ld)\n", bytes);

		      if(AVI_write_audio(out, data, bytes)<0) {
			  AVI_print_error("AVI write audio frame");
			  return(-1);
		      }

		      if (bytes == 0) {
			  aud_ms[j] = vid_ms;
			  break;
		      }

		      if ( !aud_bitrate && tc_get_audio_header(data, bytes, format, NULL, NULL, &aud_bitrate)<0) {
			  // if this is the last frame of the file, slurp in audio chunks
			  if (n == frames-1) continue;
			  aud_ms[j] = vid_ms;
		      } else 
			  aud_ms[j] += (bytes*8.0)/(format==0x1?((double)(rate*chan*bits)/1000.0):aud_bitrate);
		  }
	      } else {
		  do {
		      if ( (bytes = AVI_read_audio_chunk(in, data) ) < 0) { 
			  AVI_print_error("AVI audio read frame"); 
		      }
 
		      if(AVI_write_audio(out, data, bytes)<0) {
			  AVI_print_error("AVI write audio frame");
			  return(-1);
		      } 
		  } while (AVI_can_read_audio(in));
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
  int width, height, format=0, format_add, chan, bits;
  
  double fps;
  
  char *codec;
  
  long offset, frames, n, bytes;
  
  int key;
  
  int aud_tracks;

  // for mp3 audio
  FILE *f=NULL;
  int len, headlen, chan_i, rate_i, mp3rate_i;
  unsigned long vid_chunks=0;
  char head[8];
  off_t pos;
  double aud_ms = 0.0, vid_ms = 0.0;
  double aud_ms_w[AVI_MAX_TRACKS];

  
  if(argc==1) usage(EXIT_FAILURE);
  
  while ((ch = getopt(argc, argv, "a:b:i:o:p:f:?h")) != -1) {
    
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
      
    case 'b':
      
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      is_vbr = atoi(optarg);
      
      if(is_vbr<0) usage(EXIT_FAILURE);
      
      break;
      
    case 'o':
      
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      outfile = optarg;
      
      break;
      
    case 'p':
      
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      audfile = optarg;
      
      break;

    case 'f':
      
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      comfile = optarg;

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

  if (comfile!=NULL)
    AVI_set_comment_fd(avifile, open(comfile, O_RDONLY));

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
      AVI_set_audio_vbr(avifile, is_vbr);
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


// *************************************************
// Merge the audio track of an additional AVI file
// *************************************************
  
 audio_merge:
  
  printf("merging audio %s track (multiplexing) ...\n", audfile);
  
  // open audio file read only
  if(NULL == (avifile2 = AVI_open_input_file(audfile,1))) {
    int f=open(audfile, O_RDONLY);
    char head[8];
    if (f>0 && ( 8 == read(f, head, 8)) ) {
      if (tc_probe_audio_header(head, 8) > 0) {
	close(f);
	goto merge_mp3;
      }
      close(f);
    }
    
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
  AVI_set_audio_vbr(avifile, is_vbr);
  
  AVI_seek_start(avifile1);
  frames =  AVI_video_frames(avifile1);
  offset = 0;

  printf ("file %02d %s\n", ++cc, infile);

  for (n=0; n<AVI_MAX_TRACKS; n++)
    aud_ms_w[n] = 0.0;
  vid_chunks=0;

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
    ++vid_chunks;
    vid_ms = vid_chunks*1000.0/fps;
    
    for(j=0; j<aud_tracks; ++j) {
      
      AVI_set_audio_track(avifile1, j);
      AVI_set_audio_track(avifile, j);
      format = AVI_audio_format(avifile1);
      rate   = AVI_audio_rate(avifile1);
      chan   = AVI_audio_channels(avifile1);
      bits   = AVI_audio_bits(avifile1);
      bits   = bits==0?16:bits;
      
      // audio
      chan = AVI_audio_channels(avifile1);
      if(chan) {
	if (tc_format_ms_supported(format)) {
	  while (aud_ms_w[j] < vid_ms) {

	    mp3rate_i = format==0x1?1:0;

	    if( (bytes = AVI_read_audio_chunk(avifile1, data)) < 0) {
	      AVI_print_error("AVI audio read frame");
	      aud_ms_w[j] = vid_ms;
	      break;
	    }      

	    if(AVI_write_audio(avifile, data, bytes)<0) {
	      AVI_print_error("AVI write audio frame");
	      return(-1);
	    }

	    if (bytes == 0) {
	      aud_ms_w[j] = vid_ms;
	      break;
	    }
	    if ( !mp3rate_i && tc_get_audio_header(data, bytes, format, NULL, NULL, &mp3rate_i)<0) { 
	      // if this is the last frame of the file, slurp in audio chunks
	      if (n == frames-1) continue;
	      aud_ms_w[j] = vid_ms;
	    } else {
	      aud_ms_w[j] += (bytes*8.0)/(format==0x1?((double)(rate*chan*bits)/1000.0):mp3rate_i);
	    }

	    /*
	       fprintf(stderr, "%s track (%d) %8.0lf->%8.0lf len (%ld) rate (%d)\n", 
		    format==0x55?"MP3":format==0x1?"PCM":"AC3", 
	       j, vid_ms, aud_ms_w[j], bytes, mp3rate_i); 
	       */
	  }
	} else { // fallback solution
	  do {

	    if( (bytes = AVI_read_audio_chunk(avifile1, data)) < 0) {
	      AVI_print_error("AVI audio read frame");
	      return(-1);
	    }


	    if(AVI_write_audio(avifile, data, bytes)<0) {
	      AVI_print_error("AVI write audio frame");
		return(-1);
	      } 
	    } while (AVI_can_read_audio(avifile1));

	} // else
      } // chan

    }
    
    
    // merge additional track
    
    //bytes = AVI_read_frame(avifile2, data, &key);
    
    // audio
    chan = AVI_audio_channels(avifile2);
    AVI_set_audio_track(avifile, aud_tracks);
    format = AVI_audio_format(avifile2);
    rate   = AVI_audio_rate(avifile2);
    bits   = AVI_audio_bits(avifile2);
    bits   = bits==0?16:bits;
      
      if(chan) {
	if (tc_format_ms_supported(format)) {
	  while (aud_ms < vid_ms) {

	    mp3rate_i = format==0x1?1:0;

	    if( (bytes = AVI_read_audio_chunk(avifile2, data)) < 0) {
	      AVI_print_error("AVI audio read frame");
	      aud_ms = vid_ms;
	      break;
	    }      

	    if(AVI_write_audio(avifile, data, bytes)<0) {
	      AVI_print_error("AVI write audio frame");
	      return(-1);
	    }

	    if (bytes == 0) {
	      aud_ms = vid_ms;
	      break;
	    }

	    if ( !mp3rate_i && tc_get_audio_header(data, bytes, format, NULL, NULL, &mp3rate_i)<0) { 
	      // if this is the last frame of the file, slurp in audio chunks
	      if (n == frames-1) continue;
	      aud_ms = vid_ms;
	    } else {
	      aud_ms += (bytes*8.0)/(format==0x1?((double)(rate*chan*bits)/1000.0):mp3rate_i);
	    }

	    /*
	       fprintf(stderr, "%s track (%d) %8.0lf->%8.0lf len (%ld) rate (%d)\n", 
	       (format==0x55)?"MP3":"AC3", 
	       j, vid_ms, aud_ms, bytes, mp3rate_i); 
	       */
	  }
	} else { // fallback solution
	  do {

	    if( (bytes = AVI_read_audio_chunk(avifile2, data)) < 0) {
	      AVI_print_error("AVI audio read frame");
	      return(-1);
	    }


	    if(AVI_write_audio(avifile, data, bytes)<0) {
	      AVI_print_error("AVI write audio frame");
	      return(-1);
	    } 
	  } while (AVI_can_read_audio(avifile1));

	} // else
      } // chan
#if 0
    if(chan) {
      do {
      
	if( (bytes = AVI_read_audio_chunk(avifile2, data)) < 0) {
	  AVI_print_error("AVI audio read frame");
	  return(-1);
	}
      
      
	if(AVI_write_audio(avifile, data, bytes)<0) {
	  AVI_print_error("AVI write audio frame");
	  return(-1);
	} 
      } while (AVI_can_read_audio(avifile2));
	  
    }
#endif

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
	AVI_set_audio_track(avifile, j);
	format = AVI_audio_format(avifile1);
	
	// audio
	chan   = AVI_audio_channels(avifile1);
	rate   = AVI_audio_rate(avifile1);
	bits   = AVI_audio_bits(avifile1);
	bits   = bits==0?16:bits;
	
	if(chan) {
	  if (tc_format_ms_supported(format)) {
	    while (aud_ms_w[j] < vid_ms) {

	      mp3rate_i = format==0x1?1:0;

	      if( (bytes = AVI_read_audio_chunk(avifile1, data)) < 0) {
		AVI_print_error("AVI audio read frame");
		aud_ms_w[j] = vid_ms;
		break;
	      }      

	      if(AVI_write_audio(avifile, data, bytes)<0) {
		AVI_print_error("AVI write audio frame");
		return(-1);
	      }

	      if (bytes == 0) {
		aud_ms_w[j] = vid_ms;
		break;
	      }
	      
	      if ( !mp3rate_i && tc_get_audio_header(data, bytes, format, NULL, NULL, &mp3rate_i)<0) { 
		// if this is the last frame of the file, slurp in audio chunks
		if (n == frames-1) continue;
		aud_ms_w[j] = vid_ms;
	      } else {
		aud_ms_w[j] += (bytes*8.0)/(format==0x1?((double)(rate*chan*bits)/1000.0):mp3rate_i);
	      }

	      /*
		 fprintf(stderr, "%s track (%d) %8.0lf->%8.0lf len (%ld) rate (%d)\n", 
		 (format==0x55)?"MP3":"AC3", 
		 j, vid_ms, aud_ms_w[j], bytes, mp3rate_i); 
	       */
	    }
	  } else { // fallback solution
	    do {

	      if( (bytes = AVI_read_audio_chunk(avifile1, data)) < 0) {
		AVI_print_error("AVI audio read frame");
		return(-1);
	      }


	      if(AVI_write_audio(avifile, data, bytes)<0) {
		AVI_print_error("AVI write audio frame");
		return(-1);
	      } 
	    } while (AVI_can_read_audio(avifile1));

	  } // else
	} // chan
      } // aud_tracks
      
      // merge additional track
      // audio
      
      chan = AVI_audio_channels(avifile2);
      format = AVI_audio_format(avifile2);
      AVI_set_audio_track(avifile, aud_tracks);
      rate   = AVI_audio_rate(avifile2);
      bits   = AVI_audio_bits(avifile2);
      bits   = bits==0?16:bits;
      
      if(chan) {
	if (tc_format_ms_supported(format)) {
	  while (aud_ms < vid_ms) {
	    mp3rate = (format == 0x1)?1:0;

	    if( (bytes = AVI_read_audio_chunk(avifile2, data)) < 0) {
	      AVI_print_error("AVI audio read frame");
	      aud_ms = vid_ms;
	      break;
	    }      

	    if(AVI_write_audio(avifile, data, bytes)<0) {
	      AVI_print_error("AVI write audio frame");
	      return(-1);
	    }

	    if (bytes == 0) {
	      aud_ms = vid_ms;
	      break;
	    }
	    if ( !mp3rate_i && tc_get_audio_header(data, bytes, format, NULL, NULL, &mp3rate_i)<0) { 
	      // if this is the last frame of the file, slurp in audio chunks
	      if (n == frames-1) continue;
	      aud_ms = vid_ms;
	    } else {
	      aud_ms += (bytes*8.0)/(format==0x1?((double)(rate*chan*bits)/1000.0):mp3rate_i);
	    }

	    /*
	       fprintf(stderr, "%s track (%d) %8.0lf->%8.0lf len (%ld) rate (%d)\n", 
	       (format==0x55)?"MP3":"AC3", 
	       j, vid_ms, aud_ms, bytes, mp3rate_i); 
	       */
	  }
	} else { // fallback solution
	  do {

	    if( (bytes = AVI_read_audio_chunk(avifile2, data)) < 0) {
	      AVI_print_error("AVI audio read frame");
	      return(-1);
	    }


	    if(AVI_write_audio(avifile, data, bytes)<0) {
	      AVI_print_error("AVI write audio frame");
	      return(-1);
	    } 
	  } while (AVI_can_read_audio(avifile1));

	} // else
      } // chan
      
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


// *************************************************
// Merge a raw audio file which is either MP3 or AC3
// *************************************************

merge_mp3:

  f = fopen(audfile,"rb");
  if (!f) { perror ("fopen"); exit(1); }

  len = fread(head, 1, 8, f);
  format_add  = tc_probe_audio_header(head, len);
  headlen = tc_get_audio_header(head, len, format_add, &chan_i, &rate_i, &mp3rate_i);
  fprintf(stderr, "... this looks like a %s track ...\n", (format_add==0x55)?"MP3":"AC3");

  fseek(f, 0L, SEEK_SET);

  //set next track
  AVI_set_audio_track(avifile, aud_tracks);
  AVI_set_audio(avifile, chan_i, rate_i, 16, format_add, mp3rate_i);
  AVI_set_audio_vbr(avifile, is_vbr);

  AVI_seek_start(avifile1);
  frames =  AVI_video_frames(avifile1);
  offset = 0;

  for (n=0; n<AVI_MAX_TRACKS; ++n)
      aud_ms_w[n] = 0.0;

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

    vid_chunks++;
    vid_ms = vid_chunks*1000.0/fps;

    for(j=0; j<aud_tracks; ++j) {

      AVI_set_audio_track(avifile1, j);
      AVI_set_audio_track(avifile, j);
      format = AVI_audio_format(avifile1);

      // audio
      chan = AVI_audio_channels(avifile1);

      if(chan) {
	if (format == 0x55 || format == 0x2000 || format == 0x2001) {
	  while (aud_ms_w[j] < vid_ms) {

	    if( (bytes = AVI_read_audio_chunk(avifile1, data)) < 0) {
		AVI_print_error("AVI audio read frame");
		aud_ms_w[j] = vid_ms;
		break;
	    }      

	    if(AVI_write_audio(avifile, data, bytes)<0) {
	      AVI_print_error("AVI write audio frame");
	      return(-1);
	    }

	    if (bytes == 0) {
	      aud_ms_w[j] = vid_ms;
	      break;
	    }
	    if ( tc_get_audio_header(data, bytes, format, NULL, NULL, &mp3rate_i)<0) { 
	      // if this is the last frame of the file, slurp in audio chunks
	      if (n == frames-1) continue;
	      aud_ms_w[j] = vid_ms;
	    } else {
	      aud_ms_w[j] += (bytes*8.0)/(mp3rate_i);
	    }

	    /*
	       fprintf(stderr, "%s track (%d) %8.0lf->%8.0lf len (%ld) rate (%d)\n", 
	       (format==0x55)?"MP3":"AC3", 
	       j, vid_ms, aud_ms_w[j], bytes, mp3rate_i); 
	    */
	  }
	} else { // fallback solution
	  do {

	    if( (bytes = AVI_read_audio_chunk(avifile1, data)) < 0) {
	      AVI_print_error("AVI audio read frame");
	      return(-1);
	    }


	    if(AVI_write_audio(avifile, data, bytes)<0) {
	      AVI_print_error("AVI write audio frame");
	      return(-1);
	    } 
	  } while (AVI_can_read_audio(avifile1));

	} // else
      } // chan
    } // aud_tracks


    // merge additional track

    if(headlen>4) {
      while (aud_ms < vid_ms) {
	//printf("reading Audio Chunk ch(%ld) vms(%lf) ams(%lf)\n", vid_chunks, vid_ms, aud_ms);
	pos = ftell(f);

	len = fread (head, 1, 8, f);
	if (len<=0) { //eof
	  goto finish2;
	}

	if ( (headlen = tc_get_audio_header(head, len, format_add, NULL, NULL, &mp3rate_i))<0) {
	  fprintf(stderr, "Broken %s track #(%d)?\n", (format_add==0x55?"MP3":"AC3"), aud_tracks); 
	  aud_ms = vid_ms;
	  goto finish2;
	} else { // look in import/tcscan.c for explanation
	  aud_ms += (headlen*8.0)/(mp3rate_i);
	}

	fseek (f, pos, SEEK_SET);

	len = fread (data, headlen, 1, f);
	if (len<=0) { //eof
	  goto finish2;
	}

	AVI_set_audio_track(avifile, aud_tracks);

	if(AVI_write_audio(avifile, data, headlen)<0) {
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
 
  // more files?
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

      vid_chunks++;
      vid_ms = vid_chunks*1000.0/fps;


      for(j=0; j<aud_tracks; ++j) {

	AVI_set_audio_track(avifile1, j);
	AVI_set_audio_track(avifile, j);
	format = AVI_audio_format(avifile1);

	// audio
	chan = AVI_audio_channels(avifile1);

	if(chan) {
	  if (format == 0x55 || format == 0x2000 || format == 0x2001) {
	    while (aud_ms_w[j] < vid_ms) {

	      if( (bytes = AVI_read_audio_chunk(avifile1, data)) < 0) {
		AVI_print_error("AVI 1 audio read frame");
		aud_ms_w[j] = vid_ms;
		break;
	      }      

	      if(AVI_write_audio(avifile, data, bytes)<0) {
		AVI_print_error("AVI write audio frame");
		return(-1);
	      }

	      if (bytes == 0) {
		aud_ms_w[j] = vid_ms;
		break;
	      }
	      if ( tc_get_audio_header(data, bytes, format, NULL, NULL, &mp3rate_i)<0) { 
		// if this is the last frame of the file, slurp in audio chunks
		if (n == frames-1) continue;
		aud_ms_w[j] = vid_ms;
	      } else {
		aud_ms_w[j] += (bytes*8.0)/(mp3rate_i);
	      }

	      /*
		 fprintf(stderr, " XX %s track (%d) %8.0lf->%8.0lf len (%ld) rate (%d)\n", 
		 (format==0x55)?"MP3":"AC3", 
		 j, vid_ms, aud_ms_w[j], bytes, mp3rate_i); 
		 */
	    }
	  } else { // fallback solution
	    do {

	      if( (bytes = AVI_read_audio_chunk(avifile1, data)) < 0) {
		AVI_print_error("AVI audio read frame");
		return(-1);
	      }

	      AVI_set_audio_track(avifile, j);

	      if(AVI_write_audio(avifile, data, bytes)<0) {
		AVI_print_error("AVI write audio frame");
		return(-1);
	      }
	    } while (AVI_can_read_audio(avifile1));

	  } //else format
	} // chan
      } // aud_tracks

      // merge additional track
      // audio

      if(headlen>4) {
	while (aud_ms < vid_ms) {
	  //printf("reading Audio Chunk ch(%ld) vms(%lf) ams(%lf)\n", vid_chunks, vid_ms, aud_ms);
	  pos = ftell(f);

	  len = fread (head, 8, 1, f);
	  if (len<=0) { //eof
	    goto finish2;
	  }

	  if ( (headlen = tc_get_audio_header(head, len, format_add, NULL, NULL, &mp3rate_i))<0) {
	    fprintf(stderr, "Broken %s track #(%d)?\n", (format_add==0x55?"MP3":"AC3"), aud_tracks); 
	    aud_ms = vid_ms;
	    goto finish2;
	  } else { // look in import/tcscan.c for explanation
	    aud_ms += (headlen*8.0)/(mp3rate_i);
	  }

	  fseek (f, pos, SEEK_SET);

	  len = fread (data, headlen, 1, f);
	  if (len<=0) { //eof
	    goto finish2;
	  }

	  AVI_set_audio_track(avifile, aud_tracks);

	  if(AVI_write_audio(avifile, data, headlen)<0) {
	    AVI_print_error("AVI write audio frame");
	    return(-1);
	  }

	}
      }

      // progress
      fprintf(stderr, "[%s] (%06ld-%06ld)\r", outfile, offset, offset + n);
    }

    fprintf(stderr, "\n");

    offset += frames;
    AVI_close(avifile1);
  }


finish2:

  if (f) fclose(f);

  printf("... done multiplexing in %s\n", outfile);

  AVI_close(avifile);

  return(0);
}
