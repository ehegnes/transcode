/*
 *  af6_aux.h
 *
 *  Copyright (C) Thomas Östreich - June 2001
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

#ifndef _AF6_AUX_H
#define _AF6_AUX_H

unsigned long str2ulong(unsigned char *str);
void long2str(unsigned char *dst, int n);
int list_attributes(const CodecInfo *info);
int list_codecs();
const CodecInfo *is_valid_codec(const char *cname, fourcc_t *fourcc);
short set_attribute(const CodecInfo *info, const char *attr, const char *val);
short add_attribute(const char*);
short set_attribute_int(const CodecInfo *info, const char *attr, int val);
int get_attribute(const CodecInfo *info, const char *attr);

int setup_codec_byParam(char *mod_name, const CodecInfo *info, vob_t *vob, int verbose);
int setup_codec_byFile(char *mod_name, const CodecInfo *info, vob_t *vob, int verbose);

#endif

