/*
 *  decode_mov.c
 *
 *  Copyright (C) Malanchini Marzio - April 2003
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
#include <string.h>
#include <sys/errno.h>
#include <errno.h>
#include <unistd.h>

#include "config.h"

#ifdef HAVE_QT
#include <quicktime.h>
#endif

#include "transcode.h"
#include "ioaux.h"
#include "magic.h"


/* ------------------------------------------------------------ 
 *
 * decoder thread
 *
 * ------------------------------------------------------------*/


#ifdef HAVE_QT
void decode_mov(info_t *ipipe)
{

	quicktime_t *p_qt_structure=NULL;
	unsigned char **p_raw_buffer;
	char *p_v_codec=NULL,*p_a_codec=NULL,*p_buffer=NULL,*p_tmp=NULL;
	int s_width=0,s_height=0,s_channel=0,s_bits=0,s_buff_size=0,s_audio_size=0,s_video_size=0,s_sample=0;
	int s_cont,s_frames;
	double s_fps=0;
	long s_audio_rate,s_qt_pos;
	short *p_mask1,*p_mask2;


	if((p_qt_structure = quicktime_open(ipipe->name,1,0))== NULL)
	{
		fprintf(stderr,"(%s) error: can't open quicktime!\n",__FILE__);
		ipipe->error=1;
		import_exit(1);
	}
	quicktime_set_preload(p_qt_structure,10240000);
	s_fps=quicktime_frame_rate(p_qt_structure, 0);
	if(ipipe->format == TC_CODEC_PCM) 
	{
    		if (quicktime_audio_tracks(p_qt_structure) == 0)
		{
			quicktime_close(p_qt_structure);
      			fprintf(stderr,"(%s) error: no audio track in quicktime found!\n",__FILE__);
			ipipe->error=1;
			import_exit(1);
		}
		s_channel=quicktime_track_channels(p_qt_structure, 0);
		s_audio_rate=quicktime_sample_rate(p_qt_structure, 0);
		s_bits=quicktime_audio_bits(p_qt_structure, 0);
		s_audio_size=quicktime_audio_length(p_qt_structure,0);
		p_a_codec=quicktime_audio_compressor(p_qt_structure, 0);
	        if (ipipe->frame_limit[1] < s_audio_size)
                	s_audio_size=ipipe->frame_limit[1] - ipipe->frame_limit[0];
		else
                	s_audio_size-= ipipe->frame_limit[0];
		if (ipipe->verbose)
    			fprintf(stderr, "(%s) Audio codec=%s, rate=%ld Hz, bits=%d, channels=%d\n", __FILE__, p_a_codec, s_audio_rate, s_bits, s_channel);
		if((s_bits!=8)&&(s_bits!=16)) 
		{
			quicktime_close(p_qt_structure);
      			fprintf(stderr,"(%s) error: unsupported %d bit rate in quicktime !\n",__FILE__,s_bits);
			ipipe->error=1;
			import_exit(1);
		}
		if(s_channel > 2) 
		{
			quicktime_close(p_qt_structure);
      			fprintf(stderr,"(%s) error: too many audio tracks (%d) found in quicktime !\n",__FILE__,s_channel);
			ipipe->error=1;
			import_exit(1);
		}
		if(strlen(p_a_codec)==0) 
		{
			quicktime_close(p_qt_structure);
      			fprintf(stderr,"(%s) error: unsupported codec (empty!) in quicktime !\n",__FILE__);
			ipipe->error=1;
			import_exit(1);
		}
		if(quicktime_supported_audio(p_qt_structure, 0)!=0) 
		{
			s_qt_pos=quicktime_audio_position(p_qt_structure,0);
			s_sample=(1.00 * s_channel * s_bits *s_audio_rate)/(s_fps*8);
			s_buff_size=s_sample * sizeof(short);
			p_buffer=(char *)malloc(s_buff_size);
			if(s_bits==16)
				s_sample >>= 1;
			if(s_channel==1) 
			{
				p_mask1=(short *)p_buffer;
				quicktime_set_audio_position(p_qt_structure,s_qt_pos+ipipe->frame_limit[0],0);
				for (;s_audio_size>0;s_audio_size-=s_sample)
				{
					if ( quicktime_decode_audio(p_qt_structure,p_mask1, NULL,s_sample, 0) <0 )
					{
						quicktime_close(p_qt_structure);
						fprintf(stderr,"(%s) error: reading quicktime audio frame\n",__FILE__);
						ipipe->error=1;
						import_exit(1);
					}
					p_write (ipipe->fd_out, p_buffer, s_buff_size);
				}
			}
      			else 
			{
				s_sample >>= 1;
				p_mask1=(short *)p_buffer;
				p_mask2=(short *)malloc(s_sample * sizeof(short));
				s_qt_pos+=ipipe->frame_limit[0];
				quicktime_set_audio_position(p_qt_structure,s_qt_pos,0);
				for (;s_audio_size>0;s_audio_size-=s_sample)
				{
					if ( quicktime_decode_audio(p_qt_structure,p_mask1, NULL,s_sample, 0) <0 )
					{
						quicktime_close(p_qt_structure);
						fprintf(stderr,"(%s) error: reading quicktime audio frame\n",__FILE__);
						ipipe->error=1;
						import_exit(1);
					}
					quicktime_set_audio_position(p_qt_structure,s_qt_pos,0);
					if ( quicktime_decode_audio(p_qt_structure,p_mask2, NULL,s_sample, 1) <0 )
					{
						quicktime_close(p_qt_structure);
						fprintf(stderr,"(%s) error: reading quicktime audio frame\n",__FILE__);
						ipipe->error=1;
						import_exit(1);
					}
					for (s_cont=s_sample-1;s_cont>=0;s_cont--)
						p_mask1[s_cont<<1]= p_mask1[s_cont];
					for (s_cont=0;s_cont<s_sample;s_cont++)
						p_mask1[1+(s_cont<<1)]= p_mask2[s_cont];
					s_qt_pos+=s_sample;
					p_write (ipipe->fd_out, p_buffer, s_buff_size>>1);
				}
				free(p_mask2);
			}
			free(p_buffer);
		} 
		else if((strcasecmp(p_a_codec,QUICKTIME_RAW)==0) || (strcasecmp(p_a_codec,QUICKTIME_TWOS)==0)) 
		{
			s_sample=(1.00 * s_channel * s_bits *s_audio_rate)/(s_fps*8);
			s_buff_size=s_sample * sizeof(short);
			p_buffer=(char *)malloc(s_buff_size);
			s_qt_pos=quicktime_audio_position(p_qt_structure,0);
			quicktime_set_audio_position(p_qt_structure,s_qt_pos+ipipe->frame_limit[0],0);
			for (;s_audio_size>0;s_audio_size-=s_buff_size)
			{
				if ( quicktime_read_audio(p_qt_structure,p_buffer, s_buff_size, 0) < 0)
				{
					quicktime_close(p_qt_structure);
					fprintf(stderr,"(%s) error: reading quicktime audio frame\n",__FILE__);
					ipipe->error=1;
					import_exit(1);
				}
				p_write (ipipe->fd_out, p_buffer, s_buff_size);
			}
			quicktime_close(p_qt_structure);
			free(p_buffer);
		}
		else 
		{
			quicktime_close(p_qt_structure);
			fprintf(stderr,"(%s) error: quicktime audio codec '%s' not supported!\n",__FILE__,p_a_codec);
			ipipe->error=1;
			import_exit(1);
		}
	}
	else
	{
    		if (quicktime_video_tracks(p_qt_structure) == 0)
		{
			quicktime_close(p_qt_structure);
      			fprintf(stderr,"(%s) error: no video track in quicktime found!\n",__FILE__);
			ipipe->error=1;
			import_exit(1);
		}
		if (strlen((p_v_codec=quicktime_video_compressor(p_qt_structure, 0))) == 0)
		{
			quicktime_close(p_qt_structure);
			fprintf(stderr,"(%s) error: quicktime video codec '%s' not supported!\n",__FILE__,p_v_codec);
			ipipe->error=1;
			import_exit(1);
		}
		s_width=quicktime_video_width(p_qt_structure, 0);
		s_height=quicktime_video_height(p_qt_structure, 0);    
		s_video_size=quicktime_video_length(p_qt_structure,0);
		s_frames=quicktime_video_length(p_qt_structure, 0);
	        if (ipipe->frame_limit[1] < s_frames)
                	s_video_size=ipipe->frame_limit[1] - ipipe->frame_limit[0];
		else
                	s_video_size-= ipipe->frame_limit[0];
		if (ipipe->verbose)
			fprintf(stderr, "(%s) Video codec=%s, fps=%6.3f, width=%d, height=%d\n",__FILE__, p_v_codec, s_fps, s_width, s_height);
		if(strcasecmp(p_v_codec,QUICKTIME_DV)==0)
		{
			if ( s_fps == 25.00 )
				s_buff_size=TC_FRAME_DV_PAL;
			else
				s_buff_size=TC_FRAME_DV_NTSC;
			if ((p_buffer=(char *)malloc(s_buff_size)) == NULL)
			{
				quicktime_close(p_qt_structure);
				fprintf(stderr,"(%s) error: can't allocate buffer\n",__FILE__);
				ipipe->error=1;
				import_exit(1);
			}
			s_qt_pos=quicktime_video_position(p_qt_structure,0);
			quicktime_set_video_position(p_qt_structure,s_qt_pos+ipipe->frame_limit[0],0);
			for (s_cont=0;s_cont<s_video_size;s_cont++)
			{
      				if(quicktime_read_frame(p_qt_structure,p_buffer,0)<0) 
				{
					free(p_buffer);
					quicktime_close(p_qt_structure);
					fprintf(stderr,"(%s) error: reading quicktime video frame\n",__FILE__);
					ipipe->error=1;
					import_exit(1);
      				}
				p_write (ipipe->fd_out, p_buffer, s_buff_size);
      			}
		}
		else if(ipipe->format == TC_CODEC_RGB)
		{
      			if(quicktime_supported_video(p_qt_structure,0)==0) 
			{
				quicktime_close(p_qt_structure);
				fprintf(stderr,"(%s) error: quicktime video codec '%s' not supported for RGB\n",__FILE__,p_v_codec);
				ipipe->error=1;
				import_exit(1);
			}
      			if ((p_raw_buffer = malloc(s_height*sizeof(char *))) ==NULL)
			{
				quicktime_close(p_qt_structure);
				fprintf(stderr,"(%s) error: can't allocate row pointers\n",__FILE__);
				ipipe->error=1;
				import_exit(1);
			}
			s_buff_size=3 * s_height * s_width;
			if ((p_buffer=(char *)malloc(s_buff_size)) == NULL)
			{
				free(p_raw_buffer);
				quicktime_close(p_qt_structure);
				fprintf(stderr,"(%s) error: can't allocate rgb buffer\n",__FILE__);
				ipipe->error=1;
				import_exit(1);
			}
			p_tmp=p_buffer;
      			for(s_cont=0;s_cont<s_height;s_cont++) 
			{
				p_raw_buffer[s_cont] = p_tmp;
				p_tmp += s_width * 3;
      			}
			s_qt_pos=quicktime_video_position(p_qt_structure,0);
			quicktime_set_video_position(p_qt_structure,s_qt_pos+ipipe->frame_limit[0],0);
			for (s_cont=0;s_cont<s_video_size;s_cont++)
			{
      				if(quicktime_decode_video(p_qt_structure,p_raw_buffer,0)<0) 
				{
					free(p_raw_buffer);
					free(p_buffer);
					quicktime_close(p_qt_structure);
					fprintf(stderr,"(%s) error: reading quicktime video frame\n",__FILE__);
					ipipe->error=1;
					import_exit(1);
      				}
				p_write (ipipe->fd_out, p_buffer, s_buff_size);
      			}
			free(p_raw_buffer);
		}
		else if(ipipe->format == TC_CODEC_YUV2) 
		{
      			if((strcasecmp(p_v_codec,QUICKTIME_YUV4)!=0)&& (strcasecmp(p_v_codec,QUICKTIME_YUV420)!=0)) 
			{
				quicktime_close(p_qt_structure);
				fprintf(stderr, "(%s) error: quicktime video codec '%s' not suitable for YUV!\n", __FILE__,p_v_codec);
				ipipe->error=1;
				import_exit(1);
			}
			s_buff_size=(3 * s_height * s_width)/2;
			if ((p_buffer=(char *)malloc(s_buff_size)) == NULL)
			{
				quicktime_close(p_qt_structure);
				fprintf(stderr,"(%s) error: can't allocate rgb buffer\n",__FILE__);
				ipipe->error=1;
				import_exit(1);
			}
			s_qt_pos=quicktime_video_position(p_qt_structure,0);
			quicktime_set_video_position(p_qt_structure,s_qt_pos+ipipe->frame_limit[0],0);
			for (s_cont=0;s_cont<s_video_size;s_cont++)
			{
      				if(quicktime_read_frame(p_qt_structure,p_buffer,0)<0) 
				{
					free(p_buffer);
					quicktime_close(p_qt_structure);
					fprintf(stderr,"(%s) error: reading quicktime video frame\n",__FILE__);
					ipipe->error=1;
					import_exit(1);
      				}
				p_write (ipipe->fd_out, p_buffer, s_buff_size);
      			}
		}
		else
		{
			quicktime_close(p_qt_structure);
			fprintf(stderr,"(%s) error: unknown format mode (0x%x) \n",__FILE__,ipipe->format);
			ipipe->error=1;
			import_exit(1);
		}
		quicktime_close(p_qt_structure);
		free(p_buffer);
	}
	import_exit(0);
}
#else  
void decode_mov(info_t *ipipe)
{
	fprintf(stderr, "(%s) no support for Quicktime configured - exit.\n", __FILE__);
	ipipe->error=1;
	import_exit(1);
}
#endif
