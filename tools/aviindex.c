/*
 *  aviindex.c
 *
 *  extracts the index of an AVI file for easy seeking with --nav_seek
 *
 *  Copyright (C) Tilmann Bitterberg - June 2003
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "avilib.h"
#include "../config.h"
#include "transcode.h"

#include "aud_scan.h"

#define EXE "aviindex"

void version()
{
  printf("%s (%s v%s) (C) 2001-2003 Thomas Östreich\n", EXE, PACKAGE, VERSION);
}


void usage(int status)
{
  version();
  printf("\nUsage: %s [options]\n", EXE);
  printf("\t -o file            output file\n");
  printf("\t -i file            input file\n");
  printf("\t -n                 read index in \"smart\" mode\n");
  exit(status);
}

#define PAD_EVEN(x) ( ((x)+1) & ~1 )
static unsigned long str2ulong(unsigned char *str)
{
   return ( str[0] | (str[1]<<8) | (str[2]<<16) | (str[3]<<24) );
}

#define AVI_MAX_LEN (UINT_MAX-(1<<20)*16-2048)

const int LEN=10;


int AVI_read_data_fast(avi_t *AVI, char *buf, off_t *pos, off_t *len, off_t *key)
{

/*
 * Return codes:
 *
 *    0 = reached EOF
 *    1 = video data read
 *    2 = audio data read from track 0
 *    3 = audio data read from track 1
 *    4 = audio data read from track 2
 *    ....
 */

   off_t n;
   char data[8];
   int rlen;
   *key=(off_t)0;

   if(AVI->mode==AVI_MODE_WRITE) return 0;

   while(1)
   {
      /* Read tag and length */

      if( read(AVI->fdes,data,8) != 8 ) return 0;

      n = PAD_EVEN(str2ulong(data+4));

      if(strncasecmp(data,"IDX1",4) == 0)
      {
	 // deal with it to extract keyframe info
         *len = str2ulong(data+4);
	 *pos = lseek(AVI->fdes, 0, SEEK_CUR)-(off_t)8;
	 //fprintf (stderr, "Found an index chunk at %lld len %lld\n", *pos, *len);
         if(lseek(AVI->fdes,n,SEEK_CUR)==(off_t)-1)  return 0;
	 return 10;
         continue;
      }

      /* if we got a list tag, ignore it */

      if(strncasecmp(data,"LIST",4) == 0)
      {
         lseek(AVI->fdes,4,SEEK_CUR);
         continue;
      }


      if(strncasecmp(data,AVI->video_tag,3) == 0)
      {
         *len = str2ulong(data+4);
	 *pos = lseek(AVI->fdes, 0, SEEK_CUR)-(off_t)8;
         AVI->video_pos++;
	 rlen = (n<LEN)?n:LEN;
	 read(AVI->fdes, buf, rlen);
         if(lseek(AVI->fdes,n-rlen,SEEK_CUR)==(off_t)-1)  return 0;
         return 1;
      }
      else if(AVI->anum>=1 && strncasecmp(data,AVI->track[0].audio_tag,4) == 0)
      {
         *len = str2ulong(data+4);
	 *pos = lseek(AVI->fdes, 0, SEEK_CUR)-(off_t)8;
	 AVI->track[0].audio_posc++;
	 rlen = (n<LEN)?n:LEN;
	 read(AVI->fdes, buf, rlen);
         if(lseek(AVI->fdes,n-rlen,SEEK_CUR)==(off_t)-1)  return 0;
         return 2;
         break;
      }
      else if(AVI->anum>=2 && strncasecmp(data,AVI->track[1].audio_tag,4) == 0)
      {
         *len = str2ulong(data+4);
	 *pos = lseek(AVI->fdes, 0, SEEK_CUR)-(off_t)8;
	 AVI->track[1].audio_posc++;
	 rlen = (n<LEN)?n:LEN;
	 read(AVI->fdes, buf, rlen);
         if(lseek(AVI->fdes,n-rlen,SEEK_CUR)==(off_t)-1)  return 0;
         return 3;
         break;
      }
      else if(AVI->anum>=3 && strncasecmp(data,AVI->track[2].audio_tag,4) == 0)
      {
         *len = str2ulong(data+4);
	 *pos = lseek(AVI->fdes, 0, SEEK_CUR)-(off_t)8;
	 AVI->track[2].audio_posc++;
	 rlen = (n<LEN)?n:LEN;
	 read(AVI->fdes, buf, rlen);
         if(lseek(AVI->fdes,n-rlen,SEEK_CUR)==(off_t)-1)  return 0;
         return 4;
         break;
      }
      else if(AVI->anum>=4 && strncasecmp(data,AVI->track[3].audio_tag,4) == 0)
      {
         *len = str2ulong(data+4);
	 *pos = lseek(AVI->fdes, 0, SEEK_CUR)-(off_t)8;
	 AVI->track[3].audio_posc++;
	 rlen = (n<LEN)?n:LEN;
	 read(AVI->fdes, buf, rlen);
         if(lseek(AVI->fdes,n-rlen,SEEK_CUR)==(off_t)-1)  return 0;
         return 5;
         break;
      }
      else if(AVI->anum>=5 && strncasecmp(data,AVI->track[4].audio_tag,4) == 0)
      {
         *len = str2ulong(data+4);
	 *pos = lseek(AVI->fdes, 0, SEEK_CUR)-(off_t)8;
	 AVI->track[4].audio_posc++;
	 rlen = (n<LEN)?n:LEN;
	 read(AVI->fdes, buf, rlen);
         if(lseek(AVI->fdes,n-rlen,SEEK_CUR)==(off_t)-1)  return 0;
         return 6;
         break;
      }
      else if(AVI->anum>=6 && strncasecmp(data,AVI->track[5].audio_tag,4) == 0)
      {
         *len = str2ulong(data+4);
	 *pos = lseek(AVI->fdes, 0, SEEK_CUR)-(off_t)8;
	 AVI->track[5].audio_posc++;
	 rlen = (n<LEN)?n:LEN;
	 read(AVI->fdes, buf, rlen);
         if(lseek(AVI->fdes,n-rlen,SEEK_CUR)==(off_t)-1)  return 0;
         return 7;
         break;
      }
      else if(AVI->anum>=7 && strncasecmp(data,AVI->track[6].audio_tag,4) == 0)
      {
         *len = str2ulong(data+4);
	 *pos = lseek(AVI->fdes, 0, SEEK_CUR)-(off_t)8;
	 AVI->track[6].audio_posc++;
	 rlen = (n<LEN)?n:LEN;
	 read(AVI->fdes, buf, rlen);
         if(lseek(AVI->fdes,n-rlen,SEEK_CUR)==(off_t)-1)  return 0;
         return 8;
         break;
      }
      else if(AVI->anum>=8 && strncasecmp(data,AVI->track[7].audio_tag,4) == 0)
      {
         *len = str2ulong(data+4);
	 *pos = lseek(AVI->fdes, 0, SEEK_CUR)-(off_t)8;
	 AVI->track[7].audio_posc++;
	 rlen = (n<LEN)?n:LEN;
	 read(AVI->fdes, buf, rlen);
         if(lseek(AVI->fdes,n-rlen,SEEK_CUR)==(off_t)-1)  return 0;
         return 9;
         break;
      }
      else
         if(lseek(AVI->fdes,n,SEEK_CUR)==(off_t)-1)  return 0;
   }
}

