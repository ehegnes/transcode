/*
 *  dvd_reader.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *  Copyright (C) 2001 Billy Biggs <vektor@dumbterm.net>.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include "transcode.h"
#include "ioaux.h"

#ifdef HAVE_LIBDVDREAD

#ifdef HAVE_LIBDVDREAD_INC
#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include <dvdread/nav_read.h>
#include <dvdread/nav_print.h>
#else
#include <dvd_reader.h>
#include <ifo_types.h>
#include <ifo_read.h>
#include <nav_read.h>
#include <nav_print.h>
#endif



static long playtime=0;

/**
 * Returns true if the pack is a NAV pack.  This check is clearly insufficient,
 * and sometimes we incorrectly think that valid other packs are NAV packs.  I
 * need to make this stronger.
 */
int is_nav_pack( unsigned char *buffer )
{
    return ( buffer[ 41 ] == 0xbf && buffer[ 1027 ] == 0xbf );
}

static int verbose;
static dvd_reader_t *dvd=NULL;
static unsigned char *data=NULL;

static void ifoPrint_time(dvd_time_t *time) {
  char *rate;

  int i;

  assert((time->hour>>4) < 0xa && (time->hour&0xf) < 0xa);
  assert((time->minute>>4) < 0x7 && (time->minute&0xf) < 0xa);
  assert((time->second>>4) < 0x7 && (time->second&0xf) < 0xa);
  assert((time->frame_u&0xf) < 0xa);
  
  fprintf(stderr,"%02x:%02x:%02x.%02x", 
	 time->hour,
	 time->minute,
	 time->second,
	 time->frame_u & 0x3f);

  i=time->hour>>4;
  
  playtime = (i*10 + time->hour-(i<<4))*60*60;
  //fprintf(stderr,"\n%d %d %ld\n", time->hour, i, playtime);
  
  i=(time->minute>>4);
  
  playtime +=(i*10 + time->minute-(i<<4))*60;
  //fprintf(stderr,"%d %d %ld\n", time->minute, i, playtime);

  i=(time->second>>4);

  playtime +=i*10 + time->second-(i<<4);
  //fprintf(stderr,"%d %d %ld\n", time->second, i, playtime);

  ++playtime;

  switch((time->frame_u & 0xc0) >> 6) {
  case 1:
    rate = "25.00";
    break;
  case 3:
    rate = "29.97";
    break;
  default:
    if(time->hour == 0 && time->minute == 0 
       && time->second == 0 && time->frame_u == 0) {
      rate = "no";
    } else
      rate = "(please send a bug report)";
    break;
  } 
  //  fprintf(stderr," @ %s fps", rate);
}

static void stats_video_attributes(video_attr_t *attr, probe_info_t *probe_info) {
  
  /* The following test is shorter but not correct ISO C,
     memcmp(attr,my_friendly_zeros, sizeof(video_attr_t)) */
  if(attr->mpeg_version == 0 
     && attr->video_format == 0 
     && attr->display_aspect_ratio == 0 
     && attr->permitted_df == 0 
     && attr->unknown1 == 0 
     && attr->line21_cc_1 == 0 
     && attr->line21_cc_2 == 0 
     && attr->video_format == 0 
     && attr->letterboxed == 0 
     && attr->film_mode == 0) {
    printf("(%s) -- Unspecified --", __FILE__);
    return;
  }

  printf("(%s) ", __FILE__);

  switch(attr->mpeg_version) {
  case 0:
    printf("mpeg1 ");
    probe_info->codec=TC_CODEC_MPEG1;
    break;
  case 1:
    printf("mpeg2 ");
    probe_info->codec=TC_CODEC_MPEG2;
    break;
  default:
    printf("(please send a bug report) ");
  }
  
  switch(attr->video_format) {
  case 0:
    printf("ntsc ");
    probe_info->magic=TC_MAGIC_NTSC;
    break;
  case 1:
    printf("pal ");
    probe_info->magic=TC_MAGIC_PAL;
    break;
  default:
    printf("(please send a bug report) ");
  }
  
  switch(attr->display_aspect_ratio) {
  case 0:
    printf("4:3 ");
    probe_info->asr=2;
    break;
  case 3:
    printf("16:9 ");
    probe_info->asr=3;
    break;
  default:
    printf("(please send a bug report) ");
  }
  
  // Wide is allways allowed..!!!
  switch(attr->permitted_df) {
  case 0:
    printf("pan&scan+letterboxed ");
    break;
  case 1:
    printf("only pan&scan "); //??
    break;
  case 2:
    printf("only letterboxed ");
    break;
  case 3:
    // not specified
    break;
  default:
    printf("(please send a bug report)");
  }
  
  printf("U%x ", attr->unknown1);
  assert(!attr->unknown1);
  
  if(attr->line21_cc_1 || attr->line21_cc_2) {
    printf("NTSC CC ");
    if(attr->line21_cc_1)
      printf("1 ");
    if(attr->line21_cc_2)
      printf("2 ");
  }
  
  {
    int height = 480;
    if(attr->video_format != 0) 
      height = 576;
    switch(attr->picture_size) {
    case 0:
      printf("720x%d ", height);
      probe_info->width=720;
      probe_info->height =  height;
      break;
    case 1:
      printf("704x%d ", height);
      probe_info->width=704;
      probe_info->height =  height;
      break;
    case 2:
      printf("352x%d ", height);
      probe_info->width=352;
      probe_info->height =  height;
      break;
    case 3:
      printf("352x%d ", height/2);
      probe_info->width=352;
      probe_info->height =  height/2;
      break;      
    default:
      printf("(please send a bug report) ");
    }
    
  }

  if(attr->letterboxed) {
    printf("letterboxed ");
  }
  
  if(attr->film_mode) {
    printf("film");
  } else {
    printf("video"); //camera
  }
  printf("\n"); 
}

