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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "transcode.h"
#include "framebuffer.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#else
# ifdef SYSTEM_DARWIN
#  include "../libdldarwin/dlfcn.h"
# endif
#endif

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

int filter_next_free_id(void)
{
  int n;

  for (n=0; n<MAX_FILTER; ++n) {
    // no filter
    if (!filter[n].name)
      break;
  }
  if (n >= MAX_FILTER) {
    tc_error("internal error - No more filter slots"); 
    n = -1;
  }

  return n-1;
}

void plugin_fix_id(void)
{
  int n;
  char *offset;

  for (n=0; n<MAX_FILTER; ++n) {
    int i;
    int count=0;
    offset = filter[n].name;

    // no filter or already counted
    if (!filter[n].name || filter[n].id != 0) continue;

    for (i=n+1; i<MAX_FILTER; ++i) {
      if (filter[i].name && strcmp(filter[i].name,offset) == 0) {
	count++;
	filter[i].id = count;
      }
    }

  }
  if (verbose & TC_DEBUG) {
    for (n=0; n<MAX_FILTER; ++n) {
      fprintf(stderr, "Filter[%d].name (%s) instance # (%d)\n", n, (filter[n].name?filter[n].name:"-"), filter[n].id);
    }
  }
}
  

// removes \\ from ,
int filter_unquote_options(char *options)
{
    char *a, *b;

    if (!options) return 1;

    a = options+1;
    b = options+1;

    while (*a) {
	if (*a == ',' && *(a-1) == '\\')
	   --b;
	*b++ = *a++;
    }
    *b = '\0';

    return 0;
}

