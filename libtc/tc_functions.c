/*
 *  tc_functions.c - various common functions for transcode
 *  Written by Thomas Oestreich, Francesco Romani, Andrew Church, and others
 *
 *  This file is part of transcode, a video stream processing tool.
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

#include "libxio/xio.h"

#include "libtc.h"
#include "tc_func_excl.h"

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if defined(HAVE_ALLOCA_H)
#include <alloca.h>
#endif

/*************************************************************************/

/* local colors macro; get COL(x) macro from tc_func_excl.h */
#define COL_RED             COL(31)
#define COL_GREEN           COL(32)
#define COL_YELLOW          COL(33)
#define COL_BLUE            COL(34)
#define COL_WHITE           COL(37)
#define COL_GRAY            "\033[0m"

#define TC_MSG_BUF_SIZE     (256)

/* WARNING: we MUST keep in sync preambles order with TC_LOG* macros */
static const char *tc_log_preambles[] = {
    /* TC_LOG_ERR */
    "["COL_RED"%s"COL_GRAY"]"COL_RED" critical"COL_GRAY": %s\n", 
    /* TC_LOG_WARN */
    "["COL_RED"%s"COL_GRAY"]"COL_YELLOW" warning"COL_GRAY": %s\n", 
    /* TC_LOG_INFO */
    "["COL_BLUE"%s"COL_GRAY"] %s\n", 
    /* TC_LOG_MSG */
    "[%s] %s\n",
};

void tc_log(int level, const char *tag, const char *fmt, ...)
{
    char buf[TC_MSG_BUF_SIZE];
    char *msg = buf;
    int dynbuf = 0; 
    /* flag: we must use a dynamic (larger than static) buffer? */
    size_t size = 0;
    va_list ap;

    /* sanity check, avoid {under,over}flow; */
    level = (level < TC_LOG_ERR) ?TC_LOG_ERR :level;
    level = (level > TC_LOG_MSG) ?TC_LOG_MSG :level;

    size = strlen(tc_log_preambles[level]) 
           + strlen(tag) + strlen(fmt) + 1;          
 
    if (size > TC_MSG_BUF_SIZE) {
        dynbuf = 1;
        msg = malloc(size);
        if (msg == NULL) {
            fprintf(stderr, "(%s) CRITICAL: can't get memory in "
                    "tc_log(); tag='%s'\n", __FILE__, 
                    tag);
            return;
        }
    } else {
        size = TC_MSG_BUF_SIZE - 1;
    }

    snprintf(msg, size, tc_log_preambles[level], tag, fmt);
  
    va_start(ap, fmt);
    vfprintf(stderr, msg, ap);
    va_end(ap);

    if (dynbuf == 1) {
        free(msg);
    }
    
    /* ensure that all *other* messages are written */
    fflush(stdout);
    
    return;
}  

/*************************************************************************/

#if defined(HAVE_ALLOCA)
#define local_alloc(s) alloca(s)
#define local_free(s) do { (void)0; } while(0)
#else
#define local_alloc(s) malloc(s)
#define local_free(s) free(s)
#endif

