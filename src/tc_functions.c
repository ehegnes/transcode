/*
 *  tc_functions.c 
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

#include "tc_functions.h"

extern void version();
extern char *RED,*GREEN,*YELLOW,*BLUE,*WHITE,*GRAY;

void tc_error(char *fmt, ...)
{
  
  va_list ap;

  // munge format
  int size = strlen(fmt)+2*strlen(RED)+2*strlen(GRAY)+strlen(PACKAGE)+strlen("[] critical: \n")+1;
  char *a = malloc (size);

  version();

  snprintf(a, size, "[%s%s%s] %scritical%s: %s\n", RED, PACKAGE, GRAY, RED, GRAY, fmt);

  va_start(ap, fmt);
  vfprintf (stderr, a, ap);
  va_end(ap);
  free (a);
  //abort
  fflush(stdout);
  exit(1);
}

void tc_warn(char *fmt, ...)
{
  
  va_list ap;

  // munge format
  int size = strlen(fmt)+2*strlen(BLUE)+2*strlen(GRAY)+strlen(PACKAGE)+strlen("[]  warning: \n")+1;
  char *a = malloc (size);

  version();

  snprintf(a, size, "[%s%s%s] %swarning%s : %s\n", RED, PACKAGE, GRAY, YELLOW, GRAY, fmt);

  va_start(ap, fmt);
  vfprintf (stderr, a, ap);
  va_end(ap);
  free (a);
  fflush(stdout);
}

void tc_info(char *fmt, ...)
{
  
  va_list ap;

  // munge format
  int size = strlen(fmt)+strlen(BLUE)+strlen(GRAY)+strlen(PACKAGE)+strlen("[] \n")+1;
  char *a = malloc (size);

  version();

  snprintf(a, size, "[%s%s%s] %s\n", BLUE, PACKAGE, GRAY, fmt);

  va_start(ap, fmt);
  vfprintf (stderr, a, ap);
  va_end(ap);
  free (a);
  fflush(stdout);
}

int tc_test_program(char *name)
{
#ifndef NON_POSIX_PATH
  char *tok_path = NULL;
  char *compl_path = NULL;
  char *tmp_path = malloc(strlen(getenv("PATH")) * sizeof(char));
  char **strtokbuf = malloc(strlen(getenv("PATH")) * sizeof(char));
  int tmp_errno = 0;

  strcpy(tmp_path, getenv("PATH"));
  
  /* iterate through PATH tokens */
  for (tok_path = strtok_r(tmp_path, ":", strtokbuf);
       tok_path != NULL;
       tok_path = strtok_r(NULL, ":", strtokbuf)) {

    compl_path = malloc((strlen(tok_path) + strlen(name) + 1) * sizeof(char));
  
    strcpy(compl_path, tok_path);
    compl_path = strncat(compl_path, "/", 1);
    compl_path = strncat(compl_path, name, strlen(name));
    
    if (access(compl_path, X_OK) == 0) {
      free(tok_path);
      free(strtokbuf);
      free(compl_path);
      return 0;
    }
    if (errno != ENOENT) tmp_errno = errno;
  }
  
  free(tok_path);
  free(strtokbuf); 
  free(compl_path);
                  
  if (tmp_errno == 0) {
    fprintf(stderr, "[%s] ERROR: The '%s' program could not be found. \n"
                    "[%s]        Please check your installation.\n", PACKAGE,  name, PACKAGE);
    return errno;
  }

  /* access returned an unhandled error */
  fprintf(stderr, "[%s] ERROR: The '%s' program was found but is not accessible.\n"
                  "[%s]        %s\n"
                  "[%s]        Please check your installation.\n", PACKAGE,  name, PACKAGE,  strerror(tmp_errno), PACKAGE);
  return tmp_errno;
#else
  return 0;
#endif
}

