/*
 *  probe_xml.c
 *
 *  Copyright (C) Marzio Malanchini - Febrary 2002
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

#include "ioaux.h"
#include "tc.h"

#ifndef	HAVE_LIBXML2

void probe_xml(info_t *ipipe)
{
        fprintf(stderr, "(%s) no support for XML compiled - exit.\n", __FILE__);
        ipipe->error=1;
        return;
}

#else


#include "ioxml.h"

#define MAX_BUF 1024

extern int bitrate;
extern int binary_dump;

audiovideo_t s_audiovideo;



void probe_xml(info_t *ipipe)
{
	audiovideo_t	*p_audio_video;
	char	s_probe_cmd_buf[MAX_BUF+1];
	FILE	*p_fd;
	long	s_tot_frames_audio;
	long	s_tot_frames_video;
	probe_info_t	s_first_audio,s_other_audio;
	probe_info_t	s_first_video,s_other_video;
	int	s_first_element=0;

	s_tot_frames_audio=s_tot_frames_video=0;
	ipipe->error=f_manage_input_xml(ipipe->name,1,&s_audiovideo);
	

	if (ipipe->error==1)
	{
		(int)f_manage_input_xml(ipipe->name,0,&s_audiovideo);
		return;
	}
	for (p_audio_video=s_audiovideo.p_next;p_audio_video!=NULL;p_audio_video=p_audio_video->p_next)
	{
		if (p_audio_video->p_nome_video != NULL)
		{
			if((snprintf(s_probe_cmd_buf, MAX_BUF, "tcprobe -i %s -b %d -d %d -T %d -H %d ",p_audio_video->p_nome_video,bitrate,ipipe->verbose,ipipe->dvd_title,ipipe->factor)<0))
			{
	                	fprintf(stderr,"Buffer overflow\n");
				ipipe->error=1;
				break;
			}
			if (binary_dump)
			{
				if((snprintf(s_probe_cmd_buf+strlen(s_probe_cmd_buf), MAX_BUF-strlen(s_probe_cmd_buf), "-B ")<0))
				{
	                		fprintf(stderr,"Buffer overflow\n");
					ipipe->error=1;
					break;
				}
				if((p_fd = popen(s_probe_cmd_buf, "r"))== NULL)
				{
	                		fprintf(stderr,"Cannot open pipe\n");
					ipipe->error=1;
					break;
				}
				if (fread(&s_other_video, sizeof(probe_info_t), 1, p_fd) !=1)
				{
	                		fprintf(stderr,"Cannot read pipe\n");
					ipipe->error=1;
					break;
				}
				pclose(p_fd);
				if(s_other_video.magic == TC_MAGIC_UNKNOWN || s_other_video.magic == TC_MAGIC_PIPE || s_other_video.magic == TC_MAGIC_ERROR)
				{
					fprintf(stderr,"\n\nerror: this version of transcode supports only\n");
					fprintf(stderr,"xml file who containing files of identical file type.\n");
					fprintf(stderr,"Please clean up the %s file and restart.\n", ipipe->name);
					fprintf(stderr,"file %s with filetype %s is invalid for this operation mode.\n", p_audio_video->p_nome_video, filetype(s_other_video.magic));
					ipipe->error=1;
				}
				if ((s_first_element & 0x02) == 0)
				{
					s_first_element|=0x02;
					memcpy(&s_first_video,&s_other_video,sizeof(probe_info_t));
				}
				else
				{
					if(s_other_video.magic != s_first_video.magic)
					{
	                                	fprintf(stderr,"\nerror: multiple filetypes not valid for XML mode.\n");
						ipipe->error=1;
					}
				}
				if (p_audio_video->s_start_video > p_audio_video->s_end_video)
				{
					fprintf(stderr,"\n\nerror: start frame is greater than end frame in file %s\n",p_audio_video->p_nome_video); 
					ipipe->error=1;
				}
	                        s_tot_frames_video+=(p_audio_video->s_end_video - p_audio_video->s_start_video);       //selected frames
			}
			else
				system(s_probe_cmd_buf);
		}
		if (p_audio_video->p_nome_audio != NULL)
		{
			if((snprintf(s_probe_cmd_buf, MAX_BUF, "tcprobe -i %s -b %d -d %d -T %d -H %d ",p_audio_video->p_nome_audio,bitrate,ipipe->verbose,ipipe->dvd_title,ipipe->factor)<0))
			{
	                	fprintf(stderr,"Buffer overflow\n");
				ipipe->error=1;
				break;
			}
			if (binary_dump)
			{
				if((snprintf(s_probe_cmd_buf+strlen(s_probe_cmd_buf), MAX_BUF-strlen(s_probe_cmd_buf), "-B ")<0))
				{
	                		fprintf(stderr,"Buffer overflow\n");
					ipipe->error=1;
					break;
				}
				if((p_fd = popen(s_probe_cmd_buf, "r"))== NULL)
				{
	                		fprintf(stderr,"Cannot open pipe\n");
					ipipe->error=1;
					break;
				}
				if (fread(&s_other_audio, sizeof(probe_info_t), 1, p_fd) !=1)
				{
	                		fprintf(stderr,"Cannot read pipe\n");
					ipipe->error=1;
					break;
				}
				pclose(p_fd);
				if(s_other_audio.magic == TC_MAGIC_UNKNOWN || s_other_audio.magic == TC_MAGIC_PIPE || s_other_audio.magic == TC_MAGIC_ERROR)
				{
					fprintf(stderr,"\n\nerror: this version of transcode supports only\n");
					fprintf(stderr,"xml file who containing files of identical file type.\n");
					fprintf(stderr,"Please clean up the %s file and restart.\n", ipipe->name);
					fprintf(stderr,"file %s with filetype %s is invalid for this operation mode.\n", p_audio_video->p_nome_audio, filetype(s_other_audio.magic));
					ipipe->error=1;
				}
				if ((s_first_element & 0x01) == 0)
				{
					s_first_element|=0x01;
					memcpy(&s_first_audio,&s_other_audio,sizeof(probe_info_t));
				}
				else
				{
					if(s_other_audio.magic != s_first_audio.magic)
					{
	                                	fprintf(stderr,"\nerror: multiple filetypes not valid for XML mode.\n");
						ipipe->error=1;
					}
				}
				if (p_audio_video->s_start_audio > p_audio_video->s_end_audio)
				{
					fprintf(stderr,"\n\nerror: start frame is greater than end frame in file %s\n",p_audio_video->p_nome_video); 
					ipipe->error=1;
				}
	                        s_tot_frames_audio+=(p_audio_video->s_end_audio - p_audio_video->s_start_audio);       //selected frames
			}
		}
	}
	(int)f_manage_input_xml(NULL,0,&s_audiovideo);
	if (s_first_element & 0x03)	//have video and audio tracks
	{
		memcpy(ipipe->probe_info,&s_other_video,sizeof(probe_info_t)); //setup the probe_info structure
		ipipe->probe_info->frames=s_tot_frames_video;		//Force sum of selected frames
		ipipe->probe_info->num_tracks=s_first_audio.num_tracks;
		memcpy(ipipe->probe_info->track,&(s_first_audio.track),TC_MAX_AUD_TRACKS*sizeof(pcm_t));
	}
	else if (s_first_element & 0x02)     //have only video track
	{
		memcpy(ipipe->probe_info,&s_other_video,sizeof(probe_info_t)); //setup the probe_info structure
		ipipe->probe_info->frames=s_tot_frames_video;		//Force sum of selected frames
	}
	else if (s_first_element & 0x01)     //have only audio tracks
	{
		memcpy(ipipe->probe_info,&s_other_audio,sizeof(probe_info_t)); //setup the probe_info structure
		ipipe->probe_info->frames=s_tot_frames_audio;		//Force sum of selected frames
	}
	s_first_element=0;
}
#endif