int tc_test_program(const char *name)
{
#ifndef NON_POSIX_PATH
    const char *path = getenv("PATH");
    char *tok_path = NULL;
    char *compl_path = NULL;
    char *tmp_path;
    char **strtokbuf;
    char done;
    size_t pathlen;
    long sret;
    int error = 0;

    if (name == NULL) {
        tc_warn("ERROR: Searching for a NULL program!\n");
        return ENOENT;
    }

    if (path == NULL) {
        tc_warn("The '%s' program could not be found. \n", name);
        tc_warn("Because your PATH environment variable is not set.\n");
        return ENOENT;
    }

    pathlen = strlen(path) + 1;
    tmp_path = local_alloc(pathlen * sizeof(char));
    strtokbuf = local_alloc(pathlen * sizeof(char));

    sret = strlcpy(tmp_path, path, pathlen);
    tc_test_string(__FILE__, __LINE__, pathlen, sret, errno);

    /* iterate through PATH tokens */
    for (done = 0, tok_path = strtok_r(tmp_path, ":", strtokbuf);
            !done && tok_path;
            tok_path = strtok_r((char *)0, ":", strtokbuf)) {
        pathlen = strlen(tok_path) + strlen(name) + 2;
        compl_path = local_alloc(pathlen * sizeof(char));
        sret = snprintf(compl_path, pathlen, "%s/%s", tok_path, name);
        tc_test_string(__FILE__, __LINE__, pathlen, sret, errno);

        if (access(compl_path, X_OK) == 0) {
            error   = 0;
            done    = 1;
        } else { /* access != 0 */
            if (errno != ENOENT) {
                done    = 1;
                error   = errno;
            }
        }

        local_free(compl_path);
    }

    local_free(tmp_path);
    local_free(strtokbuf); 

    if (!done) {
        tc_warn("The '%s' program could not be found. \n", name);
        tc_warn("Please check your installation.\n");
        return ENOENT; 
    }

    if (error != 0) {
        /* access returned an unhandled error */
        tc_warn("The '%s' program was found, but is not accessible.\n", name);
        tc_warn("%s\n", strerror(errno));
        tc_warn("Please check your installation.\n");
        return error;
    }
#endif

    return 0;
}

#undef local_alloc
#undef local_free

/*************************************************************************/

#define delta 0.05
int tc_guess_frc(double fps)
{
    if (fps - delta < 00.010 && 00.010 < fps + delta)
        return 0;
    if (fps - delta < 23.976 && 23.976 < fps + delta)
        return 1;
    if (fps - delta < 24.000 && 24.000 < fps + delta)
        return 2;
    if (fps - delta < 25.000 && 25.000 < fps + delta)
        return 3;
    if (fps - delta < 29.970 && 29.970 < fps + delta)
        return 4;
    if (fps - delta < 30.000 && 30.000 < fps + delta)
        return 5;
    if (fps - delta < 50.000 && 50.000 < fps + delta)
        return 6;
    if (fps - delta < 59.940 && 59.940 < fps + delta)
        return 7;
    if (fps - delta < 60.000 && 60.000 < fps + delta)
        return 8;
    if (fps - delta <  1.000 &&  1.000 < fps + delta)
        return 9;
    if (fps - delta <  5.000 &&  5.000 < fps + delta)
        return 10;
    if (fps - delta < 10.000 && 10.000 < fps + delta)
        return 11;
    if (fps - delta < 12.000 && 12.000 < fps + delta)
        return 12;
    if (fps - delta < 15.000 && 15.000 < fps + delta)
        return 13;
    return -1;
}
#undef delta

/*************************************************************************/

int tc_test_string(const char *file, int line, int limit, long ret, int errnum)
{
    if (ret < 0) {
        fprintf(stderr, "[%s:%d] string error: %s\n",
                        file, line, strerror(errnum));
        return 1;
    }
    if (ret >= limit) {
        fprintf(stderr, "[%s:%d] truncated %ld characters\n",
                        file, line, (ret - limit) + 1);
        return 1;
    }
    return 0;
}

/*************************************************************************/

/*
 * These versions of [v]snprintf() return -1 if the string was truncated,
 * printing a message to stderr in case of truncation (or other error).
 */

int _tc_vsnprintf(const char *file, int line, char *buf, size_t limit,
                  const char *format, va_list args)
{
    int res = vsnprintf(buf, limit, format, args);
    return tc_test_string(file, line, limit, res, errno) ? -1 : res;
}


int _tc_snprintf(const char *file, int line, char *buf, size_t limit,
                 const char *format, ...)
{
    va_list args;
    int res;

    va_start(args, format);
    res = _tc_vsnprintf(file, line, buf, limit, format, args);
    va_end(args);
    return res;
}

