/*
 *  pvm_parser.c
 *
 *  Copyright (C) Malanchini Marzio - August 2003
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
#include <strings.h>
#include <string.h>
#include <pvm_parser.h>


extern int verbose;

#define PVMHOSTCAPABILITY_MASK	0x00000001
#define MERGER_MASK		0x00000010
#define EXPORTMODULE_MASK	0x00000100
#define ADDLIST_MASK		0x00001000
#define REMOVELIST_MASK		0x00010000
#define SYSTEMLIST_MASK		0x00100000


char *f_skip_space(char *p_value,char *p_default)
{
	char *p_tmp;

	p_tmp=cf_skip_frontwhite(p_value);
	if (strlen(p_tmp) == 0)
		return(p_default);
	return(p_tmp);
}


pvm_config_env *f_pvm_parser(char *p_hostfile,char *p_option)
{
	CF_ROOT_TYPE *p_root;
	CF_SECTION_TYPE *p_section;
	CF_SUBSECTION_TYPE	*p_subsection;
	CF_KEYVALUE_TYPE	*p_key_value;
	int s_tmp;
	static pvm_config_env s_pvm_conf;
	pvm_config_hosts	*p_pvm_conf_host=NULL,*p_tmp;
	pvm_config_filelist	*p_pvm_conf_fileadd=NULL,*p_pvm_conf_filerem=NULL,*p_pvm_conf_file_loglist=NULL;
	static char *p_localhost=".";	/*localhost*/
	int s_check_func=0;
	int s_list_type=0;

	
	if(!strcasecmp(p_option,"open"))
	{
		if((p_root=cf_read(p_hostfile))==NULL)
		{
			fprintf(stderr, "(%s) error reading configuration file\n",__FILE__);
			return(NULL);
		}
		memset((char *)&s_pvm_conf,'\0',sizeof(pvm_config_env));
		if((p_section=cf_get_section(p_root))!=NULL)
		{
			do 
			{
				if(!strcasecmp(p_section->name,"PvmHostCapability"))
				{
					s_check_func|=PVMHOSTCAPABILITY_MASK;
					for (p_key_value=p_section->keyvalue;p_key_value!=NULL;p_key_value=p_key_value->next)
					{
						if (verbose & TC_DEBUG)	//debug
							fprintf(stderr,"(%s) Section %s key %s value %s\n",__FILE__,p_section->name,p_key_value->key,p_key_value->value);
						if(!strcasecmp(p_key_value->key,"NumProcMaxForHost"))
							s_pvm_conf.s_nproc=atoi(p_key_value->value);
						else if(!strcasecmp(p_key_value->key,"MaxProcForCluster"))
							s_pvm_conf.s_max_proc=atoi(p_key_value->value);
						else if(!strcasecmp(p_key_value->key,"NumElabFrameForTask"))
							s_pvm_conf.s_num_frame_task=atoi(p_key_value->value);
						else if(!strcasecmp(p_key_value->key,"InternalMultipass"))
							s_pvm_conf.s_internal_multipass=(atoi(p_key_value->value)==1)?1:0;
						else
						{
							fprintf(stderr,"(%s) invalid section %s parameter %s value %s\n",__FILE__,p_section->name,p_key_value->key,p_key_value->value);
							return(f_pvm_parser(NULL,"close"));
						}
					}
					for (p_subsection=p_section->subsection;p_subsection!=NULL;p_subsection=p_subsection->next)
					{
						if (p_pvm_conf_host==NULL)
						{
							p_pvm_conf_host=(pvm_config_hosts *)calloc(sizeof(pvm_config_hosts),1);
							s_pvm_conf.p_pvm_hosts=p_pvm_conf_host;
						}
						else
						{
							p_pvm_conf_host->p_next=(pvm_config_hosts *)calloc(sizeof(pvm_config_hosts),1);
							p_pvm_conf_host=p_pvm_conf_host->p_next;
						}
						p_pvm_conf_host->p_hostname=p_localhost;	/*this is the default*/
						p_pvm_conf_host->s_nproc=1;	/*this is the default*/
						for (p_key_value=p_subsection->keyvalue;p_key_value!=NULL;p_key_value=p_key_value->next)
						{
							if (verbose & TC_DEBUG)	//debug
								fprintf(stderr,"(%s) Subsection %s key %s value %s\n",__FILE__,p_subsection->name,p_key_value->key,p_key_value->value);
							if(!strcasecmp(p_subsection->name,"Host"))
							{
								if(!strcasecmp(p_key_value->key,"Hostname"))
									p_pvm_conf_host->p_hostname=f_skip_space(p_key_value->value,p_localhost);
								else if(!strcasecmp(p_key_value->key,"NumProcMax"))
									p_pvm_conf_host->s_nproc=atoi(p_key_value->value);
								else
								{
									fprintf(stderr,"(%s) invalid subsection %s parameter %s value %s\n",__FILE__,p_subsection->name,p_key_value->key,p_key_value->value);
									return(f_pvm_parser(NULL,"close"));
								}
							}
							else
							{
								fprintf(stderr,"(%s) invalid subsection %s in sectio %s\n",__FILE__,p_subsection->name,p_section->name);
								return(f_pvm_parser(NULL,"close"));
							}
						}
					}
				}
				else if(!strcasecmp(p_section->name,"SystemMerger"))
				{
					s_check_func|=MERGER_MASK;
					s_pvm_conf.s_system_merger.p_hostname=p_localhost;	/*this is the default*/
					for (p_key_value=p_section->keyvalue;p_key_value!=NULL;p_key_value=p_key_value->next)
					{
						if (verbose & TC_DEBUG)	//debug
							fprintf(stderr,"(%s) Section %s key %s value %s\n",__FILE__,p_section->name,p_key_value->key,p_key_value->value);
						if(!strcasecmp(p_key_value->key,"Hostname"))
							s_pvm_conf.s_system_merger.p_hostname=f_skip_space(p_key_value->value,p_localhost);
						else if(!strcasecmp(p_key_value->key,"BuildOnlyBatchMergeList"))
							s_pvm_conf.s_system_merger.s_build_only_list=(atoi(p_key_value->value)>2||atoi(p_key_value->value)<0)?1:atoi(p_key_value->value);
						else if(!strcasecmp(p_key_value->key,"MultiplexParams"))
							s_pvm_conf.p_multiplex_cmd=f_skip_space(p_key_value->value,NULL);
						else
						{
							fprintf(stderr,"(%s) invalid section %s parameter %s value %s\n",__FILE__,p_section->name,p_key_value->key,p_key_value->value);
							return(f_pvm_parser(NULL,"close"));
						}
					}
				}
				else if(!strcasecmp(p_section->name,"AudioMerger"))
				{
					s_check_func|=MERGER_MASK;
					s_pvm_conf.s_audio_merger.p_hostname=p_localhost;	/*this is the default*/
					for (p_key_value=p_section->keyvalue;p_key_value!=NULL;p_key_value=p_key_value->next)
					{
						if (verbose & TC_DEBUG)	//debug
							fprintf(stderr,"(%s) Section %s key %s value %s\n",__FILE__,p_section->name,p_key_value->key,p_key_value->value);
						if(!strcasecmp(p_key_value->key,"Hostname"))
							s_pvm_conf.s_audio_merger.p_hostname=f_skip_space(p_key_value->value,p_localhost);
						else if(!strcasecmp(p_key_value->key,"BuildOnlyBatchMergeList"))
							s_pvm_conf.s_audio_merger.s_build_only_list=(atoi(p_key_value->value)>1||atoi(p_key_value->value)<0)?1:atoi(p_key_value->value);
						else
						{
							fprintf(stderr,"(%s) invalid section %s parameter %s value %s\n",__FILE__,p_section->name,p_key_value->key,p_key_value->value);
							return(f_pvm_parser(NULL,"close"));
						}
					}
				}
				else if(!strcasecmp(p_section->name,"VideoMerger"))
				{
					s_check_func|=MERGER_MASK;
					s_pvm_conf.s_video_merger.p_hostname=p_localhost;	/*this is the default*/
					for (p_key_value=p_section->keyvalue;p_key_value!=NULL;p_key_value=p_key_value->next)
					{
						if (verbose & TC_DEBUG)	//debug
							fprintf(stderr,"(%s) Section %s key %s value %s\n",__FILE__,p_section->name,p_key_value->key,p_key_value->value);
						if(!strcasecmp(p_key_value->key,"Hostname"))
							s_pvm_conf.s_video_merger.p_hostname=f_skip_space(p_key_value->value,NULL);
						else if(!strcasecmp(p_key_value->key,"BuildOnlyBatchMergeList"))
							s_pvm_conf.s_video_merger.s_build_only_list=(atoi(p_key_value->value)>1||atoi(p_key_value->value)<0)?1:atoi(p_key_value->value);
						else
						{
							fprintf(stderr,"(%s) invalid section %s parameter %s value %s\n",__FILE__,p_section->name,p_key_value->key,p_key_value->value);
							return(f_pvm_parser(NULL,"close"));
						}
					}
				}
				else if(!strcasecmp(p_section->name,"ExportAudioModule"))
				{
					s_check_func|=EXPORTMODULE_MASK;
					for (p_key_value=p_section->keyvalue;p_key_value!=NULL;p_key_value=p_key_value->next)
					{
						if (verbose & TC_DEBUG)	//debug
							fprintf(stderr,"(%s) Section %s key %s value %s\n",__FILE__,p_section->name,p_key_value->key,p_key_value->value);
						if(!strcasecmp(p_key_value->key,"Codec"))
							s_pvm_conf.s_audio_codec.p_codec=f_skip_space(p_key_value->value,NULL);
						else if(!strcasecmp(p_key_value->key,"Param1"))
							s_pvm_conf.s_audio_codec.p_par1=f_skip_space(p_key_value->value,NULL);
						else if(!strcasecmp(p_key_value->key,"Param2"))
							s_pvm_conf.s_audio_codec.p_par2=f_skip_space(p_key_value->value,NULL);
						else if(!strcasecmp(p_key_value->key,"Param3"))
							s_pvm_conf.s_audio_codec.p_par3=f_skip_space(p_key_value->value,NULL);
						else
						{
							fprintf(stderr,"(%s) invalid section %s parameter %s value %s\n",__FILE__,p_section->name,p_key_value->key,p_key_value->value);
							return(f_pvm_parser(NULL,"close"));
						}
					}
				}
				else if(!strcasecmp(p_section->name,"ExportVideoModule"))
				{
					s_check_func|=EXPORTMODULE_MASK;
					for (p_key_value=p_section->keyvalue;p_key_value!=NULL;p_key_value=p_key_value->next)
					{
						if (verbose & TC_DEBUG)	//debug
							fprintf(stderr,"(%s) Section %s key %s value %s\n",__FILE__,p_section->name,p_key_value->key,p_key_value->value);
						if(!strcasecmp(p_key_value->key,"Codec"))
							s_pvm_conf.s_video_codec.p_codec=f_skip_space(p_key_value->value,NULL);
						else if(!strcasecmp(p_key_value->key,"Param1"))
							s_pvm_conf.s_video_codec.p_par1=f_skip_space(p_key_value->value,NULL);
						else if(!strcasecmp(p_key_value->key,"Param2"))
							s_pvm_conf.s_video_codec.p_par2=f_skip_space(p_key_value->value,NULL);
						else if(!strcasecmp(p_key_value->key,"Param3"))
							s_pvm_conf.s_video_codec.p_par3=f_skip_space(p_key_value->value,NULL);
						else
						{
							fprintf(stderr,"(%s) invalid section %s parameter %s value %s\n",__FILE__,p_section->name,p_key_value->key,p_key_value->value);
							return(f_pvm_parser(NULL,"close"));
						}
					}
				}
				else if ((!strcasecmp(p_section->name,"LogAudioList"))||(!strcasecmp(p_section->name,"LogVideoList")))
				{
					if (!strcasecmp(p_section->name,"LogVideoList"))
						s_list_type=TC_VIDEO;
					else
						s_list_type=TC_AUDIO;
					for (p_key_value=p_section->keyvalue;p_key_value!=NULL;p_key_value=p_key_value->next)
					{
						if (verbose & TC_DEBUG)	//debug
							fprintf(stderr,"(%s) Section %s key %s value %s\n",__FILE__,p_section->name,p_key_value->key,p_key_value->value);
						if (p_pvm_conf_file_loglist==NULL)
						{
							p_pvm_conf_file_loglist=(pvm_config_filelist *)calloc(sizeof(pvm_config_filelist),1);
							s_pvm_conf.p_add_loglist=p_pvm_conf_file_loglist;
						}
						else
						{
							p_pvm_conf_file_loglist->p_next=(pvm_config_filelist *)calloc(sizeof(pvm_config_filelist),1);
							p_pvm_conf_file_loglist=p_pvm_conf_file_loglist->p_next;
						}
						p_pvm_conf_file_loglist->p_filename=f_skip_space(p_key_value->value,NULL);
						p_pvm_conf_file_loglist->s_type=s_list_type;
					}
				}
				else if ((!strcasecmp(p_section->name,"AddVideoList"))||(!strcasecmp(p_section->name,"AddAudioList")))
				{
					s_check_func|=ADDLIST_MASK;
					s_tmp=1;
					if (!strcasecmp(p_section->name,"AddVideoList"))
						s_list_type=TC_VIDEO;
					else
						s_list_type=TC_AUDIO;
					for (p_key_value=p_section->keyvalue;p_key_value!=NULL;p_key_value=p_key_value->next)
					{
						if (verbose & TC_DEBUG)	//debug
							fprintf(stderr,"(%s) Section %s key %s value %s\n",__FILE__,p_section->name,p_key_value->key,p_key_value->value);
						if (s_tmp)
						{
							s_tmp=0;
							if (p_pvm_conf_fileadd==NULL)
							{
								p_pvm_conf_fileadd=(pvm_config_filelist *)calloc(sizeof(pvm_config_filelist),1);
								s_pvm_conf.p_add_list=p_pvm_conf_fileadd;
							}
							else
							{
								p_pvm_conf_fileadd->p_next=(pvm_config_filelist *)calloc(sizeof(pvm_config_filelist),1);
								p_pvm_conf_fileadd=p_pvm_conf_fileadd->p_next;
							}
						}
						if(!strcasecmp(p_key_value->key,"Destination"))
							s_pvm_conf.p_add_list->p_destination=f_skip_space(p_key_value->value,NULL);
						else if(!strcasecmp(p_key_value->key,"Codec"))
							s_pvm_conf.p_add_list->p_codec=f_skip_space(p_key_value->value,NULL);
						else /*no name*/
						{
							s_tmp=1;	/*need to create a new node*/
							p_pvm_conf_fileadd->p_filename=f_skip_space(p_key_value->value,NULL);
							p_pvm_conf_fileadd->s_type=s_list_type;
						}
					}
				}
				else if ((!strcasecmp(p_section->name,"RemoveVideoList"))||(!strcasecmp(p_section->name,"RemoveAudioList")))
				{
					s_check_func|=REMOVELIST_MASK;
					if (!strcasecmp(p_section->name,"RemoveVideoList"))
						s_list_type=TC_VIDEO;
					else
						s_list_type=TC_AUDIO;
					for (p_key_value=p_section->keyvalue;p_key_value!=NULL;p_key_value=p_key_value->next)
					{
						if (verbose & TC_DEBUG)	//debug
							fprintf(stderr,"(%s) Section %s key %s value %s\n",__FILE__,p_section->name,p_key_value->key,p_key_value->value);
						if (p_pvm_conf_filerem==NULL)
						{
							p_pvm_conf_filerem=(pvm_config_filelist *)calloc(sizeof(pvm_config_filelist),1);
							s_pvm_conf.p_rem_list=p_pvm_conf_filerem;
						}
						else
						{
							p_pvm_conf_filerem->p_next=(pvm_config_filelist *)calloc(sizeof(pvm_config_filelist),1);
							p_pvm_conf_filerem=p_pvm_conf_filerem->p_next;
						}
						p_pvm_conf_filerem->p_filename=f_skip_space(p_key_value->value,NULL);
						p_pvm_conf_filerem->s_type=s_list_type;
					}
				}
				else if (!strcasecmp(p_section->name,"SystemList"))
				{
					s_check_func|=SYSTEMLIST_MASK;
					for (p_key_value=p_section->keyvalue;p_key_value!=NULL;p_key_value=p_key_value->next)
					{
						if (verbose & TC_DEBUG)	//debug
							fprintf(stderr,"(%s) Section %s key %s value %s\n",__FILE__,p_section->name,p_key_value->key,p_key_value->value);
						if(!strcasecmp(p_key_value->key,"Codec"))
							s_pvm_conf.s_sys_list.p_codec=f_skip_space(p_key_value->value,NULL);
						else if(!strcasecmp(p_key_value->key,"Destination"))
							s_pvm_conf.s_sys_list.p_destination=f_skip_space(p_key_value->value,NULL);
						else if(!strcasecmp(p_key_value->key,"MultiplexParams"))
							s_pvm_conf.p_multiplex_cmd=f_skip_space(p_key_value->value,NULL);
						else if(!strcasecmp(p_key_value->key,"BuildOnlyIntermediateFile"))
							s_pvm_conf.s_build_intermed_file=(atoi(p_key_value->value)<0||atoi(p_key_value->value)>1)?0:atoi(p_key_value->value);
						else
						{
							fprintf(stderr,"(%s) invalid section %s parameter %s value %s\n",__FILE__,p_section->name,p_key_value->key,p_key_value->value);
							return(f_pvm_parser(NULL,"close"));
						}
					}
				}
				else
				{
					fprintf(stderr,"(%s) Invalid [%s] section \n",__FILE__,p_section->name);
					return(f_pvm_parser(NULL,"close"));
				}

			} while((p_section=cf_get_next_section(p_root,p_section)) != NULL);
			if (((s_pvm_conf.s_audio_codec.p_codec ==NULL)&&(s_pvm_conf.s_video_codec.p_codec ==NULL))&&(s_check_func&EXPORTMODULE_MASK))
			{
				fprintf(stderr,"(%s) Need at least Codec parameter in the [ExportVideoModule] or [ExportAudioModule] section \n",__FILE__);
				return(f_pvm_parser(NULL,"close"));
			}
			if ((s_pvm_conf.s_system_merger.p_hostname!=NULL)&&(s_pvm_conf.p_multiplex_cmd==NULL))
			{
				fprintf(stderr,"(%s) MultiplexParams parameter required in the [SystemMerger] section \n",__FILE__);
				return(f_pvm_parser(NULL,"close"));
			}
			else if (s_pvm_conf.s_system_merger.p_hostname!=NULL)
			{
				s_pvm_conf.s_video_merger.s_build_only_list=1;
				s_pvm_conf.s_audio_merger.s_build_only_list=1;
			}
			if (s_pvm_conf.p_add_list!=NULL)
			{
				if ((s_pvm_conf.p_add_list->p_codec==NULL)&&(s_check_func&ADDLIST_MASK))
				{
					fprintf(stderr,"(%s) Need at least Codec parameter in the [AddList] section \n",__FILE__);
					return(f_pvm_parser(NULL,"close"));
				}
			}
		}
		return(&s_pvm_conf);
	}
	else if(!strcasecmp(p_option,"close"))
	{
		for(p_pvm_conf_host=s_pvm_conf.p_pvm_hosts;p_pvm_conf_host!=NULL;)
		{
			p_tmp=p_pvm_conf_host->p_next;
			free(p_pvm_conf_host);
			p_pvm_conf_host=p_tmp;
		}
		for(p_pvm_conf_fileadd=s_pvm_conf.p_add_list;p_pvm_conf_fileadd!=NULL;)
		{
			p_pvm_conf_filerem=p_pvm_conf_fileadd->p_next;
			free(p_pvm_conf_fileadd);
			p_pvm_conf_fileadd=p_pvm_conf_filerem;
		}
		for(p_pvm_conf_fileadd=s_pvm_conf.p_rem_list;p_pvm_conf_fileadd!=NULL;)
		{
			p_pvm_conf_filerem=p_pvm_conf_fileadd->p_next;
			free(p_pvm_conf_fileadd);
			p_pvm_conf_fileadd=p_pvm_conf_filerem;
		}
		memset((char *)&s_pvm_conf,'\0',sizeof(pvm_config_env));
		return(NULL);
	}
	return(NULL);
}
