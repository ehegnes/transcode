/*************************************************************************
*  MPEG SYSTEMS MULTIPLEXER                                              *
*  Erzeugen einer MPEG/SYSTEMS                           		 *
*  MULTIPLEXED VIDEO/AUDIO DATEI					 *
*  aus zwei MPEG Basis Streams						 *
*  Christoph Moar							 *
*  SIEMENS ZFE ST SN 11 / T SN 6					 *
*  (C) 1994 1995    							 *
**************************************************************************
*  Generating a MPEG/SYSTEMS						 *
*  MULTIPLEXED VIDEO/AUDIO STREAM					 *
*  from two MPEG source streams						 *
*  Christoph Moar							 *
*  SIEMENS CORPORATE RESEARCH AND DEVELOPMENT ST SN 11 / T SN 6		 *
*  (C) 1994 1995							 *
**************************************************************************
*  Einschraenkungen vorhanden. Unterstuetzt nicht gesamten MPEG/SYSTEMS  *
*  Standard. Haelt sich i.d.R. an den CSPF-Werten, zusaetzlich (noch)    *
*  nur fuer ein Audio- und/oder ein Video- Stream. Evtl. erweiterbar.    *
**************************************************************************
*  Restrictions apply. Will not support the whole MPEG/SYSTEM Standard.  *
*  Basically, will generate Constrained System Parameter Files.		 *
*  Mixes only one audio and/or one video stream. Might be expanded.	 *
*************************************************************************/

/*************************************************************************
*  mplex - MPEG/SYSTEMS multiplexer					 *
*  Copyright (C) 1994 1995 Christoph Moar				 *
*  Siemens ZFE ST SN 11 / T SN 6					 *
*									 *
*  moar@informatik.tu-muenchen.de 					 *
*       (Christoph Moar)			 			 *
*  klee@heaven.zfe.siemens.de						 *
*       (Christian Kleegrewe, Siemens only requests)			 *
*									 *
*  This program is free software; you can redistribute it and/or modify	 *
*  it under the terms of the GNU General Public License as published by	 *	
*  the Free Software Foundation; either version 2 of the License, or	 *
*  (at your option) any later version.					 *
*									 *
*  This program is distributed in the hope that it will be useful,	 *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of	 *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the	 *
*  GNU General Public License for more details.				 *
*									 *
*  You should have received a copy of the GNU General Public License	 *
*  along with this program; if not, write to the Free Software		 *
*  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		 *
*************************************************************************/

/*************************************************************************
*  Notwendige Systemmittel:						 *
*  Festplattenspeicher fuer Quell- und Zielstreams, pro Minute 		 *
*  MPEG/SYSTEMS Stream noch zusaetzlich 50-100 kByte tmp Plattenspeicher *
**************************************************************************
*  Necessary resources:							 *
*  Hard Disk space for source and destination streams, per Minute	 *
*  MPEG/SYSTEM stream an additional 50-100 kByte tmp Diskspace		 *
*************************************************************************/

#define MPEGMAIN 1
#include "main.h"
#include "consts.h"


//======================
//== Display-Routines ==
//======================
static int need_new_line = 0;

void DisplayError(char *txt)
{
  fprintf(stderr, "\nERROR: %s", txt);
  need_new_line = 1;
}
void DisplayWarning(char *txt)
{
  fprintf(stderr, "\nWARNING: %s", txt);
  need_new_line = 1;
}
void DisplayInfo(char *txt)
{
  fprintf(stderr, "\nINFO: %s", txt);
  need_new_line = 1;
}

void DisplayProgress(char *txt, int percent)
{
  if (need_new_line) 
  {  
    fprintf(stderr, "\n"); 
    need_new_line = 0;
  }   
  fprintf(stderr, "*** %s (%d)%%\r", txt, percent);
}

//===============
//== domplex() ==
//===============

static char temp_filename_v[]  = "./tmp_vXXXXXX";
static char temp_filename_a[]  = "./tmp_aXXXXXX";
static char temp_filename_a1[] = "./tmp_a1XXXXXX";

int domplex(int mplex_video, int mplex_audio)
{
  char	*video_units = NULL;
  char	*audio_units = NULL;
  char  *audio1_units = NULL;

  Video_struc video_info;
  Audio_struc audio_info;
  Audio_struc audio1_info;
  unsigned int audio_total, audio1_total, video_total;
  off_t audio_bytes, audio1_bytes, video_bytes;
  unsigned int which_streams=0;
  double startup_delay=0;

  if (mplex_video)
    which_streams |= STREAMS_VIDEO;
  if (mplex_audio && strlen(AudioFilename))
    which_streams |= STREAMS_AUDIO;
  if (mplex_audio && (mplex_type != MPEG_VCD) && strlen(Audio1Filename))
    which_streams |= STREAMS_AUDIO1;

  if (!check_files(&audio_bytes, &audio1_bytes, &video_bytes, which_streams))
    return FALSE;

  empty_video_struc(&video_info);
  empty_audio_struc(&audio_info);
  empty_audio_struc(&audio1_info);

  if (which_streams & STREAMS_VIDEO)
  {
      mkstemp(temp_filename_v);    
      video_units = temp_filename_v;
      
      if (!get_info_video (VideoFilename, video_units, &video_info, &startup_delay, &video_total, video_bytes))
	  return FALSE;
      //DisplayProgress("", 0);
  }
  else
      video_total = 0;
  
  if (which_streams & STREAMS_AUDIO)
  {
      mkstemp(temp_filename_a);
      audio_units = temp_filename_a;
      if (!get_info_audio (AudioFilename, audio_units, &audio_info, &startup_delay, &audio_total, audio_bytes))
	  return FALSE;
      //DisplayProgress("", 0);
  }
  else
      audio_total = 0;
  
  if (which_streams & STREAMS_AUDIO1)
  {
      mkstemp(temp_filename_a1);
      audio1_units = temp_filename_a1;
      if (!get_info_audio (Audio1Filename, audio1_units, &audio1_info, &startup_delay, &audio1_total, audio1_bytes))
	  return FALSE;
      DisplayProgress("", 0);
  }
  else
      audio1_total = 0;
  
  //-- GMO: erster Prameter ist file pointer fuer outfile ! --
  return outputstream (NULL, video_units, &video_info, 
		       audio_units, &audio_info, 
		       audio1_units, &audio1_info, 
		       video_total, audio_total, audio1_total, which_streams);
}

