/*
 *  dl_loader.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#else
# ifdef SYSTEM_DARWIN
#  include "libdldarwin/dlfcn.h"
# endif
#endif

#include "transcode.h"
#include "dl_loader.h"

char *mod_path=MOD_PATH;

char module[TC_BUF_MAX];

int (*TCV_export)(int opt, void *para1, void *para2);
int (*TCA_export)(int opt, void *para1, void *para2);
int (*TCV_import)(int opt, void *para1, void *para2);
int (*TCA_import)(int opt, void *para1, void *para2);

void watch_export_module(char *s, int opt, transfer_t *para)
{
    printf("module=%s [option=%02d, flag=%d]\n", s, opt, ((para==NULL)? -1:para->flag));
}

void watch_import_module(char *s, int opt, transfer_t *para)
{
    printf("module=%s [option=%02d, flag=%d]\n", s, opt, ((para==NULL)? -1:para->flag));
    fflush(stdout);
}

int tcv_export(int opt, void *para1, void *para2)
{

  int ret;

  if(verbose & TC_WATCH) 
    watch_export_module("tcv_export", opt, (transfer_t*) para1);

  ret = TCV_export(opt, para1, para2);
  
  if(ret==TC_EXPORT_ERROR && verbose & TC_DEBUG) 
    printf("video export module error\n");
  
  if(ret==TC_EXPORT_UNKNOWN && verbose & TC_DEBUG) 
    printf("option %d unsupported by video export module\n", opt); 
  
  return(ret);
}

int tca_export(int opt, void *para1, void *para2)
{
  
  int ret;
  
  if(verbose & TC_WATCH) 
    watch_export_module("tca_export", opt, (transfer_t*) para1);
  
  ret = TCA_export(opt, para1, para2);
  
  if(ret==TC_EXPORT_ERROR && verbose & TC_DEBUG) 
    printf("audio export module error\n");
  
  if(ret==TC_EXPORT_UNKNOWN && verbose & TC_DEBUG) 
    printf("option %d unsupported by audio export module\n", opt); 
  
  return(ret);
}

int tcv_import(int opt, void *para1, void *para2)
{
  
  int ret;
  
  if(verbose & TC_WATCH) 
    watch_import_module("tcv_import", opt, (transfer_t*) para1);
  
  ret = TCV_import(opt, para1, para2);
  
  if(ret==TC_IMPORT_ERROR && verbose & TC_DEBUG)
    printf("(%s) video import module error\n", __FILE__);
  
  if(ret==TC_IMPORT_UNKNOWN && verbose & TC_DEBUG) 
    printf("option %d unsupported by video import module\n", opt); 
  
  return(ret);
}

int tca_import(int opt, void *para1, void *para2)
{
  int ret;
  
  if(verbose & TC_WATCH) 
    watch_import_module("tca_import", opt, (transfer_t*) para1);
  
  ret = TCA_import(opt, para1, para2);
  
  if(ret==TC_IMPORT_ERROR && verbose & TC_DEBUG)
    printf("(%s) audio import module error\n", __FILE__);
  
  if(ret==TC_IMPORT_UNKNOWN && verbose & TC_DEBUG) 
    printf("option %d unsupported by audio import module\n", opt); 
  
  return(ret);
}


void *load_module(char *mod_name, int mode)
{
#ifdef SYS_BSD
  const
#endif  
  char *error;
  void *handle;
  
  if(mode & TC_EXPORT) {
    
    snprintf(module, sizeof(module), "%s/export_%s.so", ((mod_path==NULL)? TC_DEFAULT_MOD_PATH:mod_path), mod_name);
    
    if(verbose & TC_DEBUG) 
      printf("loading %s export module %s\n", ((mode & TC_VIDEO)? "video": "audio"), module); 
    
    handle = dlopen(module, RTLD_GLOBAL| RTLD_LAZY);
    
    if (!handle) {
      error=dlerror();
      tc_warn("%s", error);
      tc_warn("(%s) loading \"%s\" failed", __FILE__, module);
      return(NULL);
    }
    
    if(mode & TC_VIDEO) {
      TCV_export = dlsym(handle, "tc_export");   
      error = dlerror();
      if (error != NULL)  {
	tc_warn("%s", error);
	return(NULL);
      }
    }
    
    if(mode & TC_AUDIO) {
      TCA_export = dlsym(handle, "tc_export");   
      error = dlerror();
      if (error != NULL)  {
	tc_warn("%s", error);
	return(NULL);
      }
    }
    
    return(handle);
  }
  
  
  if(mode & TC_IMPORT) {
    
    snprintf(module, sizeof(module), "%s/import_%s.so", ((mod_path==NULL)? TC_DEFAULT_MOD_PATH:mod_path), mod_name);
    
    if(verbose & TC_DEBUG) 
      printf("loading %s import module %s\n", ((mode & TC_VIDEO)? "video": "audio"), module); 
    
    handle = dlopen(module, RTLD_GLOBAL| RTLD_LAZY);
    
    if (!handle) {
      error = dlerror();
      tc_warn("%s", error);
      return(NULL);
    }
    
    if(mode & TC_VIDEO) {
      TCV_import = dlsym(handle, "tc_import");   
      if ((error = dlerror()) != NULL)  {
	tc_warn("%s", error);
	return(NULL);
      }
    }
    
    
    if(mode & TC_AUDIO) {
      TCA_import = dlsym(handle, "tc_import");   
      if ((error = dlerror()) != NULL)  {
	tc_warn("%s", error);
	return(NULL);
      }
    }
    
    return(handle);
  }
  
  // wrong mode?
  return(NULL);
}

void unload_module(void *handle)
{
  if (dlclose(handle) != 0) {
      perror("unloading module");
  }
  handle=NULL;
}