static void stats_audio_attributes(audio_attr_t *attr, int track, probe_info_t *probe_info) {
  
  if(attr->audio_format == 0
     && attr->multichannel_extension == 0
     && attr->lang_type == 0
     && attr->application_mode == 0
     && attr->quantization == 0
     && attr->sample_frequency == 0
     && attr->channels == 0
     && attr->lang_extension == 0
     && attr->unknown1 == 0
     && attr->unknown1 == 0) {
    printf("(%s) -- Unspecified --", __FILE__);
    return;
  }

  //defaults for all tracks:
  ++probe_info->num_tracks;
  probe_info->track[track].chan = 2;
  probe_info->track[track].bits = 16;
  probe_info->track[track].tid = track;

  printf("(%s) ", __FILE__);  
  switch(attr->audio_format) {
  case 0:
    printf("ac3 ");
    probe_info->track[track].format = CODEC_AC3;
    break;
  case 1:
    printf("(please send a bug report) ");
    break;
  case 2:
    printf("mpeg1 ");
    probe_info->track[track].format = CODEC_MP3;
    break;
  case 3:
    printf("mpeg2ext ");
    break;
  case 4:
    printf("lpcm ");
    probe_info->track[track].format = CODEC_LPCM;
    break;
  case 5:
    printf("(please send a bug report) ");
    break;
  case 6:
    printf("dts ");
    probe_info->track[track].format = CODEC_DTS;
    break;
  default:
    printf("(please send a bug report) ");
  }
  
  if(attr->multichannel_extension)
    printf("multichannel_extension ");
  
  switch(attr->lang_type) {
  case 0:
    // not specified
      probe_info->track[track].lang=0;
      break;
  case 1:
      printf("%c%c ", attr->lang_code>>8, attr->lang_code & 0xff);
      probe_info->track[track].lang=attr->lang_code;
      break;
  default:
    printf("(please send a bug report) ");
  }

  switch(attr->application_mode) {
  case 0:
    // not specified
    break;
  case 1:
    printf("karaoke mode ");
    break;
  case 2:
    printf("surround sound mode ");
    break;
  default:
    printf("(please send a bug report) ");
  }
  
  switch(attr->quantization) {
  case 0:
    printf("16bit ");
    break;
  case 1:
    printf("20bit ");
    break;
  case 2:
    printf("24bit ");
    break;
  case 3:
    printf("drc ");
    break;
  default:
    printf("(please send a bug report) ");
  }
  
  switch(attr->sample_frequency) {
  case 0:
    printf("48kHz ");
    probe_info->track[track].samplerate = 48000;
    break;
  default:
    printf("(please send a bug report) ");
  }
  
  printf("%dCh ", attr->channels + 1);
  
  switch(attr->lang_extension) {
  case 0:
    //    printf("Not specified ");
    break;
  case 1: // Normal audio
    printf("Normal Caption ");
    break;
  case 2: // visually imparied
    printf("Audio for visually impaired ");
    break;
  case 3: // Directors 1
    printf("Director's comments #1 ");
    break;
  case 4: // Directors 2
    printf("Director's comments #2 ");
    break;
    //case 4: // Music score ?    
  default:
    printf("(please send a bug report) ");
  }
  
  printf("\n"); 
}

