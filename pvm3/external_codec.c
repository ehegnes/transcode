/*
 *  external_codec.c
 *
 *  Copyright (C) Marzio Malanchini - August 2003
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

#include <external_codec.h>
#include <stdlib.h>
#include <transcode.h>

#define MPEG2ENC_MP2ENC_PROG	"mplex"

int f_multiplexer(char *p_codec,char *p_merge_cmd,char *p_video_filename,char *p_audio_filename,char *p_dest_file,int s_verbose)
{
	char s_buffer[2*MAX_BUF];

	if(!strcasecmp(p_codec,"mpeg2enc-mp2enc"))
	{
		memset((char *)&s_buffer,'\0',2*MAX_BUF);
		snprintf((char *)&s_buffer,2*MAX_BUF,"%s %s -o %s %s %s",MPEG2ENC_MP2ENC_PROG,p_merge_cmd,p_dest_file,p_video_filename,p_audio_filename);
		if(s_verbose & TC_DEBUG)	
			fprintf(stderr,"(%s) multiplex cmd: %s\n",__FILE__,(char *)&s_buffer);
		(int)system((char *)&s_buffer);
		return(0);
	}
	else
		return(1);
}

char *f_external_suffix(char *p_codec,char *p_param)
{

	static char *p_suffix[]={".m1v\0",".m2v\0",".mpa\0",".mpeg\0",NULL};
	char s_var;
	if (p_param!=NULL)
	{
		if(!strcasecmp(p_codec,"mp2enc"))
		{
			return(p_suffix[2]);
		}
		else if(!strcasecmp(p_codec,"mpeg2enc-mp2enc"))
		{
			return(p_suffix[3]);
		}
		else if(!strcasecmp(p_codec,"mpeg2enc"))
		{
			s_var=p_param[0];
			if (p_param==NULL)	/*det the suffix*/
				return(p_suffix[0]);
			else if (strchr("1234568", tolower(s_var))==NULL)
				return(p_suffix[0]);
			else if (strchr("34568",tolower(s_var))!=NULL)
				return(p_suffix[1]);
			else
				return(p_suffix[0]);
		}
		else if(!strcasecmp(p_codec,"mpeg"))
		{
			s_var=p_param[0];
			if (p_param==NULL)	/*det the suffix*/
				return(p_suffix[0]);
			else if (strchr("1bvs2d", tolower(s_var))==NULL)
				return(p_suffix[0]);
			else if (strchr("1bv",tolower(s_var))!=NULL)
				return(p_suffix[0]);
			else
				return(p_suffix[1]);
		}
	}
	else
	{
		if(!strcasecmp(p_codec,"mp2enc"))
		{
			return(p_suffix[2]);
		}
		else if(!strcasecmp(p_codec,"mpeg2enc-mp2enc"))
		{
			return(p_suffix[3]);
		}
		else
			return(NULL);
	}
	return(NULL);
}
