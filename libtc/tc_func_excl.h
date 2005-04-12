/*
 *  tc_func_excl.h
 *
 *  Copyright (C) Thomas Östreich - August 2003
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

#ifndef _TC_FUNC_EXCL_H
#define _TC_FUNC_EXCL_H 


#define COL(x)  "\033[" #x ";1m"
char *RED    = COL(31);
char *GREEN  = COL(32);
char *YELLOW = COL(33);
char *BLUE   = COL(34);
char *WHITE  = COL(37);
char *GRAY   =  "\033[0m";

#endif