static void stats_subp_attributes(subp_attr_t *attr, int track, probe_info_t *probe_info) {
  
  if(attr->type == 0
     && attr->zero1 == 0
     && attr->lang_code == 0
     && attr->lang_extension == 0
     && attr->zero2 == 0) {
    printf("(%s) -- Unspecified --", __FILE__);
    return;
  }

  printf("(%s) ", __FILE__);  
  
  
  if(attr->type) {
    printf("subtitle %02d=<%c%c> ", track, attr->lang_code>>8, attr->lang_code & 0xff);
    if(attr->lang_extension) printf("ext=%d", attr->lang_extension);
  }
  
  printf("\n"); 
}


int dvd_query(int title, int *arg_chapter, int *arg_angle)
{
    
    int             ttn, pgc_id, titleid;
    tt_srpt_t      *tt_srpt;
    ifo_handle_t   *vmg_file;
    pgc_t          *cur_pgc;    
    ifo_handle_t   *vts_file;
    vts_ptt_srpt_t *vts_ptt_srpt;
    
    vmg_file = ifoOpen( dvd, 0 );
    if( !vmg_file ) {
	fprintf( stderr, "Can't open VMG info.\n" );
	return -1;
    }
    tt_srpt = vmg_file->tt_srpt;
    
    /**
     * Make sure our title number is valid.
     */

    titleid = title-1;
   
    if( titleid < 0 || titleid >= tt_srpt->nr_of_srpts ) {
        fprintf( stderr, "Invalid title %d.\n", titleid + 1 );
        ifoClose( vmg_file );
        return -1;
    }

    // display title infos
    if(verbose &TC_DEBUG) fprintf(stderr, "(%s) DVD title %d: %d chapter(s), %d angle(s)\n", __FILE__, title, tt_srpt->title[ titleid ].nr_of_ptts, tt_srpt->title[ titleid ].nr_of_angles);    

    /**
     * Load the VTS information for the title set our title is in.
     */

    vts_file = ifoOpen( dvd, tt_srpt->title[ titleid ].title_set_nr );
    if( !vts_file ) {
      fprintf( stderr, "Can't open the title %d info file.\n",
	       tt_srpt->title[ titleid ].title_set_nr );
        ifoClose( vmg_file );
        return -1;
    }
    
    ttn = tt_srpt->title[ titleid ].vts_ttn;
    vts_ptt_srpt = vts_file->vts_ptt_srpt;
    pgc_id = vts_ptt_srpt->title[ ttn - 1 ].ptt[0].pgcn;
    cur_pgc = vts_file->vts_pgcit->pgci_srp[ pgc_id - 1 ].pgc;
    
    if(verbose &TC_DEBUG) {
	fprintf(stderr, "(%s) DVD playback time: ", __FILE__);
	ifoPrint_time(&cur_pgc->playback_time); 
	fprintf(stderr, "\n");
    }

    //return info
    *arg_chapter = tt_srpt->title[ titleid ].nr_of_ptts;
    *arg_angle = tt_srpt->title[ titleid ].nr_of_angles;

    return(0);
}

