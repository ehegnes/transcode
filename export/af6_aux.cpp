/*
 *  af6_aux.cpp
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *  Copyright (C) 2001 Bram Avontuur (bram@avontuur.org)
 *
 *  codec parameter settings cleanup 
 *  by Gerhard Monzel <gerhard.monzel@sap.com>
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
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#if HAVE_AVIFILE_INCLUDES == 7
#include <avifile-0.7/videoencoder.h>
#include <avifile-0.7/audioencoder.h>
#include <avifile-0.7/avm_fourcc.h>
#include <avifile-0.7/avm_creators.h>
#include <avifile-0.7/avm_default.h>
#elif HAVE_AVIFILE_INCLUDES == 0
#include <avifile/videoencoder.h>
#include <avifile/audioencoder.h>
#include <avifile/avm_fourcc.h>
#include <avifile/creators.h>
#include <avifile/avm_default.h>
#endif

#include "transcode.h"

using namespace Creators;

#ifdef __cplusplus
extern "C" {
#endif


struct codec_attr {
	char *name;
	char *val;
} *attributes;

int attr_count = 0;

#ifndef __FreeBSD__ /* Does it work on other systems? */
  //static 
#endif 
void long2str(unsigned char *dst, int n)
{
   dst[0] = (n    )&0xff;
   dst[1] = (n>> 8)&0xff;
   dst[2] = (n>>16)&0xff;
   dst[3] = (n>>24)&0xff;
}

unsigned long str2ulong(unsigned char *str)
{
   return ( str[0] | (str[1]<<8) | (str[2]<<16) | (str[3]<<24) );
}

void list_codecs()
{
  avm::vector<CodecInfo>::iterator it;
  
  fprintf(stderr, "(%s) available codecs:\n", __FILE__);
  
  for ( it = video_codecs.begin(); it != video_codecs.end(); it++)
  {
     if (it->kind == CodecInfo::DShow_Dec) continue;
      
     const char *cname = it->GetName();
     fprintf(stderr, "\"%s\",", cname);
  }
  fprintf(stderr, "\n");
}
  
void list_attributes(const CodecInfo *info)
{
  int defval=-1;
  const char *def_str;

  avm::vector<AttributeInfo> enc_attr = info->encoder_info; // video_codecs[idx].encoder_info;

  fprintf(stderr, "These attributes are supported for this codec:\n\n");

  avm::vector<AttributeInfo>::const_iterator it;
  for(it=enc_attr.begin(); it!=enc_attr.end(); it++)
  {
    fprintf(stderr, "Attribute \"%s\"\n", it->GetName());
    fflush(stderr);
    switch(it->kind)
    {
      case AttributeInfo::Integer:
        GetCodecAttr(*info, it->GetName(), defval);
        fprintf(stderr, "\tType: integer (default value: %d)\n", defval);
        break;

      case AttributeInfo::Select:
        {
          GetCodecAttr(*info, it->GetName(), defval);
          fprintf(stderr, "\tType: select (default value: %s)\n",
          it->options[defval].c_str());
  	  fprintf(stderr, "\tPossible values: ");

  	  avm::vector<avm::string>::const_iterator sit;
  	  for (sit=(it->options).begin(); sit!=(it->options).end(); sit++)
          fprintf(stderr, "\"%s\" ", sit->c_str());

          fprintf(stderr, "\n");
  	  break;
        }

      case AttributeInfo::String:
  	GetCodecAttr(*info, it->GetName(), &def_str);
  	fprintf(stderr, "\tType: string (default value: %s)\n", def_str);
  	break;
    }
  }  
  fprintf(stderr, "\n");
}


int get_attribute(const CodecInfo *info, const char *attr)
{
	int defval=-1;
	
	avm::vector<AttributeInfo> enc_attr = info->encoder_info;
	avm::vector<AttributeInfo>::const_iterator it;
	
	for(it=enc_attr.begin(); it!=enc_attr.end(); it++) {

	  if (strcasecmp(attr, it->GetName()))
	    continue; //wrong attribute.
	  
	  switch(it->kind) {
	    
	  case AttributeInfo::Integer:
	    GetCodecAttr(*info, it->GetName(), defval);
	    return(defval);
	    break;
	    
	  default:
	    defval=-1;
	    break;
	  }
	}
	return(defval);
}

const CodecInfo *is_valid_codec(const char *cname, fourcc_t *found_codec)
{
  //fix by Oliver Bausinger <bausi@gmx.de>
  BITMAPINFOHEADER bih;
  bih.biCompression = 0xffffffff;
  // just to fill video_codecs list
  Creators::CreateVideoDecoder(bih, 0, 0);
  //fix ends

  if (!cname) return NULL;
  
  *found_codec = 0xFFFFFFFF;
  
  VideoEncoderInfo GetInfo();
  
  avm::vector<CodecInfo>::iterator it;
  
  for (it = video_codecs.begin(); it != video_codecs.end(); it++)
  {
      if (it->kind == CodecInfo::DShow_Dec) continue;
      
      if (!strcasecmp(cname, it->GetName()))
      {
	*found_codec = it->fourcc;
        
        //-- force all codecs to be encoder !!!   --
        //-- some codecs may be crash, but nobody --
        //-- would use it a 2nd. time :-)         --
        it->direction = CodecInfo::Both;
        
        return it;
      }
  }
  
  return NULL;
}

