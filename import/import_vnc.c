#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "transcode.h"
#include "ioaux.h"

#define MOD_NAME    "import_vnc.so"
#define MOD_VERSION "v0.0.2 (2003-11-29)"
#define MOD_CODEC   "(video) VNC"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_VID|TC_CAP_RGB|TC_CAP_YUV;

#define MOD_PRE vnc
#include "import_def.h"

#define PROGTOCALL "tcxpm2rgb"

static int pid;
static char fifo[256];

/* ------------------------------------------------------------
 *
 * open stream
 *
 * ------------------------------------------------------------*/

#if 0  /* get this from ioaux.c */
static ssize_t p_read(int fd, char *buf, size_t len)
{
   ssize_t n = 0;
   ssize_t r = 0;

   while (r < len) {
      n = read (fd, buf + r, len - r);

	  if (n == 0)
		break;
	  if (n < 0) {
		if (errno == EINTR)
		  continue;
		else
		  break;
	  } 
      r += n;
   }

   return r;
}
#endif

MOD_open
{
    if (param->flag == TC_VIDEO) {
	char fps[32]; 
	char cmdbuf[1024];

	snprintf (fifo, 256, "%s-%d", "/tmp/tc-vncfifo", getpid());
	snprintf (fps, 32, "%f", vob->fps);
	snprintf (cmdbuf, 1024, "%s -o %s", PROGTOCALL, fifo);

	mkfifo (fifo, 0600);

	switch (pid = fork()) {
	    case 0:
		{
		    int n=0;
		    char *a[16];
		    char *c = vob->im_v_string;

		    setenv("VNCREC_MOVIE_FRAMERATE", fps, 1);
		    setenv("VNCREC_MOVIE_CMD", cmdbuf, 1);

		    //close(STDOUT_FILENO);
		    //close(STDERR_FILENO);

		    a[n++] = "vncrec";
		    a[n++] = "-movie";
		    a[n++] = vob->video_in_file;
		    if ( vob->im_v_string) {
			char *d = c;
			while (1) {
			    if (c && *c) { 
				d = strchr (c, ' ');
				if (d && *d) { *d = '\0';
				    while (*c == ' ') c++;
				    a[n++] = c;
				    printf("XX |%s|\n", c);
				    c = strchr(c, ' ');
				} else {
				    printf("XXXX |%s|\n", c);
				    a[n++] = c;
				    break;
				}
			    } else  {
				d++;
				while (*d == ' ') d++;
				if (strchr(d, ' ')) *strchr(d, ' ') = '\0';
				a[n++] = d;
				printf("XXX |%s|\n", d);
				break;
			    }
			}
		    }
		    a[n++] = NULL;
		    if (execvp (a[0], &a[0])<0) {
			perror ("execvp vncrec failed. Is vncrec in your $PATH?");
			return (TC_IMPORT_ERROR);
		    }
		}
		break;
	    default:
		break;
	}
	//fprintf(stderr, "Main here\n");


	return (TC_IMPORT_OK);
    }
    return (TC_IMPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/


MOD_decode
{
    if (param->flag == TC_VIDEO) {
	int fd;
	int status, ret, wret;
	fd_set rfds;
	struct timeval tv;
	int n;
	extern int tc_dvd_access_delay;

	while (1) {

	    // timeout to catch when vncrec died
	    tv.tv_sec = tc_dvd_access_delay;
	    tv.tv_usec = 0;

	    fd = open(fifo, O_RDONLY | O_NONBLOCK);
	    if (fd < 0) { perror ("open"); break; }

	    FD_ZERO(&rfds);
	    FD_SET(fd, &rfds);

	    status = select(fd+1, &rfds, NULL, NULL, &tv);

	    if (status) {

		if (FD_ISSET(fd, &rfds)) {

		    n = 0;
		    while (n < param->size) {
			n += p_read(fd, param->buffer+n, param->size-n);
		    }
		}

		// valid frame in param->buffer


		close(fd);
		return (TC_IMPORT_OK);
	    } else {
		kill(pid, SIGKILL);
		wret = wait(&ret);
		//printf("%d ret %d;\n", wret, ret);
		close(fd);
		return (TC_IMPORT_ERROR);
	    }

	    close(fd);
	    return (TC_IMPORT_OK);

	}
    }

    return (TC_IMPORT_ERROR);
}


/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{  
    if (param->flag == TC_VIDEO) {
	int ret;
	kill(pid, SIGKILL);
	wait(&ret);
	//fprintf(stderr, "\nCLOSE1\n");
	unlink (fifo);
	//fprintf(stderr, "\nCLOSE2\n");
    }

    return (TC_IMPORT_OK);
}
