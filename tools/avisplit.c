/*
 *  avisplit.c
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
#include "limits.h"
#include <string.h>
#include "buffer.h"
#include "avilib.h"
#include "config.h"
#include "transcode.h"
#include "libioaux/framecode.h"
#include "aud_scan.h"

#define EXE "avisplit"
#define MBYTE (1<<20)

void version()
{
  printf("%s (%s v%s) (C) 2001-2002 Thomas Östreich\n", EXE, PACKAGE, VERSION);
}

void usage(int status)
{
    version();
    printf("\nUsage: %s [options]\n", EXE);
    printf("\t -i name             file name\n");
    printf("\t -s size             de-chunk based on size in MB (0=dechunk)\n");
    printf("\t -H n                split only first n chunks [all]\n");
    printf("\t -t s1-s2[,s3-s4,..] de-chunk based on time/framecode (n:m:l.k) [off]\n");
    printf("\t -c                  merge chunks on-the-fly for option -t [off]\n");
    printf("\t -m                  force split at upper limit for option -t [off]\n");
    printf("\t -o base             split to base-%%04d.avi [name-%%04d]\n");
    printf("\t -b n                handle vbr audio [1]\n");
    printf("\t -f FILE             read AVI comments from FILE [off]\n");
    printf("\t -v                  print version\n");
    exit(status);
}

// buffer
static char data[SIZE_RGB_FRAME];
static char out_file[1024];
static char *comfile = NULL;
int is_vbr = 1;

enum split_type
{
  SPLIT_BY_SIZE,
  SPLIT_BY_TIME
};

int main(int argc, char *argv[])
{

  avi_t *avifile1=NULL;
  avi_t *avifile2=NULL;

  char *in_file=NULL;

  long i, frames, bytes;

  uint64_t size=0;

  double fsize=0.0, fps;

  char *codec;

  int j, n, key, k;
  
  int key_boundary=1;

  int chunk=0, is_open, ch, split_next=INT_MAX;

  long rate, mp3rate=0L;

  int width, height, format=0, chan, bits;

  char *base=NULL;
  char argcopy[1024];

  /* added variables */
  long start_audio_keyframe[ AVI_MAX_TRACKS ];
  long byte_count_audio[ AVI_MAX_TRACKS ];
  long byte_count_at_start[ AVI_MAX_TRACKS ];
  static char audio_data[SIZE_RGB_FRAME];
  static char *single_output_file=NULL;
  struct fc_time * ttime = NULL;
  struct fc_time * tstart = NULL;
  int start_keyframe=0;
  int split_option=0;
  long audio_bytes=0;
  int first_frame=1;
  int num_frames;
  int didread = 0;

  int aud_bitrate=0;
  double aud_ms[ AVI_MAX_TRACKS ];
  double aud_ms_w[ AVI_MAX_TRACKS ];

  int vid_chunks=0;
  //int aud_chunks=0;
  double vid_ms_w = 0.0;
  double vid_ms = 0.0;
  int framesize = 0;

  char separator[] = ",";

  if(argc==1) usage(EXIT_FAILURE);
  memset(byte_count_at_start, 0 , sizeof(long)*AVI_MAX_TRACKS);
  
  while ((ch = getopt(argc, argv, "b:mco:vs:i:f:t:H:?h")) != -1) {

    switch (ch) {

    case 'b':
      
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      is_vbr = atoi(optarg);
      
      if(is_vbr<0) usage(EXIT_FAILURE);
      
      break;
      
    case 'c':  // cat
      single_output_file = out_file;
      break;

    case 'm': 
	key_boundary = 0;
      break;

    case 'H':

      if(optarg[0]=='-') usage(EXIT_FAILURE);
  	  split_next = atoi(optarg);

  	  if(split_next <= 0) {
	      fprintf(stderr, "(%s) invalid parameter for option -H\n", __FILE__);
	      exit(0);
  	  }
  	  break;

    case 'i':
  
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      in_file=optarg;
  
      break;
  
    case 's':
  
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      chunk = atoi(optarg);
      split_option=SPLIT_BY_SIZE;

      break;

    case 't':
      split_option=SPLIT_BY_TIME;
      strncpy (argcopy, optarg, 1024);

      break;

    case 'o':
  
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      base = optarg;
  
      break;

    case 'f':
      
      if(optarg[0]=='-') usage(EXIT_FAILURE);
      comfile = optarg;
      
      break;

    case 'v':
      version();
      exit(0);
      break;

    case 'h':
      usage(EXIT_SUCCESS);

    default:
      usage(EXIT_FAILURE);
    }
  }
  /*
   * check
   */
  switch (split_option) {
  case SPLIT_BY_SIZE:

    if(in_file==NULL || chunk < 0) usage(EXIT_FAILURE);

    break;

  case SPLIT_BY_TIME:

    if(in_file==NULL) usage(EXIT_FAILURE);

    break;
  }

  
  // open file
  if(NULL == (avifile1 = AVI_open_input_file(in_file,1))) {
    AVI_print_error("AVI open");
    exit(1);
  }

  // read video info;

  AVI_info(avifile1);
 
  // read video info;

  frames =  AVI_video_frames(avifile1);
  width  =  AVI_video_width(avifile1);
  height =  AVI_video_height(avifile1);

  fps    =  AVI_frame_rate(avifile1);
  codec  =  AVI_video_compressor(avifile1);
  rate   =  AVI_audio_rate(avifile1);
  chan   =  AVI_audio_channels(avifile1);
  bits   =  AVI_audio_bits(avifile1);

  for (k = 0; k<AVI_MAX_TRACKS; k++) 
      aud_ms[k] = 0.0;

  for (k = 0; k<AVI_MAX_TRACKS; k++) 
      aud_ms_w[k] = 0.0;

  switch (split_option) {

  case SPLIT_BY_SIZE:
    // no file open yet
    is_open=0;
    // index of split files
    j=0;
    // start frame
    i=0;

    //some header may be broken
    if(frames<=0) frames=INT_MAX;

    for (n=0; n<frames; ++n) {
      
      // read video frame
      bytes = AVI_read_frame(avifile1, data, &key);
      
      if(bytes < 0) {
        fprintf(stderr, "%d (%ld)\n", n, bytes);
        AVI_print_error("AVI read video frame");
        break;
      }
      
      //check for closing outputfile
	
      if(key && is_open && n && split_next) {
	    
        size = AVI_bytes_written(avifile2);
        fsize = ((double) size)/MBYTE;
	    
        if((size + MBYTE) > (uint64_t)(chunk*MBYTE)) {      
		
          // limit exceeded, close file

          fprintf(stderr, "\n");
          AVI_close(avifile2);
          avifile2=NULL;
          --split_next; //0 for trailer split mode after first chunk.
          is_open=0;
          ++j;
          i=n;
        }
      }


      // progress
      if(avifile2) {
	vid_ms = vid_chunks*1000.0/fps;

	fprintf(stderr, "[%s] (%06ld-%06d), size %4.1f MB. (V/A) (%.0f/%.0f)ms\r", out_file, i, n-1, ((double) AVI_bytes_written(avifile2))/MBYTE, vid_ms, aud_ms [0]);
      }

      if (split_next == 0) {
	  if(avifile1 != NULL)
	      AVI_close(avifile1);
	  avifile1=NULL;
	  if(avifile2 != NULL)
	      AVI_close(avifile2);

	  return (0);
      }
      
      // need new output file
      if(!is_open) {

	  if(base == NULL || strlen(base)==0) {
          sprintf(out_file, "%s-%04d", in_file, j);
        } else {
          sprintf(out_file, "%s-%04d.avi", base, j);
        }

        // prepare output file
    
        if(NULL == (avifile2 = AVI_open_output_file(out_file))) {
          AVI_print_error("AVI open");
          exit(1);
        }
    
        AVI_set_video(avifile2, width, height, fps, codec);
	if (comfile!=NULL)
	    AVI_set_comment_fd(avifile2, open(comfile, O_RDONLY));
    
        for(k=0; k< AVI_audio_tracks(avifile1); ++k) {

          AVI_set_audio_track(avifile1, k);

          rate   =  AVI_audio_rate(avifile1);
          chan   =  AVI_audio_channels(avifile1);
          bits   =  AVI_audio_bits(avifile1);

          format =  AVI_audio_format(avifile1);
          mp3rate=  AVI_audio_mp3rate(avifile1);

    
          //set next track of output file
          AVI_set_audio_track(avifile2, j);
          AVI_set_audio(avifile2, chan, rate, bits, format, mp3rate);
          AVI_set_audio_vbr(avifile2, is_vbr);
        }

        is_open=1;
      }
    
      //write frame
    
      if(AVI_write_frame(avifile2, data, bytes, key)<0) {
        AVI_print_error("AVI write video frame");
        return(-1);
      }
    
      vid_chunks++;
      vid_ms = vid_chunks*1000.0/fps;

      //audio
      for(k=0; k< AVI_audio_tracks(avifile1); ++k) {

        AVI_set_audio_track(avifile1, k);
        AVI_set_audio_track(avifile2, k);
	format = AVI_audio_format(avifile1);
	rate   = AVI_audio_rate(avifile1);
	chan   = AVI_audio_channels(avifile1);
	mp3rate= AVI_audio_mp3rate(avifile1);
	bits   = AVI_audio_bits(avifile1);
	bits   = bits==0?16:bits;

	if (tc_format_ms_supported(format)) {

	  while (aud_ms[k] < vid_ms) {

	    if( (bytes = AVI_read_audio_chunk(avifile1, data)) < 0) {
		AVI_print_error("AVI audio read frame");
		aud_ms[k] = vid_ms;
		break;
	    }

	    aud_bitrate = format==0x1?1:0;
	    aud_bitrate = format==0x2000?1:0;

	    if(AVI_write_audio(avifile2, data, bytes)<0) {
	      AVI_print_error("AVI write audio frame");
	      return(-1);
	    }

	    if (bytes == 0) {
	      aud_ms[k] = vid_ms;
	      break;
	    }

	    if ( !aud_bitrate && (framesize = tc_get_audio_header(data, bytes, format, NULL, NULL, &aud_bitrate))<0) { 
	      // if this is the last frame of the file, slurp in audio chunks
	      if (n == frames-1) continue;
	      aud_ms[k] = vid_ms;
	    } else {
	      aud_ms[k] += (bytes*8.0)/(format==0x1?((double)(rate*chan*bits)/1000.0):
				       (format==0x2000?(double)(mp3rate):aud_bitrate));
	    }
	    /*
	    printf(" 0 frame_read len=%ld (A/V) (%8.2f/%8.2f)\n", bytes, aud_ms[k], vid_ms);
	    fprintf(stderr, "%s track (%d) %8.0f->%8.0f len (%ld) frsize (%d) rate (%d)\n", 
		    format==0x55?"MP3":format==0x1?"PCM":"AC3", 
		  k, vid_ms, aud_ms[k], bytes, framesize, aud_bitrate); 
	    */
	  }
	} else {

	 do {
	    if( (bytes = AVI_read_audio_chunk(avifile1, data)) < 0) {
		AVI_print_error("AVI audio read frame");
		break;
	    }      
  
	    if(AVI_write_audio(avifile2, data, bytes)<0) {
		AVI_print_error("AVI write audio frame");
		return(-1);
	    }
	 } while (AVI_can_read_audio(avifile1));
	}
      } 

    }//process all frames

    if(avifile1 != NULL)
      AVI_close(avifile1);

    size = AVI_bytes_written(avifile2);
    vid_ms = vid_chunks*1000.0/fps;

    fprintf(stderr, "[%s] (%06ld-%06d), size %4.1f MB. vid=%8.2f ms aud=%8.2f ms\n", out_file, i, n-1, ((double) AVI_bytes_written(avifile2))/MBYTE, vid_ms, aud_ms[0]);

    if(avifile2 != NULL)
      AVI_close(avifile2);

    break;

    // XXX: use aud_ms like above
  case SPLIT_BY_TIME:

    if( parse_fc_time_string( argcopy, fps, separator, 1, &ttime ) == -1 )
      usage(EXIT_FAILURE);
    /*
     * pointer into the fc_list
     */
     tstart = ttime;
    /*
     * index of split files
     */
    j = 0;
    /*
     * no single output file
     */
    if( single_output_file != NULL ) {

      if(base == NULL || strlen(base)==0) {
        sprintf(out_file, "%s-%04d", in_file, j++ );
      } else {
        sprintf( out_file, "%s", base );
      }
      if( ( avifile2 = AVI_open_output_file( out_file ) ) == NULL ) {
        AVI_print_error( "AVI open" );
        exit( 1 );
      }
      /*
       * set video params in the output file
       */
      AVI_set_video( avifile2, width, height, fps, codec );
      if (comfile!=NULL)
	  AVI_set_comment_fd(avifile2, open(comfile, O_RDONLY));
      /*
       * set audio params in the output file
       */
      for( k = 0; k < AVI_audio_tracks( avifile1 ); k++ ) {

        AVI_set_audio_track( avifile1, k );
        AVI_set_audio_track( avifile2, k );

        rate   =  AVI_audio_rate    ( avifile1 );
        chan   =  AVI_audio_channels( avifile1 );
        bits   =  AVI_audio_bits    ( avifile1 );
        format =  AVI_audio_format  ( avifile1 );
        mp3rate=  AVI_audio_mp3rate ( avifile1 );

        AVI_set_audio( avifile2, chan, rate, bits, format, mp3rate );
        AVI_set_audio_vbr( avifile2, is_vbr );
      }
    }
    /*
     * process next fc_time_string
     */
    while( ttime != NULL ) {
      first_frame = 1;
      start_keyframe = 0;
      num_frames = ttime->etf - ttime->stf;

      /*
       * reset input file
       */
      AVI_seek_start( avifile1 );
      for( k = 0; k < AVI_audio_tracks( avifile1 ); k++ ) {
        byte_count_audio[ k ] = 0;
        start_audio_keyframe[ k ] = 0;
	AVI_set_audio_track (avifile1, k);
	AVI_set_audio_position_index (avifile1, 0);
      }
      AVI_set_audio_track (avifile1, 0);
      // reset counters
      vid_chunks = 0;
      vid_ms_w = 0.0;
      vid_ms = 0.0;

      for (k = 0; k<AVI_MAX_TRACKS; k++)  {
	aud_ms_w[k] = 0.0;
	aud_ms[k] = 0.0;
      }


      printf("\nProcessing %d frames %4d to %4d.", num_frames, ttime->stf, ttime->etf);
      /*
       * some header may be broken
       */
      if( frames <= 0 )
        frames=INT_MAX;
      /*
       * not a single output file
       */
      if( single_output_file == NULL ) {
        /*
         * prepare output file
         */
        if( base == NULL || strlen( base ) == 0 ) {
          sprintf( out_file, "%s-%04d", in_file, j++ );
        }
        else {
          sprintf( out_file, "%s-%04d", base, j++ );
        }

        if( ( avifile2 = AVI_open_output_file( out_file ) ) == NULL ) {
          AVI_print_error( "AVI open" );
          exit( 1 );
        }
        /*
         * set video params in the output file
         */
        AVI_set_video( avifile2, width, height, fps, codec );
	if (comfile!=NULL)
	    AVI_set_comment_fd(avifile2, open(comfile, O_RDONLY));
        /*
         * set audio params in the output file
         */
        for( k = 0; k < AVI_audio_tracks( avifile1 ); k++ ) {
          AVI_set_audio_track( avifile1, k );

          rate    =  AVI_audio_rate    ( avifile1 );
          chan    =  AVI_audio_channels( avifile1 );
          bits    =  AVI_audio_bits    ( avifile1 );
          format  =  AVI_audio_format  ( avifile1 );
          mp3rate =  AVI_audio_mp3rate ( avifile1 );

          AVI_set_audio_track( avifile2, k );
          AVI_set_audio( avifile2, chan, rate, bits, format, mp3rate );
	  AVI_set_audio_vbr( avifile2, is_vbr );
        }
      }
      /*
       * process all frames
       */
      for( n = 0; n < frames; n++) {
        /*
         * read video frame
         */
        bytes = AVI_read_frame( avifile1, data, &key );
        if( bytes < 0 ) {
          fprintf( stderr, "%d (%ld)\n", n, bytes );
          AVI_print_error( "AVI read video frame" );
          break;
        }

	vid_ms = (n+1)*1000.0/fps;

        /*
         * store the key frame
         */
        if( n <= ttime->stf && key ) {
          start_keyframe = n;
	  vid_ms_w = n*1000.0/fps;
	}
        /*
         * read audio frame
         */
	for( k = 0; k < AVI_audio_tracks( avifile1 ); k++ ) {

	  double tms = aud_ms[k];
	  AVI_set_audio_track( avifile1, k );
	  //audio_bytes = AVI_audio_size( avifile1, n );
	  //byte_count_audio[ k ] += audio_bytes;
	  format = AVI_audio_format (avifile1);
	  rate   = AVI_audio_rate(avifile1);
	  chan   = AVI_audio_channels(avifile1);
	  bits   = AVI_audio_bits(avifile1);
	  bits   = bits==0?16:bits;
	  mp3rate= AVI_audio_mp3rate(avifile1);
	  
	  byte_count_audio[ k ] = AVI_get_audio_position_index(avifile1);
	 
	  if (tc_format_ms_supported(format)) {

	    if (!didread) while (aud_ms[k] < vid_ms) {

	      aud_bitrate = format==0x1?1:0;
	      aud_bitrate = format==0x2000?1:0;
	      audio_bytes = AVI_read_audio_chunk( avifile1, audio_data );

	      if (audio_bytes<=0) { 
		  if (audio_bytes==0) continue;
		  aud_ms[k] = vid_ms;
		  break; 
	      }

	      if (!aud_bitrate && tc_get_audio_header(audio_data, audio_bytes, format, NULL, NULL, &aud_bitrate)<0) {
		//fprintf(stderr, "Corrupt Audio track (%d)?\n", k); 
		aud_ms[k] = vid_ms;
		break;
	      } else {
		aud_ms[k] += (audio_bytes*8.0)/(format==0x1?((double)(rate*chan*bits)/1000.0):
				       (format==0x2000?(double)(mp3rate):aud_bitrate));
	      }

	    }
	  } else {
	    if (!didread) do {
	      // this is not very optimal -- it sucks. But I am happy because it works. --tibit
	      audio_bytes = AVI_read_audio_chunk( avifile1, audio_data );
	    } while (AVI_can_read_audio(avifile1));
	  }

	  /*
	   * store the key frame
	   */
	  if( n <= ttime->stf && key ) {
	    start_audio_keyframe[ k ] = byte_count_audio[ k ];
	    aud_ms_w[k] = tms;
	  }

	}
        /*
         * if one of the preferred frames write frame (video+audio) 
         * but don't stop until the next keyframe
         */
        if( n >= ttime->stf && ( n <= ttime->etf || ( n >= ttime->stf && ! key ) ) ) {
          /*
           * do the following ONLY for the first frame
           */
          if( first_frame ) {
            /*
             * rewind n to point to the last keyframe
             */
            printf( "\nFirst Setting start frame to: %d\n", start_keyframe );
            n = start_keyframe;
            fc_set_start_time( ttime, n );
            /*
             * first the video
             */
            AVI_set_video_position( avifile1, start_keyframe );
            /*
             * then the audio
             */
	    //printf("Start Audio (%ld)\n", start_audio_keyframe[ 0 ]);
            for( k = 0; k < AVI_audio_tracks( avifile1 ); k++ ) {
              AVI_set_audio_track( avifile1, k );
              //AVI_set_audio_position( avifile1, start_audio_keyframe[ k ] );
              AVI_set_audio_position_index( avifile1, start_audio_keyframe[ k ]);
	      aud_ms[k] = aud_ms_w[k];
            }
            /*
             * re-read video and audio from rewound position
             */
            bytes = AVI_read_frame( avifile1, data, &key );
	    
	    // count the frame which will be written also this, too
	    vid_ms = vid_ms_w+1000.0/fps;

	    //printf("start_frame (%d) (%f) (%f)\n", n, vid_ms, aud_ms[0]);

            first_frame = 0;
          }
          /*
           * do the write
           */
          if( AVI_write_frame( avifile2, data, bytes, key ) < 0 ) {
            AVI_print_error( "AVI write video frame" );
            return( -1 );
          }
	  /*
	  vid_chunks++;
	  vid_ms_w = vid_chunks*1000.0/fps;
	  */

	  //printf("Before Enter (%d) (%f) (%f)\n", n, vid_ms, aud_ms[0]);
          for( k = 0; k < AVI_audio_tracks( avifile1 ); k++ ) {
	    int frsize=0;

            AVI_set_audio_track( avifile1, k );
            AVI_set_audio_track( avifile2, k );
	    format = AVI_audio_format(avifile1);
	    rate   = AVI_audio_rate(avifile1);
	    chan   = AVI_audio_channels(avifile1);
	    bits   = AVI_audio_bits(avifile1);
	    bits   = bits==0?16:bits;
	    mp3rate= AVI_audio_mp3rate(avifile1);

	    if (tc_format_ms_supported(format)) {
	      while (aud_ms[k] < vid_ms) {

		aud_bitrate = format==0x1?1:0;
		aud_bitrate = format==0x2000?1:0;

		if(  (audio_bytes = AVI_read_audio_chunk( avifile1, audio_data)) < 0 ) {
		  AVI_print_error( "AVI audio read frame" );
		  aud_ms[k] = vid_ms;
		  break;
		}      

		if( AVI_write_audio( avifile2, audio_data, audio_bytes ) < 0 ) {
		  AVI_print_error( "AVI write audio frame" );
		  return( -1 );
		}
		if (audio_bytes==0) break;

	
		if ( !aud_bitrate && (frsize = tc_get_audio_header(audio_data, audio_bytes, format, NULL, NULL, &aud_bitrate))<0) {
		  fprintf(stderr, "Corrupt %s track (%d)?\n", format==0x55?"MP3":"AC3", k); 
		  if (n == frames-1) continue;
		  aud_ms[k] = vid_ms;
		} else {
		  aud_ms[k] += (audio_bytes*8.0)/(format==0x1?((double)(rate*chan*bits)/1000.0):
				       (format==0x2000?(double)(mp3rate):aud_bitrate));
		}
		//fprintf(stderr, " 1 (%02d) %s frame_read len=%4ld fsize (%4d) (A/V) (%8.2f/%8.2f)\n", n, format==0x55?"MP3":"AC3", audio_bytes, frsize, aud_ms[k], vid_ms);
	      } // (aud_ms_w[k] < vid_ms_w)
	    } else {
	      do {
		if(  (audio_bytes = AVI_read_audio_chunk( avifile1, audio_data)) < 0 ) {
		  AVI_print_error( "AVI audio read frame" );
		  break;
		}      
		/*
		   printf("\nn (%d) bytes (%ld) posc (%ld) posb (%ld) pos (%ld)",n,audio_bytes,
		   avifile1->track[avifile1->aptr].audio_posc,
		   avifile1->track[avifile1->aptr].audio_posb,
		   avifile1->track[avifile1->aptr].audio_index[avifile1->track[avifile1->aptr].audio_posc].pos
		   ); fflush(stdout);
		   */

		if( AVI_write_audio( avifile2, audio_data, audio_bytes ) < 0 ) {
		  AVI_print_error( "AVI write audio frame" );
		  return( -1 );
		}
	      } while (AVI_can_read_audio(avifile1));
	    } // else
          } // foreach audio track

	  didread = 1;
          /*
           * print our progress
           */
          printf( "[%s] (%06d-%06d)\r", out_file, start_keyframe, n);
        } else {
	  didread = 0;
	}

	if( key_boundary ) {
	    if( n > ttime->etf && key ) {
		printf( "\n" );
		break;
	    }       
	} else {
	    if( n > ttime->etf) {
		printf( "\n" );
		break;
	    }        
	}
      }
      /*
       * if we're using split files
       * close output file
       */
      if( single_output_file == NULL ) {
        if( avifile2 != NULL )
          AVI_close( avifile2 );
      }

      ttime = ttime->next;

      printf( "\nSetting end frame to: %d | cnt(%ld)\n", n - 1, byte_count_audio[0] );
    }

    if( avifile1 != NULL ) AVI_close( avifile1 );
    /*
     * close up single output file
     */
    if( single_output_file != NULL ) {
      if( avifile2 != NULL ) AVI_close( avifile2 );
    }

    if( tstart != NULL )
      free_fc_time( tstart );

    printf( "\n" );

    break;
  }

  return( 0 );
}