int load_plugin(char *path) {
#if defined(__FreeBSD__) || defined (__APPLE__)
  const
#endif    
  char *error;

  int n;
  char *c;
  int id = filter_next_free_id();
  int len;


  //replace "=" by "/0" in filter name
  if(!filter[id].name || strlen(filter[id].name)==0) return(-1);

  filter[id].options=NULL;

  len = strlen(filter[id].name);
  for(n=0; n<len; ++n) {
    if(filter[id].name[n]=='=') {
      filter[id].name[n]='\0';
      filter[id].options=filter[id].name+n+1;
      break;
    }
  }

  filter_unquote_options(filter[id].options);

  // delete instance
  c = filter[id].name;
  while (*c) {
      if (*c == '#') { 
	  *c = '\0';
	  break;
      }
      c++;
  }
      
  sprintf(module, "%s/filter_%s.so", path, filter[id].name);
  //fprintf(stderr, "[%s] next free ID (%d) is (%d)\n", __FILE__, filter_next_free_id(), id);
  
  // try transcode's module directory
  
  filter[id].handle = dlopen(module, RTLD_NOW); 

  if (!filter[id].handle) {
    error = dlerror();
    tc_warn("loading filter module %s failed", module); 
    tc_warn("%s", error); 
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

int filter_single_init(int id)
{
  frame_list_t ptr;

  if(!plugins_loaded) return(1);    

  ptr.tag = TC_FILTER_INIT;
  ptr.filter_id = filter[id].id;
 
  // This size is very wrong for audio filters, but it is the maximum size of
  // the buffer a filter may get during that transcode run.
  ptr.size = tc_frame_height_max*tc_frame_width_max*3;

  if(filter[id].entry(&ptr, filter[id].options)<0) {
    tc_warn("filter plugin '%s' returned error - plugin skipped", filter[id].name);
    filter[id].status=0;
  } else {
    filter[id].status=1;
  }

  return 0;
}

char * filter_single_readconf(int id)
{
  frame_list_t ptr;
  
  char *buffer = (char *)malloc (PATH_MAX);

  if (!buffer) {
     fprintf(stderr, "Malloc failed in %s:%d", __FILE__, __LINE__);
     return NULL;
  }

  memset (buffer, 0, PATH_MAX);
  ptr.tag = TC_FILTER_GET_CONFIG;

  // the Filter writes into buffer

  ptr.filter_id = filter[id].id;
  if(filter[id].entry(&ptr, buffer)<0) {
    tc_warn("filter plugin '%s' returned error - plugin skipped", filter[id].name);
    filter[id].status=0;
    return NULL;
  } else {
    filter[id].status=1;
    return buffer;
  }

  return NULL;
}

int filter_single_close(int id)
{
    frame_list_t ptr;

    if(!plugins_loaded) return(1);
	
    ptr.tag = TC_FILTER_CLOSE;

    ptr.filter_id = filter[id].id;
    if(filter[id].entry(&ptr, NULL)<0) 
	tc_warn("filter plugin '%s' returned error - plugin skipped", filter[id].name);
    
    return(0);
}

// s == "name[#instance][=.*]"
int plugin_find_id(char *s)
{
  int n, id = -1;
  int instance = -1;
  char *inst; 
  char *args; 
  int len = 0;

  // fprintf (stderr, "[%s] 1 Looking for  (%s) (#%d)\n", __FILE__, s, instance);

  if (!s) return -1;

  inst = strchr (s, '#');
  args = strchr (s, '=');

  // instance?
  if (inst) { 
      instance = atoi(inst+1);
      len = inst - s;
  }
  
  // arguments?
  if (args) { 
      if (!len)
	  len = args - s;
  }
  
  if (!args && !inst)
      len = strlen (s);

  //fprintf (stderr, "[%s] 2 Looking for  (%s) (#%d)\n", __FILE__, s, instance);

  for (n=0; n<MAX_FILTER; ++n) {
	  
    if ( ( ((instance==-1)?filter[n].id:instance) == filter[n].id) && 
		  (filter[n].name && strncmp(filter[n].name, s, len) == 0)) {
      return n;
    }
  }

  return id;
}

int filter_single_configure_handle (int handle, char *options) 
{
    int ret = 0;
    filter[handle].options = options;

    // disable it.
    plugin_disable_id (handle);
    
    // filter_ need to be restarted to take new options into account
    fprintf (stderr, "[%s] filter_close (%d:%s)\n", __FILE__, ret, options);
    filter_single_close(handle);

    fprintf (stderr, "[%s] filter_init  (%d:%s)\n", __FILE__, ret, options);
    ret = filter_single_init(handle);

    // reenable it.
    plugin_enable_id (handle);

    fprintf (stderr, "[%s] filter_single_configure_handle returning (%d)\n", __FILE__, ret);
    return ret;
}

// instance maybe -1 to pick the first match
filter_t * plugin_by_name(char *name, int instance)
{
  int n;
  filter_t *t = NULL;

  for (n=0; n<MAX_FILTER; ++n) {
    if ( ( ((instance==-1)?filter[n].id:instance) == filter[n].id) && 
	            (filter[n].name && strcmp(filter[n].name, name) == 0)) {
      t = &filter[n];
    }
  }
  return t;
}

int load_single_plugin (char *mfilter_string)
{
  int id = filter_next_free_id()+1;
  vob_t *vob = tc_get_vob();

  fprintf(stderr, "[%s] Loading (%s) ..\n", __FILE__, mfilter_string);

  filter[id].namelen = strlen(mfilter_string);
  filter[id].name    = (char *) malloc (filter[id].namelen+1);
  strcpy(filter[id].name, mfilter_string);

  if (load_plugin(vob->mod_path)==0)  {
    plugins++;
    plugins_loaded = 1;
  } else {
    return 1;
  }

  plugin_fix_id();
  filter_single_init(id);

  return 0;
}

char *get_next_filter_name(char **name, int *namelen, char *string)
{
  char *res = string;
  int len = 0;

  if(string[0]=='\0') return(NULL);

  while (1) {
      if((res=strchr(res, ','))==NULL) {
	  *namelen = strlen(string);
	  len = *namelen;
	  *name = (char *)malloc(len+2);
	  memset(*name, 0, len+2);
	  memcpy(*name, string, len);

	  //return pointer to '\0'
	  return(string+strlen(string));
      }

      if ( (res-1) && *(res-1) && (*(res-1) == '\\') ) {
	  // This is a quoted ',' do not use this as a separator
	  res++;
	  continue;
      }

      break;
  }
  *namelen = (int)(res-string);
  len = *namelen;
  *name = (char *)malloc(len+2);

  memset(*name, 0, len+2);
  memcpy(*name, string, len);
  
  return(res+1);
}

int plugin_enable_id (int id)
{
  filter[id].status = 1;
  return 0;
}
int plugin_disable_id (int id)
{
  filter[id].status = 0;
  return 0;
}

int plugin_list_disabled(char *buf)
{
    int n, pos=0;

    for (n=0; n<MAX_FILTER; n++) {
	if ( (filter[n].status == 0) && filter[n].name && strlen(filter[n].name)) {
	    if (pos == 0) { // first
		pos = sprintf(buf, "\"%s\"", filter[n].name);
	    } else {
		pos += sprintf(buf+pos, ", \"%s\"", filter[n].name);
	    }
	}
    }
    sprintf(buf+pos, "\n");
    return 0;
}

int plugin_list_enabled(char *buf) 
{
    int n, pos=0;

    for (n=0; n<MAX_FILTER; n++) {
	if ( (filter[n].status == 1) && filter[n].name && strlen(filter[n].name)) {
	    if (pos == 0) { // first
		pos = sprintf(buf, "\"%s\"", filter[n].name);
	    } else {
		pos += sprintf(buf+pos, ", \"%s\"", filter[n].name);
	    }
	}
    }
    sprintf(buf+pos, "\n");
    return 0;
}

int plugin_list_loaded(char *buf)
{
    int n, pos=0;

    for (n=0; n<MAX_FILTER; n++) {
	if (filter[n].name && strlen(filter[n].name)) {
	    if (pos == 0) { // first
		pos = sprintf(buf, "\"%s\"", filter[n].name);
	    } else {
		pos += sprintf(buf+pos, ", \"%s\"", filter[n].name);
	    }
	}
    }
    sprintf(buf+pos, "\n");
    return 0;
}

int plugin_get_handle (char *name)
{
  int id = -1;

  fprintf(stderr, "[%s] Filter \"%s\" with args (%s)\n", __FILE__, name, name);
  if ( (id = plugin_find_id (name)) < 0) {
    fprintf(stderr, "[%s] Filter \"%s\" not loaded. Loading ...\n", __FILE__, name);

    if (load_single_plugin(name) != 0) {
      fprintf(stderr, "[%s] Loading filter \"%s\" failed\n", __FILE__, name);
      return -1;
    }
    id = plugin_find_id (name);
  } else {
    fprintf(stderr, "[%s] Filter \"%s\" is already loaded.\n", __FILE__, name);
  }
  return id;

}
int init_plugin(vob_t *vob)
{

  int n, j=0;
  char *offset=plugins_string;

  if(verbose & TC_DEBUG)
    if(plugins_string!= NULL) fprintf(stderr, "(%s) %s\n", __FILE__, plugins_string);
  
  // need to load the plugins

  //memset(filter, 0, sizeof(filter_t)*MAX_FILTER);

  for(n=0; n<MAX_FILTER; ++n) {
      //memset(&(filter[n]), 0, sizeof(filter[n]));
      filter[n].options = NULL;
      filter[n].name = NULL;
  }
  
  j=0;
  for(n=0; n<MAX_FILTER; ++n) {
    
    if((offset=get_next_filter_name(&(filter[j].name), &(filter[j].namelen), offset))==NULL) break;
    
    if(load_plugin(vob->mod_path)==0) ++j;
  }

  plugin_fix_id();

  return j;
}

int process_aud_plugins(aframe_list_t *ptr)
{
    
    int n;

    // The filters now do skip for themselfs
    //if (ptr->attributes & TC_FRAME_IS_SKIPPED) return 0;

    if (ptr->attributes & TC_FRAME_IS_OUT_OF_RANGE)
	return 0;

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

    // The filters now do skip for themselfs
    //if (ptr->attributes & TC_FRAME_IS_SKIPPED) return 0;

    if (ptr->attributes & TC_FRAME_IS_OUT_OF_RANGE)
	return 0;

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

  if(plugins_string==NULL) return(0);

  if(!plugins_loaded) return(1);    

    for(n=0; n<plugins; ++n) {
      filter_single_init(n);
    }
    
    return(0);
}


int filter_close()
{

    int n;

    if(plugins_string==NULL) return(0);
    
    if(!plugins_loaded) return(1);
	
    for(n=0; n<plugins; ++n)  {
	filter_single_close(n);
    }
    
    return(0);
}


int plugin_single_close(int id)
{
    if(plugins_string==NULL) return(0);
    if(!plugins_loaded) return(1);    

    if(1 || filter[id].unload)  {
	dlclose(filter[id].handle);
	free(filter[id].name);
	memset (&filter[id], 0, sizeof (filter_t));
    }

    return (0);
}
int plugin_close()
{
    
    int n;
       
    if(plugins_string==NULL) return(0);
    
    if(!plugins_loaded) return(1);
    
    for(n=0; n<plugins; ++n) {
	plugin_single_close(n);
    }
    
    return(0);
}