int main(int argc, char *argv[])
{

  avi_t *avifile1=NULL;

  char *in_file=NULL, *out_file=NULL;

  long frames;

  double fps;

  int track_num=0, aud_tracks;

  int ret;
  long i=0, chunk=0;

  int ch;
  int progress=0, old_progress=0;

  long rate;
  int format, chan, bits;
  int aud_bitrate = 0;

  FILE *out_fd    = NULL;
  int open_without_index=0;

  double vid_ms = 0.0, print_ms = 0.0;
  double aud_ms [ AVI_MAX_TRACKS ];
  char tag[8];
  char data[100];
  int vid_chunks=0, aud_chunks[AVI_MAX_TRACKS];
  off_t pos, len, key, index_pos=0, index_len=0,size=0;
  struct stat st;
  int idx_type=0;
  off_t ioff;

  if(argc==1) usage(EXIT_FAILURE);

  for (i=0; i<AVI_MAX_TRACKS; i++) {
    aud_chunks[i] = 0;
    aud_ms[i] = 0;
  }

  while ((ch = getopt(argc, argv, "a:vi:o:n?h")) != -1)
    {

	switch (ch) {
	
	case 'i':

	     if(optarg[0]=='-') usage(EXIT_FAILURE);
	    in_file=optarg;
	
	    break;
	
	case 'a':
	
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  track_num = atoi(optarg);
	
	  if(track_num<0) usage(EXIT_FAILURE);
	
	  break;

	case 'o':

	    if(optarg[0]=='-') usage(EXIT_FAILURE);
	    out_file=optarg;
	
	    break;
	
	case 'n':

	    open_without_index=1;
	
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

  // check
  if(in_file==NULL) usage(EXIT_FAILURE);
  if (!out_file) out_fd = stdout;
  else out_fd = fopen(out_file, "w+r");

  if (!out_fd) {
      perror("ERROR cannot open outputfile");
      exit(1);
  }

  // if file is larger than 2GB, regen index

  if (stat(in_file, &st)<0) {
      perror("Stat input file");
      return 1;
  }

  size = st.st_size;
  if (size > (off_t)AVI_MAX_LEN/2)
      open_without_index = 1;

  if (open_without_index) 
      fprintf(stderr, "[%s] Open \"%s\" without index\n",EXE, in_file);
  else
      fprintf(stderr, "[%s] Open \"%s\" with index (fast)\n", EXE, in_file);

  // header. Magic tag is AVIIDX1
  fprintf(out_fd, "AVIIDX1 # Generated by %s (%s-%s)\n", EXE, PACKAGE, VERSION); // Magic
  fprintf(out_fd, "TAG TYPE CHUNK CHUNK/TYPE POS LEN KEY MS\n");

  if (open_without_index) {

    // open file with index.
    if(NULL == (avifile1 = AVI_open_input_file(in_file,0))) {
      AVI_print_error("AVI open input file");
      exit(1);
    }

    aud_tracks = frames = 0;
    frames = AVI_video_frames(avifile1);
    fps    = AVI_frame_rate  (avifile1);

    aud_tracks = AVI_audio_tracks(avifile1);
    //printf("frames (%ld), aud_tracks (%d)\n", frames, aud_tracks);

    pos = key = len = (off_t)0;
    i = 0;

    while ( (ret = AVI_read_data_fast (avifile1, data, &pos, &len, &key)) != 0) {
      int audtr = ret-2;

      /* don't need this and it saves time
       * */
      if (audtr>=0 && audtr<=7) {
	AVI_set_audio_track(avifile1, audtr);
	format = AVI_audio_format (avifile1);
	chan   = AVI_audio_channels(avifile1);
	rate   = AVI_audio_rate   (avifile1);
	bits   = AVI_audio_bits   (avifile1);
	bits = bits==0?16:bits;
	if (tc_format_ms_supported(format)) {

	  aud_bitrate = format==0x1?1:0;

	  if (!aud_bitrate && tc_get_audio_header(data, LEN, format, NULL, NULL, &aud_bitrate)<0) {
	    aud_ms[audtr] = vid_ms;
	  } else
	    aud_ms[audtr] += (len*8.0)/(format==0x1?((double)(rate*chan*bits)/1000.0):aud_bitrate);
	}
      }

      switch (ret) {
	case 1: sprintf(tag, "00db");
		print_ms = vid_ms = (avifile1->video_pos)*1000.0/fps;
		chunk = avifile1->video_pos;
		if (avifile1->video_index) 
		  key = (avifile1->video_index[chunk-1].key)?1:0;
		break;
	case 2: case 3:
	case 4: case 5:
	case 6: case 7:
	case 8:
	case 9: sprintf(tag, "0%dwb", audtr+1);
		print_ms = aud_ms[audtr];
		chunk = avifile1->track[audtr].audio_posc;
		break;
	case 10: sprintf(tag, "idx1");
		 index_pos = pos;
		 index_len = len;
		 print_ms = 0.0;
		 chunk = 0;
		 break;

	case 0:
	default:
		 // never get here
		 break;
      }


      //if (index_pos != pos)
      // tag, chunk_nr
      fprintf(out_fd, "%s %d %ld %ld %lld %lld %lld %.2f\n", tag, ret, i, chunk-1, pos, len, key, print_ms);
      i++;

      // don't update the counter every chunk
      progress = (int)(pos*100/size)+1;
      if (old_progress != progress) {
	  fprintf(stderr, "[%s] Scanning ... %d%%\r", EXE, progress);
	  old_progress = progress;
      }

    }
    fprintf(stderr, "\n");

    // check if we have found an index chunk to restore keyframe info
    if (!index_pos || !index_len)
	goto out;

    fprintf(stderr, "[%s] Found an index chunk. Using it to regenerate keyframe info.\n", EXE);
    fseek (out_fd, 0, SEEK_SET);

    fgets(data, 100, out_fd); // magic
    fgets(data, 100, out_fd); // comment

    len = (off_t)0;
    vid_chunks = 0;

    lseek(avifile1->fdes, index_pos+8, SEEK_SET);
    while (len<index_len) {
	read(avifile1->fdes, tag, 8);
	
	// if its a keyframe and is a video chunk
	if (str2ulong(tag+4) && tag[1] == '0') {
	    int typen, keyn;
	    long chunkn, chunkptypen;
	    off_t posn, lenn;
	    char tagn[5];
	    double msn=0.0;

	    chunk = (long)(len/16);
	    i = 0;
	    //fprintf(stderr, "keyframe in chunk %ld\n", chunk);

	    // find line "chunk" in the logfile

	    while (i<chunk-vid_chunks) {
		fgets(data, 100, out_fd);
		i++;
	    }

	    vid_chunks += (chunk-vid_chunks);
	    posn = ftell(out_fd);
	    fgets(data, 100, out_fd);
	    fseek(out_fd, posn, SEEK_SET);
	    sscanf(data, "%s %d %ld %ld %lld %lld %d %lf", 
		      tagn, &typen, &chunkn, &chunkptypen, &posn, &lenn, &keyn, &msn);
	    fprintf(out_fd, "%s %d %ld %ld %lld %lld %d %.2f", 
		      tagn, typen, chunkn, chunkptypen, posn, lenn, 1, msn);
	}

	lseek(avifile1->fdes, 8, SEEK_CUR);
	len += 16;
    }
    


  } else { // with index

    // open file with index.
    if(NULL == (avifile1 = AVI_open_input_file(in_file,1))) {
      AVI_print_error("AVI open input file");
      exit(1);
    }
    i=0;

    if(avifile1->idx)
    {
      off_t pos, len;

      /* Search the first videoframe in the idx1 and look where
         it is in the file */

      for(i=0;i<avifile1->n_idx;i++)
         if( strncasecmp(avifile1->idx[i],avifile1->video_tag,3)==0 ) break;

      pos = str2ulong(avifile1->idx[i]+ 8);
      len = str2ulong(avifile1->idx[i]+12);

      lseek(avifile1->fdes,pos,SEEK_SET);
      if(read(avifile1->fdes,data,8)!=8) return 1;
      if( strncasecmp(data,avifile1->idx[i],4)==0 && str2ulong(data+4)==len )
      {
         idx_type = 1; /* Index from start of file */
      }
      else
      {
         lseek(avifile1->fdes,pos+avifile1->movi_start-4,SEEK_SET);
         if(read(avifile1->fdes,data,8)!=8) return 1;
         if( strncasecmp(data,avifile1->idx[i],4)==0 && str2ulong(data+4)==len )
         {
            idx_type = 2; /* Index from start of movi list */
         }
      }
      /* idx_type remains 0 if neither of the two tests above succeeds */
   }

   ioff = idx_type == 1 ? 0 : avifile1->movi_start-4;
   //fprintf(stderr, "index type (%d), ioff (%ld)\n", idx_type, (long)ioff);
    i=0;

    while (i<avifile1->n_idx) {
      memcpy(tag, avifile1->idx[i], 4);
      // tag
      fprintf(out_fd, "%c%c%c%c", 
	  avifile1->idx[i][0], avifile1->idx[i][1],
	  avifile1->idx[i][2], avifile1->idx[i][3]);

      // type, absolute chunk number
      fprintf(out_fd, " %c %ld", avifile1->idx[i][1]+1, i);
      

      switch (avifile1->idx[i][1]) {
	case '0':
	  fprintf(out_fd, " %d", vid_chunks);
	  vid_chunks++;
	  break;
	case '1': case '2':
	case '3': case '4':
	case '5': case '6':
	case '7': case '8':
	  // uhoh
	  ret = avifile1->idx[i][1]-'0';
	  fprintf(out_fd, " %d", aud_chunks[ret]);
	  aud_chunks[ret]++;
	  break;
	default:
	  fprintf(out_fd, " %d", -1);
	  break;
      }

      pos = str2ulong(avifile1->idx[i]+ 8);
      pos += ioff;
      // pos
      fprintf(out_fd, " %llu", pos);
      // len
      fprintf(out_fd, " %lu", str2ulong(avifile1->idx[i]+12));
      // flags (keyframe?);
      fprintf(out_fd, " %d", (str2ulong(avifile1->idx[i]+ 4))?1:0);

      // ms (not available here)
      fprintf(out_fd, " %.2f", 0.0);

      fprintf(out_fd, "\n");

      i++;
    }


  }


out:
  if (out_fd) fclose (out_fd);
  AVI_close(avifile1);

  return(0);
}