/*************************************************************************/

/* simple malloc wrapper with failure guard. */

void *_tc_malloc(const char *file, int line, size_t size)
{
    void *p = malloc(size);
    if(p == NULL) {
        fprintf(stderr, "[%s:%d] tc_malloc(): can't allocate %lu bytes\n",
                        file, line, (unsigned long)size);
    }
    return p;
}        

/* allocate a chunk of memory (like tc_malloc), but zeroes memory before
 * returning. */

void *_tc_zalloc(const char *file, int line, size_t size)
{
    void *p = malloc(size);
    if(p == NULL) {
        fprintf(stderr, "[%s:%d] tc_zalloc(): can't allocate %lu bytes\n",
                        file, line, (unsigned long)size);
    } else {
        memset(p, 0, size);
    }
    return p;
}

/*** FIXME ***: find a clean way to refactorize above two functions */

/* Allocate a buffer aligned to the machine's page size, if known.  The
 * buffer must be freed with buffree() (not free()). */

void *_tc_bufalloc(const char *file, int line, size_t size)
{
#ifdef HAVE_GETPAGESIZE
    unsigned long pagesize = getpagesize();
    int8_t *base = malloc(size + sizeof(void *) + pagesize);
    int8_t *ptr = NULL;
    unsigned long offset = 0;
    
    if(base == NULL) {
        fprintf(stderr, "[%s:%d] tc_bufalloc(): can't allocate %lu bytes\n",
                        file, line, (unsigned long)size);
    } else {
        ptr = base + sizeof(void *);
        offset = (unsigned long)ptr % pagesize;
        
        if (offset)
            ptr += (pagesize - offset);
        ((void **)ptr)[-1] = base;  /* save the base pointer for freeing */
    }
    return ptr;
#else  /* !HAVE_GETPAGESIZE */
    return malloc(size);
#endif
}


/* Free a buffer allocated with tc_bufalloc(). */
void tc_buffree(void *ptr)
{
#ifdef HAVE_GETPAGESIZE
    if (ptr)
	free(((void **)ptr)[-1]);
#else
    free(ptr);
#endif
}


ssize_t tc_pread(int fd, uint8_t *buf, size_t len)
{
    ssize_t n = 0;
    ssize_t r = 0;

    while (r < len) {
        n = xio_read(fd, buf + r, len - r);
        
        if (n == 0) {  /* EOF */
            break;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                break;
            }
        }
        r += n;
    }
    return r;
}


ssize_t tc_pwrite(int fd, uint8_t *buf, size_t len)
{
    ssize_t n = 0;
    ssize_t r = 0;

    while (r < len) {
        n = xio_write(fd, buf + r, len - r);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                break;
            }                
        }
        r += n;
    }
    return r;
}


#define MAX_BUF 4096
int tc_preadwrite(int fd_in, int fd_out)
{
    uint8_t buffer[MAX_BUF];
    ssize_t bytes;
    int error = 0;

    do {
        bytes = tc_pread(fd_in, buffer, MAX_BUF);

        /* error on read? */
        if (bytes < 0) {
            return -1;
        }            

        /* read stream end? */
        if (bytes != MAX_BUF) {
            error = 1;
        }            

        if (bytes) {
            /* write stream problems? */
            if (tc_pwrite(fd_out, buffer, bytes) != bytes) {
                error = 1;
            }
        }
    } while (!error);
 
    return 0;
}


int tc_file_check(const char *name)
{
    struct stat fbuf;

    if(xio_stat(name, &fbuf)) {
        tc_log_warn(__FILE__, "invalid file \"%s\"", name);
        return -1;
    }

    // file or directory?
    if(S_ISDIR(fbuf.st_mode)) {
        return 1;
    }
    return 0;
}

#ifndef major
# define major(dev)  (((dev) >> 8) & 0xff)
#endif

