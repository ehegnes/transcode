/*
 *  probe_xml.c
 *
 *  Copyright (C) Marzio Malanchini - March 2003
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

#include "ioaux.h"
#include "tc.h"

#include <sys/types.h>

#ifdef HAVE_LIBXML2

#include "ioxml.h"

#define MAX_BUF 1024

extern int binary_dump;


int f_check_video_H_W(audiovideo_t *p_audio_video)
{
	audiovideo_t *p_temp;
	int s_rc=0,s_video_tg_height=0,s_video_tg_width=0,s_v_height=0,s_v_width=0;

	for (p_temp=p_audio_video;p_temp != NULL;p_temp=p_temp->p_next)
	{
		if (s_v_height == 0)
			s_v_height=p_temp->s_v_height;	
		else if (s_v_height != p_temp->s_v_height)
			s_rc|=0x01;
		if (s_v_width == 0)
			s_v_width=p_temp->s_v_width;	
		else if (s_v_width != p_temp->s_v_width)
			s_rc|=0x02;
		if ((p_temp->s_v_tg_height != 0) && (s_video_tg_height==0))
			s_video_tg_height=p_temp->s_v_tg_height;
		else if (p_temp->s_v_tg_height != 0)
		{
			if (p_temp->s_v_tg_height != s_video_tg_height==0)
			{
				fprintf(stderr,"(%s) Warning: setting target height to %d (the target must be the same for all statements)\n",__FILE__,s_video_tg_height);
				p_temp->s_v_tg_height=s_video_tg_height;
			}
		}
		if ((p_temp->s_v_tg_width != 0) && (s_video_tg_width==0))
			s_video_tg_width=p_temp->s_v_tg_width;
		else if (p_temp->s_v_tg_width != 0)
		{
			if (p_temp->s_v_tg_width != s_video_tg_width==0)
			{
				fprintf(stderr,"(%s) Warning: setting target width to %d (the target must be the same for all statements)\n",__FILE__,s_video_tg_width);
				p_temp->s_v_tg_width=s_video_tg_width;
			}
		}
	}
	if (s_rc !=0)
	{
		if ((s_rc == 0x03) && (s_video_tg_height == 0) && (s_video_tg_width == 0))
		{
			fprintf(stderr,"(%s) error: the height and the width of the video tracks are different. Please specify target-width and target-height if you want to process the xml file\n",__FILE__);
			return(1);
		}
		else if ((s_rc == 0x01) && (s_video_tg_height == 0))
		{
			fprintf(stderr,"(%s) error: the height of the video tracks are different. Please specify target-height if you want to process the xml file\n",__FILE__);
			return(1);
		}
		else if ((s_rc == 0x02) && (s_video_tg_width == 0))
		{
			fprintf(stderr,"(%s) error: the width of the video tracks are different. Please specify target-height if you want to process the xml file\n",__FILE__);
			return(1);
		}
	}
	for (p_temp=p_audio_video;p_temp != NULL;p_temp=p_temp->p_next) //initialize all unset codec
	{
		if (s_video_tg_height!=0)
			p_temp->s_v_tg_height=s_video_tg_height;	
		if (s_video_tg_width!=0)
			p_temp->s_v_tg_width=s_video_tg_width;	
	}
	return(0);
}

void f_det_totale_video_frame(audiovideo_t *p_audio_video)
{
	if ((p_audio_video->s_video_smpte==smpte)||(p_audio_video->s_video_smpte==smpte25))
		p_audio_video->s_fps=25.00;
	else if (p_audio_video->s_video_smpte==smpte30drop)
		p_audio_video->s_fps=29.97;
	p_audio_video->s_start_video+=(long)p_audio_video->s_start_v_time * p_audio_video->s_fps;
	p_audio_video->s_end_video+=(long)p_audio_video->s_end_v_time * p_audio_video->s_fps;
}

void f_det_totale_audio_frame(audiovideo_t *p_audio_video)
{
	if ((p_audio_video->s_audio_smpte==smpte)||(p_audio_video->s_audio_smpte==smpte25))
		p_audio_video->s_fps=25.00;
	else if (p_audio_video->s_audio_smpte==smpte30drop)
		p_audio_video->s_fps=29.97;
	p_audio_video->s_start_audio+=(long)p_audio_video->s_start_a_time * p_audio_video->s_fps;
	p_audio_video->s_end_audio+=(long)p_audio_video->s_end_a_time * p_audio_video->s_fps;
}


int f_build_xml_tree(info_t *ipipe,audiovideo_t *p_audiovideo,probe_info_t *p_first_audio,probe_info_t *p_first_video,long *s_tot_frames_audio, long *s_tot_frames_video)
{
	audiovideo_t	*p_audio_video;
	char	s_probe_cmd_buf[MAX_BUF+1];
	FILE	*p_fd;
	probe_info_t	s_other_audio;
	probe_info_t	s_other_video;
	int	s_first_element=0;
	pid_t   tc_probe_pid;

	*s_tot_frames_audio=*s_tot_frames_video=0;
	ipipe->error=f_manage_input_xml(ipipe->name,1,p_audiovideo);
	
	if (ipipe->error==1)
	{
		(int)f_manage_input_xml(ipipe->name,0,p_audiovideo);
		return(-1);
	}
	for (p_audio_video=p_audiovideo->p_next;p_audio_video!=NULL;p_audio_video=p_audio_video->p_next)
	{
		if (p_audio_video->p_nome_video != NULL)
		{
			if(tc_snprintf(s_probe_cmd_buf, MAX_BUF, "tcprobe -i %s -d %d ",p_audio_video->p_nome_video,ipipe->verbose) < 0)
			{
	                	fprintf(stderr,"Buffer overflow\n");
				ipipe->error=1;
				break;
			}
			if (binary_dump)
			{
				if(tc_snprintf(s_probe_cmd_buf+strlen(s_probe_cmd_buf), MAX_BUF-strlen(s_probe_cmd_buf), "-B ") < 0)
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

                                if (fread(&tc_probe_pid, sizeof(pid_t), 1, p_fd) !=1)
                                {
                                        fprintf(stderr,"Cannot read pipe\n");
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
				p_audio_video->s_v_real_codec=s_other_video.codec;
				p_audio_video->s_v_width=s_other_video.width;
				p_audio_video->s_v_height=s_other_video.height;
				p_audio_video->s_a_real_codec=s_other_video.track[0].format;
				p_audio_video->s_a_rate=s_other_video.track[0].samplerate;
				p_audio_video->s_a_bits=s_other_video.track[0].bits;
				p_audio_video->s_a_chan=s_other_video.track[0].chan;
				p_audio_video->s_fps=s_other_video.fps;
				if(s_other_video.magic == TC_MAGIC_UNKNOWN || s_other_video.magic == TC_MAGIC_PIPE || s_other_video.magic == TC_MAGIC_ERROR)
				{
					fprintf(stderr,"\n\nerror: this version of transcode supports only\n");
					fprintf(stderr,"xml file who containing dv avi or mov file type.\n");
					fprintf(stderr,"Please clean up the %s file and restart.\n", ipipe->name);
					fprintf(stderr,"file %s with filetype %s is invalid for this operation mode.\n", p_audio_video->p_nome_video, filetype(s_other_video.magic));
					ipipe->error=1;
				}
				if (p_audio_video->s_v_magic == TC_MAGIC_UNKNOWN)	//forced by ioxml.c
				{
					if (s_other_video.magic == TC_MAGIC_AVI)	//if the magic is AVI and the codec is DV -> i need to use DV
						if (s_other_video.codec == TC_CODEC_DV)
							p_audio_video->s_v_magic=TC_MAGIC_DV_PAL;
						else
							p_audio_video->s_v_magic=s_other_video.magic;
					else
						p_audio_video->s_v_magic=s_other_video.magic;
				}
				if ((s_first_element & 0x02) == 0)
				{
					s_first_element|=0x02;
					ac_memcpy(p_first_video,&s_other_video,sizeof(probe_info_t));
				}
				f_det_totale_video_frame(p_audio_video);
				if (p_audio_video->s_start_video > p_audio_video->s_end_video)
				{
					fprintf(stderr,"\n\nerror: start frame is greater than end frame in file %s\n",p_audio_video->p_nome_video); 
					ipipe->error=1;
				}
	                        *s_tot_frames_video+=(p_audio_video->s_end_video - p_audio_video->s_start_video);       //selected frames
			}
			else
				system(s_probe_cmd_buf);
		}
		if (p_audio_video->p_nome_audio != NULL)
		{
			if(tc_snprintf(s_probe_cmd_buf, MAX_BUF, "tcprobe -i %s -d %d ",p_audio_video->p_nome_audio,ipipe->verbose) < 0)
			{
	                	fprintf(stderr,"Buffer overflow\n");
				ipipe->error=1;
				break;
			}
			if (binary_dump)
			{
				if(tc_snprintf(s_probe_cmd_buf+strlen(s_probe_cmd_buf), MAX_BUF-strlen(s_probe_cmd_buf), "-B ") < 0)
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

                                if (fread(&tc_probe_pid, sizeof(pid_t), 1, p_fd) !=1)
                                {
                                        fprintf(stderr,"Cannot read pipe\n");
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
				p_audio_video->s_a_real_codec=s_other_audio.track[0].format;
				p_audio_video->s_a_rate=s_other_video.track[0].samplerate;
				p_audio_video->s_a_bits=s_other_video.track[0].bits;
				p_audio_video->s_a_chan=s_other_video.track[0].chan;
				if(s_other_audio.magic == TC_MAGIC_UNKNOWN || s_other_audio.magic == TC_MAGIC_PIPE || s_other_audio.magic == TC_MAGIC_ERROR)
				{
					fprintf(stderr,"\n\nerror: this version of transcode supports only\n");
					fprintf(stderr,"xml file who containing dv avi or mov file type.\n");
					fprintf(stderr,"Please clean up the %s file and restart.\n", ipipe->name);
					fprintf(stderr,"file %s with filetype %s is invalid for this operation mode.\n", p_audio_video->p_nome_audio, filetype(s_other_audio.magic));
					ipipe->error=1;
				}
				if (p_audio_video->s_a_magic == TC_MAGIC_UNKNOWN)	//forced by ioxml.c
				{
					if (s_other_audio.magic == TC_MAGIC_AVI)	//if the magic is AVI and the codec is DV -> i need to use DV
						if (s_other_audio.track[0].format == CODEC_PCM)	//for instant only PCM work
							p_audio_video->s_a_magic=TC_MAGIC_AVI;
						else
							p_audio_video->s_a_magic=s_other_audio.magic;
					else
						p_audio_video->s_a_magic=s_other_audio.magic;
				}
				if ((s_first_element & 0x01) == 0)
				{
					s_first_element|=0x01;
					ac_memcpy(p_first_audio,&s_other_audio,sizeof(probe_info_t));
				}
				f_det_totale_audio_frame(p_audio_video);
				if (p_audio_video->s_start_audio > p_audio_video->s_end_audio)
				{
					fprintf(stderr,"\n\nerror: start frame is greater than end frame in file %s\n",p_audio_video->p_nome_video); 
					ipipe->error=1;
				}
	                        *s_tot_frames_audio+=(p_audio_video->s_end_audio - p_audio_video->s_start_audio);       //selected frames
			}
		}
	}
	if (p_audiovideo->p_next !=NULL)
	{
		if ((ipipe->error=f_check_video_H_W(p_audiovideo->p_next)) == 0)		//check height and width compatibility
		{
			if (p_audiovideo->p_next->s_v_tg_height !=0)
				p_first_video->height=p_audiovideo->p_next->s_v_tg_height;	//force height to target-height
			if (p_audiovideo->p_next->s_v_tg_width !=0)
				p_first_video->width=p_audiovideo->p_next->s_v_tg_width;	//force width to target-width
		}
	}
	return(s_first_element);
}

void probe_xml(info_t *ipipe)
{
	audiovideo_t s_audiovideo;
	probe_info_t	s_first_audio;
	probe_info_t	s_first_video;
	long	s_tot_frames_audio,s_tot_frames_video;
	int s_first_element;


	if ((s_first_element=f_build_xml_tree(ipipe,&s_audiovideo,&s_first_audio,&s_first_video,&s_tot_frames_audio,&s_tot_frames_video)) == -1)
	{
		//there's an error during the call of f_build_xml_tree
		return;
	}

	(int)f_manage_input_xml(NULL,0,&s_audiovideo);

	if (s_first_element & 0x03)	//have video and audio tracks
	{
		ac_memcpy(ipipe->probe_info,&s_first_video,sizeof(probe_info_t)); //setup the probe_info structure
		ipipe->probe_info->frames=s_tot_frames_video;		//Force sum of selected frames
		ipipe->probe_info->num_tracks=s_first_audio.num_tracks;
		ac_memcpy(ipipe->probe_info->track,&(s_first_audio.track),TC_MAX_AUD_TRACKS*sizeof(pcm_t));
	}
	else if (s_first_element & 0x02)     //have only video track
	{
		ac_memcpy(ipipe->probe_info,&s_first_video,sizeof(probe_info_t)); //setup the probe_info structure
		ipipe->probe_info->frames=s_tot_frames_video;		//Force sum of selected frames
	}
	else if (s_first_element & 0x01)     //have only audio tracks
	{
		ac_memcpy(ipipe->probe_info,&s_first_audio,sizeof(probe_info_t)); //setup the probe_info structure
		ipipe->probe_info->frames=s_tot_frames_audio;		//Force sum of selected frames
	}
	s_first_element=0;
}

#else  // HAVE_LIBXML2

void probe_xml(info_t *ipipe)
{
        fprintf(stderr, "(%s) no support for XML compiled - exit.\n", __FILE__);
        ipipe->error=1;
        return;
}

#endif