int dvd_probe(int title, probe_info_t *info)
{
 
  int             ttn, pgn, pgc_id, titleid, start_cell, end_cell, i, j;
  tt_srpt_t      *tt_srpt;
  ifo_handle_t   *vmg_file;
  pgc_t          *cur_pgc;    
  ifo_handle_t   *vts_file;
  vts_ptt_srpt_t *vts_ptt_srpt;
  video_attr_t   *v_attr;
  audio_attr_t   *a_attr;
  subp_attr_t    *s_attr;

  dvd_time_t     *dt;
  double          fps;
  long            hour, minute, second, ms, overall_time, cur_time;

    vmg_file = ifoOpen( dvd, 0 );
    if( !vmg_file ) {
	return -1;
    }

    tt_srpt = vmg_file->tt_srpt;
    
    
    // Make sure our title number is valid
    
    titleid = title-1;
   
    if( titleid < 0 || titleid >= tt_srpt->nr_of_srpts ) {
        fprintf( stderr, "Invalid title %d.\n", titleid + 1 );
        ifoClose( vmg_file );
        return -1;
    }

    vts_file = ifoOpen( dvd, tt_srpt->title[ titleid ].title_set_nr );
    if( !vts_file ) {
      fprintf( stderr, "Can't open the title %d info file.\n",
	       tt_srpt->title[ titleid ].title_set_nr );
      ifoClose( vmg_file );
      return -1;
    }
    
    
    if(vts_file->vtsi_mat) {
      v_attr = &vts_file->vtsi_mat->vts_video_attr;
      
      stats_video_attributes(v_attr, info);
      
      for(i = 0; i < vts_file->vtsi_mat->nr_of_vts_audio_streams; i++) {
	a_attr = &vts_file->vtsi_mat->vts_audio_attr[i];
	stats_audio_attributes(a_attr, i, info);
      }

      for(i = 0; i < vts_file->vtsi_mat->nr_of_vts_subp_streams; i++) {
	s_attr = &vts_file->vtsi_mat->vts_subp_attr[i];
	stats_subp_attributes(s_attr, i, info);
      }
      
    } else {
      fprintf(stderr, "(%s) failed to probe DVD title information\n", __FILE__);
      return -1;
    }
    
    vts_file=NULL;
    
    vts_file = ifoOpen( dvd, tt_srpt->title[ titleid ].title_set_nr );
    if( !vts_file ) {
      fprintf( stderr, "Can't open the title %d info file.\n",
	       tt_srpt->title[ titleid ].title_set_nr );
      ifoClose( vmg_file );
      return -1;
    }
    
    ttn = tt_srpt->title[ titleid ].vts_ttn;
    vts_ptt_srpt = vts_file->vts_ptt_srpt;
    pgc_id = vts_ptt_srpt->title[ ttn - 1 ].ptt[0].pgcn;
    cur_pgc = vts_file->vts_pgcit->pgci_srp[ pgc_id - 1 ].pgc;
    
    switch(((cur_pgc->playback_time).frame_u & 0xc0) >> 6) {
      
    case 1:
      info->fps=PAL_FPS;
      info->frc=3;
      info->magic = TC_MAGIC_DVD_PAL;
      break;
    case 3:
      info->fps = NTSC_FILM;
      info->frc = 1;
      info->magic = TC_MAGIC_DVD_NTSC;
      break;
    }

    fprintf(stderr, "(%s) DVD title %d/%d: %d chapter(s), %d angle(s), title set %d\n", __FILE__, title, tt_srpt->nr_of_srpts, tt_srpt->title[ titleid ].nr_of_ptts, tt_srpt->title[ titleid ].nr_of_angles, tt_srpt->title[ titleid].title_set_nr);
    
    fprintf(stderr, "(%s) title playback time: ", __FILE__);
    ifoPrint_time(&cur_pgc->playback_time); 
    fprintf(stderr, "  %ld sec\n", playtime);
    
    info->time=playtime;
   
    // stolen from ogmtools-1.0.2 dvdxchap -- tibit
    ttn = tt_srpt->title[titleid].vts_ttn;
    vts_ptt_srpt = vts_file->vts_ptt_srpt;
    overall_time = 0;

    for (i = 0; i < tt_srpt->title[titleid].nr_of_ptts - 1; i++) {
	pgc_id   = vts_ptt_srpt->title[ttn - 1].ptt[i].pgcn;
	pgn      = vts_ptt_srpt->title[ttn - 1].ptt[i].pgn;
	cur_pgc  = vts_file->vts_pgcit->pgci_srp[pgc_id - 1].pgc;

	start_cell = cur_pgc->program_map[pgn - 1] - 1;
	pgc_id     = vts_ptt_srpt->title[ttn - 1].ptt[i + 1].pgcn;
	pgn        = vts_ptt_srpt->title[ttn - 1].ptt[i + 1].pgn;
	cur_pgc    = vts_file->vts_pgcit->pgci_srp[pgc_id - 1].pgc;
	end_cell   = cur_pgc->program_map[pgn - 1] - 2;
	cur_time   = 0;
	for (j = start_cell; j <= end_cell; j++) {
	    dt = &cur_pgc->cell_playback[j].playback_time;
	    hour = ((dt->hour & 0xf0) >> 4) * 10 + (dt->hour & 0x0f);
	    minute = ((dt->minute & 0xf0) >> 4) * 10 + (dt->minute & 0x0f);
	    second = ((dt->second & 0xf0) >> 4) * 10 + (dt->second & 0x0f);
	    if (((dt->frame_u & 0xc0) >> 6) == 1)
		fps = 25.00;
	    else
		fps = 29.97;
	    dt->frame_u &= 0x3f;
	    dt->frame_u = ((dt->frame_u & 0xf0) >> 4) * 10 + (dt->frame_u & 0x0f);
	    ms = (double)dt->frame_u * 1000.0 / fps;
	    cur_time += (hour * 60 * 60 * 1000 + minute * 60 * 1000 + second * 1000 +
		    ms);
	}
	fprintf(stderr, "(%s) [Chapter %02d] %02ld:%02ld:%02ld.%03ld\n", __FILE__, i + 1,
		overall_time / 60 / 60 / 1000, (overall_time / 60 / 1000) % 60,
		(overall_time / 1000) % 60, overall_time % 1000);
	overall_time += cur_time;
    }
    fprintf(stderr, "(%s) [Chapter %02d] %02ld:%02ld:%02ld.%03ld\n", __FILE__, i + 1,
	    overall_time / 60 / 60 / 1000, (overall_time / 60 / 1000) % 60,
	    (overall_time / 1000) % 60, overall_time % 1000);

    return(0);
}

