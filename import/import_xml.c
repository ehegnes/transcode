/*
 *  import_xml.c
 *
 *  Copyright (C) Marzio Malanchini - March 2002
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

#include "transcode.h"

#define MOD_NAME    "import_xml.so"
#define MOD_VERSION "v0.0.3 (2002-03-26)"
#define MOD_CODEC   "(video) * | (audio) *"

#define MOD_PRE xml
#include "import_def.h"
#include "ioxml.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static int verbose_flag=TC_QUIET;
static int capability_flag=-1;
static FILE *s_fd_video=0;
static FILE *s_fd_audio=0;
static int s_frame_size=0;
static int s_frame_audio_size=0;
static  audiovideo_t    s_audio,*p_audio=NULL;
static  audiovideo_t    s_video,*p_video=NULL;


/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
	if(param->flag == TC_VIDEO) 
	{
		param->fd = NULL;
                if (p_video == NULL)
                {
                        if (f_manage_input_xml(vob->video_in_file,1,&s_video))  //create the XML tree
                        {
                                (int)f_manage_input_xml(NULL,0,&s_video);
                                fprintf(stderr,"\nerror: file %s has invalid format content. \n", vob->video_in_file);
				return(TC_IMPORT_ERROR);
                        }
                        p_video=s_video.p_next;
                }
		if (p_video == NULL)
		{
                        fprintf(stderr,"\nerror: there isn't no file in  %s. \n", vob->video_in_file);
			return(TC_IMPORT_ERROR);
		}
		if (strcasecmp(vob->vmod_probed,"dv") == 0)
		{
			capability_flag=TC_CAP_RGB|TC_CAP_YUV|TC_CAP_DV|TC_CAP_PCM;
			switch(vob->im_v_codec) 
			{
				case CODEC_RGB:
					s_frame_size = vob->im_v_size;
					if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -x dv -d %d -C %ld-%ld | tcdecode -x dv -y rgb -d %d -Q %d", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video,vob->verbose, vob->quality)<0))
					{
						perror("command buffer overflow");
						return(TC_IMPORT_ERROR);
					}
				break;
				case CODEC_YUV:
					s_frame_size = (vob->im_v_width * vob->im_v_height * 3)/2;
					if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -x dv -d %d -C %ld-%ld | tcdecode -x dv -y yv12 -d %d -Q %d", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video,vob->verbose, vob->quality)<0))
					{
						perror("command buffer overflow");
						return(TC_IMPORT_ERROR);
					}
				break;
				case CODEC_RAW:
					s_frame_size = (vob->im_v_height==PAL_H) ? TC_FRAME_DV_PAL:TC_FRAME_DV_NTSC;
					if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -x dv -d %d -C %ld-%ld", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video)<0))
					{
						perror("command buffer overflow");
						return(TC_IMPORT_ERROR);
					}
				break;
				default:
					fprintf(stderr, "invalid import codec request 0x%x\n", vob->im_v_codec);
					return(TC_IMPORT_ERROR);
			}
		}
		else if (strcasecmp(vob->vmod_probed,"avi") == 0)
		{
			capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_AUD|TC_CAP_VID;
			s_frame_size=0;
			if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -d %d -x avi -C %ld-%ld",p_video->p_nome_video, vob->verbose,p_video->s_start_video,p_video->s_end_video)<0)) 
			{
				perror("command buffer overflow");
				return(TC_IMPORT_ERROR);
			}
                }
		else
		{
                        fprintf(stderr,"[%s] error: video codec %s not yet supported. \n", MOD_NAME,vob->vmod_probed);
			return(TC_IMPORT_ERROR);
		}
		if((s_fd_video = popen(import_cmd_buf, "r"))== NULL)
		{
			fprintf(stderr,"Error cannot open the pipe.\n");
			return(TC_IMPORT_ERROR);
		}
		p_video=p_video->p_next;
		if(verbose_flag) 
			printf("[%s] %s\n", MOD_NAME, import_cmd_buf);
		return(0);
	}
	if(param->flag == TC_AUDIO) 
	{
		param->fd = NULL;
		if (p_audio== NULL)
		{
			if (f_manage_input_xml(vob->audio_in_file,1,&s_audio))  //create the XML tree
			{
				(int)f_manage_input_xml(NULL,0,&s_audio);
				fprintf(stderr,"\nerror: file %s has invalid format content. \n", vob->audio_in_file);
				return(TC_IMPORT_ERROR);
			}
			p_audio=s_audio.p_next;
		}
		if (p_audio == NULL)
		{
                        fprintf(stderr,"\nerror: there isn't no file in  %s. \n", vob->audio_in_file);
			return(TC_IMPORT_ERROR);
		}
		if ((p_audio->s_audio_smpte==smpte)||(p_audio->s_audio_smpte==smpte25)||(p_audio->s_audio_smpte==npt))
		{
			s_frame_audio_size=(1.00 * vob->a_rate * vob->a_bits * vob->a_chan)/(25*8);
		}
		else if (p_audio->s_audio_smpte==smpte30drop)
		{
			s_frame_audio_size=(1.00 * vob->a_rate * vob->a_bits * vob->a_chan)/(29.97*8);
		}
		if (strcasecmp(vob->amod_probed,"dv") == 0)
		{
			capability_flag=TC_CAP_RGB|TC_CAP_YUV|TC_CAP_DV|TC_CAP_PCM;
			if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -d %d -x dv -C %ld-%ld | tcdecode -x dv -y pcm -d %d -Q %d", p_audio->p_nome_audio, vob->verbose,s_frame_audio_size*p_audio->s_start_audio,s_frame_audio_size*p_audio->s_end_audio,vob->verbose,vob->quality)<0))
			{
				perror("command buffer overflow");
				return(TC_IMPORT_ERROR);
			}
		}
		else if (strcasecmp(vob->amod_probed,"avi") == 0)
		{
			capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_AUD|TC_CAP_VID;
			if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -d %d -x pcm -a %d -C %ld-%ld",p_audio->p_nome_audio, vob->verbose,vob->a_track,s_frame_audio_size*p_audio->s_start_audio,s_frame_audio_size*p_audio->s_end_audio)<0)) 
			{
				perror("command buffer overflow");
				return(TC_IMPORT_ERROR);
			}
		}
		else
		{
                        fprintf(stderr,"[%s] error: audio codec %s not yet supported. \n", MOD_NAME,vob->amod_probed);
			return(TC_IMPORT_ERROR);
		}
		if((s_fd_audio = popen(import_cmd_buf, "r"))== NULL)
		{
			fprintf(stderr,"Error cannot open the pipe.\n");
			return(TC_IMPORT_ERROR);
		}
		p_audio=p_audio->p_next;
		if(verbose_flag) 
			printf("[%s] %s\n", MOD_NAME, import_cmd_buf);
		return(0);
	}
	return(TC_IMPORT_ERROR);
}


/* ------------------------------------------------------------ 
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/

MOD_decode 
{
	int s_audio_frame_size;
	int s_video_frame_size;
	static int s_audio_frame_size_orig=0;
	static int s_video_frame_size_orig=0;
	
	if(param->flag == TC_AUDIO) 
	{
                if (param->size < s_audio_frame_size_orig)
                {
                         param->size=s_audio_frame_size_orig;
                         s_audio_frame_size_orig=0;
                }
                s_audio_frame_size=fread(param->buffer, 1, param->size, s_fd_audio);
                if (s_audio_frame_size == 0)
                {
                        if (p_audio != NULL)    // is there a file ?
                        {
				if (strcasecmp(vob->amod_probed,"dv") == 0)
				{
					if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -d %d -x dv -C %ld-%ld | tcdecode -x dv -y pcm -d %d -Q %d", p_audio->p_nome_audio, vob->verbose,s_frame_audio_size*p_audio->s_start_audio,s_frame_audio_size*p_audio->s_end_audio,vob->verbose,vob->quality)<0))
                                        {
                                                perror("command buffer overflow");
                                                return(TC_IMPORT_ERROR);
                                        }
				}
				else if (strcasecmp(vob->amod_probed,"avi") == 0)
				{
					if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -d %d -x pcm -a %d -C %ld-%ld",p_audio->p_nome_audio, vob->verbose,vob->a_track,s_frame_audio_size*p_audio->s_start_audio,s_frame_audio_size*p_audio->s_end_audio)<0)) 
					{
						perror("command buffer overflow");
						return(TC_IMPORT_ERROR);
					}
                                }
				else
				{
                        		fprintf(stderr,"[%s] error: audio codec %s not yet supported. \n", MOD_NAME,vob->amod_probed);
					return(TC_IMPORT_ERROR);
				}
                                if((s_fd_audio = popen(import_cmd_buf, "r"))== NULL)
                                {
                                        fprintf(stderr,"Error cannot open the pipe.\n");
                                        return(TC_IMPORT_ERROR);
                                }
				if(verbose_flag) 
					printf("[%s] %s\n", MOD_NAME, import_cmd_buf);
                                p_audio=p_audio->p_next;
                        }
			else
			{
				return(TC_IMPORT_ERROR);
			}
                        s_audio_frame_size=fread(param->buffer, 1,param->size, s_fd_audio);
		}
                if (param->size > s_audio_frame_size)
                {
                        s_audio_frame_size_orig=param->size;
                        param->size=s_audio_frame_size;
                }
		return(0);
	}
	if(param->flag == TC_VIDEO) 
	{
                if (param->size < s_video_frame_size_orig)
                {
                         param->size=s_video_frame_size_orig;
                         s_video_frame_size_orig=0;
                }
		s_video_frame_size=fread(param->buffer, 1,param->size, s_fd_video);
		if (s_video_frame_size == 0)
		{
			if (p_video !=NULL)	// is there a file ?
			{
				if (strcasecmp(vob->vmod_probed,"dv") == 0)
				{
					switch(vob->im_v_codec) 
					{
						case CODEC_RGB:
							if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -x dv -d %d -C %ld-%ld | tcdecode -x dv -y rgb -d %d -Q %d", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video,vob->verbose, vob->quality)<0))
							{
								perror("command buffer overflow");
								return(TC_IMPORT_ERROR);
							}
						break;
						case CODEC_YUV:
							if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -x dv -d %d -C %ld-%ld | tcdecode -x dv -y yv12 -d %d -Q %d", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video,vob->verbose, vob->quality)<0))
							{
								perror("command buffer overflow");
								return(TC_IMPORT_ERROR);
							}
						break;
						case CODEC_RAW:
							if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -x dv -d %d -C %ld-%ld", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video)<0))
							{
								perror("command buffer overflow");
								return(TC_IMPORT_ERROR);
							}
						break;
						default:
						break;
					}
				}
				else if (strcasecmp(vob->vmod_probed,"avi") == 0)
				{
					if((snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -d %d -x avi -C %ld-%ld",p_video->p_nome_video, vob->verbose,p_video->s_start_video,p_video->s_end_video)<0))
					{
						perror("command buffer overflow");
						return(TC_IMPORT_ERROR);
					}
				}
				else
				{
                        		fprintf(stderr,"[%s] error: video codec %s not yet supported. \n", MOD_NAME,vob->vmod_probed);
					return(TC_IMPORT_ERROR);
				}
                       		if((s_fd_video = popen(import_cmd_buf, "r"))== NULL)
                               	{
                                	fprintf(stderr,"Error cannot open the pipe.\n");
     		                 	return(TC_IMPORT_ERROR);
               		        }
				if(verbose_flag) 
					printf("[%s] %s\n", MOD_NAME, import_cmd_buf);
                       		p_video=p_video->p_next;
			}
			else
			{
				return(TC_IMPORT_ERROR);
			}
			s_video_frame_size=fread(param->buffer, 1,param->size, s_fd_video);
		}
                if (param->size > s_video_frame_size)
                {
                        s_video_frame_size_orig=param->size;
                        param->size=s_video_frame_size;
                }
		return(0);
	}
	return(TC_IMPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{  
	if(param->flag == TC_AUDIO) 
	{
		s_fd_audio=0;
		param->fd=NULL;	
		return(0);
	}
	if(param->flag == TC_VIDEO) 
	{
		s_fd_video=0;
		param->fd=NULL;	
		return(0);
	}
	return(TC_IMPORT_ERROR);
}



