/*
 *  export_pvm_slave.c
 *
 *  Copyright (C) Marzio Malanchini - July 2003
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#else
# ifdef SYSTEM_DARWIN
#  include "../libdldarwin/dlfcn.h"
# endif
#endif

#include <pvm_interface.h>
#include <export_pvm_slave.h>
#include <external_codec.h>
#include <transcode.h>


#define MODULE 		"pvm_functions.so"
#define MAX_BUF 	1024
#define COPY_BUFFER	10485760		/*10 MB*/

extern char *p_param1,*p_param2,*p_param3; /*codec input parameter*/
extern int (*tc_export)(int,void *,void *);
extern int tc_accel;
extern int s_elab_type;
extern char *p_out_file_name;
extern int s_list_only,verbose;
extern char *p_request_func;
extern char *p_hostname;
extern char *p_merge_cmd;

void tc_progress(char *string)
{
/*just to remove progress from child tasks*/
}

struct pvm_files_t	{
				int			s_seq;
				char 			s_file[2*MAX_BUF];
				struct pvm_files_t 	*p_next;
			};

int f_system_merge(pvm_config_env *p_pvm_conf)
{
	pvm_config_filelist *p_video_list=NULL,*p_audio_list=NULL;
	char s_buffer[MAX_BUF],*p_par=NULL;
	int s_file_dest;

	if ((s_file_dest=creat(p_pvm_conf->s_sys_list.p_destination,S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH))==-1)
	{
		fprintf(stderr,"(%s) can't create %s output file.\n",__FILE__,p_pvm_conf->s_sys_list.p_destination);
		p_pvm_conf=f_pvm_parser(p_out_file_name,"close");
		return(1);
	}
	for (p_video_list=p_pvm_conf->p_add_list;((p_video_list->s_type!=TC_AUDIO)&&(p_video_list!=NULL));p_video_list=p_video_list->p_next);
	for (p_audio_list=p_pvm_conf->p_add_list;((p_audio_list->s_type!=TC_VIDEO)&&(p_audio_list!=NULL));p_audio_list=p_audio_list->p_next);
	if (p_video_list==NULL)
	{
		fprintf(stderr,"(%s) request a system merge without video list inside %s .\n",__FILE__,p_out_file_name);
		return(1);
	}
	else if (p_audio_list==NULL)
	{
		fprintf(stderr,"(%s) request a system merge without audio list inside %s .\n",__FILE__,p_out_file_name);
		return(1);
	}
	memset((char *)&s_buffer,'\0',MAX_BUF);
	snprintf((char *)&s_buffer,MAX_BUF,"%s-%d",p_pvm_conf->s_sys_list.p_destination,getpid());
	p_par=strtok(p_pvm_conf->p_multiplex_cmd,"\"");
	while ((p_video_list!=NULL)&&(p_audio_list!=NULL))
	{
		if (f_multiplexer(p_pvm_conf->s_sys_list.p_codec,p_par,p_video_list->p_filename,p_audio_list->p_filename,(char *)&s_buffer,verbose))
		{
			fprintf(stderr,"(%s) unsupported codec %s.\n",__FILE__,p_merge_cmd);
			return(1);
		}
		if (verbose & TC_DEBUG)
			fprintf(stderr,"(%s) multiplex audio %s and video %s into %s\n",__FILE__,p_audio_list->p_filename,p_video_list->p_filename,(char *)&s_buffer);
		if(f_copy_remove_func("open",(char *)&s_buffer,s_file_dest))
			return(1);
		if (verbose & TC_DEBUG)
			fprintf(stderr,"(%s) merge into %s and remove file %s\n",__FILE__,p_pvm_conf->s_sys_list.p_destination,(char *)&s_buffer);
		p_video_list=p_video_list->p_next;
		p_audio_list=p_audio_list->p_next;
	}
	close(s_file_dest);
	return(0);
}

