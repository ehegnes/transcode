/*
 *  scan_pack.c
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
#include <string.h>
#include <sys/errno.h>
#include <errno.h>
#include <unistd.h>

#include "transcode.h"
#include "ioaux.h"
#include "aux_pes.h"
#include "seqinfo.h"
#include "demuxer.h"
#include "packets.h"

/* ------------------------------------------------------------ 
 *
 * auxiliary routines
 *
 * ------------------------------------------------------------*/

static char *picture_structure_str[4] = {
    "Invalid Picture Structure",
    "Top field",
    "Bottom field",
    "Frame Picture"
};

static int _cmp_32_bits(char *buf, long x)
{

    if(0) {
	fprintf(stderr, "MAGIC: 0x%02lx 0x%02lx 0x%02lx 0x%02lx\n", (x >> 24) & 0xff, ((x >> 16) & 0xff), ((x >>  8) & 0xff), ((x      ) & 0xff));
	fprintf(stderr, " FILE: 0x%02x 0x%02x 0x%02x 0x%02x\n", buf[0] & 0xff, buf[1] & 0xff, buf[2] & 0xff, buf[3] & 0xff);
    }
	
    if ((buf[0]& 0xff) != ((x >> 24) & 0xff))
	return 0;
    if ((buf[1]& 0xff) != ((x >> 16) & 0xff))
	return 0;
    if ((buf[2] & 0xff)!= ((x >>  8) & 0xff))
	return 0;
    if ((buf[3]& 0xff) != ((x      ) & 0xff))
	return 0;
    
  // OK found it
  return 1;
}


static int _cmp_16_bits(char *buf, long x)
{
  if(0) {
    fprintf(stderr, "MAGIC: 0x%02lx 0x%02lx 0x%02lx 0x%02lx %s\n", (x >> 24) & 0xff, ((x >> 16) & 0xff), ((x >>  8) & 0xff), ((x      ) & 0xff), filetype(x));
    fprintf(stderr, " FILE: 0x%02x 0x%02x 0x%02x 0x%02x\n", buf[2] & 0xff, buf[3] & 0xff, buf[0] & 0xff, buf[1] & 0xff);
  }
    
    if ((uint8_t)buf[0] != ((x >>  8) & 0xff))
	return 0;
    if ((uint8_t)buf[1] != ((x      ) & 0xff))
	return 0;
  
  // OK found it
  return 1;
}

static int pack_scan_16(char *video, long magic)
{
    int k, off = (video[VOB_PACKET_OFFSET] & 0xff) + VOB_PACKET_OFFSET + 1;

    for(k=off; k<=VOB_PACKET_SIZE-2; ++k) {
	if(_cmp_16_bits(video+k, magic)) return(k);
    }// scan buffer
    return(-1);
}


static int pack_scan_32(char *video, long magic)
{
    int k, off = (video[VOB_PACKET_OFFSET] & 0xff) + VOB_PACKET_OFFSET + 1;
    
    for(k=off; k<=VOB_PACKET_SIZE-4; ++k) {
	if(_cmp_32_bits(video+k, magic)) return(k);
    }// scan buffer
    return(-1);
}

int flag1=0, flag2=0, flag3=0;

int scan_pack_pics(char *video)
{

   int k, off = (video[VOB_PACKET_OFFSET] & 0xff) + VOB_PACKET_OFFSET + 1;
  
   int ctr=0;
   
   
   if(flag1) if( (video[off] & 0xff) == 0) ++ctr;
   if(flag2) if( (video[off] & 0xff) == 1  &&  (video[off+1] & 0xff) == 0) ++ctr;
   if(flag3) if( (video[off] & 0xff) == 0  && (video[off+1] & 0xff) == 1  &&  (video[off+2] & 0xff) == 0) ++ctr;

//   fprintf(stderr, "off=%d byte=0x%x byte=0x%x ctr=%d\n", off , (video[off] & 0xff), (video[VOB_PACKET_SIZE-4] & 0xff), ctr);

   if(ctr && (verbose & TC_PRIVATE)) fprintf(stderr, "split PIC code detected\n");

   flag1=flag2=flag3=0;

   for(k=off; k<=VOB_PACKET_SIZE-4; ++k) {
     if(_cmp_32_bits(video+k, MPEG_PICTURE_START_CODE)) ++ctr;
   }

   if( (video[VOB_PACKET_SIZE-1] & 0xff) == 0) flag3=1;
   if( (video[VOB_PACKET_SIZE-2] & 0xff) == 0 && (video[VOB_PACKET_SIZE-1] & 0xff) == 0) flag2=1;
   if( (video[VOB_PACKET_SIZE-3] & 0xff) == 0 && (video[VOB_PACKET_SIZE-2] & 0xff) == 0 && (video[VOB_PACKET_SIZE-1] & 0xff) == 1) flag1=1;

//   fprintf(stderr, "ctr= %d | f1=%d, f2=%d, f3=%d\n", ctr, flag1, flag2, flag3);

   return(ctr);
}

int scan_pack_ext(char *buf)
{

  int n, ret_code=-1;
  
  for(n=0; n<VOB_PACKET_SIZE-4; ++n) {
      
      if(_cmp_32_bits(buf+n, TC_MAGIC_PICEXT) && ((uint8_t) buf[n+4]>>4)==8){
	  ret_code = probe_picext(buf+n+4);
      }
  } // probe extension header
  
  return(ret_code);
}


