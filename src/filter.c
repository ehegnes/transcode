/*
 *  filter.c
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

#include "framebuffer.h"
#include <dlfcn.h>
#include <string.h>
#include "filter.h"

/* ------------------------------------------------------------ 
 *
 * video transformation plugins
 *
 * image frame buffer: ptr->video_buf
 *
 * notes: (1) physical frame data at any time stored in frame_list_t *ptr
 *        (2) vob_t *vob structure contains information on
 *            video transformation and import/export frame parameter
 *
 * ------------------------------------------------------------*/

static int plugins_loaded=0, plugins=0;

int (*tc_filter)(void *ptr, void *opt);
static char module[TC_BUF_MAX];

filter_t filter[MAX_FILTER];

int load_plugin(char *path, int id) {
#ifdef __FreeBSD__
  const
#endif    
  char *error;

  int n;

  //replace "=" by "/0" in filter name
  
  if(strlen(filter[id].name)==0) return(-1);

  filter[id].options=NULL;

  for(n=0; n<strlen(filter[id].name); ++n) {
    if(filter[id].name[n]=='=') {
      filter[id].name[n]='\0';
      filter[id].options=filter[id].name+n+1;
      break;
    }
  }
  
  sprintf(module, "%s/filter_%s.so", path, filter[id].name);
  
  // try transcode's module directory
  
  filter[id].handle = dlopen(module, RTLD_NOW); 

  if (!filter[id].handle) {
    fprintf(stderr, "[%s] loading filter module %s failed\n", PACKAGE, module); 
    if ((error = dlerror()) != NULL) fputs(error, stderr);
    return(-1);

  } else 
    if(verbose & TC_DEBUG) fprintf(stderr, "[%s] loading filter module (%d) %s\n", PACKAGE, id, module); 
  
  filter[id].entry = dlsym(filter[id].handle, "tc_filter");   
  
  if ((error = dlerror()) != NULL)  {
    fputs(error, stderr);
    return(-1);
  }

  //FIXME: XV mods seem to crach transcode if unloaded properly;-)
  filter[id].unload=1;
  if(strcmp(filter[id].name, "preview")==0) filter[id].unload=0;
  if(strcmp(filter[id].name, "pv")==0) filter[id].unload=0;
  
  return(0);
}

char *get_next_filter_name(char *name, char *string)
{
  char *res;

  if(string[0]=='\0') return(NULL);

  if((res=strchr(string, ','))==NULL) {
    strcpy(name, string);
    //return pointer to '\0'
    return(string+strlen(string));
  }
  
  memcpy(name, string, (int)(res-string));
  
  return(res+1);
}

int init_plugin(vob_t *vob)
{

  int n, j=0;
  char *offset=plugins_string;

  if(verbose & TC_DEBUG)
    if(plugins_string!= NULL) fprintf(stderr, "(%s) %s\n", __FILE__, plugins_string);
  
  // need to load the plugins
  
  for(n=0; n<MAX_FILTER; ++n) {
    
    if((offset=get_next_filter_name(filter[j].name, offset))==NULL) break;
    
    if(load_plugin(vob->mod_path, j)==0) ++j;
  }

  for (n=0; n<MAX_FILTER; ++n) {
    int i;
    int count=0;
    offset = filter[n].name;

    // no filter or already counted
    if (filter[n].name[0] == '\0' || filter[n].id != 0) continue;

    for (i=n+1; i<MAX_FILTER; ++i) {
      if (strcmp(filter[i].name,offset) == 0) {
	count++;
	filter[i].id = count;
      }
    }

  }
  if (verbose & TC_DEBUG)
    for (n=0; n<MAX_FILTER; ++n) {
      fprintf(stderr, "Filter[%d].name (%s) instance # (%d)\n", n, filter[n].name, filter[n].id);
    }
  
  return j;
}

int process_aud_plugins(aframe_list_t *ptr)
{
    
    int n;

    for(n=0; n<plugins; ++n) {
      if(filter[n].status) {
	
	ptr->filter_id = filter[n].id;
	if(filter[n].entry(ptr, NULL)<0) 
	  fprintf(stderr," (%s) filter plugin '%s' returned error - ignored\n", __FILE__, filter[n].name);
      }
    }
    return (0);
}

int process_vid_plugins(vframe_list_t *ptr)
{
    
    int n;

    for(n=0; n<plugins; ++n) {
      if(filter[n].status) {
	
	ptr->filter_id = filter[n].id;
	if(filter[n].entry(ptr, NULL)<0) 
	  fprintf(stderr," (%s) filter plugin '%s' returned error - ignored\n", __FILE__, filter[n].name);
      }
    }
    return (0);
}


int plugin_init(vob_t *vob)
{

    if(plugins_string==NULL) return(0);
    
    if(!plugins_loaded) {
	
	plugins=init_plugin(vob);
	
	if(verbose & TC_DEBUG) fprintf(stderr, "(%s) successfully loaded %d filter plugin(s)\n", __FILE__, plugins);
	plugins_loaded=1;
    }
    
    return(0);
}


int filter_init()
{

    int n;

    frame_list_t ptr;

    if(plugins_string==NULL) return(0);

    if(!plugins_loaded) return(1);    

    ptr.tag = TC_FILTER_INIT;

    for(n=0; n<plugins; ++n) {
      ptr.filter_id = filter[n].id;
      if(filter[n].entry(&ptr, filter[n].options)<0) {
	fprintf(stderr," (%s) filter plugin '%s' returned error - plugin skipped\n", __FILE__, filter[n].name);
	filter[n].status=0;
      } else {
	filter[n].status=1;
      }
    }
    
    return(0);
}


int filter_close()
{

    int n;

    frame_list_t ptr;

    if(plugins_string==NULL) return(0);
    
    if(!plugins_loaded) return(1);
	
    ptr.tag = TC_FILTER_CLOSE;

    for(n=0; n<plugins; ++n)  {
      ptr.filter_id = filter[n].id;
      if(filter[n].entry(&ptr, NULL)<0) fprintf(stderr," (%s) filter plugin '%s' returned error - ignored\n", __FILE__, filter[n].name);
    }
    
    return(0);
}


int plugin_close()
{
    
    int n;
       
    if(plugins_string==NULL) return(0);
    
    if(!plugins_loaded) return(1);
    
    for(n=0; n<plugins; ++n) if(filter[n].unload) dlclose(filter[n].handle);
    
    return(0);
}



