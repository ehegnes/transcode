/*
 *  socket.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *                Written 2003 by Tilmann Bitterberg
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
 */
/*
 * This file opens a socket and lets transcode be configured over it.
 * use tcmodinfo -s FILE to connect
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "transcode.h"
#include "filter.h"
#include "socket.h"

//for the socket
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

extern char* socket_file; // transcode.c

unsigned int tc_socket_msgchar = 0;
char *tc_socket_submit_buf=NULL;
static int socket_fd=-1;

pthread_mutex_t tc_socket_msg_lock=PTHREAD_MUTEX_INITIALIZER;

int s_write (int sock, void *buf, size_t count) 
{
    int retval=0;
    do {
	if ( (retval += write(sock, buf, count)) < 0) {
	    perror ("write");
	    break;
	} else if (retval == 0) {
	    perror("write Ending connection");
	    break;
	}
    } while (retval<count);
    return retval;
}

int tc_socket_version(char *buf)
{
    sprintf(buf, "%s%s", VERSION, "\n");
    return 0;
}

#define P(field,fmt) \
    n = snprintf (buf, M_BUF_SIZE, "%20s = "fmt"\n", #field, vob->field); \
    s_write (socket_fd, buf, n)

int tc_socket_dump_vob(char *buf) 
{
    vob_t *vob = tc_get_vob();
    int n;
    P(vmod_probed, "%s");
    P(amod_probed, "%s");
    P(vmod_probed_xml, "%s");
    P(amod_probed_xml, "%s");
    P(verbose, "%d");
    P(video_in_file, "%s");
    P(audio_in_file, "%s");
    P(nav_seek_file, "%s");
    P(in_flag, "%d");
    P(has_audio, "%d");
    P(has_audio_track, "%d");
    P(has_video, "%d");
    P(lang_code, "%d");
    P(a_track, "%d");
    P(v_track, "%d");
    P(s_track, "%d");
    P(sync, "%d");
    P(sync_ms, "%d");
    P(dvd_title, "%d");
    P(dvd_chapter1, "%d");
    P(dvd_chapter2, "%d");
    P(dvd_max_chapters, "%d");
    P(dvd_angle, "%d");
    P(ps_unit, "%d");
    P(ps_seq1, "%d");
    P(ps_seq2, "%d");
    P(ts_pid1, "%d");
    P(ts_pid2, "%d");
    P(vob_offset, "%d");
    P(vob_chunk, "%d");
    P(vob_chunk_num1, "%d");
    P(vob_chunk_num2, "%d");
    P(vob_chunk_max, "%d");
    P(vob_percentage, "%d");
    P(vob_psu_num1, "%d");
    P(vob_psu_num2, "%d");
    P(vob_info_file, "%s");
    P(pts_start, "%f");
    P(psu_offset, "%f");
    P(demuxer, "%d");
    P(format_flag, "%ld");
    P(codec_flag, "%ld");
    P(quality, "%d");
    P(af6_mode, "%d");
    P(a_stream_bitrate, "%d");
    P(a_chan, "%d");
    P(a_bits, "%d");
    P(a_rate, "%d");
    P(a_padrate, "%d");
    P(im_a_size, "%d");
    P(ex_a_size, "%d");
    P(im_a_codec, "%d");
    P(fixme_a_codec, "%d");
    P(a_leap_frame, "%d");
    P(a_leap_bytes, "%d");
    P(a_vbr, "%d");
    P(a52_mode, "%d");
    P(dm_bits, "%d");
    P(dm_chan, "%d");
    P(v_stream_bitrate, "%d");
    P(fps, "%f");
    P(im_frc, "%d");
    P(ex_fps, "%f");
    P(ex_frc, "%d");
    P(hard_fps_flag, "%d");
    P(pulldown, "%d");
    P(im_v_height, "%d");
    P(im_v_width, "%d");
    P(im_v_size, "%d");
    P(v_bpp, "%d");
    P(im_asr, "%d");
    P(ex_asr, "%d");
    P(attributes, "%d");
    P(im_v_codec, "%d");
    P(encode_fields, "%d");
    P(dv_yuy2_mode, "%d");
    P(core_a_format, "%d");
    P(volume, "%f");
    P(ac3_gain[0], "%f");
    P(ac3_gain[1], "%f");
    P(ac3_gain[2], "%f");
    P(clip_count, "%d");
    P(core_v_format, "%d");
    P(ex_v_width, "%d");
    P(ex_v_height, "%d");
    P(ex_v_size, "%d");
    P(reduce_h, "%d");
    P(reduce_w, "%d");
    P(resize1_mult, "%d");
    P(vert_resize1, "%d");
    P(hori_resize1, "%d");
    P(resize2_mult, "%d");
    P(vert_resize2, "%d");
    P(hori_resize2, "%d");
    P(zoom_width, "%d");
    P(zoom_height, "%d");
    P(zoom_filter, "%p");
    P(zoom_support, "%f");
    P(antialias, "%d");
    P(deinterlace, "%d");
    P(decolor, "%d");
    P(aa_weight, "%f");
    P(aa_bias, "%f");
    P(gamma, "%f");
    P(ex_clip_top, "%d");
    P(ex_clip_bottom, "%d");
    P(ex_clip_left, "%d");
    P(ex_clip_right, "%d");
    P(im_clip_top, "%d");
    P(im_clip_bottom, "%d");
    P(im_clip_left, "%d");
    P(im_clip_right, "%d");
    P(post_ex_clip_top, "%d");
    P(post_ex_clip_bottom, "%d");
    P(post_ex_clip_left, "%d");
    P(post_ex_clip_right, "%d");
    P(pre_im_clip_top, "%d");
    P(pre_im_clip_bottom, "%d");
    P(pre_im_clip_left, "%d");
    P(pre_im_clip_right, "%d");
    P(video_out_file, "%s");
    P(audio_out_file, "%s");
    P(avifile_in, "%p");
    P(avifile_out, "%p");
    P(avi_comment_fd, "%d");
    P(out_flag, "%d");
    P(divxbitrate, "%d");
    P(divxkeyframes, "%d");
    P(divxquality, "%d");
    P(divxcrispness, "%d");
    P(divxmultipass, "%d");
    P(video_max_bitrate, "%d");
    P(divxlogfile, "%s");
    P(min_quantizer, "%d");
    P(max_quantizer, "%d");
    P(rc_period, "%d");
    P(rc_reaction_period, "%d");
    P(rc_reaction_ratio, "%d");
    P(divx5_vbv_prof, "%d");
    P(divx5_vbv_bitrate, "%d");
    P(divx5_vbv_size, "%d");
    P(divx5_vbv_occupancy, "%d");
    P(mp3bitrate, "%d");
    P(mp3frequency, "%d");
    P(mp3quality, "%f");
    P(mp3mode, "%d");
    P(bitreservoir, "%d");
    P(lame_preset, "%s");
    P(audiologfile, "%s");
    P(ex_a_codec, "%d");
    P(ex_v_codec, "%d");
    P(ex_v_fcc, "%s");
    P(ex_a_fcc, "%s");
    P(ex_profile_name, "%s");
    P(pass_flag, "%d");
    P(lame_flush, "%d");
    P(mod_path, "%s");
    P(ttime, "%p");
    P(ttime_current, "%d");
    P(frame_interval, "%d");
    P(chanid, "%d");
    P(station_id, "%s");
    P(im_v_string, "%s");
    P(im_a_string, "%s");
    P(ex_v_string, "%s");
    P(ex_a_string, "%s");
    P(accel, "%d");
    P(video_frames_delay, "%d");
    P(m2v_requant, "%f");

    return 0;
}
#undef P


int tc_socket_preview(char *buf)
{
    int filter_id;
    unsigned int arg = 0;
    char *c, *d;
    int ret = 0;

    // preview filter loaded?
    if ( (filter_id = plugin_find_id("pv")) < 0)
	filter_id = load_single_plugin("pv=cache=20");

    // it is loaded and we have its ID in filter_id

    c = strchr (buf, ' ');
    while (c && *c && *c == ' ')
	c++;

    if (!c)
	return 1;
    
    d = strchr (c, ' ');
    if (d) {
	arg = strtol(d, (char **)NULL, 0);
    }
    
    pthread_mutex_lock(&tc_socket_msg_lock);

    if        (!strncasecmp(c, "draw",    2)) {
	tc_socket_msgchar = TC_SOCK_PV_DRAW;
	TC_SOCK_SET_ARG (tc_socket_msgchar, arg);
    
    } else if (!strncasecmp(c, "pause",  2)) {
	tc_socket_msgchar = TC_SOCK_PV_PAUSE;
    
    } else if (!strncasecmp(c, "undo", 2)) {
	tc_socket_msgchar = TC_SOCK_PV_UNDO;
    
    } else if (!strncasecmp(c, "fastfw", 6)) {
	tc_socket_msgchar = TC_SOCK_PV_FAST_FW;
    
    } else if (!strncasecmp(c, "fastbw", 6)) {
	tc_socket_msgchar = TC_SOCK_PV_FAST_BW;
  
    } else if (!strncasecmp(c, "slowfw", 6)) {
	tc_socket_msgchar = TC_SOCK_PV_SLOW_FW;
	
    } else if (!strncasecmp(c, "slowbw", 6)) {
	tc_socket_msgchar = TC_SOCK_PV_SLOW_BW;
	
    } else if (!strncasecmp(c, "toggle", 6)) {
	tc_socket_msgchar = TC_SOCK_PV_TOGGLE;

    } else if (!strncasecmp(c, "slower", 6)) {
	tc_socket_msgchar = TC_SOCK_PV_SLOWER;

    } else if (!strncasecmp(c, "faster", 6)) {
	tc_socket_msgchar = TC_SOCK_PV_FASTER;
    
    } else if (!strncasecmp(c, "rotate", 6)) {
	tc_socket_msgchar = TC_SOCK_PV_ROTATE;
    
    } else if (!strncasecmp(c, "display", 6)) {
	tc_socket_msgchar = TC_SOCK_PV_DISPLAY;
    
    } else if (!strncasecmp(c, "grab", 4)) {
	tc_socket_msgchar = TC_SOCK_PV_SAVE_JPG;
    
    } else 
	ret = 1;

    pthread_mutex_unlock(&tc_socket_msg_lock);

    return ret;
}

int tc_socket_parameter(char *buf)
{
    char *c = buf, *d;
    int filter_id;

    c = strchr (buf, ' ');
    while (c && *c == ' ')
	c++;

    if (!c)
	return 1;

    if ( (filter_id = plugin_find_id(c)) < 0)
	return 1;

    memset (buf, 0, strlen(buf));

    if ((d = filter_single_readconf(filter_id)) == NULL)
	return 1;

    strcpy (buf, d);

    free (d);
    return 0;
}

int tc_socket_list(char *buf)
{
    char *c = buf;

    c = strchr (buf, ' ');
    while (c && *c == ' ') c++;

    if (!c)
	return 1;

    if        (!strncasecmp(c, "load",    2)) {
	memset (buf, 0, M_BUF_SIZE);
	plugin_list_loaded(buf);
    } else if (!strncasecmp(c, "enable",  2)) {
	memset (buf, 0, M_BUF_SIZE);
	plugin_list_enabled(buf);
    } else if (!strncasecmp(c, "disable", 2)) {
	memset (buf, 0, M_BUF_SIZE);
	plugin_list_disabled(buf);
    } else 
	return 1;

    return 0;

}
int tc_socket_config(char *buf)
{
    char *c = buf, *d;
    int filter_id;

    c = strchr (buf, ' ');
    while (c && *c && *c == ' ')
	c++;

    if (!c)
	return 1;

    d = c;
    c = strchr (d, ' ');
    while (c && *c && *c == ' ')
	*c++ = '\0';
    
    if (!c || !d)
	return 1;

    filter_id = plugin_find_id(d);

    if (filter_id < 0) {
	return 1;
    } else
	return filter_single_configure_handle(filter_id, c);
}

int tc_socket_disable(char *buf)
{
    char *c = buf;
    int filter_id;

    c = strchr (buf, ' ');
    while (c && *c == ' ')
	c++;

    filter_id = plugin_find_id(c);

    if (filter_id < 0) {
	return 1;
    } else
	return plugin_disable_id(filter_id);
}

int tc_socket_enable(char *buf)
{
    char *c = buf;
    int filter_id;

    c = strchr (buf, ' ');
    while (c && *c == ' ')
	c++;

    filter_id = plugin_find_id(c);

    if (filter_id < 0) {
	return 1;
    } else
	return plugin_enable_id(filter_id);
}

int tc_socket_load(char *buf)
{
    char *c = buf, *d;

    // skip "load "
    c = strchr (buf, ' ');

    // eat whitespace
    while (c && *c && *c == ' ')
	c++;
    
    if (!c)
	return 1;

    d = c;
    c = strchr (c, ' ');

    if (c && *c)  {
	*c = '=';
	c++;
	if (c && *c && *c=='0') 
	    *c = '\0';
    }

    return load_single_plugin(d);
}

int tc_socket_help(char *buf)
{
    sprintf(buf, "%s",
	    "load <filter> <initial string>\n"
	    "config <filter> <string>\n"
	    "parameters <filter>\n"
	    "quit\n"
	    "help\n"
	    "version\n"
	    "enable <filter>\n"
	    "disable <filter>\n"
	    "unload <filter>\n"
	    "progress\n"
	    "pause\n"
	    "dump\n"
	    "preview <command>\n"
	    "  [ draw | undo | pause | fastfw |\n"
	    "    slowfw | slowbw | rotate |\n"
	    "    rotate | display | slower |\n"
	    "    faster | toggle | grab ]\n"
	    "list [ load | enable | disable ]\n");

    return 0;
}

int tc_socket_handle(char *buf)
{
    int ret = TC_SOCK_FAILED;

    if        (!strncasecmp(buf, "help", 2)) {
	ret = TC_SOCK_HELP;
    } else if (!strncasecmp(buf, "load", 2)) {
	ret = TC_SOCK_LOAD;
    } else if (!strncasecmp(buf, "config", 2)) {
	ret = TC_SOCK_CONFIG;
    } else if (!strncasecmp(buf, "parameters", 3)) {
	ret = TC_SOCK_PARAMETER;
    } else if (!strncasecmp(buf, "quit", 2) || !strncasecmp(buf, "exit", 2)) {
	ret = TC_SOCK_QUIT;
    } else if (!strncasecmp(buf, "version", 2)) {
	ret = TC_SOCK_VERSION;
    } else if (!strncasecmp(buf, "enable", 2)) {
	ret = TC_SOCK_ENABLE;
    } else if (!strncasecmp(buf, "disable", 2)) {
	ret = TC_SOCK_DISABLE;
    } else if (!strncasecmp(buf, "unload", 2)) {
	ret = TC_SOCK_UNLOAD;
    } else if (!strncasecmp(buf, "list", 2)) {
	ret = TC_SOCK_LIST;
    } else if (!strncasecmp(buf, "preview", 3)) {
	ret = TC_SOCK_PREVIEW;
    } else if (!strncasecmp(buf, "progress", 3)) {
	ret = TC_SOCK_PROGRESS_METER;
    } else if (!strncasecmp(buf, "pause", 3)) {
	ret = TC_SOCK_PAUSE;
    } else if (!strncasecmp(buf, "dump", 3)) {
	ret = TC_SOCK_DUMP;
    } else {
	ret = TC_SOCK_FAILED;
    }

    return ret;
}

// allows printing to the socket from everywhere
void tc_socket_submit (char *buf)
{
    // this better be a queue, but up to now we only have one user
    // -- tibit
    int len = 0;

    // we may not be running
    if (socket_fd <= 0)
	return;

    if (!tc_socket_submit_buf) {
	if ( (tc_socket_submit_buf = (char *) malloc (M_BUF_SIZE)) == NULL) {
	  fprintf(stderr, "[%s] malloc for tc_socket_submit_buf failed : %s:%d\n", 
		  "socket server" , __FILE__, __LINE__);
	  return;
	}
    }
    len = strlen (buf);
    if (len >= M_BUF_SIZE) {
	len = M_BUF_SIZE;
    }

    strncpy (tc_socket_submit_buf, buf, len);

    s_write (socket_fd, tc_socket_submit_buf, len);
}

void socket_thread(void)
{
    int retval, msgsock;
    int thisfd;

    // too hard on the stack?
    char rbuf[M_BUF_SIZE];

    struct sockaddr_un server;

    unlink(socket_file);

    thisfd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (thisfd < 0) {
	perror("socket");
	return;
    }

    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, socket_file);

    if (bind(thisfd, (struct sockaddr *) &server, sizeof(struct sockaddr_un))) {
	perror("binding stream socket");
	return;
    } 

    //fprintf(stderr, "Socket has name %s, mypid (%d)\n", server.sun_path, getpid()); fflush(stderr);
    listen(thisfd, 5);

    while (1) {
	int ret;
	
	pthread_testcancel();

	socket_fd = msgsock = accept(thisfd, 0, 0);

	//printf("Connect!\n");
	if (msgsock == -1) {
	    perror("accept");
	    goto socket_out;
	}
	do {
	    pthread_testcancel();

	    memset(rbuf, 0, M_BUF_SIZE);

	    // We wait in this read() for data to become available
	    if ((retval = read(msgsock, rbuf, M_BUF_SIZE)) < 0) {
		perror("reading stream message");
		//goto socket_out;
		continue;
	    } else if (retval == 0)  {
		//printf("read Ending connection\n");
		break;
	    } else {
		rbuf[strlen(rbuf)-1] = '\0'; // chomp
		//fprintf(stderr, "[%s]: ->|%s|\n", "socket_read", rbuf);
	    }


	    switch (ret = tc_socket_handle(rbuf)) {

		case TC_SOCK_FAILED:
		    break;
		case TC_SOCK_HELP:
		    ret = !tc_socket_help(rbuf);
		    break;
		case TC_SOCK_VERSION:
		    ret = !tc_socket_version(rbuf);
		    break;
		case TC_SOCK_LOAD:
		    ret = !tc_socket_load(rbuf);
		    memset (rbuf, 0, M_BUF_SIZE);
		    break;
		case TC_SOCK_QUIT:
		    close (msgsock);
		    socket_fd = -1;
		    msgsock = 0;
		    break;
		case TC_SOCK_UNLOAD:
		    // implement filter unload
		    break;
		case TC_SOCK_LIST:
		    ret = !tc_socket_list(rbuf);
		    break;
		case TC_SOCK_ENABLE:
		    ret = !tc_socket_enable(rbuf);
		    memset (rbuf, 0, M_BUF_SIZE);
		    break;
		case TC_SOCK_DISABLE:
		    ret = !tc_socket_disable(rbuf);
		    memset (rbuf, 0, M_BUF_SIZE);
		    break;
		case TC_SOCK_CONFIG:
		    ret = !tc_socket_config(rbuf);
		    memset (rbuf, 0, M_BUF_SIZE);
		    break;
		case TC_SOCK_PARAMETER: 
		    ret = !tc_socket_parameter(rbuf);
		    break;
		case TC_SOCK_PREVIEW:
		    ret = !tc_socket_preview(rbuf);
		    memset (rbuf, 0, M_BUF_SIZE);
		    break;
		case TC_SOCK_PROGRESS_METER:
		    tc_progress_meter = !tc_progress_meter;
		    memset (rbuf, 0, M_BUF_SIZE);
		    break;
		case TC_SOCK_PAUSE:
		    tc_pause_request();
		    memset (rbuf, 0, M_BUF_SIZE);
		    break;
		case TC_SOCK_DUMP:
		    ret = !tc_socket_dump_vob(rbuf);
		    memset (rbuf, 0, M_BUF_SIZE);
		    break;
		default:
		    break;
	    }
	    if (ret>0)
		sprintf(rbuf+strlen(rbuf), "%s", "OK\n");
	    else 
		sprintf(rbuf, "%s", "FAILED\n");

	    if (msgsock > 0)
		s_write (msgsock,  rbuf, strlen(rbuf));

	} while (msgsock);
    }
socket_out:
    close (thisfd);
    unlink (socket_file);

}

/* vim: sw=4
 */