int dvd_verify(char *dvd_path)
{
    static dvd_reader_t *_dvd=NULL;
    ifo_handle_t *vmg_file=NULL;

    _dvd = DVDOpen(dvd_path);

    if(!_dvd) return(-1);

    vmg_file = ifoOpen( _dvd, 0 );
    if(!vmg_file) { 
      DVDClose(_dvd);
      return (-1);
    }

    DVDClose(_dvd);

    return(0);
}
 

int dvd_init(char *dvd_path, int *titles, int verb)
{

    tt_srpt_t *tt_srpt;
    ifo_handle_t *vmg_file;
    
    // copy verbosity flag
    verbose = verb;
    
    /**
     * Open the disc.
     */
    
    if(dvd==NULL) {
	dvd = DVDOpen(dvd_path);
	if(!dvd) return(-1);
    }
    
    //workspace

    if(data==NULL) {
	if((data = (unsigned char *) malloc(1024 * DVD_VIDEO_LB_LEN))==NULL) {
	    fprintf(stderr, "(%s) out of memory\n", __FILE__);
	    DVDClose( dvd );
	    return(-1);
	}
    }

    
    vmg_file = ifoOpen( dvd, 0 );
    if( !vmg_file ) {
      fprintf( stderr, "Can't open VMG info.\n" );
      DVDClose( dvd );
      free(data);
      return -1;
    }

    tt_srpt = vmg_file->tt_srpt;
    
    *titles = tt_srpt->nr_of_srpts;
    
    return(0);
}

int dvd_close()
{
    if(data!=NULL) {
	free(data);
	data=NULL;
    }

    if(dvd!=NULL) {
	DVDClose(dvd);
	dvd=NULL;
    }

    return(0);
}