int f_copy_remove_func(char *p_option,char *p_file,int s_file_dest)
{
	FILE *p_file_src;
	static char *p_buffer=NULL;
	int s_tmp;

	
	if(!strcasecmp(p_option,"open"))
	{
		if (p_buffer == NULL)
			p_buffer=(char *)malloc(COPY_BUFFER);
		if ((p_file_src=fopen(p_file,"r"))==NULL) /*file exist!*/
		{
			fprintf(stderr,"(%s) file %s not found\n",__FILE__,p_file);
			return(1);
		}
		while(!feof(p_file_src))	/*copy process*/
		{
			s_tmp=fread(p_buffer,1,COPY_BUFFER,p_file_src);	
			write(s_file_dest,p_buffer,s_tmp);
		}
		fclose(p_file_src);
		remove(p_file);
	}
	else if (!strcasecmp(p_option,"close"))
	{
		free(p_buffer);
		p_buffer=NULL;
	}
	else
	{
		fprintf(stderr,"(%s) invalid option\n",__FILE__);
		return(1);
	}
	return(0);
}

pvm_res_func_t *f_export_func(int s_option,char *p_buffer,int s_size,int s_seq)
{
        static int s_cicle=0,s_serial=0;
	static pvm_res_func_t s_result={0,0,PVM_MSG_WRKN,0,NULL};
	static transfer_t s_export_param;
	static vob_t s_vob,*vob;
	int s_tmp,s_rc;
	static char s_filename[2*MAX_BUF];
	static char *p_suffix=NULL;
	static int s_file_dest=-1,s_first_encode=0;
	static struct pvm_files_t *p_file_elab=NULL,*p_file_erase=NULL,*p_file_erase_tmp=NULL;
	struct pvm_files_t *p_file_elab_tmp=NULL,*p_search=NULL;
	struct stat s_f_stat;
	FILE *p_file_src=NULL;
	pvm_config_env *p_pvm_conf=NULL;
	pvm_config_filelist *p_my_filelist=NULL;

	if (p_suffix==NULL)
		p_suffix=f_external_suffix(p_request_func,p_param1);	/*function codec based*/
	s_result.s_msg_type=PVM_MSG_WRKN;	/*always initialized to PVM_MSG_WRKN*/
	if (s_result.p_result==NULL)
	{
		s_result.s_dim_buffer=(s_size> 2*MAX_BUF)?s_size:2*MAX_BUF;
        	s_result.p_result=(char *)calloc(s_result.s_dim_buffer,1);
	}
	else if (s_size>s_result.s_dim_buffer)
	{
        	s_result.p_result=(char *)realloc(s_result.p_result,s_size);
		s_result.s_dim_buffer=s_size;
	}
	memset(s_result.p_result,'\0',s_result.s_dim_buffer);
	switch(s_option)
	{
		case PVM_JOIN_OPT_INIT:
			if (verbose & TC_DEBUG)
				fprintf(stderr,"(%s) enter in PVM_JOIN_OPT_INIT \n",__FILE__);
			memset((char *)&s_filename,'\0',2*MAX_BUF);
			if (!s_list_only)
				snprintf((char *)&s_filename,2*MAX_BUF,"%s%s",p_out_file_name,p_suffix);
			else
			{
				if (s_elab_type==TC_VIDEO)
					snprintf((char *)&s_filename,2*MAX_BUF,"%s-video.lst",p_out_file_name);
				else if (s_elab_type==TC_AUDIO)
					snprintf((char *)&s_filename,2*MAX_BUF,"%s-audio.lst",p_out_file_name);
				else
					snprintf((char *)&s_filename,2*MAX_BUF,"%s-system.lst",p_out_file_name);
			}
			s_result.s_msg_type=PVM_MSG_JOIN;	/*don't need a receive msg*/
			s_result.s_ret_size=0;
			s_result.s_rc=0;
			if (verbose & TC_DEBUG)
				fprintf(stderr,"(%s) exit from PVM_JOIN_OPT_INIT \n",__FILE__);
		break;
		case PVM_JOIN_OPT_RUN:
			if (verbose & TC_DEBUG)
				fprintf(stderr,"(%s) enter in PVM_JOIN_OPT_RUN \n",__FILE__);
			if (s_elab_type!=TC_VIDEO_AUDIO)	/*video and audio file*/
			{
				if ((s_file_dest=creat((char *)&s_filename,S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH))==-1)
				{
					fprintf(stderr,"can't open %s output file.\n",p_out_file_name);
					s_result.s_rc=1;
					break;
				}
				if (s_list_only)
				{
					memset((char *)&s_filename,'\0',2*MAX_BUF);
					if (s_elab_type==TC_VIDEO)
						snprintf((char *)&s_filename,2*MAX_BUF,"[AddVideoList]\nDestination = %s%s\nCodec = %s\n",p_out_file_name,p_suffix,p_request_func);
					else
						snprintf((char *)&s_filename,2*MAX_BUF,"[AddAudioList]\nDestination = %s%s\nCodec = %s\n",p_out_file_name,p_suffix,p_request_func);
					write(s_file_dest,(char *)&s_filename,strlen(s_filename));
				}
				s_rc=0;
				for(p_file_elab_tmp=p_file_elab;p_file_elab_tmp!=NULL;p_file_elab_tmp=p_file_elab_tmp->p_next)
				{
					if (!s_list_only)
					{
						if (f_copy_remove_func("open",(char *)&(p_file_elab_tmp->s_file),s_file_dest))
						{
							s_rc=1;
						}
					}
					else
					{
						memset((char *)&s_filename,'\0',2*MAX_BUF);
						snprintf((char *)&s_filename,2*MAX_BUF,"%s\n",(char *)&(p_file_elab_tmp->s_file));
						write(s_file_dest,(char *)&s_filename,strlen(s_filename));
					}
				}
				if (s_list_only)
				{
					memset((char *)&s_filename,'\0',2*MAX_BUF);
					if (s_elab_type==TC_VIDEO)
						memcpy((char *)&s_filename,"[RemoveVideoList]\n",19);
					else
						memcpy((char *)&s_filename,"[RemoveAudioList]\n",19);
					write(s_file_dest,(char *)&s_filename,strlen(s_filename));
				}
				else
					(int)f_copy_remove_func("close",NULL,0);
				for(p_file_erase_tmp=p_file_erase;p_file_erase_tmp!=NULL;p_file_erase_tmp=p_file_erase_tmp->p_next)
				{
					if (!s_list_only)
						remove((char *)&(p_file_erase_tmp->s_file));	/*some files are already removed*/
					else
					{
						memset((char *)&s_filename,'\0',2*MAX_BUF);
						snprintf((char *)&s_filename,2*MAX_BUF,"%s\n",(char *)&(p_file_erase_tmp->s_file));
						write(s_file_dest,(char *)&s_filename,strlen(s_filename));
					}
				}
				close(s_file_dest);
				s_result.s_rc=s_rc;
			}
			else	/*system*/
			{
				memset((char *)&s_filename,'\0',2*MAX_BUF);
				snprintf((char *)&s_filename,2*MAX_BUF,"%s-system.lst",p_out_file_name);
				if ((p_pvm_conf=f_pvm_parser((char *)&s_filename,"open"))==NULL)
				{
					fprintf(stderr,"(%s) error checking %s\n",__FILE__,(char *)&s_filename);
					s_result.s_rc=1;
				}
				else
				{
					s_result.s_rc=f_system_merge(p_pvm_conf);
					for (p_my_filelist=p_pvm_conf->p_rem_list;p_my_filelist!=NULL;p_my_filelist=p_my_filelist->p_next)
					{
						if (verbose & TC_DEBUG)
							fprintf(stderr,"(%s) remove file %s\n",__FILE__,p_my_filelist->p_filename);
						remove(p_my_filelist->p_filename);
					}
					p_pvm_conf=f_pvm_parser((char *)&s_filename,"close");
				}
			}
			s_result.s_msg_type=PVM_MSG_WORK;	/*need a receive*/
			s_result.s_ret_size=0;
			if (verbose & TC_DEBUG)
				fprintf(stderr,"(%s) exit from PVM_JOIN_OPT_RUN \n",__FILE__);
		break;
		case PVM_MSG_MERG_PASTE:
			if (s_cicle==0)
			{
				if ((s_file_dest=creat((char *)&s_filename,S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH))==-1)
				{
					fprintf(stderr,"can't open %s output file.\n",p_out_file_name);
					s_result.s_rc=1;
					break;
				}
				memset((char *)&s_filename,'\0',2*MAX_BUF);
				snprintf((char *)&s_filename,2*MAX_BUF,"[SystemList]\nDestination = %s%s\nCodec = %s\nMultiplexParams = %s\n",p_out_file_name,p_suffix,p_request_func,p_merge_cmd);
				write(s_file_dest,(char *)&s_filename,strlen(s_filename));
			}
			write(s_file_dest,p_buffer,s_size);
			s_cicle++;
			if (s_cicle==2)		/*need to paste two files*/
			{
				close(s_file_dest);
				s_result.s_msg_type=PVM_MSG_ENDTASK_SYSTEM;	/*need a receive*/
			}
			else
				s_result.s_msg_type=PVM_MSG_WRKN;	/*need a receive*/
			s_result.s_ret_size=0;		/*nothing to send*/
			s_result.s_rc=0;
		break;
		case PVM_JOIN_OPT_SENDFILE:
			memset((char *)&s_filename,'\0',2*MAX_BUF);
			if (s_elab_type==TC_VIDEO)
				snprintf((char *)&s_filename,2*MAX_BUF,"%s-video.lst",p_out_file_name);
			else
				snprintf((char *)&s_filename,2*MAX_BUF,"%s-audio.lst",p_out_file_name);
			p_file_src=fopen((char *)&s_filename,"r"); /*file exist!*/
			stat((char *)&s_filename,&s_f_stat);
			if (s_f_stat.st_size>s_result.s_dim_buffer)
			{
				s_result.p_result=(char *)realloc(s_result.p_result,s_f_stat.st_size);
				s_result.s_dim_buffer=s_f_stat.st_size;
			}
			memset(s_result.p_result,'\0',s_result.s_dim_buffer);
			fread(s_result.p_result,1,s_f_stat.st_size,p_file_src);
			fclose(p_file_src);
			s_result.s_msg_type=PVM_MSG_MERG_SEND;	
			s_result.s_ret_size=s_f_stat.st_size;
			s_result.s_rc=0;
		break;
		case PVM_JOIN_OPT_ADD_REMOVE:
			if (verbose & TC_DEBUG)
				fprintf(stderr,"(%s) enter in PVM_JOIN_OPT_ADD_REMOVE \n",__FILE__);
			if (p_file_erase==NULL)
			{
				p_file_erase=(struct pvm_files_t *)calloc(sizeof(struct pvm_files_t),1);
				p_file_erase_tmp=p_file_erase;
			}
			else
			{
				p_file_erase_tmp->p_next=(struct pvm_files_t *)calloc(sizeof(struct pvm_files_t),1);
				p_file_erase_tmp=p_file_erase_tmp->p_next;
			}
			p_file_erase_tmp->s_seq=s_seq;
			memcpy((char *)&(p_file_erase_tmp->s_file),p_buffer,s_size);
			memcpy((char *)&(p_file_erase_tmp->s_file)+s_size,p_suffix,strlen(p_suffix));
			s_result.s_msg_type=PVM_MSG_JOIN;	
			s_result.s_ret_size=0;
			s_result.s_rc=0;
			if (verbose & TC_DEBUG)
				fprintf(stderr,"(%s) exit from PVM_JOIN_OPT_ADD_REMOVE \n",__FILE__);
		break;
		case PVM_JOIN_OPT_ADD_ELAB:
			if (verbose & TC_DEBUG)
				fprintf(stderr,"(%s) enter in PVM_JOIN_OPT_ADD_ELAB \n",__FILE__);
			/*need to order for seq number*/
			if (p_file_elab==NULL)
			{
				p_file_elab_tmp=(struct pvm_files_t *)calloc(sizeof(struct pvm_files_t),1);
				p_file_elab=p_file_elab_tmp;
			}
			else
			{
				p_file_elab_tmp=p_file_elab;
				for (p_search=p_file_elab;p_search!=NULL;p_search=p_search->p_next)	
				{
					if (p_search->s_seq >s_seq)
						break;
					else
						p_file_elab_tmp=p_search;
				}
				if (p_search==p_file_elab)
				{
					p_file_elab_tmp=(struct pvm_files_t *)calloc(sizeof(struct pvm_files_t),1);
					p_file_elab_tmp->p_next=p_file_elab;
					p_file_elab=p_file_elab_tmp;
				}
				else
				{
					p_file_elab_tmp->p_next=(struct pvm_files_t *)calloc(sizeof(struct pvm_files_t),1);
					p_file_elab_tmp=p_file_elab_tmp->p_next;
					p_file_elab_tmp->p_next=p_search;
				}
			}
			p_file_elab_tmp->s_seq=s_seq;
			memcpy((char *)&(p_file_elab_tmp->s_file),p_buffer,s_size);
			memcpy((char *)&(p_file_elab_tmp->s_file)+s_size,p_suffix,strlen(p_suffix));
			s_result.s_msg_type=PVM_MSG_JOIN;	/*don't need a receive msg*/
			s_result.s_ret_size=0;
			s_result.s_rc=0;
			if (verbose & TC_DEBUG)
				fprintf(stderr,"(%s) exit from PVM_JOIN_OPT_ADD_ELAB \n",__FILE__);
		break;
		case PVM_EXP_OPT_INIT:
			if (s_size!=sizeof(vob_t))
			{
				fprintf(stderr,"invalid vob_t size\n");
				s_result.s_msg_type=PVM_MSG_CONF;
				s_result.s_ret_size=0;
				s_result.s_rc=1;
			}
			else
			{
				vob=&s_vob;
				memcpy((char *)&s_vob,p_buffer,sizeof(vob_t));
				vob->accel=tc_accel;
				vob->ex_v_fcc=p_param1;		/*setting up the correct parameter*/
				vob->ex_a_fcc=p_param2;		/*setting up the correct parameter*/
				vob->ex_profile_name=p_param3;	/*setting up the correct parameter*/
				s_export_param.flag=1;	/*write the module name*/
				if(!s_cicle)
				{
					s_cicle++;
					fprintf(stderr,"(%s) on host %s pid %d recall ",__FILE__,p_hostname,getpid());
					memset(s_result.p_result,'\0',s_result.s_dim_buffer);
				}
				tc_export(TC_EXPORT_NAME,(void *)&s_export_param,NULL); /*check the capability*/
				if (s_elab_type==TC_VIDEO)
				{
					switch (vob->im_v_codec) 
					{
						case CODEC_RGB: 
							s_tmp=(s_export_param.flag & TC_CAP_RGB);
						break;
						case CODEC_YUV: 
							s_tmp=(s_export_param.flag & TC_CAP_YUV);
						break;
						case CODEC_RAW: 
						case CODEC_RAW_YUV: 
							s_tmp=(s_export_param.flag & TC_CAP_VID);
						break;
						default:
							s_tmp=0;
					}
					s_export_param.flag=TC_VIDEO;
				}
				else	/*audio codec*/
				{
					switch (vob->im_a_codec) 
					{
						case CODEC_PCM: 
							s_tmp=(s_export_param.flag & TC_CAP_PCM);
						break;
						case CODEC_AC3: 
							s_tmp=(s_export_param.flag & TC_CAP_AC3);
						break;
						case CODEC_RAW: 
							s_tmp=(s_export_param.flag & TC_CAP_AUD);
						break;
						default:
							s_tmp=0;
					}
					s_export_param.flag=TC_AUDIO;
				}
				if (!s_tmp)		
				{
					s_result.s_msg_type=PVM_MSG_CONF;
					s_result.s_ret_size=0;
					s_result.s_rc=1;	/*capability unsupported*/
					fprintf(stderr,"(%s) unsupported codec %d",__FILE__,vob->im_v_codec);
					break;
				}
				if (tc_export(TC_EXPORT_INIT,(void *)&s_export_param,vob)==TC_EXPORT_ERROR) /*check the capability*/
				{
					if (s_elab_type==TC_VIDEO)
						fprintf(stderr,"(%s) audio export module error: init failed\n",__FILE__);
					else
						fprintf(stderr,"(%s) audio export module error: init failed\n",__FILE__);
					s_result.s_msg_type=PVM_MSG_CONF;
					s_result.s_ret_size=0;
					s_result.s_rc=-1;	
					break;
				}
				s_result.s_msg_type=PVM_MSG_CONF;
				s_result.s_ret_size=0;
				s_result.s_rc=0;
			}
		break;
		case PVM_EXP_OPT_OPEN:
			memset((char *)&s_filename,'\0',2*MAX_BUF);
			snprintf((char *)&s_filename,2*MAX_BUF,"%s-%s-%d-%d",p_out_file_name,p_hostname,getpid(),s_serial);
			s_serial++;
			if (s_elab_type==TC_VIDEO)
			{
				vob->video_out_file=(char *)&s_filename;
				s_export_param.flag=TC_VIDEO;
			}
			else	/*audio codec*/
			{
				vob->audio_out_file=(char *)&s_filename;
				s_export_param.flag=TC_AUDIO;
			}
			if (tc_export(TC_EXPORT_OPEN,(void *)&s_export_param,vob)==TC_EXPORT_ERROR) 
			{
				if (s_elab_type==TC_VIDEO)
					fprintf(stderr,"(%s) video export module error: open failed\n",__FILE__);
				else
					fprintf(stderr,"(%s) audio export module error: open failed\n",__FILE__);
				s_result.s_msg_type=PVM_MSG_CONF;	
				s_result.s_ret_size=0;
				s_result.s_rc=-1;	
				break;
			}
			s_result.s_ret_size=strlen((char *)&s_filename);	/*need a number !0*/
			memcpy(s_result.p_result,(char *)&s_filename,s_result.s_ret_size);
			s_result.s_msg_type=PVM_MSG_CONF_JOIN;	
			s_result.s_rc=0;
		break;
		case PVM_EXP_OPT_ENCODE:
			memcpy((char *)&s_export_param,p_buffer,sizeof(transfer_t));
			s_export_param.buffer=p_buffer+sizeof(transfer_t);	/*pointer to the data*/
			if (s_elab_type==TC_VIDEO)
				s_export_param.flag=TC_VIDEO;
			else	
				s_export_param.flag=TC_AUDIO;
			if (tc_export(TC_EXPORT_ENCODE,(void *)&s_export_param,vob)==TC_EXPORT_ERROR) 
			{
				if (s_elab_type==TC_VIDEO)
					fprintf(stderr,"(%s) video export module error: encode failed\n",__FILE__);
				else
					fprintf(stderr,"(%s) audio export module error: encode failed\n",__FILE__);
				s_result.s_msg_type=PVM_MSG_WORK;
				s_result.s_ret_size=0;
				s_result.s_rc=-1;	
				break;
			}
			if (s_first_encode)
			{
				s_result.s_msg_type=PVM_MSG_WRKN;
				s_result.s_ret_size=0;
			}
			else
			{
				s_first_encode=1;
				s_result.s_ret_size=strlen((char *)&s_filename);        /*need a number !0*/
				memcpy(s_result.p_result,(char *)&s_filename,s_result.s_ret_size);
				s_result.s_msg_type=PVM_JOIN_OPT_ADD_ELAB;
			}
			s_result.s_rc=0;
		break;
		case PVM_EXP_OPT_CLOSE:
			if (s_elab_type==TC_VIDEO)
				s_export_param.flag=TC_VIDEO;
			else	
				s_export_param.flag=TC_AUDIO;
			if (tc_export(TC_EXPORT_CLOSE,(void *)&s_export_param,NULL)==TC_EXPORT_ERROR) 
			{
				if (s_elab_type==TC_VIDEO)
					fprintf(stderr,"(%s) video export module error: close failed\n",__FILE__);
				else
					fprintf(stderr,"(%s) audio export module error: close failed\n",__FILE__);
				s_result.s_msg_type=PVM_MSG_CONF;
				s_result.s_ret_size=0;
				s_result.s_rc=-1;	
				break;
			}
			s_result.s_msg_type=PVM_MSG_CONF;	
			s_result.s_ret_size=0;
			s_result.s_rc=0;	
		break;
		case PVM_EXP_OPT_STOP:
			if (s_elab_type==TC_VIDEO)
				s_export_param.flag=TC_VIDEO;
			else	
				s_export_param.flag=TC_AUDIO;
			if (tc_export(TC_EXPORT_STOP,(void *)&s_export_param,vob)==TC_EXPORT_ERROR) 
			{
				if (s_elab_type==TC_VIDEO)
					fprintf(stderr,"(%s) video export module error: stop failed\n",__FILE__);
				else
					fprintf(stderr,"(%s) audio export module error: stop failed\n",__FILE__);
				s_result.s_msg_type=PVM_MSG_CONF;	
				s_result.s_ret_size=0;
				s_result.s_rc=-1;	
				break;
			}
			s_result.s_msg_type=PVM_MSG_CONF;
			s_result.s_ret_size=0;
			s_result.s_rc=0;	
		break;
		case PVM_EXP_OPT_RESTART_ENCODE:
			if (s_elab_type==TC_VIDEO)
				s_export_param.flag=TC_VIDEO;
			else	
				s_export_param.flag=TC_AUDIO;
			/*force the encoder to close the file and reopen a new one*/
			if (tc_export(TC_EXPORT_CLOSE,(void *)&s_export_param,NULL)==TC_EXPORT_ERROR) 
			{
				if (s_elab_type==TC_VIDEO)
					fprintf(stderr,"(%s) video export module error: close failed\n",__FILE__);
				else
					fprintf(stderr,"(%s) audio export module error: close failed\n",__FILE__);
				s_result.s_msg_type=PVM_MSG_WORK;	
				s_result.s_ret_size=0;
				s_result.s_rc=-1;	
				break;
			}
			memset((char *)&s_filename,'\0',2*MAX_BUF);
			snprintf((char *)&s_filename,2*MAX_BUF,"%s-%s-%d-%d",p_out_file_name,p_hostname,getpid(),s_serial);
			s_serial++;
			if (s_elab_type==TC_VIDEO)
			{
				vob->video_out_file=(char *)&s_filename;
				s_export_param.flag=TC_VIDEO;
			}
			else
			{
				vob->audio_out_file=(char *)&s_filename;
				s_export_param.flag=TC_AUDIO;
			}
			if (tc_export(TC_EXPORT_OPEN,(void *)&s_export_param,vob)==TC_EXPORT_ERROR) 
			{
				if (s_elab_type==TC_VIDEO)
					fprintf(stderr,"(%s) video export module error: open failed\n",__FILE__);
				else
					fprintf(stderr,"(%s) audio export module error: open failed\n",__FILE__);
				s_result.s_msg_type=PVM_MSG_WORK;	
				s_result.s_ret_size=0;
				s_result.s_rc=-1;	
				break;
			}
			/*finished to encode the range so the task is free: send a msg to the father and another to the merger*/
			s_result.s_ret_size=strlen((char *)&s_filename);	/*need a number !0*/
			memset(s_result.p_result,'\0',s_result.s_dim_buffer);
			memcpy(s_result.p_result,(char *)&s_filename,s_result.s_ret_size);
			if (s_elab_type==TC_VIDEO)
				s_result.s_msg_type=PVM_MSG_ENDTASK_VIDEO;	
			else
				s_result.s_msg_type=PVM_MSG_ENDTASK_AUDIO;	
			s_first_encode=0;
			s_result.s_rc=0;
		break;
		default:
		break;
	}
        return((pvm_res_func_t *)&s_result);
}

