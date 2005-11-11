/*
 *  ac3scan.c
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

#include "ac3.h"
#include "ac3scan.h"
#include "magic.h"

#define MAX_BUF 4096
static char sbuffer[MAX_BUF];

int ac3scan(FILE *fd, char *buffer, int size, int *ac_off, int *ac_bytes, int *pseudo_size, int *real_size, int verbose)
{
  int bitrate;

  float rbytes;

  int frame_size, pseudo_frame_size;

  if (fread(buffer, 5, 1, fd) !=1)
    return(TC_IMPORT_ERROR);

  if((frame_size = 2*get_ac3_framesize(buffer+2)) < 1) {
    fprintf(stderr, "(%s) AC3 framesize=%d invalid\n", __FILE__, frame_size);
    return(TC_IMPORT_ERROR);
  }

  // A single AC3 frame produces exactly 1536 samples
  // and for 2 channels and 16bit 6kB bytes PCM audio

  rbytes = (float) (size)/1024/6 * frame_size;
  pseudo_frame_size = (int) (rbytes+0.5); // XXX
  bitrate = get_ac3_bitrate(buffer+2);

  if(verbose) fprintf(stderr, "(%s) AC3 frame %d (%d) bytes | bitrate %d kBits/s | depsize %d | rbytes %f\n", __FILE__, frame_size, pseudo_frame_size, bitrate, size, rbytes);

  // return information

  *ac_off=5;
  *ac_bytes = pseudo_frame_size-(*ac_off);
  *pseudo_size = pseudo_frame_size;
  *real_size = frame_size;

  return(0);
}

static int verbose_flag=TC_QUIET;

int buf_probe_ac3(unsigned char *_buf, int len, pcm_t *pcm)
{

  int j=0, i=0, bitrate, fsize, nfchans;

  char *buffer;

  uint16_t sync_word = 0;

  // need to find syncframe:

  buffer=_buf;

  for(i=0; i<len-4; ++i) {

    sync_word = (sync_word << 8) + (uint8_t) buffer[i];

    if(sync_word == 0x0b77) break;
  }

  if(verbose_flag & TC_DEBUG) fprintf(stderr, "AC3 syncbyte @ %d\n", i);

  if(sync_word != 0x0b77) return(-1);

  j = get_ac3_samplerate(&buffer[i+1]);
  bitrate = get_ac3_bitrate(&buffer[i+1]);
  fsize = 2*get_ac3_framesize(&buffer[i+1]);
  nfchans = get_ac3_nfchans(&buffer[i+1]);

  if(j<0 || bitrate <0) return(-1);

  pcm->samplerate = j;
  pcm->chan = (nfchans<2?2:nfchans);
  pcm->bits = 16;
  pcm->format = CODEC_AC3;
  pcm->bitrate = bitrate;

  if(verbose_flag & TC_DEBUG)
      fprintf(stderr, "(%s) samplerate=%d Hz, bitrate=%d kbps, size=%d bytes\n", __FILE__, pcm->samplerate, bitrate, fsize);

  return(0);
}

void probe_ac3(info_t *ipipe)
{

    // need to find syncframe:

    if(tc_pread(ipipe->fd_in, sbuffer, MAX_BUF) != MAX_BUF) {
	ipipe->error=1;
	return;
    }

    verbose_flag = ipipe->verbose;

    //for single AC3 stream only
    if(buf_probe_ac3(sbuffer, MAX_BUF, &ipipe->probe_info->track[0])<0) {
	ipipe->error=1;
	return;
    }
    ipipe->probe_info->magic = TC_MAGIC_AC3;
    ++ipipe->probe_info->num_tracks;

    return;
}

#define inc(a) do { if (buf-_buf==len-(a)) return -1; else buf += (a); } while (0)
int buf_probe_dts (unsigned char *_buf, int len, pcm_t *pcm)
{
    int i=0;
    unsigned char *buf = _buf;
    int frame_type;
    int sample_count;
    int has_crc;
    int nrpcm_samples;
    int frame_size;
    int channels;
    int frequency;
    int bitrate;
    int emb_downmix, emb_drc, emb_ts, emb_aux, hdcd_fmt;
    const int chantab[] = { 1, 2, 2, 2, 2, 3, 3, 4, 4, 5, 6, 6, 6, 7, 8, 8 };
    const int freqtab[] =
    { -1, 8000, 16000, 32000, -1, -1, 11025, 22050, 44100, -1, -1, 12000, 24000, 48000, -1, -1 };
    const int ratetab[] =  {
	32, 56, 64, 96, 112, 128, 192, 224, 256, 320,
	384, 448, 512, 576, 640, 768, 960, 1024, 1152,
	1280, 1344, 1408, 1411/*.2*/, 1472, 1536, 1920,
	2048, 3072, 3840, -1 /*open*/, 1 /*Variable*/, 0 /*Loss-less*/
    };


    //printf("DTS DUMP: "); for (i=0; i<16; i++) printf("%02X", buf[i]); printf("\n");

    for (i=0; i<len-5; i++, buf++) {
	    if (buf[0]==0x7f && buf[1]==0xfe && buf[2]==0x80 && buf[3]==0x01) {
		//printf("DTS: found SYNC word at offset 0x%x\n", i);
		break;
	    }
    }
    /*
          buf[0]   buf[1]   buf[2]  buf[3]   buf[4]    buf[5]
       <---------><-------><------><-------><--------><----------->
       1 11111 0 0001111 00001111101101 001001 1101 01111 0 0 0 0 0
       1 23456 7 8123456 78123456781234 567812 3456 78123 4 5 6 7 8

     */

    inc(4);
    frame_type = buf[0]>>7&0x1;
    sample_count = buf[0]>>2&0x1f;
    has_crc = buf[0]>>1&0x1;
    nrpcm_samples = ( (buf[0]&0x1)<<4&0x10 ) | ( buf[1]>>2&0xf );
    frame_size = ( (buf[1]&0x3)<<16 | buf[2]<<8 | (buf[3]&0xf0 )) >> 4;
    channels = (buf[3]&0xf)<<2 | (buf[4]>>6&0x3);
    frequency = (buf[4]&0x3C)>>2;
    bitrate = (buf[4]&0x3)<<3 | (buf[5]>>5&0x7);
    emb_downmix = buf[5]>>4&0x1;
    emb_drc = buf[5]>>3&0x1;
    emb_ts = buf[5]>>2&0x1;
    emb_aux = buf[5]>>1&0x1;
    hdcd_fmt = buf[5]&0x1;

    if (channels>=0 && channels<=0xf) channels=chantab[channels];
    else channels=2;

    frequency = freqtab[frequency];
    bitrate =  ratetab[bitrate];
    pcm->samplerate = frequency;
    pcm->bitrate = bitrate;
    pcm->chan = channels;
    pcm->format = CODEC_DTS;
    pcm->bits = 16;

    if (verbose_flag & TC_DEBUG) {
	fprintf(stderr, " DTS: *** Detailed DTS header analysis ***\n");
	fprintf(stderr, " DTS: Frametype: %s\n", frame_type?"normal frame":"termination frame");
	fprintf(stderr, " DTS: Samplecount: %d (%s)\n", sample_count, (sample_count==31?"not short":"short"));
	fprintf(stderr, " DTS: CRC present: %s\n", has_crc?"yes":"no");
	fprintf(stderr, " DTS: PCM Samples Count: %d (%s)\n", nrpcm_samples, nrpcm_samples<5?"invalid":"valid");
	fprintf(stderr, " DTS: Frame Size Bytes: %d (%s)\n", frame_size, frame_size<94?"invalid":"valid");
	fprintf(stderr, " DTS: Channels: %d\n",channels);
	fprintf(stderr, " DTS: Frequency: %d Hz\n",frequency );
	fprintf(stderr, " DTS: Bitrate: %d kbps\n",bitrate );
	fprintf(stderr, " DTS: Embedded Down Mix Enabled: %s\n", emb_downmix?"yes":"no");
	fprintf(stderr, " DTS: Embedded Dynamic Range Flag: %s\n", emb_drc?"yes":"no");
	fprintf(stderr, " DTS: Embedded Time Stamp Flag: %s\n", emb_ts?"yes":"no");
	fprintf(stderr, " DTS: Auxiliary Data Flag: %s\n", emb_aux?"yes":"no");
	fprintf(stderr, " DTS: HDCD format: %s\n", hdcd_fmt?"yes":"no");
    }


    return 0;
}
#undef inc

void probe_dts(info_t *ipipe)
{

    // need to find syncframe:

    if(tc_pread(ipipe->fd_in, sbuffer, MAX_BUF) != MAX_BUF) {
	ipipe->error=1;
	return;
    }

    verbose_flag = ipipe->verbose;

    //for single DTS stream only
    if(buf_probe_dts(sbuffer, MAX_BUF, &ipipe->probe_info->track[0])<0) {
	ipipe->error=1;
	return;
    }
    ipipe->probe_info->magic = TC_MAGIC_DTS;
    ++ipipe->probe_info->num_tracks;

    return;
}