int tc_probe_path(const char *name) 
{
    struct stat fbuf;
#ifdef NET_STREAM
    struct hostent *hp;
#endif
    if(name == NULL) { 
        tc_log_warn(__FILE__, "invalid file \"%s\"", name);
        return TC_PROBE_PATH_INVALID;
    }

    if(xio_stat(name, &fbuf)==0) {
        /* inode exists */

        /* treat DVD device as absolute directory path */
        if (S_ISBLK(fbuf.st_mode)) {
            return TC_PROBE_PATH_ABSPATH;
        }

        /* char device could be several things, depending on system */
        /* *BSD DVD device? v4l? bktr? sunau? */
        if(S_ISCHR(fbuf.st_mode)) {
            switch (major(fbuf.st_rdev)) {
#ifdef SYS_BSD
# ifdef __OpenBSD__
                case 15: /* rcd */
                    return TC_PROBE_PATH_ABSPATH;
                case 42: /* sunau */
                    return TC_PROBE_PATH_SUNAU;
                case 49: /* bktr */
                    return TC_PROBE_PATH_BKTR;
# endif
# ifdef __FreeBSD__
                case 4: /* acd */
                    return TC_PROBE_PATH_ABSPATH;
                case 229: /* bktr */
                    return TC_PROBE_PATH_BKTR;
                case 0: /* OSS */
                    return TC_PROBE_PATH_OSS;
# endif
                default: /* libdvdread uses "raw" disk devices here */
                    return TC_PROBE_PATH_ABSPATH;
#else
                case 81: /* v4l (Linux) */
                    return TC_PROBE_PATH_V4L_VIDEO;
#ifdef HAVE_V4L
                case 14: /* dsp (Linux) */
                    return TC_PROBE_PATH_V4L_AUDIO;
#else
                case 14: /* OSS */
                    return TC_PROBE_PATH_OSS;
#endif
                default:
                    break;
#endif
            }
        }

        /* file or directory? */
        if (!S_ISDIR(fbuf.st_mode)) {
            return TC_PROBE_PATH_FILE;
        }

        /* directory, check for absolute path */
        if(name[0] == '/') {
            return TC_PROBE_PATH_ABSPATH;
        }

        /* directory mode */
        return TC_PROBE_PATH_RELDIR;
    } else {
#ifdef NET_STREAM
        /* check for network host */
        if ((hp = gethostbyname(name)) != NULL) {
            return(TC_PROBE_PATH_NET);
        }
#endif
        tc_log_warn(__FILE__, "invalid filename or host \"%s\"", name);
        return TC_PROBE_PATH_INVALID;
    }
    
    return TC_PROBE_PATH_INVALID;
}

/*************************************************************************/

/* embedded simple test for tc_log()

int main() 
{
    int i = 0;

    for(i = 0; i < 4; i++) {
        tc_log(i, __FILE__, "short format");
        tc_log(i, __FILE__, "a little longer format (%i)", i);
        tc_log(i, __FILE__, "a really longer format (%i) with additional "
                            "arg: %s", i, __FILE__);
        tc_log(i, __FILE__, "a really really long format (%i) with various "
                            "additional arguments: "
                            " file='%s' line='%i' date='%s' i=%i &i=%p",
                            i, __FILE__, __LINE__, __DATE__, i, &i);

#define __TAG__ "tag a bit more longer than FILE standard macro"
        tc_log(i, __TAG__, "short format");
        tc_log(i, __TAG__, "a little longer format (%i)", i);
        tc_log(i, __TAG__, "a really longer format (%i) with additional "
                            "arg: %s", i, __FILE__);
        tc_log(i, __TAG__, "a really really long format (%i) with various "
                           "additional arguments: "
                           " file='%s' line='%i' date='%s' i=%i &i=%p",
                           i, __FILE__, __LINE__, __DATE__, i, &i);
        
       
    }

    return 0;
}

*/
