/*
 *  tcxmlcheck.c
 *
 *  Copyright (C) Malanchini Marzio - March 2003
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

#include "transcode.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <math.h>

#include "ioxml.h"
#include "ioaux.h"
#include "magic.h"

#define EXE "tcxmlcheck"

#define MAX_BUF 1024
#define VIDEO_MODE 0x01
#define AUDIO_MODE 0x02

void version(void)
{
    /* print id string to stderr */
    fprintf(stderr, "%s (%s v%s) (C) 2001-2003 Thomas Oestreich\n",
                    EXE, PACKAGE, VERSION);
}

static void usage(int status)
{
	version();
	fprintf(stderr,"\nUsage: %s [options] [-]\n", EXE);
#ifdef HAVE_LIBXML2
	fprintf(stderr,"\t -i name        input video/audio xml file [stdin]\n");
	fprintf(stderr,"\t -p name        input audio xml file [none]\n");
	fprintf(stderr,"\t -B             binary output to stdout (used by transcode) [off]\n");
	fprintf(stderr,"\t -S             write stdin into shared memory (used by transcode)[off]\n");
	fprintf(stderr,"\t -V             check only video file input [off]\n");
	fprintf(stderr,"\t -A             check only audio file input [off]\n");
#endif
	fprintf(stderr,"\t -v             print version\n");

	exit(status);
}

#ifdef HAVE_LIBXML2

int binary_dump = 1;		//force to use the binary dump in probe_xml


/* ------------------------------------------------------------ 
 *
 * print a usage/version message
 *
 * ------------------------------------------------------------*/



static int f_complete_vob_info(vob_t *p_vob,int s_type_check)
{
	audiovideo_t s_audiovideo;
	int s_rc;

	s_rc=0;
	if ((s_type_check & VIDEO_MODE) !=0)
	{
		if( p_vob->video_in_file != NULL)
		{
			if( f_manage_input_xml(p_vob->video_in_file,1,&s_audiovideo))
			{
				fprintf(stderr,"(%s) Error parsing XML %s file\n",EXE,p_vob->video_in_file);
				(void) f_manage_input_xml(NULL,0,&s_audiovideo);
				return(1);
			}
			if (s_audiovideo.p_next->s_v_codec != TC_CODEC_UNKNOWN)
				p_vob->im_v_codec=s_audiovideo.p_next->s_v_codec;	
			if (s_audiovideo.p_next->s_a_codec != TC_CODEC_UNKNOWN)
				p_vob->im_a_codec=s_audiovideo.p_next->s_a_codec;	
			if ((s_audiovideo.p_next->s_v_tg_height !=0) || (s_audiovideo.p_next->s_v_tg_width !=0))
				s_rc=2;
			(void) f_manage_input_xml(NULL,0,&s_audiovideo);
		}
	}
	if ((s_type_check & AUDIO_MODE) !=0)
	{
		if( p_vob->audio_in_file != NULL) //there is an xml file for audio?
		{
			if( f_manage_input_xml(p_vob->audio_in_file,1,&s_audiovideo))
			{
				fprintf(stderr,"(%s) Error parsing XML %s file\n",EXE,p_vob->audio_in_file);
				(void) f_manage_input_xml(NULL,0,&s_audiovideo);
				return(1);
			}
			if (s_audiovideo.p_next->s_a_codec != TC_CODEC_UNKNOWN)
				p_vob->im_a_codec=s_audiovideo.p_next->s_a_codec;	
			(void) f_manage_input_xml(NULL,0,&s_audiovideo);
		}
	}
	return(s_rc);
}

/* ------------------------------------------------------------ 
 *
 * check the consistence of XML file 
 *
 * ------------------------------------------------------------*/

