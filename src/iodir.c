/*
 *  iodir.c
 *
 *  Copyright (C) Thomas Östreich - May 2002
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
#include <dirent.h>
#include "iodir.h"

#ifdef SYS_BSD
typedef	off_t off64_t;
#define	lseek64 lseek
#endif

static DIR *dir=NULL;

static char filename[PATH_MAX+2];

static char **rbuf_ptr;

static int nfiles=0, findex=0, buffered=0;  


int tc_open_directory(char *dir_name)
{
  if((dir = opendir(dir_name))==NULL) return(-1);
  return(0);
}

char *tc_scan_directory(char *dir_name)
{ 
  struct dirent *dent;
  char *end_of_dir;
  int len;
  
  if (dir_name == 0) return NULL;
  if (dir == 0) return NULL;
  
  len = strlen( dir_name );
  end_of_dir = &dir_name[ len - 1 ];
  
  if ( *end_of_dir != '/' ) { 
      end_of_dir++;
      *end_of_dir = '/';
      end_of_dir++;
      *end_of_dir = 0;	
  }
  
  switch(buffered) {
      
  case 0:
      
      while((dent = readdir( dir ))!=NULL) {
	  
	  if((strncmp(dent->d_name, ".", 1)==0) || (strcmp(dent->d_name, "..")==0)) 
	      continue;
	  
	  snprintf(filename, sizeof(filename), "%s%s", dir_name, dent->d_name);
	  
	  //counter 
	  ++nfiles;
	  
	  return(filename);
      }
      
      break;
      
  case 1:
      
      if (findex < nfiles) {
	  return(rbuf_ptr[findex++]); 
      } else {
	  return(NULL);
      }

      break;
  }
  
  return(NULL);
}


static int compare_name(const void *file1_ptr, const void *file2_ptr)
{
  return strcoll(*(const char **)file1_ptr, *(const char **)file2_ptr);
}


int tc_sortbuf_directory(char *dir_name)
{ 
  struct dirent *dent;
  char *end_of_dir;
  int n, len;

  if (dir_name == 0) return(-1);
  if (dir == 0) return(-1);
  if(nfiles == 0) return(-1);
  
  len = strlen( dir_name );
  end_of_dir = &dir_name[ len - 1 ];
  
  if ( *end_of_dir != '/' ) { 
    end_of_dir++;
    *end_of_dir = '/';
    end_of_dir++;
    *end_of_dir = 0;	
  }
  
  if((rbuf_ptr = (char **) calloc(nfiles, sizeof(char *)))==NULL) {
      perror("out of memory");
      return(-1);
  }
  
  n=0;

  while((dent = readdir( dir ))!=NULL) {
    
    if((strncmp(dent->d_name, ".", 1)==0) || (strcmp(dent->d_name, "..")==0)) 
      continue;
    
    snprintf(filename, sizeof(filename), "%s%s", dir_name, dent->d_name);
    rbuf_ptr[n++] = strdup(filename);
  }
  
  // sorting

  qsort(rbuf_ptr, nfiles, sizeof(char *), compare_name);

  buffered=1;
  findex=0;

  return(0);
}


void tc_close_directory(void)
{
  if(dir!=NULL) closedir(dir);
  dir=NULL;
}

void tc_freebuf_directory(void)
{
  free(rbuf_ptr);
}

