/*
 *  scan_pes.c
 *
 *  Copyright (C) Thomas �streich - June 2001
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

#include "transcode.h"
#include "ac3.h"
#include "ioaux.h"
#include "tc.h"
#include "aux_pes.h"
#include "mpg123.h"
#include "ac3scan.h"
#include "demuxer.h"

#define BUFFER_SIZE 262144
static uint8_t buffer[BUFFER_SIZE];

static seq_info_t si;

static int mpeg_version=0;

static int unit_ctr=0, seq_ctr=0;

static uint16_t id;

static uint32_t stream[256], track[TC_MAX_AUD_TRACKS], attr[TC_MAX_AUD_TRACKS];

static int tot_seq_ctr=0, tot_unit_ctr=0;
static unsigned int tot_bitrate=0, min_bitrate=(unsigned int)-1, max_bitrate=0;

//count packs for each presntation unit
static uint32_t unit_pack_cnt[256], unit_pack_cnt_index=0;

static int ref_pts=0;

static int show_seq_info=0, show_ext_info=0;

static int cmp_32_bits(char *buf, long x)
{

    if ((uint8_t)buf[0] != ((x >> 24) & 0xff))
	return 0;
    if ((uint8_t)buf[1] != ((x >> 16) & 0xff))
	return 0;
    if ((uint8_t)buf[2] != ((x >>  8) & 0xff))
	return 0;
    if ((uint8_t)buf[3] != ((x      ) & 0xff))
	return 0;

  // OK found it
  return 1;
}

static void unit_summary(void)
{
    int n;

    int pes_total=0;

    fprintf(stderr, "------------- presentation unit [%d] ---------------\n", unit_ctr);

    for(n=0; n<256; ++n) {
	if(stream[n] && n != 0xba) fprintf(stderr, "stream id [0x%x] %6d\n", n, stream[n]);

	if(n != 0xba) pes_total+=stream[n];
	stream[n]=0; //reset or next unit
    }

    fprintf(stderr, "%d packetized elementary stream(s) PES packets found\n", pes_total);

    fprintf(stderr, "presentation unit PU [%d] contains %d MPEG video sequence(s)\n", unit_ctr, seq_ctr);
    if (seq_ctr) {
    fprintf(stderr, "Average Bitrate is %u. Min Bitrate is %u, max is %u (%s)\n",
	((tot_bitrate*400)/1000)/seq_ctr, min_bitrate*400/1000, max_bitrate*400/1000,
	(max_bitrate==min_bitrate)?"CBR":"VBR");
    }

    ++tot_unit_ctr;
    tot_seq_ctr+=seq_ctr;

    fprintf(stderr, "---------------------------------------------------\n");

    //reset counters
    seq_ctr=0;
    show_seq_info=0;

    fflush(stdout);
}

static int mpeg1_skip_table[16] = {
  1, 0xffff,      5,     10, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff
};

/*------------------------------------------------------------------
 *
 * full source scan mode:
 *
 *------------------------------------------------------------------*/