short set_attribute_int(const CodecInfo *info, const char *attr, int val)
{
	int retval = 0;

	avm::vector<AttributeInfo>::const_iterator it;
	avm::vector<AttributeInfo> enc_attr = info->encoder_info;

	for (it = enc_attr.begin(); it != enc_attr.end(); it++)
	{
		if (strcasecmp(attr, it->GetName()))
			continue; //wrong attribute.

		switch(it->kind)
		{
		case AttributeInfo::Integer:
		  SetCodecAttr(*info, it->GetName(), val);
		  break;
		  
		default:
		  retval = 1;
		}
		retval = 1;
		break;
	}
	return retval;
}


short set_attribute(const CodecInfo *info, const char *attr, const char *val)
{
	int retval = 0;

	avm::vector<AttributeInfo>::const_iterator it;
	avm::vector<AttributeInfo> enc_attr = info->encoder_info;

	for (it = enc_attr.begin(); it != enc_attr.end(); it++)
	{
		if (strcasecmp(attr, it->GetName()))
			continue; //wrong attribute.

		switch(it->kind)
		{
		case AttributeInfo::Integer:
		  SetCodecAttr(*info, it->GetName(), atoi(val));
		  
		  break;
		case AttributeInfo::String:
		  SetCodecAttr(*info, it->GetName(), val);
		  
		  break;
		case AttributeInfo::Select:
		  SetCodecAttr(*info, it->GetName(), val);
		  break;
		}
		retval = 1;
		break;
	}
	return retval;
}


short set_attributes(const CodecInfo *info)
{
  //	const char *cname = NULL;
	int i;

	for (i = 0; i < attr_count; i++)
	{
		if (!set_attribute(info, attributes[i].name, attributes[i].val))
			return 0;
	}

	return 1;
}


void clear_attributes()
{
	if (!attr_count)
		return;
	
	if (attributes)
		free(attributes);
	attributes = NULL;
	attr_count = 0;
}

short add_attribute(const char *attr)
{
	int 
		attr_len = strlen(attr) + 1;
	char
		attrname[attr_len],
		attrval[attr_len];
	struct codec_attr
		new_attr;

	int val = sscanf(attr,"%[^=]=%s", attrname, attrval);
	if (val != 2)
		return 0;
	
	new_attr.name = strdup(attrname);
	new_attr.val = strdup(attrval);

	attributes = (struct codec_attr*)realloc(attributes, (attr_count + 1) *
		sizeof(struct codec_attr));
	attributes[attr_count++] = new_attr;	

	return 1;
}

//================
//== GMO Coding ==
//================

static void remove_ch(char *line, char *garbage)
{
  char *src;
  char *dst;
  char *idx = garbage;

  while (*idx)
  {
    src = line;
    dst = line;
   
    while (*src)
    { 
      if (*src != *idx)
      {
        *dst = *src;
        dst++;
      }
      src++;
    }
    *dst = '\0';
    idx++;
  }
}

static void adjust_ch(char *line, char ch)
{
  char *src = &line[strlen(line)];
  char *dst = line;

  //-- remove blanks from right and left side --
  do { src--; } while ( (src != line) && (*src == ch) );
  *(src+1) = '\0';
  src = line;
  while (*src == ch) src++; 

  if (src == line) return;

  //-- copy rest --
  while (*src)
  {
    *dst = *src;
    src++;
    dst++;
  }
  *dst = '\0';
}