int dvd_read(int arg_title, int arg_chapter, int arg_angle)
{
    int pgc_id, len, start_cell, cur_cell, last_cell, next_cell;
    unsigned int cur_pack;
    int ttn, pgn;

    dvd_file_t *title;
    ifo_handle_t *vmg_file;
    tt_srpt_t *tt_srpt;
    ifo_handle_t *vts_file;
    vts_ptt_srpt_t *vts_ptt_srpt;
    pgc_t *cur_pgc;
    int titleid, angle, chapid;

    chapid  = arg_chapter - 1;
    titleid = arg_title - 1;
    angle   = arg_angle - 1;


    /**
     * Load the video manager to find out the information about the titles on
     * this disc.
     */


    vmg_file = ifoOpen( dvd, 0 );
    if( !vmg_file ) {
	fprintf( stderr, "Can't open VMG info.\n" );
	return -1;
    }
    
    tt_srpt = vmg_file->tt_srpt;
    
    
    /**
     * Make sure our title number is valid.
     */
    if( titleid < 0 || titleid >= tt_srpt->nr_of_srpts ) {
        fprintf( stderr, "Invalid title %d.\n", titleid + 1 );
        ifoClose( vmg_file );
        return -1;
    }

    /**
     * Make sure the chapter number is valid for this title.
     */
    if( chapid < 0 || chapid >= tt_srpt->title[ titleid ].nr_of_ptts ) {
        fprintf( stderr, "Invalid chapter %d\n", chapid + 1 );
        ifoClose( vmg_file );
        return -1;
    }


    /**
     * Make sure the angle number is valid for this title.
     */
    if( angle < 0 || angle >= tt_srpt->title[ titleid ].nr_of_angles ) {
        fprintf( stderr, "Invalid angle %d\n", angle + 1 );
        ifoClose( vmg_file );
        return -1;
    }

    /**
     * Load the VTS information for the title set our title is in.
     */
    vts_file = ifoOpen( dvd, tt_srpt->title[ titleid ].title_set_nr );
    if( !vts_file ) {
        fprintf( stderr, "Can't open the title %d info file.\n",
                 tt_srpt->title[ titleid ].title_set_nr );
        ifoClose( vmg_file );
        return -1;
    }


    /**
     * Determine which program chain we want to watch.  This is based on the
     * chapter number.
     */

    ttn = tt_srpt->title[ titleid ].vts_ttn;
    vts_ptt_srpt = vts_file->vts_ptt_srpt;
    pgc_id = vts_ptt_srpt->title[ ttn - 1 ].ptt[ chapid ].pgcn;
    pgn = vts_ptt_srpt->title[ ttn - 1 ].ptt[ chapid ].pgn;
    cur_pgc = vts_file->vts_pgcit->pgci_srp[ pgc_id - 1 ].pgc;
    start_cell = cur_pgc->program_map[ pgn - 1 ] - 1;

    
    //ThOe

    if (chapid+1 == tt_srpt->title[ titleid ].nr_of_ptts) {
      last_cell = cur_pgc->nr_of_cells;
    } else {
      
      last_cell = cur_pgc->program_map[ (vts_ptt_srpt->title[ ttn - 1 ].ptt[ chapid+1 ].pgn) - 1 ] - 1;
    }
    
    /**
     * We've got enough info, time to open the title set data.
     */
    
    title = DVDOpenFile( dvd, tt_srpt->title[ titleid ].title_set_nr,
                         DVD_READ_TITLE_VOBS);
    
    if( !title ) {
        fprintf( stderr, "Can't open title VOBS (VTS_%02d_1.VOB).\n",
                 tt_srpt->title[ titleid ].title_set_nr );
        ifoClose( vts_file );
        ifoClose( vmg_file );
        return -1;
    }

    /**
     * Playback the cells for our chapter.
     */
    
    next_cell = start_cell;
    
    for( cur_cell = start_cell; next_cell < last_cell; ) {
      
        cur_cell = next_cell;

        /* Check if we're entering an angle block. */
        if( cur_pgc->cell_playback[ cur_cell ].block_type
                                        == BLOCK_TYPE_ANGLE_BLOCK ) {
            int i;

            cur_cell += angle;
            for( i = 0;; ++i ) {
                if( cur_pgc->cell_playback[ cur_cell + i ].block_mode
                                          == BLOCK_MODE_LAST_CELL ) {
                    next_cell = cur_cell + i + 1;
                    break;
                }
            }
        } else {
	  next_cell = cur_cell + 1;
        }
	
	/**
	 * We loop until we're out of this cell.
	 */
	
	for( cur_pack = cur_pgc->cell_playback[ cur_cell ].first_sector;
	     cur_pack < cur_pgc->cell_playback[ cur_cell ].last_sector; ) {
	  
	  dsi_t dsi_pack;
	  unsigned int next_vobu, next_ilvu_start, cur_output_size;
	  
	  
	  /**
	   * Read NAV packet.
	   */

	     nav_retry:	  
	  
	  len = DVDReadBlocks( title, (int) cur_pack, 1, data );
	  if( len != 1 ) {
	    fprintf( stderr, "Read failed for block %d\n", cur_pack );
	    ifoClose( vts_file );
	    ifoClose( vmg_file );
	    DVDCloseFile( title );
	    return -1;
	  }

	  //assert( is_nav_pack( data ) );
	  if(!is_nav_pack(data)) {
	    cur_pack++;
	    goto nav_retry;
	  }

	  /**
	   * Parse the contained dsi packet.
	   */
	  navRead_DSI( &dsi_pack, &(data[ DSI_START_BYTE ]));

	  if(!( cur_pack == dsi_pack.dsi_gi.nv_pck_lbn)) {
	    cur_output_size = 0;
	    dsi_pack.vobu_sri.next_vobu = SRI_END_OF_CELL;
	  }
	  
	  
	  /**
	   * Determine where we go next.  These values are the ones we mostly
	   * care about.
	   */
	  next_ilvu_start = cur_pack
	    + dsi_pack.sml_agli.data[ angle ].address;
	  cur_output_size = dsi_pack.dsi_gi.vobu_ea;
	  
	  
	  /**
	   * If we're not at the end of this cell, we can determine the next
	   * VOBU to display using the VOBU_SRI information section of the
	   * DSI.  Using this value correctly follows the current angle,
	   * avoiding the doubled scenes in The Matrix, and makes our life
	   * really happy.
	   *
	   * Otherwise, we set our next address past the end of this cell to
	   * force the code above to go to the next cell in the program.
	   */
	  if( dsi_pack.vobu_sri.next_vobu != SRI_END_OF_CELL ) {
	    next_vobu = cur_pack
	      + ( dsi_pack.vobu_sri.next_vobu & 0x7fffffff );
	  } else {
	    next_vobu = cur_pack + cur_output_size + 1;
	  }
	  
	  assert( cur_output_size < 1024 );
	  cur_pack++;
	  
	  /**
	   * Read in and output cursize packs.
	   */
	  len = DVDReadBlocks( title, (int) cur_pack, cur_output_size, data );
	  if( len != (int) cur_output_size ) {
	    fprintf( stderr, "Read failed for %d blocks at %d\n",
		     cur_output_size, cur_pack );
	    ifoClose( vts_file );
	    ifoClose( vmg_file );
	    DVDCloseFile( title );
	    return -1;
	  }
	  
	  fwrite( data, cur_output_size, DVD_VIDEO_LB_LEN, stdout );
	  
	  if(verbose & TC_STATS) fprintf( stderr,"%d %d\n", cur_pack, cur_output_size);
	  cur_pack = next_vobu;
	}
    }    
    ifoClose( vts_file );
    ifoClose( vmg_file );
    DVDCloseFile( title );
    
    return 0;
}