void scan_pack_payload(char *video, int n, int verbose)
{

    int k;
    char buf[256];
    unsigned long i_pts, i_dts;

    int aud_tag, vid_tag;

    int len;

    double pts;

    seq_info_t si;

    // scan payload
    
    // time stamp:
    memcpy(buf, &video[4], 6);
    pts = read_time_stamp(buf);

    //    printf("PTS=%ld %d %f %ld\n",  read_time_stamp_long(buf),read_ts(buf), read_ts(buf)/90000., parse_pts(buf, 2));
    
    // payload length
    len = stream_read_int16(&video[18]);
    
    printf("[%06d] id=0x%x SCR=%12.8f size=%4d\n", n, (video[17] & 0xff), pts, len);
    
    
    if((video[17] & 0xff) == P_ID_MPEG) {
	
	if((k=pack_scan_32(video, TC_MAGIC_M2V))!=-1) {
	    
	    printf("\tMPEG SEQ start code found in packet %d, offset %4d\n", n, k);
	    
	    
	    //read packet header 
	    memcpy(buf, &video[20], 16);
	    get_pts_dts(buf, &i_pts, &i_dts);
	    
	    printf( "\tPTS=%f DTS=%f\n", (double) i_pts / 90000., (double) i_dts / 90000.);

	    stats_sequence(&video[k+4], &si);
    
	}
	
	if((k=pack_scan_32(video, MPEG_SEQUENCE_END_CODE))!=-1) 
	    printf("\tMPEG SEQ   end code found in packet %d, offset %4d\n", n, k);
	
	if((k=pack_scan_32(video, MPEG_EXT_START_CODE))!=-1) {
	    
	    if(((uint8_t)video[k+4]>>4)==8) {
		int mode = probe_picext(&video[k+4]);
		printf("\tMPEG EXT start code found in packet %d, offset %4d, %s\n", n, k, picture_structure_str[mode]);
	    } else 
		printf("\tMPEG EXT start code found in packet %d, offset %4d\n", n, k);
	}

	if((k=pack_scan_32(video, MPEG_GOP_START_CODE))!=-1) {
	    printf("\tMPEG GOP start code found in packet %d, offset %4d, gop [%03d] ", n, k, gop_cnt);
	    gop_pts=pts;
	    ++gop_cnt;
	    gop=1;
	    probe_group((uint8_t*) &video[k+4]);
	}
	
	if((k=pack_scan_32(video, MPEG_PICTURE_START_CODE))!=-1) 
	    printf("\tMPEG PIC start code found in packet %d, offset %4d\n", n, k);
	
	if((k=pack_scan_32(video, MPEG_SYSTEM_START_CODE))!=-1)
	    printf("\tMPEG SYS start code found in packet %d, offset %4d\n", n, k);
	
	if((k=pack_scan_32(video, MPEG_PADDING_START_CODE))!=-1)
	    printf("\tMPEG PAD start code found in packet %d, offset %4d\n", n, k);
    }
    
    if((video[17] & 0xff) == P_ID_AC3) {
	
	      //position of track code
	      uint8_t *ibuf=video+14;
	      uint8_t *tmp=ibuf + 9 + ibuf[8];

	      //read packet header 
	      memcpy(buf, &video[20], 16);
	      get_pts_dts(buf, &i_pts, &i_dts);
	      
	      printf("\t[%s] substream PTS=%f [0x%x]\n", __FILE__, (double) i_pts / 90000., *tmp);
	      
	      if((k=pack_scan_16(video, TC_MAGIC_AC3))!=-1) {
		if(gop) {
		  
		  printf("\tAC3 sync frame, packet %6d, offset %3d, gop [%03d], A-V %.3f\n", n, k, gop_cnt-1, pts-gop_pts);
		  gop=0;
		  
	    } else 
	      printf("\tAC3 sync frame found in packet %d, offset %d\n", n, k);
	}
	
	if((k=pack_scan_32(video, MPEG_PADDING_START_CODE))!=-1)
	    printf("\tMPEG PAD start code found in packet %d, offset %4d\n", n, k);
	
    }
    
    if((video[17] & 0xff) >= 0xc0 && (video[17] & 0xff) <= 0xdf) {

      //read packet header 
      memcpy(buf, &video[20], 16);
      get_pts_dts(buf, &i_pts, &i_dts);
      
      printf("\tMPEG audio PTS=%f [0x%x]\n", (double) i_pts / 90000., (video[17] & 0xff));
    }
    
    if((video[17] & 0xff) == P_ID_PROG) {
	
	aud_tag = (video[23]>>2) & 0x3f;
	vid_tag = video[24] & 0x1f;
	
	printf("\tMPEG PRG start code found in packet %d, A=%d, V=%d\n", n, aud_tag, vid_tag);
	
    }// check for sync packet
    
    return;
}

int scan_pack_header(char *buf, long x)
{

    int ret = _cmp_32_bits(buf, x);
    if(0) fprintf(stderr, "ret=%d\n", ret);
    return(ret);
}