void scan_pes(int verbose, FILE *in_file)
{
  int n, has_pts_dts=0;
  unsigned long i_pts, i_dts;

  uint8_t * buf;
  uint8_t * end;
  uint8_t * tmp1=NULL;
  uint8_t * tmp2=NULL;
  int complain_loudly;

  long int pack_header_last=0, pack_header_ctr=0, pack_header_pos=0, pack_header_inc=0;

  char scan_buf[256];

  complain_loudly = 1;
  buf = buffer;

    for(n=0; n<256; ++n) stream[n]=0;

    do {
      end = buf + fread (buf, 1, buffer + BUFFER_SIZE - buf, in_file);
      buf = buffer;

      //scan buffer
      while (buf + 4 <= end) {

	// check for valid start code
	if (buf[0] || buf[1] || (buf[2] != 0x01)) {
	  if (complain_loudly) {

	    fprintf (stderr, "missing start code at %#lx\n",
		     ftell (in_file) - (end - buf));
	    if ((buf[0] == 0) && (buf[1] == 0) && (buf[2] == 0))
	      fprintf (stderr, "incorrect zero-byte padding detected - ignored\n");

	    complain_loudly = 0;
	  }
	  buf++;
	  continue;
	}// check for valid start code


	id = buf[3] &  0xff;


	switch (buf[3]) {

	case 0xb9:	/* program end code */

	    fprintf(stderr, "found program end code [0x%x]\n", buf[3] & 0xff);

	    goto summary;

	case 0xba:	/* pack header */

	    pack_header_pos = ftell (in_file) - (end - buf);
	    pack_header_inc = pack_header_pos - pack_header_last;

	    if (pack_header_inc==0) {
		fprintf(stderr, "found first packet header at stream offset 0x%#x\n", 0);
	    } else {
		if((pack_header_inc-((pack_header_inc>>11)<<11))) printf ("pack header out of sequence at %#lx (+%#lx)\n", pack_header_ctr, pack_header_inc);
	    }

	    pack_header_last=pack_header_pos;
	    ++pack_header_ctr;
	    ++stream[id];

	    /* skip */
	    if ((buf[4] & 0xc0) == 0x40)	        /* mpeg2 */
		tmp1 = buf + 14 + (buf[13] & 7);
	    else if ((buf[4] & 0xf0) == 0x20)	        /* mpeg1 */
		tmp1 = buf + 12;
	    else if (buf + 5 > end)
		goto copy;
	    else {
		fprintf (stderr, "weird pack header\n");
		import_exit(1);
	    }

	    if (tmp1 > end)
		goto copy;
	    buf = tmp1;
	    break;


	case 0xbd:	/* private stream 1 */

	  if(!stream[id]) fprintf(stderr, "found %s stream [0x%x]\n", "private_stream_1", buf[3] & 0xff);

	  ++stream[id];

	  tmp2 = buf + 6 + (buf[4] << 8) + buf[5];
	  if (tmp2 > end)
	    goto copy;
	  if ((buf[6] & 0xc0) == 0x80)	/* mpeg2 */
	    tmp1 = buf + 9 + buf[8];
	  else {	/* mpeg1 */
	    for (tmp1 = buf + 6; *tmp1 == 0xff; tmp1++)
	      if (tmp1 == buf + 6 + 16) {
		fprintf (stderr, "too much stuffing\n");
		buf = tmp2;
		break;
	      }
	    if ((*tmp1 & 0xc0) == 0x40)
	      tmp1 += 2;
	    tmp1 += mpeg1_skip_table [*tmp1 >> 4];
	  }

	  if(verbose & TC_DEBUG) fprintf(stderr, "[0x%x] (sub_id=0x%02x)\n", buf[3] & 0xff, *tmp1);

	  if((*tmp1-0x80) >= 0 && (*tmp1-0x80)<TC_MAX_AUD_TRACKS && !track[*tmp1-0x80] ) {
	    fprintf(stderr, "found AC3 audio track %d [0x%x]\n", *tmp1-0x80, *tmp1);
	    track[*tmp1-0x80]=1;
	  }

	  buf = tmp2;

	  break;

	case 0xbf:

	  if(!stream[id]) fprintf(stderr, "found %s [0x%x]\n", "navigation pack", buf[3] & 0xff);

	  ++stream[id];

	  tmp2 = buf + 6 + (buf[4] << 8) + buf[5];
	  if (tmp2 > end)
	    goto copy;

	  buf = tmp2;

	  break;

	case 0xbe:

	  if(!stream[id]) fprintf(stderr, "found %s stream [0x%x]\n", "padding", buf[3] & 0xff);

	  ++stream[id];

	  tmp2 = buf + 6 + (buf[4] << 8) + buf[5];
	  if (tmp2 > end)
	    goto copy;

	  buf = tmp2;

	  break;

	case 0xbb:

	  if(!stream[id]) fprintf(stderr, "found %s stream [0x%x]\n", "unknown", buf[3] & 0xff);

	  ++stream[id];

	  tmp2 = buf + 6 + (buf[4] << 8) + buf[5];
	  if (tmp2 > end)
	    goto copy;

	  buf = tmp2;

	  break;


	  //MPEG audio, maybe more???

	case 0xc0:
	case 0xc1:
	case 0xc2:
	case 0xc3:
	case 0xc4:
	case 0xc5:
	case 0xc6:
	case 0xc7:
	case 0xc8:
	case 0xc9:
	case 0xca:
	case 0xcb:
	case 0xcc:
	case 0xcd:
	case 0xce:
	case 0xcf:
	case 0xd0:
	case 0xd1:
	case 0xd2:
	case 0xd3:
	case 0xd4:
	case 0xd5:
	case 0xd6:
	case 0xd7:
	case 0xd8:
	case 0xd9:
	case 0xda:
	case 0xdb:
	case 0xdc:
	case 0xdd:
	case 0xde:
	case 0xdf:

	    if(!stream[id]) fprintf(stderr, "found %s track %d [0x%x]\n", "ISO/IEC 13818-3 or 11172-3 MPEG audio", (buf[3] & 0xff) - 0xc0, buf[3] & 0xff);

	    ++stream[id];

	    tmp2 = buf + 6 + (buf[4] << 8) + buf[5];
	    if (tmp2 > end)
		goto copy;
	    if ((buf[6] & 0xc0) == 0x80)	/* mpeg2 */
		tmp1 = buf + 9 + buf[8];
	    else {	/* mpeg1 */
		for (tmp1 = buf + 6; *tmp1 == 0xff; tmp1++)
		    if (tmp1 == buf + 6 + 16) {
			fprintf (stderr, "too much stuffing\n");
		buf = tmp2;
		break;
		    }
		if ((*tmp1 & 0xc0) == 0x40)
		    tmp1 += 2;
		tmp1 += mpeg1_skip_table [*tmp1 >> 4];
	    }

	    buf = tmp2;

	    break;

	case 0xe0:	/* video */
	case 0xe1:	/* video */
	case 0xe2:	/* video */
	case 0xe3:	/* video */
	case 0xe4:	/* video */
	case 0xe5:	/* video */
	case 0xe6:	/* video */
	case 0xe7:	/* video */
	case 0xe8:	/* video */
	case 0xe9:	/* video */

	    id = buf[3] &  0xff;

	    tmp2 = buf + 6 + (buf[4] << 8) + buf[5];
	    if (tmp2 > end)
		goto copy;
	    if ((buf[6] & 0xc0) == 0x80) {
		/* mpeg2 */
		tmp1 = buf + 9 + buf[8];

		if(!stream[id]) fprintf(stderr, "found %s stream [0x%x]\n", "ISO/IEC 13818-2 or 11172-2 MPEG video", buf[3] & 0xff);
		++stream[id];

		mpeg_version=2;

		// get pts time stamp:
		ac_memcpy(scan_buf, &buf[6], 16);
		has_pts_dts=get_pts_dts(scan_buf, &i_pts, &i_dts);

		if(has_pts_dts) {

		  if(!show_seq_info) {
		    for(n=0; n<100; ++n) {

		      if(cmp_32_bits(buf+n, TC_MAGIC_M2V)) {
			stats_sequence(buf+n+4, &si);
			show_seq_info=1;
			break;
		      }
		    }
		  }
		  for(n=0; n<100; ++n) {
		    if(cmp_32_bits(buf+n, TC_MAGIC_M2V)) {
		      stats_sequence_silent(buf+n+4, &si);
		      if (si.brv>max_bitrate) max_bitrate=si.brv;
		      if (si.brv<min_bitrate) min_bitrate=si.brv;
		      tot_bitrate += si.brv;
		      break;
		    }
		  }

		  if( ref_pts != 0 && i_pts < ref_pts) {

		    unit_summary();
		    unit_ctr++;
		  }
		  ref_pts=i_pts;
		  ++seq_ctr;
		}

	    } else {
	      /* mpeg1 */

	      if(!stream[id]) fprintf(stderr, "found %s stream [0x%x]\n", "MPEG-1 video", buf[3] & 0xff);
	      ++stream[id];

	      mpeg_version=1;

	      if(!show_seq_info) {
		for(n=0; n<100; ++n) {

		  if(cmp_32_bits(buf+n, TC_MAGIC_M2V)) {
		    stats_sequence(buf+n+4, &si);
		    show_seq_info=1;
		  }
		}
	      }

	      // get pts time stamp:
	      ac_memcpy(scan_buf, &buf[6], 16);
	      has_pts_dts=get_pts_dts(scan_buf, &i_pts, &i_dts);

	      if(has_pts_dts) {

		if( ref_pts != 0 && i_pts < ref_pts) {

		  //unit_summary();
		  unit_ctr++;
		}
		ref_pts=i_pts;

		++seq_ctr;
	      }

	      for (tmp1 = buf + 6; *tmp1 == 0xff; tmp1++)
		if (tmp1 == buf + 6 + 16) {
		  fprintf (stderr, "too much stuffing\n");
		  buf = tmp2;
		  break;
		}
	      if ((*tmp1 & 0xc0) == 0x40)
		tmp1 += 2;
	      tmp1 += mpeg1_skip_table [*tmp1 >> 4];
	    }

	    buf = tmp2;
	    break;


	case 0xb3:

	    fprintf(stderr, "found MPEG sequence start code [0x%x]\n", buf[3] & 0xff);
	    fprintf(stderr, "(%s) looks like an elementary stream - not program stream\n", __FILE__);

	    stats_sequence(&buf[4], &si);

	    return;

	    break;


	default:
	    if (buf[3] < 0xb9) {
		fprintf(stderr, "(%s) looks like an elementary stream - not program stream\n", __FILE__);

		return;
	    }

	    /* skip */
	    tmp1 = buf + 6 + (buf[4] << 8) + buf[5];
	    if (tmp1 > end)
		goto copy;
	    buf = tmp1;
	    break;

	} //start code selection
      } //scan buffer

      if (buf < end) {
      copy:
	  /* we only pass here for mpeg1 ps streams */
	  memmove (buffer, buf, end - buf);
      }
      buf = buffer + (end - buf);

    } while (end == buffer + BUFFER_SIZE);

    fprintf(stderr, "end of stream reached\n");

 summary:

    unit_summary();

    fprintf(stderr, "(%s) detected a total of %d presentation unit(s) PU and %d sequence(s)\n", __FILE__, tot_unit_ctr, tot_seq_ctr);

}