int setup_codec_byFile(char *mod_name, const CodecInfo *info, vob_t *vob, int verbose)
{
  FILE *cfg_file;
  char line[128];
  char fname[256];
  char param_name[32];
  char *pstr;
  int  value, param_val;
  int list_attr = 0;
  int  n   = 0;
  int  hit = 0;

  //-- try to open config-file --
  //-----------------------------  
  strcpy(fname, "~/.transcode/export_af6.conf");
  cfg_file = fopen(fname, "r");
  if (!cfg_file) 
  {
    snprintf(fname, sizeof(fname), "%s/export_af6.conf", vob->mod_path);
    cfg_file = fopen(fname, "r");
  }  
  if (!cfg_file) return 0;

  //-- search codec section --
  //--------------------------
  while ( fgets(line, 128, cfg_file) )
  { 
    //-- remove comments --
    if ( (pstr = strchr(line, '#')) != NULL ) *pstr = '\0';

    //-- remove "carbage" --
    remove_ch(line, "\t\n");
    if (!strlen(line)) continue;

    if ( (pstr = strchr(line, '[')) != NULL ) 
    {
      char *ptmp; 
      if ( (ptmp = strchr(pstr,']')) != NULL )
      {
        *ptmp = '\0';
        pstr++;
        if ( !strcmp(pstr, info->GetName()) ) 
        {
          hit = 1;
          break;
        }
      }
    }  
  }

  //-- codec section found --
  //-------------------------
  if (hit)
  {
    while ( fgets(line, 128, cfg_file) )
    {
      //-- remove comments --
      if ( (pstr = strchr(line, '#')) != NULL ) *pstr = '\0';

      //-- remove "carbage" --
      remove_ch(line, "\t\n");
      if (!strlen(line)) continue;

      //-- break on next section ... --
      if ( (pstr = strchr(line, '[')) != NULL ) break;

      //-- ... or test for valid parameter definition --
      if ( (pstr = strchr(line, '=')) != NULL)
      { 
        *pstr = '\0';
        pstr++;
        if (!(*pstr)) continue;
        
        adjust_ch(line, ' ');
        adjust_ch(pstr, ' ');
        if ( !strlen(line) || !strlen(pstr) ) continue;

        //-- get name and value of parameter entry --
        strcpy(param_name, line);
        param_val = atoi(pstr);
        
        //-- count parameter --
        n++;
        if (n==1) printf("[%s] using config from (%s)\n", mod_name, fname);  
        
        //-- set now --
        Creators::SetCodecAttr(*info, param_name, param_val);
        Creators::GetCodecAttr(*info, param_name, value);

        //-- validation --
        if (param_val != value)
        {
          fprintf(stderr, "[%s] failed to set '%s' (%d) for encoder\n", 
                  mod_name, param_name, param_val);

          //-- force update of registry-entry -> this will be helpfull --
          //-- for codecs reading there properties from registry like  --
          //-- Win32-Divx4.11 !                                        --  
          // update_registry(info, param_name, param_val);

          list_attr = 1;
        }
        else if (1) //verbose & TC_DEBUG)
        {
          printf("[%s] set '%s' to (%d)\n", 
                 mod_name, param_name, param_val);
        } 
      } 
    }  
  }   

  fclose(cfg_file);

  //-- sometimes need to know, which properties are available --
  //------------------------------------------------------------ 
  if ( (n && list_attr) || (verbose & TC_DEBUG)) list_attributes(info);

  return n;
} 

int setup_codec_byParam(char *mod_name, const CodecInfo *info, vob_t *vob, int verbose)
{
  int value     = 0;
  int list_attr = 0;

  //-- set Bitrate --
  //-----------------
  
  if(vob->divxbitrate != VBITRATE) {

    Creators::SetCodecAttr(*info, "BitRate", vob->divxbitrate);
    Creators::GetCodecAttr(*info, "BitRate", value);
    if (vob->divxbitrate != value)
      {
	fprintf(stderr, "[%s] failed to set 'BitRate' (%d) for encoder\n", 
		mod_name, vob->divxbitrate);
	list_attr = 1;
      }
    else if (verbose & TC_DEBUG)
      {
	printf("[%s] set 'BitRate' to (%d)\n", mod_name, vob->divxbitrate);
      } 
  }
  
  //-- set Keyframes --
  //-------------------
  

  if(vob->divxkeyframes != VKEYFRAMES) {

    value = 0;
    Creators::SetCodecAttr(*info, "KeyFrames", vob->divxkeyframes);
    Creators::GetCodecAttr(*info, "KeyFrames", value);
    if (vob->divxkeyframes != value)
      {
	fprintf(stderr, "[%s] failed to set 'KeyFrames' (%d) for encoder\n", 
		mod_name, vob->divxkeyframes);
	list_attr = 1;
      }
    else if (verbose & TC_DEBUG)
      {
	printf("[%s] set 'KeyFrames' to (%d)\n", mod_name, vob->divxkeyframes);
      } 
  }

  //-- Set Crispness --
  //-------------------
  
  if(vob->divxcrispness != VCRISPNESS) {

    value = 0;
    Creators::SetCodecAttr(*info, "Crispness", vob->divxcrispness);
    Creators::GetCodecAttr(*info, "Crispness", value);
    if (vob->divxcrispness != value)
      {
	fprintf(stderr, "[%s] failed to set 'Crispness' (%d) for encoder\n", 
		mod_name, vob->divxcrispness);
	list_attr = 1;
      }
    else if (verbose & TC_DEBUG)
      {
	printf("[%s] set 'Crispness' to (%d)\n", mod_name, vob->divxcrispness);
      } 
  }
  
  //-- sometimes need to know, which properties are available --
  //------------------------------------------------------------ 
  if (list_attr || (verbose & TC_DEBUG)) list_attributes(info);
  
  return 1;
}


#ifdef __cplusplus
}
#endif