int main(int argc, char *argv[])
{

	vob_t s_vob,*p_vob;
	char *p_in_v_file="/dev/stdin",*p_in_a_file=NULL,*p_audio_tmp=NULL,*p_video_tmp=NULL;
	pid_t s_pid;
	int s_bin_dump=0,s_type_check=VIDEO_MODE|AUDIO_MODE,s_shmem=0;
	int s_shm,s_rc,s_cmd;
	key_t s_key=0x00112233;
	
	//proper initialization
	memset(&s_vob, 0, sizeof(vob_t));
	s_pid=getpid();
	
	
	s_vob.audio_in_file= "/dev/zero";
	s_vob.video_in_file= "/dev/zero";
	
	while ((s_cmd = getopt(argc, argv, "i:p:vSBAVh")) != -1) 
	{
		switch (s_cmd) 
		{
			case 'i': 
		  		if(optarg[0]=='-') 
			  		usage(EXIT_FAILURE);
		  		p_in_v_file = optarg;
		  	break;
			case 'p': 
		  		if(optarg[0]=='-') 
			  		usage(EXIT_FAILURE);
		  		p_in_a_file = optarg;
		  	break;
			case 'B':
		  		s_bin_dump = 1;
		  	break;
			case 'A':
		  		s_type_check = AUDIO_MODE;
		  	break;
			case 'V':
		  		s_type_check = VIDEO_MODE;
		  	break;
			case 'S':
		  		s_shmem = 1;
		  	break;
			case 'v': 
		  		version();
		  		exit(0);
		  	break;
			case 'h':
		  		usage(EXIT_SUCCESS);
			default:
				usage(EXIT_FAILURE);
		}
	}
	
	if(optind < argc) 
	{
		if(strcmp(argv[optind],"-")!=0) 
			usage(EXIT_FAILURE);
        }
	// need at least a file name
	if(argc==1) 
		usage(EXIT_FAILURE);
	/* ------------------------------------------------------------ 
	 *
	 * fill out defaults for info structure
	 *
	 * ------------------------------------------------------------*/
	
	if ((s_bin_dump) && (s_shmem))
	{
		if((s_shm = shmget(s_key, 0, 0600)) != -1)
			shmctl(s_shm, IPC_RMID, NULL);

		if((tc_pread(STDIN_FILENO, (uint8_t *) &s_vob, sizeof(vob_t))) != sizeof(vob_t))
		{
			fprintf(stderr,"(%s) Error reading data from stdin\n",EXE);
			exit(1);
		}
		if((s_shm=shmget(s_key,sizeof(vob_t),IPC_CREAT|0600)) == -1)
		{
			fprintf(stderr,"(%s) Cannot create shared memory segment\n",EXE);
			exit(1);
		}
		if ((p_vob=(vob_t *)shmat(s_shm,NULL,0)) == (vob_t *)-1)
		{
			shmctl( s_shm, IPC_RMID, NULL );
			(int) shmdt(p_vob);
			fprintf(stderr,"(%s) Cannot attach shared memory segment\n",EXE);
			exit(1);
		}
		ac_memcpy((char *)p_vob,(char *) &s_vob, sizeof(vob_t));
		(int) shmdt(p_vob);
		exit(0);
	}
	else if (s_bin_dump)
	{
		if((s_shm=shmget(s_key,sizeof(vob_t),0600)) == -1)
		{
			fprintf(stderr,"(%s) Cannot create shared memory segment: use -S option to create it\n",EXE);
			exit(1);
		}
		if ((p_vob=(vob_t *)shmat(s_shm,NULL,0)) == (vob_t *)-1)
		{
			shmctl( s_shm, IPC_RMID, NULL );
			(int) shmdt(p_vob);
			fprintf(stderr,"(%s) Cannot attach shared memory segment\n",EXE);
			exit(1);
		}
		ac_memcpy((char *)&s_vob,(char *) p_vob, sizeof(vob_t));
		shmctl( s_shm, IPC_RMID, NULL );
		(int) shmdt(p_vob);
		p_video_tmp=s_vob.video_in_file;
		p_audio_tmp=s_vob.audio_in_file;
	}
	if(p_in_v_file==NULL) 
	{
		s_vob.video_in_file= p_in_v_file;
		if(p_in_a_file==NULL) 
			s_vob.audio_in_file= p_in_a_file;
		else
			s_vob.audio_in_file= p_in_a_file;
	}
	else
	{
		s_vob.video_in_file= p_in_v_file;
		if(p_in_a_file==NULL) 
			s_vob.audio_in_file= p_in_v_file;
		else
			s_vob.audio_in_file= p_in_a_file;
	}
	
	
	/* ------------------------------------------------------------ 
	 *
	 * start with the program              
	 *
	 * ------------------------------------------------------------*/
	if ((s_rc=f_complete_vob_info(&s_vob,s_type_check)) == 1)
		return(1);
	else
	{
		if (s_bin_dump)
		{
			s_vob.video_in_file=p_video_tmp;
			s_vob.audio_in_file=p_audio_tmp;
			if(tc_pwrite(STDOUT_FILENO, (uint8_t *) &s_vob, sizeof(vob_t)) != sizeof(vob_t))
			{
				fprintf(stderr,"(%s) Error writing data to stdout\n",EXE);
				exit(1);
			}
			if(tc_pwrite(STDOUT_FILENO, (uint8_t *)&s_rc, sizeof(int)) != sizeof(int))
			{
				fprintf(stderr,"(%s) Error writing data to stdout\n",EXE);
				exit(1);
			}
		}
		return(0);
	}
}
#else
int main(int argc, char *argv[])
{
	usage(EXIT_SUCCESS);
}
#endif
