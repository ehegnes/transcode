/*
 *  socket.h
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

#ifndef __TC_SOCKET_H
#define __TC_SOCKET_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pthread.h>

enum TC_SOCKET_RETURN {
    TC_SOCK_FAILED,
    TC_SOCK_QUIT,
    TC_SOCK_HELP,
    TC_SOCK_VERSION,
    TC_SOCK_LOAD,
    TC_SOCK_UNLOAD,
    TC_SOCK_LIST,
    TC_SOCK_ENABLE,
    TC_SOCK_DISABLE,
    TC_SOCK_CONFIG,
    TC_SOCK_PARAMETER,
    TC_SOCK_PREVIEW,
    TC_SOCK_PROGRESS_METER,
};

// The heigh 8 bits are used to pass a char as an arg
#define TC_SOCK_COMMAND_MASK 0x00ffffff
#define TC_SOCK_ARG_MASK     0xff000000
#define TC_SOCK_ARG_SHIFT    24

#define TC_SOCK_GET_ARG(arg)    ( ((arg&TC_SOCK_ARG_MASK) >> TC_SOCK_ARG_SHIFT)&0xff )
#define TC_SOCK_SET_ARG(to,arg) \
	(( to = ((arg<<TC_SOCK_ARG_SHIFT)&TC_SOCK_ARG_MASK) | (to&TC_SOCK_COMMAND_MASK) ) )

#define TC_SOCK_PV_NONE       0
#define TC_SOCK_PV_PAUSE      1
#define TC_SOCK_PV_DRAW       2
#define TC_SOCK_PV_UNDO       3
#define TC_SOCK_PV_SLOW_FW    4
#define TC_SOCK_PV_SLOW_BW    5
#define TC_SOCK_PV_FAST_FW    6
#define TC_SOCK_PV_FAST_BW    7
#define TC_SOCK_PV_SLOWER     9
#define TC_SOCK_PV_FASTER    10
#define TC_SOCK_PV_TOGGLE    11
#define TC_SOCK_PV_ROTATE    12
#define TC_SOCK_PV_DISPLAY   13

void tc_socket_submit (char *buf);
void socket_thread(void);

extern pthread_mutex_t tc_socket_msg_lock;
extern unsigned int tc_socket_msgchar;

#endif // __TC_SOCKET_H