/*------------------------------------------------------------------
 *
 * probe only mode:
 *
 *------------------------------------------------------------------*/


void probe_pes(info_t *ipipe)
{

    int n, num, has_pts_dts=0;
    int aid, ret, initial_sync=0, has_audio=0;

    unsigned long i_pts, i_dts;
    long probe_bytes=0, total_bytes=0;

    uint8_t * buf;
    uint8_t * end;
    uint8_t * tmp1=NULL;
    uint8_t * tmp2=NULL;

    long pack_pts_1=0, pack_pts_2=0, pack_pts_3=0;

    long int pack_header_last=0, pack_header_ctr=0, pack_header_pos=0, pack_header_inc=0;

    char scan_buf[256];

    double pack_ppp=0;

    FILE *in_file = fdopen(ipipe->fd_in, "r");

    buf = buffer;

    for(n=0; n<256; ++n) stream[n]=unit_pack_cnt[n]=0;

    do {

      probe_bytes = fread (buf, 1, buffer + BUFFER_SIZE - buf, in_file);

      if(probe_bytes<0) {
	ipipe->error=1;
	return;
      }

      total_bytes += probe_bytes;

      //limit amount of search stream bytes
      if(total_bytes > TC_MAX_SEEK_BYTES * ipipe->factor) return;

      end = buf + probe_bytes;
      buf = buffer;

      //scan buffer
      while (buf + 4 <= end) {

	// check for valid start code
	if (buf[0] || buf[1] || (buf[2] != 0x01)) {

	  if (ipipe->verbose & TC_DEBUG) {
	    fprintf (stderr, "missing start code at %#lx\n",
		     ftell (in_file) - (end - buf));
	    if ((buf[0] == 0) && (buf[1] == 0) && (buf[2] == 0))
	      fprintf (stderr, "incorrect zero-byte padding detected - ignored\n");
	  }
	  ipipe->probe_info->attributes=TC_INFO_NO_DEMUX;
	  buf++;
	  continue;
	}// check for valid start code

	id = buf[3] &  0xff;

	switch (buf[3]) {

	  //------------------------------
	  //
	  // packet header start/end code
	  //
	  //------------------------------

	case 0xb9:	/* program end code */
	  return;
	  break;

	case 0xba:	/* pack header */

	    pack_header_pos = ftell (in_file) - (end - buf);
	    pack_header_inc = pack_header_pos - pack_header_last;

	    if((pack_header_inc-((pack_header_inc>>11)<<11)))
	      ipipe->probe_info->attributes=TC_INFO_NO_DEMUX|TC_INFO_MPEG_PS;

	    pack_header_last=pack_header_pos;

	    ++pack_header_ctr;
	    ++stream[id];

	    /* skip */
	    if ((buf[4] & 0xc0) == 0x40) {	                /* mpeg2 */
		tmp1 = buf + 14 + (buf[13] & 7);
		ipipe->probe_info->codec=TC_CODEC_MPEG2;
	    } else if ((buf[4] & 0xf0) == 0x20) {	        /* mpeg1 */
		tmp1 = buf + 12;
		ipipe->probe_info->codec=TC_CODEC_MPEG1;
	    } else if (buf + 5 > end)
		goto copy;
	    else {
		fprintf (stderr, "weird pack header\n");
		import_exit(1);
	    }

	    // get PPP - starts

	    ac_memcpy(scan_buf, &buf[4], 16);
	    pack_pts_2 = read_time_stamp_long(scan_buf);
	    pack_ppp = read_time_stamp(scan_buf);

	    if(pack_pts_2 == pack_pts_1)
	      if(ipipe->verbose & TC_DEBUG) fprintf(stderr, "SCR=%8ld (%8ld) unit=%d @ offset %10.4f (sec)\n", pack_pts_2, pack_pts_1, ipipe->probe_info->unit_cnt, pack_pts_1/90000.0);

	    if(pack_pts_2 < pack_pts_1) {

	      pack_pts_3 += pack_pts_1;

	      if(ipipe->verbose & TC_DEBUG) fprintf(stderr, "SCR=%8ld (%8ld) unit=%d @ offset %10.4f (sec)\n", pack_pts_2, pack_pts_1, ipipe->probe_info->unit_cnt+1, pack_pts_3/90000.0);

	      ++unit_pack_cnt_index;

	      //reset all video/audio information at this point - start

	      memset(ipipe->probe_info, 0, sizeof(probe_info_t));
	      for(n=0; n<256; ++n) stream[n]=0;
	      for(n=0; n<TC_MAX_AUD_TRACKS; ++n) track[n]=attr[n]=0;
	      show_seq_info=0;
	      //reset - ends

	      ipipe->probe_info->unit_cnt=unit_pack_cnt_index;
	    }

	    ++unit_pack_cnt[unit_pack_cnt_index];
	    pack_pts_1 = pack_pts_2;

	    // get PPP - ends

	    if (tmp1 > end)
		goto copy;
	    buf = tmp1;

	    break;

	    //------------------------
	    //
	    // MPEG video
	    //
	    //------------------------

	case 0xe0:	/* video */
	case 0xe1:	/* video */
	case 0xe2:	/* video */
	case 0xe3:	/* video */
	case 0xe4:	/* video */
	case 0xe5:	/* video */
	case 0xe6:	/* video */
	case 0xe7:	/* video */
	case 0xe8:	/* video */
	case 0xe9:	/* video */

	    id = buf[3] &  0xff;

	    tmp2 = buf + 6 + (buf[4] << 8) + buf[5];
	    if (tmp2 > end)
		goto copy;
	    if ((buf[6] & 0xc0) == 0x80) {
		/* mpeg2 */
		tmp1 = buf + 9 + buf[8];

		++stream[id];

		mpeg_version=2;
		ipipe->probe_info->codec=TC_CODEC_MPEG2;

		// get pts time stamp:
		ac_memcpy(scan_buf, &buf[6], 16);
		has_pts_dts=get_pts_dts(scan_buf, &i_pts, &i_dts);

		if(has_pts_dts) {

		  /*
		   I have no idea why the has_audio==0 is there. It seems to
		   cause problems at least for:
		   http://lists.exit1.org/pipermail/transcode-devel/2003-October/000004.html
		   I'll remove it until someone complains -- tibit
		   */
#if 0
		  if(ipipe->probe_info->pts_start==0 || has_audio==0) {
#else
		  if(ipipe->probe_info->pts_start==0) {
#endif
		    ipipe->probe_info->pts_start=(double)i_pts/90000.0;
		    initial_sync=1;
		  }

		  if(!show_seq_info) {

		    for(n=0; n<128; ++n) {

		      if(cmp_32_bits(buf+n, TC_MAGIC_M2V)) {
			probe_sequence(buf+n+4, ipipe->probe_info);
			show_seq_info=1;
		      }
		    }
		  } // probe sequence header
		}


		if(!show_ext_info) {

		  int ret_code=-1;
		  int bb=tmp2-tmp1;

		  if(bb<0 || bb>2048) bb=2048;

		  for(n=0; n<bb; ++n) {

		    if(cmp_32_bits(buf+n, TC_MAGIC_PICEXT)
		       && (buf[n+4]>>4)==8) {

		      ret_code = probe_extension(buf+n+4, ipipe->probe_info);

		      //ret_code
		      //-1 = invalid header
		      // 1 = (TFF=1,RFF=0)
		      // 0 else

		      if(ret_code==1) ++ipipe->probe_info->ext_attributes[0];
		      if(ret_code==0) ++ipipe->probe_info->ext_attributes[1];

		    }
		  } // probe extension header

		  ref_pts=i_pts;
		  ++seq_ctr;
		}

	    } else {
	      /* mpeg1 */

	      ++stream[id];

	      mpeg_version=1;

	      //MPEG1 may have audio but no time stamps
	      initial_sync=1;
	      ipipe->probe_info->codec=TC_CODEC_MPEG1;

	      if(!show_seq_info) {
		for(n=0; n<100; ++n) {

		  if(cmp_32_bits(buf+n, TC_MAGIC_M2V)) {
		    probe_sequence(buf+n+4, ipipe->probe_info);
		    show_seq_info=1;
		  }
		}
	      }

	      // get pts time stamp:
	      ac_memcpy(scan_buf, &buf[6], 16);
	      has_pts_dts=get_pts_dts(scan_buf, &i_pts, &i_dts);

	      if(has_pts_dts) {

		if( ref_pts != 0 && i_pts < ref_pts) unit_ctr++;

		ref_pts=i_pts;

		++seq_ctr;

		if(ipipe->probe_info->pts_start==0 || has_audio==0) {
		  ipipe->probe_info->pts_start=(double)i_pts/90000.0;
		}

	      }

	      for (tmp1 = buf + 6; *tmp1 == 0xff; tmp1++)
		if (tmp1 == buf + 6 + 16) {
		  fprintf (stderr, "too much stuffing\n");
		  buf = tmp2;
		  break;
		}
	      if ((*tmp1 & 0xc0) == 0x40)
		tmp1 += 2;
	      tmp1 += mpeg1_skip_table [*tmp1 >> 4];
	    }

	    buf = tmp2;
	    break;


	    //----------------------------------
	    //
	    // private stream 1
	    //
	    //----------------------------------


	case 0xbd:

	    ++stream[id];

	    tmp2 = buf + 6 + (buf[4] << 8) + buf[5];
	    if (tmp2 > end)
		goto copy;
	    if ((buf[6] & 0xc0) == 0x80)	/* mpeg2 */
		tmp1 = buf + 9 + buf[8];
	    else {	/* mpeg1 */
		for (tmp1 = buf + 6; *tmp1 == 0xff; tmp1++)
		    if (tmp1 == buf + 6 + 16) {
		      fprintf (stderr, "too much stuffing\n");
		      buf = tmp2;
		      break;
		    }
		if ((*tmp1 & 0xc0) == 0x40)
		  tmp1 += 2;
		tmp1 += mpeg1_skip_table [*tmp1 >> 4];
	    }

	    aid = *tmp1;

	    //-------------
	    //
	    //subtitle
	    //
	    //-------------

	    if((aid >= 0x20) && (aid <= 0x3F)) {

	      num=aid-0x20;

	      if(!(attr[num] & PACKAGE_SUBTITLE)) {

		if(!track[num]) {
		  ++ipipe->probe_info->num_tracks;
		  track[num]=1;
		  ipipe->probe_info->track[num].tid=num;
		}

		if(!(ipipe->probe_info->track[num].attribute &
		     PACKAGE_SUBTITLE)&& initial_sync) {
		  ipipe->probe_info->track[num].attribute |= PACKAGE_SUBTITLE;

		  // get pts time stamp:
		  ac_memcpy(scan_buf, &buf[6], 16);
		  has_pts_dts=get_pts_dts(scan_buf, &i_pts, &i_dts);
		}
	      }
	    }

	    //-------------
	    //
	    //AC3 audio
	    //
	    //-------------

	    if(((aid >= 0x80 && aid <= 0x88)) || (aid>=0x90 && aid<=0x9f)) {
	      num=aid-0x80;

	      if(!(attr[num] & PACKAGE_AUDIO_AC3) && initial_sync) {

		if(!track[num]) {
		  ++ipipe->probe_info->num_tracks;
		  track[num]=1;
		  ipipe->probe_info->track[num].tid=num;
		}

		if(!(ipipe->probe_info->track[num].attribute &
		     PACKAGE_AUDIO_AC3)) {

		  tmp1 +=4;

		  //need to scan payload for more AC3 audio info
		  ret = buf_probe_ac3(tmp1, tmp2-tmp1, &ipipe->probe_info->track[num]);
		  if(ret==0) {
		    ipipe->probe_info->track[num].attribute |= PACKAGE_AUDIO_AC3;
		    ac_memcpy(scan_buf, &buf[6], 16);
		    has_pts_dts=get_pts_dts(scan_buf, &i_pts, &i_dts);
		    ipipe->probe_info->track[num].pts_start=(double) i_pts/90000.;
		    has_audio=1;
		  }
		}
	      }
	    }

	    //-------------
	    //
	    // DTS audio
	    //
	    //-------------

	    if((aid >= 0x89) && (aid <= 0x8f)) {
	      num=aid-0x80;

	      if(!(attr[num] & PACKAGE_AUDIO_DTS) && initial_sync) {

		if(!track[num]) {
		  ++ipipe->probe_info->num_tracks;
		  track[num]=1;
		  ipipe->probe_info->track[num].tid=num;
		}

		if(!(ipipe->probe_info->track[num].attribute &
		     PACKAGE_AUDIO_DTS)) {

			tmp1+=4;

		  //need to scan payload for more DTS audio info
		  ipipe->probe_info->track[num].attribute |= PACKAGE_AUDIO_DTS;
		  buf_probe_dts(tmp1, tmp2-tmp1, &ipipe->probe_info->track[num]);

		  ac_memcpy(scan_buf, &buf[6], 16);
		  has_pts_dts=get_pts_dts(scan_buf, &i_pts, &i_dts);
		  ipipe->probe_info->track[num].pts_start=(double) i_pts/90000.;
		  has_audio=1;
	      }
	    }
		}
	    //-------------
	    //
	    //AC3 audio
	    //
	    //-------------

	    if((aid >= 0x80) && (aid <= 0x88)) {
	      num=aid-0x80;

	      if(!(attr[num] & PACKAGE_AUDIO_AC3) && initial_sync) {

		if(!track[num]) {
		  ++ipipe->probe_info->num_tracks;
		  track[num]=1;
		  ipipe->probe_info->track[num].tid=num;
		}

		if(!(ipipe->probe_info->track[num].attribute &
		     PACKAGE_AUDIO_AC3)) {

		  tmp1 +=4;

		  //need to scan payload for more AC3 audio info
		  ret = buf_probe_ac3(tmp1, tmp2-tmp1, &ipipe->probe_info->track[num]);
		  if(ret==0) {
		    ipipe->probe_info->track[num].attribute |= PACKAGE_AUDIO_AC3;
		    ac_memcpy(scan_buf, &buf[6], 16);
		    has_pts_dts=get_pts_dts(scan_buf, &i_pts, &i_dts);
		    ipipe->probe_info->track[num].pts_start=(double) i_pts/90000.;
		    has_audio=1;
		  }
		}
	      }
	    }

	    //-------------
	    //
	    //LPCM audio
	    //
	    //-------------

	    if((aid >= 0xA0) && (aid <= 0xBF)) {
	      num=aid-0xA0;

	      if(!(attr[num] & PACKAGE_AUDIO_PCM) && initial_sync) {

		if(!track[num]) {
		  ++ipipe->probe_info->num_tracks;
		  track[num]=1;
		  ipipe->probe_info->track[num].tid=num;
		}

		if(!(ipipe->probe_info->track[num].attribute &
		     PACKAGE_AUDIO_PCM)) {

		  tmp1 += 4;

		  ipipe->probe_info->track[num].attribute |= PACKAGE_AUDIO_PCM;

		  switch ((tmp1[1] >> 4) & 3) {
		  case 0: ipipe->probe_info->track[num].samplerate = 48000;
			  break;
		  case 1: ipipe->probe_info->track[num].samplerate = 96000;
			  break;
		  case 2: ipipe->probe_info->track[num].samplerate = 44100;
			  break;
		  case 3: ipipe->probe_info->track[num].samplerate = 32000;
			  break;
		  }
		  switch ((tmp1[1] >> 6) & 3) {
		  case 0: ipipe->probe_info->track[num].bits = 16;
			  break;
		  case 1: ipipe->probe_info->track[num].bits = 20;
			  break;
		  case 2: ipipe->probe_info->track[num].bits = 24;
			  break;
		  default: fprintf (stderr, "unknown LPCM quantization\n");
			  import_exit (1);
		  }
		  ipipe->probe_info->track[num].chan = 1 + (tmp1[1] & 7);
		  ipipe->probe_info->track[num].bitrate
		    = ipipe->probe_info->track[num].samplerate
		      * ipipe->probe_info->track[num].bits
		      * ipipe->probe_info->track[num].chan / 1000;
		  ipipe->probe_info->track[num].format=CODEC_LPCM;

		  ac_memcpy(scan_buf, &buf[6], 16);
		  has_pts_dts=get_pts_dts(scan_buf, &i_pts, &i_dts);
		  ipipe->probe_info->track[num].pts_start=(double) i_pts/90000.;
		  has_audio=1;
		  //ipipe->probe_info->track[num].pts_start=pack_ppp;
		}
	      }
	    }

	    buf = tmp2;

	    break;


	    //------------------------
	    //
	    // MPEG audio
	    //
	    //------------------------

	case 0xc0:
	case 0xc1:
	case 0xc2:
	case 0xc3:
	case 0xc4:
	case 0xc5:
	case 0xc6:
	case 0xc7:
	case 0xc8:
	case 0xc9:
	case 0xca:
	case 0xcb:
	case 0xcc:
	case 0xcd:
	case 0xce:
	case 0xcf:
	case 0xd0:
	case 0xd1:
	case 0xd2:
	case 0xd3:
	case 0xd4:
	case 0xd5:
	case 0xd6:
	case 0xd7:
	case 0xd8:
	case 0xd9:
	case 0xda:
	case 0xdb:
	case 0xdc:
	case 0xdd:
	case 0xde:
	case 0xdf:

	  ++stream[id];

	  tmp2 = buf + 6 + (buf[4] << 8) + buf[5];
	  if (tmp2 > end)
	    goto copy;
	  if ((buf[6] & 0xc0) == 0x80)	/* mpeg2 */
	    tmp1 = buf + 9 + buf[8];
	  else {	/* mpeg1 */
	    for (tmp1 = buf + 6; *tmp1 == 0xff; tmp1++)
	      if (tmp1 == buf + 6 + 16) {
		fprintf (stderr, "too much stuffing\n");
		buf = tmp2;
		break;
	      }
	    if ((*tmp1 & 0xc0) == 0x40)
	      tmp1 += 2;
	    tmp1 += mpeg1_skip_table [*tmp1 >> 4];
	  }

	  //add found track
	  //need to scan payload for more MPEG audio info

	  num=(buf[3] & 0xff) - 0xc0;

	  if(num >= 0 && !track[num] && num<TC_MAX_AUD_TRACKS && initial_sync) {

	    ++ipipe->probe_info->num_tracks;

#ifdef HAVE_LAME
	    //need to scan payload for more MPEG audio info
	    if(end-buf>0) buf_probe_mp3(buf, end-buf, &ipipe->probe_info->track[num]);
#else
	    //all we know for now
	    ipipe->probe_info->track[num].format=CODEC_MP3;
	    ipipe->probe_info->track[num].tid=num;
#endif

	    ac_memcpy(scan_buf, &buf[6], 16);
	    has_pts_dts=get_pts_dts(scan_buf, &i_pts, &i_dts);

	    if(has_pts_dts) {
	      ipipe->probe_info->track[num].pts_start=(double) i_pts/90000.;
	      track[num]=1;
	    }

	    has_audio=1;
	  }

	  buf = tmp2;
	  break;

	case 0xb3:

	  //MPEG video ES
	  probe_sequence(&buf[4], ipipe->probe_info);

	  ipipe->probe_info->codec=TC_CODEC_MPEG;
	  if ((buf[6] & 0xc0) == 0x80) ipipe->probe_info->codec=TC_CODEC_MPEG2;

	  return;
	  break;

	default:
	  if (buf[3] < 0xb9) {
	    fprintf(stderr, "(%s) looks like an elementary stream - not program stream\n", __FILE__);
	    ipipe->probe_info->codec=TC_CODEC_MPEG;
	    if ((buf[6] & 0xc0) == 0x80) ipipe->probe_info->codec=TC_CODEC_MPEG2;
	    return;
	  }

	  /* skip */

	  tmp1 = buf + 6 + (buf[4] << 8) + buf[5];
	  if (tmp1 > end)
	    goto copy;
	  buf = tmp1;
	  break;

	} //start code selection
      } //scan buffer

      if (buf < end) {
      copy:
	  /* we only pass here for mpeg1 ps streams */
	  memmove (buffer, buf, end - buf);
      }
      buf = buffer + (end - buf);

    } while (end == buffer + BUFFER_SIZE);

    return;
}
