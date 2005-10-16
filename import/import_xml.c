/*
 *  import_xml.c
 *
 *  Copyright (C) Marzio Malanchini - March 2002
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

#define MOD_NAME    "import_xml.so"
#define MOD_VERSION "v0.0.8 (2003-07-09)"
#define MOD_CODEC   "(video) * | (audio) *"

#include "transcode.h"

static int verbose_flag = TC_QUIET;
static int capability_flag = -1;

#define MOD_PRE xml
#include "import_def.h"

#include "ioxml.h"
#include "magic.h"
#include "probe_xml.h"
#include "zoom.h"


#define M_AUDIOMAX(a,b)  (b==LONG_MAX)?LONG_MAX:a*b

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];
static FILE *s_fd_video=0;
static FILE *s_fd_audio=0;
static  audiovideo_t    s_audio,*p_audio=NULL;
static  audiovideo_t    s_video,*p_video=NULL,*p_video_prev;
static	int s_frame_size=0;
static char *p_vframe_buffer=NULL;
static	int s_v_codec;
static	long s_a_magic;
static	long s_v_magic;

int binary_dump=1;		//force the use of binary dump to create the correct XML tree



static int f_af6_sync(FILE *s_fd,char s_type)
{
	int s_skip;
	char s_buffer;

	s_skip=0;
	for (;;)
	{
		if(fread(&s_buffer, 1, 1, s_fd) !=1)
			return(TC_IMPORT_ERROR);
		if (s_buffer == 'T')
		{
			if(fread(&s_buffer, 1, 1, s_fd) !=1)
				return(TC_IMPORT_ERROR);
			if (s_buffer == 'a')
			{
				if(fread(&s_buffer, 1, 1, s_fd) !=1)
					return(TC_IMPORT_ERROR);
				if (s_buffer == 'f')
				{
					if(fread(&s_buffer, 1, 1, s_fd) !=1)
						return(TC_IMPORT_ERROR);
					if (s_buffer == '6')
						return(0);
					else
						s_skip++;
				}
				else
					s_skip++;
			}
			else
				s_skip++;
		}
		else
			s_skip++;
		if (s_skip > (1<<20))
		{
			if ( s_type == 'V' )
				tc_tag_warn(MOD_NAME, "no video af6 sync string found within 1024 kB of stream");
			else
				tc_tag_warn(MOD_NAME, "no audio af6 sync string found within 1024 kB of stream");
			return(TC_IMPORT_ERROR);
		}
	}
}


static int f_dim_check(audiovideo_t *p_temp,int *s_new_height,int *s_new_width)
{
	int s_rc;
	
	s_rc=0;
	if (p_temp->s_v_tg_width==0)
		*s_new_width=p_temp->s_v_width;
	else
	{
		s_rc=1;
		*s_new_width=p_temp->s_v_tg_width;
	}
	if (p_temp->s_v_tg_height==0)
		*s_new_height=p_temp->s_v_height;
	else
	{
		s_rc=1;
		*s_new_height=p_temp->s_v_tg_height;
	}
	return(s_rc);
}

static int f_calc_frame_size(audiovideo_t *p_temp,int s_codec)
{
	int s_new_height,s_new_width;
	
	if (f_dim_check(p_temp,&s_new_height,&s_new_width))
	{
		switch(s_codec) 
		{
			case CODEC_RGB:
				return(3*s_new_width*s_new_height);
			break;
			default:
				return((3*s_new_width*s_new_height)/2);
			break;
		}
	}
	return(s_frame_size);
}


static video_filter_t *f_video_filter(char *p_filter)
{
	static video_filter_t s_v_filter;

	if (p_filter !=NULL)
	{
		if(strcasecmp(p_filter,"bell")==0) 
		{
			s_v_filter.f_zoom_filter=Bell_filter;
			s_v_filter.s_zoom_support=Bell_support;
			s_v_filter.p_zoom_filter="Bell";
		}
		else if(strcasecmp(p_filter,"box")==0) 
		{
			s_v_filter.f_zoom_filter=Box_filter;
			s_v_filter.s_zoom_support=Box_support;
			s_v_filter.p_zoom_filter="Box";
		}
		else if(strncasecmp(p_filter,"mitchell",1)==0)
		{
			s_v_filter.f_zoom_filter=Mitchell_filter;
			s_v_filter.s_zoom_support=Mitchell_support;
			s_v_filter.p_zoom_filter="Mitchell";
		}
		else if(strncasecmp(p_filter,"hermite",1)==0)
		{
			s_v_filter.f_zoom_filter=Hermite_filter;
			s_v_filter.s_zoom_support=Hermite_support;
			s_v_filter.p_zoom_filter="Hermite";
		}
		else if(strncasecmp(p_filter,"B_spline",1)==0)
		{
			s_v_filter.f_zoom_filter=B_spline_filter;
			s_v_filter.s_zoom_support=B_spline_support;
			s_v_filter.p_zoom_filter="B_spline";
		}
		else if(strncasecmp(p_filter,"triangle",1)==0)
		{
			s_v_filter.f_zoom_filter=Triangle_filter;
			s_v_filter.s_zoom_support=Triangle_support;
			s_v_filter.p_zoom_filter="Triangle";
		}
		else //"lanczos3" default
		{
			s_v_filter.f_zoom_filter=Lanczos3_filter;
			s_v_filter.s_zoom_support=Lanczos3_support;
			s_v_filter.p_zoom_filter="Lanczos3";
		}
	}
	else //"lanczos3" default
	{
		s_v_filter.f_zoom_filter=Lanczos3_filter;
		s_v_filter.s_zoom_support=Lanczos3_support;
		s_v_filter.p_zoom_filter="Lanczos3";
	}
	return((video_filter_t *)&s_v_filter);

}

static void f_mod_video_frame(transfer_t *param,audiovideo_t *p_temp,int s_codec,int s_cleanup)
{
	static pixel_t *p_pixel_tmp=NULL;
	int s_new_height,s_new_width;
	image_t	s_src_image,s_dst_image;
	image_t	s_src_image_Y,s_dst_image_Y;
	image_t	s_src_image_UV,s_dst_image_UV;
	zoomer_t	*p_zoomer,*p_zoomer_Y,*p_zoomer_UV;
	static video_filter_t *p_v_filter;
	static audiovideo_t *p_tmp=NULL;
	

	if (s_cleanup)
	{
		if (p_pixel_tmp !=NULL)
			free(p_pixel_tmp);
		return;
	}
	if (f_dim_check(p_temp,&s_new_height,&s_new_width))
	{
		if (p_tmp != p_temp)
		{
			p_tmp=p_temp;
			p_v_filter=f_video_filter(p_temp->p_v_resize_filter);
			if(verbose_flag) 
				tc_tag_info(MOD_NAME,"setting resize video filter to %s",
						p_v_filter->p_zoom_filter);
		}
		switch(s_codec) 
		{
			case CODEC_RGB:
				if (p_pixel_tmp ==NULL)
					p_pixel_tmp = (pixel_t*)malloc((3*p_temp->s_v_tg_width * p_temp->s_v_tg_height));
				memset((char *)p_pixel_tmp,'\0',(3*p_temp->s_v_tg_width * p_temp->s_v_tg_height));
				zoom_setup_image(&s_src_image,p_temp->s_v_width,p_temp->s_v_height,3,(pixel_t *)p_vframe_buffer);
				zoom_setup_image(&s_dst_image,s_new_width,s_new_height,3,p_pixel_tmp);
				p_zoomer=zoom_image_init(&s_dst_image,&s_src_image,p_v_filter->f_zoom_filter,p_v_filter->s_zoom_support);
				s_src_image.data=p_vframe_buffer;
				s_dst_image.data=p_pixel_tmp;
				zoom_image_process(p_zoomer);
				s_src_image.data++;
				s_dst_image.data++;
				zoom_image_process(p_zoomer);
				s_src_image.data++;
				s_dst_image.data++;
				zoom_image_process(p_zoomer);
				zoom_image_done(p_zoomer);
			break;
			default:
				if (p_pixel_tmp ==NULL)
					p_pixel_tmp = (pixel_t*)malloc(((3*p_temp->s_v_tg_width * p_temp->s_v_tg_height)/2));
				memset((char *)p_pixel_tmp,'\0',((3*p_temp->s_v_tg_width * p_temp->s_v_tg_height)/2));
				zoom_setup_image(&s_src_image_Y,p_temp->s_v_width,p_temp->s_v_height,1,(pixel_t *)p_vframe_buffer);
				zoom_setup_image(&s_src_image_UV,(p_temp->s_v_width)/2,(p_temp->s_v_height)/2,1,(pixel_t *)(p_vframe_buffer+(p_temp->s_v_width*p_temp->s_v_height)));
				zoom_setup_image(&s_dst_image_Y,s_new_width,s_new_height,1,p_pixel_tmp);
				zoom_setup_image(&s_dst_image_UV,s_new_width/2,s_new_height/2,1,p_pixel_tmp+(s_new_width*s_new_height));
				p_zoomer_Y=zoom_image_init(&s_dst_image_Y,&s_src_image_Y,p_v_filter->f_zoom_filter,p_v_filter->s_zoom_support);
				p_zoomer_UV=zoom_image_init(&s_dst_image_UV,&s_src_image_UV,p_v_filter->f_zoom_filter,p_v_filter->s_zoom_support);
				s_src_image_Y.data=p_vframe_buffer;
				s_dst_image_Y.data=p_pixel_tmp;
				zoom_image_process(p_zoomer_Y);
				s_src_image_UV.data=p_vframe_buffer+(p_temp->s_v_width*p_temp->s_v_height);
				s_dst_image_UV.data=p_pixel_tmp+(s_new_width*s_new_height);
				zoom_image_process(p_zoomer_UV);
				s_src_image_UV.data=p_vframe_buffer+(p_temp->s_v_width*p_temp->s_v_height)+((p_temp->s_v_width*p_temp->s_v_height)>>2);
				s_dst_image_UV.data=p_pixel_tmp+(s_new_width*s_new_height)+((s_new_width*s_new_height)>>2);
				zoom_image_process(p_zoomer_UV);
				zoom_image_done(p_zoomer_Y);
				zoom_image_done(p_zoomer_UV);
			break;
		}
		ac_memcpy(param->buffer,p_pixel_tmp,param->size);	//copy the new image buffer
	}
	else
	{
		ac_memcpy(param->buffer,p_vframe_buffer,param->size);	//only copy the original buffer
	}
}


/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
	info_t	s_info_dummy;
	probe_info_t s_probe_dummy1,s_probe_dummy2;
	long s_tot_dummy1,s_tot_dummy2;
	int s_frame_audio_size=0;

	if(param->flag == TC_VIDEO) 
	{
		param->fd = NULL;
                if (p_video == NULL)
                {
			s_info_dummy.name=vob->video_in_file;	//init the video XML input file name
			s_info_dummy.verbose=vob->verbose;	//init the video XML input file name
                        if (f_build_xml_tree(&s_info_dummy,&s_video,&s_probe_dummy1,&s_probe_dummy2,&s_tot_dummy1,&s_tot_dummy2) == -1)	//create the XML tree
                        {
                                (int)f_manage_input_xml(NULL,0,&s_video);
                                tc_tag_warn(MOD_NAME,"file %s has invalid format content.", 
						vob->video_in_file);
				return(TC_IMPORT_ERROR);
                        }
                        p_video=s_video.p_next;
                }
		if (p_video == NULL)
		{
                        tc_tag_warn(MOD_NAME,"there isn't no file in  %s.", 
					vob->video_in_file);
			return(TC_IMPORT_ERROR);
		}
		if(p_video->s_v_codec == TC_CODEC_UNKNOWN) 
		{
			if (vob->dv_yuy2_mode == TC_TRUE)
		    		s_v_codec=CODEC_YUY2;
			else
				s_v_codec=vob->im_v_codec;
		}
		else
		{
			s_v_codec=p_video->s_v_codec;
		}
		s_v_magic=p_video->s_v_magic;
		switch(s_v_magic)
		{
		   case TC_MAGIC_DV_PAL:
		   case TC_MAGIC_DV_NTSC:
			capability_flag=TC_CAP_RGB|TC_CAP_YUV|TC_CAP_DV|TC_CAP_PCM;
			switch(s_v_codec) 
			{
				case CODEC_RGB:
					s_frame_size = 3*(p_video->s_v_width * p_video->s_v_height);
					if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -x dv -d %d -C %ld-%ld | tcdecode -x dv -y rgb -d %d -Q %d", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video,vob->verbose, vob->quality) < 0)
					{
						perror("command buffer overflow");
						return(TC_IMPORT_ERROR);
					}
				break;
				case CODEC_YUY2:
					s_frame_size = (3*(p_video->s_v_width * p_video->s_v_height))/2;
					if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -x dv -d %d -C %ld-%ld | tcdecode -x dv -y yuv420p -Y -d %d -Q %d", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video,vob->verbose, vob->quality) < 0)
					{
						perror("command buffer overflow");
						return(TC_IMPORT_ERROR);
					}
				break;
				case CODEC_YUV:
					s_frame_size = (3*(p_video->s_v_width * p_video->s_v_height))/2;
					if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -x dv -d %d -C %ld-%ld | tcdecode -x dv -y yuv420p -d %d -Q %d", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video,vob->verbose, vob->quality) < 0)
					{
						perror("command buffer overflow");
						return(TC_IMPORT_ERROR);
					}
				break;
				case CODEC_RAW:
				case CODEC_RAW_YUV:
					s_frame_size = (p_video->s_v_height==PAL_H) ? TC_FRAME_DV_PAL:TC_FRAME_DV_NTSC;
					if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -x dv -d %d -C %ld-%ld", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video) < 0)
					{
						perror("command buffer overflow");
						return(TC_IMPORT_ERROR);
					}
				break;
				default:
					tc_tag_warn(MOD_NAME, "invalid import codec request 0x%x", s_v_codec);
					return(TC_IMPORT_ERROR);
			}
		   break;
		   case TC_MAGIC_MOV:
			capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_YUV;
			switch(s_v_codec) 
			{
				case CODEC_RGB:
					s_frame_size = (3*(p_video->s_v_width * p_video->s_v_height));
					if (p_video->s_v_real_codec == TC_CODEC_DV)
					{
						if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcdecode -x mov -i \"%s\" -d %d -C %ld,%ld -Q %d|tcdecode -x dv -y rgb -d %d -Q %d", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video, vob->quality,vob->verbose,vob->quality) < 0)
						{
							perror("command buffer overflow");
							return(TC_IMPORT_ERROR);
						}
					}
					else
					{
						if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcdecode -x mov -y rgb -i \"%s\" -d %d -C %ld,%ld -Q %d", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video, vob->quality) < 0)
						{
							perror("command buffer overflow");
							return(TC_IMPORT_ERROR);
						}
					}
				break;
				case CODEC_YUV:
					s_frame_size = (3*(p_video->s_v_width * p_video->s_v_height))/2;
					if (p_video->s_v_real_codec == TC_CODEC_DV)
					{
						if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcdecode -x mov -i \"%s\" -d %d -C %ld,%ld -Q %d|tcdecode -x dv -y yuv420p -d %d -Q %d", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video, vob->quality,vob->verbose,vob->quality) < 0)
						{
							perror("command buffer overflow");
							return(TC_IMPORT_ERROR);
						}
					}
					else
					{
						if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcdecode -x mov -y yuv2 -i \"%s\" -d %d -C %ld,%ld -Q %d", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video, vob->quality) < 0)
						{
							perror("command buffer overflow");
							return(TC_IMPORT_ERROR);
						}
					}
				break;
				default:
					tc_tag_warn(MOD_NAME, "invalid import codec request 0x%x", s_v_codec);
					return(TC_IMPORT_ERROR);
			}
		   break;
		   case TC_MAGIC_AVI:
			capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_AUD|TC_CAP_VID;
			switch(s_v_codec) 
			{
				case CODEC_RGB:
				s_frame_size = (3*(p_video->s_v_width * p_video->s_v_height));
				if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -x avi -d %d -C %ld-%ld", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video) < 0)
				{
					perror("command buffer overflow");
					return(TC_IMPORT_ERROR);
				}
				break;
				default:
                       			tc_tag_warn(MOD_NAME,"video codec 0x%x not yet supported.", s_v_codec);
					return(TC_IMPORT_ERROR);
					;
			}
		   break;
		   case TC_MAGIC_AF6:
			capability_flag=TC_CAP_RGB|TC_CAP_YUV|TC_CAP_PCM;
			switch(s_v_codec) 
			{
				case CODEC_RGB:
				s_frame_size = (3*(p_video->s_v_width * p_video->s_v_height));
				if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcdecode -i \"%s\" -x af6video -y rgb -d %d -C %ld,%ld", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video) < 0)
				{
					perror("command buffer overflow");
					return(TC_IMPORT_ERROR);
				}
				break;
				case CODEC_YUV:
					s_frame_size = (3*(p_video->s_v_width * p_video->s_v_height))/2;
				if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcdecode -i \"%s\" -x af6video -y yuv420p -d %d -C %ld,%ld", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video) < 0)
				{
					perror("command buffer overflow");
					return(TC_IMPORT_ERROR);
				}
				break;
				default:
                       			tc_tag_warn(MOD_NAME,"video codec 0x%x not yet supported.", s_v_codec);
					return(TC_IMPORT_ERROR);
					;
			}
		  break;
		  default:
                       	tc_tag_warn(MOD_NAME,"video magic 0x%lx not yet supported.", s_v_magic);
			return(TC_IMPORT_ERROR);
		}
		if((s_fd_video = popen(import_cmd_buf, "r"))== NULL)
		{
			tc_tag_warn(MOD_NAME,"Error cannot open the pipe.");
			return(TC_IMPORT_ERROR);
		}
		param->size=f_calc_frame_size(p_video,s_v_codec);	//setting the frame size
		p_vframe_buffer=(char *)malloc(s_frame_size);
		if(verbose_flag) 
			tc_tag_info(MOD_NAME,"setting target video size to %d",param->size);
		p_video_prev=p_video;
		p_video=p_video->p_next;
		if(verbose_flag) 
			tc_tag_info(MOD_NAME, "%s", import_cmd_buf);
		return(0);
	}
	if(param->flag == TC_AUDIO) 
	{
		param->fd = NULL;
		if (p_audio== NULL)
		{
			if (vob->audio_in_file !=NULL)
				s_info_dummy.name=vob->audio_in_file;	//init the video XML input file name
			else
				s_info_dummy.name=vob->video_in_file;	//init the video XML input file name

			s_info_dummy.verbose=vob->verbose;	//init the video XML input file name
                        if (f_build_xml_tree(&s_info_dummy,&s_audio,&s_probe_dummy1,&s_probe_dummy2,&s_tot_dummy1,&s_tot_dummy2) == -1)	//create the XML tree
			{
				(int)f_manage_input_xml(NULL,0,&s_audio);
				tc_tag_warn(MOD_NAME,"file %s has invalid format content.", vob->audio_in_file);
				return(TC_IMPORT_ERROR);
			}
			p_audio=s_audio.p_next;
		}
		if (p_audio == NULL)
		{
                        tc_tag_warn(MOD_NAME,"there isn't no file in  %s.", vob->audio_in_file);
			return(TC_IMPORT_ERROR);
		}
		s_frame_audio_size=(1.00 * p_audio->s_a_bits * p_audio->s_a_chan * p_audio->s_a_rate)/(8*p_audio->s_fps);
		if(verbose_flag) 
			tc_tag_info(MOD_NAME,"setting audio size to %d",s_frame_audio_size);
		s_a_magic=p_audio->s_a_magic;
		switch(s_a_magic)
		{
		   case TC_MAGIC_DV_PAL:
		   case TC_MAGIC_DV_NTSC:
			capability_flag=TC_CAP_RGB|TC_CAP_YUV|TC_CAP_DV|TC_CAP_PCM;
			if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -d %d -x dv -C %ld-%ld | tcdecode -x dv -y pcm -d %d -Q %d", p_audio->p_nome_audio, vob->verbose,M_AUDIOMAX(s_frame_audio_size,p_audio->s_start_audio),M_AUDIOMAX(s_frame_audio_size,p_audio->s_end_audio),vob->verbose,vob->quality) < 0)
			{
				perror("command buffer overflow");
				return(TC_IMPORT_ERROR);
			}
		   break;
		   case TC_MAGIC_AVI:
			capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_AUD|TC_CAP_VID;
			if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -d %d -x pcm -a %d -C %ld-%ld",p_audio->p_nome_audio, vob->verbose,vob->a_track,M_AUDIOMAX(s_frame_audio_size,p_audio->s_start_audio),M_AUDIOMAX(s_frame_audio_size,p_audio->s_end_audio)) < 0)
			{
				perror("command buffer overflow");
				return(TC_IMPORT_ERROR);
			}
		   break;
		   case TC_MAGIC_MOV:
			capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_YUV;
			if (p_audio->s_a_bits == 16)
				s_frame_audio_size >>= 1;
			if (p_audio->s_a_chan == 2)
				s_frame_audio_size >>= 1;
			if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcdecode -i \"%s\" -d %d -x mov -y pcm -C %ld,%ld",p_audio->p_nome_audio, vob->verbose,M_AUDIOMAX(s_frame_audio_size,p_audio->s_start_audio),M_AUDIOMAX(s_frame_audio_size,p_audio->s_end_audio)) < 0)
			{
				perror("command buffer overflow");
				return(TC_IMPORT_ERROR);
			}
		   break;
		   case TC_MAGIC_AF6:
			capability_flag=TC_CAP_RGB|TC_CAP_YUV|TC_CAP_PCM;
			if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcdecode -i \"%s\" -d %d -x af6audio -y pcm -C %ld,%ld",p_audio->p_nome_audio, vob->verbose,M_AUDIOMAX(s_frame_audio_size,p_audio->s_start_audio),M_AUDIOMAX(s_frame_audio_size,p_audio->s_end_audio)) < 0)
			{
				perror("command buffer overflow");
				return(TC_IMPORT_ERROR);
			}
		   break;
		   default:
                        tc_tag_warn(MOD_NAME,"audio magic 0x%lx not yet supported.",s_a_magic);
			return(TC_IMPORT_ERROR);
		}
		if((s_fd_audio = popen(import_cmd_buf, "r"))== NULL)
		{
			tc_tag_warn(MOD_NAME,"Error cannot open the pipe.");
			return(TC_IMPORT_ERROR);
		}
		p_audio=p_audio->p_next;
		if(verbose_flag) 
			tc_tag_info(MOD_NAME, "%s", import_cmd_buf);
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
	static int s_v_af6_sync=0,s_a_af6_sync=0;
	int s_frame_audio_size=0;

	if(param->flag == TC_AUDIO) 
	{
                if (param->size < s_audio_frame_size_orig)
                {
                         param->size=s_audio_frame_size_orig;
                         s_audio_frame_size_orig=0;
                }
		if (s_a_magic == TC_MAGIC_AF6)
		{
			if (s_a_af6_sync == 0)
			{
				if (f_af6_sync(s_fd_audio,'A') == 0)
					s_a_af6_sync=1;
				else
					return(TC_IMPORT_ERROR);
			}
		}
                s_audio_frame_size=fread(param->buffer, 1, param->size, s_fd_audio);
                if (s_audio_frame_size == 0)
                {
                        if (p_audio != NULL)    // is there a file ?
                        {
				s_frame_audio_size=(1.00 * p_audio->s_a_bits * p_audio->s_a_chan * p_audio->s_a_rate)/(8*p_audio->s_fps);
				if(verbose_flag) 
					tc_tag_info(MOD_NAME,"setting audio size to %d",s_frame_audio_size);
				s_a_magic=p_audio->s_a_magic;
				switch(s_a_magic)
				{
				   case TC_MAGIC_DV_PAL:
				   case TC_MAGIC_DV_NTSC:
					if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -d %d -x dv -C %ld-%ld | tcdecode -x dv -y pcm -d %d -Q %d", p_audio->p_nome_audio, vob->verbose,M_AUDIOMAX(s_frame_audio_size,p_audio->s_start_audio),M_AUDIOMAX(s_frame_audio_size,p_audio->s_end_audio),vob->verbose,vob->quality) < 0)
                                        {
                                                perror("command buffer overflow");
                                                return(TC_IMPORT_ERROR);
                                        }
				   break;
				   case TC_MAGIC_AVI:
					if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -d %d -x pcm -a %d -C %ld-%ld",p_audio->p_nome_audio, vob->verbose,vob->a_track,M_AUDIOMAX(s_frame_audio_size,p_audio->s_start_audio),M_AUDIOMAX(s_frame_audio_size,p_audio->s_end_audio)) < 0)
					{
						perror("command buffer overflow");
						return(TC_IMPORT_ERROR);
					}
				   break;
		   		   case TC_MAGIC_MOV:
					if (p_audio->s_a_bits == 16)
						s_frame_audio_size >>= 1;
					if (p_audio->s_a_chan == 2)
						s_frame_audio_size >>= 1;
					if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcdecode -i \"%s\" -d %d -x mov -y pcm -C %ld,%ld",p_audio->p_nome_audio, vob->verbose,M_AUDIOMAX(s_frame_audio_size,p_audio->s_start_audio),M_AUDIOMAX(s_frame_audio_size,p_audio->s_end_audio)) < 0)
					{
						perror("command buffer overflow");
						return(TC_IMPORT_ERROR);
					}
		   		   break;
				   case TC_MAGIC_AF6:
					if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcdecode -i \"%s\" -d %d -x af6audio -y pcm -C %ld,%ld",p_audio->p_nome_audio, vob->verbose,M_AUDIOMAX(s_frame_audio_size,p_audio->s_start_audio),M_AUDIOMAX(s_frame_audio_size,p_audio->s_end_audio)) < 0)
					{
						perror("command buffer overflow");
						return(TC_IMPORT_ERROR);
					}
				   break;
				   default:
                        		tc_tag_warn(MOD_NAME,"audio magic 0x%lx not yet supported.",s_a_magic);
					return(TC_IMPORT_ERROR);
				}
                                if((s_fd_audio = popen(import_cmd_buf, "r"))== NULL)
                                {
                                        tc_tag_warn(MOD_NAME,"Error cannot open the pipe.");
                                        return(TC_IMPORT_ERROR);
                                }
				if(verbose_flag) 
					tc_tag_info(MOD_NAME, "%s", import_cmd_buf);
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
                if (s_frame_size < s_video_frame_size_orig)
                {
                         s_frame_size=s_video_frame_size_orig;
                         s_video_frame_size_orig=0;
                }
		if (s_v_magic == TC_MAGIC_AF6)
		{
			if (s_v_af6_sync == 0)
			{
				if (f_af6_sync(s_fd_video,'V') == 0)
					s_v_af6_sync=1;
				else
					return(TC_IMPORT_ERROR);
			}
		}
		s_video_frame_size=fread(p_vframe_buffer, 1,s_frame_size, s_fd_video);
		f_mod_video_frame(param,p_video_prev,s_v_codec,0);
		if (s_video_frame_size == 0)
		{
			if (p_video !=NULL)	// is there a file ?
			{
				if(p_video->s_v_codec == TC_CODEC_UNKNOWN) 
				{
					if (vob->dv_yuy2_mode == TC_TRUE)
		    				s_v_codec=CODEC_YUY2;
					else
						s_v_codec=vob->im_v_codec;
				}
				else
				{
				    	s_v_codec=p_video->s_v_codec;
				}
				s_v_magic=p_video->s_v_magic;
				switch(s_v_magic)
				{
		   		   case TC_MAGIC_DV_PAL:
		   		   case TC_MAGIC_DV_NTSC:
					switch(s_v_codec) 
					{
						case CODEC_RGB:
							s_frame_size = 3*(p_video->s_v_width * p_video->s_v_height);
							if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -x dv -d %d -C %ld-%ld | tcdecode -x dv -y rgb -d %d -Q %d", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video,vob->verbose, vob->quality) < 0)
							{
								perror("command buffer overflow");
								return(TC_IMPORT_ERROR);
							}
						break;
						case CODEC_YUY2:
							s_frame_size = (3*(p_video->s_v_width * p_video->s_v_height))/2;
							if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -x dv -d %d -C %ld-%ld | tcdecode -x dv -y yuv420p -Y -d %d -Q %d", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video,vob->verbose, vob->quality) < 0)
							{
								perror("command buffer overflow");
								return(TC_IMPORT_ERROR);
							}
						break;
						case CODEC_YUV:
							s_frame_size = (3*(p_video->s_v_width * p_video->s_v_height))/2;
							if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -x dv -d %d -C %ld-%ld | tcdecode -x dv -y yuv420p -d %d -Q %d", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video,vob->verbose, vob->quality) < 0)
							{
								perror("command buffer overflow");
								return(TC_IMPORT_ERROR);
							}
						break;
						case CODEC_RAW:
						case CODEC_RAW_YUV:
							s_frame_size = (p_video->s_v_height==PAL_H) ? TC_FRAME_DV_PAL:TC_FRAME_DV_NTSC;
							if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -x dv -d %d -C %ld-%ld", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video) < 0)
							{
								perror("command buffer overflow");
								return(TC_IMPORT_ERROR);
							}
						break;
						default:
							;;
					}
		   		   break;
		   		   case TC_MAGIC_MOV:
					switch(s_v_codec) 
					{
						case CODEC_RGB:
							s_frame_size = (3*(p_video->s_v_width * p_video->s_v_height));
							if (p_video->s_v_real_codec == TC_CODEC_DV)
							{
								if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcdecode -x mov -i \"%s\" -d %d -C %ld,%ld -Q %d|tcdecode -x dv -y rgb -d %d -Q %d", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video, vob->quality,vob->verbose,vob->quality) < 0)
								{
									perror("command buffer overflow");
									return(TC_IMPORT_ERROR);
								}
							}
							else
							{
								if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcdecode -x mov -y rgb -i \"%s\" -d %d -C %ld,%ld -Q %d", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video, vob->quality) < 0)
								{
									perror("command buffer overflow");
									return(TC_IMPORT_ERROR);
								}
							}
						break;
						case CODEC_YUV:
							s_frame_size = (3*(p_video->s_v_width * p_video->s_v_height))/2;
							if (p_video->s_v_real_codec == TC_CODEC_DV)
							{
								if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcdecode -x mov -i \"%s\" -d %d -C %ld,%ld -Q %d|tcdecode -x dv -y yuv420p -d %d -Q %d", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video, vob->quality,vob->verbose,vob->quality) < 0)
								{
									perror("command buffer overflow");
									return(TC_IMPORT_ERROR);
								}
							}
							else
							{
								if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcdecode -x mov -y yuv2 -i \"%s\" -d %d -C %ld,%ld -Q %d", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video, vob->quality) < 0)
								{
									perror("command buffer overflow");
									return(TC_IMPORT_ERROR);
								}
							}
						break;
						default:
							;;
					}
		   		   break;
		   		   case TC_MAGIC_AVI:
					switch(s_v_codec) 
					{
						case CODEC_RGB:
							s_frame_size = (3*(p_video->s_v_width * p_video->s_v_height));
							if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcextract -i \"%s\" -x avi -d %d -C %ld-%ld", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video) < 0)
							{
								perror("command buffer overflow");
								return(TC_IMPORT_ERROR);
							}
						break;
						default:
                        				tc_tag_warn(MOD_NAME,"video codec 0x%x not yet supported.",s_v_codec);
							return(TC_IMPORT_ERROR);
							;
					}
		   		   break;
				   case TC_MAGIC_AF6:
					switch(s_v_codec) 
					{
						case CODEC_RGB:
						s_frame_size = (3*(p_video->s_v_width * p_video->s_v_height));
						if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcdecode -i \"%s\" -x af6video -y rgb -d %d -C %ld,%ld", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video) < 0)
						{
							perror("command buffer overflow");
							return(TC_IMPORT_ERROR);
						}
						break;
						case CODEC_YUV:
							s_frame_size = (3*(p_video->s_v_width * p_video->s_v_height))/2;
						if(tc_snprintf(import_cmd_buf, MAX_BUF, "tcdecode -i \"%s\" -x af6video -y yuv420p -d %d -C %ld,%ld", p_video->p_nome_video,vob->verbose,p_video->s_start_video,p_video->s_end_video) < 0)
						{
							perror("command buffer overflow");
							return(TC_IMPORT_ERROR);
						}
						break;
						default:
							tc_tag_warn(MOD_NAME,"video codec 0x%x not yet supported.",s_v_codec);
							return(TC_IMPORT_ERROR);
							;
					}
				   break;
				   default:
                        		tc_tag_warn(MOD_NAME,"video magic 0x%lx not yet supported.",s_v_magic);
					return(TC_IMPORT_ERROR);
				}
                       		if((s_fd_video = popen(import_cmd_buf, "r"))== NULL)
                               	{
                                	tc_tag_warn(MOD_NAME,"Error cannot open the pipe.");
     		                 	return(TC_IMPORT_ERROR);
               		        }
				param->size=f_calc_frame_size(p_video,s_v_codec);	//setting the frame size
				if(verbose_flag) 
					tc_tag_info(MOD_NAME,"setting target video size to %d",param->size);
				p_video_prev=p_video;
                       		p_video=p_video->p_next;
				if(verbose_flag) 
					tc_tag_info(MOD_NAME, "%s", import_cmd_buf);
			}
			else
			{
				return(TC_IMPORT_ERROR);
			}
			s_video_frame_size=fread(p_vframe_buffer, 1,s_frame_size, s_fd_video);	//read the new frame
			f_mod_video_frame(param,p_video_prev,s_v_codec,0);
		}
                if (s_frame_size > s_video_frame_size)
                {
                        s_video_frame_size_orig=s_frame_size;
                        s_frame_size=s_video_frame_size;
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
		f_mod_video_frame(NULL,NULL,0,1); //cleanup
		s_fd_video=0;
		param->fd=NULL;	
		return(0);
	}
	return(TC_IMPORT_ERROR);
}



