/*
  ogminfo -- utility for extracting raw streams from an OGG media file

  Written by Moritz Bunkus <moritz@bunkus.org>
  Integrated into transcode by Tilmann Bitterberg <transcode@tibit.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif

#if !defined(SYSTEM_DARWIN)
#ifdef HAVE_MALLOC_H
# include <malloc.h>
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ioaux.h"
#include "tc.h"

#define NOAUDIO 0
#define NOVIDEO 1
#define NOTEXT  2

unsigned char *xaudio = NULL;
unsigned char *xvideo = NULL;
unsigned char *xtext = NULL;
int no[3];
int      xraw = 0;

int verbose = 0;

#if (HAVE_OGG && HAVE_VORBIS) 

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "ogmstreams.h"

#define BLOCK_SIZE 4096

#define ACVORBIS 0xffff
#define ACPCM    0x0001
#define ACMP3    0x0055
#define ACAC3    0x2000

typedef struct stream_t {
  int                 serial;
  int                 fd;
  double              sample_rate;
  int                 eos, comment;
  int                 sno;
  char                stype;
  ogg_stream_state    instate;
  struct stream_t    *next;

  int                 acodec;
  ogg_int64_t         bwritten;

  ogg_stream_state    outstate;
  int                 packetno;
  ogg_int64_t         max_granulepos;

  int                 subnr;
} stream_t;

stream_t *first;
int      nastreams = 0, nvstreams = 0, ntstreams = 0, numstreams = 0;
char basename[] = "stdout";

void add_stream(stream_t *ndmx) {
  stream_t *cur = first;
  
  if (first == NULL) {
    first = ndmx;
    first->next = NULL;
  } else {
    cur = first;
    while (cur->next != NULL)
      cur = cur->next;
    cur->next = ndmx;
    ndmx->next = NULL;
  }
}

stream_t *find_stream(int fserial) {
  stream_t *cur = first;
  
  while (cur != NULL) {
    if (cur->serial == fserial)
      break;
    cur = cur->next;
  }
  
  return cur;
}

double highest_ts = 0;

int extraction_requested(unsigned char *s, int stream, int type) {
  int i;
  
  if (no[type])
    return 0;
  if (strlen((char *)s) == 0)
    return 1;
  for (i = 0; i < strlen((char *)s); i++)
    if (s[i] == stream)
      return 1;

  return 0;
}

void flush_pages(stream_t *stream, ogg_packet *op) {
  ogg_page page;
  int ih, ib;

  while (ogg_stream_flush(&stream->outstate, &page)) {
    ih = write(stream->fd, page.header, page.header_len);
    ib = write(stream->fd, page.body, page.body_len);
    if (verbose > 1)
      fprintf(stderr, "(%s) x/a%d: %d + %d written\n", __FILE__, stream->sno,
              ih, ib);
  }
}

void write_pages(stream_t *stream, ogg_packet *op) {
  ogg_page page;
  int ih, ib;

  while (ogg_stream_pageout(&stream->outstate, &page)) {
    ih = write(stream->fd, page.header, page.header_len);
    ib = write(stream->fd, page.body, page.body_len);
    if (verbose > 1)
      fprintf(stderr, "(%s) x/a%d: %d + %d written\n", __FILE__, stream->sno,
              ih, ib);
  }
}

void handle_packet(stream_t *stream, ogg_packet *pack, ogg_page *page) {
  int i, hdrlen, end;
  long long lenbytes;
  char *sub;
  char out[100];
  ogg_int64_t pgp, sst;
  
  //fprintf(stderr, "Going handle 1\n");
  if (pack->e_o_s) {
    stream->eos = 1;
    pack->e_o_s = 1;
  }

  if (((double)pack->granulepos * 1000.0 / (double)stream->sample_rate) >
      highest_ts)
    highest_ts = (double)pack->granulepos * 1000.0 /
                  (double)stream->sample_rate;

  switch (stream->stype) {
    case 'v': 
      if (!extraction_requested(xvideo, stream->sno, NOVIDEO))
        return;
      break;
    case 'a': 
      if (!extraction_requested(xaudio, stream->sno, NOAUDIO))
        return;
      break;
    case 't': 
      if (!extraction_requested(xtext, stream->sno, NOTEXT))
        return;
      break;
  }

  hdrlen = (*pack->packet & OGM_PACKET_LEN_BITS01) >> 6;
  hdrlen |= (*pack->packet & OGM_PACKET_LEN_BITS2) << 1;
  for (i = 0, lenbytes = 0; i < hdrlen; i++) {
    lenbytes = lenbytes << 8;
    lenbytes += *((unsigned char *)pack->packet + hdrlen - i);
  }

  switch (stream->stype) {
    case 'v':
      if (((*pack->packet & 3) == OGM_PACKET_TYPE_HEADER) ||
          ((*pack->packet & 3) == OGM_PACKET_TYPE_COMMENT))
        return;
      i = write(stream->fd, (char *)&pack->packet[hdrlen + 1],
	  pack->bytes - 1 - hdrlen);
      if (verbose > 1)
        fprintf(stderr, "(%s) x/v%d: %d written\n", __FILE__, stream->sno, i);
      break;
    case 't':
      if (((*pack->packet & 3) == OGM_PACKET_TYPE_HEADER) ||
          ((*pack->packet & 3) == OGM_PACKET_TYPE_COMMENT))
        return;

      if (xraw) {
        i = write(stream->fd, (char *)&pack->packet[hdrlen + 1],
                  pack->bytes - 1 - hdrlen);
        if (verbose > 1)
          fprintf(stderr, "(%s) x/t%d: %d written\n", __FILE__,
                  stream->sno, i);
        return;
      }

      sub = (char *)&pack->packet[hdrlen + 1];
      if ((strlen(sub) > 1) || (*sub != ' ')) {
        sst = (pack->granulepos / stream->sample_rate) * 1000;
        pgp = sst + lenbytes;
        sprintf(out, "%d\r\n%02d:%02d:%02d,%03d --> " \
                "%02d:%02d:%02d,%03d\r\n", stream->subnr + 1,
                (int)(sst / 3600000),
                (int)(sst / 60000) % 60,
                (int)(sst / 1000) % 60,
                (int)(sst % 1000),
                (int)(pgp / 3600000),
                (int)(pgp / 60000) % 60,
                (int)(pgp / 1000) % 60,
                (int)(pgp % 1000));
        i = write(stream->fd, out, strlen(out));
        end = strlen(sub) - 1;
        while ((end >= 0) && ((sub[end] == '\n') || (sub[end] == '\r'))) {
          sub[end] = 0;
          end--;
        }
        i += write(stream->fd, sub, strlen(sub));
        i += write(stream->fd, "\r\n\r\n", 4);
        stream->subnr++;
        if (verbose > 1)
          fprintf(stderr, "(%s) x/t%d: %d written\n", __FILE__,
                  stream->sno, i);
      }
      break;
    case 'a':
      switch (stream->acodec) {
        case ACVORBIS:
          if (xraw) {
            if (stream->packetno == 0)
              i = write(stream->fd, (char *)pack->packet, pack->bytes);
            else
              i = write(stream->fd, (char *)&pack->packet[1],
                        pack->bytes - 1);
            if (verbose > 1)
              fprintf(stderr, "(%s) x/a%d: %d written\n", __FILE__,
                      stream->sno, i);
            return;
          }
          stream->max_granulepos = (pack->granulepos > stream->max_granulepos ?
                                    pack->granulepos : stream->max_granulepos);
          if ((stream->packetno == 0) || (stream->packetno == 2)) {
            ogg_stream_packetin(&stream->outstate, pack);
            flush_pages(stream, pack);
          } else {
            ogg_stream_packetin(&stream->outstate, pack);
            write_pages(stream, pack);
          }
          stream->packetno++;
          break;
        default:
          if (((*pack->packet & 3) == OGM_PACKET_TYPE_HEADER) ||
              ((*pack->packet & 3) == OGM_PACKET_TYPE_COMMENT))
            return;

          i = write(stream->fd, pack->packet + 1 + hdrlen,
                    pack->bytes - 1 - hdrlen);
          stream->bwritten += i;
          if (verbose > 1)
            fprintf(stderr, "(%s) x/a%d: %d written\n", __FILE__,
                    stream->sno, i);
          break;
      }
    break;
  }
}

void process_ogm(int fdin, int fdout)
{
  ogg_sync_state    sync;
  ogg_page          page;
  ogg_packet        pack;
  vorbis_info       vi;
  vorbis_comment    vc;
  char             *buf, *new_name;
  int               nread, np, sno;
  int               endofstream = 0;
  stream_t         *stream = NULL;

  ogg_sync_init(&sync);
  while (1) {
    np = ogg_sync_pageseek(&sync, &page);
    if (np < 0) {
      fprintf(stderr, "(%s) ogg_sync_pageseek failed\n", __FILE__);
      return;
    }
    if (np == 0) {
      buf = ogg_sync_buffer(&sync, BLOCK_SIZE);
      if (!buf) {
        fprintf(stderr, "(%s) ogg_sync_buffer failed\n", __FILE__);
        return;
      }
      if ((nread = read(fdin, buf, BLOCK_SIZE)) <= 0) {
        if (verbose > 0)
          fprintf(stderr, "(%s) end of stream 1\n", __FILE__);
        return;
      }
      ogg_sync_wrote(&sync, nread);
      continue;
    }

    if (!ogg_page_bos(&page)) {
      break;
    } else {
      ogg_stream_state sstate;
      sno = ogg_page_serialno(&page);
      if (ogg_stream_init(&sstate, sno)) {
        fprintf(stderr, "(%s) ogg_stream_init failed\n", __FILE__);
        return;
      }
      ogg_stream_pagein(&sstate, &page);
      ogg_stream_packetout(&sstate, &pack);

      if ((pack.bytes >= 7) && ! strncmp(&pack.packet[1], "vorbis", 6)) {

        stream = (stream_t *)malloc(sizeof(stream_t));
        if (stream == NULL) {
          fprintf(stderr, "malloc failed.\n");
          exit(1);
        }

        memset(stream, 0, sizeof(stream_t));
        if (verbose > 0) {
          vorbis_info_init(&vi);
          vorbis_comment_init(&vc);
          if (vorbis_synthesis_headerin(&vi, &vc, &pack) >= 0) {
            fprintf(stderr, "(%s) (a%d/%d) Vorbis audio (channels %d " \
                    "rate %ld)\n", __FILE__, nastreams + 1, numstreams + 1,
                    vi.channels, vi.rate);
            stream->sample_rate = vi.rate;
          } else
            fprintf(stderr, "(%s) (a%d/%d) Vorbis audio stream indicated " \
                    "but no Vorbis stream header found.\n", __FILE__,
                    nastreams + 1, numstreams + 1);
        }
        stream->serial = sno;
        stream->acodec = ACVORBIS;
        stream->sample_rate = -1;
        stream->sno = nastreams + 1;
        stream->stype = 'a';
        memcpy(&stream->instate, &sstate, sizeof(sstate));
        if (extraction_requested(xaudio, nastreams + 1, NOAUDIO)) {
	  stream->fd = fdout;
          if (stream->fd == -1) {
            fprintf(stderr, "(%s) Failed to create \"%s\" (%d, %s).\n",
                    __FILE__, "new_name", errno, strerror(errno));
            exit(1);
          }
          if (!xraw)
            ogg_stream_init(&stream->outstate, rand());
          if (verbose > 0)
            fprintf(stderr, "(%s) Extracting a%d to \"%s\".\n", __FILE__,
                    nastreams + 1, "new_name");
          do
            handle_packet(stream, &pack, &page);
          while (ogg_stream_packetout(&stream->instate, &pack) == 1);
        }
        add_stream(stream);
        nastreams++;
        numstreams++;
      } else if ((pack.bytes >= 142) &&
                 !strncmp(&pack.packet[1],"Direct Show Samples embedded in Ogg",
                          35) ) {
        if ((*(int32_t*)(pack.packet+96) == 0x05589f80) &&
            (pack.bytes >= 184)) {
           fprintf(stderr, "(%s) (v%d/%d) Found old video header. Not " \
                   "supported.\n", __FILE__, nvstreams + 1, numstreams + 1);
        } else if (*(int32_t*)pack.packet+96 == 0x05589F81) {
          fprintf(stderr, "(%s) (a%d/%d) Found old audio header. Not " \
                  "supported.\n", __FILE__, nastreams + 1, numstreams + 1);
        } else {
          if (verbose > 0)
            fprintf(stderr, "(%s) OGG stream %d has an old header with an " \
                    "unknown type.", __FILE__, numstreams + 1);
        }
      }  else if (((*pack.packet & OGM_PACKET_TYPE_BITS ) == OGM_PACKET_TYPE_HEADER) &&
	          (pack.bytes >= (int)(sizeof(ogm_stream_header) + 1 - sizeof(int)))) {
        ogm_stream_header *sth = (ogm_stream_header *)(pack.packet + 1);
        if (!strncmp(sth->streamtype, "video", 5)) {
          unsigned long codec;
          char ccodec[5];
          strncpy(ccodec, sth->subtype, 4);
          ccodec[4] = 0;
          codec = (sth->subtype[0] << 24) + 
            (sth->subtype[1] << 16) + (sth->subtype[2] << 8) + sth->subtype[3]; 
          if (verbose > 0)
            fprintf(stderr, "(%s) (v%d/%d) fps: %.3f width height: %dx%d " \
                    "codec: %p (%s)\n", __FILE__, nvstreams + 1,
                    numstreams + 1,
                    (double)10000000 / (double)sth->time_unit,
                    sth->sh.video.width, sth->sh.video.height, (void *)codec,
                    ccodec);
          stream = (stream_t *)malloc(sizeof(stream_t));
          if (stream == NULL) {
            fprintf(stderr, "malloc failed.\n");
            exit(1);
          }
          stream->stype = 'v';
          stream->serial = sno;
          stream->sample_rate = (double)10000000 / (double)sth->time_unit;
          stream->sno = nvstreams + 1;
          memcpy(&stream->instate, &sstate, sizeof(sstate));
          if (extraction_requested(xvideo, nvstreams + 1, NOVIDEO)) {
	    stream->fd = fdout;
            
            if (verbose > 0)
              fprintf(stderr, "(%s) Extracting v%d to \"%s\".\n", __FILE__,
                      nvstreams + 1, "new_name");
            do {
              handle_packet(stream, &pack, &page);
	    } while (ogg_stream_packetout(&stream->instate, &pack) == 1);
          }
          add_stream(stream);
          nvstreams++;
          numstreams++;
        } else if (!strncmp(sth->streamtype, "audio", 5)) {
          int codec;
          char buf[5];
          memcpy(buf, sth->subtype, 4);
          buf[4] = 0;
          codec = strtoul(buf, NULL, 16);
          if (verbose > 0) {
            fprintf(stderr, "(%s) (a%d/%d) codec: %d (0x%04x) (%s), bits per " \
                    "sample: %d channels: %hd  samples per second: %lld",
                    __FILE__, nastreams + 1, numstreams + 1, codec, codec,
                    codec == ACPCM ? "PCM" : codec == 55 ? "MP3" :
                    codec == ACMP3 ? "MP3" :
                    codec == ACAC3 ? "AC3" : "unknown",
                    sth->bits_per_sample, sth->sh.audio.channels,
                    (long long)sth->samples_per_unit);
             fprintf(stderr, " avgbytespersec: %hd blockalign: %d\n",
                     sth->sh.audio.avgbytespersec, sth->sh.audio.blockalign);
          }
          stream = (stream_t *)malloc(sizeof(stream_t));
          if (stream == NULL) {
            fprintf(stderr, "malloc failed.\n");
            exit(1);
          }
          stream->sno = nastreams + 1;
          stream->stype = 'a';
          stream->sample_rate = sth->samples_per_unit *
                                sth->sh.audio.channels;
          stream->serial = sno;
          stream->acodec = codec;
          memcpy(&stream->instate, &sstate, sizeof(sstate));
          if (extraction_requested(xaudio, nastreams + 1, NOAUDIO)) {
              
	    /*
                      codec == ACPCM ? "wav" :
                      codec == ACMP3 ? "mp3" :
                      codec == ACAC3 ? "ac3" : "audio");
		      */
            stream->fd = fdout;
            if (verbose > 0)
              fprintf(stderr, "(%s) Extracting a%d to \"%s\".\n", __FILE__,
                      nastreams + 1, "new_name");
            do {
              handle_packet(stream, &pack, &page);
	    } while (ogg_stream_packetout(&stream->instate, &pack) == 1);
          }
          add_stream(stream);
          nastreams++;
          numstreams++;
        } else if (!strncmp(sth->streamtype, "text", 4)) {
          if (verbose > 0)
            fprintf(stderr, "(%s) (t%d/%d) text/subtitle stream\n", __FILE__,
                    ntstreams + 1, numstreams + 1);
          stream = (stream_t *)malloc(sizeof(stream_t));
          if (stream == NULL) {
            fprintf(stderr, "malloc failed.\n");
            exit(1);
          }
          stream->sno = ntstreams + 1;
          stream->stype = 't';
          stream->sample_rate = (double)10000000 / (double)sth->time_unit;
          stream->serial = sno;
          memcpy(&stream->instate, &sstate, sizeof(sstate));
          if (extraction_requested(xtext, ntstreams + 1, NOTEXT)) {
            new_name = malloc(strlen(basename) + 20);
            if (!new_name) {
              fprintf(stderr, "(%s) Failed to allocate %d bytes.\n", __FILE__,
                (int)strlen(basename) + 10);
              exit(1);
            }
            if (!xraw)
              sprintf(new_name, "%s-t%d.srt", basename, ntstreams + 1);
            else
              sprintf(new_name, "%s-t%d.raw", basename, ntstreams + 1);
            //stream->fd = open(new_name, O_WRONLY | O_CREAT | O_TRUNC,
            //                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            stream->fd = fdout;
            if (stream->fd == -1) {
              fprintf(stderr, "(%s) Failed to create \"%s\" (%d, %s).\n",
                      __FILE__, new_name, errno, strerror(errno));
              exit(1);
            }
            if (verbose > 0)
              fprintf(stderr, "(%s) Extracting t%d to \"%s\".\n", __FILE__,
                      ntstreams + 1, new_name);
            do
              handle_packet(stream, &pack, &page);
            while (ogg_stream_packetout(&stream->instate, &pack) == 1);
          }
          add_stream(stream);
          ntstreams++;
          numstreams++;
        } else {
          fprintf(stderr, "(%s) (%d) found new header of unknown/" \
                  "unsupported type\n", __FILE__, numstreams + 1);
        }

      } else {
        fprintf(stderr, "(%s) OGG stream %d is of an unknown type " \
                "(bad header?)\n", __FILE__, numstreams + 1);
      }
    }
  }

  endofstream = 0;
  while (!endofstream) {
    sno = ogg_page_serialno(&page);
    stream = find_stream(sno);
    if (stream == NULL) {
      if (verbose > 1)
        fprintf(stderr, "(%s) Encountered packet for an unknown serial " \
                "%d !?\n", __FILE__, sno);
    } else {
      if (verbose > 1)
        fprintf(stderr, "(%s) %c%d: NEW PAGE\n",
                __FILE__, stream->stype, stream->sno);
                
      ogg_stream_pagein(&stream->instate, &page);
      while (ogg_stream_packetout(&stream->instate, &pack) == 1)
        handle_packet(stream, &pack, &page);
    }

    while (ogg_sync_pageseek(&sync, &page) <= 0) {
      buf = ogg_sync_buffer(&sync, BLOCK_SIZE);
      nread = read(fdin, buf, BLOCK_SIZE);
      if (nread <= 0) {
        stream = first;
        while (stream != NULL) {
          switch (stream->stype) {
            case 'v':
	      close(stream->fd);
              break;
            case 't':
              if (stream->fd > 0)
                close(stream->fd);
              break;
            case 'a':
              if ((stream->fd > 0) && !xraw) {
                switch (stream->acodec) {
                  case ACVORBIS:
                    if (!stream->eos) {
                      pack.b_o_s = 0;
                      pack.e_o_s = 1;
                      pack.packet = NULL;
                      pack.bytes = 0;
                      pack.granulepos = stream->max_granulepos;
                      pack.packetno = stream->packetno;
                      ogg_stream_packetin(&stream->outstate, &pack);
                    }
                    flush_pages(stream, &pack);
                    ogg_stream_clear(&stream->outstate);
                    break;
                }
                close(stream->fd);
              } else if (stream->fd > 0)
                close(stream->fd);
              break;
          }
          stream = stream->next;
        }
        if (verbose > 0)
          fprintf(stderr, "(%s) end of stream\n", __FILE__);
        endofstream = 1;
        break;
      } else
        ogg_sync_wrote(&sync, nread);
    }
  }
}
#endif /* have OGG */

void extract_ogm (info_t *ipipe)
{
  // track

  no[NOTEXT]  = 1;
  no[NOAUDIO] = 0;
  no[NOVIDEO] = 0;
  xraw = 1;

  xvideo = malloc (16); xaudio = malloc (16); xtext = malloc (16);
  memset (xvideo, 0, 16); memset (xaudio, 0, 16); memset (xtext, 0, 16);

  verbose = ipipe->verbose;

  if (ipipe->select == TC_VIDEO) {

    no[NOAUDIO] = 1;
    xvideo[0] = (unsigned char)(ipipe->track+1);
    
  }

  if (ipipe->select == TC_AUDIO) {

    no[NOVIDEO] = 1;
    xaudio[0] = (unsigned char)(ipipe->track+1);
    
    // we need !xraw because no tool seems to be able to handle 
    // raw vorbis streams -- tibit

    if (ipipe->codec == TC_CODEC_VORBIS) {
      xraw = 0;
    }

  }

#if (HAVE_OGG && HAVE_VORBIS) 
  process_ogm(ipipe->fd_in, ipipe->fd_out);
#else
  fprintf(stderr, "No support for Ogg/Vorbis compiled in\n");
  import_exit(1);
#endif
}



/* vim: sw=2
 */