static long startsec;
static long startusec;

static void rip_counter_init(long int *t1, long int *t2)
{
  struct timeval tv;
  struct timezone tz={0,0};
  
  gettimeofday(&tv,&tz);
  
  *t1=tv.tv_sec;
  *t2=tv.tv_usec;
  
}

static void rip_counter_close()
{
    fprintf(stderr,"\n");
}

static long range_a = -1, range_b = -1;
static long range_starttime = -1;

static void rip_counter_set_range(long from, long to)
{
  range_a = from;
  range_b = to-1;
}

static void counter_print(long int pida, long int pidn, long int t1, long int t2)
{
  struct timeval tv;
  struct timezone tz={0,0};

  double fps;

  if(gettimeofday(&tv,&tz)<0) return;

  fps=(pidn-pida)/((tv.tv_sec+tv.tv_usec/1000000.0)-(t1+t2/1000000.0));

  fps = (2048 * fps) / (1<<20); 
  
  if(fps>0) {
      if(range_b != -1 && pidn>=range_a) {
          double done;
          long secleft;
          
          if(range_starttime == -1) range_starttime = tv.tv_sec;
          done = (double)(pidn-range_a)/(range_b-range_a);
          secleft = (long)((1-done)*(double)(tv.tv_sec-range_starttime)/done);
	  
	  fprintf(stderr, "extracting blocks [%08ld], %4.1f MB/s, %4.1f%%, ETA: %ld:%02ld:%02ld   \r", pidn-pida, fps, 100*done,
		 secleft/3600, (secleft/60) % 60, secleft % 60);
      }
  }
}

int dvd_stream(int arg_title)
{
    int pgc_id, len, start_cell;
    unsigned long cur_pack=0, max_sectors=0, blocks_left=0, blocks_written=0, first_block=0;
    int ttn, pgn;

    dvd_file_t *title;
    ifo_handle_t *vmg_file;
    tt_srpt_t *tt_srpt;
    ifo_handle_t *vts_file;
    vts_ptt_srpt_t *vts_ptt_srpt;
    pgc_t *cur_pgc;
    int titleid, angle, chapid;
    unsigned int cur_output_size=1024, blocks=0;

    chapid  = 0;
    titleid = arg_title - 1;
    angle   = 0;


    /**
     * Load the video manager to find out the information about the titles on
     * this disc.
     */


    vmg_file = ifoOpen( dvd, 0 );
    if( !vmg_file ) {
	fprintf( stderr, "Can't open VMG info.\n" );
	return -1;
    }
    
    tt_srpt = vmg_file->tt_srpt;
    
    
    /**
     * Make sure our title number is valid.
     */
    if( titleid < 0 || titleid >= tt_srpt->nr_of_srpts ) {
        fprintf( stderr, "Invalid title %d.\n", titleid + 1 );
        ifoClose( vmg_file );
        return -1;
    }

    /**
     * Make sure the chapter number is valid for this title.
     */
    if( chapid < 0 || chapid >= tt_srpt->title[ titleid ].nr_of_ptts ) {
        fprintf( stderr, "Invalid chapter %d\n", chapid + 1 );
        ifoClose( vmg_file );
        return -1;
    }


    /**
     * Make sure the angle number is valid for this title.
     */
    if( angle < 0 || angle >= tt_srpt->title[ titleid ].nr_of_angles ) {
        fprintf( stderr, "Invalid angle %d\n", angle + 1 );
        ifoClose( vmg_file );
        return -1;
    }

    /**
     * Load the VTS information for the title set our title is in.
     */
    vts_file = ifoOpen( dvd, tt_srpt->title[ titleid ].title_set_nr );
    if( !vts_file ) {
        fprintf( stderr, "Can't open the title %d info file.\n",
                 tt_srpt->title[ titleid ].title_set_nr );
        ifoClose( vmg_file );
        return -1;
    }


    /**
     * Determine which program chain we want to watch.  This is based on the
     * chapter number.
     */

    ttn = tt_srpt->title[ titleid ].vts_ttn;
    vts_ptt_srpt = vts_file->vts_ptt_srpt;
    pgc_id = vts_ptt_srpt->title[ ttn - 1 ].ptt[ chapid ].pgcn;
    pgn = vts_ptt_srpt->title[ ttn - 1 ].ptt[ chapid ].pgn;
    cur_pgc = vts_file->vts_pgcit->pgci_srp[ pgc_id - 1 ].pgc;
    start_cell = cur_pgc->program_map[ pgn - 1 ] - 1;

    
    /**
     * We've got enough info, time to open the title set data.
     */
    
    title = DVDOpenFile( dvd, tt_srpt->title[ titleid ].title_set_nr,
                         DVD_READ_TITLE_VOBS);
    
    if( !title ) {
        fprintf( stderr, "Can't open title VOBS (VTS_%02d_1.VOB).\n",
                 tt_srpt->title[ titleid ].title_set_nr );
        ifoClose( vts_file );
        ifoClose( vmg_file );
        return -1;
    }

    /**
     * Playback the cells for our title
     */

    cur_pack = cur_pgc->cell_playback[start_cell].first_sector;

    max_sectors = (long) cur_pgc->cell_playback[ cur_pgc->nr_of_cells -1].last_sector;

    first_block = cur_pack;

    fprintf(stderr,"(%s) title %02d, %ld blocks (%ld-%ld)\n", __FILE__, tt_srpt->title[ titleid ].title_set_nr, (long) DVDFileSize(title), (long) cur_pack, (long) max_sectors);
    
    if((long) DVDFileSize(title) <  max_sectors ||  cur_pack < 0)
      fprintf(stderr, "(%s) internal error\n", __FILE__);
    
    //sanity check
    if(max_sectors <= cur_pack) max_sectors = (long) DVDFileSize(title);

    /**
     * Read NAV packet.
     */

    len = DVDReadBlocks( title, (int) cur_pack, 1, data );
    
    if( len != 1 ) {
      fprintf( stderr, "Read failed for block %ld\n", cur_pack );
      ifoClose( vts_file );
      ifoClose( vmg_file );
      DVDCloseFile( title );
      return -1;
    }
    
    if(data[38]==0 && data[39]==0 && data[40]==1 && data[41]==0xBF &&
       data[1024]==0 && data[1025]==0 && data[1026]==1 && data[1027]==0xBF) {
      
      fprintf( stderr, "(%s) navigation packet at offset %d\n", __FILE__,  (int) cur_pack);
    }
    
    // loop until all packs of title are written
    
    blocks_left = max_sectors+1;
    rip_counter_set_range(cur_pack, blocks_left);
    rip_counter_init(&startsec, &startusec);

    while(blocks_left > 0) {
      
      blocks = (blocks_left>cur_output_size) ? cur_output_size:blocks_left;
      
      len = DVDReadBlocks( title, (int) cur_pack, blocks, data );
      if( len != (int) blocks) {

	  rip_counter_close();
	  
	  if(len>=0) {
	      if(len>0)fwrite(data, len, DVD_VIDEO_LB_LEN, stdout);
	      fprintf( stderr, "%ld blocks written\n", blocks_written+len);
	  }
	
	ifoClose( vts_file );
	ifoClose( vmg_file );
	DVDCloseFile( title );
	return -1;
      } 
      
      fwrite(data, blocks, DVD_VIDEO_LB_LEN, stdout);
      blocks_written += blocks;

      counter_print(first_block, blocks_written, startsec, startusec); 
	  
      cur_pack += blocks;
      blocks_left -= blocks;
      

      if(verbose & TC_STATS) fprintf(stderr,"%ld %d\n", cur_pack, cur_output_size);
    } 
    rip_counter_close();

    fprintf(stderr, "(%s) %ld blocks written\n", __FILE__, blocks_written);
    
    ifoClose( vts_file );
    ifoClose( vmg_file );
    DVDCloseFile( title );
    
    return 0;
}


#else
int dvd_query(int arg_title, int *arg_chapter, int *arg_angle)
{
  fprintf(stderr, "(%s) no support for DVD reading configured - exit.\n", __FILE__);
  
  return(-1);
}

int dvd_init(char *dvd_path, int *arg_title, int verb) 
{
  fprintf(stderr, "(%s) no support for DVD reading configured - exit.\n", __FILE__);
  
  return(-1);
}

int dvd_read(int arg_title, int arg_chapter, int arg_angle) 
{
 
  fprintf(stderr, "(%s) no support for DVD reading configured - exit.\n", __FILE__);

return(-1);
}

int dvd_stream(int arg_title)
{
 
  fprintf(stderr, "(%s) no support for DVD reading configured - exit.\n", __FILE__);

return(-1);
}

int dvd_close() 
{
 
  fprintf(stderr, "(%s) no support for DVD reading configured - exit.\n", __FILE__);

return(-1);
}
int dvd_verify(char *name) 
{
 
  fprintf(stderr, "(%s) no support for DVD reading configured - exit.\n", __FILE__);

return(-1);
}
int dvd_probe(int title, probe_info_t *info)
{
  fprintf(stderr, "(%s) no support for DVD reading configured - exit.\n", __FILE__);

return(-1);
}
#endif
